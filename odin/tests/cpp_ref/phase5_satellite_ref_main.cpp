/* Differential-test driver for phase 5's first checkpoint: the four
 * self-contained satellite modules crop-module.cpp depends on -
 * photosynthesis-FvCB, O3-impact, voc-guenther, voc-jjv (voc-common is pure
 * data, exercised through the other three).
 *
 * Unlike every phase-4 driver, none of these take a SoilColumn/climate/
 * MonicaModel - every function here is a pure scalar/struct-in, struct-out
 * calculation, confirmed by reading each header. So this is a parameter
 * sweep oracle (the same shape as phase 3's fcSatPwpFromKA5textureClass
 * sweep), not a daily trace-diff: no "day" concept applies to code that's
 * really called hourly, inside crop-module.cpp's not-yet-ported
 * photosynthesis loop.
 *
 * Prints one row per case, prefixed by which function it exercises
 * (FVCB/O3/GUENTHER/JJV), tab-separated, doubles formatted "%.17g" except
 * NaN, which is normalized to the literal "NAN" - MSVC's printf renders NaN
 * as "nan" or "-nan(ind)" depending on how it arose, while Odin's strconv
 * renders it "NaN"; the underlying bit-is-NaN fact is what actually matters
 * (real, deterministic IEEE754 propagation from a couple of grid points
 * where LAI=0 drives a division by zero - not the C++-quirk/uninitialised-
 * memory kind of divergence), so both sides normalize to one spelling
 * instead of trying to match an arbitrary NaN payload's text rendering.
 * Rows built via a small tab-join helper, not raw printf - a raw-printf
 * format-string/argument-count mismatch is exactly the bug that motivated
 * switching to this style while bringing this driver up.
 *
 * Usage: phase5_satellite_ref (no args)
 * See odin/tests/cpp_ref/run_phase5_satellite.sh.
 */

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/O3-impact.h"
#include "core/photosynthesis-FvCB.h"
#include "core/voc-guenther.h"
#include "core/voc-jjv.h"

using namespace std;

static string fmtd(double v) {
  if (std::isnan(v)) return "NAN";
  char buf[64];
  snprintf(buf, sizeof(buf), "%.17g", v);
  return string(buf);
}

static string fmti(long long v) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", v);
  return string(buf);
}

static void row(const vector<string> &fields) {
  string line;
  for (size_t i = 0; i < fields.size(); i++) {
    if (i > 0) line += "\t";
    line += fields[i];
  }
  printf("%s\n", line.c_str());
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // --- FvCB: full grid over the 7 scalar inputs ---
  {
    vector<double> global_rads = {0.0, 2.0, 8.0};
    vector<double> extra_terr_rads = {5.0, 20.0};
    vector<double> solar_els = {-0.2, 0.0, 0.5, 1.3};
    vector<double> LAIs = {0.0, 1.0, 4.0};
    vector<double> leaf_temps = {-5.0, 15.0, 30.0};
    vector<double> VPDs = {0.2, 2.0};
    vector<double> Cas = {300.0, 600.0};

    FvCB::FvCB_canopy_hourly_params par;
    par.Vcmax_25 = 60.0;

    for (double global_rad : global_rads)
      for (double extra_terr_rad : extra_terr_rads)
        for (double solar_el : solar_els)
          for (double LAI : LAIs)
            for (double leaf_temp : leaf_temps)
              for (double VPD : VPDs)
                for (double Ca : Cas) {
                  FvCB::FvCB_canopy_hourly_in in;
                  in.global_rad = global_rad;
                  in.extra_terr_rad = extra_terr_rad;
                  in.solar_el = solar_el;
                  in.LAI = LAI;
                  in.leaf_temp = leaf_temp;
                  in.VPD = VPD;
                  in.Ca = Ca;

                  auto out = FvCB::FvCB_canopy_hourly_C3(in, par);
                  // NOTE(c++-quirk, not reproducible): when global_rad <= 0,
                  // FvCB_canopy_hourly_C3 never assigns out.{sunlit,shaded}.
                  // {ci,cc} (see the "handle cases where no photosynthesis
                  // can occur" branch) and FvCB_leaf_fraction::ci/cc have no
                  // in-class initialiser, so they hold indeterminate stack
                  // garbage - genuine C++ UB, not a value any translation
                  // could or should reproduce. Normalized to 0 here (which is
                  // what Odin's zero-initialised local naturally produces)
                  // rather than diffing uninitialised memory, the same
                  // "documented gap, symmetric normalisation" pattern used
                  // for SoilProfileParameters in the phase 1 capstone.
                  if (in.global_rad <= 0.0) {
                    out.sunlit.ci = out.sunlit.cc = out.shaded.ci = out.shaded.cc = 0.0;
                  }
                  row({"FVCB", fmtd(global_rad), fmtd(extra_terr_rad), fmtd(solar_el), fmtd(LAI),
                       fmtd(leaf_temp), fmtd(VPD), fmtd(Ca), fmtd(out.canopy_net_photos),
                       fmtd(out.canopy_resp), fmtd(out.canopy_gross_photos), fmtd(out.jmax_c),
                       fmtd(out.sunlit.LAI), fmtd(out.sunlit.gs), fmtd(out.sunlit.kc),
                       fmtd(out.sunlit.ko), fmtd(out.sunlit.oi), fmtd(out.sunlit.ci),
                       fmtd(out.sunlit.cc), fmtd(out.sunlit.comp), fmtd(out.sunlit.vcMax),
                       fmtd(out.sunlit.jMax), fmtd(out.sunlit.rad), fmtd(out.sunlit.jj),
                       fmtd(out.sunlit.jv), fmtd(out.shaded.LAI), fmtd(out.shaded.gs),
                       fmtd(out.shaded.kc), fmtd(out.shaded.ko), fmtd(out.shaded.oi),
                       fmtd(out.shaded.ci), fmtd(out.shaded.cc), fmtd(out.shaded.comp),
                       fmtd(out.shaded.vcMax), fmtd(out.shaded.jMax), fmtd(out.shaded.rad),
                       fmtd(out.shaded.jj), fmtd(out.shaded.jv)});
                }
  }

  // --- O3-impact: curated scenarios, varying a few dims at a time ---
  {
    O3impact::O3_impact_params par;
    struct Scenario {
      double FC, WP, SWC, ET0, O3a, gs;
      int h;
      double reldev, GDD_flo, GDD_mat, fO3s_d_prev, sum_O3_up;
      bool waterDeficit;
    };
    vector<Scenario> scenarios;
    // vary h (0 vs >0) and reldev around the senescence/recovery thresholds
    for (int h : {0, 1, 12}) {
      for (double reldev : {0.05, 0.2, 0.5, 0.9}) {
        for (bool wd : {false, true}) {
          scenarios.push_back({0.30, 0.10, 0.20, 4.0, 40.0, 0.3, h, reldev, 500.0, 1200.0, 0.8, 10.0, wd});
        }
      }
    }
    // vary SWC across FC/WP boundary, and sum_O3_up
    for (double SWC : {0.05, 0.10, 0.15, 0.20, 0.25, 0.35}) {
      for (double sum_O3_up : {0.0, 5.0, 50.0}) {
        scenarios.push_back({0.30, 0.10, SWC, 4.0, 40.0, 0.3, 6, 0.4, 500.0, 1200.0, 0.9, sum_O3_up, true});
      }
    }

    for (auto &s : scenarios) {
      O3impact::O3_impact_in in;
      in.FC = s.FC;
      in.WP = s.WP;
      in.SWC = s.SWC;
      in.ET0 = s.ET0;
      in.O3a = s.O3a;
      in.gs = s.gs;
      in.h = s.h;
      in.reldev = s.reldev;
      in.GDD_flo = s.GDD_flo;
      in.GDD_mat = s.GDD_mat;
      in.fO3s_d_prev = s.fO3s_d_prev;
      in.sum_O3_up = s.sum_O3_up;

      auto out = O3impact::O3_impact_hourly(in, par, s.waterDeficit);
      row({"O3", fmtd(s.FC), fmtd(s.WP), fmtd(s.SWC), fmtd(s.ET0), fmtd(s.O3a), fmtd(s.gs),
           fmti(s.h), fmtd(s.reldev), fmtd(s.GDD_flo), fmtd(s.GDD_mat), fmtd(s.fO3s_d_prev),
           fmtd(s.sum_O3_up), fmti(s.waterDeficit ? 1 : 0), fmtd(out.hourly_O3_up),
           fmtd(out.fO3s_d), fmtd(out.fO3l), fmtd(out.fLS), fmtd(out.WS_st_clos)});
    }
  }

  // --- voc-guenther: vary species/microclimate over a curated set ---
  {
    vector<double> mFols = {0.0, 0.05, 0.3};
    vector<double> slas = {15.0, 25.0};
    vector<double> lais = {0.5, 3.0};
    vector<double> ef_isos = {0.0, 5.0, 20.0};
    vector<double> ef_monos = {0.0, 2.0};
    vector<double> rads = {0.0, 100.0, 600.0};
    vector<double> tFols = {-5.0, 15.0, 35.0};

    for (double mFol : mFols)
      for (double sla : slas)
        for (double lai : lais)
          for (double ef_iso : ef_isos)
            for (double ef_mono : ef_monos)
              for (double rad : rads)
                for (double tFol : tFols) {
                  Voc::SpeciesData sd;
                  sd.id = 1;
                  sd.mFol = mFol;
                  sd.sla = sla;
                  sd.lai = lai;
                  sd.EF_ISO = ef_iso;
                  sd.EF_MONO = ef_mono;
                  sd.EF_MONOS = ef_mono * 0.5;

                  Voc::MicroClimateData mcd;
                  mcd.rad = rad;
                  mcd.tFol = tFol;

                  auto ems = Voc::calculateGuentherVOCEmissions(sd, mcd, 1.0);
                  row({"GUENTHER", fmtd(mFol), fmtd(sla), fmtd(lai), fmtd(ef_iso), fmtd(ef_mono),
                       fmtd(rad), fmtd(tFol), fmtd(ems.isoprene_emission),
                       fmtd(ems.monoterpene_emission)});
                }
  }

  // --- voc-jjv: vary species/cpdata/microclimate over a curated set ---
  {
    vector<double> mFols = {0.0, 0.05, 0.3};
    vector<double> slas = {15.0, 25.0};
    vector<double> tFols = {-5.0, 15.0, 35.0};
    vector<double> rads = {0.0, 100.0, 600.0};
    vector<double> co2s = {350.0, 400.0, 450.0};
    vector<double> jMaxs = {0.0, 50.0, 150.0};
    vector<double> vcMaxs = {0.0, 30.0, 80.0};

    for (double mFol : mFols)
      for (double sla : slas)
        for (double tFol : tFols)
          for (double rad : rads)
            for (double co2 : co2s)
              for (double jMax : jMaxs)
                for (double vcMax : vcMaxs) {
                  Voc::SpeciesData sd;
                  sd.id = 2;
                  sd.mFol = mFol;
                  sd.sla = sla;
                  sd.lai = 2.0;

                  Voc::MicroClimateData mcd;
                  mcd.rad = rad;
                  mcd.rad24 = rad * 0.9;
                  mcd.rad240 = rad * 0.8;
                  mcd.tFol = tFol;
                  mcd.tFol24 = tFol - 1.0;
                  mcd.tFol240 = tFol - 2.0;
                  mcd.sunlitfoliagefraction24 = 0.6;
                  mcd.co2concentration = co2;

                  Voc::CPData cpd;
                  cpd.kc = 260.0;
                  cpd.ko = 179000.0;
                  cpd.oi = 210000.0;
                  cpd.ci = 280.0;
                  cpd.comp = 40.0;
                  cpd.vcMax = vcMax;
                  cpd.jMax = jMax;

                  auto ems = Voc::calculateJJVVOCEmissions(sd, mcd, cpd, 1.0, true);
                  row({"JJV", fmtd(mFol), fmtd(sla), fmtd(tFol), fmtd(rad), fmtd(co2), fmtd(jMax),
                       fmtd(vcMax), fmtd(ems.isoprene_emission), fmtd(ems.monoterpene_emission)});
                }
  }

  return 0;
}
