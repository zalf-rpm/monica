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

//This code is based on kernels/mobile/modules/physiology/vocjjv/vocjjv.h & vocguenther.h & common/math/...
/*!
 * @brief
 *    This gas exchange module calculates only
 *    emission of biogenetic volatile organic compounds.
 *
 *    implemented by: Ruediger Grote (RG), IMK-IFU Garmisch-Partenkirchen,
 *                    ruediger.grote@imk.fzk.de
 *
 * @author
 *    ruediger grote
 */

#ifndef  VOC_COMMON_H_
#define  VOC_COMMON_H_

#include <map>
#include <vector>
#include <cmath>

namespace Voc
{
	template<typename T>
	bool flt_equal_eps(T f1, T f2, T eps = std::numeric_limits<T>::epsilon())
	{
		return ((std::fabs(f1) < eps) && (std::fabs(f2) < eps))
			|| std::fabs(T(f1 - f2)) < eps;
	}

	template<typename T> bool flt_equal_zero(T f, T eps = std::numeric_limits<T>::epsilon())
	{
		return  flt_equal_eps(T(0.0), f, eps);
	}

	template<typename T>
	inline T const& bound_max(T const& val, T const& max)
	{
		return val > max ? max : val;
	}

	template<typename T> inline T sqr(T const& n) { return  n * n; }
	
	//----------------------------------------------------------------------------
	
#define KILO 1.0e+03

	// conv constants
	static const double NMOL_IN_UMOL = KILO; //!< nmol to umol
	static const double UMOL_IN_NMOL = 1.0 / NMOL_IN_UMOL; //!< umol to nmol
	static const double FPAR = 0.45; //!< conversion factor for global into PAR (Monteith 1965, Meek et al. 1984)
	static const double D_IN_K = 273.15; //!< kelvin at zero degree celsius
	static const double G_IN_KG = 1.0e+03; //!< 0.001 kg per g
	static const double UMOL_IN_W = 4.57; //!< conversion factor from Watt in umol PAR (Cox et al. 1998)
	static const double W_IN_UMOL = 1.0 / UMOL_IN_W; //!< conversion factor from umol PAR in Watt (Cox et al. 1998)
	static const double NG_IN_UG = 1.0e+03; //!< conversion factor from nano to micro (gramm)
	static const double UG_IN_NG = 1.0 / NG_IN_UG; //!<

	// phys constants
	static const double RGAS = 8.3143; //!< general gas constant  [J mol-1 K-1]

	// chem constants
	static const double MC = 12.0; //!< molecular weight of carbon  [g mol-1]
	static const double C_ISO = 5.0; //!< number of carbons in Isoprene (C5H8)
	static const double C_MONO = 10.0; //!< number of carbons in Monoterpene (C10H16)

	// time constants
	static const unsigned int SEC_IN_MIN = 60; //<! minute to seconds
	static const unsigned int MIN_IN_HR = 60; //!< hour to minutes
	static const unsigned int HR_IN_DAY = 24; //!< day to hours
	static const unsigned int MONTHS_IN_YEAR = 12; //!< year to months
	static const unsigned int SEC_IN_HR = (SEC_IN_MIN * MIN_IN_HR); //!< hour to seconds
	static const unsigned int MIN_IN_DAY = (MIN_IN_HR * HR_IN_DAY); //!< day to minutes
	static const unsigned int SEC_IN_DAY = (SEC_IN_HR * HR_IN_DAY); //!< day to seconds

	// voc module specific constants

	//! absorbance factor, Collatz et al. 1991 
	static double const ABSO{0.860};
	//! light modifier, Guenther et al. 1993; (Staudt et al. 2004 f. Q.ilex: 0.0041, Bertin et al. 1997 f. Q.ilex: 0.00147, Harley et al. 2004 f. M.indica: 0.00145) 
	static double const ALPHA{0.0027};
	//! monoterpene scaling factor, Guenther et al. 1995 (cit. in Guenther 1999 says this value originates from Guenther 1993) 
	static double const BETA{0.09};
	//!  fraction of electrons used from excess electron transport (−), from Sun et al. 2012 dataset, Grote et al. 2014 
	static double const C1{0.17650};   //fw: 0.17650e-3 in Grote et al. 2014 to preserve the constant relation to C2;
	//! fraction of electrons used from photosynthetic electron transport (−),from Sun et al. 2012 dataset, Grote et al. 2014 
	static double const C2{0.00280};   //fw: 0.00280e-3 (umol m-2 s-1) in Grote et al. 2014; here in nmol m-2 s-1;
	//! emission-class dependent empirical coefficient for temperature acitivity factor of isoprene from MEGAN v2.1 (Guenther et al. 2012)
	static double const CEO_ISO{2.0};
	//! emission-class dependent empirical coefficient for temperature acitivity factor of a-pinene, b-pinene, oMT, Limonen, etc from MEGAN v2.1 (Guenther et al. 2012)
	static double const CEO_MONO{1.83};
	//! first temperature modifier (J mol-1), Guenther et al. 1993; (Staudt et al. 2004 f. Q.ilex: 87620, Bertin et al. 1997 f. Q.ilex: 74050, Harley et al. f. M.indica: 124600, in WIMOVAC 95100) 
	static double const CT1{95000.0};
	//! second temperature modifier (J mol-1), Guenther et al. 1993; (Staudt et al. 2004 f. Q.ilex: 188200, Bertin et al. 1997 f. Q.ilex: 638600, Harley et al. f. M.indica: 254800) 
	static double const CT2{230000.0};
	//! radiation modifier, Guenther et al. 1993; (Staudt et al. 2004 f. Q.ilex: 1.04, Bertin et al. 1997 f. Q.ilex: 1.21, Harley et al. 2004 f. M.indica: 1.218) 
	static double const CL1{1.066};
	//! saturating amount of electrons that can be supplied from other sources (μmol m−2 s−1), Sun et al. 2012 dataset, Grote et al. 2014 (delta J_sat in paper)
	static double const GAMMA_MAX{34.0};
	//! reference photosynthetically active quantum flux density/light density (standard conditions) (umol m-2 s-1), Guenther et al. 1993 
	static double const PPFD0{1000.0};
	//! reference (leaf) temperature (standard conditions) (K), Guenther et al. 1993 
	static double const TEMP0{25.0 + D_IN_K};
	//! temperature with maximum emission (K), Guenther et al. 1993; (Staudt et al. 2004 f. Q.ilex: 317, Bertin et al. 1997 f. Q.ilex: 311.6, Harley et al. f. M.indica: 313.4, in WIMOVAC 311.83) 
	static double const TOPT{314.0};
	//! reference temperature (K), Guenther et al. 1993 
	static double const TREF{30.0 + D_IN_K};
	
	//---------------------------------------------------------------------------

	struct SpeciesData
	{
		int id{0};

		//common
		double EF_MONOS{0.0};
		double EF_MONO{0.0};
		double EF_ISO{0.0};

		//double SCALE_I{1.0};
		//double SCALE_M{1.0};

		// species and canopy layer specific foliage biomass(dry weight).
		// physiology  mFol_vtfl  mFol_vtfl  double  V : F  0.0  kg : m^-2
		double mFol; //!< foliage mass

		//std::map<std::size_t, double> phys_isoAct_vtfl; 
		//std::map<std::size_t, double> phys_monoAct_vtfl;
		
		// species specific leaf area index.
		//physiology  lai_vtfl  lai_vtfl  double  V : F  0.0  m ^ 2 : m^-2
		double lai; //!< LAI

		// specific foliage area (m2 kgDW-1).
		//vegstructure  specific_foliage_area sla_vtfl  double  V : F  0.0  m ^ 2 : g : 10 ^ -3
		double sla; //!< specific leaf area (pc_SpecificLeafArea / cps.cultivarParams.pc_SpecificLeafArea)

		//jjv
		double THETA{0.0};
		double FAGE{0.0};
		double CT_IS{0.0};
		double CT_MT{0.0};
		double HA_IS{0.0};
		double HA_MT{0.0};
		double DS_IS{0.0};
		double DS_MT{0.0};
		double HD_IS{0.0};
		double HD_MT{0.0};

		// leaf internal O2 concentration per canopy layer(umol m - 2)
		//	physiology  oi_vtfl  oi_vtfl  double  V : F  210.0  mol : 10 ^ -6 : m^-2
		double internalO2concentration; //!< leaf internal O2 concentration
		
		// Michaelis - Menten constant for O2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
		// physiology  ko_vtfl  ko_vtfl  double  V : F  0.0  mol : 10 ^ -6 : mol^-1 : bar : 10 ^ -6
		double ko; 

		// actual electron transport capacity per canopy layer(umol m - 2 s - 1)
		// physiology  jMax_vtfl  jMax_vtfl  double  V : F  0.0  mol : 10 ^ -6 : m^-2 : s^-1
		double jMax; 

		// species and layer specific intercellular concentration of CO2 (umol mol-1)
    // physiology  ci_vtfl  ci_vtfl  double  V : F  350.0  mol : 10 ^ -6 : m^-2
		double intercellularCO2concentration; //!< 
		
		// actual activity state of rubisco  per canopy layer (umol m-2 s-1)
		// physiology  vcMax_vtfl  vcMax_vtfl  double  V : F  0.0  mol : 10 ^ -6 : m^-2 : s^-1
		double vcMax;
		
		// CO2 compensation point at 25oC per canopy layer (umol m-2)
		// physiology  comp_vtfl  comp_vtfl  double  V : F  30.0  mol : 10 ^ -6 : m^-2
		double CO2compensationPointAt25DegC;
		
		// Michaelis - Menten constant for CO2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
		// physiology  kc_vtfl  kc_vtfl  double  V : F  0.0  mol : 10 ^ -6 : mol^-1 : bar : 10 ^ -6
		double kc;
	};

	//----------------------------------------------------------------------------

	struct MicroClimateData
	{
		//common
		double rad; //!< radiation 
		double rad24; //!< radiation last 24 hours
		double rad240; //!< radiation last 240 hours
		double tFol; //!< foliage temperature 
		double tFol24; //!< foliage temperature last 24 hours
		double tFol240; //!< foliage temperature last 240 hours

		//jjv
		// fraction of sunlit foliage over the past 24 hours per canopy layer
		// microclimate  sunlitfoliagefraction24_fl  sunlitfoliagefraction24_fl  double  F  0.0 ?
		double sunlitfoliagefraction24;
	};

	//----------------------------------------------------------------------------

	struct Emissions
	{
		std::map<int, double> speciesId_2_isoprene_emission;
		std::map<int, double> speciesId_2_monoterpene_emission;

		double isoprene_emission{0.0}; //!< isoprene emissions per timestep
		double monoterpene_emission{0.0}; //!< monoterpene emissions per timestep
	};

	//----------------------------------------------------------------------------

	struct  photosynth_t
	{
		double par{0.0};     //!< photosynthetic active radiation (umol m-2 s-1)
		double par24{0.0};   //!< 1 day aggregated photosynthetic active radiation (umol m-2 s-1)
		double par240{0.0};  //!< 10 days aggregated photosynthetic active radiation (umol m-2 s-1)
	};

	//----------------------------------------------------------------------------

	struct  foliage_t
	{
		double tempK{0.0}; //!< foliage temperature within a canopy layer (K)
		double tempK24{0.0}; //!< 1 day aggregated foliage temperature within a canopy layer (K)
		double tempK240{0.0}; //!< 10 days aggregated foliage temperature within a canopy layer (K)
	};

	//----------------------------------------------------------------------------

	struct  enzyme_activity_t
	{
		double ef_iso{0.0}; //!< emission factor of isoprene(ug gDW-1 h-1)
		double ef_mono{0.0}; //!< emission factor of monoterpenes (ug gDW-1 h-1)
	};

	//----------------------------------------------------------------------------

	struct  leaf_emission_t
	{
		size_t foliage_layer{0};

		photosynth_t pho;
		foliage_t fol;
		enzyme_activity_t enz_act;
	};

	//----------------------------------------------------------------------------

	struct LeafEmissions
	{
		LeafEmissions() {}
		LeafEmissions(double x, double y) : isoprene(x), monoterp(y) {}

		double isoprene{0.0}; //!< isoprene emission (ug m-2ground h-1)
		double monoterp{0.0}; //!< monoterpene emission (ug m-2ground h-1)
	};

}

#endif

