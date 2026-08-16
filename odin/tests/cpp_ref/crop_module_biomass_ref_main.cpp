/* Differential-test driver for phase 5 checkpoint 5 (biomass/dry matter +
 * stress): fcHeatStressImpact, fcFrostKill, fcDroughtImpactOnFertility,
 * fcCropNitrogen, fcCropDryMatter, fcMoveDeadRootBiomassToSoil,
 * addAndDistributeRootBiomassInSoil, calcRootDensityFactorAndSum - all in
 * src/core/crop-module.cpp.
 *
 * Extends checkpoint 4's day_step with these functions in step()'s real
 * order (crop-module.cpp:712-745): fcCropPhotosynthesis (checkpoint 4),
 * fcHeatStressImpact, fcFrostKill (gated on simParams->pc_FrostKillOn, true
 * by default and in sim-min.json), fcDroughtImpactOnFertility,
 * fcCropNitrogen, fcCropDryMatter, then fcGrossPrimaryProduction/
 * fcNetPrimaryProduction. Still not ported (checkpoint 6/7, not replicated):
 * reference evapotranspiration, fcCropWaterUptake, fcCropNUptake, every
 * fireEvent call.
 *
 * Unlike checkpoints 2-4, this driver wires two of CropModule's three
 * callback fields for real, since fcFrostKill and fcMoveDeadRootBiomassToSoil
 * (called from fcCropDryMatter) actually invoke them:
 *   - getSnowDepthAndCalcTempUnderSnow: wired to a REAL, daily-stepped
 *     SoilTemperature+SoilMoisture pair (soiltemperature::step then
 *     soilmoisture::step each day, same chaining phase 4's soilorganic
 *     checkpoint used), via soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow -
 *     needed for fcFrostKill's snow-depth branch to be more than dead code.
 *     model->soilMoisture->cropModule stays nullptr (bare soil, matching
 *     every phase-4 driver) - this checkpoint verifies CropModule's own
 *     functions, not full crop/soil coupling (checkpoint 7's job).
 *   - addOrganicMatter: NOT wired to a real SoilOrganic (already verified in
 *     phase 4; re-verifying it here would be scope creep). Instead a
 *     recording callback sums whatever map is passed and remembers the last
 *     nConcentration and a running call count - enough to confirm
 *     fcMoveDeadRootBiomassToSoil computes and passes sensible values,
 *     without needing a second full SoilOrganic instance.
 *
 * One scenario: real wheat, past germination via setStage(1), the real
 * Hohenfinow2 climate-min.csv record for a full year (NUM_DAYS=365) - long
 * enough to hit real winter frost dynamics (pc_FrostKillOn=true by default)
 * and a real growing season's worth of dry-matter accumulation.
 *
 * Usage: crop_module_biomass_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_crop_module_biomass.sh.
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

static const double ATM_CO2 = 380.0; // ppm, illustrative constant
static const double ATM_O3 = 60.0;   // ppb, illustrative constant

static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

static void dump_crop_module_biomass(const std::string &PATH, const CropModule &OBJ) {
  TD(vc_CropHeatRedux);
  TD(vc_TotalCropHeatImpact);
  TI(vc_DaysAfterBeginFlowering);

  TD(vc_LT50);
  TD(vc_LT50M);
  TD(vc_CropFrostRedux);

  TD(vc_DroughtImpactOnFertility);

  TD(vc_CriticalNConcentration);
  TD(vc_TargetNConcentration);
  TD(rootNRedux);
  TD(vc_CropNRedux);

  TD(vc_AbovegroundBiomass);
  TD(vc_BelowgroundBiomass);
  TD(vc_TotalBiomass);
  TD_VEC(vc_OrganBiomass);
  TD_VEC(vc_OrganDeadBiomass);
  TD_VEC(vc_OrganGreenBiomass);
  TD_VEC(vc_OrganGrowthIncrement);
  TD_VEC(vc_OrganSenescenceIncrement);
  TD(vc_RootBiomass);
  TD(vc_TotalBiomassNContent);
  TD(vc_CropNDemand);

  TD(vc_MaxRootingDepth);
  TD(vc_RootingDepth_m);
  TI(vc_RootingDepth);
  TI(vc_RootingZone);
  TD(vc_TotalRootLength);
  TD_VEC(vc_RootDensity);
  TD_VEC(vc_RootDiameter);
  TD(vc_MaxNUptake);
  TD(vc_CurrentTotalTemperatureSumRoot);

  TD(vc_GrossPrimaryProduction);
  TD(vc_NetPrimaryProduction);
}

// checkpoint 3/4's day_step, extended with the checkpoint-5 functions in
// step()'s real order.
static void day_step(CropModule *cm, double meanAirTemperature, double maxAirTemperature,
                     double minAirTemperature, double globalRadiation, double sunshineHours,
                     Date currentDate, bool frostKillOn) {
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

  int vs_JulianDay = int(currentDate.julianDay());

  fcRadiation(cm, vs_JulianDay, globalRadiation, sunshineHours);

  cm->vc_OxygenDeficit =
      fcOxygenDeficiency(cm, pc_CriticalOxygenContent[cm->vc_DevelopmentalStage]);

  size_t old_DevelopmentalStage = cm->vc_DevelopmentalStage;

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

    fcCropPhotosynthesis(cm, meanAirTemperature, maxAirTemperature, minAirTemperature, ATM_CO2,
                         ATM_O3, currentDate);

    fcHeatStressImpact(cm, maxAirTemperature, minAirTemperature);

    if (frostKillOn) {
      fcFrostKill(cm, maxAirTemperature, minAirTemperature);
    }

    fcDroughtImpactOnFertility(cm);

    fcCropNitrogen(cm);

    fcCropDryMatter(cm, meanAirTemperature);

    cm->vc_GrossPrimaryProduction = fcGrossPrimaryProduction(cm);
    cm->vc_NetPrimaryProduction = fcNetPrimaryProduction(cm, cm->vc_TotalRespired);
  }

  cm->noOfCropSteps++;
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: crop_module_biomass_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];
  std::string pathToClimateCsv = argv[2];
  int numDays = atoi(argv[3]);

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
  model->currentCropModule = nullptr;
  model->soilMoisture->cropModule = nullptr; // bare soil - see the file comment

  std::string monicaParametersDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));

  CropParameters wheatCropParams;
  cropparameters::merge(&wheatCropParams, load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));

  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));

  double lastOrganicMatterTotal = 0;
  double lastOrganicMatterNConc = 0;
  int organicMatterCallCount = 0;
  auto recordingAddOrganicMatter = [&](std::map<size_t, double> layer2amount, double nconc) {
    double total = 0;
    for (const auto &kv : layer2amount) total += kv.second;
    lastOrganicMatterTotal = total;
    lastOrganicMatterNConc = nconc;
    organicMatterCallCount++;
  };
  auto realGetSnowDepth = [&](double avgAirTemp) {
    return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(model->soilMoisture.get(), avgAirTemp);
  };
  auto noopFireEvent = [](std::string) {};

  auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                           &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                           noopFireEvent, recordingAddOrganicMatter, realGetSnowDepth, nullptr);
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
  if (numDays > 0 && numDays < n) n = numDays;

  bool frostKillOn = cpp.simulationParameters.pc_FrostKillOn;

  for (int day = 0; day < n; day++) {
    double tmin = da.dataForTimestep(Climate::tmin, day);
    double tmax = da.dataForTimestep(Climate::tmax, day);
    double tavg = da.dataForTimestep(Climate::tavg, day);
    double wind = da.dataForTimestep(Climate::wind, day);
    double globrad = da.dataForTimestep(Climate::globrad, day);
    double precip = da.dataForTimestep(Climate::precip, day);
    double relhumid = da.dataForTimestep(Climate::relhumid, day);
    Date currentDate = da.dateForStep(day);

    double vs_GroundwaterDepth = (day % 40) < 15 ? 3.0 : 15.0;
    double et0 = -1.0; // climate-min.csv has no et0 column

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);
    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       (int)da.julianDayForStep(day), et0);

    day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate, frostKillOn);

    trace::set_day(day);
    dump_crop_module_biomass("cropModule", *cm);
    fprintf(stdout, "%d\tcropModule.recording.lastOrganicMatterTotal\t%.17g\n", day,
            lastOrganicMatterTotal);
    fprintf(stdout, "%d\tcropModule.recording.lastOrganicMatterNConc\t%.17g\n", day,
            lastOrganicMatterNConc);
    fprintf(stdout, "%d\tcropModule.recording.organicMatterCallCount\t%d\n", day,
            organicMatterCallCount);
  }

  // === Scenario B: synthetic forced root senescence ===
  // Real wheat's pc_OrganSenescenceRate for the root organ is 0 at every
  // stage (crops/wheat/winter-wheat.json) - scenario A's dailyDeadRootBiomassIncrement
  // is therefore genuinely, correctly always 0 there, and recordingAddOrganicMatter
  // never fires. This scenario forces a small positive root senescence rate
  // to exercise fcMoveDeadRootBiomassToSoil's real addOrganicMatter call.
  {
    auto model2 = makeMonicaModel(cpp);
    model2->currentCropModule = nullptr;
    model2->soilMoisture->cropModule = nullptr;

    CropParameters senescentCropParams = wheatCropParams;
    for (auto &stage : senescentCropParams.cultivarParams.pc_OrganSenescenceRate) {
      if (!stage.empty()) stage[0] = 0.01; // root organ (index 0)
    }

    double lastOrganicMatterTotalB = 0;
    double lastOrganicMatterNConcB = 0;
    int organicMatterCallCountB = 0;
    auto recordingAddOrganicMatterB = [&](std::map<size_t, double> layer2amount, double nconc) {
      double total = 0;
      for (const auto &kv : layer2amount) total += kv.second;
      lastOrganicMatterTotalB = total;
      lastOrganicMatterNConcB = nconc;
      organicMatterCallCountB++;
    };
    auto realGetSnowDepthB = [&](double avgAirTemp) {
      return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(model2->soilMoisture.get(),
                                                                    avgAirTemp);
    };

    auto cmB = makeCropModule(model2->soilColumn.get(), &senescentCropParams, &wheatResidueParams,
                              &cpp.siteParameters, &cpp.userCropParameters,
                              &cpp.simulationParameters, noopFireEvent, recordingAddOrganicMatterB,
                              realGetSnowDepthB, nullptr);
    setStage(cmB.get(), 1);

    int nB = std::min(60, (int)da.noOfStepsPossible());
    for (int day = 0; day < nB; day++) {
      double tmin = da.dataForTimestep(Climate::tmin, day);
      double tmax = da.dataForTimestep(Climate::tmax, day);
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double wind = da.dataForTimestep(Climate::wind, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      double precip = da.dataForTimestep(Climate::precip, day);
      double relhumid = da.dataForTimestep(Climate::relhumid, day);
      Date currentDate = da.dateForStep(day);

      double vs_GroundwaterDepth = (day % 40) < 15 ? 3.0 : 15.0;
      double et0 = -1.0;

      soiltemperature::step(model2->soilTemperature.get(), tmin, tmax, globrad);
      soilmoisture::step(model2->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                         (relhumid / 100.0), tavg, wind, model2->envPs.p_WindSpeedHeight, globrad,
                         (int)da.julianDayForStep(day), et0);

      day_step(cmB.get(), tavg, tmax, tmin, globrad, 0.0, currentDate, frostKillOn);

      trace::set_day(day);
      fprintf(stdout, "%d\tcropModuleB.vc_OrganDeadBiomass[0]\t%.17g\n", day,
              cmB->vc_OrganDeadBiomass[0]);
      fprintf(stdout, "%d\tcropModuleB.recording.lastOrganicMatterTotal\t%.17g\n", day,
              lastOrganicMatterTotalB);
      fprintf(stdout, "%d\tcropModuleB.recording.lastOrganicMatterNConc\t%.17g\n", day,
              lastOrganicMatterNConcB);
      fprintf(stdout, "%d\tcropModuleB.recording.organicMatterCallCount\t%d\n", day,
              organicMatterCallCountB);
    }
  }

  return 0;
}
