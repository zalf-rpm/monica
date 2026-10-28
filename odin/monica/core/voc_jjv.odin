// Port of src/core/voc-jjv.{h,cpp}: the JJV (Grote et al. 2014) biogenic VOC
// (isoprene/monoterpene) emission model.
//
// Fully self-contained - confirmed by reading the header: takes SpeciesData/
// CPData/MicroClimateData (plain data, voc_common.odin), nothing here touches
// CropModule or MonicaModel.
package core

import libc "core:c/libc"
import "../../support/cr_math"

// C++: double gamma_PH(double, double, CPData)
//
// Emission calculation as described in Grote et al. (2014). Limitations of
// photosynthetic performance due to drought (currently commented out in the
// C++), nitrogen, and phenology are considered upstream (in the not-yet-
// ported crop growth/photosynthesis code); only the temperature-driven
// vcMax/jMax -> vcmax_in/jmax_in step happens here.
@(private)
voc_jjv_gamma_ph :: proc(par, species_theta: f64, cpd: Voc_Cp_Data) -> f64 {
	parabs := par * VOC_ABSO

	// electron transport rate and electron usage
	// "km": michaelis-menten coefficient for electron transport capacity
	km := cpd.ko > 0.0 ? cpd.kc * (1.0 + cpd.oi/cpd.ko) : 0.0

	// "jj": electron provision (umol m-2 s-1) / electron transport rate
	tmp_var := ((parabs + cpd.j_max) * (parabs + cpd.j_max)) - (4.0 * species_theta * parabs * cpd.j_max)
	// NOTE(c++-quirk): the C++ comment claims tmp_var should be the inverse
	// sqrt, but the code only takes the plain sqrt - reproduced as-is.
	jj := tmp_var > 0.0 ? (parabs + cpd.j_max - libc.sqrt(tmp_var)) / (2.0 * species_theta) : 0.0

	// "jv": used electron transport for photosynthesis (C assimilation) (umol
	// m-2 s-1) / fraction of J used for photosynthesis / electron flux
	// required to support Rubisco-limited carbon assimilation
	jv := cpd.ci+km > 0.0 ? 4.0 * cpd.vc_max * (cpd.ci + 2.0*cpd.comp) / (cpd.ci + km) : 0.0

	// bvoc emission potential from photosynthesis (excess energy after carbon
	// assimilation)
	return cpd.comp > 0.0 ? (VOC_C1 + VOC_C2*max(-VOC_GAMMA_MAX, jj-jv)) * jj * min(1.0, cpd.ci/cpd.comp) : 0.0
}

// C++: double gamma_PH_Grote2014(double, double, double, double)
//
// Unused in the C++ (the one call site is commented out in favour of
// gamma_PH) but a real top-level function in the .cpp - ported for
// completeness.
@(private)
voc_jjv_gamma_ph_grote2014 :: proc(comp, ci, jj, jv: f64) -> f64 {
	return comp > 0.0 ? (VOC_C1 + VOC_C2*max(-VOC_GAMMA_MAX, jj-jv)) * jj * min(1.0, ci/comp) : 0.0
}

// C++: struct GammaEnRes (file-local)
@(private)
Voc_Jjv_Gamma_En_Res :: struct {
	en_iso:      f64, // activity factor related to enzyme activity (isoprene synthase)
	en_mono:     f64, // activity factor related to enzyme activity (monoterpene synthase)
	ennorm_iso:  f64, // normalized activity factor related to enzyme activity (isoprene synthase)
	ennorm_mono: f64, // normalized activity factor related to enzyme activity (monoterpene synthase)
}

// C++: GammaEnRes gamma_EN(double, double, const SpeciesData&)
//
// Emission calculation as described in Grote et al. (2014); enzymatic
// activity of isoprene and monoterpene synthase.
@(private)
voc_jjv_gamma_en :: proc(temp_k, norm_temp_k: f64, species: ^Voc_Species_Data) -> Voc_Jjv_Gamma_En_Res {
	res: Voc_Jjv_Gamma_En_Res

	// T in (K) should actually never be below zero
	assert(temp_k > 0.0)

	// Calculate actual bvoc emission potential from enzyme activity
	res.en_iso =
		cr_math.exp(species.ct_is-species.ha_is/(VOC_RGAS*temp_k)) /
		(1.0 + cr_math.exp((species.ds_is*temp_k-species.hd_is)/(VOC_RGAS*temp_k)))
	res.en_mono =
		cr_math.exp(species.ct_mt-species.ha_mt/(VOC_RGAS*temp_k)) /
		(1.0 + cr_math.exp((species.ds_mt*temp_k-species.hd_mt)/(VOC_RGAS*temp_k)))

	// Calculate normalized bvoc emission potential from enzyme activity
	res.ennorm_iso =
		cr_math.exp(species.ct_is-species.ha_is/(VOC_RGAS*norm_temp_k)) /
		(1.0 + cr_math.exp((species.ds_is*norm_temp_k-species.hd_is)/(VOC_RGAS*norm_temp_k)))
	res.ennorm_mono =
		cr_math.exp(species.ct_mt-species.ha_mt/(VOC_RGAS*norm_temp_k)) /
		(1.0 + cr_math.exp((species.ds_mt*norm_temp_k-species.hd_mt)/(VOC_RGAS*norm_temp_k)))

	return res
}

// C++: LeafEmissions calcLeafEmission(const leaf_emission_t&, const
//        leaf_emission_t&, const SpeciesData&, const MicroClimateData&,
//        CPData, bool)
@(private)
voc_jjv_calc_leaf_emission :: proc(
	lemi: ^Voc_Leaf_Emission_T,
	leminorm: ^Voc_Leaf_Emission_T,
	species: ^Voc_Species_Data,
	mcd: ^Voc_Micro_Climate_Data,
	cp_data: Voc_Cp_Data,
	calculate_par_temp_term: bool = false,
) -> Voc_Leaf_Emissions {
	lems: Voc_Leaf_Emissions

	// CALCULATE BVOC EMISSION POTENTIALS WITH RESPECT TO PHOTOSYNTHESIS AND
	// ENZYMATIC ACTIVITY
	// activity factor for Leaf Age (common to both MEGAN and JJV)
	gamma_a := species.fage

	// activity factor for Temperature (only emission from storages = light
	// dependent fraction LDF is 0)
	gamma_t := cr_math.exp(VOC_BETA * (lemi.fol.temp_k - leminorm.fol.temp_k))

	// Emission potential from photosynthesis (energy supply); same for
	// isoprene and monoterpene
	gamma_ph := voc_jjv_gamma_ph(lemi.pho.par, species.theta, cp_data)

	// normalized activity factor for photosynthesis (energy supply)
	gamma_phnorm := voc_jjv_gamma_ph(leminorm.pho.par, species.theta, cp_data)

	gamma_phrel := gamma_phnorm > 0.0 ? gamma_ph / gamma_phnorm : 0.0

	// Emission potential from enzymatic activity of isoprene and monoterpene
	// synthase; actual and normalized
	gamma_en := voc_jjv_gamma_en(lemi.fol.temp_k, leminorm.fol.temp_k, species)

	// Calculate total scaling factor for sun leaves for isoprene and
	// monoterpene
	gamma_iso := gamma_en.ennorm_iso > 0.0 ? gamma_a * gamma_phrel * (gamma_en.en_iso / gamma_en.ennorm_iso) : 0.0
	gamma_mono := gamma_en.ennorm_mono > 0.0 ? gamma_a * gamma_phrel * (gamma_en.en_mono / gamma_en.ennorm_mono) : 0.0

	// PAST TEMPERATURE AND RADIATION DEPENDENCE TERMS FROM VOCMEGAN.CPP
	// MODULE (Guenther et al. 2006, 2012)
	eopt_iso := 1.0
	eopt_mono := 1.0
	par0 := 1.0
	c_p := 1.0
	if calculate_par_temp_term {
		// Factor for temperature dependence of past days (light dependent
		// factors, LDF), from MEGAN. Light independent factors (LIF) for
		// isoprene = 0; for monoterpenes = gamma.t (emissions from storage)
		eopt_iso = VOC_CEO_ISO * cr_math.exp(0.05*((lemi.fol.temp_k24-297.0)+(lemi.fol.temp_k240-297.0)))
		eopt_mono = VOC_CEO_MONO * cr_math.exp(0.05*((lemi.fol.temp_k24-297.0)+(lemi.fol.temp_k240-297.0)))

		// Factor for PPFD dependence of past days (light dependent factors,
		// LDF), from MEGAN. We calculate explicit LIF emission for MT from
		// storage --> no LIF/LDF coefficients needed.
		par0 = 200.0*mcd.sunlitfoliagefraction24 + 50.0*(1.0-mcd.sunlitfoliagefraction24)
		c_p = 0.0468 * cr_math.exp(0.0005*(lemi.pho.par24-par0)) * cr_math.pow(lemi.pho.par240, 0.6)
	}

	// "enz_act.ef_iso/mono": isoprene/monoterpene emission factor/rate
	lems.isoprene = lemi.enz_act.ef_iso * gamma_iso * eopt_iso * c_p
	lems.monoterp = (lemi.enz_act.ef_mono * gamma_mono * eopt_mono * c_p) + species.ef_monos*gamma_t

	return lems
}

// C++: std::pair<SpeciesData, CPData> element of the sds vector
Voc_Species_Cp_Pair :: struct {
	species: Voc_Species_Data,
	cp_data: Voc_Cp_Data,
}

// C++: Voc::Emissions Voc::calculateJJVVOCEmissionsMultipleSpecies(
//        std::vector<std::pair<SpeciesData,CPData>>, const
//        MicroClimateData&, double, bool)
voc_jjv_emissions_multiple_species :: proc(
	sds: []Voc_Species_Cp_Pair,
	mcd: ^Voc_Micro_Climate_Data,
	day_fraction: f64 = 1.0,
	calculate_par_temp_term: bool = false,
	allocator := context.allocator,
) -> Voc_Emissions {
	ems: Voc_Emissions
	ems.species_id_to_isoprene_emission = make(map[int]f64, allocator)
	ems.species_id_to_monoterpene_emission = make(map[int]f64, allocator)

	tslength := f64(VOC_SEC_IN_DAY) * day_fraction

	for p in sds {
		species := p.species
		cp_data := p.cp_data

		if species.m_fol > 0.0 {
			lemi: Voc_Leaf_Emission_T
			leminorm: Voc_Leaf_Emission_T

			// factors for conversion from enzyme activity (umol m-2 (leaf area)
			// s-1) to emission factor (ugC g-1 h-1)
			lsw := VOC_G_IN_KG / species.sla
			c0 := f64(VOC_SEC_IN_HR) * VOC_MC * VOC_UMOL_IN_NMOL // C0 means carbon zero

			// VOCMEGAN recalculation: emission activity recalculated from
			// growthpsim calculations. enz_act.ef_iso/mono can be calculated
			// more exactly but cancels out to just EF_ISO/MONO for static co2
			// concentration.
			nd_co2_concentration_fl := mcd.co2concentration // CO2 concentration per canopy layer
			f_co2 := 370.0 * 1.0 / nd_co2_concentration_fl
			lsw_gsim := VOC_NG_IN_UG * (1.0 / (f64(VOC_SEC_IN_HR) * VOC_MC) * (1000.0 / species.sla))

			// "isoAct_vtfl"/"monoAct_vtfl" [nmol m-2 leaf area s-1] activity
			// state of isoprene/monoterpene synthase
			iso_act := species.scale_i * species.ef_iso * lsw_gsim * (1.0 / 5.0) * f_co2
			mono_act := species.scale_m * species.ef_mono * lsw_gsim * (1.0 / 10.0) * f_co2

			// "enz_act.ef_iso/mono" --> emission factor (including
			// seasonality)
			lemi.enz_act.ef_iso = VOC_C_ISO * c0 * iso_act / (lsw * species.scale_i) // (ugC gDW-1 h-1)
			lemi.enz_act.ef_mono = VOC_C_MONO * c0 * mono_act / (lsw * species.scale_m) // (ugC gDW-1 h-1)

			// conversion of microclimate variables
			lemi.pho.par = mcd.rad * VOC_FPAR * VOC_UMOL_IN_W
			lemi.pho.par24 = mcd.rad24 * VOC_FPAR * VOC_UMOL_IN_W
			lemi.pho.par240 = mcd.rad240 * VOC_FPAR * VOC_UMOL_IN_W
			lemi.fol.temp_k = mcd.t_fol + VOC_D_IN_K
			lemi.fol.temp_k24 = mcd.t_fol24 + VOC_D_IN_K
			lemi.fol.temp_k240 = mcd.t_fol240 + VOC_D_IN_K

			// normalized microclimate variables
			leminorm.pho.par = VOC_PPFD0
			leminorm.fol.temp_k = VOC_TREF

			// emission in dependence on light and temperature for
			// photosynthesis and enzyme activity, weighted over canopy layers.
			// NOTE(c++-quirk): the outer function's own calculateParTempTerm
			// parameter is never passed through here - the C++ call site omits
			// the argument entirely, so calcLeafEmission always uses its
			// default (false), regardless of what the caller passed in.
			// Reproduced exactly (hardcoded false, not calculateParTempTerm).
			lems := voc_jjv_calc_leaf_emission(&lemi, &leminorm, &species, mcd, cp_data, false)

			// conversion from (ugC g-1 h-1) to (umol m-2 ground s-1) and
			// weighting with leaf area and time step length in seconds
			c := (lsw / (f64(VOC_SEC_IN_HR) * VOC_MC)) * species.lai * tslength

			// species and layer specific isoprene/monoterpene emission (umol
			// m-2Ground ts-1)
			ts_isoprene_em := (1.0 / VOC_C_ISO) * c * lems.isoprene
			ts_monoterpene_em := (1.0 / VOC_C_MONO) * c * lems.monoterp

			ems.species_id_to_isoprene_emission[species.id] = ts_isoprene_em
			ems.isoprene_emission += ts_isoprene_em
			ems.species_id_to_monoterpene_emission[species.id] = ts_monoterpene_em
			ems.monoterpene_emission += ts_monoterpene_em
		} else {
			ems.species_id_to_isoprene_emission[species.id] = 0.0
			ems.species_id_to_monoterpene_emission[species.id] = 0.0
		}
	}
	return ems
}

// C++: inline Emissions Voc::calculateJJVVOCEmissions(SpeciesData, const
//        MicroClimateData&, CPData, double, bool)
voc_jjv_emissions :: proc(
	sd: Voc_Species_Data,
	mcd: ^Voc_Micro_Climate_Data,
	cpdata: Voc_Cp_Data,
	day_fraction: f64 = 1.0,
	calculate_par_temp_term: bool = false,
	allocator := context.allocator,
) -> Voc_Emissions {
	sds := []Voc_Species_Cp_Pair{{sd, cpdata}}
	return voc_jjv_emissions_multiple_species(sds, mcd, day_fraction, calculate_par_temp_term, allocator)
}
