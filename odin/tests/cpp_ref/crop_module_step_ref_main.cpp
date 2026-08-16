/* Differential-test driver for phase 5 checkpoint 7 (step() orchestration):
 * monica::cropmodule::step() itself, plus the FAO-56 dual-Kc block inline in
 * it (never a separate cropmodule:: function). This is the final checkpoint
 * of phase 5 - every function ported in checkpoints 3-6 is finally wired
 * together into one real, callable daily entry point, and it also widens
 * every phase-4 module's bare-soil oracle to a live crop.
 *
 * Unlike every earlier checkpoint's driver, which hand-copied step()'s body
 * into a local day_step (since step() itself wasn't ported yet), this driver
 * calls the real, now-ported cropmodule::step() directly - there is nothing
 * left to hand-replicate.
 *
 * Full soil-module chain, all four phase-4 modules together for the first
 * time since phase 4 itself: soiltemperature::step -> soilmoisture::step ->
 * cropmodule::step -> soilorganic::step -> soiltransport::step (soilorganic
 * before soiltransport matches the real monicamodel::generalStep order per
 * soil_organic_ref_main.cpp's file comment; crop step sits between
 * soilmoisture and soilorganic so today's dead root biomass, added via the
 * real addOrganicMatter callback below, gets incorporated by soilorganic the
 * same day - the true canonical position relative to monica-model.cpp's real
 * step is that file's job, not this checkpoint's).
 *
 * All three CropModule callbacks are real for the first time:
 *   - fireEvent: traces every fired event as its own line, so the C++/Odin
 *     event *sequence* is diffed exactly like every other field, not just
 *     the numeric state.
 *   - addOrganicMatter: wired to the real, now-shared SoilOrganic (not a
 *     recording stand-in like checkpoint 5's oracle) - model->soilOrganic
 *     and cropModule's addOrganicMatter calls are the same instance,
 *     matching production.
 *   - getSnowDepthAndCalcTempUnderSnow: wired to the real, now-shared
 *     SoilMoisture, same reasoning.
 * model->soilMoisture->cropModule / model->soilOrganic->cropModule /
 * model->soilTransport->cropModule are all set to the real CropModule
 * (matching production soilcolumn::putCrop/soiltransport::putCrop wiring) -
 * this is what actually widens phase 4's bare-soil coverage to a live crop:
 * soilmoisture's Kc/soil-coverage/height/devStage read paths,
 * soilorganic's NPP read for NEP/NEE, and soiltransport's N-uptake read all
 * activate for the first time.
 *
 * Unlike every earlier checkpoint, this scenario does NOT force setStage(1):
 * with a real, daily-stepped SoilTemperature now driving soilColumn->layers[0]'s
 * temperature, germination is real and exercised, not skipped.
 *
 * One scenario: real wheat, the real Hohenfinow2 climate-min.csv record for
 * NUM_DAYS=500 days (more than a full year, enough to reach and pass through
 * a full winter-wheat season from natural germination).
 *
 * Usage: crop_module_step_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_crop_module_step.sh.
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

static void dump_crop_module(const std::string &PATH, const CropModule &OBJ) {
  // --- phenology (checkpoint 3) ---
  TI(vc_DevelopmentalStage);
  TD_VEC(vc_CurrentTemperatureSum);
  TD(vc_CurrentTotalTemperatureSum);
  TB(vc_ErrorStatus);
  TI(vc_AnthesisDay);
  TI(vc_MaturityDay);
  TB(vc_MaturityReached);
  TD(vc_DaylengthFactor);
  TD(vc_VernalisationFactor);
  TD(vc_VernalisationDays);
  TD(vc_RelativeTotalDevelopment);
  TD(vc_KcFactor);
  TD(vc_KcbFactor);
  TD(vc_CropHeight);
  TD(vc_CropDiameter);
  TD(vc_LeafAreaIndex);
  TD(vc_GreenAreaIndex);
  TD(vc_SoilCoverage);

  // --- stress (checkpoint 5) ---
  TD(vc_CropHeatRedux);
  TD(vc_TotalCropHeatImpact);
  TD(vc_LT50);
  TD(vc_LT50M);
  TD(vc_CropFrostRedux);
  TD(vc_DroughtImpactOnFertility);

  // --- nitrogen / biomass (checkpoints 5-6) ---
  TD(vc_CriticalNConcentration);
  TD(vc_TargetNConcentration);
  TD(rootNRedux);
  TD(vc_CropNRedux);
  TD(vc_AbovegroundBiomass);
  TD(vc_BelowgroundBiomass);
  TD(vc_TotalBiomass);
  TD_VEC(vc_OrganBiomass);
  TD_VEC(vc_OrganGreenBiomass);
  TD(vc_RootBiomass);
  TD(vc_TotalBiomassNContent);
  TD(vc_CropNDemand);
  TD(vc_MaxRootingDepth);
  TD(vc_RootingDepth_m);
  TI(vc_RootingDepth);
  TI(vc_RootingZone);
  TD(vc_TotalRootLength);

  // --- water (checkpoint 6) ---
  TD(vc_ReferenceEvapotranspiration);
  TD(vc_OxygenDeficit);
  TD_VEC(vc_RootEffectivity);
  TD_VEC(vc_RootDensity);
  TD(soilColumn->layers[0].vs_SoilMoisture_m3);
  TD(soilColumn->layers[0].vs_FieldCapacity);
  TD(soilColumn->layers[0].vs_PermanentWiltingPoint);
  TI(soilColumn->vm_GroundwaterTableLayer);
  TD(vc_ActualTranspiration);
  TD(vc_TranspirationDeficit);
  TD(vc_PotentialTranspiration);
  TD_VEC(vc_Transpiration);
  TD(vc_NetPrecipitation);
  TD(vc_InterceptionStorage);

  // --- nitrogen uptake (checkpoint 6) ---
  TD(vc_TotalNUptake);
  TD(vc_TotalNInput);
  TD(vc_FixedN);
  TD(vc_SumTotalNUptake);
  TD_VEC(vc_NUptakeFromLayer);

  // --- photosynthesis (checkpoint 4) ---
  TD(vc_AssimilationRate);
  TD(vc_GrossPhotosynthesis);
  TD(vc_Assimilates);
  TD(vc_TotalRespired);

  // --- production (checkpoints 4/6) ---
  TD(vc_GrossPrimaryProduction);
  TD(vc_NetPrimaryProduction);

  // --- transplant / cutting bookkeeping (checkpoint 7) ---
  TD(vc_TransplantEfficiency);
  TI(vc_DaysSinceTransplant);
  TI(noOfCropSteps);
}

static void dump_soil_moisture_diag(const std::string &PATH, const SoilMoisture &OBJ) {
  TD(vc_PercentageSoilCoverage);
  TD(vc_KcFactor);
  TD(vc_NetPrecipitation);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: crop_module_step_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  auto recordingFireEvent = [](string event) {
    trace::line_str(trace::join("cropModule", "event"), event);
  };
  // soilmoisture.cpp's crop-coupling "outer gate" (vc_PercentageSoilCoverage/
  // vc_KcFactor/vc_NetPrecipitation) reads sm->monica.currentCropModule, a
  // SEPARATE MonicaModel-level pointer from sm->cropModule (soilmoisture.cpp:156-172)
  // - not the SoilMoisture-owned field this driver also sets below. Both must
  // point at the same real CropModule for the crop-coupled branch to
  // actually activate; the Odin port deliberately unifies the two onto one
  // field (soil_moisture.odin's package comment), so only this driver needs
  // both set. Captures model->currentCropModule.get(), not a separate `cm`
  // local, to sidestep this entirely.
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
                     recordingFireEvent, realAddOrganicMatter, realGetSnowDepth, nullptr);
  auto *cm = model->currentCropModule.get();

  // widen phase 4's bare-soil coupling to this real, live crop - matches
  // production soilcolumn::putCrop/soiltransport::putCrop wiring.
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
    double et0 = -1.0; // climate-min.csv has no et0 column

    trace::set_day(day);

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);
    // dualKcMethod is not a parameter here in the real C++ - soilmoisture::step
    // reads it via its own sm->monica->simPs back-pointer (wired by
    // makeMonicaModel), unlike the Odin port's explicit dual_kc_method
    // parameter (soil_moisture.odin has no monica back-pointer at all).
    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       (int)da.julianDayForStep(day), et0);

    step(cm, tavg, tmax, tmin, globrad, 0.0, currentDate, (relhumid / 100.0), wind,
        model->envPs.p_WindSpeedHeight, ATM_CO2, ATM_O3, precip, -1.0);

    soilorganic::step(model->soilOrganic.get(), tavg, precip, wind);
    soiltransport::step(model->soilTransport.get());

    dump_crop_module("cropModule", *cm);
    dump_soil_moisture_diag("soilMoistureDiag", *model->soilMoisture);
  }

  return 0;
}
