/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include  "stics-nit-denit-n2o.h"

namespace stics {

double inline stepwiseLinearFunction3(double x,
                                      double xmin,
                                      double xmax,
                                      double ymin = 0.0,
                                      double ymax = 1.0) {
  if (x < xmin) return ymin;
  if (x > xmax) return ymax;
  return ymin + (ymax - ymin) / (xmax - xmin) * (x - xmin);
}

inline double stepwiseLinearFunction4(double x,
                                      double xmin,
                                      double x1,
                                      double x2,
                                      double xmax,
                                      double ymin = 0.0,
                                      double ymax = 1.0) {
  if (x <= xmin) return ymin;
  if (xmin < x && x < x1) return ymin + (ymax - ymin) / (x1 - xmin) * (x - xmin);
  if (x1 <= x && x <= x2) return ymax;
  return ymax - ((ymax - ymin) / (xmax - x2) * (x - x2));
}

namespace nit {

double fNH4(double NH4, double nh4_min, double w, double Kamm) {
  return (NH4 - nh4_min) / ((NH4 - nh4_min) + (w * Kamm));
}

double fpH(double pHminnit, double pH, double pHmaxnit) {
  return stepwiseLinearFunction3(pH, pHminnit, pHmaxnit);
}

double fTgauss(double t, double tnitopt_gauss, double scale_tnitopt) {
  return exp(-1 * (t - (tnitopt_gauss * tnitopt_gauss)) / (scale_tnitopt * scale_tnitopt));
}


double fTstep(double t, double tnitmin, double tnitopt, double tnitopt2, double tnitmax) {
  return stepwiseLinearFunction4(t, tnitmin, tnitopt, tnitopt2, tnitmax);
}

double fWFPS(double wfps, double hminn, double hoptn, double fc, double sat) {
  return stepwiseLinearFunction4(wfps, hminn, hoptn, fc, sat);
}

} // namespace nit

// nitrification
double vnit(const Monica::SticsParameters& ps,
            double NH4,
            double pH,
            double soilT,
            double wfps,
            double soilwaterContent,
            double fc,
            double sat) {
  auto vnitpot = 0.0;
  auto fNH4res = 0.0;
  switch (ps.code_vnit) {
    case 1:
      vnitpot = ps.fnx * (NH4 - ps.nh4_min);
      fNH4res = 1;
      break;
    case 2:
      vnitpot = ps.vnitmax;
      fNH4res = nit::fNH4(NH4, ps.nh4_min, soilwaterContent, ps.Kamm);
      break;
    default:;
  }

  auto fTres = 0;
  switch (ps.code_tnit) {
    case 1: fTres = nit::fTstep(soilT, ps.tnitmin, ps.tnitopt, ps.tnitop2, ps.tnitmax); break;
    case 2: fTres = nit::fTgauss(soilT, ps.tnitopt_gauss, ps.scale_tnitopt); break;
    default:;
  }

  return
    vnitpot
    * fNH4res
    * nit::fpH(ps.pHminnit, pH, ps.pHmaxnit)
    * fTres
    * nit::fWFPS(wfps, ps.hminn, ps.hoptn, fc, sat);
}

namespace denit {

double fNO3(double NO3, double w, double Kd) {
  return NO3 / (NO3 + (w * Kd));
}

double fT(double t, double tdenitopt_gauss, double scale_tdenitopt) {
  auto delta = t - tdenitopt_gauss;
  return exp(-1 * (delta * delta) / (scale_tdenitopt * scale_tdenitopt));
}

// water-filled pore space
double fWFPS(double wfps, double wfpsc) {
  return pow((wfps - wfpsc) / (1 - wfpsc), 1.74);
}

} // namespace denit

// denitrification
double vdenit(const Monica::SticsParameters& ps,
              double corg,
              double NO3,
              double soilT,
              double wfps,
              double soilwaterContent) {
  auto vdenitpot = 0.0;
  switch (ps.code_pdenit) {
    case 1: vdenitpot = ps.vpotdenit; break;
    case 2: vdenitpot = stepwiseLinearFunction3(corg, ps.cmin_pdenit, ps.cmax_pdenit, ps.min_pdenit, ps.max_pdenit); break;
    default:;
  }

  return
    vdenitpot
    * denit::fNO3(NO3, soilwaterContent, ps.Kd)
    * denit::fT(soilT, ps.tdenitopt_gauss, ps.scale_tdenitopt)
    * denit::fWFPS(wfps, ps.wfpsc);
}

namespace N2O {

double fpH(double pH, double pHminden, double pHmaxden) {
  return stepwiseLinearFunction3(pH, pHminden, pHmaxden, 1.0, 0.0);
}

double fWFPS(double WFPS, double wfpsc) {
  return 1 - (WFPS - wfpsc) / (1 - wfpsc);
}

double rcor(double wfpsc, double pH, double pHminden, double pHmaxden) {
  auto rest = fpH(pH, pHminden, pHmaxden);
  return rest / fWFPS(0.815, wfpsc);
}

double fNO3(double NO3) {
  return NO3 / (NO3 + 1);
}

}

double N20(const Monica::SticsParameters& ps,
           double corg,
           double NO3,
           double soilT,
           double wfps,
           double soilwaterContent,
           double NH4,
           double pH,
           double fc,
           double sat) {

  auto z = 0;
  switch (ps.code_rationit) {
    case 1: z = ps.rationit; break;
    case 2: z = 0.16 * (0.4 * wfps - 1.04) / (wfps - 1.04); break;
    default:;
  }

  auto r = 0;
  switch (ps.code_ratiodenit) {
    case 1: r = ps.ratiodenit; break;
    case 2: r =
      N2O::rcor(ps.wfpsc, pH, ps.pHminden, ps.pHmaxden)
      * N2O::fWFPS(wfps, ps.wfpsc)
      * N2O::fNO3(NO3);
      break;
    default:;
  }

  auto N2Onit = z * vnit(ps, NH4, pH, soilT, wfps, soilwaterContent, fc, sat);
  auto N2Odenit = r * vdenit(ps, corg, NO3, soilT, wfps, soilwaterContent);

  return N2Onit + N2Odenit;
}

} // namespace stics
