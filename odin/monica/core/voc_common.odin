// Port of src/core/voc-common.h: shared constants and data structs for the
// voc-guenther/voc-jjv biogenic VOC emission modules. Header-only in C++ (no
// voc-common.cpp) - pure data, serialize/deserialize dropped per the capnp
// convention.
package core

// conv constants
VOC_NMOL_IN_UMOL: f64 : 1.0e+03 // nmol to umol
VOC_UMOL_IN_NMOL: f64 : 1.0 / VOC_NMOL_IN_UMOL // umol to nmol
VOC_MOL_IN_MMOL: f64 : 1.0e-03
VOC_MMOL_IN_MOL: f64 : 1.0 / VOC_MOL_IN_MMOL
VOC_FPAR: f64 : 0.45 // conversion factor for global into PAR (Monteith 1965, Meek et al. 1984)
VOC_D_IN_K: f64 : 273.15 // kelvin at zero degree celsius
VOC_G_IN_KG: f64 : 1.0e+03 // 0.001 kg per g
VOC_UMOL_IN_W: f64 : 4.57 // conversion factor from Watt in umol PAR (Cox et al. 1998)
VOC_W_IN_UMOL: f64 : 1.0 / VOC_UMOL_IN_W // conversion factor from umol PAR in Watt (Cox et al. 1998)
VOC_NG_IN_UG: f64 : 1.0e+03 // conversion factor from nano to micro (gramm)
VOC_UG_IN_NG: f64 : 1.0 / VOC_NG_IN_UG

// phys constants
VOC_RGAS: f64 : 8.3143 // general gas constant [J mol-1 K-1]

// chem constants
VOC_MC: f64 : 12.0 // molecular weight of carbon [g mol-1]
VOC_C_ISO: f64 : 5.0 // number of carbons in Isoprene (C5H8)
VOC_C_MONO: f64 : 10.0 // number of carbons in Monoterpene (C10H16)

// time constants
VOC_SEC_IN_MIN: int : 60
VOC_MIN_IN_HR: int : 60
VOC_HR_IN_DAY: int : 24
VOC_MONTHS_IN_YEAR: int : 12
VOC_SEC_IN_HR: int : VOC_SEC_IN_MIN * VOC_MIN_IN_HR
VOC_MIN_IN_DAY: int : VOC_MIN_IN_HR * VOC_HR_IN_DAY
VOC_SEC_IN_DAY: int : VOC_SEC_IN_HR * VOC_HR_IN_DAY

// meteo constants
VOC_PO2: f64 : 0.208 // volumetric percentage of oxygen in the canopy air

// voc module specific constants
VOC_ABSO: f64 : 0.860 // absorbance factor, Collatz et al. 1991
// light modifier, Guenther et al. 1993
VOC_ALPHA: f64 : 0.0027
// monoterpene scaling factor, Guenther et al. 1995
VOC_BETA: f64 : 0.09
// fraction of electrons used from excess electron transport (-), Sun et al. 2012 / Grote et al. 2014
VOC_C1: f64 : 0.17650
// fraction of electrons used from photosynthetic electron transport (-), Sun et al. 2012 / Grote et al. 2014
VOC_C2: f64 : 0.00280
// emission-class dependent empirical coefficient for temperature activity factor of isoprene, MEGAN v2.1
VOC_CEO_ISO: f64 : 2.0
// emission-class dependent empirical coefficient for temperature activity factor of a-pinene etc, MEGAN v2.1
VOC_CEO_MONO: f64 : 1.83
// first temperature modifier (J mol-1), Guenther et al. 1993
VOC_CT1: f64 : 95000.0
// second temperature modifier (J mol-1), Guenther et al. 1993
VOC_CT2: f64 : 230000.0
// radiation modifier, Guenther et al. 1993
VOC_CL1: f64 : 1.066
// saturating amount of electrons that can be supplied from other sources (umol m-2 s-1), Grote et al. 2014
VOC_GAMMA_MAX: f64 : 34.0
// reference photosynthetically active quantum flux density (umol m-2 s-1), Guenther et al. 1993
VOC_PPFD0: f64 : 1000.0
// reference (leaf) temperature (K), Guenther et al. 1993
VOC_TEMP0: f64 : 25.0 + VOC_D_IN_K
// temperature with maximum emission (K), Guenther et al. 1993
VOC_TOPT: f64 : 314.0
// reference temperature (K), Guenther et al. 1993
VOC_TREF: f64 : 30.0 + VOC_D_IN_K

// photofarquhar specific constants
VOC_TK25: f64 : 298.16

// C++: struct Voc::CPData - crop photosynthesis result variables
Voc_Cp_Data :: struct {
	kc:     f64, // Michaelis-Menten const for CO2 reaction of rubisco per canopy layer (umol mol-1 ubar-1)
	ko:     f64, // Michaelis-Menten const for O2 reaction of rubisco per canopy layer (umol mol-1 ubar-1)
	oi:     f64, // species and layer specific intercellular concentration of CO2 (umol mol-1)
	ci:     f64, // leaf internal O2 concentration per canopy layer (umol m-2)
	comp:   f64, // CO2 compensation point at 25oC per canopy layer (umol m-2)
	vcMax:  f64, // actual activity state of rubisco per canopy layer (umol m-2 s-1)
	jMax:   f64, // actual electron transport capacity per canopy layer (umol m-2 s-1)
	jj:     f64, // electron provision (unit leaf area), umol m-2 s-1
	jj1000: f64, // electron provision (unit leaf area) under normalized conditions, umol m-2 s-1
	jv:     f64, // used electron transport for photosynthesis (unit leaf area), umol m-2 s-1
}

// C++: struct Voc::SpeciesData
Voc_Species_Data :: struct {
	id: int,

	// common
	EF_MONOS: f64, // emission rate of stored terpenes under standard conditions (ug gDW-1 h-1)
	EF_MONO:  f64, // monoterpene emission rate under standard conditions (ug gDW-1 h-1)
	EF_ISO:   f64, // isoprene emission rate under standard conditions (ug gDW-1 h-1)

	// jjv
	THETA: f64, // curvature parameter
	FAGE:  f64, // relative decrease of emission synthesis per foliage age class
	CT_IS: f64, // scaling constant for temperature sensitivity of isoprene synthase
	CT_MT: f64, // scaling constant for temperature sensitivity

	HA_IS: f64, // activation energy for isoprene synthase (J mol-1)
	HA_MT: f64, // activation energy for GDP synthase (J mol-1)

	DS_IS: f64, // entropy term for isoprene synthase sensitivity to temperature (J:mol-1:K-1)
	DS_MT: f64, // entropy term for GDP synthase sensitivity to temperature (J:mol-1:K-1)
	HD_IS: f64, // deactivation energy for isoprene synthase (J mol-1)
	HD_MT: f64, // deactivation energy for monoterpene synthase (J mol-1)

	HDJ: f64, // curvature parameter of jMax (J mol-1)
	SDJ: f64, // electron transport temperature response parameter

	KC25:   f64, // Michaelis-Menten constant for CO2 at 25oC (umol mol-1 ubar-1)
	KO25:   f64, // Michaelis-Menten constant for O2 at 25oC (mmol mol-1 mbar-1)
	VCMAX25: f64, // maximum RubP saturated rate of carboxylation at 25oC for sun leaves (umol m-2 s-1)
	QJVC:   f64, // relation between maximum electron transport rate and RubP saturated rate of carboxylation

	AEKC:   f64, // activation energy for Michaelis-Menten constant for CO2 (J mol-1)
	AEKO:   f64, // activation energy for Michaelis-Menten constant for O2 (J mol-1)
	AEJM:   f64, // activation energy for electron transport (J mol-1)
	AEVC:   f64, // activation energy for photosynthesis (J mol-1)
	SLAMIN: f64, // specific leaf area under full light (m2 kg-1)

	SCALE_I: f64,
	SCALE_M: f64,

	// species and canopy layer specific foliage biomass (dry weight)
	mFol: f64,

	// species specific leaf area index
	lai: f64,

	// specific foliage area (m2 kgDW-1)
	sla: f64,
}

// C++ in-class initialisers: THETA{0.9}, FAGE{1.0}, HD_IS{284600.0},
// HD_MT{284600.0}, HDJ{220000.0}, SDJ{703.0}, KC25{260.0}, KO25{179.0},
// VCMAX25{80.0}, QJVC{2.0}, AEKC{59356}, AEKO{35948}, AEJM{37000},
// AEVC{58520}, SLAMIN{20}, SCALE_I{1.0}, SCALE_M{1.0}. Everything else is
// 0, matching Odin's zero value.
make_voc_species_data :: proc() -> Voc_Species_Data {
	return Voc_Species_Data {
		THETA   = 0.9,
		FAGE    = 1.0,
		HD_IS   = 284600.0,
		HD_MT   = 284600.0,
		HDJ     = 220000.0,
		SDJ     = 703.0,
		KC25    = 260.0,
		KO25    = 179.0,
		VCMAX25 = 80.0,
		QJVC    = 2.0,
		AEKC    = 59356,
		AEKO    = 35948,
		AEJM    = 37000,
		AEVC    = 58520,
		SLAMIN  = 20,
		SCALE_I = 1.0,
		SCALE_M = 1.0,
	}
}

// C++: struct Voc::MicroClimateData
Voc_Micro_Climate_Data :: struct {
	// common
	rad:    f64, // radiation per canopy layer (W m-2)
	rad24:  f64, // radiation regime over the last 24 hours (W m-2)
	rad240: f64, // radiation regime over the last 10 days (W m-2)
	tFol:    f64, // foliage temperature per canopy layer (oC)
	tFol24:  f64, // temperature regime over the last 24 hours
	tFol240: f64, // temperature regime over the last 10 days

	// jjv
	sunlitfoliagefraction:   f64, // fraction of sunlit foliage per canopy layer
	sunlitfoliagefraction24: f64, // fraction of sunlit foliage over the past 24 hours per canopy layer

	co2concentration: f64,
}

// C++: struct Voc::Emissions
Voc_Emissions :: struct {
	speciesId_2_isoprene_emission:    map[int]f64, // [umol m-2Ground ts-1] isoprene emissions per timestep and plant
	speciesId_2_monoterpene_emission: map[int]f64, // [umol m-2Ground ts-1] monoterpene emissions per timestep and plant

	isoprene_emission:    f64, // [umol m-2Ground ts-1] isoprene emissions per timestep
	monoterpene_emission: f64, // [umol m-2Ground ts-1] monoterpene emissions per timestep
}

// C++: Voc::Emissions::operator+=(const Emissions&)
voc_emissions_add :: proc(ems: ^Voc_Emissions, other: ^Voc_Emissions, allocator := context.allocator) {
	if ems.speciesId_2_isoprene_emission == nil {
		ems.speciesId_2_isoprene_emission = make(map[int]f64, allocator)
	}
	if ems.speciesId_2_monoterpene_emission == nil {
		ems.speciesId_2_monoterpene_emission = make(map[int]f64, allocator)
	}
	for k, v in other.speciesId_2_isoprene_emission {
		ems.speciesId_2_isoprene_emission[k] += v
	}
	for k, v in other.speciesId_2_monoterpene_emission {
		ems.speciesId_2_monoterpene_emission[k] += v
	}
	ems.isoprene_emission += other.isoprene_emission
	ems.monoterpene_emission += other.monoterpene_emission
}

// C++: struct Voc::photosynth_t
Voc_Photosynth_T :: struct {
	par:    f64, // photosynthetic active radiation (umol m-2 s-1)
	par24:  f64, // 1 day aggregated photosynthetic active radiation (umol m-2 s-1)
	par240: f64, // 10 days aggregated photosynthetic active radiation (umol m-2 s-1)
}

// C++: struct Voc::foliage_t
Voc_Foliage_T :: struct {
	tempK:    f64, // foliage temperature within a canopy layer (K)
	tempK24:  f64, // 1 day aggregated foliage temperature within a canopy layer (K)
	tempK240: f64, // 10 days aggregated foliage temperature within a canopy layer (K)
}

// C++: struct Voc::enzyme_activity_t
Voc_Enzyme_Activity_T :: struct {
	ef_iso:  f64, // emission factor of isoprene (ug gDW-1 h-1)
	ef_mono: f64, // emission factor of monoterpenes (ug gDW-1 h-1)
}

// C++: struct Voc::leaf_emission_t
Voc_Leaf_Emission_T :: struct {
	foliage_layer: int, // C++ size_t

	pho:     Voc_Photosynth_T,
	fol:     Voc_Foliage_T,
	enz_act: Voc_Enzyme_Activity_T,
}

// C++: struct Voc::LeafEmissions
Voc_Leaf_Emissions :: struct {
	isoprene: f64, // isoprene emission (ug m-2ground h-1)
	monoterp: f64, // monoterpene emission (ug m-2ground h-1)
}
