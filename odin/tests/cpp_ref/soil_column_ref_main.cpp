/* Differential-test driver for phase 3 checkpoint 3b: SoilLayer/SoilColumn
 * construction.
 *
 * Reads a site.json fixture's SiteParameters.SoilProfileParameters directly
 * (no include-from-file/ref resolution needed - it's inline in the
 * Hohenfinow2 fixtures), builds the equal-sized soil parameter list via
 * createEqualSizedSoilPMs (already covered on its own by the phase 3a
 * interpolation sweep; exercised again here against real data), constructs a
 * SoilColumn via makeSoilColumn, and dumps every layer's full state plus a
 * few SoilColumn-level getters.
 *
 * pwpFcSatFunction is hardcoded to "Wessolek2009" and maxMineralisationDepth
 * to 0.4 (both match what the Hohenfinow2 fixtures actually resolve to -
 * checkpoint 3c wires this up through the real merge/CentralParameterProvider
 * pipeline instead of hardcoding it).
 *
 * Prints:
 *   SC<TAB>numberOfLayers<TAB>vs_NumberOfOrganicLayers<TAB>layerThickness
 *   L<TAB>index<TAB>...32 fields, see the Odin driver for the exact list...
 *   DEPTH<TAB>depth<TAB>getLayerNumberForDepth
 *   SUMTEMP<TAB>sumSoilTemperature(all layers)
 *   CALCORG<TAB>calculateNumberOfOrganicLayers
 * doubles formatted "%.17g", matching jsonx's validated MSVC-%.17g-identical
 * dump used on the Odin side.
 *
 * Usage: soil_column_ref <pathToSiteJson>
 * See odin/tests/cpp_ref/run_soil_column.sh.
 */

#include <cstdio>
#include <string>

#include "core/soilcolumn.h"
#include "json11/json11-helper.h"
#include "tools/helper.h"

using namespace std;
using namespace monica;
using namespace Soil;
using namespace Tools;
using namespace json11;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: soil_column_ref <pathToSiteJson>\n");
    return 2;
  }
  string pathToSiteJson = argv[1];

  auto siteJ = readAndParseJsonFile(pathToSiteJson);
  if (siteJ.failure()) {
    for (const auto &e : siteJ.errors) fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }

  auto siteParams = siteJ.result["SiteParameters"];
  auto soilProfileParams = siteParams["SoilProfileParameters"].array_items();

  string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  std::function<Errors(SoilParameters *, int)> setFn =
      Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);

  double layerThickness = double_valueD(siteParams, "LayerThickness", 0.1);
  int numberOfLayers = (int)int_valueD(siteParams, "NumberOfLayers", 20);

  auto pmsRes =
      Soil::createEqualSizedSoilPMs(setFn, soilProfileParams, layerThickness, numberOfLayers);
  if (pmsRes.failure()) {
    for (const auto &e : pmsRes.errors) fprintf(stderr, "%s\n", e.c_str());
  }

  double maxMineralisationDepth = 0.4;
  auto sc = makeSoilColumn(layerThickness, maxMineralisationDepth, pmsRes.result);

  printf("SC\t%d\t%d\t%.17g\n", (int)soilcolumn::numberOfLayers(sc.get()),
         sc->vs_NumberOfOrganicLayers, soilcolumn::layerThickness(sc.get()));

  for (size_t i = 0; i < sc->layers.size(); i++) {
    auto &sl = sc->layers[i];
    printf("L\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%d\t"
           "%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%s\t%.17g\t%.17g\t%.17g\t"
           "%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",
           (int)i, sl.vs_LayerThickness, sl.vs_SoilWaterFlux, sl.vs_SOM_Slow, sl.vs_SOM_Fast,
           sl.vs_SMB_Slow, sl.vs_SMB_Fast, sl.vs_SoilCarbamid, sl.vs_SoilNH4, sl.vs_SoilNO2,
           sl.vs_SoilNO3, sl.vs_SoilFrozen ? 1 : 0, sl.vs_SoilSandContent, sl.vs_SoilClayContent,
           sl.vs_SoilpH, sl.vs_SoilStoneContent, sl.vs_Lambda, sl.vs_FieldCapacity,
           sl.vs_Saturation, sl.vs_PermanentWiltingPoint, sl.vs_SoilTexture.c_str(),
           sl.vs_SoilAmmonium, sl.vs_SoilNitrate, sl.vs_Soil_CN_Ratio, sl.vs_SoilMoisturePercentFC,
           sl.vs_SoilRawDensity, sl.vs_SoilBulkDensity, sl.vs_SoilOrganicCarbon,
           sl.vs_SoilOrganicMatter, sl.vs_SoilMoisture_m3, sl.vs_SoilTemperature,
           soillayer::soilMoisturePF(&sl), soillayer::soilNmin(&sl),
           soillayer::soilSiltContent(&sl), soillayer::soilRawDensity(&sl),
           soillayer::soilBulkDensity(&sl));
    // (soilOrganicCarbon/soilOrganicMatter appended via a 2nd printf to keep
    // the format string above under common compiler literal-length limits)
    printf("L2\t%d\t%.17g\t%.17g\n", (int)i, soillayer::soilOrganicCarbon(&sl),
           soillayer::soilOrganicMatter(&sl));
  }

  double depths[] = {0.05, 0.35, 1.0, 1.99, 2.5};
  for (double depth : depths) {
    printf("DEPTH\t%.17g\t%d\n", depth, (int)soilcolumn::getLayerNumberForDepth(sc.get(), depth));
  }
  printf("SUMTEMP\t%.17g\n", soilcolumn::sumSoilTemperature(sc.get(), (int)sc->layers.size()));
  printf("CALCORG\t%d\n", soilcolumn::calculateNumberOfOrganicLayers(sc.get()));

  return 0;
}
