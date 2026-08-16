/* Differential-test driver for phase 3's "required second oracle" (plan-odin.md):
 * an interpolation sweep over fcSatPwpFromKA5textureClass and its
 * fcSatPwpFromVanGenuchten{Vereecken,Toth}/fcSatPwpFromToth siblings, reached
 * through the public updateUnsetPwpFcSatFrom* entry points (the fcSatPwpFrom*
 * functions themselves, and updateUnsetPwpFcSatFromKA5textureClass, are C++
 * anonymous-namespace/file-local - not linkable from here; the public wrappers
 * exercise exactly the same code).
 *
 * The per-layer state diff (createSoilPMs + SoilColumn, a later phase-3
 * checkpoint) only exercises the soil types and densities the Hohenfinow2
 * fixtures happen to contain. This sweeps every KA5 texture class in
 * SoilCharacteristicData.json x raw densities spanning and straddling the
 * 1100-1900 kg/m3 table breakpoints x organic-matter values straddling the
 * 0/1.5/3/6/11.5 breakpoints, plus a handful of soilRawDensity/soilBulkDensity
 * and soilOrganicCarbon/soilOrganicMatter fallback-resolution combos, plus a
 * grid for the three closed-form Van Genuchten / Toth variants.
 *
 * Prints one row per case:
 *   KA5|KA5FB|VGV|VGT|TOTH<TAB>...inputs...<TAB>success(0/1)<TAB>fc<TAB>sat<TAB>pwp
 * doubles formatted "%.17g", matching jsonx's validated MSVC-%.17g-identical dump.
 *
 * Usage: soil_pwp_fc_sat_ref (no args)
 * See odin/tests/cpp_ref/run_soil_pwp_fc_sat.sh.
 */

#include <cstdio>
#include <string>
#include <vector>

#include "soil/soil.h"
#include "tools/helper.h"

using namespace std;
using namespace Soil;
using namespace Tools;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);

  string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  auto ka5Fn = Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);

  vector<string> textures = {"HH",  "HN",  "LS2", "LS3", "LS4", "LT2", "LT3", "LTS", "LU",
                             "SL2", "SL3", "SL4", "SLU", "SS",  "ST2", "ST3", "SU2", "SU3",
                             "SU4", "TL",  "TS2", "TS3", "TS4", "TT",  "TU2", "TU3", "TU4",
                             "ULS", "US",  "UT2", "UT3", "UT4", "UU",  "FS",  "FSGS", "FSMS",
                             "GS",  "MS",  "MSFS", "MSGS", "XX" /* unknown - error path */};
  vector<double> rawDensities; // kg/m3, spans and straddles the 1100-1900 breakpoints
  for (double rd = 900; rd <= 2100; rd += 50) rawDensities.push_back(rd);
  vector<double> organicMatters = {0.0, 0.005, 0.015, 0.03, 0.06, 0.115, 0.15}; // kg/kg fractions

  for (const auto &tex : textures) {
    for (double rd : rawDensities) {
      for (double om : organicMatters) {
        SoilParameters sp;
        sp.vs_SoilTexture = tex;
        sp.vs_SoilStoneContent = 0.1;
        sp._vs_SoilRawDensity = rd;
        sp._vs_SoilOrganicMatter = om;
        sp.vs_FieldCapacity = -1;
        sp.vs_Saturation = -1;
        sp.vs_PermanentWiltingPoint = -1;
        Errors e = ka5Fn(&sp, 1);
        printf("KA5\t%s\t%.17g\t%.17g\t%d\t%.17g\t%.17g\t%.17g\n", tex.c_str(), rd, om,
               e.success() ? 1 : 0, sp.vs_FieldCapacity, sp.vs_Saturation,
               sp.vs_PermanentWiltingPoint);
      }
    }
  }

  // soilRawDensity/soilOrganicMatter fallback-resolution combos: leave the raw
  // override unset (-1) and set the companion (bulk density / organic carbon)
  // instead, so soilparameters::soilRawDensity/soilOrganicMatter compute the
  // fallback formula rather than passing a direct override through.
  {
    vector<double> claysForFallback = {0.05, 0.15, 0.3};
    for (double clay : claysForFallback) {
      SoilParameters sp;
      sp.vs_SoilTexture = "LS2";
      sp.vs_SoilClayContent = clay;
      sp.vs_SoilStoneContent = 0.0;
      sp._vs_SoilBulkDensity = 1500;  // raw density stays -1 -> fallback formula
      sp._vs_SoilOrganicCarbon = 0.01; // organic matter stays -1 -> fallback formula
      sp.vs_FieldCapacity = -1;
      sp.vs_Saturation = -1;
      sp.vs_PermanentWiltingPoint = -1;
      Errors e = ka5Fn(&sp, 1);
      printf("KA5FB\t%.17g\t%d\t%.17g\t%.17g\t%.17g\n", clay, e.success() ? 1 : 0,
             sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
    }
  }

  vector<double> sands = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
  vector<double> clays = {0.0, 0.05, 0.1, 0.2, 0.3, 0.4};
  vector<double> bulkDensities = {1100, 1300, 1500, 1700, 1900};
  vector<double> orgCarbons = {0.005, 0.01, 0.02, 0.03};
  vector<double> stones = {0.0, 0.15};
  vector<int> layerNos = {1, 5}; // exercises VanGenuchtenToth's isTopSoil true/false

  for (double sand : sands) {
    for (double clay : clays) {
      if (sand + clay > 1.0) continue;
      for (double bd : bulkDensities) {
        for (double oc : orgCarbons) {
          for (double stone : stones) {
            {
              SoilParameters sp;
              sp.vs_SoilSandContent = sand;
              sp.vs_SoilClayContent = clay;
              sp.vs_SoilStoneContent = stone;
              sp._vs_SoilBulkDensity = bd;
              sp._vs_SoilOrganicCarbon = oc;
              sp.vs_FieldCapacity = -1;
              sp.vs_Saturation = -1;
              sp.vs_PermanentWiltingPoint = -1;
              Errors e = Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken(&sp, 1);
              printf("VGV\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%d\t%.17g\t%.17g\t%.17g\n", sand,
                     clay, bd, oc, stone, e.success() ? 1 : 0, sp.vs_FieldCapacity,
                     sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
            }
            for (int layerNo : layerNos) {
              SoilParameters sp;
              sp.vs_SoilSandContent = sand;
              sp.vs_SoilClayContent = clay;
              sp.vs_SoilStoneContent = stone;
              sp._vs_SoilBulkDensity = bd;
              sp._vs_SoilOrganicCarbon = oc;
              sp.vs_FieldCapacity = -1;
              sp.vs_Saturation = -1;
              sp.vs_PermanentWiltingPoint = -1;
              Errors e = Soil::updateUnsetPwpFcSatFromVanGenuchtenToth(&sp, layerNo);
              printf("VGT\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%d\t%d\t%.17g\t%.17g\t%.17g\n",
                     sand, clay, bd, oc, stone, layerNo, e.success() ? 1 : 0,
                     sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
            }
            {
              SoilParameters sp;
              sp.vs_SoilSandContent = sand;
              sp.vs_SoilClayContent = clay;
              sp.vs_SoilStoneContent = stone;
              sp._vs_SoilBulkDensity = bd;
              sp._vs_SoilOrganicCarbon = oc;
              sp.vs_FieldCapacity = -1;
              sp.vs_Saturation = -1;
              sp.vs_PermanentWiltingPoint = -1;
              Errors e = Soil::updateUnsetPwpFcSatFromToth(&sp, 1);
              printf("TOTH\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%d\t%.17g\t%.17g\t%.17g\n", sand,
                     clay, bd, oc, stone, e.success() ? 1 : 0, sp.vs_FieldCapacity,
                     sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
            }
          }
        }
      }
    }
  }

  // -1 sentinel combinations (plan-odin.md phase 3's "required second oracle"):
  // every case above leaves vs_FieldCapacity/vs_Saturation/vs_PermanentWiltingPoint
  // ALL unset (-1). But updateUnsetPwpFcSatFrom* checks each of the three
  // independently (`if (sp->vs_FieldCapacity < 0) ...`, `if (sp->vs_Saturation < 0)
  // ...`, `if (sp->vs_PermanentWiltingPoint < 0) ...`) after the disjunctive
  // "is anything unset" guard - the exact sentinel-with-fallback shape that caused
  // the ff0f0fc regression (plan.md) elsewhere. A caller that already knows e.g.
  // field capacity from a site.json horizon override, and only wants saturation and
  // wilting point computed, exercises a per-field branch none of the rows above
  // ever reach. Sweep all 8 combinations of which of the three are preset
  // (non-negative) vs -1, through each of the four entry points.
  {
    const double fcPreset = 0.35, satPreset = 0.45, pwpPreset = 0.12;
    for (int mask = 0; mask < 8; mask++) {
      bool fcSet = mask & 1, satSet = mask & 2, pwpSet = mask & 4;

      {
        SoilParameters sp;
        sp.vs_SoilTexture = "LS2";
        sp.vs_SoilStoneContent = 0.1;
        sp._vs_SoilRawDensity = 1500;
        sp._vs_SoilOrganicMatter = 0.03;
        sp.vs_FieldCapacity = fcSet ? fcPreset : -1;
        sp.vs_Saturation = satSet ? satPreset : -1;
        sp.vs_PermanentWiltingPoint = pwpSet ? pwpPreset : -1;
        Errors e = ka5Fn(&sp, 1);
        printf("KA5SENT\t%d\t%d\t%.17g\t%.17g\t%.17g\n", mask, e.success() ? 1 : 0,
               sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
      }
      {
        SoilParameters sp;
        sp.vs_SoilSandContent = 0.3;
        sp.vs_SoilClayContent = 0.15;
        sp.vs_SoilStoneContent = 0.1;
        sp._vs_SoilBulkDensity = 1500;
        sp._vs_SoilOrganicCarbon = 0.01;
        sp.vs_FieldCapacity = fcSet ? fcPreset : -1;
        sp.vs_Saturation = satSet ? satPreset : -1;
        sp.vs_PermanentWiltingPoint = pwpSet ? pwpPreset : -1;
        Errors e = Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken(&sp, 1);
        printf("VGVSENT\t%d\t%d\t%.17g\t%.17g\t%.17g\n", mask, e.success() ? 1 : 0,
               sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
      }
      {
        SoilParameters sp;
        sp.vs_SoilSandContent = 0.3;
        sp.vs_SoilClayContent = 0.15;
        sp.vs_SoilStoneContent = 0.1;
        sp._vs_SoilBulkDensity = 1500;
        sp._vs_SoilOrganicCarbon = 0.01;
        sp.vs_FieldCapacity = fcSet ? fcPreset : -1;
        sp.vs_Saturation = satSet ? satPreset : -1;
        sp.vs_PermanentWiltingPoint = pwpSet ? pwpPreset : -1;
        Errors e = Soil::updateUnsetPwpFcSatFromVanGenuchtenToth(&sp, 1);
        printf("VGTSENT\t%d\t%d\t%.17g\t%.17g\t%.17g\n", mask, e.success() ? 1 : 0,
               sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
      }
      {
        SoilParameters sp;
        sp.vs_SoilSandContent = 0.3;
        sp.vs_SoilClayContent = 0.15;
        sp.vs_SoilStoneContent = 0.1;
        sp._vs_SoilBulkDensity = 1500;
        sp._vs_SoilOrganicCarbon = 0.01;
        sp.vs_FieldCapacity = fcSet ? fcPreset : -1;
        sp.vs_Saturation = satSet ? satPreset : -1;
        sp.vs_PermanentWiltingPoint = pwpSet ? pwpPreset : -1;
        Errors e = Soil::updateUnsetPwpFcSatFromToth(&sp, 1);
        printf("TOTHSENT\t%d\t%d\t%.17g\t%.17g\t%.17g\n", mask, e.success() ? 1 : 0,
               sp.vs_FieldCapacity, sp.vs_Saturation, sp.vs_PermanentWiltingPoint);
      }
    }
  }

  return 0;
}
