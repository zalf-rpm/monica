/* Differential-test driver for phase 5 checkpoint 4 (photosynthesis +
 * assimilation): fcCropPhotosynthesis (the largest function in the port,
 * ~1100 lines), fcGrossPrimaryProduction, fcNetPrimaryProduction - all in
 * src/core/crop-module.cpp. This is where the phase-5-checkpoint-1 satellite
 * modules (photosynthesis-FvCB, O3-impact, voc-guenther, voc-jjv) actually
 * get wired into the crop module.
 *
 * Same "driver reimplements just enough of step()'s call order" approach as
 * crop_module_phenology_ref_main.cpp - this driver's day_step is that one's
 * phenology_day_step extended with fcCropPhotosynthesis/
 * fcGrossPrimaryProduction/fcNetPrimaryProduction in the vc_DevelopmentalStage>0
 * block, matching step()'s real order exactly. Still not ported (checkpoint
 * 5/6/7's job, and so not replicated here): fcHeatStressImpact, fcFrostKill,
 * fcDroughtImpactOnFertility, fcCropNitrogen, fcCropDryMatter,
 * fcCropWaterUptake, fcCropNUptake, and every fireEvent call. Because none of
 * those run, vc_CropNRedux/vc_TranspirationDeficit/vc_OrganGreenBiomass stay
 * at their checkpoint-2 construction-time values for the whole run (nothing
 * in checkpoints 2-4's scope mutates them) - a "degenerate but correct"
 * situation like checkpoint 3's fcCropGreenArea test, not a bug: the
 * photosynthesis math itself still varies meaningfully day to day from real
 * weather and real phenology-driven Kc/LAI/height progression.
 *
 * Four scenarios:
 *   A: real wheat, default flags (pc_CO2Method=3, the Long/Mitchell CO2
 *      response; __enable_hourly_FvCB_photosynthesis__=false, so the daily
 *      Penning De Vries / HERMES radiation-interception path, not the hourly
 *      FvCB loop), started past germination via setStage(1), run over
 *      NUM_DAYS_A days of the real Hohenfinow2 climate-min.csv record.
 *   B: real wheat with __enable_hourly_FvCB_photosynthesis__ forced true and
 *      vc_RootingDepth manually forced positive (root distribution is
 *      checkpoint 6's job, not yet ported, so it stays 0 by default - which
 *      would skip the O3 block entirely) - exercises the entire hourly
 *      FvCB/O3-impact/VOC-guenther/VOC-jjv wiring, the main point of this
 *      checkpoint. A handful of real climate days (hourly work is 24x/day).
 *   C: real wheat with cm->pc_CO2Method forced to 2 - the Hoffmann 1995 CO2
 *      response branch (pc_CO2Method is a plain CropModule member, never set
 *      from any parameter file in the real system either).
 *   D: a synthetic cultivar with speciesParams.pc_CarboxylationPathway forced
 *      to 2 - the C4/non-Carboxylation-1 branch (real wheat is
 *      CarboxylationPathway=1, so scenario A never reaches this).
 *
 * Usage: crop_module_photosynthesis_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>
 * See odin/tests/cpp_ref/run_crop_module_photosynthesis.sh.
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

static const double ATM_CO2 = 380.0; // ppm, illustrative constant
static const double ATM_O3 = 60.0;   // ppb, illustrative constant

static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

static void dump_crop_module_photosynthesis(const std::string &PATH, const CropModule &OBJ) {
  TD(vc_AssimilationRate);
  TD(vc_KTkc);
  TD(vc_KTko);
  // cropPhotosynthesisResults' kc/ko/oi/ci/comp/vcMax/jMax/jj/jj1000/jv fields
  // (and, downstream, jjvEmissions) are NOT dumped: FvCB_canopy_hourly_out's
  // per-leaf-fraction fields (photosynthesis-FvCB.cpp:597-602) are only
  // partially set in the "no photosynthesis can occur" branch
  // (global_rad<=0) - LAI and gs are, but kc/ko/oi/ci/comp/vcMax/jMax/rad/jj/
  // jj1000/jv are left as indeterminate stack garbage. Since the hourly loop
  // always ends at h=23 (11pm - virtually always night), cropPhotosynthesisResults'
  // end-of-day snapshot is this genuinely undefined garbage on essentially
  // every real day, not a reproducible quirk - the same class of gap as
  // checkpoint 1's FvCB_leaf_fraction.ci/cc finding, just wider than that
  // checkpoint's own grid happened to surface. LAI (vc_sunlitLeafAreaIndex/
  // vc_shadedLeafAreaIndex below), gs (consumed by the O3 block via
  // avg_leaf_gs) and canopy_gross_photos/canopy_net_photos/canopy_resp (feed
  // dailyGP, i.e. vc_GrossPhotosynthesis and friends) are all well-defined in
  // both branches and stay in this dump.
  TD(vc_GrossPhotosynthesis);
  TD(vc_GrossPhotosynthesis_mol);
  TD(vc_GrossPhotosynthesisReference_mol);
  TD(vc_Assimilates);
  TD(vc_GrossAssimilates);
  TD(vc_MaintenanceRespirationAS);
  TD(vc_GrowthRespirationAS);
  TD(vc_TotalRespired);
  TD(vc_NetMaintenanceRespiration);
  TD(fractionOfInterceptedRadiation1);

  TD(vc_GrossPrimaryProduction);
  TD(vc_NetPrimaryProduction);
  TD(vc_Respiration);

  TD(vc_O3_shortTermDamage);
  TD(vc_O3_longTermDamage);
  TD(vc_O3_senescence);
  TD(vc_O3_sumUptake);
  TD(vc_O3_WStomatalClosure);

  TD(guentherEmissions.isoprene_emission);
  TD(guentherEmissions.monoterpene_emission);
  // jjvEmissions not dumped - see the comment above cropPhotosynthesisResults.

  TD_VEC(vc_sunlitLeafAreaIndex);
  TD_VEC(vc_shadedLeafAreaIndex);
}

// checkpoint 3's phenology_day_step extended with fcCropPhotosynthesis/
// fcGrossPrimaryProduction/fcNetPrimaryProduction, matching step()'s real
// order (crop-module.cpp:712-745).
static void day_step(CropModule *cm, double meanAirTemperature, double maxAirTemperature,
                     double minAirTemperature, double globalRadiation, double sunshineHours,
                     Date currentDate, int julianDayOverride = -1) {
  const auto &pc_BaseDaylength = cm->cropParams.cultivarParams.pc_BaseDaylength;
  const auto &pc_CriticalOxygenContent = cm->cropParams.speciesParams.pc_CriticalOxygenContent;
  const auto &pc_DaylengthRequirement = cm->cropParams.cultivarParams.pc_DaylengthRequirement;
  const auto pc_MaxCropHeight = cm->cropParams.cultivarParams.pc_MaxCropHeight;
  const auto pc_Perennial = cm->cropParams.cultivarParams.pc_Perennial;
  const auto &pc_SpecificLeafArea = cm->cropParams.cultivarParams.pc_SpecificLeafArea;
  const auto &pc_StageKcFactor = cm->cropParams.cultivarParams.pc_StageKcFactor;
  const auto &pc_StageTemperatureSum = cm->cropParams.cultivarParams.pc_StageTemperatureSum;
  const auto &pc_VernalisationRequirement = cm->cropParams.cultivarParams.pc_VernalisationRequirement;
  const auto &speciesPs = cm->cropParams.speciesParams;

  int vs_JulianDay =
      julianDayOverride >= 0 ? julianDayOverride : int(currentDate.julianDay());

  fcRadiation(cm, vs_JulianDay, globalRadiation, sunshineHours);

  cm->vc_OxygenDeficit =
      fcOxygenDeficiency(cm, pc_CriticalOxygenContent[cm->vc_DevelopmentalStage]);

  size_t old_DevelopmentalStage = cm->vc_DevelopmentalStage;

  if (!cm->perennialCropDormancyPeriodEndDate.isValid()) {
    cm->perennialCropDormancyPeriodEndDate =
        speciesPs.dormancyEndDoy == 0
            ? currentDate
            : Date(1, 1, currentDate.year()) + (speciesPs.dormancyEndDoy - 1);
  }
  if (!pc_Perennial || currentDate >= cm->perennialCropDormancyPeriodEndDate) {
    fcCropDevelopmentalStage(cm, meanAirTemperature, cm->soilColumn->layers[0].vs_SoilMoisture_m3,
                             cm->soilColumn->layers[0].vs_FieldCapacity,
                             cm->soilColumn->layers[0].vs_PermanentWiltingPoint, currentDate);
  }

  if (isAnthesisDay(cm, old_DevelopmentalStage, cm->vc_DevelopmentalStage)) {
    cm->vc_AnthesisDay = vs_JulianDay;
  } else if (isMaturityDay(cm, old_DevelopmentalStage, cm->vc_DevelopmentalStage)) {
    cm->vc_MaturityDay = vs_JulianDay;
    cm->vc_MaturityReached = true;
  }

  cm->vc_DaylengthFactor =
      fcDaylengthFactor(cm, pc_DaylengthRequirement[cm->vc_DevelopmentalStage],
                        cm->vc_EffectiveDayLength, cm->vc_PhotoperiodicDaylength,
                        pc_BaseDaylength[cm->vc_DevelopmentalStage]);

  tie(cm->vc_VernalisationFactor, cm->vc_VernalisationDays) = fcVernalisationFactor(
      cm, meanAirTemperature, pc_VernalisationRequirement[cm->vc_DevelopmentalStage],
      cm->vc_VernalisationDays);

  if (cm->vc_TotalTemperatureSum == 0.0) {
    cm->vc_RelativeTotalDevelopment = 0.0;
  } else {
    cm->vc_RelativeTotalDevelopment = cm->vc_CurrentTotalTemperatureSum / cm->vc_TotalTemperatureSum;
  }

  if (cm->vc_DevelopmentalStage == 0) {
    cm->vc_KcFactor = cm->siteParams->bareSoilKcFactor;
  } else {
    cm->vc_KcFactor = fcKcFactor(cm, pc_StageTemperatureSum[cm->vc_DevelopmentalStage],
                                 cm->vc_CurrentTemperatureSum[cm->vc_DevelopmentalStage],
                                 pc_StageKcFactor[cm->vc_DevelopmentalStage],
                                 pc_StageKcFactor[cm->vc_DevelopmentalStage - 1]);
  }

  if (cm->vc_DevelopmentalStage > 0) {
    auto maxCropHeight = pc_MaxCropHeight;

    fcCropSize(cm, maxCropHeight);

    fcCropGreenArea(cm, meanAirTemperature, cm->vc_OrganGrowthIncrement[OId::LEAF],
                    cm->vc_OrganSenescenceIncrement[OId::LEAF],
                    pc_SpecificLeafArea[cm->vc_DevelopmentalStage - 1],
                    pc_SpecificLeafArea[cm->vc_DevelopmentalStage], pc_SpecificLeafArea[1],
                    pc_StageTemperatureSum[cm->vc_DevelopmentalStage],
                    cm->vc_CurrentTemperatureSum[cm->vc_DevelopmentalStage]);

    cm->vc_SoilCoverage = fcSoilCoverage(cm);

    fcCropPhotosynthesis(cm, meanAirTemperature, maxAirTemperature, minAirTemperature, ATM_CO2,
                         ATM_O3, currentDate);

    cm->vc_GrossPrimaryProduction = fcGrossPrimaryProduction(cm);
    cm->vc_NetPrimaryProduction = fcNetPrimaryProduction(cm, cm->vc_TotalRespired);
  }

  cm->noOfCropSteps++;
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr,
            "usage: crop_module_photosynthesis_ref <pathToSimJson> <pathToClimateCsv> <numDaysA>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];
  std::string pathToClimateCsv = argv[2];
  int numDaysA = atoi(argv[3]);

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

  auto noopFireEvent = [](std::string) {};
  auto noopAddOrganicMatter = [](std::map<size_t, double>, double) {};
  auto noopGetSnowDepth = [](double) { return std::make_pair(0.0, 0.0); };

  CSVViaHeaderOptions copts{
      Json::object{{"no-of-climate-file-header-lines", 2}, {"csv-separator", std::string(",")}}};
  auto climRes = readClimateDataFromCSVFileViaHeaders(pathToClimateCsv, copts);
  if (climRes.failure()) {
    for (const auto &e : climRes.errors) fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  DataAccessor da = climRes.result;

  // === Scenario A: real wheat, default flags, real climate ===
  {
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    setStage(cm.get(), 1);

    int n = (int)da.noOfStepsPossible();
    if (numDaysA > 0 && numDaysA < n) n = numDaysA;

    for (int day = 0; day < n; day++) {
      double tmin = da.dataForTimestep(Climate::tmin, day);
      double tmax = da.dataForTimestep(Climate::tmax, day);
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      Date currentDate = da.dateForStep(day);

      day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate);

      trace::set_day(day);
      dump_crop_module_photosynthesis("cropModuleA", *cm);
    }
  }

  // === Scenario B: real wheat, hourly FvCB forced on, rooted ===
  {
    CropModuleParameters hourlyCropModParams = cpp.userCropParameters;
    hourlyCropModParams.__enable_hourly_FvCB_photosynthesis__ = true;

    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &hourlyCropModParams, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    setStage(cm.get(), 1);
    cm->vc_RootingDepth = 3; // root distribution is checkpoint 6, not yet ported

    int n = std::min(15, (int)da.noOfStepsPossible());

    for (int day = 0; day < n; day++) {
      double tmin = da.dataForTimestep(Climate::tmin, day);
      double tmax = da.dataForTimestep(Climate::tmax, day);
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      Date currentDate = da.dateForStep(day);

      day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate);

      trace::set_day(day);
      dump_crop_module_photosynthesis("cropModuleB", *cm);
    }
  }

  // === Scenario C: real wheat, pc_CO2Method forced to 2 (Hoffmann) ===
  {
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    setStage(cm.get(), 1);
    cm->pc_CO2Method = 2;

    int n = std::min(20, (int)da.noOfStepsPossible());

    for (int day = 0; day < n; day++) {
      double tmin = da.dataForTimestep(Climate::tmin, day);
      double tmax = da.dataForTimestep(Climate::tmax, day);
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      Date currentDate = da.dateForStep(day);

      day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate);

      trace::set_day(day);
      dump_crop_module_photosynthesis("cropModuleC", *cm);
    }
  }

  // === Scenario D: synthetic C4 cultivar (pc_CarboxylationPathway=2) ===
  {
    CropParameters c4CropParams = wheatCropParams;
    c4CropParams.speciesParams.pc_CarboxylationPathway = 2;

    auto cm = makeCropModule(model->soilColumn.get(), &c4CropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    setStage(cm.get(), 1);

    int n = std::min(20, (int)da.noOfStepsPossible());

    for (int day = 0; day < n; day++) {
      double tmin = da.dataForTimestep(Climate::tmin, day);
      double tmax = da.dataForTimestep(Climate::tmax, day);
      double tavg = da.dataForTimestep(Climate::tavg, day);
      double globrad = da.dataForTimestep(Climate::globrad, day);
      Date currentDate = da.dateForStep(day);

      day_step(cm.get(), tavg, tmax, tmin, globrad, 0.0, currentDate);

      trace::set_day(day);
      dump_crop_module_photosynthesis("cropModuleD", *cm);
    }
  }

  return 0;
}
