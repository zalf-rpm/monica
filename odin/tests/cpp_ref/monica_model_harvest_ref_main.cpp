/* Differential-test driver for phase 6 checkpoint 2's second prerequisite:
 * monicamodel::harvestCurrentCrop/incorporateCurrentCrop
 * (src/core/monica-model.cpp), deferred from checkpoint 1 since they need
 * HarvestData::Spec/OptCarbonManagementData (now Harvest_Spec/
 * Harvest_Opt_Carbon_Management_Data, hoisted into monica_model.odin) and
 * the cropmodule:: yield getters this phase's first prerequisite ported.
 *
 * Reuses phase 5 checkpoint 7's exact construction/growth-run pattern (see
 * crop_module_yield_ref_main.cpp) to reach non-degenerate biomass, then
 * exercises harvestCurrentCrop's three branches plus incorporateCurrentCrop,
 * chained on the same live CropModule/MonicaModel (harvestCurrentCrop only
 * sets clearCropUponNextDay - it doesn't zero any CropModule biomass field
 * itself, that happens via dailyReset's crop removal, phase 6 checkpoint 1 -
 * so each scenario's state is the previous scenario's result, same
 * "chained, not independent" approach crop_module_yield_ref uses for
 * applyCutting).
 *
 * Scenarios (indexed as trace "days"):
 *   0: harvestCurrentCrop, exported=true, empty spec, old default behavior
 *      (optCarbonConservation=false) - useSecondaryCropYields from
 *      simPs.p_UseSecondaryYields (true in sim-min.json)
 *   1: harvestCurrentCrop, exported=true, empty spec, optCarbonConservation
 *      branch (humus-balance residue split)
 *   2: harvestCurrentCrop, exported=true, empty spec, optCarbonConservation
 *      + cropUsage=greenManure (forces fractionToBeLeftOnField=1.0)
 *   3: harvestCurrentCrop, detailed spec (organ2specVal covers organs 1
 *      and 3, one incorporated one left as overlay)
 *   4: harvestCurrentCrop, exported=false, empty spec (the "total plant"
 *      else-branch)
 *   5: incorporateCurrentCrop
 *
 * Usage: monica_model_harvest_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_monica_model_harvest.sh.
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

static void dump_model_harvest(const std::string &PATH, const MonicaModel &OBJ) {
  TD(optCarbonExportedResidues);
  TD(optCarbonReturnedResidues);
  TD(humusBalanceCarryOver);
  TB(clearCropUponNextDay);
}

static void dump_soil_organic_top3(const std::string &PATH, const SoilOrganic &OBJ) {
  for (int i = 0; i < 3; i++) {
    auto p2 = trace::index(PATH, i);
    trace::line_int(p2 + ".vo_AOM_Pool.size", (long long)OBJ.soilColumn.layers.at(i).vo_AOM_Pool.size());
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: monica_model_harvest_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  model->sumOrganicFertilizerDM = 500.0; // exercised by scenarios 1-2's optCarbonConservation branch

  // scenario 0: harvestCurrentCrop, exported=true, empty spec, old default behavior
  trace::set_day(0);
  {
    HarvestData::Spec spec;
    HarvestData::OptCarbonManagementData ocmd; // optCarbonConservation=false (default)
    monicamodel::harvestCurrentCrop(model.get(), true, spec, ocmd, 0);
  }
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  // scenario 1: harvestCurrentCrop, exported=true, empty spec, optCarbonConservation
  trace::set_day(1);
  {
    HarvestData::Spec spec;
    HarvestData::OptCarbonManagementData ocmd;
    ocmd.optCarbonConservation = true;
    ocmd.cropImpactOnHumusBalance = 100.0;
    ocmd.maxResidueRecoverFraction = 0.8;
    ocmd.residueHeq = 20.0;
    ocmd.organicFertilizerHeq = 15.0;
    ocmd.cropUsage = HarvestData::biomassProduction;
    monicamodel::harvestCurrentCrop(model.get(), true, spec, ocmd, 1);
  }
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  // scenario 2: same as 1 but cropUsage=greenManure (forces fractionToBeLeftOnField=1.0)
  trace::set_day(2);
  {
    HarvestData::Spec spec;
    HarvestData::OptCarbonManagementData ocmd;
    ocmd.optCarbonConservation = true;
    ocmd.cropImpactOnHumusBalance = 100.0;
    ocmd.maxResidueRecoverFraction = 0.8;
    ocmd.residueHeq = 20.0;
    ocmd.organicFertilizerHeq = 15.0;
    ocmd.cropUsage = HarvestData::greenManure;
    monicamodel::harvestCurrentCrop(model.get(), true, spec, ocmd, 0);
  }
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  // scenario 3: detailed spec covering organs 1 (leaf) and 3 (fruit)
  trace::set_day(3);
  {
    HarvestData::Spec spec;
    HarvestData::Spec::Value v1;
    v1.exportPercentage = 70.0;
    v1.incorporate = true;
    spec.organ2specVal[1] = v1;
    HarvestData::Spec::Value v3;
    v3.exportPercentage = 90.0;
    v3.incorporate = false;
    spec.organ2specVal[3] = v3;
    HarvestData::OptCarbonManagementData ocmd;
    monicamodel::harvestCurrentCrop(model.get(), true, spec, ocmd, 0);
  }
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  // scenario 4: exported=false, empty spec - the "total plant" else-branch
  trace::set_day(4);
  {
    HarvestData::Spec spec;
    HarvestData::OptCarbonManagementData ocmd;
    monicamodel::harvestCurrentCrop(model.get(), false, spec, ocmd, 0);
  }
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  // scenario 5: incorporateCurrentCrop
  trace::set_day(5);
  monicamodel::incorporateCurrentCrop(model.get());
  dump_model_harvest("model", *model);
  dump_soil_organic_top3("so", *model->soilOrganic);

  return 0;
}
