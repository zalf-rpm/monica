/* Differential-test driver for phase 5 checkpoint 3 (phenology + canopy
 * geometry): fcRadiation, fcDaylengthFactor, fcVernalisationFactor,
 * fcOxygenDeficiency, fcCropDevelopmentalStage (+ its
 * fcUpdateCropParametersForPerennial dependency), fcKcFactor, fcCropSize,
 * fcCropGreenArea, fcSoilCoverage, setStage, and the anthesis/maturity query
 * functions - all in src/core/crop-module.cpp.
 *
 * None of these are step() itself (checkpoint 7), so this driver replicates
 * just enough of step()'s call order and small inline bookkeeping (the
 * perennial-dormancy-period-end-date init, the old/new devstage anthesis/
 * maturity field updates, vc_RelativeTotalDevelopment, the stage-0-vs-fcKcFactor
 * branch) to exercise the checkpoint's functions in a realistic sequence -
 * same "driver reimplements just enough orchestration" approach phase 4 used
 * to chain already-ported step functions before monicamodel::generalStep
 * existed. Deliberately NOT replicated here (checkpoint 7's job): the FAO-56
 * inline Kcb block (not a cropmodule:: function), all fireEvent calls,
 * fcCropPhotosynthesis onward.
 *
 * Two scenarios:
 *   A: real wheat CropParameters/CropResidueParameters (same as
 *      crop_module_ref_main.cpp), started past germination via setStage(1) -
 *      germination requires soiltemperature::step, which is checkpoint 7's
 *      job to chain in, and EmergenceMoistureControlOn/EmergenceFloodingControlOn
 *      are both false in sim-min.json, so soilMoisture_m3/fieldCapacity/
 *      permanentWiltingPoint are provably unused once past stage 0 anyway.
 *      Runs NUM_DAYS days of the real Hohenfinow2 climate-min.csv record,
 *      exercising real long-day fcDaylengthFactor, real vernalisation
 *      dynamics across a real winter/summer temperature swing, and whatever
 *      of the N-/water-stress developmental acceleration wheat's own
 *      pc_AssimilatePartitioningCoeff happens to trigger.
 *   B: a synthetic cultivar (cloned from wheat, then overridden) exercising
 *      every branch scenario A cannot reach: pc_Perennial=true with a
 *      dormancyStartDoy reset, short-day fcDaylengthFactor (negative
 *      pc_DaylengthRequirement), __enable_Phenology_WangEngelTemperatureResponse__,
 *      an explicit CropParameters-level __enable_vernalisation_factor_fix__
 *      override (Some(false), overriding the CropModuleParameters default),
 *      fcRadiation's globalRadiation<=0 branch, and stage-0 germination
 *      (soilColumn.layers[0].vs_SoilTemperature forced across
 *      pc_BaseTemperature[0] partway through). Runs a small hand-written
 *      weather sequence, identical on both sides.
 *
 * Usage: crop_module_phenology_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>
 * See odin/tests/cpp_ref/run_crop_module_phenology.sh.
 */

#include <cstdio>
#include <map>
#include <string>

#include "climate/climate-file-io.h"
#include "core/monica-model.h"
#include "core/monica-parameters.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "soil/soil.h"
#include "tools/helper.h"
#include "trace_common.h"

using namespace std;
using namespace Tools;
using namespace Climate;
using namespace json11;
using namespace monica;
using namespace monica::cropmodule;

static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

static void dump_crop_module_phenology(const std::string &PATH, const CropModule &OBJ) {
  TD(vc_Declination);
  TD(vc_AstronomicDayLenght);
  TD(vc_EffectiveDayLength);
  TD(vc_PhotoperiodicDaylength);
  TD(vc_PhotActRadiationMean);
  TD(vc_ClearDayRadiation);
  TD(vc_OvercastDayRadiation);
  TD(vc_ExtraterrestrialRadiation);
  TD(vc_GlobalRadiation);

  TD(vc_OxygenDeficit);
  TI(vc_TimeUnderAnoxia);

  TI(vc_DevelopmentalStage);
  TD_VEC(vc_CurrentTemperatureSum);
  TD(vc_CurrentTotalTemperatureSum);
  TB(vc_ErrorStatus);
  TS(vc_ErrorMessage);
  TB(vc_GrowthCycleEnded);
  TI(noOfOrgans);
  TI(noOfDevStages);
  TS(cropParams.cultivarParams.pc_CultivarId);

  TI(vc_AnthesisDay);
  TI(vc_MaturityDay);
  TB(vc_MaturityReached);

  TD(vc_DaylengthFactor);

  TD(vc_VernalisationFactor);
  TD(vc_VernalisationDays);

  TD(vc_RelativeTotalDevelopment);

  TD(vc_KcFactor);

  TD(vc_CropHeight);
  TD(vc_CropDiameter);

  TD(vc_LeafAreaIndex);
  TD(vc_GreenAreaIndex);

  TD(vc_SoilCoverage);
}

// Replicates just the step()-body excerpt this checkpoint needs, for one day.
// Matches step()'s call order exactly (see crop-module.cpp:517-710), minus
// everything not yet ported.
static void phenology_day_step(CropModule *cm, double meanAirTemperature, double globalRadiation,
                               double sunshineHours, Date currentDate, int julianDayOverride = -1) {
  const auto &pc_BaseDaylength = cm->cropParams.cultivarParams.pc_BaseDaylength;
  const auto &pc_CriticalOxygenContent = cm->cropParams.speciesParams.pc_CriticalOxygenContent;
  const auto &pc_DaylengthRequirement = cm->cropParams.cultivarParams.pc_DaylengthRequirement;
  const auto pc_MaxCropHeight = cm->cropParams.cultivarParams.pc_MaxCropHeight;
  const auto pc_Perennial = cm->cropParams.cultivarParams.pc_Perennial;
  const auto &pc_SpecificLeafArea = cm->cropParams.cultivarParams.pc_SpecificLeafArea;
  const auto &pc_StageKcFactor = cm->cropParams.cultivarParams.pc_StageKcFactor;
  const auto &pc_StageTemperatureSum = cm->cropParams.cultivarParams.pc_StageTemperatureSum;
  const auto &pc_VernalisationRequirement = cm->cropParams.cultivarParams.pc_VernalisationRequirement;
  const auto &speciesPs = cm->cropParams.speciesParams;

  int vs_JulianDay =
      julianDayOverride >= 0 ? julianDayOverride : int(currentDate.julianDay());

  fcRadiation(cm, vs_JulianDay, globalRadiation, sunshineHours);

  cm->vc_OxygenDeficit =
      fcOxygenDeficiency(cm, pc_CriticalOxygenContent[cm->vc_DevelopmentalStage]);

  size_t old_DevelopmentalStage = cm->vc_DevelopmentalStage;

  // start accumulating temperature sums only after dormancy
  if (!cm->perennialCropDormancyPeriodEndDate.isValid()) {
    cm->perennialCropDormancyPeriodEndDate =
        speciesPs.dormancyEndDoy == 0
            ? currentDate
            : Date(1, 1, currentDate.year()) + (speciesPs.dormancyEndDoy - 1);
  }
  if (!pc_Perennial || currentDate >= cm->perennialCropDormancyPeriodEndDate) {
    fcCropDevelopmentalStage(cm, meanAirTemperature, cm->soilColumn->layers[0].vs_SoilMoisture_m3,
                             cm->soilColumn->layers[0].vs_FieldCapacity,
                             cm->soilColumn->layers[0].vs_PermanentWiltingPoint, currentDate);
  }

  if (isAnthesisDay(cm, old_DevelopmentalStage, cm->vc_DevelopmentalStage)) {
    cm->vc_AnthesisDay = vs_JulianDay;
  } else if (isMaturityDay(cm, old_DevelopmentalStage, cm->vc_DevelopmentalStage)) {
    cm->vc_MaturityDay = vs_JulianDay;
    cm->vc_MaturityReached = true;
  }

  cm->vc_DaylengthFactor =
      fcDaylengthFactor(cm, pc_DaylengthRequirement[cm->vc_DevelopmentalStage],
                        cm->vc_EffectiveDayLength, cm->vc_PhotoperiodicDaylength,
                        pc_BaseDaylength[cm->vc_DevelopmentalStage]);

  tie(cm->vc_VernalisationFactor, cm->vc_VernalisationDays) = fcVernalisationFactor(
      cm, meanAirTemperature, pc_VernalisationRequirement[cm->vc_DevelopmentalStage],
      cm->vc_VernalisationDays);

  if (cm->vc_TotalTemperatureSum == 0.0) {
    cm->vc_RelativeTotalDevelopment = 0.0;
  } else {
    cm->vc_RelativeTotalDevelopment = cm->vc_CurrentTotalTemperatureSum / cm->vc_TotalTemperatureSum;
  }

  if (cm->vc_DevelopmentalStage == 0) {
    cm->vc_KcFactor = cm->siteParams->bareSoilKcFactor;
  } else {
    cm->vc_KcFactor = fcKcFactor(cm, pc_StageTemperatureSum[cm->vc_DevelopmentalStage],
                                 cm->vc_CurrentTemperatureSum[cm->vc_DevelopmentalStage],
                                 pc_StageKcFactor[cm->vc_DevelopmentalStage],
                                 pc_StageKcFactor[cm->vc_DevelopmentalStage - 1]);
  }

  if (cm->vc_DevelopmentalStage > 0) {
    auto maxCropHeight = pc_MaxCropHeight;

    fcCropSize(cm, maxCropHeight);

    fcCropGreenArea(cm, meanAirTemperature, cm->vc_OrganGrowthIncrement[OId::LEAF],
                    cm->vc_OrganSenescenceIncrement[OId::LEAF],
                    pc_SpecificLeafArea[cm->vc_DevelopmentalStage - 1],
                    pc_SpecificLeafArea[cm->vc_DevelopmentalStage], pc_SpecificLeafArea[1],
                    pc_StageTemperatureSum[cm->vc_DevelopmentalStage],
                    cm->vc_CurrentTemperatureSum[cm->vc_DevelopmentalStage]);

    cm->vc_SoilCoverage = fcSoilCoverage(cm);
  }

  cm->noOfCropSteps++;
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: crop_module_phenology_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];
  std::string pathToClimateCsv = argv[2];
  int numDaysA = atoi(argv[3]);

  // --- build CentralParameterProvider, exactly like crop_module_ref_main.cpp ---
  std::string pathOfSimJson, simFileName;
  std::tie(pathOfSimJson, simFileName) = splitPathToFile(pathToSimJson);

  auto simj = readAndParseJsonFile(pathToSimJson);
  if (simj.failure()) {
    for (const auto &e : simj.errors) fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  auto simm = simj.result.object_items();
  simm["sim.json"] = pathToSimJson;

  auto pathToCropJson = simm["crop.json"].string_value();
  if (!isAbsolutePath(pathToCropJson)) simm["crop.json"] = pathOfSimJson + pathToCropJson;
  auto pathToSiteJson = simm["site.json"].string_value();
  if (!isAbsolutePath(pathToSiteJson)) simm["site.json"] = pathOfSimJson + pathToSiteJson;
  auto pathToClimateCSVInSim = simm["climate.csv"].string_value();
  if (!isAbsolutePath(pathToClimateCSVInSim))
    simm["climate.csv"] = pathOfSimJson + pathToClimateCSVInSim;

  std::map<std::string, Json> ps;
  ps["sim"] = Json(simm);
  ps["crop"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["crop.json"].string_value()), false)),
      false);
  ps["site"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["site.json"].string_value()), false)),
      false);

  auto env = createEnvJsonFromJsonObjects(ps);
  auto envParams = env["params"];

  CentralParameterProvider cpp;
  std::string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["Wessolek2009"] =
      Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchten"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenVereecken"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenToth"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenToth;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["Toth"] = Soil::updateUnsetPwpFcSatFromToth;

  centralparameterprovider::merge(&cpp, envParams);

  auto model = makeMonicaModel(cpp);

  std::string monicaParametersDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));

  CropParameters wheatCropParams;
  cropparameters::merge(&wheatCropParams, load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));

  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));

  auto noopFireEvent = [](std::string) {};
  auto noopAddOrganicMatter = [](std::map<size_t, double>, double) {};
  auto noopGetSnowDepth = [](double) { return std::make_pair(0.0, 0.0); };

  // === Scenario A: real wheat, real climate, started past germination ===
  {
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    setStage(cm.get(), 1);

    CSVViaHeaderOptions copts{
        Json::object{{"no-of-climate-file-header-lines", 2}, {"csv-separator", std::string(",")}}};
    auto climRes = readClimateDataFromCSVFileViaHeaders(pathToClimateCsv, copts);
    if (climRes.failure()) {
      for (const auto &e : climRes.errors) fprintf(stderr, "%s\n", e.c_str());
      return 1;
    }
    DataAccessor da = climRes.result;

    int n = (int)da.noOfStepsPossible();
    if (numDaysA > 0 && numDaysA < n) n = numDaysA;

    for (int day = 0; day < n; day++) {
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      Date currentDate = da.dateForStep(day);

      phenology_day_step(cm.get(), tavg, globrad, 0.0, currentDate);

      trace::set_day(day);
      dump_crop_module_phenology("cropModuleA", *cm);
    }
  }

  // === Scenario B: synthetic perennial/short-day/WangEngel/germination ===
  {
    CropParameters synthCropParams = wheatCropParams;
    synthCropParams.cultivarParams.pc_Perennial = true;
    synthCropParams.cultivarParams.pc_MinTempDev_WE = 0.0;
    synthCropParams.cultivarParams.pc_OptTempDev_WE = 20.0;
    synthCropParams.cultivarParams.pc_MaxTempDev_WE = 35.0;
    synthCropParams.__enable_vernalisation_factor_fix__ = false; // overrides the CropModuleParameters default
    synthCropParams.speciesParams.dormancyStartDoy =
        6; // day-of-year 6, crossed quickly by the synthetic dates below
    synthCropParams.speciesParams.dormancyEndDoy = 3;
    // shrink the stage temperature sums so multiple stages pass within a
    // handful of synthetic days
    for (auto &v : synthCropParams.cultivarParams.pc_StageTemperatureSum) v = std::min(v, 5.0);
    for (auto &v : synthCropParams.cultivarParams.pc_DaylengthRequirement) v = -std::abs(v) - 1.0; // short-day

    CropParameters perennialNextSeason = synthCropParams;
    perennialNextSeason.cultivarParams.pc_CultivarId = "synthetic-next-season";

    CropModuleParameters synthCropModParams = cpp.userCropParameters;
    synthCropModParams.__enable_Phenology_WangEngelTemperatureResponse__ = true;
    synthCropModParams.__enable_vernalisation_factor_fix__ = true;

    auto cm = makeCropModule(model->soilColumn.get(), &synthCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &synthCropModParams, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    cm->perennialCropParams = kj::heap<CropParameters>(perennialNextSeason);
    cm->cropParams.cultivarParams.pc_CultivarId = "synthetic-season-1";

    // Hand-written weather: rising soil temperature (germination), one day
    // with globalRadiation<=0 and nonzero sunshine hours (fcRadiation's other
    // branch), then enough warm days to cycle through several stages and
    // cross dormancyStartDoy.
    struct Day {
      double meanAirTemperature, globalRadiation, sunshineHours;
      int julianDay;
    };
    Day days[] = {
        {-2.0, 8.0, 0.0, 1},   {-1.0, 8.0, 0.0, 2},   {0.0, 0.0, 6.0, 3},
        {5.0, 10.0, 0.0, 4},   {10.0, 12.0, 0.0, 5},  {15.0, 14.0, 0.0, 6},
        {16.0, 14.0, 0.0, 7},  {17.0, 15.0, 0.0, 8},  {18.0, 15.0, 0.0, 9},
        {19.0, 16.0, 0.0, 10}, {20.0, 16.0, 0.0, 11}, {21.0, 17.0, 0.0, 12},
        {22.0, 17.0, 0.0, 13}, {20.0, 16.0, 0.0, 14}, {18.0, 15.0, 0.0, 15},
        {16.0, 14.0, 0.0, 16}, {14.0, 13.0, 0.0, 17}, {12.0, 12.0, 0.0, 18},
        {10.0, 11.0, 0.0, 19}, {8.0, 10.0, 0.0, 20},
    };

    for (size_t day = 0; day < sizeof(days) / sizeof(days[0]); day++) {
      // rises above pc_BaseTemperature[0] on day index 3 (julianDay 4)
      cm->soilColumn->layers[0].vs_SoilTemperature = -3.0 + double(day);

      Date currentDate = Date(days[day].julianDay, 1, 2000, false, false, true);
      phenology_day_step(cm.get(), days[day].meanAirTemperature, days[day].globalRadiation,
                         days[day].sunshineHours, currentDate, days[day].julianDay);

      trace::set_day((int)day);
      dump_crop_module_phenology("cropModuleB", *cm);
    }
  }

  return 0;
}
