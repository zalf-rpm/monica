/* Differential-test driver for phase 4's first module: soiltemperature.cpp.
 *
 * Builds a real MonicaModel from the Hohenfinow2 sim-min.json fixture, via the
 * exact same CentralParameterProvider pipeline central_params_ref_main.cpp
 * already proved byte-identical (checkpoint 3c), then calls makeMonicaModel so
 * model->soilColumn, model->soilTemperature and model->soilMoisture (incl. its
 * real snowComponent/frostComponent) are all constructed by the unmodified
 * production code.
 *
 * soiltemperature::step/calcSoilSurfaceTemperature read exactly three things
 * through the model->monica back-pointer: currentCropModule->vc_SoilCoverage,
 * soilMoisture->snowComponent->vm_SnowDepth and
 * soilMoisture->frostComponent->vm_TemperatureUnderSnow (see the "package
 * comment" in odin/monica/core/soil_temperature.odin for why the Odin port
 * takes these as explicit parameters instead of a MonicaModel pointer -
 * CropModule (phase 5) and a live, stepped SoilMoisture (a phase-4 sibling
 * module not yet ported) don't exist). This driver leaves currentCropModule
 * null throughout (bare soil - vc_SoilCoverage 0.0 on both sides, matching the
 * ternary's else-branch; the soilCoverage>0 path is phase 5's job to widen this
 * oracle for) and pokes snowComponent/frostComponent's two fields directly to a
 * synthetic, deterministic sequence each day - identical on both sides - rather
 * than running soilmoisture's own step(). That isolates soiltemperature's own
 * matrix-solve/heat-transfer arithmetic completely, while still exercising the
 * real, unmodified monica::soiltemperature::step and calcSoilSurfaceTemperature
 * (both branches of the snow check, both signs of the shading-coefficient
 * computation) over a real multi-year climate record.
 *
 * Usage: soil_temperature_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_soil_temperature.sh.
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

static void dump_soil_layer(const std::string &PATH, const SoilLayer &OBJ) {
  TD(vs_LayerThickness);
  TD(vs_SoilWaterFlux);
  TD(vs_SOM_Slow);
  TD(vs_SOM_Fast);
  TD(vs_SMB_Slow);
  TD(vs_SMB_Fast);
  TD(vs_SoilCarbamid);
  TD(vs_SoilNH4);
  TD(vs_SoilNO2);
  TD(vs_SoilNO3);
  TB(vs_SoilFrozen);
  TD(vs_SoilSandContent);
  TD(vs_SoilClayContent);
  TD(vs_SoilpH);
  TD(vs_SoilStoneContent);
  TD(vs_Lambda);
  TD(vs_FieldCapacity);
  TD(vs_Saturation);
  TD(vs_PermanentWiltingPoint);
  TS(vs_SoilTexture);
  TD(vs_SoilAmmonium);
  TD(vs_SoilNitrate);
  TD(vs_Soil_CN_Ratio);
  TD(vs_SoilMoisturePercentFC);
  TD(vs_SoilRawDensity);
  TD(vs_SoilBulkDensity);
  TD(vs_SoilOrganicCarbon);
  TD(vs_SoilOrganicMatter);
  TD(vs_SoilMoisture_m3);
  TD(vs_SoilTemperature);
  // vo_AOM_Pool deliberately not dumped here - always empty at this point in
  // the port (crop residues/fertiliser incorporation is phase 6), and
  // soiltemperature never reads it.
}

static void dump_soil_temperature_module_parameters(const std::string &PATH,
                                                     const SoilTemperatureModuleParameters &OBJ) {
  TD(pt_NTau);
  TD(pt_InitialSurfaceTemperature);
  TD(pt_BaseTemperature);
  TD(pt_QuartzRawDensity);
  TD(pt_DensityAir);
  TD(pt_DensityWater);
  TD(pt_DensityHumus);
  TD(pt_SpecificHeatCapacityAir);
  TD(pt_SpecificHeatCapacityQuartz);
  TD(pt_SpecificHeatCapacityWater);
  TD(pt_SpecificHeatCapacityHumus);
  TD(pt_SoilAlbedo);
  TD(pt_SoilMoisture);
}

static void dump_soil_temperature(const std::string &PATH, const SoilTemperature &OBJ) {
  TP(soilColumn);
  dump_soil_layer(trace::join(PATH, "soilColumnGroundLayer"), OBJ.soilColumnGroundLayer);
  dump_soil_layer(trace::join(PATH, "soilColumnBottomLayer"), OBJ.soilColumnBottomLayer);
  dump_soil_temperature_module_parameters(trace::join(PATH, "params"), OBJ.params);
  TI(noOfTempLayers);
  TI(noOfSoilLayers);
  TD_VEC(soilTemperature);
  TD_VEC(V);
  TD_VEC(volumeMatrix);
  TD_VEC(volumeMatrixOld);
  TD_VEC(B);
  TD_VEC(matrixPrimaryDiagonal);
  TD_VEC(matrixSecondaryDiagonal);
  TD_VEC(heatConductivity);
  TD_VEC(heatConductivityMean);
  TD_VEC(heatCapacity);
  TD(dampingFactor);
  TD(soilSurfaceTemperature);
  TD_VEC(solution);
  TD_VEC(matrixDiagonal);
  TD_VEC(matrixLowerTriangle);
  TD_VEC(heatFlow);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: soil_temperature_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  // --- build the real MonicaModel (soilColumn, soilTemperature, soilMoisture
  // incl. snow/frost, soilOrganic, soilTransport - all via the production
  // makeMonicaModel) ---
  auto model = makeMonicaModel(cpp);

  // --- climate ---
  // Built via the json constructor (which runs merge()), not by hand-setting
  // fields on a default-constructed CSVViaHeaderOptions: merge() derives
  // lineNoOfDataStart from lineNoOfHeaderLine+noOfHeaderLines, and skipping
  // that step leaves a stale default that crashes the CSV parser. Mirrors
  // sim-min.json's "climate.csv-options" keys exactly.
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

  for (int day = 0; day < n; day++) {
    double tmin = da.dataForTimestep(Climate::tmin, day);
    double tmax = da.dataForTimestep(Climate::tmax, day);
    double globrad = da.dataForTimestep(Climate::globrad, day);

    // synthetic, deterministic snow sequence - identical on both sides, not
    // derived from any model. Exercises both branches of
    // calcSoilSurfaceTemperature's snow check repeatedly over the run.
    double snowDepth = (day % 30) < 10 ? 50.0 : 0.0;
    double temperatureUnderSnow = -2.0 - double(day % 5);
    model->soilMoisture->snowComponent->vm_SnowDepth = snowDepth;
    model->soilMoisture->frostComponent->vm_TemperatureUnderSnow = temperatureUnderSnow;

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);

    trace::set_day(day);
    dump_soil_temperature("soilTemperature", *model->soilTemperature);
  }

  return 0;
}
