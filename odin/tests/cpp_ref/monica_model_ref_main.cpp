/* Differential-test driver for phase 6 checkpoint 1 (MonicaModel scaffolding
 * + standalone helpers): src/core/monica-model.h's MonicaModel struct and the
 * non-step() half of src/core/monica-model.cpp - makeMonicaModel,
 * CO2ForDate (both overloads), groundwaterDepthForDate, clearEvents, the
 * daily-sum accumulators, dailyReset, applyMineralFertiliser,
 * applyOrganicFertiliser, applyMineralFertiliserViaNMinMethod,
 * applyIrrigation, applyTillage. Also exercises the soilcolumn.h mutators
 * these wrap: applyMineralFertiliser, applyMineralFertiliserViaNMinMethod,
 * applyMineralFertiliserViaNDemand, applyPossibleDelayedFerilizer,
 * applyPossibleTopDressing, applyIrrigation, applyIrrigationViaTrigger,
 * applyTillage, deleteAOMPool, putCrop/removeCrop, clearTopDressingParams.
 *
 * No daily loop / CropModule construction needed: every function here is a
 * direct mutator called against a real, live MonicaModel (bare soil - the
 * same "real MonicaModel, no crop" scope every phase-4 driver used).
 * setOtherCropHeightAndLAIt's non-nil-crop branch and dailyReset's
 * crop-clearing branch are left for checkpoint 6's full step() integration,
 * which is the first checkpoint with a live CropModule wired into a
 * MonicaModel in production code.
 *
 * Scenarios (indexed as trace "days"):
 *   0: state right after makeMonicaModel
 *   1: applyMineralFertiliser
 *   2: applyOrganicFertiliser
 *   3: applyMineralFertiliserViaNMinMethod, soil too wet -> delayed path,
 *      then applyPossibleDelayedFerilizer drains the queue next "day"
 *   4: applyMineralFertiliserViaNMinMethod, soil dry enough -> immediate
 *      apply + top-dressing split, then applyPossibleTopDressing drains it
 *      over the following days
 *   5: applyMineralFertiliserViaNDemand
 *   6: applyIrrigation
 *   7: applyIrrigationViaTrigger (putCrop wires a live CropModule in first -
 *      the only scenario needing one, since the trigger reads
 *      cropModule->cropParams/vc_CurrentTotalTemperatureSum)
 *   8: applyTillage (perturbs layers 0-2 first so the average is non-trivial)
 *   9: deleteAOMPool
 *  10: addDailySumFertiliser/addDailySumOrganicFertilizerDM/
 *      addDailySumIrrigationWater/resetFertiliserCounter/clearEvents/
 *      dailyReset (bare soil, clearCropUponNextDay stays false: no crop to
 *      clear)
 *  11: CO2ForDate sweep (both overloads) x groundwaterDepthForDate sweep -
 *      pure functions, no model needed
 *
 * Usage: monica_model_ref <pathToSimJson>
 * See odin/tests/cpp_ref/run_monica_model.sh.
 */

#include <cstdio>
#include <map>
#include <string>

#include "core/monica-model.h"
#include "core/monica-parameters.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "soil/soil.h"
#include "tools/helper.h"
#include "trace_common.h"

using namespace std;
using namespace Tools;
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

static void dump_model(const std::string &PATH, const MonicaModel &OBJ) {
  TD(sumFertiliser);
  TD(sumOrgFertiliser);
  TD(dailySumFertiliser);
  TD(dailySumOrgFertiliser);
  TD(dailySumOrganicFertilizerDM);
  TD(sumOrganicFertilizerDM);
  TD(humusBalanceCarryOver);
  TD(dailySumIrrigationWater);
  TB(clearCropUponNextDay);
  TI(cultivationMethodCount);
}

static void dump_soil_column_top3(const std::string &PATH, const SoilColumn &OBJ) {
  TD(vs_SurfaceWaterStorage);
  TD(vf_TopDressing);
  TI(vf_TopDressingDelay);
  for (int i = 0; i < 3; i++) {
    auto p2 = trace::index(PATH, i);
    trace::line_f64(p2 + ".vs_SoilNO3", OBJ.layers.at(i).vs_SoilNO3);
    trace::line_f64(p2 + ".vs_SoilNH4", OBJ.layers.at(i).vs_SoilNH4);
    trace::line_f64(p2 + ".vs_SoilCarbamid", OBJ.layers.at(i).vs_SoilCarbamid);
    trace::line_f64(p2 + ".vs_SoilTemperature", OBJ.layers.at(i).vs_SoilTemperature);
    trace::line_f64(p2 + ".vs_SoilMoisture_m3", OBJ.layers.at(i).vs_SoilMoisture_m3);
    trace::line_f64(p2 + ".vs_SoilOrganicCarbon", OBJ.layers.at(i).vs_SoilOrganicCarbon);
    trace::line_int(p2 + ".vo_AOM_Pool.size", (long long)OBJ.layers.at(i).vo_AOM_Pool.size());
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: monica_model_ref <pathToSimJson>\n");
    return 2;
  }

  std::string pathToSimJson = argv[1];
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

  // scenario 0: right after construction
  trace::set_day(0);
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 1: applyMineralFertiliser
  trace::set_day(1);
  MineralFertilizerParameters mfp;
  mfp.vo_NO3 = 50.0;
  mfp.vo_NH4 = 30.0;
  mfp.vo_Carbamid = 20.0;
  monicamodel::applyMineralFertiliser(model.get(), mfp, 40.0); // kg N ha-1
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 2: applyOrganicFertiliser
  trace::set_day(2);
  OrganicMatterParameters omp;
  omp.vo_AOM_DryMatterContent = 0.35;
  omp.vo_AOM_NH4Content = 0.002;
  omp.vo_AOM_NO3Content = 0.0005;
  omp.vo_AOM_CarbamidContent = 0.0;
  omp.vo_PartAOM_to_AOM_Slow = 0.5;
  omp.vo_PartAOM_to_AOM_Fast = 0.3;
  omp.vo_CN_Ratio_AOM_Slow = 20.0;
  omp.vo_CN_Ratio_AOM_Fast = 10.0;
  omp.vo_NConcentration = 0.01;
  monicamodel::applyOrganicFertiliser(model.get(), omp, 1000.0, true, 0);
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 3: applyMineralFertiliserViaNMinMethod, soil too wet -> delayed
  trace::set_day(3);
  model->soilColumn->layers.at(0).vs_SoilMoisture_m3 =
      model->soilColumn->layers.at(0).vs_FieldCapacity + 0.01; // force "too wet"
  double fertAmount1 = monicamodel::applyMineralFertiliserViaNMinMethod(
      model.get(), mfp, makeNMinCropParameters(0.3, 80.0, 40.0));
  trace::line_f64("fertAmount1", fertAmount1);
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);
  // drain the delayed queue on the "next day"
  double drained = soilcolumn::applyPossibleDelayedFerilizer(model->soilColumn.get());
  monicamodel::addDailySumFertiliser(model.get(), drained);
  trace::line_f64("drained", drained);
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 4: applyMineralFertiliserViaNMinMethod, soil dry -> immediate +
  // top-dressing split
  trace::set_day(4);
  model->soilColumn->layers.at(0).vs_SoilMoisture_m3 =
      model->soilColumn->layers.at(0).vs_FieldCapacity - 0.05; // force "dry enough"
  double fertAmount2 = monicamodel::applyMineralFertiliserViaNMinMethod(
      model.get(), mfp, makeNMinCropParameters(0.3, 400.0, 400.0)); // large demand -> top-dressing split
  trace::line_f64("fertAmount2", fertAmount2);
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);
  for (int i = 0; i < 3; i++) {
    trace::set_day(4);
    double topDressed = soilcolumn::applyPossibleTopDressing(model->soilColumn.get());
    trace::line_f64(trace::index("topDressed", i), topDressed);
  }
  dump_model("model", *model);
  dump_soil_column_top3("sc", *model->soilColumn);
  soilcolumn::clearTopDressingParams(model->soilColumn.get());
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 5: applyMineralFertiliserViaNDemand
  trace::set_day(5);
  double demandFert =
      soilcolumn::applyMineralFertiliserViaNDemand(model->soilColumn.get(), mfp, 0.25, 60.0);
  trace::line_f64("demandFert", demandFert);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 6: applyIrrigation
  trace::set_day(6);
  monicamodel::applyIrrigation(model.get(), 20.0, 5.0);
  dump_model("model", *model);
  trace::line_f64("sc.vs_SurfaceWaterStorage", model->soilColumn->vs_SurfaceWaterStorage);
  trace::line_f64("sc.layers[0].vs_SoilNO3", model->soilColumn->layers.at(0).vs_SoilNO3);
  trace::line_f64("so.irrigationAmount", model->soilOrganic->irrigationAmount);

  // scenario 7: applyIrrigationViaTrigger - needs a live cropModule
  trace::set_day(7);
  auto wheatCropParams = std::make_unique<CropParameters>();
  std::string monicaParametersDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));
  cropparameters::merge(wheatCropParams.get(), load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));
  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));
  auto cm = makeCropModule(
      model->soilColumn.get(), wheatCropParams.get(), &wheatResidueParams, &model->sitePs,
      &model->cropPs, &model->simPs, [](string) {}, [](const std::map<size_t, double> &, double) {},
      [](double) { return std::make_pair(0.0, 0.0); }, &model->intercropping);
  soilcolumn::putCrop(model->soilColumn.get(), cm.get());
  cm->vc_CurrentTotalTemperatureSum =
      (cm->cropParams.cultivarParams.pc_HeatSumIrrigationStart +
       cm->cropParams.cultivarParams.pc_HeatSumIrrigationEnd) /
      2.0; // inside the irrigation window
  AutomaticIrrigationParameters aip;
  aip.amount = 15.0;
  aip.threshold = 0.99; // force triggered
  aip.nitrateConcentration = 3.0;
  aip.criticalMoistureDepthM = 0.3;
  auto trig = soilcolumn::applyIrrigationViaTrigger(model->soilColumn.get(), aip);
  trace::line_bool("triggered", trig.first);
  trace::line_f64("triggeredAmount", trig.second);
  trace::line_f64("sc.vs_SurfaceWaterStorage", model->soilColumn->vs_SurfaceWaterStorage);
  soilcolumn::removeCrop(model->soilColumn.get());

  // scenario 8: applyTillage
  trace::set_day(8);
  for (int i = 0; i < 3; i++) {
    model->soilColumn->layers.at(i).vs_SoilNO3 = 0.001 * (i + 1);
    model->soilColumn->layers.at(i).vs_SoilTemperature = 5.0 + i;
    model->soilColumn->layers.at(i).vs_SoilMoisture_m3 = 0.2 + 0.01 * i;
  }
  monicamodel::applyTillage(model.get(), 0.25);
  dump_soil_column_top3("sc", *model->soilColumn);

  // scenario 9: deleteAOMPool - the organic-fertiliser application in
  // scenario 2 added a real AOM pool; verify it survives (well above the
  // 1e-5 threshold) and the count is unaffected by an unrelated deleteAOMPool
  // call.
  trace::set_day(9);
  int poolCountBefore = (int)model->soilColumn->layers.at(0).vo_AOM_Pool.size();
  soilcolumn::deleteAOMPool(model->soilColumn.get());
  int poolCountAfter = (int)model->soilColumn->layers.at(0).vo_AOM_Pool.size();
  trace::line_int("poolCountBefore", poolCountBefore);
  trace::line_int("poolCountAfter", poolCountAfter);

  // scenario 10: daily-sum accumulators + clearEvents + dailyReset (bare soil)
  trace::set_day(10);
  monicamodel::addDailySumFertiliser(model.get(), 5.0);
  monicamodel::addDailySumOrganicFertilizerDM(model.get(), 7.0);
  monicamodel::addDailySumIrrigationWater(model.get(), 3.0);
  dump_model("model", *model);
  monicamodel::resetFertiliserCounter(model.get());
  dump_model("model", *model);
  model->currentEvents.insert("Sowing");
  model->currentEvents.insert("Irrigation");
  monicamodel::clearEvents(model.get());
  {
    std::string joined;
    for (auto &e : model->previousDaysEvents) joined += e + ",";
    trace::line_str("previousDaysEvents", joined);
    joined.clear();
    for (auto &e : model->currentEvents) joined += e + ",";
    trace::line_str("currentEvents", joined);
  }
  model->clearCropUponNextDay = false; // bare soil - nothing to clear
  monicamodel::dailyReset(model.get());
  dump_model("model", *model);

  // scenario 11: pure CO2ForDate / groundwaterDepthForDate sweeps
  trace::set_day(11);
  mas::schema::climate::RCP rcps[] = {
      mas::schema::climate::RCP::RCP19, mas::schema::climate::RCP::RCP26,
      mas::schema::climate::RCP::RCP34, mas::schema::climate::RCP::RCP45,
      mas::schema::climate::RCP::RCP60, mas::schema::climate::RCP::RCP70,
      mas::schema::climate::RCP::RCP85};
  double years[] = {1990.0, 2020.0, 2050.0, 2100.0};
  double jdays[] = {1.0, 100.0, 200.0, 365.0};
  int idx = 0;
  for (double year : years) {
    for (double jday : jdays) {
      for (bool leap : {false, true}) {
        for (auto rcp : rcps) {
          double co2 = monicamodel::CO2ForDate(year, jday, leap, rcp);
          trace::line_f64(trace::index("co2Sweep", idx++), co2);
        }
      }
    }
  }
  Date d1 = Date::julianDate(150, 2021);
  double co2FromDate = monicamodel::CO2ForDate(d1, mas::schema::climate::RCP::RCP45);
  trace::line_f64("co2FromDate", co2FromDate);

  int idx2 = 0;
  double maxGw[] = {10.0, 18.0};
  double minGw[] = {5.0, 20.0};
  int months[] = {1, 3, 7, 12};
  double jdays2[] = {1.0, 91.0, 182.0, 273.0, 365.0};
  for (double mx : maxGw) {
    for (double mn : minGw) {
      for (int mo : months) {
        for (double jd : jdays2) {
          for (bool leap : {false, true}) {
            double gw = monicamodel::groundwaterDepthForDate(mx, mn, mo, jd, leap);
            trace::line_f64(trace::index("gwSweep", idx2++), gw);
          }
        }
      }
    }
  }

  return 0;
}
