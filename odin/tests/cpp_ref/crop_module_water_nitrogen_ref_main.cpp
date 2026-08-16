/* Differential-test driver for phase 5 checkpoint 6 (water + nitrogen
 * uptake): fcReferenceEvapotranspiration, fcCropWaterUptake, fcCropNUptake,
 * getEffectiveRootingDepth - all in src/core/crop-module.cpp.
 *
 * Extends checkpoint 5's day_step with these functions in step()'s real
 * order (crop-module.cpp:712-745), which also finally moves
 * fcGrossPrimaryProduction/fcNetPrimaryProduction to their real position
 * (after fcCropNUptake, not right after fcCropDryMatter as checkpoint 5's
 * driver had them for lack of anything in between yet). This closes out
 * every cropmodule:: function step() calls in its vc_DevelopmentalStage>0
 * block except the fireEvent-driven bookkeeping itself (checkpoint 7).
 *
 * climate-min.csv has no et0 column, so referenceEvapotranspiration is
 * always -1 here (matching every earlier driver's convention) and
 * fcReferenceEvapotranspiration is always the live branch, never the
 * pass-through "use climate file's et0" else branch.
 *
 * Reuses the same real getSnowDepthAndCalcTempUnderSnow /recording
 * addOrganicMatter callback wiring checkpoint 5 established - see that
 * file's header comment for the rationale.
 *
 * One scenario: real wheat, past germination via setStage(1), the real
 * Hohenfinow2 climate-min.csv record for a full year (NUM_DAYS=365).
 *
 * Usage: crop_module_water_nitrogen_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_crop_module_water_nitrogen.sh.
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

static void dump_crop_module_water_nitrogen(const std::string &PATH, const CropModule &OBJ) {
  TD(vc_ReferenceEvapotranspiration);
  TD(vc_StomataResistance);

  TD(vc_NetPrecipitation);
  TD(vc_InterceptionStorage);
  TD(vc_EvaporatedFromIntercept);
  TD(vc_PotentialTranspiration);
  TD(vc_RemainingEvapotranspiration);
  TD(vc_ActualTranspiration);
  TD(vc_ActualTranspirationDeficit);
  TD(vc_PotentialTranspirationDeficit);
  TD(vc_TranspirationReduced);
  TD(vc_TranspirationDeficit);
  TD_VEC(vc_Transpiration);
  TD_VEC(vc_TranspirationRedux);
  TD_VEC(vc_RootEffectivity);
  trace::line_f64(trace::join(PATH, "getEffectiveRootingDepth"), getEffectiveRootingDepth(&OBJ));

  TD_VEC(vs_SoilMineralNContent);
  TD_VEC(vc_NUptakeFromLayer);
  TD(vc_TotalNUptake);
  TD(vc_TotalNInput);
  TD(vc_FixedN);
  TD(vc_SumTotalNUptake);
  TD(vc_NConcentrationRoot);
  TD(vc_NConcentrationAbovegroundBiomass);

  TD(vc_GrossPrimaryProduction);
  TD(vc_NetPrimaryProduction);
}

// checkpoint 5's day_step, extended with the checkpoint-6 functions in
// step()'s real order - GPP/NPP move to their real position, after
// fcCropWaterUptake/fcCropNUptake.
static void day_step(CropModule *cm, double meanAirTemperature, double maxAirTemperature,
                     double minAirTemperature, double globalRadiation, double sunshineHours,
                     Date currentDate, bool frostKillOn, double relativeHumidity, double windSpeed,
                     double windSpeedHeight, double grossPrecipitation) {
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
  auto *soilColumn = cm->soilColumn;

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

    // calculate reference evapotranspiration - climate-min.csv has no et0
    // column, so this is always the live branch (referenceEvapotranspiration < 0)
    double referenceEvapotranspiration = -1.0;
    if (referenceEvapotranspiration < 0) {
      cm->vc_ReferenceEvapotranspiration =
          fcReferenceEvapotranspiration(cm, maxAirTemperature, minAirTemperature,
                                        relativeHumidity, meanAirTemperature, windSpeed,
                                        windSpeedHeight, ATM_CO2);
    } else {
      cm->vc_ReferenceEvapotranspiration = referenceEvapotranspiration;
    }

    fcCropWaterUptake(cm, soilColumn->vm_GroundwaterTableLayer, grossPrecipitation,
                      cm->vc_CurrentTotalTemperatureSum, cm->vc_TotalTemperatureSum);

    fcCropNUptake(cm, soilColumn->vm_GroundwaterTableLayer, cm->vc_CurrentTotalTemperatureSum,
                 cm->vc_TotalTemperatureSum);

    cm->vc_GrossPrimaryProduction = fcGrossPrimaryProduction(cm);
    cm->vc_NetPrimaryProduction = fcNetPrimaryProduction(cm, cm->vc_TotalRespired);
  }

  cm->noOfCropSteps++;
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr,
            "usage: crop_module_water_nitrogen_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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
  model->soilMoisture->cropModule = nullptr; // bare soil - see checkpoint 5's file comment

  std::string monicaParametersDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));

  CropParameters wheatCropParams;
  cropparameters::merge(&wheatCropParams, load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));

  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));

  auto noopFireEvent = [](std::string) {};
  auto noopAddOrganicMatter = [](std::map<size_t, double>, double) {};
  auto realGetSnowDepth = [&](double avgAirTemp) {
    return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(model->soilMoisture.get(), avgAirTemp);
  };

  auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                           &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                           noopFireEvent, noopAddOrganicMatter, realGetSnowDepth, nullptr);
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

    day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate, frostKillOn,
             (relhumid / 100.0), wind, model->envPs.p_WindSpeedHeight, precip);

    trace::set_day(day);
    dump_crop_module_water_nitrogen("cropModule", *cm);
  }

  return 0;
}
