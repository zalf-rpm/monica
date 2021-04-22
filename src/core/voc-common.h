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
#include <limits>

#include "common/common-typedefs.h"
#include "models/monica/monica_params.capnp.h"

namespace Voc
{
#define KILO 1.0e+03
#define MILLI 1.0e-03

	// conv constants
	static const double NMOL_IN_UMOL = KILO; //!< nmol to umol
	static const double UMOL_IN_NMOL = 1.0 / NMOL_IN_UMOL; //!< umol to nmol
	static const double MOL_IN_MMOL = MILLI;
	static const double MMOL_IN_MOL = 1.0 / MOL_IN_MMOL;
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

	// meteo constants
	static const double PO2 = 0.208; //!< volumentric percentage of oxygen in the canopy air

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
		
	// photofarquhar specific constants
	static double const TK25 = 298.16;

	//---------------------------------------------------------------------------

	//crop photosynthesis result variables
	struct CPData 
	{ 
		void serialize(mas::models::monica::Voc::CPData::Builder builder) const {
			builder.setKc(kc);
			builder.setKo(ko);
			builder.setOi(oi);
			builder.setCi(ci);
			builder.setComp(comp);
			builder.setVcMax(vcMax);
			builder.setJMax(jMax);
			builder.setJj(jj);
			builder.setJj1000(jj1000);
			builder.setJv(jv);
		}
		void deserialize(mas::models::monica::Voc::CPData::Reader reader) {
			kc = reader.getKc();
			ko = reader.getKo();
			oi = reader.getOi();
			ci = reader.getCi();
			comp = reader.getComp();
			vcMax = reader.getVcMax();
			jMax = reader.getJMax();
			jj = reader.getJj();
			jj1000 = reader.getJj1000();
			jv = reader.getJv();
		}

		double kc{0}; //!< Michaelis - Menten constant for CO2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
		double ko{0}; //!< Michaelis - Menten constant for O2 reaction of rubisco per canopy layer(umol mol - 1 ubar - 1)
		double oi{0}; //!< species and layer specific intercellular concentration of CO2 (umol mol-1)
		double ci{0}; //!< leaf internal O2 concentration per canopy layer(umol m - 2)
		double comp{0}; //!< CO2 compensation point at 25oC per canopy layer (umol m-2)
		double vcMax{0}; //!< actual activity state of rubisco  per canopy layer (umol m-2 s-1)
		double jMax{0}; //!<  actual electron transport capacity per canopy layer(umol m - 2 s - 1)
		double jj{0}; //umol m-2 s-1 ... electron provision (unit leaf area)
		double jj1000{0}; //umol m-2 s-1 ... electron provision (unit leaf area) under normalized conditions 
		double jv{0}; //umol m-2 s-1 ... used electron transport for photosynthesis (unit leaf area)
	};

	struct SpeciesData
	{
		void serialize(mas::models::monica::Voc::SpeciesData::Builder builder) const {
			builder.setId(id);
			builder.setEfMonos(EF_MONO);
			builder.setEfMono(EF_MONO);
			builder.setEfIso(EF_ISO);
			builder.setTheta(THETA);
      builder.setFage(FAGE);
      builder.setCtIs(CT_IS);
      builder.setCtMt(CT_MT);
      builder.setHaIs(HA_IS);
      builder.setHaMt(HA_MT);
      builder.setDsIs(DS_IS);
      builder.setDsMt(DS_MT);
      builder.setHdIs(HD_IS);
      builder.setHdMt(HD_MT);
      builder.setHdj(HDJ);
      builder.setSdj(SDJ);
      builder.setKc25(KC25);
      builder.setKo25(KO25);
      builder.setVcMax25(VCMAX25);
      builder.setQjvc(QJVC);
      builder.setAekc(AEKC);
      builder.setAeko(AEKO);
      builder.setAejm(AEJM);
      builder.setAevc(AEVC);
      builder.setSlaMin(SLAMIN);
      builder.setScaleI(SCALE_I);
      builder.setScaleM(SCALE_M);
			builder.setMFol(mFol);
      builder.setLai(lai);
      builder.setSla(sla);
		}
		void deserialize(mas::models::monica::Voc::SpeciesData::Reader reader) {
			id = (int)reader.getId();
			EF_MONO = reader.getEfMonos();
			EF_MONO = reader.getEfMono();
			EF_ISO = reader.getEfIso();
			THETA = reader.getTheta();
			FAGE = reader.getFage();
			CT_IS = reader.getCtIs();
			CT_MT = reader.getCtMt();
			HA_IS = reader.getHaIs();
			HA_MT = reader.getHaMt();
			DS_IS = reader.getDsIs();
			DS_MT = reader.getDsMt();
			HD_IS = reader.getHdIs();
			HD_MT = reader.getHdMt();
			HDJ = reader.getHdj();
			SDJ = reader.getSdj();
			KC25 = reader.getKc25();
			KO25 = reader.getKo25();
			VCMAX25 = reader.getVcMax25();
			QJVC = reader.getQjvc();
			AEKC = reader.getAekc();
			AEKO = reader.getAeko();
			AEJM = reader.getAejm();
			AEVC = reader.getAevc();
			SLAMIN = reader.getSlaMin();
			SCALE_I = reader.getScaleI();
			SCALE_M = reader.getScaleM();
			mFol = reader.getMFol();
			lai = reader.getLai();
			sla = reader.getSla();
		}

		int id{0};

		//common
		double EF_MONOS{0.0}; //!< emission rate of stored terpenes under standard conditions (ug gDW-1 h-1)
		double EF_MONO{0.0}; //!< monoterpene emission rate under standard conditions (ug gDW-1 h-1)
		double EF_ISO{0.0}; //!< isoprene emission rate under standard conditions (ug gDW-1 h-1)

		//jjv
		double THETA{0.9}; //!< curvature parameter
		double FAGE{1.0}; //!< relative decrease of emission synthesis per foliage age class
		double CT_IS{0.0}; //!< scaling constant for temperature sensitivity of isoprene synthase.
		double CT_MT{0.0}; //!< scaling constant for temperature sensitivity

		double HA_IS{0.0}; //!< activation energy for isoprene synthase (J mol-1)
		double HA_MT{0.0}; //!< activation energy for GDP synthase (J mol-1)

		double DS_IS{0.0}; //!< entropy term for isoprene synthase sensitivity to temperature (J:mol-1:K-1)
		double DS_MT{0.0}; //!< entropy term for GDP synthase sensitivity to temperature (J:mol-1:K-1)
		double HD_IS{284600.0}; //!< deactivation energy for isoprene synthase (J mol-1)
		double HD_MT{284600.0}; //!< deactivation energy for monoterpene synthase (J mol-1)

		double HDJ{220000.0}; //! fw: "HDJ": curvature parameter of jMax (J mol-1) (Kattge et al. 2007: 200000; Farquhar et al. 1980: 220000; Harley et al. 1992: 201000) 
		double SDJ{703.0}; //! fw: "SDJ": electron transport temperature response parameter (Kattge et al. 2007: 647; Farquhar et al. 1980: 710; Harley et al. 1992: 650)
		double KC25{260.0}; //!< Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1)
		double KO25{179.0}; //!< Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1)
		double VCMAX25{80.0}; //!< corn: 13.1 | maximum RubP saturated rate of carboxylation at 25oC for sun leaves (umol m-2 s-1)
		double QJVC{2.0}; //!< relation between maximum electron transport rate and RubP saturated rate of carboxylation (--)

		double AEKC{59356}; //!< for corn | activation energy for Michaelis-Menten constant for CO2 (J mol-1)
		double AEKO{35948}; //!< for corn | activation energy for Michaelis-Menten constant for O2 (J mol-1)
		double AEJM{37000}; //!< for corn | activation energy for electron transport (J mol-1) 
		double AEVC{58520}; //!< for corn | activation energy for photosynthesis (J mol-1)
		double SLAMIN{20}; //!< for corn | specific leaf area under full light (m2 kg-1)
		
		double SCALE_I{1.0};
		double SCALE_M{1.0};

		// species and canopy layer specific foliage biomass(dry weight).
		// physiology  mFol_vtfl  mFol_vtfl  double  V : F  0.0  kg : m^-2
		// vc_OrganBiomass[LEAF]
		double mFol{0};

		//std::map<std::size_t, double> phys_isoAct_vtfl; 
		//std::map<std::size_t, double> phys_monoAct_vtfl;
		
		// species specific leaf area index.
		// physiology  lai_vtfl  lai_vtfl  double  V : F  0.0  m ^ 2 : m^-2
		// CropModule::get_LeafAreaIndex()
		double lai{0};

		// specific foliage area (m2 kgDW-1).
		// vegstructure  specific_foliage_area sla_vtfl  double  V : F  0.0  m ^ 2 : g : 10 ^ -3
		// specific leaf area (pc_SpecificLeafArea / cps.cultivarParams.pc_SpecificLeafArea) pc_SpecificLeafArea[vc_DevelopmentalStage]
		double sla{0};
	};

	//----------------------------------------------------------------------------

	struct MicroClimateData
	{
		void serialize(mas::models::monica::Voc::MicroClimateData::Builder builder) const {
			builder.setRad(rad);
			builder.setRad24(rad24);
			builder.setRad240(rad240);
			builder.setTFol(tFol);
			builder.setTFol24(tFol24);
			builder.setTFol240(tFol240);
			builder.setSunlitfoliagefraction(sunlitfoliagefraction);
			builder.setSunlitfoliagefraction24(sunlitfoliagefraction24);
			builder.setCo2concentration(co2concentration);
		}
		void deserialize(mas::models::monica::Voc::MicroClimateData::Reader reader) {
			rad = reader.getRad();
			rad24 = reader.getRad24();
			rad240 = reader.getRad240();
			tFol = reader.getTFol();
			tFol24 = reader.getTFol24();
			tFol240 = reader.getTFol240();
			sunlitfoliagefraction = reader.getSunlitfoliagefraction();
			sunlitfoliagefraction24 = reader.getSunlitfoliagefraction24();
			co2concentration = reader.getCo2concentration();
		}

		//common
		// radiation per canopy layer(W m - 2)
		// microclimate  rad_fl rad_fl  double  F  0.0  W:m^-2
		double rad{0};
		// radiation regime over the last 24hours(W m - 2)
		// microclimate  rad24_fl rad24_fl  double  F  0.0  W:m^-2
		double rad24{0};
		// radiation regime over the last 10days (W m-2)
		// microclimate  rad240_fl rad240_fl  double  F  0.0  W:m^-2
		double rad240{0};
		// foliage temperature per canopy layer(oC)
		// microclimate  tFol_fl tFol_fl  double  F  0.0  oC
		double tFol{0};
		// temperature regime over the last 24hours
		// microclimate  tFol24_fl tFol24_fl  double  F  0.0  oC
		double tFol24{0};
		// temperature regime over the last 10days
		// microclimate  tFol240_fl tFol240_fl  double  F  0.0  oC
		double tFol240{0};

		//jjv
		// fraction of sunlit foliage per canopy layer
		// microclimate  ts_sunlitfoliagefraction_fl ts_sunlitfoliagefraction_fl  double  F  0.0 ?
		double sunlitfoliagefraction{0};
		// fraction of sunlit foliage over the past 24 hours per canopy layer
		// microclimate  sunlitfoliagefraction24_fl  sunlitfoliagefraction24_fl  double  F  0.0 ?
		double sunlitfoliagefraction24{0};

		double co2concentration{0};
	};

	//----------------------------------------------------------------------------

	struct Emissions
	{
		void serialize(mas::models::monica::Voc::Emissions::Builder builder) const {
			{
				auto isos = builder.initSpeciesIdToIsopreneEmission((uint)speciesId_2_isoprene_emission.size());
				uint i = 0;
				for (auto p : speciesId_2_isoprene_emission) {
					isos[i].setSpeciesId(p.first);
					isos[i++].setEmission(p.second);;
				}
			}
			{
				auto monos = builder.initSpeciesIdToMonoterpeneEmission((uint)speciesId_2_monoterpene_emission.size());
				uint i = 0;
				for (auto p : speciesId_2_monoterpene_emission) {
					monos[i].setSpeciesId(p.first);
					monos[i++].setEmission(p.second);;
				}
			}
			builder.setIsopreneEmission(isoprene_emission);
			builder.setMonoterpeneEmission(monoterpene_emission);
		}

		void deserialize(mas::models::monica::Voc::Emissions::Reader reader) {
      speciesId_2_isoprene_emission.clear();
      for (auto e : reader.getSpeciesIdToIsopreneEmission()) speciesId_2_isoprene_emission[(int)e.getSpeciesId()] = e.getEmission();

			speciesId_2_monoterpene_emission.clear();
			for (auto e : reader.getSpeciesIdToMonoterpeneEmission()) speciesId_2_monoterpene_emission[(int)e.getSpeciesId()] = e.getEmission();
			
			isoprene_emission = reader.getIsopreneEmission();
			monoterpene_emission = reader.getMonoterpeneEmission();
		}

		Emissions& operator+=(const Emissions& other)
		{
			for(auto p : other.speciesId_2_isoprene_emission)
				speciesId_2_isoprene_emission[p.first] += p.second;
			for(auto p : other.speciesId_2_monoterpene_emission)
				speciesId_2_monoterpene_emission[p.first] += p.second;
			isoprene_emission += other.isoprene_emission;
			monoterpene_emission += other.monoterpene_emission;
			return *this;
		}

		std::map<int, double> speciesId_2_isoprene_emission; //!< [umol m-2Ground ts-1] isoprene emissions per timestep and plant
		std::map<int, double> speciesId_2_monoterpene_emission; //!< [umol m-2Ground ts-1] monoterpene emissions per timestep and plant

		double isoprene_emission{0.0}; //!< [umol m-2Ground ts-1] isoprene emissions per timestep
		double monoterpene_emission{0.0}; //!< [umol m-2Ground ts-1] monoterpene emissions per timestep
	};

	//----------------------------------------------------------------------------

	struct  photosynth_t
	{
		void serialize(mas::models::monica::Voc::PhotosynthT::Builder builder) const {
			builder.setPar(par);
			builder.setPar24(par24);
			builder.setPar240(par240);
		}
		void deserialize(mas::models::monica::Voc::PhotosynthT::Reader reader) {
			par = reader.getPar();
			par24 = reader.getPar24();
			par240 = reader.getPar240();
		}

		double par{0.0};     //!< photosynthetic active radiation (umol m-2 s-1)
		double par24{0.0};   //!< 1 day aggregated photosynthetic active radiation (umol m-2 s-1)
		double par240{0.0};  //!< 10 days aggregated photosynthetic active radiation (umol m-2 s-1)
	};

	//----------------------------------------------------------------------------

	struct  foliage_t
	{
		void serialize(mas::models::monica::Voc::FoliageT::Builder builder) const {
			builder.setTempK(tempK);
			builder.setTempK24(tempK24);
			builder.setTempK240(tempK240);
		}
		void deserialize(mas::models::monica::Voc::FoliageT::Reader reader) {
			tempK = reader.getTempK();
			tempK24 = reader.getTempK24();
			tempK240 = reader.getTempK240();
		}

		double tempK{0.0}; //!< foliage temperature within a canopy layer (K)
		double tempK24{0.0}; //!< 1 day aggregated foliage temperature within a canopy layer (K)
		double tempK240{0.0}; //!< 10 days aggregated foliage temperature within a canopy layer (K)
	};

	//----------------------------------------------------------------------------

	struct  enzyme_activity_t
	{
		void serialize(mas::models::monica::Voc::EnzymeActivityT::Builder builder) const {
			builder.setEfIso(ef_iso);
			builder.setEfMono(ef_mono);
		}
		void deserialize(mas::models::monica::Voc::EnzymeActivityT::Reader reader) {
			ef_iso = reader.getEfIso();
			ef_mono = reader.getEfMono();
		}

		double ef_iso{0.0}; //!< emission factor of isoprene(ug gDW-1 h-1)
		double ef_mono{0.0}; //!< emission factor of monoterpenes (ug gDW-1 h-1)
	};

	//----------------------------------------------------------------------------

	struct  leaf_emission_t
	{
		void serialize(mas::models::monica::Voc::LeafEmissionT::Builder builder) const {
			builder.setFoliageLayer((uint16_t)foliage_layer);
			pho.serialize(builder.initPho());
			fol.serialize(builder.initFol());
			enz_act.serialize(builder.initEnzAct());
		}
		void deserialize(mas::models::monica::Voc::LeafEmissionT::Reader reader) {
			foliage_layer = reader.getFoliageLayer();
			pho.deserialize(reader.getPho());
			fol.deserialize(reader.getFol());
			enz_act.deserialize(reader.getEnzAct());
		}

		std::size_t foliage_layer{0};

		photosynth_t pho;
		foliage_t fol;
		enzyme_activity_t enz_act;
	};

	//----------------------------------------------------------------------------

	struct LeafEmissions
	{
		LeafEmissions() {}
		LeafEmissions(double x, double y) : isoprene(x), monoterp(y) {}

		void serialize(mas::models::monica::Voc::LeafEmissions::Builder builder) const {
			builder.setIsoprene(isoprene);
			builder.setMonoterp(monoterp);
		}
		void deserialize(mas::models::monica::Voc::LeafEmissions::Reader reader) {
			isoprene = reader.getIsoprene();
			monoterp = reader.getMonoterp();
		}

		double isoprene{0.0}; //!< isoprene emission (ug m-2ground h-1)
		double monoterp{0.0}; //!< monoterpene emission (ug m-2ground h-1)
	};

}

#endif

