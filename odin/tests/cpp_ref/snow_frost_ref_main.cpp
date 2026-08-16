/* Differential-test driver for phase 4's snow-component/frost-component
 * sub-modules of soilmoisture.
 *
 * Both C++ files are fully self-contained (no MonicaModel/CropModule
 * dependency anywhere - confirmed by reading the whole of each file), unlike
 * soiltemperature.cpp. So this driver reuses the real, already-constructed
 * model->soilMoisture->snowComponent/frostComponent (built by the unmodified
 * makeMonicaModel -> makeSoilMoisture -> initializeFromParams chain, the same
 * "real MonicaModel" approach soil_temperature_ref_main.cpp uses) and drives
 * snowcomponent::calcSnowLayer / frostcomponent::calcSoilFrost directly, in
 * the same order and with the same "no live crop" (vc_NetPrecipitation ==
 * precipitation) simplification soil_temperature_ref_main.cpp uses - see that
 * file's header comment for the full rationale on the bare-soil scope.
 *
 * Usage: snow_frost_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_snow_frost.sh.
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

using namespace Tools;
using namespace Climate;
using namespace json11;
using namespace monica;

static void dump_snow_component(const std::string &PATH, const SnowComponent &OBJ) {
  TP(soilColumn);
  TD(vm_SnowDensity);
  TD(vm_SnowDepth);
  TD(vm_FrozenWaterInSnow);
  TD(vm_LiquidWaterInSnow);
  TD(vm_WaterToInfiltrate);
  TD(vm_maxSnowDepth);
  TD(vm_AccumulatedSnowDepth);
  TD(vm_SnowmeltTemperature);
  TD(vm_SnowAccumulationThresholdTemperature);
  TD(vm_TemperatureLimitForLiquidWater);
  TD(vm_CorrectionRain);
  TD(vm_CorrectionSnow);
  TD(vm_RefreezeTemperature);
  TD(vm_RefreezeP1);
  TD(vm_RefreezeP2);
  TD(vm_NewSnowDensityMin);
  TD(vm_SnowMaxAdditionalDensity);
  TD(vm_SnowPacking);
  TD(vm_SnowRetentionCapacityMin);
  TD(vm_SnowRetentionCapacityMax);
}

static void dump_frost_component(const std::string &PATH, const FrostComponent &OBJ) {
  TP(soilColumn);
  TD(vm_FrostDepth);
  TD(vm_accumulatedFrostDepth);
  TD(vm_NegativeDegreeDays);
  TD(vm_ThawDepth);
  TI(vm_FrostDays);
  TD_VEC(vm_LambdaRedux);
  TD(vm_TemperatureUnderSnow);
  TD(vm_HydraulicConductivityRedux);
  TD(pt_TimeStep);
  TD(pm_HydraulicConductivityRedux);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: snow_frost_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];
  std::string pathToClimateCsv = argv[2];
  int numDays = atoi(argv[3]);

  // --- build CentralParameterProvider, exactly like central_params_ref_main.cpp ---
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

  // --- build the real MonicaModel; model->soilMoisture->{snowComponent,
  // frostComponent} are constructed by the unmodified initializeFromParams ---
  auto model = makeMonicaModel(cpp);

  // --- climate ---
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

  model->currentCropModule = nullptr; // bare soil - see the file comment

  auto *sc = model->soilMoisture->snowComponent.get();
  auto *fc = model->soilMoisture->frostComponent.get();

  for (int day = 0; day < n; day++) {
    double tavg = da.dataForTimestep(Climate::tavg, day);
    double precip = da.dataForTimestep(Climate::precip, day);

    snowcomponent::calcSnowLayer(sc, tavg, precip);
    frostcomponent::calcSoilFrost(fc, tavg, sc->vm_SnowDepth);

    trace::set_day(day);
    dump_snow_component("snowComponent", *sc);
    dump_frost_component("frostComponent", *fc);
  }

  return 0;
}
