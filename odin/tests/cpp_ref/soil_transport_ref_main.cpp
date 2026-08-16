/* Differential-test driver for phase 4's soiltransport.cpp.
 *
 * Same "real MonicaModel, bare soil" approach as the soiltemperature/
 * soilmoisture checkpoints. Unlike those, SoilTransport::step reads soil
 * layer NO3/water-flux state that the already-ported-and-verified
 * soilmoisture::step actually produces (vs_SoilWaterFlux, vs_FluxAtLowerBoundary),
 * so this driver chains soilmoisture::step before soiltransport::step each
 * day - real production data flow, not another synthetic input, since
 * soilmoisture is already proven identical. soilorganic::step (which would
 * normally run between them and update vs_SoilNO3 via mineralisation) is not
 * ported yet, so vs_SoilNO3 evolves purely through soiltransport's own
 * mass-conservation math day over day starting from the fixture's initial
 * value - a valid, self-contained test of soiltransport in isolation.
 *
 * Usage: soil_transport_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_soil_transport.sh.
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

static void dump_soil_transport_module_parameters(const std::string &PATH,
                                                   const SoilTransportModuleParameters &OBJ) {
  TD(pq_DispersionLength);
  TD(pq_AD);
  TD(pq_DiffusionCoefficientStandard);
  TD(pq_NDeposition);
}

static void dump_soil_transport(const std::string &PATH, const SoilTransport &OBJ) {
  TP(soilColumn);
  dump_soil_transport_module_parameters(trace::join(PATH, "modParams"), OBJ.modParams);
  TP(siteParams);
  TP(envParams);
  TP(cropModParams);

  TD_VEC(vq_Convection);
  TD_VEC(vq_DiffusionCoeff);
  TD_VEC(vq_Dispersion);
  TD_VEC(vq_DispersionCoeff);
  TD(vq_LeachingAtBoundary);
  TD_VEC(vc_NUptakeFromLayer);
  TD_VEC(vq_PoreWaterVelocity);
  TD_VEC(vs_SoilMineralNContent);
  TD_VEC(vq_SoilNO3);
  TD_VEC(vq_SoilNO3_aq);
  TD(vq_TimeStep);
  TD_VEC(vq_TotalDispersion);
  TD_VEC(vq_PercolationRate);

  TP(cropModule);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: soil_transport_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  // --- build the real MonicaModel; model->soilMoisture and model->soilTransport
  // are both constructed by the unmodified initializeMonicaModelFromParams ---
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
  model->soilTransport->cropModule = nullptr;

  for (int day = 0; day < n; day++) {
    double tmin = da.dataForTimestep(Climate::tmin, day);
    double tmax = da.dataForTimestep(Climate::tmax, day);
    double tavg = da.dataForTimestep(Climate::tavg, day);
    double wind = da.dataForTimestep(Climate::wind, day);
    double globrad = da.dataForTimestep(Climate::globrad, day);
    double precip = da.dataForTimestep(Climate::precip, day);
    double relhumid = da.dataForTimestep(Climate::relhumid, day);
    int julday = (int)da.julianDayForStep(day);

    double vs_GroundwaterDepth = (day % 40) < 15 ? 3.0 : 15.0;
    double et0 = -1.0; // climate-min.csv has no et0 column

    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       julday, et0);
    soiltransport::step(model->soilTransport.get());

    trace::set_day(day);
    dump_soil_transport("soilTransport", *model->soilTransport);
  }

  return 0;
}
