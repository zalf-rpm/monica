/* Differential-test driver for phase 6 checkpoint 6 - the checkpoint that
 * closes out phase 6: monicamodel::step/generalStep/cropStep
 * (src/core/monica-model.cpp), wiring together every earlier phase-6
 * checkpoint (MonicaModel scaffolding, all 12 worksteps, cultivation-
 * method) into the real daily orchestration entry point.
 *
 * This is the first oracle in the port that does NOT hand-step
 * soiltemperature/soilmoisture/cropmodule/soilorganic/soiltransport, does
 * NOT supply a synthetic groundwater depth, and does NOT hardcode
 * atmospheric CO2/O3 - monicamodel::step's generalStep/cropStep halves
 * resolve all of that internally now (from climateData/envPs/
 * groundwaterInformation), for real, for the first time in this port. The
 * daily loop is exactly the sequence run-monica.cpp's runMonica (phase 7,
 * not yet ported) will use: cultivationmethod::apply(date) [static
 * worksteps] -> cultivationmethod::apply(model, true) [dynamic worksteps,
 * start-of-day] -> monicamodel::step(model) [crop step + general step] ->
 * cultivationmethod::apply(model, false) [dynamic worksteps, non-start-of-
 * day] -> monicamodel::dailyReset(model) [daily-sum reset, event rotation,
 * and - unlike every earlier phase-6 oracle - the real currentCropModule
 * clear-to-null after a harvest].
 *
 * Same real crop-min.json rotation as cultivation_method_ref_main.cpp
 * ("include-from-file"/"ref" pre-resolved by hand).
 *
 * Scenarios (indexed as trace "days"):
 *   0: makeCultivationMethod() + merge() dump, before reinit.
 *   1: after reinit(startOfSeason) - dump allAbsWorksteps.size,
 *      unfinishedDynamicWorksteps.size, startDate/absStartDate/
 *      absLatestSowingDate/endDate/absEndDate, areOnlyAbsoluteWorksteps,
 *      allDynamicWorkstepsFinished.
 *   2..N: the real season end to end, dumping model state periodically and
 *      at the moment automatic harvest fires.
 *
 * Usage: monica_model_step_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_monica_model_step.sh.
 */

#include <cstdio>
#include <map>
#include <string>

#include "climate/climate-file-io.h"
#include "core/monica-model.h"
#include "core/monica-parameters.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "run/cultivation-method.h"
#include "run/workstep.h"
#include "soil/soil.h"
#include "tools/helper.h"
#include "trace_common.h"

using namespace std;
using namespace Tools;
using namespace Climate;
using namespace json11;
using namespace monica;


static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

static void dump_cm_basic(const std::string &PATH, const CultivationMethod &OBJ) {
  TI(customId);
  TS(name);
  TB(canBeSkipped);
  TB(isCoverCrop);
  TB(repeat);
  trace::line_int(trace::join(PATH, "allWorksteps.size"), (long long)OBJ.allWorksteps.size());
}

static void dump_model_bits(const std::string &PATH, const MonicaModel &OBJ) {
  TD(sumFertiliser);
  TD(dailySumFertiliser);
  TD(sumOrgFertiliser);
  TD(dailySumIrrigationWater);
  TI(cultivationMethodCount);
  TB(clearCropUponNextDay);
  TP(currentCropModule);
  TD(vs_GroundwaterDepth);
  TD(vw_AtmosphericCO2Concentration);
  TD(vw_AtmosphericO3Concentration);
  TI(p_daysWithCrop);
  TD(p_accuNStress);
  TD(p_accuWaterStress);
  TD(p_accuHeatStress);
  TD(p_accuOxygenStress);
  TD(soilColumn->layers[0].vs_SoilTemperature);
  TD(soilColumn->layers[0].vs_SoilMoisture_m3);
  if (OBJ.currentCropModule) {
    trace::line_f64(trace::join(PATH, "cropModule.vc_LeafAreaIndex"), OBJ.currentCropModule->vc_LeafAreaIndex);
    trace::line_int(
        trace::join(PATH, "cropModule.vc_DevelopmentalStage"),
        (long long)OBJ.currentCropModule->vc_DevelopmentalStage);
    trace::line_f64(trace::join(PATH, "cropModule.vc_TotalBiomass"), OBJ.currentCropModule->vc_TotalBiomass);
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: monica_model_step_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  Json speciesJson = load(monicaParametersDir + "/crops/wheat.json");
  Json cultivarJson = load(monicaParametersDir + "/crops/wheat/winter-wheat.json");
  Json residueJson = load(monicaParametersDir + "/crop-residues/wheat.json");
  Json anJson = load(monicaParametersDir + "/mineral-fertilisers/AN.json");
  Json camJson = load(monicaParametersDir + "/organic-fertilisers/CAM.json");

  Json cropJson = Json::object{
      {"cropParams", Json::object{{"species", speciesJson}, {"cultivar", cultivarJson}}},
      {"residueParams", residueJson}};

  Json cmJson = Json::object{
      {"customId", 1},
      {"name", "WW"},
      {"worksteps",
       Json::array{
           Json::object{
               {"type", "AutomaticIrrigation"},
               {"irrigateCrop", true},
               {"startStage", 4},
               {"endStage", 4},
               {"parameters",
                Json::object{
                    {"irrigationParameters",
                     Json::object{{"nitrateConcentration", Json::array{0, "mg dm-3"}}}},
                    {"amount", Json::array{17, "mm"}},
                    {"trigger_if_nFC_below_%", Json::array{90, "%"}},
                    {"calc_nFC_until_depth_m", Json::array{0.3, "m"}}}}},
           Json::object{{"date", "0000-09-22"}, {"type", "Sowing"}, {"crop", cropJson}},
           Json::object{
               {"type", "NDemandFertilization"},
               {"date", "0001-03-15"},
               {"N-demand", Json::array{40.0, "kg"}},
               {"depth", Json::array{0.3, "m"}},
               {"partition", anJson}},
           Json::object{
               {"type", "NDemandFertilization"},
               {"date", "0001-04-15"},
               {"N-demand", Json::array{80.0, "kg"}},
               {"depth", Json::array{0.3, "m"}},
               {"partition", anJson}},
           Json::object{
               {"type", "NDemandFertilization"},
               {"date", "0001-05-15"},
               {"N-demand", Json::array{40.0, "kg"}},
               {"depth", Json::array{0.3, "m"}},
               {"partition", anJson}},
           Json::object{
               {"type", "AutomaticHarvest"},
               {"latest-date", "0001-09-05"},
               {"min-%-asw", 10},
               {"max-%-asw", 99.0},
               {"max-3d-precip-sum", 2},
               {"max-curr-day-precip", 0.1},
               {"harvest-time", "maturity"},
               {"incorporateIntoLayerNo", 2}},
           Json::object{
               {"type", "OrganicFertilization"},
               {"days", 1},
               {"after", "Harvest"},
               {"amount", Json::array{15000, "kg N"}},
               {"parameters", camJson},
               {"incorporation", true},
               {"incorporateIntoLayerNo", 2}}}}};

  // scenario 0: makeCultivationMethod()
  trace::set_day(0);
  CultivationMethod cm = makeCultivationMethod(cmJson);
  dump_cm_basic("cm", cm);

  // scenario 1: reinit before season start
  trace::set_day(1);
  Date seasonStart = Date::fromIsoDateString("2020-09-22");
  cultivationmethod::reinit(&cm, seasonStart);
  trace::line_int("cm.allAbsWorksteps.size", (long long)cm.allAbsWorksteps.size());
  trace::line_int("cm.unfinishedDynamicWorksteps.size", (long long)cm.unfinishedDynamicWorksteps.size());
  trace::line_str("cm.startDate", cultivationmethod::startDate(&cm).toIsoDateString());
  trace::line_str("cm.absStartDate", cultivationmethod::absStartDate(&cm).toIsoDateString());
  trace::line_str("cm.absLatestSowingDate", cultivationmethod::absLatestSowingDate(&cm).toIsoDateString());
  trace::line_str("cm.endDate", cultivationmethod::endDate(&cm).toIsoDateString());
  trace::line_str("cm.absEndDate", cultivationmethod::absEndDate(&cm).toIsoDateString());
  trace::line_bool("cm.areOnlyAbsoluteWorksteps", cultivationmethod::areOnlyAbsoluteWorksteps(&cm));
  trace::line_bool("cm.allDynamicWorkstepsFinished", cultivationmethod::allDynamicWorkstepsFinished(&cm));
  trace::line_int("cm.staticWorksteps.size", (long long)cultivationmethod::staticWorksteps(&cm).size());
  trace::line_int(
      "cm.allDynamicWorksteps.size", (long long)cultivationmethod::allDynamicWorksteps(&cm).size());

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

  // real production daily loop, driven entirely through
  // cultivationmethod::apply's three overloads + monicamodel::step +
  // monicamodel::dailyReset - the same call sequence run-monica.cpp's
  // runMonica (phase 7, not yet ported) will use. Unlike every earlier
  // phase-6 oracle, this one no longer hand-steps soiltemperature/
  // soilmoisture/cropmodule/soilorganic/soiltransport, and no longer
  // supplies a synthetic groundwater depth or hardcoded ATM_CO2/ATM_O3 -
  // monicamodel::step's generalStep/cropStep halves now resolve all of that
  // internally (from climateData/envPs/groundwaterInformation), for real,
  // for the first time in this port.
  bool harvested = false;
  for (int day = 0; day < n; day++) {
    double tmin = da.dataForTimestep(Climate::tmin, day);
    double tmax = da.dataForTimestep(Climate::tmax, day);
    double tavg = da.dataForTimestep(Climate::tavg, day);
    double wind = da.dataForTimestep(Climate::wind, day);
    double globrad = da.dataForTimestep(Climate::globrad, day);
    double precip = da.dataForTimestep(Climate::precip, day);
    double relhumid = da.dataForTimestep(Climate::relhumid, day);
    Date currentDate = da.dateForStep(day);
    model->currentStepDate = currentDate;
    model->climateData.push_back(std::map<Climate::ACD, double>{
        {Climate::tmin, tmin}, {Climate::tmax, tmax}, {Climate::tavg, tavg},
        {Climate::wind, wind}, {Climate::globrad, globrad}, {Climate::precip, precip},
        {Climate::relhumid, relhumid}});

    cultivationmethod::apply(&cm, currentDate, model.get());
    cultivationmethod::apply(&cm, model.get(), true);

    monicamodel::step(model.get());

    cultivationmethod::apply(&cm, model.get(), false);

    if (!harvested) {
      for (auto &e : model->currentEvents) {
        if (e == "AutomaticHarvest") {
          harvested = true;
          trace::set_day(1000 + day);
          dump_model_bits("model", *model);
          trace::line_str("harvestedOnDate", currentDate.toIsoDateString());
        }
      }
    }

    monicamodel::dailyReset(model.get());

    if (day % 60 == 0 || day == n - 1) {
      trace::set_day(day);
      dump_model_bits("model", *model);
    }
  }

  trace::set_day(9000);
  dump_model_bits("modelFinal", *model);
  trace::line_bool("harvested", harvested);
  trace::line_int(
      "cm.unfinishedDynamicWorksteps.sizeFinal", (long long)cm.unfinishedDynamicWorksteps.size());

  return 0;
}
