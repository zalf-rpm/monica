/* Proof driver for the trace-dump machinery (odin/tests/cpp_ref/trace_common.h
 * and odin/monica/trace/trace.odin).
 *
 * Builds the soil column from a site.json exactly as soil_column_ref does, then
 * dumps every SoilLayer field through the trace macros. Phase 3 already proved
 * that both implementations construct an identical SoilColumn, so any
 * disagreement here is a fault in the *trace machinery* - the value formatting,
 * the path construction or the field coverage - and not in the model. That is
 * what makes this a meaningful test of the harness itself before phase 4 starts
 * relying on it.
 *
 * Phase 4 extends this by adding dump functions for SoilMoisture,
 * SoilTemperature, SoilTransport, SoilOrganic, SnowComponent and FrostComponent
 * and calling them once per simulated day.
 *
 * Usage: trace_ref <pathToSiteJson>
 * See odin/tests/cpp_ref/run_trace.sh.
 */

#include <cstdio>
#include <string>

#include <functional>

#include "core/soilcolumn.h"
#include "json11/json11-helper.h"
#include "soil/soil.h"
#include "trace_common.h"

using namespace Tools;
using namespace json11;
using namespace monica;

static void dump_soil_layer(const std::string &PATH, const SoilLayer &OBJ) {
  TD(vs_LayerThickness);
  TD(vs_SoilWaterFlux);
  TD(vs_SOM_Slow);
  TD(vs_SOM_Fast);
  TD(vs_SMB_Slow);
  TD(vs_SMB_Fast);
  TD(vs_SoilCarbamid);
  TD(vs_SoilNH4);
  TD(vs_SoilNO2);
  TD(vs_SoilNO3);
  TB(vs_SoilFrozen);
  TD(vs_SoilSandContent);
  TD(vs_SoilClayContent);
  TD(vs_SoilpH);
  TD(vs_SoilStoneContent);
  TD(vs_Lambda);
  TD(vs_FieldCapacity);
  TD(vs_Saturation);
  TD(vs_PermanentWiltingPoint);
  TS(vs_SoilTexture);
  TD(vs_SoilAmmonium);
  TD(vs_SoilNitrate);
  TD(vs_Soil_CN_Ratio);
  TD(vs_SoilMoisturePercentFC);
  TD(vs_SoilRawDensity);
  TD(vs_SoilBulkDensity);
  TD(vs_SoilOrganicCarbon);
  TD(vs_SoilOrganicMatter);
  TD(vs_SoilMoisture_m3);
  TD(vs_SoilTemperature);

  for (size_t i = 0; i < OBJ.vo_AOM_Pool.size(); i++) {
    const std::string p = trace::index(trace::join(PATH, "vo_AOM_Pool"), (int)i);
    const AOM_Properties &a = OBJ.vo_AOM_Pool[i];
#define AOM(f) trace::line_f64(trace::join(p, #f), (double)a.f)
    AOM(vo_AOM_Slow);
    AOM(vo_AOM_Fast);
    AOM(vo_AOM_SlowDecRate_to_SMB_Slow);
    AOM(vo_AOM_SlowDecRate_to_SMB_Fast);
    AOM(vo_AOM_FastDecRate_to_SMB_Slow);
    AOM(vo_AOM_FastDecRate_to_SMB_Fast);
    AOM(vo_AOM_SlowDecCoeff);
    AOM(vo_AOM_FastDecCoeff);
    AOM(vo_AOM_SlowDecCoeffStandard);
    AOM(vo_AOM_FastDecCoeffStandard);
    AOM(vo_PartAOM_Slow_to_SMB_Slow);
    AOM(vo_PartAOM_Slow_to_SMB_Fast);
    AOM(vo_CN_Ratio_AOM_Slow);
    AOM(vo_CN_Ratio_AOM_Fast);
    AOM(vo_DaysAfterApplication);
    AOM(vo_AOM_DryMatterContent);
    AOM(vo_AOM_NH4Content);
    AOM(vo_AOM_SlowDelta);
    AOM(vo_AOM_FastDelta);
#undef AOM
    trace::line_bool(trace::join(p, "incorporation"), a.incorporation);
    trace::line_bool(trace::join(p, "noVolatilization"), a.noVolatilization);
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: trace_ref <pathToSiteJson>\n");
    return 2;
  }

  auto siteJ = readAndParseJsonFile(argv[1]);
  if (siteJ.failure()) {
    for (const auto &e : siteJ.errors) fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }

  // Mirrors soil_column_ref_main.cpp: call createEqualSizedSoilPMs directly
  // rather than going through siteparameters::merge, because the pwpFcSat
  // function map is populated by monica-run before the merge, not by the merge
  // itself - going through merge here yields an empty profile.
  auto siteParams = siteJ.result["SiteParameters"];
  auto soilProfileParams = siteParams["SoilProfileParameters"].array_items();

  std::string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  std::function<Errors(Soil::SoilParameters *, int)> setFn =
      Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);

  double layerThickness = double_valueD(siteParams, "LayerThickness", 0.1);
  int numberOfLayers = (int)int_valueD(siteParams, "NumberOfLayers", 20);

  auto pmsRes =
      Soil::createEqualSizedSoilPMs(setFn, soilProfileParams, layerThickness, numberOfLayers);
  if (pmsRes.failure()) {
    for (const auto &e : pmsRes.errors) fprintf(stderr, "%s\n", e.c_str());
  }

  auto sc = makeSoilColumn(layerThickness, 0.4, pmsRes.result);

  trace::set_day(0);
  for (size_t i = 0; i < sc->layers.size(); i++) {
    dump_soil_layer(trace::index("soilColumn.layers", (int)i), sc->layers[i]);
  }
  return 0;
}
