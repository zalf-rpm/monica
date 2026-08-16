/* Differential-test driver for phase 4's soilorganic.cpp - the last of the
 * four soil-physics modules (soiltemperature, soilmoisture, soiltransport,
 * soilorganic; stics-nit-denit-n2o was folded into this checkpoint as a small
 * prerequisite, since soilorganic::step calls into it when the STICS flags
 * are enabled - false in every fixture in this repo, so that branch is dead
 * code here but still must compile and be correct).
 *
 * Same "real MonicaModel, bare soil" approach as the other soil-physics
 * checkpoints, but this one chains all three modules that run before
 * soilorganic in the real monicamodel::generalStep order: soiltemperature,
 * then soilmoisture, then soilorganic (soiltransport runs after soilorganic
 * in the real order, so it's irrelevant here). Because soilmoisture is now
 * real and already verified (module 2b), soiltemperature no longer needs
 * synthetic snow-depth injection the way soil_temperature_ref_main.cpp did
 * before soilmoisture existed - it reads the real, live
 * model->soilMoisture->snowComponent/frostComponent state through the
 * unmodified st->monica back-pointer, exactly like production.
 *
 * Usage: soil_organic_ref <pathToSimJson> <pathToClimateCsv> <numDays>
 * See odin/tests/cpp_ref/run_soil_organic.sh.
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

static void dump_stics_parameters(const std::string &PATH, const SticsParameters &OBJ) {
  TI(code_vnit);
  TI(code_tnit);
  TI(code_rationit);
  TI(code_hourly_wfps_nit);
  TI(code_pdenit);
  TI(code_ratiodenit);
  TI(code_hourly_wfps_denit);
  TB(use_nit);
  TB(use_denit);
  TB(use_n2o);
  TD(hminn);
  TD(hoptn);
  TD(pHminnit);
  TD(pHmaxnit);
  TD(nh4_min);
  TD(pHminden);
  TD(pHmaxden);
  TD(wfpsc);
  TD(tdenitopt_gauss);
  TD(scale_tdenitopt);
  TD(Kd);
  TD(k_desat);
  TD(fnx);
  TD(vnitmax);
  TD(Kamm);
  TD(tnitmin);
  TD(tnitopt);
  TD(tnitop2);
  TD(tnitmax);
  TD(tnitopt_gauss);
  TD(scale_tnitopt);
  TD(rationit);
  TD(cmin_pdenit);
  TD(cmax_pdenit);
  TD(min_pdenit);
  TD(max_pdenit);
  TD(ratiodenit);
  TD(profdenit);
  TD(vpotdenit);
}

static void dump_soil_organic_module_parameters(const std::string &PATH,
                                                 const SoilOrganicModuleParameters &OBJ) {
  TD(po_SOM_SlowDecCoeffStandard);
  TD(po_SOM_FastDecCoeffStandard);
  TD(po_SMB_SlowMaintRateStandard);
  TD(po_SMB_FastMaintRateStandard);
  TD(po_SMB_SlowDeathRateStandard);
  TD(po_SMB_FastDeathRateStandard);
  TD(po_SMB_UtilizationEfficiency);
  TD(po_SOM_SlowUtilizationEfficiency);
  TD(po_SOM_FastUtilizationEfficiency);
  TD(po_AOM_SlowUtilizationEfficiency);
  TD(po_AOM_FastUtilizationEfficiency);
  TD(po_AOM_FastMaxC_to_N);
  TD(po_PartSOM_Fast_to_SOM_Slow);
  TD(po_PartSMB_Slow_to_SOM_Fast);
  TD(po_PartSMB_Fast_to_SOM_Fast);
  TD(po_PartSOM_to_SMB_Slow);
  TD(po_PartSOM_to_SMB_Fast);
  TD(po_CN_Ratio_SMB);
  TD(po_LimitClayEffect);
  TD(po_QTenFactor);
  TD(po_TempDecOptimal);
  TD(po_MoistureDecOptimal);
  TD(po_AmmoniaOxidationRateCoeffStandard);
  TD(po_NitriteOxidationRateCoeffStandard);
  TD(po_TransportRateCoeff);
  TD(po_SpecAnaerobDenitrification);
  TD(po_ImmobilisationRateCoeffNO3);
  TD(po_ImmobilisationRateCoeffNH4);
  TD(po_Denit1);
  TD(po_Denit2);
  TD(po_Denit3);
  TD(po_HydrolysisKM);
  TD(po_ActivationEnergy);
  TD(po_HydrolysisP1);
  TD(po_HydrolysisP2);
  TD(po_AtmosphericResistance);
  TD(po_N2OProductionRate);
  TD(po_Inhibitor_NH3);
  TD(ps_MaxMineralisationDepth);
  TB(__enable_kaiteew_TempOnDecompostion__);
  TB(__enable_kaiteew_MoistOnDecompostion__);
  TB(__enable_kaiteew_ClayOnDecompostion__);
  dump_stics_parameters(trace::join(PATH, "sticsParams"), OBJ.sticsParams);
}

static void dump_soil_organic(const std::string &PATH, const SoilOrganic &OBJ) {
  // soilColumn is a C++ reference, not a pointer - always "set" once
  // constructed, matching what the Odin walker's pointer skip emits for the
  // equivalent (never-nil) Odin pointer field. See soil_moisture_ref_main.cpp.
  trace::line_str(trace::join(PATH, "soilColumn"), "<ptr:set>");
  dump_soil_organic_module_parameters(trace::join(PATH, "params"), OBJ.params);
  TI(vs_NumberOfLayers);
  TI(vs_NumberOfOrganicLayers);
  TB(addedOrganicMatter);
  TD(irrigationAmount);
  TD_VEC(vo_ActAmmoniaOxidationRate);
  TD_VEC(vo_ActNitrificationRate);
  TD_VEC(vo_ActDenitrificationRate);
  TD_VEC(vo_AOM_FastDeltaSum);
  TD_VEC(vo_AOM_FastInput);
  TD_VEC(vo_AOM_FastSum);
  TD_VEC(vo_AOM_SlowDeltaSum);
  TD_VEC(vo_AOM_SlowInput);
  TD_VEC(vo_AOM_SlowSum);
  TD_VEC(vo_CBalance);
  TD(vo_DecomposerRespiration);
  TS(vo_ErrorMessage);
  TD_VEC(vo_InertSoilOrganicC);
  TD_VEC(vo_InertSoilOrganicC_highCN);
  TD(vo_N2O_Produced);
  TD(vo_N2O_Produced_Nit);
  TD(vo_N2O_Produced_Denit);
  TD(vo_NetEcosystemExchange);
  TD(vo_NetEcosystemProduction);
  TD(vo_NetNMineralisation);
  TD_VEC(vo_NetNMineralisationRate);
  TD(vo_Total_NH3_Volatilised);
  TD(vo_NH3_Volatilised);
  TD_VEC(vo_SMB_CO2EvolutionRate);
  TD_VEC(vo_SMB_FastDelta);
  TD_VEC(vo_SMB_SlowDelta);
  TD_VEC(vs_SoilMineralNContent);
  TD_VEC(vo_SoilOrganicC);
  TD_VEC(vo_SoilOrganicC_highCN);
  TD_VEC(vo_SOM_FastDelta);
  TD_VEC(vo_SOM_FastInput);
  TD_VEC(vo_SOM_SlowDelta);
  TD(vo_SumDenitrification);
  TD(vo_SumNetNMineralisation);
  TD(vo_SumN2O_Produced);
  TD(vo_SumNH3_Volatilised);
  TD(vo_TotalDenitrification);
  TB(incorporation);
  TP(cropModule);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 4) {
    fprintf(stderr, "usage: soil_organic_ref <pathToSimJson> <pathToClimateCsv> <numDays>\n");
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

  // --- build the real MonicaModel; model->soilTemperature/soilMoisture/
  // soilOrganic are all constructed by the unmodified
  // initializeMonicaModelFromParams ---
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
  model->soilOrganic->cropModule = nullptr;

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

    soiltemperature::step(model->soilTemperature.get(), tmin, tmax, globrad);
    soilmoisture::step(model->soilMoisture.get(), vs_GroundwaterDepth, precip, tmax, tmin,
                       (relhumid / 100.0), tavg, wind, model->envPs.p_WindSpeedHeight, globrad,
                       julday, et0);
    soilorganic::step(model->soilOrganic.get(), tavg, precip, wind);

    trace::set_day(day);
    dump_soil_organic("soilOrganic", *model->soilOrganic);
  }

  return 0;
}
