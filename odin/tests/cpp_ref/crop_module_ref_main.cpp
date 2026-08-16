/* Differential-test driver for phase 5 checkpoint 2 (CropModule scaffolding):
 * the CropModule struct and makeCropModule constructor, src/core/crop-module.{h,cpp}.
 *
 * No climate data or day loop is needed - makeCropModule is a single
 * construction, not a step function, and makeMonicaModel only needs a
 * CentralParameterProvider. Builds a real MonicaModel (for a real, live
 * SoilColumn - same "real MonicaModel, bare soil" approach as every phase-4
 * checkpoint) plus a real CropParameters/CropResidueParameters loaded exactly
 * like params_ref_main.cpp loads them (wheat species + winter-wheat cultivar +
 * wheat crop residues from $MONICA_PARAMETERS), then calls makeCropModule and
 * dumps the resulting struct's ~130 own scalar/vector fields.
 *
 * Deliberately NOT dumped, all for the same reason - already verified
 * elsewhere, or untouched by this constructor, so re-dumping them here would
 * only add noise, not coverage:
 *   - cropParams / residueParams / perennialCropParams: copied/cloned by
 *     value from inputs already round-tripped through cropparameters::merge/
 *     to_json in phase 1's params_ref oracle.
 *   - soilColumn / siteParams / simParams / cropModParams / intercropping:
 *     pointers, dumped as set/nil only (matches the Odin walker's pointer
 *     handling).
 *   - guentherEmissions / jjvEmissions / vocSpecies / cropPhotosynthesisResults
 *     / perennialCropDormancyPeriodEndDate: default-constructed by the
 *     constructor, never touched by it - checkpoint 1 already verified
 *     Voc::SpeciesData's in-class defaults; a later checkpoint that actually
 *     populates these is the right place to verify them.
 *   - fireEvent / addOrganicMatter / getSnowDepthAndCalcTempUnderSnow:
 *     function values, not dumpable (matches the Odin walker's proc skip).
 *
 * Four scenarios (indexed as trace "days" 0-3) exercise makeCropModule's
 * branches:
 *   0: real wheat + real Hohenfinow2 site/crop-module params (baseline;
 *      pc_AdjustRootDepthForSoilProps=true, vs_ImpenetrableLayerDepth=-1 ->
 *      no clamp)
 *   1: same but pc_AdjustRootDepthForSoilProps forced false (skips the
 *      soil-adjusted rooting-depth branch entirely)
 *   2: same as 0 but vs_ImpenetrableLayerDepth forced to a small positive
 *      value that is less than the computed max rooting depth (clamp branch)
 *   3: same as 0 but with a synthetic cultivar whose pc_StageKcFactor is
 *      scaled down to peak below 1.0 (the Kcb "low-coverage crop" branch;
 *      real wheat's pc_StageKcFactor peaks at 1.1, so scenario 0 already
 *      exercises the "high-coverage" branch on its own - this scenario is
 *      what reaches the other side of that `if`)
 *
 * Usage: crop_module_ref <pathToSimJson>
 * See odin/tests/cpp_ref/run_crop_module.sh.
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

static void dump_crop_module(const std::string &PATH, const CropModule &OBJ) {
  TI(noOfOrgans);
  TI(noOfDevStages);

  TI(vc_TransplantShockDuration);
  TI(vc_DaysSinceTransplant);
  TD(vc_TransplantEfficiency);

  TD(vc_TranspirationDeficit);
  TD(vc_PotentialTranspirationDeficit);
  TD(vc_ActualTranspirationDeficit);
  TD(vc_TranspirationReduced);
  TD(rootNRedux);
  TI(vc_TimeUnderAnoxia);

  TP(intercropping);
  TP(soilColumn);
  TP(siteParams);
  TP(simParams);
  TP(cropModParams);

  TD(vc_AbovegroundBiomass);
  TD(vc_AbovegroundBiomassOld);
  TD(vc_ActualTranspiration);
  TD(vc_Assimilates);
  TD(vc_AssimilationRate);
  TD(vc_AstronomicDayLenght);
  TD(vc_BelowgroundBiomass);
  TD(vc_BelowgroundBiomassOld);
  TD(vc_ClearDayRadiation);
  TI(pc_CO2Method);
  TD(vc_CriticalNConcentration);
  TD(vc_CropDiameter);
  TD(vc_CropFrostRedux);
  TD(vc_CropHeatRedux);
  TD(vc_CropHeight);
  TD(vc_CropNDemand);
  TD(vc_CropNRedux);
  TD_VEC(vc_CropWaterUptake);
  TD_VEC(vc_CurrentTemperatureSum);
  TD(vc_CurrentTotalTemperatureSum);
  TD(vc_CurrentTotalTemperatureSumRoot);
  TD(vc_DaylengthFactor);
  TI(vc_DaysAfterBeginFlowering);
  TD(vc_Declination);
  TI(vc_DevelopmentalStage);
  TI(noOfCropSteps);
  TD(vc_DroughtImpactOnFertility);
  TD(vc_EffectiveDayLength);
  TB(vc_ErrorStatus);
  TS(vc_ErrorMessage);
  TD(vc_EvaporatedFromIntercept);
  TD(vc_ExtraterrestrialRadiation);
  TI(vc_FinalDevelopmentalStage);
  TD(vc_FixedN);
  TD(vc_GlobalRadiation);
  TD(vc_GreenAreaIndex);
  TD(vc_GrossAssimilates);
  TD(vc_GrossPhotosynthesis);
  TD(vc_GrossPhotosynthesis_mol);
  TD(vc_GrossPhotosynthesisReference_mol);
  TD(vc_GrossPrimaryProduction);
  TB(vc_GrowthCycleEnded);
  TD(vc_GrowthRespirationAS);
  TD(vc_InterceptionStorage);
  TD(vc_KcFactor);
  TD(vc_LeafAreaIndex);
  TD_VEC(vc_sunlitLeafAreaIndex);
  TD_VEC(vc_shadedLeafAreaIndex);
  TD(vc_LT50);
  TD(vc_LT50M);
  TD(vc_MaintenanceRespirationAS);
  TD(vc_MaxNUptake);
  TD(vc_MaxRootingDepth);
  TD(vc_NetMaintenanceRespiration);
  TD(vc_NetPhotosynthesis);
  TD(vc_NetPrecipitation);
  TD(vc_NetPrimaryProduction);
  TD(vc_NConcentrationAbovegroundBiomass);
  TD(vc_NConcentrationAbovegroundBiomassOld);
  TD(vc_NContentDeficit);
  TD(vc_NConcentrationRoot);
  TD(vc_NConcentrationRootOld);
  TD_VEC(vc_NUptakeFromLayer);
  TD_VEC(vc_OrganBiomass);
  TD_VEC(vc_OrganDeadBiomass);
  TD_VEC(vc_OrganGreenBiomass);
  TD_VEC(vc_OrganGrowthIncrement);
  TD_VEC(vc_OrganSenescenceIncrement);
  TD(vc_OvercastDayRadiation);
  TD(vc_OxygenDeficit);
  TD(vc_PhotoperiodicDaylength);
  TD(vc_PhotActRadiationMean);
  TD(vc_PotentialTranspiration);
  TD(vc_ReferenceEvapotranspiration);
  TD(vc_RelativeTotalDevelopment);
  TD(vc_RemainingEvapotranspiration);
  TD(vc_ReserveAssimilatePool);
  TD(vc_RootBiomass);
  TD(vc_RootBiomassOld);
  TD_VEC(vc_RootDensity);
  TD_VEC(vc_RootDiameter);
  TD_VEC(vc_RootEffectivity);
  TI(vc_RootingDepth);
  TD(vc_RootingDepth_m);
  TI(vc_RootingZone);
  TD(vc_SoilCoverage);
  TD_VEC(vs_SoilMineralNContent);
  TD(vc_SoilSpecificMaxRootingDepth);
  TD(vs_SoilSpecificMaxRootingDepth);
  TD(vc_KcbFactor);
  TD(vc_Kcb_ini);
  TD(vc_Kcb_mid);
  TD(vc_Kcb_end);
  TD(vc_StomataResistance);
  TI(vc_StorageOrgan);
  TD(vc_TargetNConcentration);
  TD(vc_TimeStep);
  TI(TimeUnderAnoxiaThresholdDefault);
  TD(vc_TotalBiomass);
  TD(vc_TotalBiomassNContent);
  TD(vc_TotalCropHeatImpact);
  TD(vc_TotalNInput);
  TD(vc_TotalNUptake);
  TD(vc_TotalRespired);
  TD(vc_Respiration);
  TD(vc_SumTotalNUptake);
  TD(vc_TotalRootLength);
  TD(vc_TotalTemperatureSum);
  TD(vc_TemperatureSumToFlowering);
  TD_VEC(vc_Transpiration);
  TD_VEC(vc_TranspirationRedux);
  TD(vc_VernalisationDays);
  TD(vc_VernalisationFactor);
  TB(dyingOut);
  TD(vc_AccumulatedETa);
  TD(vc_AccumulatedTranspiration);
  TD(vc_sumExportedCutBiomass);
  TD(vc_exportedCutBiomass);
  TD(vc_sumResidueCutBiomass);
  TD(vc_residueCutBiomass);
  TI(vc_CuttingDelayDays);
  TI(vc_AnthesisDay);
  TI(vc_MaturityDay);
  TB(vc_MaturityReached);

  TI(stepSize24);
  TI(stepSize240);
  TD_VEC(rad24);
  TD_VEC(rad240);
  TD_VEC(tfol24);
  TD_VEC(tfol240);
  TI(index24);
  TI(index240);
  TB(full24);
  TB(full240);

  TD(vc_O3_shortTermDamage);
  TD(vc_O3_longTermDamage);
  TD(vc_O3_senescence);
  TD(vc_O3_sumUptake);
  TD(vc_O3_WStomatalClosure);

  TB(assimilatePartCoeffsReduced);
  TD(vc_KTkc);
  TD(vc_KTko);

  TB(stemElongationEventFired);

  TD(intercroppingOtherCropHeight);
  TD(intercroppingOtherLAIt);

  TD(fractionOfInterceptedRadiation1);
  TD(fractionOfInterceptedRadiation2);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: crop_module_ref <pathToSimJson>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];

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

  // --- real MonicaModel, for a real, live SoilColumn ---
  auto model = makeMonicaModel(cpp);

  // --- real CropParameters / CropResidueParameters, loaded the same way
  // params_ref_main.cpp loads them ---
  std::string monicaParametersDir =
      fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}"));

  CropParameters wheatCropParams;
  cropparameters::merge(&wheatCropParams, load(monicaParametersDir + "/crops/wheat.json"),
                        load(monicaParametersDir + "/crops/wheat/winter-wheat.json"));

  CropResidueParameters wheatResidueParams;
  cropresidueparameters::merge(&wheatResidueParams,
                               load(monicaParametersDir + "/crop-residues/wheat.json"));

  auto noopFireEvent = [](std::string) {};
  auto noopAddOrganicMatter = [](std::map<size_t, double>, double) {};
  auto noopGetSnowDepth = [](double) { return std::make_pair(0.0, 0.0); };

  // --- scenario 0: baseline ---
  {
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    trace::set_day(0);
    dump_crop_module("cropModule", *cm);
  }

  // --- scenario 1: pc_AdjustRootDepthForSoilProps forced false ---
  {
    CropModuleParameters cropModParams = cpp.userCropParameters;
    cropModParams.pc_AdjustRootDepthForSoilProps = false;
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cropModParams, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    trace::set_day(1);
    dump_crop_module("cropModule", *cm);
  }

  // --- scenario 2: vs_ImpenetrableLayerDepth clamp branch ---
  {
    SiteParameters siteParams = cpp.siteParameters;
    siteParams.vs_ImpenetrableLayerDepth = 0.2;
    auto cm = makeCropModule(model->soilColumn.get(), &wheatCropParams, &wheatResidueParams,
                             &siteParams, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    trace::set_day(2);
    dump_crop_module("cropModule", *cm);
  }

  // --- scenario 3: synthetic low-Kc cultivar (Kcb "low-coverage crop" branch) ---
  {
    CropParameters lowKcCropParams = wheatCropParams;
    for (auto &v : lowKcCropParams.cultivarParams.pc_StageKcFactor) v *= 0.5;
    auto cm = makeCropModule(model->soilColumn.get(), &lowKcCropParams, &wheatResidueParams,
                             &cpp.siteParameters, &cpp.userCropParameters, &cpp.simulationParameters,
                             noopFireEvent, noopAddOrganicMatter, noopGetSnowDepth, nullptr);
    trace::set_day(3);
    dump_crop_module("cropModule", *cm);
  }

  return 0;
}
