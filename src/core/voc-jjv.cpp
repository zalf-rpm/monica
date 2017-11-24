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

//This code is based on kernels/mobile/modules/physiology/vocjjv/vocjjv.cpp
/*!
 * @brief
 *
 *    This gas exchange module calculates only
 *    emission of biogenic volatile organic compounds.
 *    cf. Grote et al., 2014
 *
 *    implemented by: Felix Wiß (fw), IMK-IFU Garmisch-Partenkirchen, felix.wiss@kit.edu
 *                    edited from the original mobile2d vocjjv model implemented by
 *                    Ruediger Grote (rg), IMK-IFU Garmisch-Partenkirchen, ruediger.grote@kit.edu
 *
 * @date
 *    20.02.2015
 *
 * @author
 *    ruediger grote (rg)
 *    felix wiss (fw)
 */

#include <algorithm>
#include <cassert>

#include  "voc-jjv.h"

using namespace Voc;

Voc::Emissions Voc::calculateJJVVOCEmissionsMultipleSpecies(std::vector<std::pair<SpeciesData, std::map<CPData, double>>> sds,
																														const MicroClimateData& mcd,
																														double dayFraction,
																														bool calculateParTempTerm)
{
	Emissions ems;

	double const tslength = SEC_IN_DAY * dayFraction;

	for(const auto& p : sds)
	{
		const SpeciesData& species = p.first;
		const auto& cpData = p.second;

		if(species.mFol > 0.0)
		{
			leaf_emission_t lemi;
			leaf_emission_t leminorm;

			// factors for conversion from enzyme activity (umol m-2 (leaf area) s-1) to emission factor (ugC g-1 h-1)
			double const lsw = G_IN_KG / species.sla;
			double const C0 = SEC_IN_HR * MC * UMOL_IN_NMOL;

			// "isoAct_vtfl"/"monoAct_vtfl" [nmol m-2 leaf area s-1] activity state of isoprene/monterpene synthase
			// "enz_act.ef_iso/mono" --> emission factor (including seasonality!!!; similar to EF_ISO() and EF_MONO() but they provide no info about seasonality) 
			lemi.enz_act.ef_iso = species.EF_ISO; // lconst::C_ISO * C0 * species.phys_isoAct_vtfl.at(fl) / (lsw * species.SCALE_I);       // (ugC gDW-1 h-1)
			lemi.enz_act.ef_mono = species.EF_MONO; // lconst::C_MONO * C0 * species.phys_monoAct_vtfl.at(fl) / (lsw * species.SCALE_M);    // (ugC gDW-1 h-1)

			// conversion of microclimate variables 
			lemi.pho.par = mcd.rad * FPAR * UMOL_IN_W;      // fw: par (umol m-2 s-1 pa-radiation)] = rad_fl (W m-2 global radiation) * 0.45 * 4.57 ..
			lemi.pho.par24 = mcd.rad24 * FPAR * UMOL_IN_W;
			lemi.pho.par240 = mcd.rad240 * FPAR * UMOL_IN_W;
			lemi.fol.tempK = mcd.tFol + D_IN_K;
			lemi.fol.tempK24 = mcd.tFol24 + D_IN_K;
			lemi.fol.tempK240 = mcd.tFol240 + D_IN_K;

			// normalized microclimate variables 
			leminorm.pho.par = PPFD0;
			leminorm.fol.tempK = TREF;

			// emission in dependence on light and temperature for photosynthesis and enzyme activity, weighted over canopy layers 
			auto lems = calcLeafEmission(lemi, leminorm, species, mcd, cpData);

			// conversion from (ugC g-1 h-1) to (umol m-2 ground s-1) and weighting with leaf area and time step length in seconds
			//(reciprocal to the input conversion) 
			// TODO(fw#): check if area correction is for m-2 ground or m-2 lai
			double const C = (lsw / (SEC_IN_HR * MC)) * species.lai * tslength;

			// TODO(fw#): check if area correction is for m-2 ground or m-2 lai
			// fw: "isopr/ts_monoterpene_emission_vtfl": species and layer specific isoprene/monterpene emission (umol m-2Ground ts-1). 
			double ts_isoprene_em = (1.0 / C_ISO)  * C * lems.isoprene;
			double ts_monoterpene_em = (1.0 / C_MONO) * C * lems.monoterp;

			//TODO(fw#): implement this!!!
			//ph_.ts_carbonuptake_vtfl[vt][fl] -= ((ph_.ts_isoprene_emission_vtfl[vt][fl] * C_ISO + ph_.ts_monoterpene_emission_vtfl[vt][fl] * C_MONO) * MC / (UMOL_IN_MOL * G_IN_KG));  // rg 18.06.10;

			// "ts_isoprene_emission/ts_monoterpene_emission": isoprene/monoterpene emission from the whole canopy and all species (umol m-2 ground). 
			ems.speciesId_2_isoprene_emission[species.id] = ts_isoprene_em;
			ems.isoprene_emission += ts_isoprene_em;
			ems.speciesId_2_monoterpene_emission[species.id] = ts_monoterpene_em;
			ems.monoterpene_emission += ts_monoterpene_em;
		}
		else
		{
			ems.speciesId_2_isoprene_emission[species.id] = 0.0;
			ems.speciesId_2_monoterpene_emission[species.id] = 0.0;
		}
	}
	return ems;
}


LeafEmissions Voc::calcLeafEmission(const leaf_emission_t& lemi,
																		const leaf_emission_t& leminorm,
																		const SpeciesData& species,
																		const MicroClimateData& mcd,
																		const std::map<CPData, double>& cpData,
																		bool calculateParTempTerm)
{
	LeafEmissions lems;

	//CALCULATE BVOC EMISSION POTENTIALS WITH RESPECT TO PHOTOSYNTHESIS AND ENZYMATIC ACTIVITY 
	// activity factor for Leaf Age (rg: common to both MEGAN and JJV)
	double gamma_a = species.FAGE;    // older version: double const  gamma_a     = 1.0;

	// activity factor for Temperature (NEW: only emission from storages = light dependend fraction LDF is 0) (= Light independent fraction (of exp(betaT(nclass_in)) form))
	double gamma_t = exp(BETA * (lemi.fol.tempK - leminorm.fol.tempK)); // fw: = 1.0 for normalized conditions, gamma_t=[15;0.6] für tempK=[0°C;35°C]

	// Emission potential from photosynthesis (energy supply); same for isoprene and monoterpene; actual and normalized 
	// activity factor for photosynthesis (energy supply)
	double gamma_ph = gamma_PH(lemi, species, mcd, cpData);

	//!< normalized activity factor for photosynthesis (energy supply)
	double gamma_phnorm = gamma_PH(leminorm, species, mcd, cpData);

	double gamma_phrel = gamma_phnorm > 0.0 ? gamma_ph / gamma_phnorm : 0.0;

	//Emission potential from enzymatic activity of isoprene and monoterpene synthase; actual and normalized 
	auto gamma_en = gamma_EN(lemi, leminorm, species);

	// Calculate total scaling factor for sun leaves
	// relative emission response/total scaling factor (-)
	//const double gamma_rer = gamma_a * gamma_phrel * gamma_enrel;

	// Calculate total scaling factor for sun leaves for isoprene and monoterpene 
	// relative emission response/total scaling factor for isoprene (-)
	double gamma_iso = gamma_en.ennorm_iso > 0.0 ? gamma_a * gamma_phrel * (gamma_en.en_iso / gamma_en.ennorm_iso) : 0.0;

	// relative emission response/total scaling factor for monoterpene (-)
	double gamma_mono = gamma_en.ennorm_mono > 0.0 ? gamma_a * gamma_phrel * (gamma_en.en_mono / gamma_en.ennorm_mono) : 0.0;

	// PAST TEMPERATURE AND RADIATION DEPENDENCE TERMS FROM VOCMEGAN.CPP MODULE (Guenther et al. 2006, 2012)
	double EOPT_ISO = 1.0;
	double EOPT_MONO = 1.0;
	double PAR0 = 1.0;
	double C_P = 1.0;
	if(calculateParTempTerm)
	{
		//Factor for temperature dependence of past days (light dependent factors, LDF), from MEGAN (Guenther et al. 2006, 2012)
		//Light independent factors (LIF) for isoprene = 0; for monoterpenes = gamma.t (emissions from storage)
		EOPT_ISO = CEO_ISO * exp(0.05 * ((lemi.fol.tempK24 - 297.0) + (lemi.fol.tempK240 - 297.0)));
		EOPT_MONO = CEO_MONO * exp(0.05 * ((lemi.fol.tempK24 - 297.0) + (lemi.fol.tempK240 - 297.0)));
		//old: f_term  = 2.034 * exp( 0.05 * ((lemi.fol.tempK24 - 297.0) + (lemi.fol.tempK240 - 297.0)));

		//Factor for PPFD dependence of past days (light dependent factors, LDF), from MEGAN (Guenther et al. 2006, 2012)
		//Light independent factors (LIF) for isoprene = 0; for monoterpenes = 0.4 to 0.8
		//We calculate explicitly LIF emission for MT from storage -->  no LIF/LDF coefficients needed
		//PAR0 = 200.0 * mcd.ts_sunlitfoliagefraction_fl.at(_fl) + 50.0 * (1.0 - mcd.ts_sunlitfoliagefraction_fl.at(_fl));
		PAR0 = 200.0 * mcd.sunlitfoliagefraction24 + 50.0 * (1.0 - mcd.sunlitfoliagefraction24);
		C_P = 0.0468 * exp(0.0005 * (lemi.pho.par24 - PAR0)) * pow(lemi.pho.par240, 0.6);
	}

	// fw: "enz_act.ef_iso/mono" (from iso/monoAct_vtfl from psim): isoprene/monoterpene emission factor/rate 
	lems.isoprene = lemi.enz_act.ef_iso * gamma_iso * EOPT_ISO * C_P;
	lems.monoterp = (lemi.enz_act.ef_mono * gamma_mono * EOPT_MONO * C_P) + species.EF_MONOS * gamma_t;
	// without link to psim or with psim running with CalcActivity == false:
	//lemi.enz_act.ef_iso = species.EF_ISO; lemi.enz_act.ef_mono = species.EF_MONO; 

	return lems;
}

double Voc::gamma_PH(const leaf_emission_t& lemi,
										 SpeciesData species,
										 const MicroClimateData& mcd,
										 std::map<CPData, double> cpData)
{
	double tempK = lemi.fol.tempK;

	// Emission calculation as described in Grote et al. (2014);
	//
	//NOTE:   Limitations of photosynthetic performance due to drought(currently outcommented),
	//nitrogen, and phenology (seasonality) are generally considered in the growthpsim
	//(VCMAX25, QJVC -> vcAct25, jAct25) while further dependenceies on shading
	//(vcAct25, jact25 -> vcMax25, jMax25) and temperature (vcMax25, jMax25 -> vcMax, jMax)
	//are accounted for in the farquhar models) modules before putting them into this routine
	//(vcMax, jMax -> vcmax_in, jmax_in).
	//There is an option to set these variables here for testing purpose only.

	//const size_t fl = lemi.foliage_layer;
	double parabs = lemi.pho.par * ABSO;
	// fw: TODO: implement absorbed radiation (parshd_fl, parsun_fl) together with fraction of sunlit foliage (ts_sunlitfoliagefraction_fl, 1 - ts_sunlitfoliagefraction_fl)

	// Michaelis - Menten constant for CO2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
	// physiology  kc_vtfl  kc_vtfl  double  V : F  0.0  mol : 10 ^ -6 : mol^-1 : bar : 10 ^ -6
	double kc = cpData[KC]; // 0;
	// Michaelis - Menten constant for O2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
	// physiology  ko_vtfl  ko_vtfl  double  V : F  0.0  mol : 10 ^ -6 : mol^-1 : bar : 10 ^ -6
	double ko = cpData[KO]; // 0;
	// species and layer specific intercellular concentration of CO2 (umol mol-1)
	// physiology  ci_vtfl  ci_vtfl  double  V : F  350.0  mol : 10 ^ -6 : m^-2
	double ci = cpData[CI]; // 0;
	// leaf internal O2 concentration per canopy layer(umol m - 2)
	//	physiology  oi_vtfl  oi_vtfl  double  V : F  210.0  mol : 10 ^ -6 : m^-2
	double oi = cpData[OI]; // 0;
	// CO2 compensation point at 25oC per canopy layer (umol m-2)
	// physiology  comp_vtfl  comp_vtfl  double  V : F  30.0  mol : 10 ^ -6 : m^-2
	double comp = cpData[COMP]; // 0;
	// actual activity state of rubisco  per canopy layer (umol m-2 s-1)
	// physiology  vcMax_vtfl  vcMax_vtfl  double  V : F  0.0  mol : 10 ^ -6 : m^-2 : s^-1
	double vcMax = cpData[VCMAX]; // 0;
	// actual electron transport capacity per canopy layer(umol m - 2 s - 1)
	// physiology  jMax_vtfl  jMax_vtfl  double  V : F  0.0  mol : 10 ^ -6 : m^-2 : s^-1
	double jMax = cpData[JMAX]; // 0;
	/*
	for(auto frad : {mcd.sunlitfoliagefraction, 1 - mcd.sunlitfoliagefraction})
	{
//		double oi_ = PO2 * MMOL_IN_MOL;
//		if(mcd.tFol > 25.0)
//			oi_ *= (0.047 - 0.001308*mcd.tFol + 0.000025603*sqr(mcd.tFol) - 0.00000021441*pow(mcd.tFol, 3.0)) / 0.026934;
//		oi += frad * oi_;

		// For testing: photosynthesis variables without spatial or seasonal differentiation (according Collatz et al. 1991) 
		// temperature modification term; efficiency reduction due to temperature
//		double ft_term = 1.0 / (1.0 + exp((-species.HDJ + species.SDJ * lemi.fol.tempK) / (RGAS * lemi.fol.tempK)));

		// calc kc
		//--------
		// fw
		// fw: "KC25": Michaelis-Menten constant for CO2 at 25 °C (umol mol-1 / ubar)
		//kc += species.KC25 * pow(2.1, (tempK - TEMP0) * 0.1);

		// jarvis.cpp:194
		double term1 = (tempK - TK25) / (TK25 * tempK * RGAS);
		double term2 = sqrt(tempK / TK25);
		// berryball.cpp:155
//		double term_arrh = term1;
		//double term2 = 1; 

//		kc += frad * species.KC25 * exp(species.AEKC * term1) * term2;

		// calc ko
		//--------
		// fw: "KO25": Michaelis-Menten constant for O2 at 25oC (mmol mol-1 / mbar)
		//ko += species.KO25 * pow(1.2, (tempK - TEMP0) * 0.1);

//		ko += frad * species.KO25 * exp(species.AEKO * term1) * term2;

		// calc comp
		//----------
		//"oi_vtfl": leaf internal O2 concentration per canopy layer(umol m - 2)
		//comp += 0.5 * species.kc * species.oi * 0.21 / species.ko; // 

		//berryball.cpp:216
//		static double const AEC = 24600.0;  // activation energy for compensation point
//		static double const C25 = 36.9;     // compensation point value at 25 oC
//		comp += frad * C25 * exp(AEC * term_arrh);
		
//		ci += frad * 0.7 * 370.0;

		//calc vcMax
		//----------
		//fw: "VCMAX25": maximum RubP saturated rate of carboxylation at 25oC for sun leaves (umol m-2 s-1)
		//vcMax += frad * species.VCMAX25 * ft_term * pow(2.4, (lemi.fol.tempK - TEMP0) * 0.1);
		
		//berryball.cpp:161/jarvis.cpp:200
//		vcMax += frad * species.VCMAX25 * exp(species.AEVC * term1) * term2;

		// calc jMax
		//----------
		// fw: "AEJM": activation energy for electron transport (J mol-1); "QJVC": relation between maximum electron transport rate and RubP saturated rate of carboxylation (--)
		//jMax += frad * species.VCMAX25 * species.QJVC * ft_term * exp(species.AEJM * (lemi.fol.tempK - TEMP0) / (RGAS * TEMP0 * lemi.fol.tempK));

		double slaScale = species.SLAMIN / species.sla;
		//simplified from growthpsim.cpp:1909, instead of vcAct25_vtfl from farquhardndc.cpp:825
		double jAct25 = species.VCMAX25 * species.QJVC;
		double jMax25 = jAct25 * slaScale;

		// berryball.cpp:181
		//double const term_peak = 
		//	(1.0 + exp((273.15 * species.SDJ - species.HDJ) / (273.15 * RGAS)))
		//	/ (1.0 + exp((species.SDJ * tempK - species.HDJ) / (RGAS * tempK)));
		//jMax += frad * jMax25 * exp(species.AEJM * term_arrh) * term_peak;
		//this can be ignored, because VCMAX25 should vcAct25, which we don't have
		//if(jAct25 > 0)
		//	jMax *= species.VCMAX25 * species.QJVC / jAct25;

		// jarvis.cpp:204
		jMax += frad * jMax25 * exp(species.AEJM * term1) * term2;
	}
	*/

	// electron transport rate and electron usage 
	// fw: "km": michaelis-menten coefficient for electron transport capacity
	double km = ko > 0.0 ? kc * (1.0 + oi / ko) : 0.0;

	// fw: "jj": electron provision (umol m-2 s-1) / electron transport rate
	const double tmp_var = ((parabs + jMax) * (parabs + jMax)) - (4.0 * species.THETA * parabs * jMax);
	// fw: In Grote et al. 2014 tmp_var is stated as the inverse sqrt even though it is only the sqrt 
	double  jj = tmp_var > 0.0 ? (parabs + jMax - sqrt(tmp_var)) / (2.0 * species.THETA) : 0.0;

	// fw: "jv": used electron transport for photosynthesis (C assimilation) (umol m-2 s-1) / fraction of J used for photosynthesis / electron flux required to support Rubisco-limited carbon assimilation 
	// fw: "comp_vtfl" : CO2 compensation point(umol mol - 1)
	double  jv = ci + km > 0.0 ? 4.0 * vcMax * (ci + 2.0 * comp) / (ci + km)
		: 0.0;

	// bvoc emission potential from photosynthesis (excess energy after carbon assimilation)
	// _gamma->ph = (( C1 + C2 * std::max( -GAMMA_MAX, jj - jv)) * jj * std::min( 1.0, this->phys->ci_vtfl[vt][fl] / this->phys->comp_vtfl[vt][fl])); // Wie dann mit normalized Bedingungen umgehen???
	return comp > 0.0
		? ((C1 + C2 * std::max(-GAMMA_MAX, jj - jv)) * jj * std::min(1.0, ci / comp))
		: 0.0;
}

GammaEnRes Voc::gamma_EN(const leaf_emission_t& lemi,
												 const leaf_emission_t& leminorm,
												 const SpeciesData& species)
{
	GammaEnRes res;

	// Emission calculation as described in Grote et al. (2014); enzymatic activity of isoprene and monoterpene synthase 

	// T in (K) should actually never be below zero
	assert(lemi.fol.tempK > 0.0);

	// Calculate actual bvoc emission potential from enzyme activity 
	res.en_iso = exp(species.CT_IS - species.HA_IS / (RGAS * lemi.fol.tempK))
		/ (1.0 + exp((species.DS_IS * lemi.fol.tempK - species.HD_IS) / (RGAS * lemi.fol.tempK)));
	res.en_mono = exp(species.CT_MT - species.HA_MT / (RGAS * lemi.fol.tempK))
		/ (1.0 + exp((species.DS_MT * lemi.fol.tempK - species.HD_MT) / (RGAS * lemi.fol.tempK)));

	// Calculate normalized bvoc emission potential from enzyme activity 
	res.ennorm_iso = exp(species.CT_IS - species.HA_IS / (RGAS * leminorm.fol.tempK))
		/ (1.0 + exp((species.DS_IS * leminorm.fol.tempK - species.HD_IS) / (RGAS * leminorm.fol.tempK)));
	res.ennorm_mono = exp(species.CT_MT - species.HA_MT / (RGAS * leminorm.fol.tempK))
		/ (1.0 + exp((species.DS_MT * leminorm.fol.tempK - species.HD_MT) / (RGAS * leminorm.fol.tempK)));

	return res;
}


