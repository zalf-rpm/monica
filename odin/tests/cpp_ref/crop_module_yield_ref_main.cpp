/* Differential-test driver for phase 6 checkpoint 2's prerequisite: the
 * cropmodule:: yield/N-content getters and applyCutting
 * (src/core/crop-module.cpp), deferred from phase 5 checkpoint 5/7 to
 * "whichever checkpoint actually needs them" - the Harvest and Cutting
 * worksteps do.
 *
 * Reuses phase 5 checkpoint 7's exact construction (real MonicaModel via
 * makeMonicaModel, real wheat CropParameters/CropResidueParameters, the full
 * four-module soil chain, real callbacks) and day loop to grow a real crop
 * for NUM_DAYS days first - the yield getters need non-degenerate
 * vc_OrganBiomass/vc_TotalBiomass/vc_NConcentration* values, which only a
 * real multi-day run produces.
 *
 * Scenarios (indexed as trace "days") after the growth run:
 *   0: organIdsForPrimaryYield, getPrimaryCropYield, getSecondaryCropYield,
 *      getResidueBiomass (both useSecondaryCropYields true/false, and with
 *      an explicit alternativeCropYield), getResiduesNConcentration,
 *      getPrimaryYieldNConcentration, getResiduesNContent,
 *      getPrimaryYieldNContent, getSecondaryYieldNContent,
 *      getAbovegroundBiomassNContent - all read-only, no mutation.
 *   1: applyCutting, percentage unit, cut, on organ 1 (leaf)
 *   2: applyCutting, biomass unit, left, on organ 2 (shoot) - chained after
 *      scenario 1, same live CropModule (each scenario's starting state is
 *      the previous scenario's result, not independent - both sides see the
 *      same chain so this is still exactly comparable)
 *   3: applyCutting, LAI unit (only "left" is meaningful), on organ 1 (leaf)
 *   4: applyCutting with an empty `organs` map - exercises the
 *      pc_OrganIdsForCutting auto-fill branch (real wheat's own cutting
 *      organ list, whatever it is)
 *
 * Usage: crop_module_yield_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_crop_module_yield.sh.
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

static void dump_yield_getters(const std::string &PATH, CropModule *cm) {
  auto ids = organIdsForPrimaryYield(cm);
  int i = 0;
  for (auto id : ids) trace::line_int(trace::index(PATH + ".organIdsForPrimaryYield", i++), id);

  double primary = getPrimaryCropYield(cm);
  double secondary = getSecondaryCropYield(cm);
  trace::line_f64(PATH + ".getPrimaryCropYield", primary);
  trace::line_f64(PATH + ".getSecondaryCropYield", secondary);
  trace::line_f64(PATH + ".getResidueBiomass(true,-1)", getResidueBiomass(cm, true, -1));
  trace::line_f64(PATH + ".getResidueBiomass(false,-1)", getResidueBiomass(cm, false, -1));
  trace::line_f64(PATH + ".getResidueBiomass(true,500)", getResidueBiomass(cm, true, 500.0));
  trace::line_f64(PATH + ".getResiduesNConcentration(-1)", getResiduesNConcentration(cm, -1));
  trace::line_f64(PATH + ".getResiduesNConcentration(500)", getResiduesNConcentration(cm, 500.0));
  trace::line_f64(PATH + ".getPrimaryYieldNConcentration(-1)", getPrimaryYieldNConcentration(cm, -1));
  trace::line_f64(PATH + ".getResiduesNContent(true,-1,-1)", getResiduesNContent(cm, true, -1, -1));
  trace::line_f64(PATH + ".getPrimaryYieldNContent(-1)", getPrimaryYieldNContent(cm, -1));
  trace::line_f64(PATH + ".getSecondaryYieldNContent(-1,-1)", getSecondaryYieldNContent(cm, -1, -1));
  trace::line_f64(PATH + ".getAbovegroundBiomassNContent", getAbovegroundBiomassNContent(cm));
}

static void dump_cutting_state(const std::string &PATH, const CropModule &OBJ) {
  TD(vc_AbovegroundBiomass);
  TD(vc_TotalBiomassNContent);
  TD_VEC(vc_OrganBiomass);
  TD_VEC(vc_OrganDeadBiomass);
  TD_VEC(vc_OrganGreenBiomass);
  TD(vc_LeafAreaIndex);
  TI(vc_DevelopmentalStage);
}
#undef TD
#define TD(field) trace::line_f64(trace::join(PATH, #field), (double)(OBJ).field)
static void dump_cutting_state2(const std::string &PATH, const CropModule &OBJ) {
  TI(vc_CuttingDelayDays);
  TD(vc_exportedCutBiomass);
  TD(vc_sumExportedCutBiomass);
  TD(vc_residueCutBiomass);
  TD(vc_sumResidueCutBiomass);
  TD(cropParams.cultivarParams.pc_MaxAssimilationRate);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: crop_module_yield_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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
    step(cm, tavg, tmax, tmin, globrad, 0.0, currentDate, (relhumid / 100.0), wind,
        model->envPs.p_WindSpeedHeight, ATM_CO2, ATM_O3, precip, -1.0);
    soilorganic::step(model->soilOrganic.get(), tavg, precip, wind);
    soiltransport::step(model->soilTransport.get());
  }

  // scenario 0: read-only yield getters on the grown crop
  trace::set_day(0);
  dump_yield_getters("y", cm);
  dump_cutting_state("cropModule", *cm);
  dump_cutting_state2("cropModule", *cm);

  // scenario 1: applyCutting, percentage unit, cut, organ 1 (leaf)
  trace::set_day(1);
  {
    std::map<int, CuttingData::Value> organs;
    CuttingData::Value v;
    v.value = 0.3; // 30%
    v.unit = CuttingData::percentage;
    v.cut_or_left = CuttingData::cut;
    organs[1] = v;
    std::map<int, double> exports;
    exports[1] = 0.6; // 60% exported
    applyCutting(cm, organs, exports, 0.9);
  }
  dump_cutting_state("cropModule", *cm);
  dump_cutting_state2("cropModule", *cm);

  // scenario 2: applyCutting, biomass unit, left, organ 2 (shoot)
  trace::set_day(2);
  {
    std::map<int, CuttingData::Value> organs;
    CuttingData::Value v;
    v.value = 200.0; // leave 200 kg ha-1
    v.unit = CuttingData::biomass;
    v.cut_or_left = CuttingData::left;
    organs[2] = v;
    std::map<int, double> exports;
    exports[2] = 0.5;
    applyCutting(cm, organs, exports, 1.0);
  }
  dump_cutting_state("cropModule", *cm);
  dump_cutting_state2("cropModule", *cm);

  // scenario 3: applyCutting, LAI unit, organ 1 (leaf)
  trace::set_day(3);
  {
    std::map<int, CuttingData::Value> organs;
    CuttingData::Value v;
    v.value = 1.0; // target LAI 1.0
    v.unit = CuttingData::LAI;
    v.cut_or_left = CuttingData::left;
    organs[1] = v;
    std::map<int, double> exports;
    exports[1] = 0.4;
    applyCutting(cm, organs, exports, 0.95);
  }
  dump_cutting_state("cropModule", *cm);
  dump_cutting_state2("cropModule", *cm);

  // scenario 4: empty organs map -> pc_OrganIdsForCutting auto-fill branch
  trace::set_day(4);
  {
    std::map<int, CuttingData::Value> organs;
    std::map<int, double> exports;
    applyCutting(cm, organs, exports, 1.0);
    trace::line_int("cropModule.autoFilledOrgans.size", (long long)organs.size());
  }
  dump_cutting_state("cropModule", *cm);
  dump_cutting_state2("cropModule", *cm);

  return 0;
}
