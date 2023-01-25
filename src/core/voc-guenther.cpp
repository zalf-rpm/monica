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

//This code is based on kernels/mobile/modules/physiology/vocjjv/vocguenther.cpp
/*
 * @brief
 *  implemented by: Ruediger Grote (RG), IMK-IFU Garmisch-Partenkirchen,
 *      ruediger.grote@imk.fzk.de
 *  last changes:  22.08.2006 (bug fix and optional use of Guenther 1999 and 2006)
 *      22.02.2010 (considering Guenther et al. 1997 modification for Guenther algorithm
 *                 implementing co2 impact according to Wilkinson et al. 2009)
 *      14.05.10 by R.Grote (editing for preparation of 2D transfer)
 *
 * @author
 *  ruediger grote
 */

#include "voc-guenther.h"

#include <iostream>

#include "tools/helper.h"

using namespace Voc;
using namespace Tools;

LeafEmissions calcLeafEmission(const leaf_emission_t& lemi,
                               double _species_EF_MONOS) {
  LeafEmissions lems;
  {
    // isoprene, Guenther et al. 1999 (from Harley et al. 2004)
    double const  x30 = (1.0 / TOPT - 1.0 / (30.0 + D_IN_K)) / RGAS;
    double const  cti30 = CT2 * exp(CT1 * x30) / (CT2 - CT1 * (1.0 - exp(CT2 * x30)));

    double const  eopt = flt_equal_zero(cti30) ? lemi.enz_act.ef_iso : (lemi.enz_act.ef_iso / cti30);
    double const  x = (1.0 / TOPT - 1.0 / lemi.fol.tempK) / RGAS;

    // emission scaling factor of isoprenes to temperature
    double const  cti = CT2 * exp(CT1 * x) / (CT2 - CT1 * (1.0 - exp(CT2 * x)));
    // emission scaling factor to light
    double const  cl = ALPHA * CL1 * lemi.pho.par / sqrt(1.0 + sqr(ALPHA) * sqr(lemi.pho.par));

    //std::cout << "eopt: " << eopt << " cl: " << cl << " cti: " << cti << " = " << (eopt * bound_max(cl, 1.0) * cti) << std::endl;
    lems.isoprene = eopt * bound_max(cl, 1.0) * cti;
  }

  {
    // monoterpene, Guenther et al. (1993, 1995 (ctm), 1997 (factor 0.961, cit. in Lindfors et al. 2000)) 
    double const  ctm = exp(BETA * (lemi.fol.tempK - TREF));
    // ??
    double const  cti = exp(CT1 * (lemi.fol.tempK - TREF) / (RGAS * TREF * lemi.fol.tempK)) /
      (0.961 + exp(CT2 * (lemi.fol.tempK - TOPT) / (RGAS * TREF * lemi.fol.tempK)));
    double const  cl = ALPHA * CL1 * lemi.pho.par / sqrt(1.0 + sqr(ALPHA) * sqr(lemi.pho.par));

    lems.monoterp = _species_EF_MONOS * ctm + lemi.enz_act.ef_mono * bound_max(cl, 1.0) * cti;
  }

  return lems;
}

Voc::Emissions
Voc::calculateGuentherVOCEmissionsMultipleSpecies(std::vector<SpeciesData> sds,
                                                  const MicroClimateData& mcd,
                                                  double dayFraction) {
  Emissions ems;

  double const tslength = SEC_IN_DAY * dayFraction;

  for(const SpeciesData& species : sds) {
    if(species.mFol > 0.0) {
      leaf_emission_t lemi;

      // conversion of enzyme activity (umol m-2 s-1) in emission factor (ugC g-1 h-1)
      // specific leaf weight (g m-2)
      double const lsw = G_IN_KG / species.sla;
      static double const  C0 = SEC_IN_HR * MC * UG_IN_NG;
      lemi.enz_act.ef_iso = species.EF_ISO; //5.0 * C0 * species.phys_isoAct_vtfl.at(fl) / (lsw * species.SCALE_I);
      lemi.enz_act.ef_mono = species.EF_MONO; //10.0 * C0 * species.phys_monoAct_vtfl.at(fl) / (lsw * species.SCALE_M);

      // conversion of microclimate variables
      lemi.pho.par = mcd.rad * FPAR * W_IN_UMOL; // par [umol m-2 s-1 pa-radiation] = rad_fl [W m-2 global radiation] * 0.45 * 4.57
      //lemi.pho.par24 = mcd.rad24 * FPAR * W_IN_UMOL;
      //lemi.pho.par240 = mcd.rad240 * FPAR * W_IN_UMOL;
      lemi.fol.tempK = mcd.tFol + D_IN_K;
      //lemi.fol.tempK24 = mcd.tFol24 + D_IN_K;
      //lemi.fol.tempK240 = mcd.tFol240 + D_IN_K;

      // emission in dependence on light and temperature, weighted over canopy layers
      auto lems = calcLeafEmission(lemi, species.EF_MONOS);

      // conversion from (ugC g-1 h-1) to (umol m-2 s-1) and weighting with leaf area and time
      double const  C1 = (lsw / (SEC_IN_HR * MC)) * species.lai * tslength;
      double ts_isoprene_em = (1.0 / C_ISO) * C1 * lems.isoprene;
      double ts_monoterpene_em = (1.0 / C_MONO) * C1 * lems.monoterp;
      //std::cout << C1 << " " << lems.isoprene << " " << ts_isoprene_em << std::endl;

      // works only with 24 hour time step???        ph_.cUpt_vtfl[vt][fl] -= ((ph_.ts_isoprene_emission_vtfl[vt][fl] * 5.0 + ph_.ts_monoterpene_emission_vtfl[vt][fl] * 10.0) * MC / (UMOL_IN_MOL * G_IN_KG));  // rg 18.06.10

      ems.speciesId_2_isoprene_emission[species.id] = ts_isoprene_em;
      ems.isoprene_emission += ts_isoprene_em;
      ems.speciesId_2_monoterpene_emission[species.id] = ts_monoterpene_em;
      ems.monoterpene_emission += ts_monoterpene_em;
    } else {
      ems.speciesId_2_isoprene_emission[species.id] = 0.0;
      ems.speciesId_2_monoterpene_emission[species.id] = 0.0;
    }
  }

  return ems;
}



