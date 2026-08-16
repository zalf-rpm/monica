/* Differential-test driver for phase 4 module 2b: soilmoisture.cpp itself
 * (module 2a already proved snow-component/frost-component in isolation).
 *
 * Same "real MonicaModel, bare soil" approach as soil_temperature_ref_main.cpp
 * and snow_frost_ref_main.cpp: builds a real MonicaModel via the unmodified
 * makeMonicaModel (so model->soilMoisture, incl. its real snowComponent/
 * frostComponent, is constructed by production code), leaves
 * currentCropModule/soilMoisture->cropModule null throughout (bare soil -
 * every CropModule-conditional branch in soilmoisture.cpp becomes dead code,
 * deferred to phase 5 - see soil_moisture.odin's package comment), and drives
 * soilmoisture::step directly over a real multi-year climate record.
 *
 * vs_GroundwaterDepth is not produced anywhere yet (monicamodel::
 * groundwaterDepthForDate is phase 6 orchestration) - synthesized as a
 * deterministic alternating shallow/deep sequence, identical on both sides,
 * to exercise both percolationWithGroundwater and percolationWithoutGroundwater.
 * et0 is -1.0 throughout, matching climate-min.csv's real shape (it has no et0
 * column, so the real pipeline would compute this exact -1.0 too).
 *
 * Usage: soil_moisture_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_soil_moisture.sh.
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

static void dump_soil_moisture_module_parameters(const std::string &PATH,
                                                  const SoilMoistureModuleParameters &OBJ) {
  TD(pm_SaturatedHydraulicConductivity);
  TD(pm_SurfaceRoughness);
  TD(pm_GroundwaterDischarge);
  TD(pm_HydraulicConductivityRedux);
  TD(pm_SnowAccumulationTresholdTemperature);
  TD(pm_KcFactor);
  TD(pm_TemperatureLimitForLiquidWater);
  TD(pm_CorrectionSnow);
  TD(pm_CorrectionRain);
  TD(pm_SnowMaxAdditionalDensity);
  TD(pm_NewSnowDensityMin);
  TD(pm_SnowRetentionCapacityMin);
  TD(pm_RefreezeParameter1);
  TD(pm_RefreezeParameter2);
  TD(pm_RefreezeTemperature);
  TD(pm_SnowMeltTemperature);
  TD(pm_SnowPacking);
  TD(pm_SnowRetentionCapacityMax);
  TD(pm_EvaporationZeta);
  TD(pm_XSACriticalSoilMoisture);
  TD(pm_MaximumEvaporationImpactDepth);
  TD(pm_MaxPercolationRate);
  TD(pm_MoistureInitValue);
}

static void dump_soil_moisture(const std::string &PATH, const SoilMoisture &OBJ) {
  TD(vm_EvaporatedFromSurface);
  // soilColumn/siteParameters/envPs/cropPs are C++ references, not pointers -
  // always "set" once constructed, matching what the Odin walker's pointer
  // skip emits for the equivalent (never-nil) Odin pointer fields.
  trace::line_str(trace::join(PATH, "soilColumn"), "<ptr:set>");
  trace::line_str(trace::join(PATH, "siteParameters"), "<ptr:set>");
  dump_soil_moisture_module_parameters(trace::join(PATH, "params"), OBJ.params);
  trace::line_str(trace::join(PATH, "envPs"), "<ptr:set>");
  trace::line_str(trace::join(PATH, "cropPs"), "<ptr:set>");
  TI(numberOfMoistureLayers);
  TI(numberOfSoilLayers);

  TD(vm_ActualEvaporation);
  TD(vm_ActualEvapotranspiration);
  TD(vm_ActualTranspiration);
  TD_VEC(vm_AvailableWater);
  TD(vm_CapillaryRise);
  TD_VEC(pm_CapillaryRiseRate);
  TD_VEC(vm_CapillaryWater);
  TD_VEC(vm_CapillaryWater70);
  TD_VEC(vm_Evaporation);
  TD_VEC(vm_Evapotranspiration);
  TD_VEC(vm_FieldCapacity);
  TD(vm_FluxAtLowerBoundary);
  TD_VEC(vm_GravitationalWater);
  TD(vm_GrossPrecipitation);
  TD(vm_GroundwaterAdded);
  TD(vm_GroundwaterDischarge);
  TI(vm_GroundwaterTableLayer);
  TD_VEC(vm_HeatConductivity);
  TD(vm_HydraulicConductivityRedux);
  TD(vm_Infiltration);
  TD(vm_Interception);
  TD(vc_KcFactor);
  TD_VEC(vm_Lambda);
  TD(vs_Latitude);
  TD_VEC(vm_LayerThickness);
  TD(pm_LayerThickness);
  TD(pm_LeachingDepth);
  TI(pm_LeachingDepthLayer);
  TD(pm_MaxPercolationRate);
  TD(vc_NetPrecipitation);
  TB(vm_LastWettingWasRain);
  TD(vm_Ke);
  TD(vm_irrigFwEvent);
  TB(vm_irrigIsDripEvent);
  TD(vw_NetRadiation);
  TD_VEC(vm_PermanentWiltingPoint);
  TD(vc_PercentageSoilCoverage);
  TD_VEC(vm_PercolationRate);
  TD(vm_ReferenceEvapotranspiration);
  TD_VEC(vm_ResidualEvapotranspiration);
  TD_VEC(vm_SaturatedHydraulicConductivity);

  TD_VEC(vm_SoilMoisture);
  TD(vm_SoilMoisture_crit);
  TD(vm_SoilMoistureDeficit);
  TD_VEC(vm_SoilPoreVolume);
  TD(vc_StomataResistance);
  TD(vm_SurfaceRoughness);
  TD(vm_SurfaceRunOff);
  TD(vm_SumSurfaceRunOff);
  TD(vm_SurfaceWaterStorage);
  TD(pt_TimeStep);
  TD(vm_TotalWaterRemoval);
  TD_VEC(vm_Transpiration);
  TD_VEC(vm_WaterFlux);
  TD(vm_XSACriticalSoilMoisture);

  dump_snow_component(trace::join(PATH, "snowComponent"), *OBJ.snowComponent);
  dump_frost_component(trace::join(PATH, "frostComponent"), *OBJ.frostComponent);
  TP(cropModule);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: soil_moisture_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  // --- build the real MonicaModel; model->soilMoisture (incl. its real
  // snowComponent/frostComponent) is constructed by the unmodified
  // initializeFromParams ---
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
  model->soilMoisture->cropModule = nullptr;

  for (int day = 0; day < n; day++) {
    double tmin = da.dataForTimestep(Climate::tmin, day);
    double tmax = da.dataForTimestep(Climate::tmax, day);
    double tavg = da.dataForTimestep(Climate::tavg, day);
    double wind = da.dataForTimestep(Climate::wind, day);
    double globrad = da.dataForTimestep(Climate::globrad, day);
    double precip = da.dataForTimestep(Climate::precip, day);
    double relhumid = da.dataForTimestep(Climate::relhumid, day);
    int julday = (int)da.julianDayForStep(day);

    // synthetic, deterministic groundwater depth sequence - identical on both
    // sides, not produced by any not-yet-ported orchestration code. Exercises
    // both percolationWithGroundwater (shallow) and percolationWithoutGroundwater
    // (deep) repeatedly over the run.
    double vs_GroundwaterDepth = (day % 40) < 15 ? 3.0 : 15.0;
    double et0 = -1.0; // climate-min.csv has no et0 column

    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       julday, et0);

    trace::set_day(day);
    dump_soil_moisture("soilMoisture", *model->soilMoisture);
  }

  return 0;
}
