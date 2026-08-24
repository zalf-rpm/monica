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

// C++: struct monica::SoilOrganic
Soil_Organic :: struct {
	soil_column:                  ^Soil_Column,
	mod_params:                   p.Soil_Organic_Module_Parameters,
	added_organic_matter:         bool,
	irrigation_amount:            f64,
	act_ammonia_oxidation_rate:   [dynamic]f64,
	act_nitrification_rate:       [dynamic]f64,
	act_denitrification_rate:     [dynamic]f64,
	aom_fast_delta_sum:           [dynamic]f64,
	aom_fast_input:               [dynamic]f64,
	aom_fast_sum:                 [dynamic]f64,
	aom_slow_delta_sum:           [dynamic]f64,
	aom_slow_input:               [dynamic]f64,
	aom_slow_sum:                 [dynamic]f64,
	c_balance:                    [dynamic]f64,
	decomposer_respiration:       f64,
	error_message:                string,
	inert_soil_organic_c:         [dynamic]f64,
	inert_soil_organic_c_high_cn: [dynamic]f64,
	n2o_produced:                 f64, // [kg-N2O-N/ha]
	n2o_produced_nit:             f64, // [kg-N2O-N/ha]
	n2o_produced_denit:           f64, // [kg-N2O-N/ha]
	net_ecosystem_exchange:       f64,
	net_ecosystem_production:     f64,
	net_n_mineralisation:         f64,
	net_n_mineralisation_rate:    [dynamic]f64,
	total_nh3_volatilised:        f64,
	nh3_volatilised:              f64,
	smb_co2_evolution_rate:       [dynamic]f64,
	smb_fast_delta:               [dynamic]f64,
	smb_slow_delta:               [dynamic]f64,
	vs_SoilMineralNContent:       [dynamic]f64,
	soil_organic_c:               [dynamic]f64,
	soil_organic_c_high_cn:       [dynamic]f64,
	som_fast_delta:               [dynamic]f64,
	som_fast_input:               [dynamic]f64,
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
	nools := sc.vs_NumberOfOrganicLayers
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

	po_PartSOM_to_SMB_Slow := so.mod_params.po_PartSOM_to_SMB_Slow
	po_PartSOM_to_SMB_Fast := so.mod_params.po_PartSOM_to_SMB_Fast
	po_SOM_SlowDecCoeffStandard := so.mod_params.po_SOM_SlowDecCoeffStandard
	po_SOM_FastDecCoeffStandard := so.mod_params.po_SOM_FastDecCoeffStandard
	po_PartSOM_Fast_to_SOM_Slow := so.mod_params.po_PartSOM_Fast_to_SOM_Slow
	po_inert_CN_lower_limit := 11.0
	po_inert_CN_upper_limit := 350.0

	for i in 0 ..< sc.vs_NumberOfOrganicLayers {
		layer := &sc.layers[i]
		layi := &sc.layers[i]

		so.soil_organic_c[i] = soil_organic_carbon(layer) * soil_bulk_density(layer)

		if layi.vs_Soil_CN_Ratio > 100 {
			so.inert_soil_organic_c_high_cn[i] =
				(so.soil_organic_c[i] * layer.vs_LayerThickness / 1000 * 10000.0) /
				layi.vs_Soil_CN_Ratio *
				(po_inert_CN_lower_limit - layi.vs_Soil_CN_Ratio) /
				(po_inert_CN_lower_limit / po_inert_CN_upper_limit - 1) /
				10000.0 *
				1000.0 /
				layer.vs_LayerThickness

			so.soil_organic_c_high_cn[i] =
				so.soil_organic_c[i] - so.inert_soil_organic_c_high_cn[i]

			so.inert_soil_organic_c[i] =
				(0.049 *
					libc.pow(
						so.soil_organic_c_high_cn[i] * layer.vs_LayerThickness / 1000 * 10000.0,
						1.139,
					)) /
				10000.0 *
				1000.0 /
				layer.vs_LayerThickness

			so.inert_soil_organic_c[i] =
				so.inert_soil_organic_c[i] + so.inert_soil_organic_c_high_cn[i]
		} else {
			so.inert_soil_organic_c[i] =
				(0.049 *
					libc.pow(
						so.soil_organic_c[i] * layer.vs_LayerThickness / 1000 * 10000.0,
						1.139,
					)) /
				10000.0 *
				1000.0 /
				layer.vs_LayerThickness
		}
		so.soil_organic_c[i] -= so.inert_soil_organic_c[i]

		layer.vs_SMB_Slow = po_PartSOM_to_SMB_Slow * so.soil_organic_c[i]
		layer.vs_SMB_Fast = po_PartSOM_to_SMB_Fast * so.soil_organic_c[i]

		layer.vs_SOM_Slow =
			so.soil_organic_c[i] /
			(1.0 +
					po_SOM_SlowDecCoeffStandard /
						(po_SOM_FastDecCoeffStandard * po_PartSOM_Fast_to_SOM_Slow))
		layer.vs_SOM_Fast = so.soil_organic_c[i] - layer.vs_SOM_Slow
		so.soil_organic_c[i] -= layer.vs_SMB_Slow + layer.vs_SMB_Fast

		layer.vs_SoilOrganicCarbon =
			(so.soil_organic_c[i] + so.inert_soil_organic_c[i]) / soil_bulk_density(layer)

		so.act_denitrification_rate[i] = 0.0
	}

	return so
}

// C++: void monica::soilorganic::foUrea(SoilOrganic*)
soil_organic_fo_urea :: proc(so: ^Soil_Organic, allocator := context.allocator) {
	sc := so.soil_column
	nools := sc.vs_NumberOfOrganicLayers
	vo_SoilCarbamid_solid := make([dynamic]f64, nools, allocator)
	vo_SoilCarbamid_aq := make([dynamic]f64, nools, allocator)
	vo_HydrolysisRate1 := make([dynamic]f64, nools, allocator)
	vo_HydrolysisRate2 := make([dynamic]f64, nools, allocator)
	vo_HydrolysisRateMax := make([dynamic]f64, nools, allocator)
	vo_Hydrolysis_pH_Effect := make([dynamic]f64, nools, allocator)
	vo_HydrolysisRate := make([dynamic]f64, nools, allocator)
	vo_H3OIonConcentration := 0.0
	vo_NH3aq_EquilibriumConst := 0.0
	vo_NH3_EquilibriumConst := 0.0
	vs_SoilNH4aq := 0.0
	vo_NH3aq := 0.0
	vo_NH3gas := 0.0
	vo_NH3_Volatilising := 0.0

	po_HydrolysisKM := so.mod_params.po_HydrolysisKM
	po_HydrolysisP1 := so.mod_params.po_HydrolysisP1
	po_HydrolysisP2 := so.mod_params.po_HydrolysisP2
	po_ActivationEnergy := so.mod_params.po_ActivationEnergy

	so.nh3_volatilised = 0.0

	for i in 0 ..< nools {
		layer := &sc.layers[i]

		// kmol urea m-3 soil
		vo_SoilCarbamid_solid[i] =
			layer.vs_SoilCarbamid / soil.PO_UREA_MOLECULAR_WEIGHT / soil.PO_UREA_TO_N / 1000.0

		// mol urea kg Solution-1
		vo_SoilCarbamid_aq[i] =
			(-1258.9 +
				13.2843 * (layer.vs_SoilTemperature + 273.15) -
				0.047381 *
					((layer.vs_SoilTemperature + 273.15) * (layer.vs_SoilTemperature + 273.15)) +
				5.77264e-5 * libc.pow(layer.vs_SoilTemperature + 273.15, 3.0))

		// kmol urea m-3 soil
		vo_SoilCarbamid_aq[i] =
			(vo_SoilCarbamid_aq[i] / (1.0 + (vo_SoilCarbamid_aq[i] * 0.0453))) *
			layer.vs_SoilMoisture_m3

		if vo_SoilCarbamid_aq[i] >= vo_SoilCarbamid_solid[i] {
			vo_SoilCarbamid_aq[i] = vo_SoilCarbamid_solid[i]
			vo_SoilCarbamid_solid[i] = 0.0
		} else {
			vo_SoilCarbamid_solid[i] -= vo_SoilCarbamid_aq[i]
		}

		// Calculate urea hydrolysis
		vo_HydrolysisRate1[i] =
			(po_HydrolysisP1 * (soil_organic_matter(layer) * 100.0) * soil.PO_SOM_TO_C +
				po_HydrolysisP2) /
			soil.PO_UREA_MOLECULAR_WEIGHT

		vo_HydrolysisRate2[i] =
			vo_HydrolysisRate1[i] / libc.exp(-po_ActivationEnergy / (8.314 * 310.0))

		vo_HydrolysisRateMax[i] =
			vo_HydrolysisRate2[i] *
			libc.exp(-po_ActivationEnergy / (8.314 * (layer.vs_SoilTemperature + 273.15)))

		vo_Hydrolysis_pH_Effect[i] = libc.exp(
			-0.064 * ((layer.vs_SoilpH - 6.5) * (layer.vs_SoilpH - 6.5)),
		)

		// kmol urea kg soil-1 s-1
		vo_HydrolysisRate[i] =
			vo_HydrolysisRateMax[i] *
			soil_organic_fo_moist_on_hydrolysis(so, soil_moisture_pf(layer)) *
			vo_Hydrolysis_pH_Effect[i] *
			vo_SoilCarbamid_aq[i] /
			(po_HydrolysisKM + vo_SoilCarbamid_aq[i])

		// kmol urea m soil-3 d-1
		vo_HydrolysisRate[i] = vo_HydrolysisRate[i] * 86400.0 * soil_bulk_density(layer)

		if vo_HydrolysisRate[i] >= vo_SoilCarbamid_aq[i] {
			layer.vs_SoilNH4 += layer.vs_SoilCarbamid
			layer.vs_SoilCarbamid = 0.0
		} else {
			// kg N m soil-3
			layer.vs_SoilCarbamid -=
				vo_HydrolysisRate[i] * soil.PO_UREA_MOLECULAR_WEIGHT * soil.PO_UREA_TO_N * 1000.0

			// kg N m soil-3
			layer.vs_SoilNH4 +=
				vo_HydrolysisRate[i] * soil.PO_UREA_MOLECULAR_WEIGHT * soil.PO_UREA_TO_N * 1000.0
		}

		// Calculate general volatilisation from NH4-Pool in top layer
		if i == 0 {
			// NOTE(c++-quirk): C++ `auto layer0 = so->soilColumn.layers.at(0);` is
			// a BY-VALUE COPY (every other loop variable in this file is `auto
			// &layi`). The vs_SoilNH4 mutation below therefore never reaches the
			// real soil column - reproduced exactly, not fixed.
			layer0 := sc.layers[0]

			vo_H3OIonConcentration = libc.pow(10.0, -layer0.vs_SoilpH) // kmol m-3
			vo_NH3aq_EquilibriumConst = libc.pow(
				10.0,
				(-2728.3 / (layer0.vs_SoilTemperature + 273.15)) - 0.094219,
			) // K2 in Sadeghi's program

			vo_NH3_EquilibriumConst = libc.pow(
				10.0,
				(1630.5 / (layer0.vs_SoilTemperature + 273.15)) - 2.301,
			) // K1 in Sadeghi's program
			_ = vo_NH3_EquilibriumConst

			// kmol m-3, assuming that all NH4 is solved
			vs_SoilNH4aq = layer0.vs_SoilNH4 / (soil.PO_NH4_MOLECULAR_WEIGHT * 1000.0)

			// kmol m-3
			vo_NH3aq = vs_SoilNH4aq / (1.0 + (vo_H3OIonConcentration / vo_NH3aq_EquilibriumConst))

			vo_NH3gas = vo_NH3aq

			// kg N m-3 d-1
			vo_NH3_Volatilising = vo_NH3gas * soil.PO_NH3_MOLECULAR_WEIGHT * 1000.0

			if vo_NH3_Volatilising >= layer0.vs_SoilNH4 {
				vo_NH3_Volatilising = layer0.vs_SoilNH4
				layer0.vs_SoilNH4 = 0.0
			} else {
				layer0.vs_SoilNH4 -= vo_NH3_Volatilising
			}

			// kg N m-2 d-1
			so.nh3_volatilised = vo_NH3_Volatilising * layer0.vs_LayerThickness
		}
	}

	// set incorporation to false, if carbamid part is falling below a treshold
	// only, if organic matter was not recently added
	if len(vo_SoilCarbamid_aq) > 0 && vo_SoilCarbamid_aq[0] < 0.001 && !so.added_organic_matter {
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

	nools := sc.vs_NumberOfOrganicLayers
	po_SOM_SlowDecCoeffStandard := params.po_SOM_SlowDecCoeffStandard
	po_SOM_FastDecCoeffStandard := params.po_SOM_FastDecCoeffStandard
	po_SMB_SlowDeathRateStandard := params.po_SMB_SlowDeathRateStandard
	po_SMB_SlowMaintRateStandard := params.po_SMB_SlowMaintRateStandard
	po_SMB_FastDeathRateStandard := params.po_SMB_FastDeathRateStandard
	po_SMB_FastMaintRateStandard := params.po_SMB_FastMaintRateStandard
	po_SOM_SlowUtilizationEfficiency := params.po_SOM_SlowUtilizationEfficiency
	po_SOM_FastUtilizationEfficiency := params.po_SOM_FastUtilizationEfficiency
	po_PartSOM_Fast_to_SOM_Slow := params.po_PartSOM_Fast_to_SOM_Slow
	po_PartSMB_Slow_to_SOM_Fast := params.po_PartSMB_Slow_to_SOM_Fast
	po_PartSMB_Fast_to_SOM_Fast := params.po_PartSMB_Fast_to_SOM_Fast
	po_SMB_UtilizationEfficiency := params.po_SMB_UtilizationEfficiency
	po_CN_Ratio_SMB := params.po_CN_Ratio_SMB
	po_AOM_SlowUtilizationEfficiency := params.po_AOM_SlowUtilizationEfficiency
	po_AOM_FastUtilizationEfficiency := params.po_AOM_FastUtilizationEfficiency
	po_ImmobilisationRateCoeffNH4 := params.po_ImmobilisationRateCoeffNH4
	po_ImmobilisationRateCoeffNO3 := params.po_ImmobilisationRateCoeffNO3

	AOMslow_to_SMBfast := make([dynamic]f64, nools, allocator)
	AOMslow_to_SMBslow := make([dynamic]f64, nools, allocator)
	AOMfast_to_SMBfast := make([dynamic]f64, nools, allocator)

	// Sum of decomposition rates for fast added organic matter pools
	vo_AOM_FastDecRateSum := make([dynamic]f64, nools, allocator)
	// Sum of all changes to added organic matter fast pool [kg C m-3]
	vo_AOM_FastDeltaSum := make([dynamic]f64, nools, allocator)
	// Sum of decomposition rates for slow added organic matter pools
	vo_AOM_SlowDecRateSum := make([dynamic]f64, nools, allocator)
	// Sum of all changes to added organic matter slow pool [kg C m-3]
	vo_AOM_SlowDeltaSum := make([dynamic]f64, nools, allocator)

	// [kg m-3]
	for i in 0 ..< len(so.c_balance) {
		so.c_balance[i] = 0.0
	}

	// N balance of each layer [kg N m-3]
	vo_NBalance := make([dynamic]f64, nools, allocator)

	vo_SMB_FastCO2EvolutionRate := make([dynamic]f64, nools, allocator)
	vo_SMB_FastDeathRate := make([dynamic]f64, nools, allocator)
	vo_SMB_FastDeathRateCoeff := make([dynamic]f64, nools, allocator)
	vo_SMB_FastDecRate := make([dynamic]f64, nools, allocator)
	vo_SMB_FastMaintRateCoeff := make([dynamic]f64, nools, allocator)
	vo_SMB_FastMaintRate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.smb_fast_delta) {
		so.smb_fast_delta[i] = 0.0
	}

	vo_SMB_SlowCO2EvolutionRate := make([dynamic]f64, nools, allocator)
	vo_SMB_SlowDeathRate := make([dynamic]f64, nools, allocator)
	vo_SMB_SlowDeathRateCoeff := make([dynamic]f64, nools, allocator)
	vo_SMB_SlowDecRate := make([dynamic]f64, nools, allocator)
	vo_SMB_SlowMaintRateCoeff := make([dynamic]f64, nools, allocator)
	vo_SMB_SlowMaintRate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.smb_slow_delta) {
		so.smb_slow_delta[i] = 0.0
	}

	vo_SOM_FastDecCoeff := make([dynamic]f64, nools, allocator)
	vo_SOM_FastDecRate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.som_fast_delta) {
		so.som_fast_delta[i] = 0.0
	}

	vo_SOM_SlowDecCoeff := make([dynamic]f64, nools, allocator)
	vo_SOM_SlowDecRate := make([dynamic]f64, nools, allocator)
	for i in 0 ..< len(so.som_slow_delta) {
		so.som_slow_delta[i] = 0.0
	}

	// Calculation of decay rate coefficients
	for i in 0 ..< nools {
		layi := &sc.layers[i]
		tod :=
			params.__enable_kaiteew_TempOnDecompostion__ ? soil_organic_fo_temp_on_decompostion_kaiteew(so, layi.vs_SoilTemperature, params.po_QTenFactor, params.po_TempDecOptimal) : soil_organic_fo_temp_on_decompostion(so, layi.vs_SoilTemperature) // prev code

		mod_ :=
			params.__enable_kaiteew_MoistOnDecompostion__ ? soil_organic_fo_moist_on_decompostion_kaiteew(so, layi.vs_SoilMoisture_m3, layi.vs_Saturation, params.po_MoistureDecOptimal) : soil_organic_fo_moist_on_decompostion(so, soil_moisture_pf(layi)) // prev code

		cod :=
			params.__enable_kaiteew_ClayOnDecompostion__ ? soil_organic_fo_clay_on_decompostion_kaiteew(so, layi.vs_SoilClayContent, params.po_LimitClayEffect) : soil_organic_fo_clay_on_decompostion(so, layi.vs_SoilClayContent, params.po_LimitClayEffect) // prev code

		vo_SOM_SlowDecCoeff[i] = po_SOM_SlowDecCoeffStandard * tod * mod_
		vo_SOM_FastDecCoeff[i] = po_SOM_FastDecCoeffStandard * tod * mod_
		vo_SOM_SlowDecRate[i] = vo_SOM_SlowDecCoeff[i] * layi.vs_SOM_Slow
		vo_SOM_FastDecRate[i] = vo_SOM_FastDecCoeff[i] * layi.vs_SOM_Fast

		vo_SMB_SlowMaintRateCoeff[i] = po_SMB_SlowMaintRateStandard * cod * tod * mod_
		vo_SMB_FastMaintRateCoeff[i] = po_SMB_FastMaintRateStandard * cod * tod * mod_

		vo_SMB_SlowMaintRate[i] = vo_SMB_SlowMaintRateCoeff[i] * layi.vs_SMB_Slow
		vo_SMB_FastMaintRate[i] = vo_SMB_FastMaintRateCoeff[i] * layi.vs_SMB_Fast
		vo_SMB_SlowDeathRateCoeff[i] = po_SMB_SlowDeathRateStandard * tod * mod_
		vo_SMB_FastDeathRateCoeff[i] = po_SMB_FastDeathRateStandard * tod * mod_
		vo_SMB_SlowDeathRate[i] = vo_SMB_SlowDeathRateCoeff[i] * layi.vs_SMB_Slow
		vo_SMB_FastDeathRate[i] = vo_SMB_FastDeathRateCoeff[i] * layi.vs_SMB_Fast

		vo_SMB_SlowDecRate[i] = vo_SMB_SlowDeathRate[i] + vo_SMB_SlowMaintRate[i]
		vo_SMB_FastDecRate[i] = vo_SMB_FastDeathRate[i] + vo_SMB_FastMaintRate[i]

		for &AOM_Pool in layi.vo_AOM_Pool {
			AOM_Pool.vo_AOM_SlowDecCoeff = AOM_Pool.vo_AOM_SlowDecCoeffStandard * tod * mod_
			AOM_Pool.vo_AOM_FastDecCoeff = AOM_Pool.vo_AOM_FastDecCoeffStandard * tod * mod_
		}
	}

	// Calculation of pool changes by decomposition
	for i in 0 ..< nools {
		layi := &sc.layers[i]

		for &props in layi.vo_AOM_Pool {
			// Eq.6-5 and 6-6 in the DAISY manual
			props.vo_AOM_SlowDelta = -(props.vo_AOM_SlowDecCoeff * props.vo_AOM_Slow)
			if -props.vo_AOM_SlowDelta > props.vo_AOM_Slow {
				props.vo_AOM_SlowDelta = -props.vo_AOM_Slow
			}
			props.vo_AOM_FastDelta = -(props.vo_AOM_FastDecCoeff * props.vo_AOM_Fast)
			if -props.vo_AOM_FastDelta > props.vo_AOM_Fast {
				props.vo_AOM_FastDelta = -props.vo_AOM_Fast
			}
		}

		// Eq.6-7 in the DAISY manual
		vo_AOM_SlowDecRateSum[i] = 0.0

		for &props in layi.vo_AOM_Pool {
			props.vo_AOM_SlowDecRate_to_SMB_Slow =
				props.vo_PartAOM_Slow_to_SMB_Slow * props.vo_AOM_SlowDecCoeff * props.vo_AOM_Slow
			props.vo_AOM_SlowDecRate_to_SMB_Fast =
				props.vo_PartAOM_Slow_to_SMB_Fast * props.vo_AOM_SlowDecCoeff * props.vo_AOM_Slow

			vo_AOM_SlowDecRateSum[i] +=
				props.vo_AOM_SlowDecRate_to_SMB_Slow + props.vo_AOM_SlowDecRate_to_SMB_Fast

			AOMslow_to_SMBfast[i] += props.vo_AOM_SlowDecRate_to_SMB_Fast
			AOMslow_to_SMBslow[i] += props.vo_AOM_SlowDecRate_to_SMB_Slow
		}

		// Eq.6-8 in the DAISY manual
		vo_AOM_FastDecRateSum[i] = 0.0
		AOMfast_to_SMBfast[i] = 0.0

		for &props in layi.vo_AOM_Pool {
			props.vo_AOM_FastDecRate_to_SMB_Fast = props.vo_AOM_FastDecCoeff * props.vo_AOM_Fast
			vo_AOM_FastDecRateSum[i] += props.vo_AOM_FastDecRate_to_SMB_Fast
			AOMfast_to_SMBfast[i] += props.vo_AOM_FastDecRate_to_SMB_Fast
		}

		so.smb_slow_delta[i] =
			(po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i]) +
			(po_SOM_FastUtilizationEfficiency *
					(1.0 - po_PartSOM_Fast_to_SOM_Slow) *
					vo_SOM_FastDecRate[i]) +
			(po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i]) -
			vo_SMB_SlowDecRate[i]

		so.smb_fast_delta[i] =
			(po_SMB_UtilizationEfficiency *
				(1.0 - po_PartSMB_Slow_to_SOM_Fast) *
				(vo_SMB_SlowDeathRate[i] + vo_SMB_FastDeathRate[i])) +
			(po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i]) +
			(po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i]) -
			vo_SMB_FastDecRate[i]

		// Eq.6-9 in the DAISY manual
		so.som_slow_delta[i] =
			po_PartSOM_Fast_to_SOM_Slow * vo_SOM_FastDecRate[i] - vo_SOM_SlowDecRate[i]

		if (layi.vs_SOM_Slow + so.som_slow_delta[i]) < 0.0 {
			so.som_slow_delta[i] = layi.vs_SOM_Slow
		}

		// Eq.6-10 in the DAISY manual
		so.som_fast_delta[i] =
			po_PartSMB_Slow_to_SOM_Fast * vo_SMB_SlowDeathRate[i] +
			po_PartSMB_Fast_to_SOM_Fast * vo_SMB_FastDeathRate[i] -
			vo_SOM_FastDecRate[i]

		if (layi.vs_SOM_Fast + so.som_fast_delta[i]) < 0.0 {
			so.som_fast_delta[i] = layi.vs_SOM_Fast
		}

		vo_AOM_SlowDeltaSum[i] = 0.0
		vo_AOM_FastDeltaSum[i] = 0.0

		for &props in layi.vo_AOM_Pool {
			vo_AOM_SlowDeltaSum[i] += props.vo_AOM_SlowDelta
			vo_AOM_FastDeltaSum[i] += props.vo_AOM_FastDelta
		}
	}

	// Calculation of N balance
	for i in 0 ..< nools {
		layi := &sc.layers[i]

		CN_Ratio_SOM_Slow := layi.vs_Soil_CN_Ratio
		CN_Ratio_SOM_Fast := CN_Ratio_SOM_Slow

		vo_NBalance[i] =
			-(so.smb_slow_delta[i] / po_CN_Ratio_SMB) -
			(so.smb_fast_delta[i] / po_CN_Ratio_SMB) -
			(so.som_slow_delta[i] / CN_Ratio_SOM_Slow) -
			(so.som_fast_delta[i] / CN_Ratio_SOM_Fast)

		for &props in layi.vo_AOM_Pool {
			if libc.fabs(props.vo_CN_Ratio_AOM_Fast) >= 1.0e-7 {
				vo_NBalance[i] -= props.vo_AOM_FastDelta / props.vo_CN_Ratio_AOM_Fast
			}
			if libc.fabs(props.vo_CN_Ratio_AOM_Slow) >= 1.0e-7 {
				vo_NBalance[i] -= props.vo_AOM_SlowDelta / props.vo_CN_Ratio_AOM_Slow
			}
		}
	}

	// Check for Nmin availablity in case of immobilisation
	so.net_n_mineralisation = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]

		vo_CN_Ratio_SOM_Slow := layi.vs_Soil_CN_Ratio
		vo_CN_Ratio_SOM_Fast := vo_CN_Ratio_SOM_Slow

		if vo_NBalance[i] < 0.0 {
			if libc.fabs(vo_NBalance[i]) >=
			   ((layi.vs_SoilNH4 * po_ImmobilisationRateCoeffNH4) +
					   (layi.vs_SoilNO3 * po_ImmobilisationRateCoeffNO3)) {
				vo_AOM_SlowDeltaSum[i] = 0.0
				vo_AOM_FastDeltaSum[i] = 0.0

				for &props in layi.vo_AOM_Pool {
					if props.vo_CN_Ratio_AOM_Slow >=
					   (po_CN_Ratio_SMB / po_AOM_SlowUtilizationEfficiency) {
						props.vo_AOM_SlowDelta = 0.0
						// correction of the fluxes across pools
						AOMslow_to_SMBfast[i] -= props.vo_AOM_SlowDecRate_to_SMB_Fast
						AOMslow_to_SMBslow[i] -= props.vo_AOM_SlowDecRate_to_SMB_Slow
					}

					if props.vo_CN_Ratio_AOM_Fast >=
					   (po_CN_Ratio_SMB / po_AOM_FastUtilizationEfficiency) {
						props.vo_AOM_FastDelta = 0.0
						// correction of the fluxes across pools
						AOMfast_to_SMBfast[i] -= props.vo_AOM_FastDecRate_to_SMB_Fast
					}

					vo_AOM_SlowDeltaSum[i] += props.vo_AOM_SlowDelta
					vo_AOM_FastDeltaSum[i] += props.vo_AOM_FastDelta
				}

				if vo_CN_Ratio_SOM_Slow >= (po_CN_Ratio_SMB / po_SOM_SlowUtilizationEfficiency) {
					so.som_slow_delta[i] = 0.0
				}

				if vo_CN_Ratio_SOM_Fast >= (po_CN_Ratio_SMB / po_SOM_FastUtilizationEfficiency) {
					so.som_fast_delta[i] = 0.0
				}

				// Recalculation of SMB pool changes
				so.smb_slow_delta[i] =
					(po_SOM_SlowUtilizationEfficiency * vo_SOM_SlowDecRate[i]) +
					(po_SOM_FastUtilizationEfficiency *
							(1.0 - po_PartSOM_Fast_to_SOM_Slow) *
							vo_SOM_FastDecRate[i]) +
					(po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBslow[i]) -
					vo_SMB_SlowDecRate[i]

				if (layi.vs_SMB_Slow + so.smb_slow_delta[i]) < 0.0 {
					so.smb_slow_delta[i] = layi.vs_SMB_Slow
				}

				so.smb_fast_delta[i] =
					(po_SMB_UtilizationEfficiency *
						(1.0 - po_PartSMB_Slow_to_SOM_Fast) *
						(vo_SMB_SlowDeathRate[i] + vo_SMB_FastDeathRate[i])) +
					(po_AOM_FastUtilizationEfficiency * AOMfast_to_SMBfast[i]) +
					(po_AOM_SlowUtilizationEfficiency * AOMslow_to_SMBfast[i]) -
					vo_SMB_FastDecRate[i]

				if (layi.vs_SMB_Fast + so.smb_fast_delta[i]) < 0.0 {
					so.smb_fast_delta[i] = layi.vs_SMB_Fast
				}

				// Recalculation of N balance under conditions of immobilisation
				vo_NBalance[i] =
					-(so.smb_slow_delta[i] / po_CN_Ratio_SMB) -
					(so.smb_fast_delta[i] / po_CN_Ratio_SMB) -
					(so.som_slow_delta[i] / vo_CN_Ratio_SOM_Slow) -
					(so.som_fast_delta[i] / vo_CN_Ratio_SOM_Fast)

				for &props in layi.vo_AOM_Pool {
					if libc.fabs(props.vo_CN_Ratio_AOM_Fast) >= 1.0e-7 {
						vo_NBalance[i] -= (props.vo_AOM_FastDelta / props.vo_CN_Ratio_AOM_Fast)
					}
					if libc.fabs(props.vo_CN_Ratio_AOM_Slow) >= 1.0e-7 {
						vo_NBalance[i] -= (props.vo_AOM_SlowDelta / props.vo_CN_Ratio_AOM_Slow)
					}
				}

				// Update of Soil NH4 after recalculated N balance
				layi.vs_SoilNH4 += libc.fabs(vo_NBalance[i])
			} else {
				// Bedarf kann durch Ammonium-Pool nicht gedeckt werden --> Nitrat wird verwendet
				if libc.fabs(vo_NBalance[i]) >= (layi.vs_SoilNH4 * po_ImmobilisationRateCoeffNH4) {
					layi.vs_SoilNO3 -=
						libc.fabs(vo_NBalance[i]) -
						(layi.vs_SoilNH4 * po_ImmobilisationRateCoeffNH4)
					layi.vs_SoilNH4 -= layi.vs_SoilNH4 * po_ImmobilisationRateCoeffNH4
				} else {
					layi.vs_SoilNH4 -= libc.fabs(vo_NBalance[i])
				}
			}
		} else {
			layi.vs_SoilNH4 += libc.fabs(vo_NBalance[i])
		}

		lay0 := &sc.layers[0]
		so.net_n_mineralisation_rate[i] = libc.fabs(vo_NBalance[i]) * lay0.vs_LayerThickness // [kg m-3] --> [kg m-2]
		so.net_n_mineralisation += libc.fabs(vo_NBalance[i]) * lay0.vs_LayerThickness // [kg m-3] --> [kg m-2]
		so.sum_net_n_mineralisation += libc.fabs(vo_NBalance[i]) * lay0.vs_LayerThickness // [kg m-3] --> [kg m-2]
	}

	so.decomposer_respiration = 0.0

	// Calculation of CO2 evolution
	for i in 0 ..< nools {
		vo_SMB_SlowCO2EvolutionRate[i] =
			((1.0 - po_SOM_SlowUtilizationEfficiency) * vo_SOM_SlowDecRate[i]) +
			((1.0 - po_SOM_FastUtilizationEfficiency) *
					(1.0 - po_PartSOM_Fast_to_SOM_Slow) *
					vo_SOM_FastDecRate[i]) +
			((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBslow[i]) +
			vo_SMB_SlowMaintRate[i]

		vo_SMB_FastCO2EvolutionRate[i] =
			(1.0 - po_SMB_UtilizationEfficiency) *
				(((1.0 - po_PartSMB_Slow_to_SOM_Fast) * vo_SMB_SlowDeathRate[i]) +
						((1.0 - po_PartSMB_Fast_to_SOM_Fast) * vo_SMB_FastDeathRate[i])) +
			((1.0 - po_AOM_SlowUtilizationEfficiency) * AOMslow_to_SMBfast[i]) +
			((1.0 - po_AOM_FastUtilizationEfficiency) * AOMfast_to_SMBfast[i]) +
			vo_SMB_FastMaintRate[i]

		so.smb_co2_evolution_rate[i] =
			vo_SMB_SlowCO2EvolutionRate[i] + vo_SMB_FastCO2EvolutionRate[i]

		so.decomposer_respiration +=
			so.smb_co2_evolution_rate[i] * sc.layers[i].vs_LayerThickness // [kg C m-3] -> [kg C m-2]
	}
}

// C++: double monica::soilorganic::foClayOnDecompostionKaiteew(SoilOrganic*, double, double)
soil_organic_fo_clay_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	d_SoilClayContent, d_LimitClayEffect: f64,
) -> f64 {
	clayOnDecomposition := 0.0

	if d_SoilClayContent >= 0.0 && d_SoilClayContent <= 1.0 {
		clayOnDecomposition =
			(1.0 - d_LimitClayEffect) / (1.0 + libc.exp(-3.14 + d_SoilClayContent * 16)) +
			d_LimitClayEffect
	} else {
		so.error_message = "irregular clay content"
	}

	return clayOnDecomposition
}

// C++: double monica::soilorganic::foClayOnDecompostion(SoilOrganic*, double, double)
soil_organic_fo_clay_on_decompostion :: proc(
	so: ^Soil_Organic,
	d_SoilClayContent, d_LimitClayEffect: f64,
) -> f64 {
	fo_ClayOnDecompostion := 0.0

	if d_SoilClayContent >= 0.0 && d_SoilClayContent <= d_LimitClayEffect {
		fo_ClayOnDecompostion = 1.0 - 2.0 * d_SoilClayContent
	} else if d_SoilClayContent > d_LimitClayEffect && d_SoilClayContent <= 1.0 {
		fo_ClayOnDecompostion = 1.0 - 2.0 * d_LimitClayEffect
	} else {
		so.error_message = "irregular clay content"
	}

	return fo_ClayOnDecompostion
}

// C++: double monica::soilorganic::foTempOnDecompostionKaiteew(SoilOrganic*, double, double, double)
soil_organic_fo_temp_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	soilTemperature, QTenFactor, tempDecOptimal: f64,
) -> f64 {
	tempOnDecomposition := 0.0

	if soilTemperature > 0.0 && soilTemperature <= 100.0 {
		tempOnDecomposition =
			1.0 /
			libc.pow(
				1.0 + libc.exp(soilTemperature - (2.72 + tempDecOptimal)),
				QTenFactor / (tempDecOptimal / 3.14),
			) *
			(-1.0 + libc.pow(QTenFactor, soilTemperature / 15.76))
	} else if soilTemperature <= 0.0 && soilTemperature > -50.0 {
		tempOnDecomposition = 0.0
	} else {
		so.error_message = "irregular soil temperature"
	}

	return tempOnDecomposition
}

// C++: double monica::soilorganic::foTempOnDecompostion(SoilOrganic*, double)
soil_organic_fo_temp_on_decompostion :: proc(so: ^Soil_Organic, d_SoilTemperature: f64) -> f64 {
	fo_TempOnDecompostion := 0.0

	if d_SoilTemperature <= 0.0 && d_SoilTemperature > -40.0 {
		fo_TempOnDecompostion = 0.0
	} else if d_SoilTemperature > 0.0 && d_SoilTemperature <= 20.0 {
		fo_TempOnDecompostion = 0.1 * d_SoilTemperature
	} else if d_SoilTemperature > 20.0 && d_SoilTemperature <= 70.0 {
		fo_TempOnDecompostion = libc.exp(
			0.47 - (0.027 * d_SoilTemperature) + (0.00193 * d_SoilTemperature * d_SoilTemperature),
		)
	} else {
		so.error_message = "irregular soil temperature"
	}

	return fo_TempOnDecompostion
}

// C++: double monica::soilorganic::foMoistOnDecompostionKaiteew(SoilOrganic*, double, double, double)
soil_organic_fo_moist_on_decompostion_kaiteew :: proc(
	so: ^Soil_Organic,
	d_SoilMoisture_m3, d_Saturation, d_MoistureDecOptimal: f64,
) -> f64 {
	moistOnDecomposition := 0.0

	if d_SoilMoisture_m3 / d_Saturation >= 0.0 && d_SoilMoisture_m3 / d_Saturation <= 1.0 {
		moistOnDecomposition = libc.exp(
			-18 * libc.pow(d_SoilMoisture_m3 / d_Saturation - d_MoistureDecOptimal, 2),
		)
	} else {
		so.error_message = "irregular soil water content"
	}

	return moistOnDecomposition
}

// C++: double monica::soilorganic::foMoistOnDecompostion(SoilOrganic*, double)
soil_organic_fo_moist_on_decompostion :: proc(so: ^Soil_Organic, d_SoilMoisture_pF: f64) -> f64 {
	fo_MoistOnDecompostion := 0.0

	if libc.fabs(d_SoilMoisture_pF) <= 1.0e-7 {
		fo_MoistOnDecompostion = 0.6
	} else if d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5 {
		fo_MoistOnDecompostion = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5)
	} else if d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5 {
		fo_MoistOnDecompostion = 1.0
	} else if d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 6.5 {
		fo_MoistOnDecompostion = 1.0 - ((d_SoilMoisture_pF - 2.5) / 4.0)
	} else if d_SoilMoisture_pF > 6.5 {
		fo_MoistOnDecompostion = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_MoistOnDecompostion
}

// C++: double monica::soilorganic::foMoistOnHydrolysis(SoilOrganic*, double)
soil_organic_fo_moist_on_hydrolysis :: proc(so: ^Soil_Organic, d_SoilMoisture_pF: f64) -> f64 {
	fo_MoistOnHydrolysis := 0.0

	if d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.1 {
		fo_MoistOnHydrolysis = 0.72
	} else if d_SoilMoisture_pF > 1.1 && d_SoilMoisture_pF <= 2.4 {
		fo_MoistOnHydrolysis = 0.2207 * d_SoilMoisture_pF + 0.4672
	} else if d_SoilMoisture_pF > 2.4 && d_SoilMoisture_pF <= 3.4 {
		fo_MoistOnHydrolysis = 1.0
	} else if d_SoilMoisture_pF > 3.4 && d_SoilMoisture_pF <= 4.6 {
		fo_MoistOnHydrolysis = -0.8659 * d_SoilMoisture_pF + 3.9849
	} else if d_SoilMoisture_pF > 4.6 {
		fo_MoistOnHydrolysis = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_MoistOnHydrolysis
}

// C++: double monica::soilorganic::foTempOnNitrification(SoilOrganic*, double)
soil_organic_fo_temp_on_nitrification :: proc(so: ^Soil_Organic, soilTemp: f64) -> f64 {
	result := 0.0

	if soilTemp <= 2.0 && soilTemp > -40.0 {
		result = 0.0
	} else if soilTemp > 2.0 && soilTemp <= 6.0 {
		result = 0.15 * (soilTemp - 2.0)
	} else if soilTemp > 6.0 && soilTemp <= 20.0 {
		result = 0.1 * soilTemp
	} else if soilTemp > 20.0 && soilTemp <= 70.0 {
		result = libc.exp(0.47 - (0.027 * soilTemp) + (0.00193 * soilTemp * soilTemp))
	} else {
		so.error_message = "irregular soil temperature"
	}

	return result
}

// C++: double monica::soilorganic::foMoistOnNitrification(SoilOrganic*, double)
soil_organic_fo_moist_on_nitrification :: proc(so: ^Soil_Organic, d_SoilMoisture_pF: f64) -> f64 {
	fo_MoistOnNitrification := 0.0

	if libc.fabs(d_SoilMoisture_pF) <= 1.0e-7 {
		fo_MoistOnNitrification = 0.6
	} else if d_SoilMoisture_pF > 0.0 && d_SoilMoisture_pF <= 1.5 {
		fo_MoistOnNitrification = 0.6 + 0.4 * (d_SoilMoisture_pF / 1.5)
	} else if d_SoilMoisture_pF > 1.5 && d_SoilMoisture_pF <= 2.5 {
		fo_MoistOnNitrification = 1.0
	} else if d_SoilMoisture_pF > 2.5 && d_SoilMoisture_pF <= 5.0 {
		fo_MoistOnNitrification = 1.0 - ((d_SoilMoisture_pF - 2.5) / 2.5)
	} else if d_SoilMoisture_pF > 5.0 {
		fo_MoistOnNitrification = 0.0
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_MoistOnNitrification
}

// C++: double monica::soilorganic::foMoistOnDenitrification(SoilOrganic*, double, double)
soil_organic_fo_moist_on_denitrification :: proc(
	so: ^Soil_Organic,
	d_SoilMoisture_m3, d_Saturation: f64,
) -> f64 {
	po_Denit1 := so.mod_params.po_Denit1
	po_Denit2 := so.mod_params.po_Denit2
	po_Denit3 := so.mod_params.po_Denit3
	fo_MoistOnDenitrification := 0.0

	if (d_SoilMoisture_m3 / d_Saturation) <= 0.8 {
		fo_MoistOnDenitrification = 0.0
	} else if (d_SoilMoisture_m3 / d_Saturation) > 0.8 &&
	   (d_SoilMoisture_m3 / d_Saturation) <= 0.9 {
		fo_MoistOnDenitrification =
			po_Denit1 * ((d_SoilMoisture_m3 / d_Saturation) - po_Denit2) / (po_Denit3 - po_Denit2)
	} else if (d_SoilMoisture_m3 / d_Saturation) > 0.9 &&
	   (d_SoilMoisture_m3 / d_Saturation) <= 1.0 {
		fo_MoistOnDenitrification =
			po_Denit1 +
			(1.0 - po_Denit1) *
				((d_SoilMoisture_m3 / d_Saturation) - po_Denit3) /
				(1.0 - po_Denit3)
	} else {
		so.error_message = "irregular soil water content"
	}

	return fo_MoistOnDenitrification
}

// C++: double monica::soilorganic::foNH3onNitriteOxidation(SoilOrganic*, double, double)
soil_organic_fo_nh3_on_nitrite_oxidation :: proc(
	so: ^Soil_Organic,
	d_SoilNH4, d_SoilpH: f64,
) -> f64 {
	po_Inhibitor_NH3 := so.mod_params.po_Inhibitor_NH3

	fo_NH3onNitriteOxidation :=
		po_Inhibitor_NH3 /
		(po_Inhibitor_NH3 +
				d_SoilNH4 * (1 - 1 / (1.0 + libc.pow(10.0, d_SoilpH - soil.PO_PKA_NH3))))

	return fo_NH3onNitriteOxidation
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
	vo_AOM_Addition: bool,
	vw_MeanAirTemperature, vw_WindSpeed: f64,
	allocator := context.allocator,
) {
	vo_SoilWet: f64
	vo_N_PotVolatilisedSum := 0.0
	vo_N_ActVolatilised := 0.0
	vo_DaysAfterApplicationSum := 0

	lay0 := so.soil_column.layers[0] // by-value copy - see the doc comment above
	lay0.vo_AOM_Pool = clone_aom_pool(lay0.vo_AOM_Pool, allocator) // deep-copy, matching std::vector's copy ctor

	if soil_moisture_pf(&lay0) > 2.5 {
		vo_SoilWet = 0.0
	} else {
		vo_SoilWet = 1.0
	}

	AOM_Pool := lay0.vo_AOM_Pool
	for props in AOM_Pool {
		vo_DaysAfterApplicationSum += props.vo_DaysAfterApplication
	}

	if vo_DaysAfterApplicationSum > 0 || vo_AOM_Addition {
		vo_N_PotVolatilisedSum = 0.0

		for props in AOM_Pool {
			vo_AOM_TAN_Content := props.vo_AOM_NH4Content * 1000.0 * props.vo_AOM_DryMatterContent

			vo_MaxVolatilisation :=
				0.0495 *
				libc.pow(1.1020, vo_SoilWet) *
				libc.pow(1.0223, vw_MeanAirTemperature) *
				libc.pow(1.0417, vw_WindSpeed) *
				libc.pow(1.1080, props.vo_AOM_DryMatterContent) *
				libc.pow(0.8280, vo_AOM_TAN_Content) *
				libc.pow(f64(11.300), props.incorporation ? 1.0 : 0.0)

			vo_VolatilisationHalfLife :=
				1.0380 *
				libc.pow(1.1020, vo_SoilWet) *
				libc.pow(0.9600, vw_MeanAirTemperature) *
				libc.pow(0.9500, vw_WindSpeed) *
				libc.pow(1.1750, props.vo_AOM_DryMatterContent) *
				libc.pow(1.1060, vo_AOM_TAN_Content) *
				libc.pow(f64(1.0000), props.incorporation ? 1.0 : 0.0) *
				(18869.3 * libc.exp(-lay0.vs_SoilpH / 0.63321) + 0.70165)

			vo_VolatilisationRate :=
				vo_MaxVolatilisation *
				(vo_VolatilisationHalfLife /
						libc.pow(
							f64(props.vo_DaysAfterApplication) + vo_VolatilisationHalfLife,
							f64(2.0),
						))

			vo_N_PotVolatilised :=
				vo_VolatilisationRate *
				vo_AOM_TAN_Content *
				(props.vo_AOM_Slow + props.vo_AOM_Fast) /
				10000.0 /
				1000.0

			vo_N_PotVolatilisedSum += vo_N_PotVolatilised
		}

		if lay0.vs_SoilNH4 > vo_N_PotVolatilisedSum {
			vo_N_ActVolatilised = vo_N_PotVolatilisedSum
		} else {
			vo_N_ActVolatilised = lay0.vs_SoilNH4
		}

		// update NH4 content of top soil layer with volatilisation balance
		lay0.vs_SoilNH4 -= (vo_N_ActVolatilised / lay0.vs_LayerThickness)
	} else {
		vo_N_ActVolatilised = 0.0
	}

	// NH3 volatilised from top layer NH4 pool. See Urea section
	so.total_nh3_volatilised = vo_N_ActVolatilised + so.nh3_volatilised // [kg N m-2]

	for &props in AOM_Pool {
		if props.vo_DaysAfterApplication > 0 && !vo_AOM_Addition {
			props.vo_DaysAfterApplication += 1
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

	nools := sc.vs_NumberOfOrganicLayers
	po_AmmoniaOxidationRateCoeffStandard := params.po_AmmoniaOxidationRateCoeffStandard
	po_NitriteOxidationRateCoeffStandard := params.po_NitriteOxidationRateCoeffStandard

	vo_AmmoniaOxidationRateCoeff := make([dynamic]f64, nools, context.temp_allocator)
	vo_NitriteOxidationRateCoeff := make([dynamic]f64, nools, context.temp_allocator)

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		NH4i := layi.vs_SoilNH4

		vo_AmmoniaOxidationRateCoeff[i] =
			po_AmmoniaOxidationRateCoeffStandard *
			soil_organic_fo_temp_on_nitrification(so, layi.vs_SoilTemperature) *
			soil_organic_fo_moist_on_nitrification(so, soil_moisture_pf(layi))

		so.act_ammonia_oxidation_rate[i] = vo_AmmoniaOxidationRateCoeff[i] * NH4i

		vo_NitriteOxidationRateCoeff[i] =
			po_NitriteOxidationRateCoeffStandard *
			soil_organic_fo_temp_on_nitrification(so, layi.vs_SoilTemperature) *
			soil_organic_fo_moist_on_nitrification(so, soil_moisture_pf(layi)) *
			soil_organic_fo_nh3_on_nitrite_oxidation(so, NH4i, layi.vs_SoilpH)

		so.act_nitrification_rate[i] = vo_NitriteOxidationRateCoeff[i] * layi.vs_SoilNO2

		// Update NH4, NO2 and NO3 content with nitrification balance
		// Stange, F., C. Nendel (2014): N.N., in preparation
		if NH4i > so.act_ammonia_oxidation_rate[i] {
			layi.vs_SoilNH4 -= so.act_ammonia_oxidation_rate[i]
			layi.vs_SoilNO2 += so.act_ammonia_oxidation_rate[i]
		} else {
			layi.vs_SoilNO2 += NH4i
			layi.vs_SoilNH4 = 0.0
		}

		if layi.vs_SoilNO2 > so.act_nitrification_rate[i] {
			layi.vs_SoilNO2 -= so.act_nitrification_rate[i]
			layi.vs_SoilNO3 += so.act_nitrification_rate[i]
		} else {
			layi.vs_SoilNO3 += layi.vs_SoilNO2
			layi.vs_SoilNO2 = 0.0
		}
	}
}

// C++: void monica::soilorganic::foSticsNitrification(SoilOrganic*)
soil_organic_fo_stics_nitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	sticsParams := &so.mod_params.sticsParams

	nools := sc.vs_NumberOfOrganicLayers

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.vs_SoilMoisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		NH4i := layi.vs_SoilNH4

		kgN_per_m3_to_mgN_per_kg := 1000.0 * 1000.0 / sbdi
		mgN_per_kg_to_kgN_per_m3 := 1 / kgN_per_m3_to_mgN_per_kg

		so.act_nitrification_rate[i] = stics_vnit(
				sticsParams,
				NH4i * kgN_per_m3_to_mgN_per_kg, // kg-NH4-N/m3-soil -> mg-NH4-N/kg-soil
				layi.vs_SoilpH,
				layi.vs_SoilTemperature,
				smi / layi.vs_Saturation, // soil water-filled pore space []
				smi * 1000 / sbdi, // gravimetric soil water content kg-water/kg-soil
				layi.vs_FieldCapacity,
				layi.vs_Saturation,
			) * mgN_per_kg_to_kgN_per_m3 // mg-N -> kg-N

		if NH4i > so.act_nitrification_rate[i] {
			layi.vs_SoilNH4 -= so.act_nitrification_rate[i]
			layi.vs_SoilNO3 += so.act_nitrification_rate[i]
		} else {
			layi.vs_SoilNO3 += NH4i
			layi.vs_SoilNH4 = 0.0
		}
	}
}

// C++: void monica::soilorganic::foDenitrification(SoilOrganic*)
soil_organic_fo_denitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.vs_NumberOfOrganicLayers
	vo_PotDenitrificationRate := make([dynamic]f64, nools, context.temp_allocator)
	po_SpecAnaerobDenitrification := params.po_SpecAnaerobDenitrification
	po_TransportRateCoeff := params.po_TransportRateCoeff
	so.total_denitrification = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		NO3i := layi.vs_SoilNO3

		// Temperature function is the same as in Nitrification subroutine
		vo_PotDenitrificationRate[i] =
			po_SpecAnaerobDenitrification *
			so.smb_co2_evolution_rate[i] *
			soil_organic_fo_temp_on_nitrification(so, layi.vs_SoilTemperature)

		so.act_denitrification_rate[i] = min(
			vo_PotDenitrificationRate[i] *
			soil_organic_fo_moist_on_denitrification(
				so,
				layi.vs_SoilMoisture_m3,
				layi.vs_Saturation,
			),
			po_TransportRateCoeff * NO3i,
		)

		// update NO3 content of soil layer with denitrification balance [kg N m-3]
		if NO3i > so.act_denitrification_rate[i] {
			layi.vs_SoilNO3 -= so.act_denitrification_rate[i]
		} else {
			so.act_denitrification_rate[i] = NO3i
			layi.vs_SoilNO3 = 0.0
		}

		so.total_denitrification += so.act_denitrification_rate[i] * layi.vs_LayerThickness // [kg m-3] --> [kg m-2]
	}

	so.sum_denitrification += so.total_denitrification // [kg N m-2]
}

// C++: void monica::soilorganic::foSticsDenitrification(SoilOrganic*)
soil_organic_fo_stics_denitrification :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	sticsParams := &so.mod_params.sticsParams

	nools := sc.vs_NumberOfOrganicLayers
	so.total_denitrification = 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.vs_SoilMoisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		lti := layi.vs_LayerThickness
		NO3i := layi.vs_SoilNO3

		kgN_per_m3_to_mgN_per_kg := 1000.0 * 1000.0 / sbdi
		mgN_per_kg_to_kgN_per_m3 := 1 / kgN_per_m3_to_mgN_per_kg

		so.act_denitrification_rate[i] = stics_vdenit(
				sticsParams,
				soil_organic_carbon(layi) * 100.0, // kg-C/kg-soil = % [0-1] -> % [0-100]
				NO3i * kgN_per_m3_to_mgN_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
				layi.vs_SoilTemperature,
				smi / layi.vs_Saturation, // soil water-filled pore space []
				smi * 1000 / sbdi, // gravimetric soil water content kg-water/kg-soil
			) * mgN_per_kg_to_kgN_per_m3 // mg-N -> kg-N

		// update NO3 content of soil layer with denitrification balance [kg N m-3]
		if NO3i > so.act_denitrification_rate[i] {
			layi.vs_SoilNO3 -= so.act_denitrification_rate[i]
		} else {
			so.act_denitrification_rate[i] = NO3i
			layi.vs_SoilNO3 = 0.0
		}
		so.total_denitrification += so.act_denitrification_rate[i] * lti // [kg m-3] --> [kg m-2]
	}

	so.sum_denitrification += so.total_denitrification // [kg N m-2]
}

// C++: double monica::soilorganic::foN2OProduction(SoilOrganic*)
soil_organic_fo_n2o_production :: proc(so: ^Soil_Organic) -> f64 {
	sc := so.soil_column
	params := &so.mod_params

	nools := sc.vs_NumberOfOrganicLayers
	N2OProductionRate := params.po_N2OProductionRate
	pKaHNO2 := soil.PO_PKA_HNO2
	sumN2OProduced := 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		pHi := layi.vs_SoilpH
		NO2i := layi.vs_SoilNO2
		lti := layi.vs_LayerThickness
		tempi := layi.vs_SoilTemperature

		// pKaHNO2 original concept pow10. We used pow2 to allow reactive HNO2
		// being available at higher pH values
		pH_response := 1.0 / (1.0 + libc.pow(2.0, pHi - pKaHNO2))

		N2OProductionAtLayer :=
			NO2i *
			soil_organic_fo_temp_on_nitrification(so, tempi) *
			N2OProductionRate *
			pH_response *
			lti *
			10000 // convert from kg N-N2O m-3 to kg N-N2O ha-1 (for each layer)

		sumN2OProduced += N2OProductionAtLayer
	}

	return sumN2OProduced
}

// C++: SoilOrganic::NitDenitN2O monica::soilorganic::foSticsN2OProduction(SoilOrganic*)
//
// Returns (sumN2OProducedNit, sumN2OProducedDenit).
soil_organic_fo_stics_n2o_production :: proc(so: ^Soil_Organic) -> (f64, f64) {
	sc := so.soil_column
	sticsParams := &so.mod_params.sticsParams

	nools := sc.vs_NumberOfOrganicLayers
	sumN2OProducedNit := 0.0
	sumN2OProducedDenit := 0.0

	for i in 0 ..< nools {
		layi := &sc.layers[i]
		smi := layi.vs_SoilMoisture_m3 // m3-water/m3-soil
		sbdi := soil_bulk_density(layi) // kg-soil/m3-soil
		lti := layi.vs_LayerThickness

		kgN_per_m3_to_mgN_per_kg := 1000.0 * 1000.0 / sbdi
		mgN_per_kg_to_kgN_per_m3 := 1 / kgN_per_m3_to_mgN_per_kg

		stics2monicaUnits := mgN_per_kg_to_kgN_per_m3 * lti * 10000.0 // /kg-soil -> /m3-soil// /m3-soil -> /m2-soil// /m2-soil -> /ha-soil

		N2Onit, N2Odenit := stics_n2o(
			sticsParams,
			layi.vs_SoilNO3 * kgN_per_m3_to_mgN_per_kg, // kg-NO3-N/m3-soil -> mg-NO3-N/kg-soil
			smi / layi.vs_Saturation, // soil water-filled pore space []
			layi.vs_SoilpH,
			so.act_nitrification_rate[i] * kgN_per_m3_to_mgN_per_kg, // nitrification rate [mg-N/kg-soil/day]
			so.act_denitrification_rate[i] * kgN_per_m3_to_mgN_per_kg, // denitrification rate [mg-N/kg-soil/day]
		)

		sumN2OProducedNit += N2Onit * stics2monicaUnits
		sumN2OProducedDenit += N2Odenit * stics2monicaUnits
	}

	return sumN2OProducedNit, sumN2OProducedDenit
}

// C++: void monica::soilorganic::foPoolUpdate(SoilOrganic*)
soil_organic_fo_pool_update :: proc(so: ^Soil_Organic) {
	sc := so.soil_column
	nools := sc.vs_NumberOfOrganicLayers

	for i in 0 ..< nools {
		layi := &sc.layers[i]

		so.aom_slow_delta_sum[i] = 0.0
		so.aom_fast_delta_sum[i] = 0.0
		so.aom_slow_sum[i] = 0.0
		so.aom_fast_sum[i] = 0.0

		for &pool in layi.vo_AOM_Pool {
			pool.vo_AOM_Slow += pool.vo_AOM_SlowDelta
			pool.vo_AOM_Fast += pool.vo_AOM_FastDelta

			so.aom_slow_delta_sum[i] += pool.vo_AOM_SlowDelta
			so.aom_fast_delta_sum[i] += pool.vo_AOM_FastDelta

			so.aom_slow_sum[i] += pool.vo_AOM_Slow
			so.aom_fast_sum[i] += pool.vo_AOM_Fast
		}

		layi.vs_SOM_Slow += so.som_slow_delta[i]
		layi.vs_SOM_Fast += so.som_fast_delta[i]
		layi.vs_SMB_Slow += so.smb_slow_delta[i]
		layi.vs_SMB_Fast += so.smb_fast_delta[i]

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
		layi.vs_SoilOrganicCarbon =
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
	layer2addedOrganicMatterAmount: map[int]f64,
	addedOrganicMatterNConcentration: f64 = 0,
	allocator := context.allocator,
) {
	sc := so.soil_column
	nools := sc.vs_NumberOfOrganicLayers
	layerThickness := sc.layers[0].vs_LayerThickness

	areCropResidueParams := int(params.vo_CN_Ratio_AOM_Fast * 10000.0) == 0

	calc_CN_Ratio_AOM_Fast_and_added_Corg_amount :: proc(
		params: ^p.Organic_Matter_Parameters,
		so: ^Soil_Organic,
		layerThickness: f64,
		vo_AddedOrganicMatterAmount, vo_AddedOrganicMatterNConcentration: f64,
	) -> (
		CN_ratio_AOM_fast: f64,
		added_Corg_amount: f64,
		added_Norg_amount: f64,
	) {
		added_Corg_amount =
			(params.vo_CorgContent <= 0.0 ? soil.PO_AOM_TO_C : params.vo_CorgContent) *
			vo_AddedOrganicMatterAmount *
			params.vo_AOM_DryMatterContent /
			10000.0 /
			layerThickness

		added_Norg_amount =
			vo_AddedOrganicMatterNConcentration <= 0.0 ? 0.01 : (vo_AddedOrganicMatterAmount * params.vo_AOM_DryMatterContent * vo_AddedOrganicMatterNConcentration / 10000.0 / layerThickness)

		N_for_AOM_slow :=
			added_Corg_amount * params.vo_PartAOM_to_AOM_Slow / params.vo_CN_Ratio_AOM_Slow
		if N_for_AOM_slow < added_Norg_amount {
			N_for_AOM_fast := added_Norg_amount - N_for_AOM_slow
			CN_ratio_AOM_fast = added_Corg_amount * params.vo_PartAOM_to_AOM_Fast / N_for_AOM_fast
		} else {
			CN_ratio_AOM_fast = so.mod_params.po_AOM_FastMaxC_to_N
		}

		CN_ratio_AOM_fast = min(CN_ratio_AOM_fast, so.mod_params.po_AOM_FastMaxC_to_N)
		return
	}

	rounded_AOM_SlowDecCoeffStandard := tl.round_shifted_int(params.vo_AOM_SlowDecCoeffStandard, 4)
	rounded_AOM_FastDecCoeffStandard := tl.round_shifted_int(params.vo_AOM_FastDecCoeffStandard, 4)
	rounded_PartAOM_Slow_to_SMB_Slow := tl.round_shifted_int(params.vo_PartAOM_Slow_to_SMB_Slow, 4)
	rounded_PartAOM_Slow_to_SMB_Fast := tl.round_shifted_int(params.vo_PartAOM_Slow_to_SMB_Fast, 4)
	rounded_CN_Ratio_AOM_Slow := tl.round_shifted_int(params.vo_CN_Ratio_AOM_Slow, 4)

	are_same_aom_props_as_om_params :: proc(
		props: ^Aom_Properties,
		rounded_AOM_SlowDecCoeffStandard,
		rounded_AOM_FastDecCoeffStandard,
		rounded_PartAOM_Slow_to_SMB_Slow,
		rounded_PartAOM_Slow_to_SMB_Fast,
		rounded_CN_Ratio_AOM_Slow: int,
	) -> bool {
		return(
			tl.round_shifted_int(props.vo_AOM_SlowDecCoeffStandard, 4) ==
				rounded_AOM_SlowDecCoeffStandard &&
			tl.round_shifted_int(props.vo_AOM_FastDecCoeffStandard, 4) ==
				rounded_AOM_FastDecCoeffStandard &&
			tl.round_shifted_int(props.vo_PartAOM_Slow_to_SMB_Slow, 4) ==
				rounded_PartAOM_Slow_to_SMB_Slow &&
			tl.round_shifted_int(props.vo_PartAOM_Slow_to_SMB_Fast, 4) ==
				rounded_PartAOM_Slow_to_SMB_Fast &&
			tl.round_shifted_int(props.vo_CN_Ratio_AOM_Slow, 4) == rounded_CN_Ratio_AOM_Slow \
		)
	}

	// sorted keys, matching std::map's iteration order
	keys := make([dynamic]int, 0, len(layer2addedOrganicMatterAmount), allocator)
	for k in layer2addedOrganicMatterAmount {
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
			v := layer2addedOrganicMatterAmount[k]
			if k < nools {
				sc.layers[k].vs_SoilCarbamid +=
					v *
					params.vo_AOM_DryMatterContent *
					params.vo_AOM_CarbamidContent /
					10000.0 /
					layerThickness
			}
		}
	}

	poolSetIndex := -1
	if areCropResidueParams {
		i := 0
		for &props in sc.layers[0].vo_AOM_Pool {
			if are_same_aom_props_as_om_params(
				&props,
				rounded_AOM_SlowDecCoeffStandard,
				rounded_AOM_FastDecCoeffStandard,
				rounded_PartAOM_Slow_to_SMB_Slow,
				rounded_PartAOM_Slow_to_SMB_Fast,
				rounded_CN_Ratio_AOM_Slow,
			) {
				poolSetIndex = i
				break
			}
			i += 1
		}
	}

	for intoLayerIndex in keys {
		addedOrganicMatterAmount := layer2addedOrganicMatterAmount[intoLayerIndex]
		if intoLayerIndex >= len(sc.layers) {
			continue
		}
		intoLayer := &sc.layers[intoLayerIndex]

		calced_CN_Ratio_AOM_Fast, added_Corg_amount, _ :=
			calc_CN_Ratio_AOM_Fast_and_added_Corg_amount(
				params,
				so,
				layerThickness,
				addedOrganicMatterAmount,
				addedOrganicMatterNConcentration,
			)

		AOM_slow_input := 0.0
		AOM_fast_input := 0.0

		if poolSetIndex < 0 {
			pool: Aom_Properties
			pool.vo_AOM_SlowDecCoeffStandard = params.vo_AOM_SlowDecCoeffStandard
			pool.vo_AOM_FastDecCoeffStandard = params.vo_AOM_FastDecCoeffStandard
			pool.vo_CN_Ratio_AOM_Slow = params.vo_CN_Ratio_AOM_Slow
			pool.vo_CN_Ratio_AOM_Fast =
				areCropResidueParams ? calced_CN_Ratio_AOM_Fast : params.vo_CN_Ratio_AOM_Fast
			pool.vo_PartAOM_Slow_to_SMB_Slow = params.vo_PartAOM_Slow_to_SMB_Slow
			pool.vo_PartAOM_Slow_to_SMB_Fast = params.vo_PartAOM_Slow_to_SMB_Fast
			pool.incorporation = so.incorporation
			pool.noVolatilization = areCropResidueParams

			for i in 0 ..< nools {
				append(&sc.layers[i].vo_AOM_Pool, pool)

				if i == intoLayerIndex {
					cpool := &intoLayer.vo_AOM_Pool[len(intoLayer.vo_AOM_Pool) - 1]
					cpool.vo_DaysAfterApplication = 1
					cpool.vo_AOM_DryMatterContent = params.vo_AOM_DryMatterContent
					cpool.vo_AOM_NH4Content = params.vo_AOM_NH4Content
					cpool.vo_AOM_Slow = params.vo_PartAOM_to_AOM_Slow * added_Corg_amount
					AOM_slow_input = cpool.vo_AOM_Slow
					cpool.vo_AOM_Fast = params.vo_PartAOM_to_AOM_Fast * added_Corg_amount
					AOM_fast_input = cpool.vo_AOM_Fast
				}
			}

			poolSetIndex = len(sc.layers[0].vo_AOM_Pool) - 1
		} else {
			cpool := &intoLayer.vo_AOM_Pool[poolSetIndex]
			AOM_slow_input = params.vo_PartAOM_to_AOM_Slow * added_Corg_amount
			cpool.vo_AOM_Slow += AOM_slow_input
			added_CN_ratio_AOM_fast :=
				areCropResidueParams ? calced_CN_Ratio_AOM_Fast : params.vo_CN_Ratio_AOM_Fast
			pool_fast_N := cpool.vo_AOM_Fast / cpool.vo_CN_Ratio_AOM_Fast
			added_fast_N :=
				params.vo_PartAOM_to_AOM_Fast * added_Corg_amount / added_CN_ratio_AOM_fast
			AOM_fast_input = params.vo_PartAOM_to_AOM_Fast * added_Corg_amount
			cpool.vo_AOM_Fast += AOM_fast_input
			new_CN_ratio_AOM_fast := cpool.vo_AOM_Fast / (pool_fast_N + added_fast_N)
			cpool.vo_CN_Ratio_AOM_Fast = new_CN_ratio_AOM_fast
		}

		soil_NH4_input :=
			params.vo_AOM_NH4Content *
			addedOrganicMatterAmount *
			params.vo_AOM_DryMatterContent /
			10000.0 /
			layerThickness

		soil_NO3_input :=
			params.vo_AOM_NO3Content *
			addedOrganicMatterAmount *
			params.vo_AOM_DryMatterContent /
			10000.0 /
			layerThickness

		SOM_FastInput :=
			max(0.0, (1.0 - (params.vo_PartAOM_to_AOM_Slow + params.vo_PartAOM_to_AOM_Fast))) *
			added_Corg_amount

		intoLayer.vs_SoilNH4 += soil_NH4_input
		intoLayer.vs_SoilNO3 += soil_NO3_input
		intoLayer.vs_SOM_Fast += SOM_FastInput

		so.aom_slow_input[intoLayerIndex] += AOM_slow_input
		so.aom_fast_input[intoLayerIndex] += AOM_fast_input
		so.som_fast_input[intoLayerIndex] += SOM_FastInput
	}

	so.added_organic_matter = true
}

// C++: void monica::soilorganic::addOrganicMatter(SoilOrganic*, const
//        OrganicMatterParameters&, double, double, size_t)
soil_organic_add_organic_matter_amount :: proc(
	so: ^Soil_Organic,
	params: ^p.Organic_Matter_Parameters,
	amount: f64,
	nConcentration: f64 = 0,
	intoLayerIndex: int = 0,
	allocator := context.allocator,
) {
	m := make(map[int]f64, 1, allocator)
	m[intoLayerIndex] = amount
	soil_organic_add_organic_matter(so, params, m, nConcentration, allocator)
}

// C++: double monica::soilorganic::getOrganicN(const SoilOrganic*, int)
soil_organic_get_organic_n :: proc(so: ^Soil_Organic, i: int) -> f64 {
	orgN := 0.0

	orgN += so.soil_column.layers[i].vs_SMB_Fast / so.mod_params.po_CN_Ratio_SMB
	orgN += so.soil_column.layers[i].vs_SMB_Slow / so.mod_params.po_CN_Ratio_SMB

	cn := so.soil_column.layers[i].vs_Soil_CN_Ratio
	orgN += so.soil_column.layers[i].vs_SOM_Fast / cn
	orgN += so.soil_column.layers[i].vs_SOM_Slow / cn

	for aomp in so.soil_column.layers[i].vo_AOM_Pool {
		orgN += aomp.vo_AOM_Fast / aomp.vo_CN_Ratio_AOM_Fast
		orgN += aomp.vo_AOM_Slow / aomp.vo_CN_Ratio_AOM_Slow
	}

	return orgN
}

// C++: double monica::soilorganic::getSoilOrganicC(const SoilOrganic*, int)
soil_organic_get_soil_organic_c :: proc(so: ^Soil_Organic, iLayer: int) -> f64 {
	return so.soil_organic_c[iLayer] / soil_bulk_density(&so.soil_column.layers[iLayer])
}

// C++: double monica::soilorganic::getNetNMineralisationRate(const SoilOrganic*, int)
soil_organic_get_net_n_mineralisation_rate :: proc(so: ^Soil_Organic, iLayer: int) -> f64 {
	return so.net_n_mineralisation_rate[iLayer] * 10000.0
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
	d_NetPrimaryProduction, d_DecomposerRespiration: f64,
) -> f64 {
	return d_NetPrimaryProduction - (d_DecomposerRespiration * 10000.0) // [kg C ha-1 d-1]
}

// C++: double monica::soilorganic::foNetEcosystemExchange(SoilOrganic*, double, double)
//
// Net ecosystem exchange [kg C ha-1 d-1]. NEE = NEP (M.U.F. Kirschbaum and R.
// Mueller (2001)). Per definition NPP is negative and respiration is positive.
soil_organic_fo_net_ecosystem_exchange :: proc(
	so: ^Soil_Organic,
	d_NetPrimaryProduction, d_DecomposerRespiration: f64,
) -> f64 {
	return -d_NetPrimaryProduction + (d_DecomposerRespiration * 10000.0) // [kg C ha-1 d-1]
}

// C++: void monica::soilorganic::step(SoilOrganic*, double, double, double)
soil_organic_step :: proc(so: ^Soil_Organic, meanAirTemperature, precipitation, windSpeed: f64) {
	// C++: `so->cropModule ? so->cropModule->vc_NetPrimaryProduction : 0` - real
	// SoilOrganic struct field, no monica-back-pointer deviation needed here.
	netPrimaryProduction := so.crop_module != nil ? so.crop_module.vc_NetPrimaryProduction : 0

	soil_organic_fo_urea(so)
	soil_organic_fo_mit(so)
	soil_organic_fo_volatilisation(so, so.added_organic_matter, meanAirTemperature, windSpeed)

	if so.mod_params.sticsParams.use_nit {
		soil_organic_fo_stics_nitrification(so)
	} else {
		soil_organic_fo_nitrification(so)
	}

	if so.mod_params.sticsParams.use_denit {
		soil_organic_fo_stics_denitrification(so)
	} else {
		soil_organic_fo_denitrification(so)
	}

	n2o_nit, n2o_denit: f64
	if so.mod_params.sticsParams.use_n2o {
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
		netPrimaryProduction,
		so.decomposer_respiration,
	)
	so.net_ecosystem_exchange = soil_organic_fo_net_ecosystem_exchange(
		so,
		netPrimaryProduction,
		so.decomposer_respiration,
	)

	so.sum_nh3_volatilised += so.nh3_volatilised
	so.sum_n2o_produced += so.n2o_produced

	so.irrigation_amount = 0.0

	nools := so.soil_column.vs_NumberOfOrganicLayers
	for i in 0 ..< nools {
		so.aom_slow_input[i] = 0.0
		so.aom_fast_input[i] = 0.0
		so.som_fast_input[i] = 0.0
	}
	so.added_organic_matter = false
}
