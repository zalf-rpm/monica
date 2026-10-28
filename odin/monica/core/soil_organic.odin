// Port of src/core/soilorganic.{h,cpp}: SoilOrganic, make_soil_organic,
// soil_organic_step, and every other soilorganic:: proc.
//
// No monica-back-pointer deviation: the C++ struct takes `SoilColumn&` and
// `SoilOrganicModuleParameters` directly (no MonicaModel&), just like
// soiltransport. `cropModule` follows the established phase-5 stub pattern
// (crop_module_stub.odin), extended with `vc_NetPrimaryProduction` (the one
// field this file reads, in soil_organic_step).
//
// Two real C++ copy-vs-reference quirks, reproduced exactly (not "fixed"):
// `foUrea`'s `auto layer0 = so->soilColumn.layers.at(0);` and
// `foVolatilisation`'s `auto lay0 = so->soilColumn.layers.at(0);` are BOTH
// by-value copies (no `&`), unlike every other `auto &layi = ...` in this
// file. Mutations made through `layer0`/`lay0` (vs_SoilNH4, and in
// foVolatilisation's case the AOM pool's vo_DaysAfterApplication too, since
// AOM_Pool is bound as a reference *into the copy*) never reach the real soil
// column - a genuine, confirmed upstream bug. See soil_organic_fo_urea and
// soil_organic_fo_volatilisation below for exactly where this happens.
package core

import tl "../../support/tools"
import p "../params"
import "../soil"
import libc "core:c/libc"
import "../../support/cr_math"

// C++: struct monica::SoilOrganic
Soil_Organic :: struct {
	soil_column:                  ^Soil_Column,
	mod_params:                   p.Soil_Organic_Module_Parameters,
	added_organic_matter:         bool,
	irrigation_amount:            f64,
	act_ammonia_oxidation_rate:   [dynamic]f64 `unit:"kgN m-3 d-1"`,
	act_nitrification_rate:       [dynamic]f64 `unit:"kgN m-3 d-1"`,
	act_denitrification_rate:     [dynamic]f64 `unit:"kgN m-3 d-1"`,
	aom_fast_delta_sum:           [dynamic]f64,
	aom_fast_input:               [dynamic]f64 `unit:"kgC m-3"`,
	aom_fast_sum:                 [dynamic]f64,
	aom_slow_delta_sum:           [dynamic]f64,
	aom_slow_input:               [dynamic]f64 `unit:"kgC m-3"`,
	aom_slow_sum:                 [dynamic]f64,
	c_balance:                    [dynamic]f64,
	decomposer_respiration:       f64,
	error_message:                string,
	inert_soil_organic_c:         [dynamic]f64,
	inert_soil_organic_c_high_cn: [dynamic]f64,
	n2o_produced:                 f64 `unit:"kgN ha-1"`,
	n2o_produced_nit:             f64 `unit:"kgN ha-1"`,
	n2o_produced_denit:           f64 `unit:"kgN ha-1"`,
	net_ecosystem_exchange:       f64 `unit:"kgC ha-1"`,
	net_ecosystem_production:     f64 `unit:"kgC ha-1"`,
	net_n_mineralisation:         f64,
	net_n_mineralisation_rate:    [dynamic]f64,
	total_nh3_volatilised:        f64,
	nh3_volatilised:              f64,
	smb_co2_evolution_rate:       [dynamic]f64,
	smb_fast_delta:               [dynamic]f64,
	smb_slow_delta:               [dynamic]f64,
	// vs_SoilMineralNContent:       [dynamic]f64,
	soil_organic_c:               [dynamic]f64,
	soil_organic_c_high_cn:       [dynamic]f64,
	som_fast_delta:               [dynamic]f64,
	som_fast_input:               [dynamic]f64 `unit:"kgC m-3"`,
	som_slow_delta:               [dynamic]f64,
	sum_denitrification:          f64, // kg-N/m2
	sum_net_n_mineralisation:     f64,
	sum_n2o_produced:             f64,
	sum_nh3_volatilised:          f64,
	total_denitrification:        f64,

	// True if organic fertilizer has been added with a following
	// incorporation. Automatically set to false if carbamid amount falls
	// below 0.001.
	incorporation:                bool,
	crop_module:                  ^Crop_Module,
}

// C++: kj::Own<SoilOrganic> monica::makeSoilOrganic(SoilColumn&,
//        SoilOrganicModuleParameters)
//
// Returns by value, matching make_soil_column/make_soil_temperature/
// make_soil_moisture/make_soil_transport's precedent. Inlines C++'s
// soilorganic::initializeFromParams(SoilOrganic*), which has no other caller.
make_soil_organic :: proc(
	soil_column: ^Soil_Column,
	params: p.Soil_Organic_Module_Parameters,
) -> Soil_Organic {
	so: Soil_Organic
	so.soil_column = soil_column
	so.mod_params = params

	sc := so.soil_column
	nools := sc.number_of_organic_layers
	resize(&so.act_ammonia_oxidation_rate, nools)
	resize(&so.act_nitrification_rate, nools)
	resize(&so.act_denitrification_rate, nools)
	resize(&so.aom_fast_delta_sum, nools)
	resize(&so.aom_fast_input, nools)
	resize(&so.aom_fast_sum, nools)
	resize(&so.aom_slow_delta_sum, nools)
	resize(&so.aom_slow_input, nools)
	resize(&so.aom_slow_sum, nools)
	resize(&so.c_balance, nools)
	resize(&so.inert_soil_organic_c, nools)
	resize(&so.inert_soil_organic_c_high_cn, nools)
	resize(&so.net_n_mineralisation_rate, nools)
	resize(&so.smb_co2_evolution_rate, nools)
	resize(&so.smb_fast_delta, nools)
	resize(&so.smb_slow_delta, nools)
	resize(&so.soil_organic_c, nools)
	resize(&so.soil_organic_c_high_cn, nools)
	resize(&so.som_fast_input, nools)
	resize(&so.som_fast_delta, nools)
	resize(&so.som_slow_delta, nools)

	part_som_to_smb_slow := so.mod_params.part_som_to_smb_slow
	part_som_to_smb_fast := so.mod_params.part_som_to_smb_fast
	som_slow_dec_coeff_standard := so.mod_params.som_slow_dec_coeff_standard
	som_fast_dec_coeff_standard := so.mod_params.som_fast_dec_coeff_standard
	part_som_fast_to_som_slow := so.mod_params.part_som_fast_to_som_slow
	po_inert_cn_lower_limit := 11.0
	po_inert_cn_upper_limit := 350.0

	for i in 0 ..< sc.number_of_organic_layers {
		layer := &sc.layers[i]
		layi := &sc.layers[i]

		so.soil_organic_c[i] = soil_organic_carbon(layer) * soil_bulk_density(layer)

		if layi.soil_cn_ratio > 100 {
			so.inert_soil_organic_c_high_cn[i] =
				(so.soil_organic_c[i] * layer.layer_thickness_m / 1000 * 10000.0) /
				layi.soil_cn_ratio *
				(po_inert_cn_lower_limit - layi.soil_cn_ratio) /
				(po_inert_cn_lower_limit / po_inert_cn_upper_limit - 1) /
				10000.0 *
				1000.0 /
				layer.layer_thickness_m

			so.soil_organic_c_high_cn[i] =
				so.soil_organic_c[i] - so.inert_soil_organic_c_high_cn[i]

			so.inert_soil_organic_c[i] =
				(0.049 *
					cr_math.pow(
						so.soil_organic_c_high_cn[i] * layer.layer_thickness_m / 1000 * 10000.0,
						1.139,
					)) /
				10000.0 *
				1000.0 /
				layer.layer_thickness_m

			so.inert_soil_organic_c[i] =
				so.inert_soil_organic_c[i] + so.inert_soil_organic_c_high_cn[i]
		} else {
			so.inert_soil_organic_c[i] =
				(0.049 *
					cr_math.pow(
						so.soil_organic_c[i] * layer.layer_thickness_m / 1000 * 10000.0,
						1.139,
					)) /
				10000.0 *
				1000.0 /
				layer.layer_thickness_m
		}
		so.soil_organic_c[i] -= so.inert_soil_organic_c[i]

		layer.smb_slow = part_som_to_smb_slow * so.soil_organic_c[i]
		layer.smb_fast = part_som_to_smb_fast * so.soil_organic_c[i]

		layer.som_slow =
			so.soil_organic_c[i] /
			(1.0 +
					som_slow_dec_coeff_standard /
						(som_fast_dec_coeff_standard * part_som_fast_to_som_slow))
		layer.som_fast = so.soil_organic_c[i] - layer.som_slow
		so.soil_organic_c[i] -= layer.smb_slow + layer.smb_fast

		layer.soil_organic_carbon =
			(so.soil_organic_c[i] + so.inert_soil_organic_c[i]) / soil_bulk_density(layer)

		so.act_denitrification_rate[i] = 0.0
	}

	return so
}

// C++: void monica::soilorganic::foUrea(SoilOrganic*)
soil_organic_fo_urea :: proc(so: ^Soil_Organic, allocator := context.allocator) {
	sc := so.soil_column
	nools := sc.number_of_organic_layers
	soil_carbamid_solid := make([dynamic]f64, nools, allocator)
	soil_carbamid_aq := make([dynamic]f64, nools, allocator)
	hydrolysis_rate1 := make([dynamic]f64, nools, allocator)
	hydrolysis_rate2 := make([dynamic]f64, nools, allocator)
	hydrolysis_rate_max := make([dynamic]f64, nools, allocator)
	hydrolysis_ph_effect := make([dynamic]f64, nools, allocator)
	hydrolysis_rate := make([dynamic]f64, nools, allocator)
	h3o_ion_concentration := 0.0
	nh3_aq_equilibrium_const := 0.0
	nh3_equilibrium_const := 0.0
	soil_nh4_aq := 0.0
	nh3_aq := 0.0
	nh3_gas := 0.0
	nh3_volatilising := 0.0

	hydrolysis_km := so.mod_params.hydrolysis_km
	hydrolysis_p1 := so.mod_params.hydrolysis_p1
	hydrolysis_p2 := so.mod_params.hydrolysis_p2
	activation_energy := so.mod_params.activation_energy

	so.nh3_volatilised = 0.0

	for i in 0 ..< nools {
		layer := &sc.layers[i]

		// kmol urea m-3 soil
		soil_carbamid_solid[i] =
			layer.soil_carbamid / soil.PO_UREA_MOLECULAR_WEIGHT / soil.PO_UREA_TO_N / 1000.0

		// mol urea kg Solution-1
		soil_carbamid_aq[i] =
			(-1258.9 +
				13.2843 * (layer.soil_temperature + 273.15) -
				0.047381 *
					((layer.soil_temperature + 273.15) * (layer.soil_temperature + 273.15)) +
				5.77264e-5 * cr_math.pow(layer.soil_temperature + 273.15, 3.0))

		// kmol urea m-3 soil
		soil_carbamid_aq[i] =
			(soil_carbamid_aq[i] / (1.0 + (soil_carbamid_aq[i] * 0.0453))) *
			layer.soil_moisture_m3

		if soil_carbamid_aq[i] >= soil_carbamid_solid[i] {
			soil_carbamid_aq[i] = soil_carbamid_solid[i]
			soil_carbamid_solid[i] = 0.0
		} else {
			soil_carbamid_solid[i] -= soil_carbamid_aq[i]
		}

		// Calculate urea hydrolysis
		hydrolysis_rate1[i] =
			(hydrolysis_p1 * (soil_organic_matter(layer) * 100.0) * soil.PO_SOM_TO_C +
				hydrolysis_p2) /
			soil.PO_UREA_MOLECULAR_WEIGHT

		hydrolysis_rate2[i] =
			hydrolysis_rate1[i] / cr_math.exp(-activation_energy / (8.314 * 310.0))

		hydrolysis_rate_max[i] =
			hydrolysis_rate2[i] *
			cr_math.exp(-activation_energy / (8.314 * (layer.soil_temperature + 273.15)))

		hydrolysis_ph_effect[i] = cr_math.exp(
			-0.064 * ((layer.soil_ph - 6.5) * (layer.soil_ph - 6.5)),
		)

		// kmol urea kg soil-1 s-1
		hydrolysis_rate[i] =
			hydrolysis_rate_max[i] *
			soil_organic_fo_moist_on_hydrolysis(so, soil_moisture_pf(layer)) *
			hydrolysis_ph_effect[i] *
			soil_carbamid_aq[i] /
			(hydrolysis_km + soil_carbamid_aq[i])

		// kmol urea m soil-3 d-1
		hydrolysis_rate[i] = hydrolysis_rate[i] * 86400.0 * soil_bulk_density(layer)

		if hydrolysis_rate[i] >= soil_carbamid_aq[i] {
			layer.soil_nh4 += layer.soil_carbamid
			layer.soil_carbamid = 0.0
		} else {
			// kg N m soil-3
			layer.soil_carbamid -=
				hydrolysis_rate[i] * soil.PO_UREA_MOLECULAR_WEIGHT * soil.PO_UREA_TO_N * 1000.0

			// kg N m soil-3
			layer.soil_nh4 +=
				hydrolysis_rate[i] * soil.PO_UREA_MOLECULAR_WEIGHT * soil.PO_UREA_TO_N * 1000.0
		}

		// Calculate general volatilisation from NH4-Pool in top layer
		if i == 0 {
			// NOTE(c++-quirk): C++ `auto layer0 = so->soilColumn.layers.at(0);` is
			// a BY-VALUE COPY (every other loop variable in this file is `auto
			// &layi`). The vs_SoilNH4 mutation below therefore never reaches the
			// real soil column - reproduced exactly, not fixed.
			layer0 := sc.layers[0]

			h3o_ion_concentration = cr_math.pow(10.0, -layer0.soil_ph) // kmol m-3
			nh3_aq_equilibrium_const = cr_math.pow(
				10.0,
				(-2728.3 / (layer0.soil_temperature + 273.15)) - 0.094219,
			) // K2 in Sadeghi's program

			nh3_equilibrium_const = cr_math.pow(
				10.0,
				(1630.5 / (layer0.soil_temperature + 273.15)) - 2.301,
			) // K1 in Sadeghi's program
			_ = nh3_equilibrium_const

			// kmol m-3, assuming that all NH4 is solved
			soil_nh4_aq = layer0.soil_nh4 / (soil.PO_NH4_MOLECULAR_WEIGHT * 1000.0)

			// kmol m-3
			nh3_aq = soil_nh4_aq / (1.0 + (h3o_ion_concentration / nh3_aq_equilibrium_const))

			nh3_gas = nh3_aq

			// kg N m-3 d-1
			nh3_volatilising = nh3_gas * soil.PO_NH3_MOLECULAR_WEIGHT * 1000.0

			if nh3_volatilising >= layer0.soil_nh4 {
				nh3_volatilising = layer0.soil_nh4
				layer0.soil_nh4 = 0.0
			} else {
				layer0.soil_nh4 -= nh3_volatilising
			}

			// kg N m-2 d-1
			so.nh3_volatilised = nh3_volatilising * layer0.layer_thickness_m
		}
	}

	// set incorporation to false, if carbamid part is falling below a treshold
	// only, if organic matter was not recently added
	if len(soil_carbamid_aq) > 0 && soil_carbamid_aq[0] < 0.001 && !so.added_organic_matter {
		so.incorporation = false
	}
}

// C++: void monica::soilorganic::foMIT(SoilOrganic*)
//
// Internal Subroutine MIT - Mineralisation Immobilisitation Turn-Over.
//
// NOTE(c++-quirk, not a bug): the C++'s local `vo_AOM_FastDeltaSum`/
// `vo_AOM_SlowDeltaSum` (declared partway through the function body) shadow
// the struct field names but are genuinely fresh function-local vectors, not
// aliases to `so->vo_AOM_FastDeltaSum`/`so->vo_AOM_SlowDeltaSum` - those
// struct fields are populated later, by foPoolUpdate. Reproduced as local
// variables here too, never touching `so.aom_fast_delta_sum`/
// `so.aom_slow_delta_sum`.
soil_organic_fo_mit :: proc(so: ^Soil_Organic, allocator := context.allocator) {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.number_of_organic_layers
	som_slow_dec_coeff_standard := params.som_slow_dec_coeff_standard
	som_fast_dec_coeff_standard := params.som_fast_dec_coeff_standard
	smb_slow_death_rate_standard := params.smb_slow_death_rate_standard
	smb_slow_maint_rate_standard := params.smb_slow_maint_rate_standard
	smb_fast_death_rate_standard := params.smb_fast_death_rate_standard
	smb_fast_maint_rate_standard := params.smb_fast_maint_rate_standard
	som_slow_utilization_efficiency := params.som_slow_utilization_efficiency
	som_fast_utilization_efficiency := params.som_fast_utilization_efficiency
	part_som_fast_to_som_slow := params.part_som_fast_to_som_slow
	part_smb_slow_to_som_fast := params.part_smb_slow_to_som_fast
	part_smb_fast_to_som_fast := params.part_smb_fast_to_som_fast
	smb_utilization_efficiency := params.smb_utilization_efficiency
	cn_ratio_smb := params.cn_ratio_smb
	aom_slow_utilization_efficiency := params.aom_slow_utilization_efficiency
	aom_fast_utilization_efficiency := params.aom_fast_utilization_efficiency
	immobilisation_rate_coeff_nh4 := params.immobilisation_rate_coeff_nh4
	immobilisation_rate_coeff_no3 := params.immobilisation_rate_coeff_no3

	ao_mslow_to_sm_bfast := make([dynamic]f64, nools, allocator)
	ao_mslow_to_sm_bslow := make([dynamic]f64, nools, allocator)
	ao_mfast_to_sm_bfast := make([dynamic]f64, nools, allocator)

	// Sum of decomposition rates for fast added organic matter pools
	aom_fast_dec_rate_sum := make([dynamic]f64, nools, allocator)
	// Sum of all changes to added organic matter fast pool [kg C m-3]
	aom_fast_delta_sum := make([dynamic]f64, nools, allocator)
	// Sum of decomposition rates for slow added organic matter pools
	aom_slow_dec_rate_sum := make([dynamic]f64, nools, allocator)
	// Sum of all changes to added organic matter slow pool [kg C m-3]
	aom_slow_delta_sum := make([dynamic]f64, nools, allocator)

	// [kg m-3]
	for i in 0 ..< len(so.c_balance) {
		so.c_balance[i] = 0.0
	}

	// N balance of each layer [kg N m-3]
	n_balance := make([dynamic]f64, nools, allocator)

	smb_fast_co2_evolution_rate := make([dynamic]f64, nools, allocator)
	smb_fast_death_rate := make([dynamic]f64, nools, allocator)
	smb_fast_death_rate_coeff := make([dynamic]f64, nools, allocator)
	smb_fast_dec_rate := make([dynamic]f64, nools, allocator)
	smb_fast_maint_rate_coeff := make([dynamic]f64, nools, allocator)
	smb_fast_maint_rate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.smb_fast_delta) {
		so.smb_fast_delta[i] = 0.0
	}

	smb_slow_co2_evolution_rate := make([dynamic]f64, nools, allocator)
	smb_slow_death_rate := make([dynamic]f64, nools, allocator)
	smb_slow_death_rate_coeff := make([dynamic]f64, nools, allocator)
	smb_slow_dec_rate := make([dynamic]f64, nools, allocator)
	smb_slow_maint_rate_coeff := make([dynamic]f64, nools, allocator)
	smb_slow_maint_rate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.smb_slow_delta) {
		so.smb_slow_delta[i] = 0.0
	}

	som_fast_dec_coeff := make([dynamic]f64, nools, allocator)
	som_fast_dec_rate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.som_fast_delta) {
		so.som_fast_delta[i] = 0.0
	}

	som_slow_dec_coeff := make([dynamic]f64, nools, allocator)
	som_slow_dec_rate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.som_slow_delta) {
		so.som_slow_delta[i] = 0.0
	}

	// Calculation of decay rate coefficients
	for i in 0 ..< nools {
		layi := &sc.layers[i]
		tod :=
			params.__enable_kaiteew_TempOnDecompostion__ ? soil_organic_fo_temp_on_decompostion_kaiteew(so, layi.soil_temperature, params.q_ten_factor, params.temp_dec_optimal) : soil_organic_fo_temp_on_decompostion(so, layi.soil_temperature) // prev code

		mod_ :=
			params.__enable_kaiteew_MoistOnDecompostion__ ? soil_organic_fo_moist_on_decompostion_kaiteew(so, layi.soil_moisture_m3, layi.saturation, params.moisture_dec_optimal) : soil_organic_fo_moist_on_decompostion(so, soil_moisture_pf(layi)) // prev code

		cod :=
			params.__enable_kaiteew_ClayOnDecompostion__ ? soil_organic_fo_clay_on_decompostion_kaiteew(so, layi.soil_clay_content, params.limit_clay_effect) : soil_organic_fo_clay_on_decompostion(so, layi.soil_clay_content, params.limit_clay_effect) // prev code

		som_slow_dec_coeff[i] = som_slow_dec_coeff_standard * tod * mod_
		som_fast_dec_coeff[i] = som_fast_dec_coeff_standard * tod * mod_
		som_slow_dec_rate[i] = som_slow_dec_coeff[i] * layi.som_slow
		som_fast_dec_rate[i] = som_fast_dec_coeff[i] * layi.som_fast

		smb_slow_maint_rate_coeff[i] = smb_slow_maint_rate_standard * cod * tod * mod_
		smb_fast_maint_rate_coeff[i] = smb_fast_maint_rate_standard * cod * tod * mod_

		smb_slow_maint_rate[i] = smb_slow_maint_rate_coeff[i] * layi.smb_slow
		smb_fast_maint_rate[i] = smb_fast_maint_rate_coeff[i] * layi.smb_fast
		smb_slow_death_rate_coeff[i] = smb_slow_death_rate_standard * tod * mod_
		smb_fast_death_rate_coeff[i] = smb_fast_death_rate_standard * tod * mod_
		smb_slow_death_rate[i] = smb_slow_death_rate_coeff[i] * layi.smb_slow
		smb_fast_death_rate[i] = smb_fast_death_rate_coeff[i] * layi.smb_fast

		smb_slow_dec_rate[i] = smb_slow_death_rate[i] + smb_slow_maint_rate[i]
		smb_fast_dec_rate[i] = smb_fast_death_rate[i] + smb_fast_maint_rate[i]

		for &aom_pool in layi.aom_pool {
			aom_pool.aom_slow_dec_coeff = aom_pool.aom_slow_dec_coeff_standard * tod * mod_
			aom_pool.aom_fast_dec_coeff = aom_pool.aom_fast_dec_coeff_standard * tod * mod_
		}
	}

	// Calculation of pool changes by decomposition
	for i in 0 ..< nools {
		layi := &sc.layers[i]

		for &props in layi.aom_pool {
			// Eq.6-5 and 6-6 in the DAISY manual
			props.aom_slow_delta = -(props.aom_slow_dec_coeff * props.aom_slow)
			if -props.aom_slow_delta > props.aom_slow {
				props.aom_slow_delta = -props.aom_slow
			}
			props.aom_fast_delta = -(props.aom_fast_dec_coeff * props.aom_fast)
			if -props.aom_fast_delta > props.aom_fast {
				props.aom_fast_delta = -props.aom_fast
			}
		}

		// Eq.6-7 in the DAISY manual
		aom_slow_dec_rate_sum[i] = 0.0

		for &props in layi.aom_pool {
			props.aom_slow_dec_rate_to_smb_slow =
				props.part_aom_slow_to_smb_slow * props.aom_slow_dec_coeff * props.aom_slow
			props.aom_slow_dec_rate_to_smb_fast =
				props.part_aom_slow_to_smb_fast * props.aom_slow_dec_coeff * props.aom_slow

			aom_slow_dec_rate_sum[i] +=
				props.aom_slow_dec_rate_to_smb_slow + props.aom_slow_dec_rate_to_smb_fast

			ao_mslow_to_sm_bfast[i] += props.aom_slow_dec_rate_to_smb_fast
			ao_mslow_to_sm_bslow[i] += props.aom_slow_dec_rate_to_smb_slow
		}

		// Eq.6-8 in the DAISY manual
		aom_fast_dec_rate_sum[i] = 0.0
		ao_mfast_to_sm_bfast[i] = 0.0

		for &props in layi.aom_pool {
			props.aom_fast_dec_rate_to_smb_fast = props.aom_fast_dec_coeff * props.aom_fast
			aom_fast_dec_rate_sum[i] += props.aom_fast_dec_rate_to_smb_fast
			ao_mfast_to_sm_bfast[i] += props.aom_fast_dec_rate_to_smb_fast
		}

		so.smb_slow_delta[i] =
			(som_slow_utilization_efficiency * som_slow_dec_rate[i]) +
			(som_fast_utilization_efficiency *
					(1.0 - part_som_fast_to_som_slow) *
					som_fast_dec_rate[i]) +
			(aom_slow_utilization_efficiency * ao_mslow_to_sm_bslow[i]) -
			smb_slow_dec_rate[i]

		so.smb_fast_delta[i] =
			(smb_utilization_efficiency *
				(1.0 - part_smb_slow_to_som_fast) *
				(smb_slow_death_rate[i] + smb_fast_death_rate[i])) +
			(aom_fast_utilization_efficiency * ao_mfast_to_sm_bfast[i]) +
			(aom_slow_utilization_efficiency * ao_mslow_to_sm_bfast[i]) -
			smb_fast_dec_rate[i]

		// Eq.6-9 in the DAISY manual
		so.som_slow_delta[i] =
			part_som_fast_to_som_slow * som_fast_dec_rate[i] - som_slow_dec_rate[i]

		if (layi.som_slow + so.som_slow_delta[i]) < 0.0 {
			so.som_slow_delta[i] = layi.som_slow
		}

		// Eq.6-10 in the DAISY manual
		so.som_fast_delta[i] =
			part_smb_slow_to_som_fast * smb_slow_death_rate[i] +
			part_smb_fast_to_som_fast * smb_fast_death_rate[i] -
			som_fast_dec_rate[i]

		if (layi.som_fast + so.som_fast_delta[i]) < 0.0 {
			so.som_fast_delta[i] = layi.som_fast
		}

		aom_slow_delta_sum[i] = 0.0
		aom_fast_delta_sum[i] = 0.0

		for &props in layi.aom_pool {
			aom_slow_delta_sum[i] += props.aom_slow_delta
			aom_fast_delta_sum[i] += props.aom_fast_delta
		}
	}

	// Calculation of N balance
	for i in 0 ..< nools {
		layi := &sc.layers[i]

		cn_ratio_som_slow := layi.soil_cn_ratio
		cn_ratio_som_fast := cn_ratio_som_slow

		n_balance[i] =
			-(so.smb_slow_delta[i] / cn_ratio_smb) -
			(so.smb_fast_delta[i] / cn_ratio_smb) -
			(so.som_slow_delta[i] / cn_ratio_som_slow) -
			(so.som_fast_delta[i] / cn_ratio_som_fast)

		for &props in layi.aom_pool {
			if libc.fabs(props.cn_ratio_aom_fast) >= 1.0e-7 {
				n_balance[i] -= props.aom_fast_delta / props.cn_ratio_aom_fast
			}
			if libc.fabs(props.cn_ratio_aom_slow) >= 1.0e-7 {
				n_balance[i] -= props.aom_slow_delta / props.cn_ratio_aom_slow
			}
		}
	}

	// Check for Nmin availablity in case of immobilisation
	so.net_n_mineralisation = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]

		cn_ratio_som_slow := layi.soil_cn_ratio
		cn_ratio_som_fast := cn_ratio_som_slow

		if n_balance[i] < 0.0 {
			if libc.fabs(n_balance[i]) >=
			   ((layi.soil_nh4 * immobilisation_rate_coeff_nh4) +
					   (layi.soil_no3 * immobilisation_rate_coeff_no3)) {
				aom_slow_delta_sum[i] = 0.0
				aom_fast_delta_sum[i] = 0.0

				for &props in layi.aom_pool {
					if props.cn_ratio_aom_slow >=
					   (cn_ratio_smb / aom_slow_utilization_efficiency) {
						props.aom_slow_delta = 0.0
						// correction of the fluxes across pools
						ao_mslow_to_sm_bfast[i] -= props.aom_slow_dec_rate_to_smb_fast
						ao_mslow_to_sm_bslow[i] -= props.aom_slow_dec_rate_to_smb_slow
					}

					if props.cn_ratio_aom_fast >=
					   (cn_ratio_smb / aom_fast_utilization_efficiency) {
						props.aom_fast_delta = 0.0
						// correction of the fluxes across pools
						ao_mfast_to_sm_bfast[i] -= props.aom_fast_dec_rate_to_smb_fast
					}

					aom_slow_delta_sum[i] += props.aom_slow_delta
					aom_fast_delta_sum[i] += props.aom_fast_delta
				}

				if cn_ratio_som_slow >= (cn_ratio_smb / som_slow_utilization_efficiency) {
					so.som_slow_delta[i] = 0.0
				}

				if cn_ratio_som_fast >= (cn_ratio_smb / som_fast_utilization_efficiency) {
					so.som_fast_delta[i] = 0.0
				}

				// Recalculation of SMB pool changes
				so.smb_slow_delta[i] =
					(som_slow_utilization_efficiency * som_slow_dec_rate[i]) +
					(som_fast_utilization_efficiency *
							(1.0 - part_som_fast_to_som_slow) *
							som_fast_dec_rate[i]) +
					(aom_slow_utilization_efficiency * ao_mslow_to_sm_bslow[i]) -
					smb_slow_dec_rate[i]

				if (layi.smb_slow + so.smb_slow_delta[i]) < 0.0 {
					so.smb_slow_delta[i] = layi.smb_slow
				}

				so.smb_fast_delta[i] =
					(smb_utilization_efficiency *
						(1.0 - part_smb_slow_to_som_fast) *
						(smb_slow_death_rate[i] + smb_fast_death_rate[i])) +
					(aom_fast_utilization_efficiency * ao_mfast_to_sm_bfast[i]) +
					(aom_slow_utilization_efficiency * ao_mslow_to_sm_bfast[i]) -
					smb_fast_dec_rate[i]

				if (layi.smb_fast + so.smb_fast_delta[i]) < 0.0 {
					so.smb_fast_delta[i] = layi.smb_fast
				}

				// Recalculation of N balance under conditions of immobilisation
				n_balance[i] =
					-(so.smb_slow_delta[i] / cn_ratio_smb) -
					(so.smb_fast_delta[i] / cn_ratio_smb) -
					(so.som_slow_delta[i] / cn_ratio_som_slow) -
					(so.som_fast_delta[i] / cn_ratio_som_fast)

				for &props in layi.aom_pool {
					if libc.fabs(props.cn_ratio_aom_fast) >= 1.0e-7 {
						n_balance[i] -= (props.aom_fast_delta / props.cn_ratio_aom_fast)
					}
					if libc.fabs(props.cn_ratio_aom_slow) >= 1.0e-7 {
						n_balance[i] -= (props.aom_slow_delta / props.cn_ratio_aom_slow)
					}
				}

				// Update of Soil NH4 after recalculated N balance
				layi.soil_nh4 += libc.fabs(n_balance[i])
			} else {
				// Bedarf kann durch Ammonium-Pool nicht gedeckt werden --> Nitrat wird verwendet
				if libc.fabs(n_balance[i]) >= (layi.soil_nh4 * immobilisation_rate_coeff_nh4) {
					layi.soil_no3 -=
						libc.fabs(n_balance[i]) - (layi.soil_nh4 * immobilisation_rate_coeff_nh4)
					layi.soil_nh4 -= layi.soil_nh4 * immobilisation_rate_coeff_nh4
				} else {
					layi.soil_nh4 -= libc.fabs(n_balance[i])
				}
			}
		} else {
			layi.soil_nh4 += libc.fabs(n_balance[i])
		}

		lay0 := &sc.layers[0]
		so.net_n_mineralisation_rate[i] = libc.fabs(n_balance[i]) * lay0.layer_thickness_m // [kg m-3] --> [kg m-2]
		so.net_n_mineralisation += libc.fabs(n_balance[i]) * lay0.layer_thickness_m // [kg m-3] --> [kg m-2]
		so.sum_net_n_mineralisation += libc.fabs(n_balance[i]) * lay0.layer_thickness_m // [kg m-3] --> [kg m-2]
	}

	so.decomposer_respiration = 0.0

	// Calculation of CO2 evolution
	for i in 0 ..< nools {
		smb_slow_co2_evolution_rate[i] =
			((1.0 - som_slow_utilization_efficiency) * som_slow_dec_rate[i]) +
			((1.0 - som_fast_utilization_efficiency) *
					(1.0 - part_som_fast_to_som_slow) *
					som_fast_dec_rate[i]) +
			((1.0 - aom_slow_utilization_efficiency) * ao_mslow_to_sm_bslow[i]) +
			smb_slow_maint_rate[i]

		smb_fast_co2_evolution_rate[i] =
			(1.0 - smb_utilization_efficiency) *
				(((1.0 - part_smb_slow_to_som_fast) * smb_slow_death_rate[i]) +
						((1.0 - part_smb_fast_to_som_fast) * smb_fast_death_rate[i])) +
			((1.0 - aom_slow_utilization_efficiency) * ao_mslow_to_sm_bfast[i]) +
			((1.0 - aom_fast_utilization_efficiency) * ao_mfast_to_sm_bfast[i]) +
			smb_fast_maint_rate[i]

		so.smb_co2_evolution_rate[i] =
			smb_slow_co2_evolution_rate[i] + smb_fast_co2_evolution_rate[i]

		so.decomposer_respiration += so.smb_co2_evolution_rate[i] * sc.layers[i].layer_thickness_m // [kg C m-3] -> [kg C m-2]
	}
}

// C++: double monica::soilorganic::foClayOnDecompostionKaiteew(SoilOrganic*, double, double)
soil_organic_fo_clay_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	d_soil_clay_content, d_limit_clay_effect: f64,
) -> f64 {
	clay_on_decomposition := 0.0

	if d_soil_clay_content >= 0.0 && d_soil_clay_content <= 1.0 {
		clay_on_decomposition =
			(1.0 - d_limit_clay_effect) / (1.0 + cr_math.exp(-3.14 + d_soil_clay_content * 16)) +
			d_limit_clay_effect
	} else {
		so.error_message = "irregular clay content"
	}

	return clay_on_decomposition
}

// C++: double monica::soilorganic::foClayOnDecompostion(SoilOrganic*, double, double)
soil_organic_fo_clay_on_decompostion :: proc(
	so: ^Soil_Organic,
	d_soil_clay_content, d_limit_clay_effect: f64,
) -> f64 {
	fo_clay_on_decompostion := 0.0

	if d_soil_clay_content >= 0.0 && d_soil_clay_content <= d_limit_clay_effect {
		fo_clay_on_decompostion = 1.0 - 2.0 * d_soil_clay_content
	} else if d_soil_clay_content > d_limit_clay_effect && d_soil_clay_content <= 1.0 {
		fo_clay_on_decompostion = 1.0 - 2.0 * d_limit_clay_effect
	} else {
		so.error_message = "irregular clay content"
	}

	return fo_clay_on_decompostion
}

// C++: double monica::soilorganic::foTempOnDecompostionKaiteew(SoilOrganic*, double, double, double)
soil_organic_fo_temp_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	soil_temperature, q_ten_factor, temp_dec_optimal: f64,
) -> f64 {
	temp_on_decomposition := 0.0

	if soil_temperature > 0.0 && soil_temperature <= 100.0 {
		temp_on_decomposition =
			1.0 /
			cr_math.pow(
				1.0 + cr_math.exp(soil_temperature - (2.72 + temp_dec_optimal)),
				q_ten_factor / (temp_dec_optimal / 3.14),
			) *
			(-1.0 + cr_math.pow(q_ten_factor, soil_temperature / 15.76))
	} else if soil_temperature <= 0.0 && soil_temperature > -50.0 {
		temp_on_decomposition = 0.0
	} else {
		so.error_message = "irregular soil temperature"
	}

	return temp_on_decomposition
}

// C++: double monica::soilorganic::foTempOnDecompostion(SoilOrganic*, double)
soil_organic_fo_temp_on_decompostion :: proc(so: ^Soil_Organic, d_soil_temperature: f64) -> f64 {
	fo_temp_on_decompostion := 0.0

	if d_soil_temperature <= 0.0 && d_soil_temperature > -40.0 {
		fo_temp_on_decompostion = 0.0
	} else if d_soil_temperature > 0.0 && d_soil_temperature <= 20.0 {
		fo_temp_on_decompostion = 0.1 * d_soil_temperature
	} else if d_soil_temperature > 20.0 && d_soil_temperature <= 70.0 {
		fo_temp_on_decompostion = cr_math.exp(
			0.47 - (0.027 * d_soil_temperature) + (0.00193 * d_soil_temperature * d_soil_temperature),
		)
	} else {
		so.error_message = "irregular soil temperature"
	}

	return fo_temp_on_decompostion
}

// C++: double monica::soilorganic::foMoistOnDecompostionKaiteew(SoilOrganic*, double, double, double)
soil_organic_fo_moist_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	d_soil_moisture_m3, d_saturation, d_moisture_dec_optimal: f64,
) -> f64 {
	moist_on_decomposition := 0.0

	if d_soil_moisture_m3 / d_saturation >= 0.0 && d_soil_moisture_m3 / d_saturation <= 1.0 {
		moist_on_decomposition = cr_math.exp(
			-18 * cr_math.pow(d_soil_moisture_m3 / d_saturation - d_moisture_dec_optimal, 2),
		)
	} else {
		so.error_message = "irregular soil water content"
	}

	return moist_on_decomposition
}

// C++: double monica::soilorganic::foMoistOnDecompostion(SoilOrganic*, double)
soil_organic_fo_moist_on_decompostion :: proc(so: ^Soil_Organic, d_soil_moisture_pf: f64) -> f64 {
	fo_moist_on_decompostion := 0.0

	if libc.fabs(d_soil_moisture_pf) <= 1.0e-7 {
		fo_moist_on_decompostion = 0.6
	} else if d_soil_moisture_pf > 0.0 && d_soil_moisture_pf <= 1.5 {
		fo_moist_on_decompostion = 0.6 + 0.4 * (d_soil_moisture_pf / 1.5)
	} else if d_soil_moisture_pf > 1.5 && d_soil_moisture_pf <= 2.5 {
		fo_moist_on_decompostion = 1.0
	} else if d_soil_moisture_pf > 2.5 && d_soil_moisture_pf <= 6.5 {
		fo_moist_on_decompostion = 1.0 - ((d_soil_moisture_pf - 2.5) / 4.0)
	} else if d_soil_moisture_pf > 6.5 {
		fo_moist_on_decompostion = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_moist_on_decompostion
}

// C++: double monica::soilorganic::foMoistOnHydrolysis(SoilOrganic*, double)
soil_organic_fo_moist_on_hydrolysis :: proc(so: ^Soil_Organic, d_soil_moisture_pf: f64) -> f64 {
	fo_moist_on_hydrolysis := 0.0

	if d_soil_moisture_pf > 0.0 && d_soil_moisture_pf <= 1.1 {
		fo_moist_on_hydrolysis = 0.72
	} else if d_soil_moisture_pf > 1.1 && d_soil_moisture_pf <= 2.4 {
		fo_moist_on_hydrolysis = 0.2207 * d_soil_moisture_pf + 0.4672
	} else if d_soil_moisture_pf > 2.4 && d_soil_moisture_pf <= 3.4 {
		fo_moist_on_hydrolysis = 1.0
	} else if d_soil_moisture_pf > 3.4 && d_soil_moisture_pf <= 4.6 {
		fo_moist_on_hydrolysis = -0.8659 * d_soil_moisture_pf + 3.9849
	} else if d_soil_moisture_pf > 4.6 {
		fo_moist_on_hydrolysis = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_moist_on_hydrolysis
}

// C++: double monica::soilorganic::foTempOnNitrification(SoilOrganic*, double)
soil_organic_fo_temp_on_nitrification :: proc(so: ^Soil_Organic, soil_temp: f64) -> f64 {
	result := 0.0

	if soil_temp <= 2.0 && soil_temp > -40.0 {
		result = 0.0
	} else if soil_temp > 2.0 && soil_temp <= 6.0 {
		result = 0.15 * (soil_temp - 2.0)
	} else if soil_temp > 6.0 && soil_temp <= 20.0 {
		result = 0.1 * soil_temp
	} else if soil_temp > 20.0 && soil_temp <= 70.0 {
		result = cr_math.exp(0.47 - (0.027 * soil_temp) + (0.00193 * soil_temp * soil_temp))
	} else {
		so.error_message = "irregular soil temperature"
	}

	return result
}

// C++: double monica::soilorganic::foMoistOnNitrification(SoilOrganic*, double)
soil_organic_fo_moist_on_nitrification :: proc(so: ^Soil_Organic, d_soil_moisture_pf: f64) -> f64 {
	fo_moist_on_nitrification := 0.0

	if libc.fabs(d_soil_moisture_pf) <= 1.0e-7 {
		fo_moist_on_nitrification = 0.6
	} else if d_soil_moisture_pf > 0.0 && d_soil_moisture_pf <= 1.5 {
		fo_moist_on_nitrification = 0.6 + 0.4 * (d_soil_moisture_pf / 1.5)
	} else if d_soil_moisture_pf > 1.5 && d_soil_moisture_pf <= 2.5 {
		fo_moist_on_nitrification = 1.0
	} else if d_soil_moisture_pf > 2.5 && d_soil_moisture_pf <= 5.0 {
		fo_moist_on_nitrification = 1.0 - ((d_soil_moisture_pf - 2.5) / 2.5)
	} else if d_soil_moisture_pf > 5.0 {
		fo_moist_on_nitrification = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_moist_on_nitrification
}

// C++: double monica::soilorganic::foMoistOnDenitrification(SoilOrganic*, double, double)
soil_organic_fo_moist_on_denitrification :: proc(
	so: ^Soil_Organic,
	d_soil_moisture_m3, d_saturation: f64,
) -> f64 {
	denit1 := so.mod_params.denit1
	denit2 := so.mod_params.denit2
	denit3 := so.mod_params.denit3
	fo_moist_on_denitrification := 0.0

	if (d_soil_moisture_m3 / d_saturation) <= 0.8 {
		fo_moist_on_denitrification = 0.0
	} else if (d_soil_moisture_m3 / d_saturation) > 0.8 &&
	   (d_soil_moisture_m3 / d_saturation) <= 0.9 {
		fo_moist_on_denitrification =
			denit1 * ((d_soil_moisture_m3 / d_saturation) - denit2) / (denit3 - denit2)
	} else if (d_soil_moisture_m3 / d_saturation) > 0.9 &&
	   (d_soil_moisture_m3 / d_saturation) <= 1.0 {
		fo_moist_on_denitrification =
			denit1 +
			(1.0 - denit1) *
				((d_soil_moisture_m3 / d_saturation) - denit3) /
				(1.0 - denit3)
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_moist_on_denitrification
}

// C++: double monica::soilorganic::foNH3onNitriteOxidation(SoilOrganic*, double, double)
soil_organic_fo_nh3_on_nitrite_oxidation :: proc(
	so: ^Soil_Organic,
	d_soil_nh4, d_soil_ph: f64,
) -> f64 {
	inhibitor_nh3 := so.mod_params.inhibitor_nh3

	fo_nh3_on_nitrite_oxidation :=
		inhibitor_nh3 /
		(inhibitor_nh3 +
				d_soil_nh4 * (1 - 1 / (1.0 + cr_math.pow(10.0, d_soil_ph - soil.PO_PKA_NH3))))

	return fo_nh3_on_nitrite_oxidation
}

// C++: void monica::soilorganic::foVolatilisation(SoilOrganic*, bool, double, double)
//
// NH3 loss after manure/slurry application (ALFAM model, Soegaard et al.
// 2002). NOTE(c++-quirk): `auto lay0 = so->soilColumn.layers.at(0);` is a
// BY-VALUE COPY, and `vector<AOM_Properties> &AOM_Pool = lay0.vo_AOM_Pool;`
// binds into that copy. Both the vs_SoilNH4 update below and the
// vo_DaysAfterApplication increments at the end therefore never reach the
// real soil column - reproduced exactly, including deep-copying vo_AOM_Pool
// (a C++ std::vector copy is a deep copy; Odin's [dynamic]T copy is a shallow
// header copy that would otherwise alias the real layer's backing array).
soil_organic_fo_volatilisation :: proc(
	so: ^Soil_Organic,
	aom_addition: bool,
	mean_air_temperature, wind_speed: f64,
	allocator := context.allocator,
) {
	soil_wet: f64
	n_pot_volatilised_sum := 0.0
	n_act_volatilised := 0.0
	days_after_application_sum := 0

	lay0 := so.soil_column.layers[0] // by-value copy - see the doc comment above
	lay0.aom_pool = clone_aom_pool(lay0.aom_pool, allocator) // deep-copy, matching std::vector's copy ctor

	if soil_moisture_pf(&lay0) > 2.5 {
		soil_wet = 0.0
	} else {
		soil_wet = 1.0
	}

	aom_pool := lay0.aom_pool
	for props in aom_pool {
		days_after_application_sum += props.days_after_application
	}

	if days_after_application_sum > 0 || aom_addition {
		n_pot_volatilised_sum = 0.0

		for props in aom_pool {
			aom_tan_content := props.aom_nh4_content * 1000.0 * props.aom_dry_matter_content

			max_volatilisation :=
				0.0495 *
				cr_math.pow(1.1020, soil_wet) *
				cr_math.pow(1.0223, mean_air_temperature) *
				cr_math.pow(1.0417, wind_speed) *
				cr_math.pow(1.1080, props.aom_dry_matter_content) *
				cr_math.pow(0.8280, aom_tan_content) *
				cr_math.pow(f64(11.300), props.incorporation ? 1.0 : 0.0)

			volatilisation_half_life :=
				1.0380 *
				cr_math.pow(1.1020, soil_wet) *
				cr_math.pow(0.9600, mean_air_temperature) *
				cr_math.pow(0.9500, wind_speed) *
				cr_math.pow(1.1750, props.aom_dry_matter_content) *
				cr_math.pow(1.1060, aom_tan_content) *
				cr_math.pow(f64(1.0000), props.incorporation ? 1.0 : 0.0) *
				(18869.3 * cr_math.exp(-lay0.soil_ph / 0.63321) + 0.70165)

			volatilisation_rate :=
				max_volatilisation *
				(volatilisation_half_life /
						cr_math.pow(
							f64(props.days_after_application) + volatilisation_half_life,
							f64(2.0),
						))

			n_pot_volatilised :=
				volatilisation_rate *
				aom_tan_content *
				(props.aom_slow + props.aom_fast) /
				10000.0 /
				1000.0

			n_pot_volatilised_sum += n_pot_volatilised
		}

		if lay0.soil_nh4 > n_pot_volatilised_sum {
			n_act_volatilised = n_pot_volatilised_sum
		} else {
			n_act_volatilised = lay0.soil_nh4
		}

		// update NH4 content of top soil layer with volatilisation balance
		lay0.soil_nh4 -= (n_act_volatilised / lay0.layer_thickness_m)
	} else {
		n_act_volatilised = 0.0
	}

	// NH3 volatilised from top layer NH4 pool. See Urea section
	so.total_nh3_volatilised = n_act_volatilised + so.nh3_volatilised // [kg N m-2]

	for &props in aom_pool {
		if props.days_after_application > 0 && !aom_addition {
			props.days_after_application += 1
		}
	}
}

// Deep-clones an AOM_Pool ([dynamic]Aom_Properties) - a plain element-wise
// copy suffices since Aom_Properties has no nested dynamic/pointer fields.
// See soil_organic_fo_volatilisation's doc comment for why this is needed.
@(private)
clone_aom_pool :: proc(
	pool: [dynamic]Aom_Properties,
	allocator := context.allocator,
) -> [dynamic]Aom_Properties {
	cloned := make([dynamic]Aom_Properties, len(pool), allocator)
	copy(cloned[:], pool[:])
	return cloned
}

// C++: void monica::soilorganic::foNitrification(SoilOrganic*)
soil_organic_fo_nitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.number_of_organic_layers
	ammonia_oxidation_rate_coeff_standard := params.ammonia_oxidation_rate_coeff_standard
	nitrite_oxidation_rate_coeff_standard := params.nitrite_oxidation_rate_coeff_standard

	ammonia_oxidation_rate_coeff := make([dynamic]f64, nools, context.temp_allocator)
	nitrite_oxidation_rate_coeff := make([dynamic]f64, nools, context.temp_allocator)

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		nh4_i := layi.soil_nh4

		ammonia_oxidation_rate_coeff[i] =
			ammonia_oxidation_rate_coeff_standard *
			soil_organic_fo_temp_on_nitrification(so, layi.soil_temperature) *
			soil_organic_fo_moist_on_nitrification(so, soil_moisture_pf(layi))

		so.act_ammonia_oxidation_rate[i] = ammonia_oxidation_rate_coeff[i] * nh4_i

		nitrite_oxidation_rate_coeff[i] =
			nitrite_oxidation_rate_coeff_standard *
			soil_organic_fo_temp_on_nitrification(so, layi.soil_temperature) *
			soil_organic_fo_moist_on_nitrification(so, soil_moisture_pf(layi)) *
			soil_organic_fo_nh3_on_nitrite_oxidation(so, nh4_i, layi.soil_ph)

		so.act_nitrification_rate[i] = nitrite_oxidation_rate_coeff[i] * layi.soil_no2

		// Update NH4, NO2 and NO3 content with nitrification balance
		// Stange, F., C. Nendel (2014): N.N., in preparation
		if nh4_i > so.act_ammonia_oxidation_rate[i] {
			layi.soil_nh4 -= so.act_ammonia_oxidation_rate[i]
			layi.soil_no2 += so.act_ammonia_oxidation_rate[i]
		} else {
			layi.soil_no2 += nh4_i
			layi.soil_nh4 = 0.0
		}

		if layi.soil_no2 > so.act_nitrification_rate[i] {
			layi.soil_no2 -= so.act_nitrification_rate[i]
			layi.soil_no3 += so.act_nitrification_rate[i]
		} else {
			layi.soil_no3 += layi.soil_no2
			layi.soil_no2 = 0.0
		}
	}
}

// C++: void monica::soilorganic::foSticsNitrification(SoilOrganic*)
soil_organic_fo_stics_nitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	stics_params := &so.mod_params.stics_params

	nools := sc.number_of_organic_layers

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.soil_moisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		nh4_i := layi.soil_nh4

		kg_n_per_m3_to_mg_n_per_kg := 1000.0 * 1000.0 / sbdi
		mg_n_per_kg_to_kg_n_per_m3 := 1 / kg_n_per_m3_to_mg_n_per_kg

		so.act_nitrification_rate[i] = stics_vnit(
				stics_params,
				nh4_i * kg_n_per_m3_to_mg_n_per_kg, // kg-NH4-N/m3-soil -> mg-NH4-N/kg-soil
				layi.soil_ph,
				layi.soil_temperature,
				smi / layi.saturation, // soil water-filled pore space []
				smi * 1000 / sbdi, // gravimetric soil water content kg-water/kg-soil
				layi.field_capacity,
				layi.saturation,
			) * mg_n_per_kg_to_kg_n_per_m3 // mg-N -> kg-N

		if nh4_i > so.act_nitrification_rate[i] {
			layi.soil_nh4 -= so.act_nitrification_rate[i]
			layi.soil_no3 += so.act_nitrification_rate[i]
		} else {
			layi.soil_no3 += nh4_i
			layi.soil_nh4 = 0.0
		}
	}
}

// C++: void monica::soilorganic::foDenitrification(SoilOrganic*)
soil_organic_fo_denitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.number_of_organic_layers
	pot_denitrification_rate := make([dynamic]f64, nools, context.temp_allocator)
	spec_anaerob_denitrification := params.spec_anaerob_denitrification
	transport_rate_coeff := params.transport_rate_coeff
	so.total_denitrification = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		no3_i := layi.soil_no3

		// Temperature function is the same as in Nitrification subroutine
		pot_denitrification_rate[i] =
			spec_anaerob_denitrification *
			so.smb_co2_evolution_rate[i] *
			soil_organic_fo_temp_on_nitrification(so, layi.soil_temperature)

		so.act_denitrification_rate[i] = min(
			pot_denitrification_rate[i] *
			soil_organic_fo_moist_on_denitrification(so, layi.soil_moisture_m3, layi.saturation),
			transport_rate_coeff * no3_i,
		)

		// update NO3 content of soil layer with denitrification balance [kg N m-3]
		if no3_i > so.act_denitrification_rate[i] {
			layi.soil_no3 -= so.act_denitrification_rate[i]
		} else {
			so.act_denitrification_rate[i] = no3_i
			layi.soil_no3 = 0.0
		}

		so.total_denitrification += so.act_denitrification_rate[i] * layi.layer_thickness_m // [kg m-3] --> [kg m-2]
	}

	so.sum_denitrification += so.total_denitrification // [kg N m-2]
}

// C++: void monica::soilorganic::foSticsDenitrification(SoilOrganic*)
soil_organic_fo_stics_denitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	stics_params := &so.mod_params.stics_params

	nools := sc.number_of_organic_layers
	so.total_denitrification = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.soil_moisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		lti := layi.layer_thickness_m
		no3_i := layi.soil_no3

		kg_n_per_m3_to_mg_n_per_kg := 1000.0 * 1000.0 / sbdi
		mg_n_per_kg_to_kg_n_per_m3 := 1 / kg_n_per_m3_to_mg_n_per_kg

		so.act_denitrification_rate[i] = stics_vdenit(
				stics_params,
				soil_organic_carbon(layi) * 100.0, // kg-C/kg-soil = % [0-1] -> % [0-100]
				no3_i * kg_n_per_m3_to_mg_n_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
				layi.soil_temperature,
				smi / layi.saturation, // soil water-filled pore space []
				smi * 1000 / sbdi, // gravimetric soil water content kg-water/kg-soil
			) * mg_n_per_kg_to_kg_n_per_m3 // mg-N -> kg-N

		// update NO3 content of soil layer with denitrification balance [kg N m-3]
		if no3_i > so.act_denitrification_rate[i] {
			layi.soil_no3 -= so.act_denitrification_rate[i]
		} else {
			so.act_denitrification_rate[i] = no3_i
			layi.soil_no3 = 0.0
		}
		so.total_denitrification += so.act_denitrification_rate[i] * lti // [kg m-3] --> [kg m-2]
	}

	so.sum_denitrification += so.total_denitrification // [kg N m-2]
}

// C++: double monica::soilorganic::foN2OProduction(SoilOrganic*)
soil_organic_fo_n2o_production :: proc(so: ^Soil_Organic) -> f64 {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.number_of_organic_layers
	n2o_production_rate := params.n2o_production_rate
	pka_hno2 := soil.PO_PKA_HNO2
	sum_n2o_produced := 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		ph_i := layi.soil_ph
		no2_i := layi.soil_no2
		lti := layi.layer_thickness_m
		tempi := layi.soil_temperature

		// pKaHNO2 original concept pow10. We used pow2 to allow reactive HNO2
		// being available at higher pH values
		ph_response := 1.0 / (1.0 + cr_math.pow(2.0, ph_i - pka_hno2))

		n2o_production_at_layer :=
			no2_i *
			soil_organic_fo_temp_on_nitrification(so, tempi) *
			n2o_production_rate *
			ph_response *
			lti *
			10000 // convert from kg N-N2O m-3 to kg N-N2O ha-1 (for each layer)

		sum_n2o_produced += n2o_production_at_layer
	}

	return sum_n2o_produced
}

// C++: SoilOrganic::NitDenitN2O monica::soilorganic::foSticsN2OProduction(SoilOrganic*)
//
// Returns (sumN2OProducedNit, sumN2OProducedDenit).
soil_organic_fo_stics_n2o_production :: proc(so: ^Soil_Organic) -> (f64, f64) {
	sc := so.soil_column
	stics_params := &so.mod_params.stics_params

	nools := sc.number_of_organic_layers
	sum_n2o_produced_nit := 0.0
	sum_n2o_produced_denit := 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.soil_moisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		lti := layi.layer_thickness_m

		kg_n_per_m3_to_mg_n_per_kg := 1000.0 * 1000.0 / sbdi
		mg_n_per_kg_to_kg_n_per_m3 := 1 / kg_n_per_m3_to_mg_n_per_kg

		stics_to_monica_units := mg_n_per_kg_to_kg_n_per_m3 * lti * 10000.0 // /kg-soil -> /m3-soil// /m3-soil -> /m2-soil// /m2-soil -> /ha-soil

		n2o_nit, n2o_denit := stics_n2o(
			stics_params,
			layi.soil_no3 * kg_n_per_m3_to_mg_n_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
			smi / layi.saturation, // soil water-filled pore space []
			layi.soil_ph,
			so.act_nitrification_rate[i] * kg_n_per_m3_to_mg_n_per_kg, // nitrification rate [mg-N/kg-soil/day]
			so.act_denitrification_rate[i] * kg_n_per_m3_to_mg_n_per_kg, // denitrification rate [mg-N/kg-soil/day]
		)

		sum_n2o_produced_nit += n2o_nit * stics_to_monica_units
		sum_n2o_produced_denit += n2o_denit * stics_to_monica_units
	}

	return sum_n2o_produced_nit, sum_n2o_produced_denit
}

// C++: void monica::soilorganic::foPoolUpdate(SoilOrganic*)
soil_organic_fo_pool_update :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	nools := sc.number_of_organic_layers

	for i in 0 ..< nools {
		layi := &sc.layers[i]

		so.aom_slow_delta_sum[i] = 0.0
		so.aom_fast_delta_sum[i] = 0.0
		so.aom_slow_sum[i] = 0.0
		so.aom_fast_sum[i] = 0.0

		for &pool in layi.aom_pool {
			pool.aom_slow += pool.aom_slow_delta
			pool.aom_fast += pool.aom_fast_delta

			so.aom_slow_delta_sum[i] += pool.aom_slow_delta
			so.aom_fast_delta_sum[i] += pool.aom_fast_delta

			so.aom_slow_sum[i] += pool.aom_slow
			so.aom_fast_sum[i] += pool.aom_fast
		}

		layi.som_slow += so.som_slow_delta[i]
		layi.som_fast += so.som_fast_delta[i]
		layi.smb_slow += so.smb_slow_delta[i]
		layi.smb_fast += so.smb_fast_delta[i]

		so.c_balance[i] =
			so.aom_slow_input[i] +
			so.aom_fast_input[i] +
			so.aom_slow_delta_sum[i] +
			so.aom_fast_delta_sum[i] +
			so.smb_slow_delta[i] +
			so.smb_fast_delta[i] +
			so.som_slow_delta[i] +
			so.som_fast_delta[i] +
			so.som_fast_input[i]

		// ([kg C kg-1] * [kg m-3]) - [kg C m-3]
		so.soil_organic_c[i] =
			(soil_organic_carbon(layi) * soil_bulk_density(layi)) - so.inert_soil_organic_c[i]
		so.soil_organic_c[i] += so.c_balance[i]

		// [kg C m-3] / [kg m-3] --> [kg C kg-1]
		layi.soil_organic_carbon =
			(so.soil_organic_c[i] + so.inert_soil_organic_c[i]) / soil_bulk_density(layi)
	}
}

// C++: void monica::soilorganic::addOrganicMatter(SoilOrganic*, const
//        OrganicMatterParameters&, const map<size_t,double>&, double)
//
// Not exercised by this checkpoint's oracle (no fertilisation/incorporation
// workstep drives the test fixtures - those are phase 6). Ported for
// completeness. layer2addedOrganicMatterAmount is iterated in ascending key
// order to match C++ std::map's sorted iteration (Odin map iteration order is
// unspecified) - same technique odin/monica/trace/trace.odin's
// dump_map_int_f64 already uses.
soil_organic_add_organic_matter :: proc(
	so: ^Soil_Organic,
	params: ^p.Organic_Matter_Parameters,
	layer_to_added_organic_matter_amount: map[int]f64,
	added_organic_matter_n_concentration: f64 = 0,
	allocator := context.allocator,
) {
	sc := so.soil_column
	nools := sc.number_of_organic_layers
	layer_thickness := sc.layers[0].layer_thickness_m

	are_crop_residue_params := int(params.cn_ratio_aom_fast * 10000.0) == 0

	calc_cn_ratio_aom_fast_and_added_corg_amount :: proc(
		params: ^p.Organic_Matter_Parameters,
		so: ^Soil_Organic,
		layer_thickness: f64,
		added_organic_matter_amount, added_organic_matter_n_concentration: f64,
	) -> (
		cn_ratio_aom_fast: f64,
		added_corg_amount: f64,
		added_norg_amount: f64,
	) {
		added_corg_amount =
			(params.corg_content <= 0.0 ? soil.PO_AOM_TO_C : params.corg_content) *
			added_organic_matter_amount *
			params.aom_dry_matter_content /
			10000.0 /
			layer_thickness

		added_norg_amount =
			added_organic_matter_n_concentration <= 0.0 ? 0.01 : (added_organic_matter_amount * params.aom_dry_matter_content * added_organic_matter_n_concentration / 10000.0 / layer_thickness)

		n_for_aom_slow :=
			added_corg_amount * params.part_aom_to_aom_slow / params.cn_ratio_aom_slow
		if n_for_aom_slow < added_norg_amount {
			n_for_aom_fast := added_norg_amount - n_for_aom_slow
			cn_ratio_aom_fast = added_corg_amount * params.part_aom_to_aom_fast / n_for_aom_fast
		} else {
			cn_ratio_aom_fast = so.mod_params.aom_fast_max_c_to_n
		}

		cn_ratio_aom_fast = min(cn_ratio_aom_fast, so.mod_params.aom_fast_max_c_to_n)
		return
	}

	rounded_aom_slow_dec_coeff_standard := tl.round_shifted_int(params.aom_slow_dec_coeff_standard, 4)
	rounded_aom_fast_dec_coeff_standard := tl.round_shifted_int(params.aom_fast_dec_coeff_standard, 4)
	rounded_part_aom_slow_to_smb_slow := tl.round_shifted_int(params.part_aom_slow_to_smb_slow, 4)
	rounded_part_aom_slow_to_smb_fast := tl.round_shifted_int(params.part_aom_slow_to_smb_fast, 4)
	rounded_cn_ratio_aom_slow := tl.round_shifted_int(params.cn_ratio_aom_slow, 4)

	are_same_aom_props_as_om_params :: proc(
		props: ^Aom_Properties,
		rounded_aom_slow_dec_coeff_standard,
		rounded_aom_fast_dec_coeff_standard,
		rounded_part_aom_slow_to_smb_slow,
		rounded_part_aom_slow_to_smb_fast,
		rounded_cn_ratio_aom_slow: int,
	) -> bool {
		return(
			tl.round_shifted_int(props.aom_slow_dec_coeff_standard, 4) ==
				rounded_aom_slow_dec_coeff_standard &&
			tl.round_shifted_int(props.aom_fast_dec_coeff_standard, 4) ==
				rounded_aom_fast_dec_coeff_standard &&
			tl.round_shifted_int(props.part_aom_slow_to_smb_slow, 4) ==
				rounded_part_aom_slow_to_smb_slow &&
			tl.round_shifted_int(props.part_aom_slow_to_smb_fast, 4) ==
				rounded_part_aom_slow_to_smb_fast &&
			tl.round_shifted_int(props.cn_ratio_aom_slow, 4) == rounded_cn_ratio_aom_slow \
		)
	}

	// sorted keys, matching std::map's iteration order
	keys := make([dynamic]int, 0, len(layer_to_added_organic_matter_amount), allocator)
	for k in layer_to_added_organic_matter_amount {
		append(&keys, k)
	}
	for i in 1 ..< len(keys) {
		k := keys[i]
		j := i - 1
		for j >= 0 && keys[j] > k {
			keys[j + 1] = keys[j]
			j -= 1
		}
		keys[j + 1] = k
	}

	if nools > 0 {
		for k in keys {
			v := layer_to_added_organic_matter_amount[k]
			if k < nools {
				sc.layers[k].soil_carbamid +=
					v *
					params.aom_dry_matter_content *
					params.aom_carbamid_content /
					10000.0 /
					layer_thickness
			}
		}
	}

	pool_set_index := -1
	if are_crop_residue_params {
		i := 0
		for &props in sc.layers[0].aom_pool {
			if are_same_aom_props_as_om_params(
				&props,
				rounded_aom_slow_dec_coeff_standard,
				rounded_aom_fast_dec_coeff_standard,
				rounded_part_aom_slow_to_smb_slow,
				rounded_part_aom_slow_to_smb_fast,
				rounded_cn_ratio_aom_slow,
			) {
				pool_set_index = i
				break
			}
			i += 1
		}
	}

	for into_layer_index in keys {
		added_organic_matter_amount := layer_to_added_organic_matter_amount[into_layer_index]
		if into_layer_index >= len(sc.layers) {
			continue
		}
		into_layer := &sc.layers[into_layer_index]

		calced_cn_ratio_aom_fast, added_corg_amount, _ :=
			calc_cn_ratio_aom_fast_and_added_corg_amount(
				params,
				so,
				layer_thickness,
				added_organic_matter_amount,
				added_organic_matter_n_concentration,
			)

		aom_slow_input := 0.0
		aom_fast_input := 0.0

		if pool_set_index < 0 {
			pool: Aom_Properties
			pool.aom_slow_dec_coeff_standard = params.aom_slow_dec_coeff_standard
			pool.aom_fast_dec_coeff_standard = params.aom_fast_dec_coeff_standard
			pool.cn_ratio_aom_slow = params.cn_ratio_aom_slow
			pool.cn_ratio_aom_fast =
				are_crop_residue_params ? calced_cn_ratio_aom_fast : params.cn_ratio_aom_fast
			pool.part_aom_slow_to_smb_slow = params.part_aom_slow_to_smb_slow
			pool.part_aom_slow_to_smb_fast = params.part_aom_slow_to_smb_fast
			pool.incorporation = so.incorporation
			pool.no_volatilization = are_crop_residue_params

			for i in 0 ..< nools {
				append(&sc.layers[i].aom_pool, pool)

				if i == into_layer_index {
					cpool := &into_layer.aom_pool[len(into_layer.aom_pool) - 1]
					cpool.days_after_application = 1
					cpool.aom_dry_matter_content = params.aom_dry_matter_content
					cpool.aom_nh4_content = params.aom_nh4_content
					cpool.aom_slow = params.part_aom_to_aom_slow * added_corg_amount
					aom_slow_input = cpool.aom_slow
					cpool.aom_fast = params.part_aom_to_aom_fast * added_corg_amount
					aom_fast_input = cpool.aom_fast
				}
			}

			pool_set_index = len(sc.layers[0].aom_pool) - 1
		} else {
			cpool := &into_layer.aom_pool[pool_set_index]
			aom_slow_input = params.part_aom_to_aom_slow * added_corg_amount
			cpool.aom_slow += aom_slow_input
			added_cn_ratio_aom_fast :=
				are_crop_residue_params ? calced_cn_ratio_aom_fast : params.cn_ratio_aom_fast
			pool_fast_n := cpool.aom_fast / cpool.cn_ratio_aom_fast
			added_fast_n :=
				params.part_aom_to_aom_fast * added_corg_amount / added_cn_ratio_aom_fast
			aom_fast_input = params.part_aom_to_aom_fast * added_corg_amount
			cpool.aom_fast += aom_fast_input
			new_cn_ratio_aom_fast := cpool.aom_fast / (pool_fast_n + added_fast_n)
			cpool.cn_ratio_aom_fast = new_cn_ratio_aom_fast
		}

		soil_nh4_input :=
			params.aom_nh4_content *
			added_organic_matter_amount *
			params.aom_dry_matter_content /
			10000.0 /
			layer_thickness

		soil_no3_input :=
			params.aom_no3_content *
			added_organic_matter_amount *
			params.aom_dry_matter_content /
			10000.0 /
			layer_thickness

		som_fast_input :=
			max(0.0, (1.0 - (params.part_aom_to_aom_slow + params.part_aom_to_aom_fast))) *
			added_corg_amount

		into_layer.soil_nh4 += soil_nh4_input
		into_layer.soil_no3 += soil_no3_input
		into_layer.som_fast += som_fast_input

		so.aom_slow_input[into_layer_index] += aom_slow_input
		so.aom_fast_input[into_layer_index] += aom_fast_input
		so.som_fast_input[into_layer_index] += som_fast_input
	}

	so.added_organic_matter = true
}

// C++: void monica::soilorganic::addOrganicMatter(SoilOrganic*, const
//        OrganicMatterParameters&, double, double, size_t)
soil_organic_add_organic_matter_amount :: proc(
	so: ^Soil_Organic,
	params: ^p.Organic_Matter_Parameters,
	amount: f64,
	n_concentration: f64 = 0,
	into_layer_index: int = 0,
	allocator := context.allocator,
) {
	m := make(map[int]f64, 1, allocator)
	m[into_layer_index] = amount
	soil_organic_add_organic_matter(so, params, m, n_concentration, allocator)
}

// C++: double monica::soilorganic::getOrganicN(const SoilOrganic*, int)
soil_organic_get_organic_n :: proc(so: ^Soil_Organic, i: int) -> f64 {
	org_n := 0.0

	org_n += so.soil_column.layers[i].smb_fast / so.mod_params.cn_ratio_smb
	org_n += so.soil_column.layers[i].smb_slow / so.mod_params.cn_ratio_smb

	cn := so.soil_column.layers[i].soil_cn_ratio
	org_n += so.soil_column.layers[i].som_fast / cn
	org_n += so.soil_column.layers[i].som_slow / cn

	for aomp in so.soil_column.layers[i].aom_pool {
		org_n += aomp.aom_fast / aomp.cn_ratio_aom_fast
		org_n += aomp.aom_slow / aomp.cn_ratio_aom_slow
	}

	return org_n
}

// C++: double monica::soilorganic::getSoilOrganicC(const SoilOrganic*, int)
soil_organic_get_soil_organic_c :: proc(so: ^Soil_Organic, i_layer: int) -> f64 {
	return so.soil_organic_c[i_layer] / soil_bulk_density(&so.soil_column.layers[i_layer])
}

// C++: double monica::soilorganic::getNetNMineralisationRate(const SoilOrganic*, int)
soil_organic_get_net_n_mineralisation_rate :: proc(so: ^Soil_Organic, i_layer: int) -> f64 {
	return so.net_n_mineralisation_rate[i_layer] * 10000.0
}

// C++: double monica::soilorganic::getNH3_Volatilised(const SoilOrganic*)
soil_organic_get_nh3_volatilised :: proc(so: ^Soil_Organic) -> f64 {
	return so.total_nh3_volatilised * 10000.0
}

// C++: double monica::soilorganic::getSumNH3_Volatilised(const SoilOrganic*)
soil_organic_get_sum_nh3_volatilised :: proc(so: ^Soil_Organic) -> f64 {
	return so.sum_nh3_volatilised * 10000.0
}

// C++: double monica::soilorganic::getSumN2O_Produced(const SoilOrganic*)
soil_organic_get_sum_n2o_produced :: proc(so: ^Soil_Organic) -> f64 {
	return so.sum_n2o_produced
}

// C++: double monica::soilorganic::getNetNMineralisation(const SoilOrganic*)
soil_organic_get_net_n_mineralisation :: proc(so: ^Soil_Organic) -> f64 {
	return so.net_n_mineralisation * 10000.0
}

// C++: double monica::soilorganic::getSumNetNMineralisation(const SoilOrganic*)
soil_organic_get_sum_net_n_mineralisation :: proc(so: ^Soil_Organic) -> f64 {
	return so.sum_net_n_mineralisation * 10000.0
}

// C++: double monica::soilorganic::getSumDenitrification(const SoilOrganic*)
soil_organic_get_sum_denitrification :: proc(so: ^Soil_Organic) -> f64 {
	return so.sum_denitrification * 10000.0
}

// C++: double monica::soilorganic::getDenitrification(const SoilOrganic*)
soil_organic_get_denitrification :: proc(so: ^Soil_Organic) -> f64 {
	return so.total_denitrification * 10000.0
}

// C++: double monica::soilorganic::getDecomposerRespiration(const SoilOrganic*)
soil_organic_get_decomposer_respiration :: proc(so: ^Soil_Organic) -> f64 {
	return so.decomposer_respiration * 10000.0
}

// C++: double monica::soilorganic::foNetEcosystemProduction(SoilOrganic*, double, double)
//
// Net ecosystem production [kg C ha-1 d-1].
soil_organic_fo_net_ecosystem_production :: proc(
	so: ^Soil_Organic,
	d_net_primary_production, d_decomposer_respiration: f64,
) -> f64 {
	return d_net_primary_production - (d_decomposer_respiration * 10000.0) // [kg C ha-1 d-1]
}

// C++: double monica::soilorganic::foNetEcosystemExchange(SoilOrganic*, double, double)
//
// Net ecosystem exchange [kg C ha-1 d-1]. NEE = NEP (M.U.F. Kirschbaum and R.
// Mueller (2001)). Per definition NPP is negative and respiration is positive.
soil_organic_fo_net_ecosystem_exchange :: proc(
	so: ^Soil_Organic,
	d_net_primary_production, d_decomposer_respiration: f64,
) -> f64 {
	return -d_net_primary_production + (d_decomposer_respiration * 10000.0) // [kg C ha-1 d-1]
}

// C++: void monica::soilorganic::step(SoilOrganic*, double, double, double)
soil_organic_step :: proc(so: ^Soil_Organic, mean_air_temperature, precipitation, wind_speed: f64) {
	// C++: `so->cropModule ? so->cropModule->vc_NetPrimaryProduction : 0` - real
	// SoilOrganic struct field, no monica-back-pointer deviation needed here.
	net_primary_production := so.crop_module != nil ? so.crop_module.net_primary_production : 0

	soil_organic_fo_urea(so)
	soil_organic_fo_mit(so)
	soil_organic_fo_volatilisation(so, so.added_organic_matter, mean_air_temperature, wind_speed)

	if so.mod_params.stics_params.use_nit {
		soil_organic_fo_stics_nitrification(so)
	} else {
		soil_organic_fo_nitrification(so)
	}

	if so.mod_params.stics_params.use_denit {
		soil_organic_fo_stics_denitrification(so)
	} else {
		soil_organic_fo_denitrification(so)
	}

	n2o_nit, n2o_denit: f64
	if so.mod_params.stics_params.use_n2o {
		n2o_nit, n2o_denit = soil_organic_fo_stics_n2o_production(so)
	} else {
		n2o_nit, n2o_denit = soil_organic_fo_n2o_production(so), 0.0
	}
	so.n2o_produced_nit = n2o_nit
	so.n2o_produced_denit = n2o_denit
	so.n2o_produced = so.n2o_produced_nit + so.n2o_produced_denit

	soil_organic_fo_pool_update(so)

	so.net_ecosystem_production = soil_organic_fo_net_ecosystem_production(
		so,
		net_primary_production,
		so.decomposer_respiration,
	)
	so.net_ecosystem_exchange = soil_organic_fo_net_ecosystem_exchange(
		so,
		net_primary_production,
		so.decomposer_respiration,
	)

	so.sum_nh3_volatilised += so.nh3_volatilised
	so.sum_n2o_produced += so.n2o_produced

	so.irrigation_amount = 0.0

	nools := so.soil_column.number_of_organic_layers
	for i in 0 ..< nools {
		so.aom_slow_input[i] = 0.0
		so.aom_fast_input[i] = 0.0
		so.som_fast_input[i] = 0.0
	}
	so.added_organic_matter = false
}
