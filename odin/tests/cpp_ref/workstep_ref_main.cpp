/* Differential-test driver for phase 6 checkpoint 2: src/run/workstep.h/.cpp
 * (the WorkstepType tag, WorkstepData variant, Workstep struct, common/
 * central-dispatch functions) and every concrete workstep's merge/apply/
 * condition/reinit, normally spread across src/worksteps/*.{h,cpp}.
 *
 * Reuses the real Hohenfinow2 crop-min.json rotation's own worksteps
 * (AutomaticIrrigation/Sowing/NDemandFertilization x3/AutomaticHarvest/
 * OrganicFertilization) with their "include-from-file"/"ref" JSON patterns
 * pre-resolved by hand (embedding the loaded species/cultivar/residue/
 * fertiliser JSON objects directly) rather than routing through
 * create-env-from-json-config - that machinery (and the full date-matching/
 * unfinishedDynamicWorksteps dispatch) is cultivation-method's job, phase 6
 * checkpoint 5, not this one. This driver mimics only as much of that
 * dispatch as individual workstep functions need to be exercised
 * correctly: dated worksteps (Sowing, NDemandFertilization) are apply()'d
 * directly on their matching simulated day; undated/dynamic worksteps
 * (AutomaticIrrigation, AutomaticHarvest) go through
 * applyWithPossibleCondition every day after one reinit() at day 0, the
 * same two dispatch paths cultivation-method.cpp itself uses.
 *
 * Synthetic JSON covers the five workstep types crop-min.json doesn't use:
 * Transplant, AutomaticSowing, Harvest, Cutting, MineralFertilization,
 * Tillage, Irrigation.
 *
 * Scenarios (indexed as trace "days"):
 *   0: merge() + makeWorkstep() for all 12 workstep types (real + synthetic
 *      JSON), dumped before any apply - exercises every merge() function.
 *   1..N: the growth-run day loop (real Hohenfinow2 climate), applying
 *      Sowing on day 0, the three NDemandFertilization worksteps on their
 *      matching days, and AutomaticIrrigation/AutomaticHarvest every day via
 *      applyWithPossibleCondition - the real rotation, end to end, up to and
 *      including automatic harvest firing.
 *   after the loop: OrganicFertilization.apply() (mirrors the fixture's
 *      "after: Harvest" dynamic trigger, applied directly here), then
 *      Tillage/Irrigation/MineralFertilization/Cutting/Harvest/Transplant/
 *      AutomaticSowing each applied once on the same (now harvested, bare)
 *      model - Cutting needs a live crop, so it re-sows first;
 *      AutomaticSowing's condition() is exercised across a few synthetic
 *      climate states rather than applied for real (no fixture needs its
 *      apply() path, and it needs a fresh un-sown model).
 *
 * Usage: workstep_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_workstep.sh.
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

static void dump_ws_common(const std::string &PATH, const Workstep &OBJ) {
  trace::line_bool(trace::join(PATH, "date.isValid"), OBJ.date.isValid());
  trace::line_str(trace::join(PATH, "date.toIsoDateString"), OBJ.date.toIsoDateString());
  TI(applyNoOfDaysAfterEvent);
  TS(afterEvent);
  TI(daysAfterEventCount);
  TB(daysAfterEventCountActivated);
  TB(isActive);
  TB(runAtStartOfDay);
  trace::line_int(trace::join(PATH, "errors.errors.size"), (long long)OBJ.errors.errors.size());
}

static void dump_model_bits(const std::string &PATH, const MonicaModel &OBJ) {
  TD(sumFertiliser);
  TD(dailySumFertiliser);
  TD(sumOrgFertiliser);
  TD(dailySumIrrigationWater);
  TI(cultivationMethodCount);
  TB(clearCropUponNextDay);
  TP(currentCropModule);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: workstep_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  // --- the real crop-min.json rotation, "include-from-file"/"ref" pre-resolved ---
  Json aiJson = Json::object{
      {"type", "AutomaticIrrigation"},
      {"irrigateCrop", true},
      {"startStage", 4},
      {"endStage", 4},
      {"parameters",
       Json::object{
           {"irrigationParameters", Json::object{{"nitrateConcentration", Json::array{0, "mg dm-3"}}}},
           {"amount", Json::array{17, "mm"}},
           {"trigger_if_nFC_below_%", Json::array{90, "%"}},
           {"calc_nFC_until_depth_m", Json::array{0.3, "m"}}}}};
  Json sowingJson =
      Json::object{{"date", "0000-09-22"}, {"type", "Sowing"}, {"crop", cropJson}};
  Json nd1Json = Json::object{
      {"type", "NDemandFertilization"},
      {"date", "0001-03-15"},
      {"N-demand", Json::array{40.0, "kg"}},
      {"depth", Json::array{0.3, "m"}},
      {"partition", anJson}};
  Json nd2Json = Json::object{
      {"type", "NDemandFertilization"},
      {"date", "0001-04-15"},
      {"N-demand", Json::array{80.0, "kg"}},
      {"depth", Json::array{0.3, "m"}},
      {"partition", anJson}};
  Json nd3Json = Json::object{
      {"type", "NDemandFertilization"},
      {"date", "0001-05-15"},
      {"N-demand", Json::array{40.0, "kg"}},
      {"depth", Json::array{0.3, "m"}},
      {"partition", anJson}};
  Json ahJson = Json::object{
      {"type", "AutomaticHarvest"},
      {"latest-date", "0001-09-05"},
      {"min-%-asw", 10},
      {"max-%-asw", 99.0},
      {"max-3d-precip-sum", 2},
      {"max-curr-day-precip", 0.1},
      {"harvest-time", "maturity"},
      {"incorporateIntoLayerNo", 2}};
  Json ofJson = Json::object{
      {"type", "OrganicFertilization"},
      {"days", 1},
      {"after", "Harvest"},
      {"amount", Json::array{15000, "kg N"}},
      {"parameters", camJson},
      {"incorporation", true},
      {"incorporateIntoLayerNo", 2}};

  // --- synthetic JSON for the 5 types crop-min.json doesn't use ---
  Json transplantJson = Json::object{
      {"date", "0000-05-01"},
      {"type", "Transplant"},
      {"crop", cropJson},
      {"initialStage", 3},
      {"initialTemperatureSum", 120.0},
      {"initialLAI", 0.3},
      {"postTransplantDelay", 5}};
  Json autoSowingJson = Json::object{
      {"type", "AutomaticSowing"},
      {"crop", cropJson},
      {"earliest-date", "0000-09-01"},
      {"latest-date", "0000-10-15"},
      {"min-temp", 5.0},
      {"days-in-temp-window", 5},
      {"temp-sum-above-base-temp", 50.0},
      {"base-temp", 0.0}};
  Json harvestJson = Json::object{
      {"type", "Harvest"}, {"exported", true}, {"incorporateIntoLayerNo", 1}};
  Json cuttingJson = Json::object{
      {"type", "Cutting"},
      {"organs", Json::object{{"leaf", Json::array{40, "%", "cut"}}}},
      {"export", Json::object{{"leaf", 60}}},
      {"cut-max-assimilation-rate", 90}};
  Json mineralFertJson = Json::object{
      {"type", "MineralFertilization"}, {"amount", 50.0}, {"partition", anJson}};
  Json tillageJson = Json::object{{"type", "Tillage"}, {"depth", 0.25}};
  Json irrigationJson = Json::object{
      {"type", "Irrigation"}, {"amount", 20.0},
      {"parameters", Json::object{{"nitrateConcentration", 5.0}}}};

  // scenario 0: merge()/makeWorkstep() for all 12 types
  trace::set_day(0);
  WSPtr wsAI = makeWorkstep(aiJson);
  WSPtr wsSow = makeWorkstep(sowingJson);
  WSPtr wsND1 = makeWorkstep(nd1Json);
  WSPtr wsND2 = makeWorkstep(nd2Json);
  WSPtr wsND3 = makeWorkstep(nd3Json);
  WSPtr wsAH = makeWorkstep(ahJson);
  WSPtr wsOF = makeWorkstep(ofJson);
  WSPtr wsTransplant = makeWorkstep(transplantJson);
  WSPtr wsAutoSow = makeWorkstep(autoSowingJson);
  WSPtr wsHarvest = makeWorkstep(harvestJson);
  WSPtr wsCutting = makeWorkstep(cuttingJson);
  WSPtr wsMinFert = makeWorkstep(mineralFertJson);
  WSPtr wsTillage = makeWorkstep(tillageJson);
  WSPtr wsIrrig = makeWorkstep(irrigationJson);

  dump_ws_common("wsAI", *wsAI);
  dump_ws_common("wsSow", *wsSow);
  dump_ws_common("wsND1", *wsND1);
  dump_ws_common("wsAH", *wsAH);
  dump_ws_common("wsOF", *wsOF);
  dump_ws_common("wsTransplant", *wsTransplant);
  dump_ws_common("wsAutoSow", *wsAutoSow);
  dump_ws_common("wsHarvest", *wsHarvest);
  dump_ws_common("wsCutting", *wsCutting);
  dump_ws_common("wsMinFert", *wsMinFert);
  dump_ws_common("wsTillage", *wsTillage);
  dump_ws_common("wsIrrig", *wsIrrig);
  {
    auto &sd = std::get<SowingData>(wsSow->data);
    trace::line_str("wsSow.cropName", sd.cropParams.speciesParams.pc_SpeciesId);
    trace::line_f64("wsSow.initialKcb", sd.initialKcb);
    trace::line_bool("wsSow.isValid", sd.isValid);
  }
  {
    auto &td = std::get<TransplantData>(wsTransplant->data);
    trace::line_f64("wsTransplant.initialGDD", td.initialGDD);
    trace::line_int("wsTransplant.initialStage", (long long)td.initialStage);
  }
  {
    auto &cd = std::get<CuttingData>(wsCutting->data);
    trace::line_int("wsCutting.organId2cuttingSpec.size", (long long)cd.organId2cuttingSpec.size());
    for (auto &kv : cd.organId2cuttingSpec) {
      trace::line_f64("wsCutting.spec.value", kv.second.value);
      trace::line_int("wsCutting.spec.unit", (int)kv.second.unit);
      trace::line_int("wsCutting.spec.cut_or_left", (int)kv.second.cut_or_left);
    }
    trace::line_f64("wsCutting.cutMaxAssimilationRateFraction", cd.cutMaxAssimilationRateFraction);
  }
  {
    auto &aid = std::get<AutomaticIrrigationData>(wsAI->data);
    trace::line_int("wsAI.startStage", aid.startStage);
    trace::line_int("wsAI.endStage", aid.endStage);
    trace::line_bool("wsAI.irrigateCrop", aid.irrigateCrop);
    trace::line_f64("wsAI.params.amount", aid.params.amount);
    trace::line_f64("wsAI.params.threshold", aid.params.threshold);
  }
  {
    auto &nd = std::get<NDemandFertilizationData>(wsND1->data);
    trace::line_f64("wsND1.Ndemand", nd.Ndemand);
    trace::line_f64("wsND1.depth", nd.depth);
  }

  // reinit AI/AH before the season starts (mirrors cultivation-method::reinit)
  Date seasonStart = Date::fromIsoDateString("2020-09-22");
  workstep::reinit(wsAI.get(), seasonStart);
  workstep::reinit(wsAH.get(), seasonStart);

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

    double vs_GroundwaterDepth = (day % 40) < 15 ? 3.0 : 15.0;
    double et0 = -1.0;

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);
    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       (int)da.julianDayForStep(day), et0);

    // dated worksteps: apply on their exact matching day (mirrors
    // cultivationmethod::apply(cm, date, model)'s exact-date dispatch)
    if (workstep::absDate(wsSow.get()) == currentDate) workstep::apply(wsSow.get(), model.get());
    if (workstep::absDate(wsND1.get()) == currentDate) workstep::apply(wsND1.get(), model.get());
    if (workstep::absDate(wsND2.get()) == currentDate) workstep::apply(wsND2.get(), model.get());
    if (workstep::absDate(wsND3.get()) == currentDate) workstep::apply(wsND3.get(), model.get());

    if (model->currentCropModule) {
      cropmodule::step(model->currentCropModule.get(), tavg, tmax, tmin, globrad, 0.0, currentDate,
                       (relhumid / 100.0), wind, model->envPs.p_WindSpeedHeight, ATM_CO2, ATM_O3,
                       precip, -1.0);
    }

    // Cutting - applied mid-season on the still-growing primary crop (the
    // realistic use case for this workstep), well before automatic harvest
    // normally fires.
    if (day == 100 && model->currentCropModule) {
      trace::set_day(9003);
      trace::line_f64("cropModule.vc_LeafAreaIndex.beforeCut", model->currentCropModule->vc_LeafAreaIndex);
      workstep::apply(wsCutting.get(), model.get());
      trace::line_f64("cropModule.vc_LeafAreaIndex.afterCut", model->currentCropModule->vc_LeafAreaIndex);
      trace::line_f64("cropModule.vc_exportedCutBiomass", model->currentCropModule->vc_exportedCutBiomass);
      trace::set_day(day);
    }

    // dynamic worksteps: every day via applyWithPossibleCondition (mirrors
    // cultivationmethod::apply(cm, model, bool)'s unfinishedDynamicWorksteps loop)
    workstep::applyWithPossibleCondition(wsAI.get(), model.get());
    if (!harvested && workstep::applyWithPossibleCondition(wsAH.get(), model.get())) {
      harvested = true;
      trace::set_day(1000 + day);
      dump_model_bits("model", *model);
      trace::line_str("harvestedOnDate", currentDate.toIsoDateString());
    }

    soilorganic::step(model->soilOrganic.get(), tavg, precip, wind);
    soiltransport::step(model->soilTransport.get());

    if (day % 60 == 0 || day == n - 1) {
      trace::set_day(day);
      dump_model_bits("model", *model);
    }
  }

  trace::set_day(9000);
  dump_model_bits("modelFinal", *model);
  trace::line_bool("harvested", harvested);

  // OrganicFertilization - mirrors the fixture's "after: Harvest" trigger,
  // applied directly here rather than via conditionCommon's afterEvent
  // machinery (that machinery is exercised generically by
  // workstep_condition_common's own logic, already covered by the
  // AutomaticIrrigation/AutomaticHarvest dynamic-dispatch loop above).
  workstep::apply(wsOF.get(), model.get());
  trace::set_day(9001);
  dump_model_bits("model", *model);

  // Tillage / Irrigation / MineralFertilization - bare-soil-safe, applied
  // directly on the now-harvested (crop-free) model.
  workstep::apply(wsTillage.get(), model.get());
  workstep::apply(wsIrrig.get(), model.get());
  workstep::apply(wsMinFert.get(), model.get());
  trace::set_day(9002);
  dump_model_bits("model", *model);
  trace::line_f64("sc.vs_SurfaceWaterStorage", model->soilColumn->vs_SurfaceWaterStorage);
  trace::line_f64("sc.layers0.vs_SoilNO3", model->soilColumn->layers.at(0).vs_SoilNO3);

  // Harvest - dump then apply on whatever crop module is currently present
  // (the fixture's own crop, possibly already automatic-harvested above -
  // dailyReset, which would actually clear currentCropModule to null after
  // a harvest, is cultivation-method/step() territory, phase 6 checkpoints
  // 5-6, not yet ported - so this exercises Harvest's own logic on the
  // still-present CropModule regardless).
  trace::set_day(9004);
  workstep::apply(wsHarvest.get(), model.get());
  dump_model_bits("model", *model);

  return 0;
}
