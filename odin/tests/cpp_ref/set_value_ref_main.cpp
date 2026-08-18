/* Differential-test driver for the SetValue workstep (src/worksteps/
 * set-value.cpp), ported now that its phase 6 deferral reason (needed OId/
 * buildOutputTable/the Spec evaluator - phase 7) no longer applies.
 *
 * Reuses phase 5 checkpoint 7's exact construction/growth-run pattern (see
 * crop_module_yield_ref_main.cpp's own header comment) to reach a real,
 * non-degenerate grown wheat crop, then exercises makeSetValueWorkstep +
 * workstep::apply directly against the live MonicaModel/CropModule/
 * SoilColumn.
 *
 * Only the two output ids the real C++ buildOutputTable actually registers
 * a setf for among the 21 this port ported (Stage, Mois) are exercised -
 * the constant-scalar merge branch (Stage), the constant-scalar-broadcast-
 * across-a-layer-range branch (Mois with a [from,to] var), and the
 * oid-reference merge branch (Mois's value itself another output id,
 * re-evaluated live at apply time). The `["=", a, op, b]` arithmetic-
 * expression value syntax is deliberately NOT exercised here: the real C++
 * buildPrimitiveCalcExpression machinery is fully implemented upstream, but
 * this port's build_output.odin explicitly leaves that branch unbuilt
 * (Set_Value_Get_Value stays .NONE) as a separate, still-deferred
 * sub-feature - so a scenario using it would show an expected, documented
 * divergence rather than a bug, and doesn't belong in a byte-identical
 * comparison.
 *
 * Usage: set_value_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_set_value.sh.
 */
#include <cstdio>
#include <map>
#include <string>

#include "climate/climate-file-io.h"
#include "core/monica-model.h"
#include "core/monica-parameters.h"
#include "io/output.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "run/workstep.h"
#include "soil/soil.h"
#include "tools/helper.h"
#include "trace_common.h"
#include "worksteps/set-value.h"

using namespace std;
using namespace Tools;
using namespace Climate;
using namespace json11;
using namespace monica;

static const double ATM_CO2 = 380.0;
static const double ATM_O3 = 60.0;

static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

static void dump_state(const std::string &PATH, const MonicaModel &M) {
  const CropModule &OBJ = *M.currentCropModule;
  trace::line_int(trace::join(PATH, "vc_DevelopmentalStage"), (long long)OBJ.vc_DevelopmentalStage);
  trace::line_f64(trace::join(PATH, "vc_CurrentTotalTemperatureSum"), OBJ.vc_CurrentTotalTemperatureSum);
  trace::line_f64(trace::join(PATH, "vc_AbovegroundBiomass"), OBJ.vc_AbovegroundBiomass);
  for (int i = 0; i < 3; i++) {
    trace::line_f64(
        trace::index(PATH + ".soilColumn.vs_SoilMoisture_m3", i),
        M.soilColumn->layers.at(i).vs_SoilMoisture_m3);
  }
  auto ci = M.currentEvents.find("SetValue");
  trace::line_bool(trace::join(PATH, "currentEvents.hasSetValue"), ci != M.currentEvents.end());
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: set_value_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  std::string monicaParametersDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));

  CropParameters wheatCropParams;
  cropparameters::merge(&wheatCropParams, load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));

  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));

  auto noFireEvent = [](string) {};
  auto realAddOrganicMatter = [&](std::map<size_t, double> layer2amount, double nconc) {
    soilorganic::addOrganicMatter(model->soilOrganic.get(), model->currentCropModule->residueParams,
                                  layer2amount, nconc);
  };
  auto realGetSnowDepth = [&](double avgAirTemp) {
    return soilmoisture::getSnowDepthAndCalcTemperatureUnderSnow(model->soilMoisture.get(), avgAirTemp);
  };

  model->currentCropModule =
      makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                     &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                     noFireEvent, realAddOrganicMatter, realGetSnowDepth, nullptr);
  auto *cm = model->currentCropModule.get();

  model->soilMoisture->cropModule = cm;
  model->soilOrganic->cropModule = cm;
  model->soilTransport->cropModule = cm;

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

  // silent growth run - only the final state matters for this oracle
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
    double et0 = -1.0;

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);
    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       (int)da.julianDayForStep(day), et0);
    cropmodule::step(cm, tavg, tmax, tmin, globrad, 0.0, currentDate, (relhumid / 100.0), wind,
                     model->envPs.p_WindSpeedHeight, ATM_CO2, ATM_O3, precip, -1.0);
    soilorganic::step(model->soilOrganic.get(), tavg, precip, wind);
    soiltransport::step(model->soilTransport.get());
  }

  // scenario 0: constant scalar -> Stage setf (cropmodule::setStage)
  trace::set_day(0);
  {
    Json j = Json::object{{"var", std::string("Stage")}, {"value", 3}};
    auto ws = makeSetValueWorkstep(j);
    workstep::apply(&ws, model.get());
  }
  dump_state("s", *model);

  // scenario 1: constant scalar broadcast across a layer range -> Mois setf
  trace::set_day(1);
  {
    Json j = Json::object{{"var", Json::array{std::string("Mois"), Json::array{1, 3}}}, {"value", 0.28}};
    auto ws = makeSetValueWorkstep(j);
    workstep::apply(&ws, model.get());
  }
  dump_state("s", *model);

  // scenario 2: oid-reference value (re-evaluated live at apply time),
  // targeting a single layer
  trace::set_day(2);
  {
    Json j = Json::object{
        {"var", Json::array{std::string("Mois"), Json::array{2, 2}}},
        {"value", Json::array{std::string("AbBiom")}}};
    auto ws = makeSetValueWorkstep(j);
    workstep::apply(&ws, model.get());
  }
  dump_state("s", *model);

  return 0;
}
