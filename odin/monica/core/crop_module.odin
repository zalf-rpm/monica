// Phase 5 checkpoint 2: src/core/crop-module.h's CropModule struct and
// src/core/crop-module.cpp's makeCropModule constructor (the first overload -
// the second, `mas::schema::model::monica::CropModuleState::Reader`-based, is
// Cap'n Proto deserialize and dropped, per plan-odin.md's "Explicitly
// dropped" table). Replaces crop_module_stub.odin's 11-field placeholder
// wholesale; every field here originally kept its exact C++ name, so the
// phase-4 modules that already hold a `cropModule: ^Crop_Module` pointer
// (soilmoisture, soiltransport, soilorganic) compiled unchanged against this
// drop-in replacement. Fields were later renamed to snake_case in the
// post-port cleanup - see odin/NAME_MAP.md for the C++ name each one had.
//
// Phase 5 checkpoint 3 adds the phenology + canopy-geometry cropmodule::
// functions: fcRadiation, fcDaylengthFactor, fcVernalisationFactor,
// fcOxygenDeficiency, fcCropDevelopmentalStage (+ its
// fcUpdateCropParametersForPerennial dependency), fcKcFactor, fcCropSize,
// fcCropGreenArea, fcSoilCoverage, setStage, the anthesis/maturity query
// functions, and the small pure getters. Not yet ported: fcCropPhotosynthesis
// onward (checkpoint 4+), step() itself and every fire_event-driven bit of
// orchestration inline in it (checkpoint 7), forceTransplantState (fired by a
// Transplant workstep, checkpoint 7), setPerennialCropParameters (fired by
// Sowing, checkpoint 7), organIdsForPrimaryYield (yield, checkpoint 5),
// getEffectiveRootingDepth (root/water, checkpoint 6).
package core

import d "../../support/date"
import tl "../../support/tools"
import p "../params"
import libc "core:c/libc"
import "core:fmt"
import "core:slice"
import "../../support/cr_math"

// Phase 5 checkpoint 4 adds fcCropPhotosynthesis (the largest function in
// crop-module.cpp, ~1100 lines) plus fcGrossPrimaryProduction,
// fcNetPrimaryProduction and calculateVOCEmissions - the functions that wire
// the phase-5-checkpoint-1 satellite modules (photosynthesis-FvCB,
// O3-impact, voc-guenther, voc-jjv) into the crop module proper.

// C++: monica::OId::ORGAN (src/io/output.h) - a plain (unscoped) C++ enum
// nested in struct OId, so OId::LEAF etc. are usable directly as organ
// indices into organ_biomass and friends. io/output.h itself is phase 7
// (not yet ported); crop-module.cpp only ever needs the bare index values,
// not the rest of OId, so they're defined locally here instead of waiting.
Organ_Root :: 0
Organ_Leaf :: 1
Organ_Shoot :: 2
Organ_Fruit :: 3
Organ_Struct :: 4
Organ_Sugar :: 5

// C++: struct monica::CropModule (src/core/crop-module.h)
//
// The fields are grouped by the component that owns them (architecture.md
// section 5, plan-component-interfaces.md C2) into sub-structs embedded with
// `using`: `cm.leaf_area_index` still works, output paths (`crop.<field>`)
// too (reflectpath looks through `using`), and the grouping is visible in the
// type. Owner = the procedure that computes a field; which procedures still
// write another group's fields is listed in architecture.md.
Crop_Module :: struct {
	no_of_organs:          int, // C++ size_t
	no_of_dev_stages:      int, // C++ size_t
	// C++: Intercropping *intercropping - Intercropping itself is dropped
	// (Cap'n Proto RPC, see plan-odin.md's dropped table); every real use of
	// this field is behind `if (isIntercropping)`, always false in every
	// fixture in this repo, so it stays a typed-but-inert pointer.
	intercropping:         rawptr,
	soil_column:           ^Soil_Column,
	site_params:           ^p.Site_Parameters,
	sim_params:            ^p.Simulation_Parameters,
	mod_params:            ^p.Crop_Module_Parameters,
	crop_params:           p.Crop_Parameters,
	residue_params:        p.Crop_Residue_Parameters,
	// C++: kj::Own<CropParameters> perennial_crop_params - nil unless a
	// Sowing workstep's separatePerennialCropParams is set (phase 6).
	perennial_crop_params: ^p.Crop_Parameters,
	error_status:          bool,
	error_message:         string,

	using radiation:       Crop_Radiation,
	using phenology:       Crop_Phenology,
	using canopy:          Crop_Canopy,
	using photosynthesis:  Crop_Photosynthesis,
	using o3:              Crop_O3,
	using voc:             Crop_Voc,
	using stress:          Crop_Stress,
	using n_status:        Crop_N_Status,
	using growth:          Crop_Growth,
	using roots:           Crop_Root_System,
	using et0:             Crop_Et0,
	using water_uptake:    Crop_Water_Uptake,
	using n_uptake:        Crop_N_Uptake,
	using carbon:          Crop_Carbon,
	using management:      Crop_Management,
	using ic:              Crop_Intercropping,
}

// Radiation and day length (fc_radiation): a function of the day, latitude and the weather.
Crop_Radiation :: struct {
	declination:                f64 `unit:"°"`, //! old EFF0
	astronomic_day_lenght:      f64 `unit:"h"`, //! old DL
	effective_day_length:       f64 `unit:"h"`, //! old DLE
	photoperiodic_daylength:    f64 `unit:"h"`, //! old DLP
	extraterrestrial_radiation: f64 `unit:"MJ m-2"`,
	clear_day_radiation:        f64 `unit:"J m-2"`, //! old DRC
	overcast_day_radiation:     f64 `unit:"J m-2"`, //! old DRO
	phot_act_radiation_mean:    f64 `unit:"J m-2"`, //! old RDN
	global_radiation:           f64 `unit:"MJ m-2"`,
}

// What crop_radiation needs (plan-component-interfaces.md D1).
Radiation_In :: struct {
	julian_day:       f64 `unit:"d"`,
	latitude:         f64 `unit:"°" src:"site"`,
	// measured; <= 0: estimated from the sunshine hours
	global_radiation: f64 `unit:"MJ m-2" src:"weather"`,
	sunshine_hours:   f64 `unit:"h" src:"weather"`,
}

// Photosynthesis and respiration (fc_crop_photosynthesis, classic or FvCB).
Crop_Photosynthesis :: struct {
	co2_method:                         int,
	assimilation_rate:                  f64, //! old AMAX
	gross_photosynthesis:               f64 `unit:"kgCH2O ha-1"`, //! old GPHOT
	gross_photosynthesis_mol:           f64,
	gross_photosynthesis_reference_mol: f64,
	gross_assimilates:                  f64,
	assimilates:                        f64 `unit:"kgCH2O ha-1"`,
	growth_respiration_as:              f64 `unit:"kgCH2O ha-1"`,
	maintenance_respiration_as:         f64 `unit:"kgCH2O ha-1"`,
	net_maintenance_respiration:        f64, //! old MAINT
	total_respired:                     f64,
	sunlit_leaf_area_index:             [dynamic]f64,
	shaded_leaf_area_index:             [dynamic]f64,
	k_tkc:                              f64, // old KTkc
	k_tko:                              f64, // old KTkc
	fraction_of_intercepted_radiation1: f64,
	fraction_of_intercepted_radiation2: f64,
}

// Growth and partitioning: organ biomass, senescence, reserves, N demand (first part of
// fc_crop_dry_matter).
Crop_Growth :: struct {
	storage_organ:                  int,
	organ_biomass:                  [dynamic]f64, //! old WORG
	organ_green_biomass:            [dynamic]f64,
	organ_dead_biomass:             [dynamic]f64, //! old WDORG
	organ_growth_increment:         [dynamic]f64 `unit:"kgDM ha-1"`, //! old GORG
	organ_senescence_increment:     [dynamic]f64, //! old DGORG
	aboveground_biomass:            f64 `unit:"kgDM ha-1"`, //! old OBMAS
	aboveground_biomass_old:        f64, //! old OBALT
	belowground_biomass:            f64,
	belowground_biomass_old:        f64,
	total_biomass:                  f64,
	reserve_assimilate_pool:        f64, //! old ASPOO
	net_photosynthesis:             f64 `unit:"kgCH2O ha-1"`, //! old GTW
	crop_n_demand:                  f64, //! old DTGESN
	max_n_uptake:                   f64, //! old MAXUP
	assimilate_part_coeffs_reduced: bool,
	dying_out:                      bool,
}

// Water uptake: interception, potential and actual transpiration per layer, water stress
// (fc_crop_water_uptake).
Crop_Water_Uptake :: struct {
	interception_storage:            f64,
	net_precipitation:               f64,
	evaporated_from_intercept:       f64 `unit:"mm"`,
	remaining_evapotranspiration:    f64 `unit:"mm"`,
	potential_transpiration:         f64,
	actual_transpiration:            f64 `unit:"mm"`,
	transpiration:                   [dynamic]f64 `unit:"mm"`, //! old TP
	transpiration_redux:             [dynamic]f64, //! old TRRED
	root_effectivity:                [dynamic]f64, //! old WUEFF
	crop_water_uptake:               [dynamic]f64, //! old TP
	transpiration_deficit:           f64 `unit:"0-1"`, //! old TRREL
	potential_transpiration_deficit: f64 `unit:"0-1"`,
	actual_transpiration_deficit:    f64 `unit:"0-1"`,
	transpiration_reduced:           f64 `unit:"mm"`,
	accumulated_et_a:                f64,
	accumulated_transpiration:       f64,
}

// N uptake: per layer, fixation, N contents (fc_crop_n_uptake).
Crop_N_Uptake :: struct {
	n_uptake_from_layer:                 [dynamic]f64, //! old PE
	fixed_n:                             f64 `unit:"kgN ha-1"`,
	n_concentration_aboveground_biomass: f64 `unit:"kgN kgDM-1"`, //! old GEHOB
	n_concentration_root:                f64 `unit:"kgN kgDM-1"`, //! old WUGEH
	total_biomass_n_content:             f64 `unit:"kgN ha-1"`, //! old PESUM
	total_n_input:                       f64,
	total_n_uptake:                      f64 `unit:"kgN ha-1"`, //! old SUMPE
	sum_total_n_uptake:                  f64 `unit:"kgN ha-1"`, //! summation of all calculated NUptake; needed for sensitivity analysis
	n_content_deficit:                   f64,
}

// Carbon balance (fc_gross_primary_production, fc_net_primary_production).
Crop_Carbon :: struct {
	gross_primary_production: f64 `unit:"kgC ha-1"`,
	net_primary_production:   f64 `unit:"kgC ha-1"`,
	respiration:              f64,
}

// Management state: transplant shock, cutting (force_transplant_state, apply_cutting).
Crop_Management :: struct {
	transplant_shock_duration: int,
	days_since_transplant:     int,
	transplant_efficiency:     f64,
	cutting_delay_days:        int,
	exported_cut_biomass:      f64 `unit:"kgDM ha-1"`,
	sum_exported_cut_biomass:  f64 `unit:"kgDM ha-1"`,
	residue_cut_biomass:       f64 `unit:"kgDM ha-1"`,
	sum_residue_cut_biomass:   f64 `unit:"kgDM ha-1"`,
}

// Intercropping: the other crop's height and LAI (set_other_crop_height_and_lait).
Crop_Intercropping :: struct {
	intercropping_other_crop_height: f64,
	intercropping_other_lait:        f64,
}

// C++ in-class initialisers
make_crop_module_defaults :: proc() -> Crop_Module {
	return Crop_Module {
		days_since_transplant = -1,
		transplant_efficiency = 1.0,
		transpiration_deficit = 1.0,
		co2_method = 3,
		crop_frost_redux = 1.0,
		crop_heat_redux = 1.0,
		crop_n_redux = 1.0,
		drought_impact_on_fertility = 1.0,
		kc_factor = 0.6,
		lt50 = -3.0,
		lt50_m = -3.0,
		kcb_factor = 0.15,
		kcb_ini = 0.15,
		storage_organ = 4,
		time_step = 1.0,
		time_under_anoxia_threshold_default = 4,
		cutting_delay_days = 0,
		anthesis_day = -1,
		maturity_day = -1,
		step_size24 = 24,
		step_size240 = 240,
		o3_short_term_damage = 1.0,
		o3_long_term_damage = 1.0,
		o3_senescence = 1.0,
		o3_w_stomatal_closure = 1.0,
		intercropping_other_crop_height = -1,
		intercropping_other_lait = -1,
	}
}

@(private)
clone_f64_array :: proc(a: [dynamic]f64, allocator := context.allocator) -> [dynamic]f64 {
	out := make([dynamic]f64, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_bool_array :: proc(a: [dynamic]bool, allocator := context.allocator) -> [dynamic]bool {
	out := make([dynamic]bool, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_f64_2d_array :: proc(
	a: [dynamic][dynamic]f64,
	allocator := context.allocator,
) -> [dynamic][dynamic]f64 {
	out := make([dynamic][dynamic]f64, len(a), allocator)
	for row, i in a {
		out[i] = clone_f64_array(row, allocator)
	}
	return out
}

@(private)
clone_yield_component_array :: proc(
	a: [dynamic]p.Yield_Component,
	allocator := context.allocator,
) -> [dynamic]p.Yield_Component {
	out := make([dynamic]p.Yield_Component, len(a), allocator)
	copy(out[:], a[:])
	return out
}

@(private)
clone_species_parameters :: proc(
	sp: p.Species_Parameters,
	allocator := context.allocator,
) -> p.Species_Parameters {
	out := sp
	out.base_temperature = clone_f64_array(sp.base_temperature, allocator)
	out.organ_maintenance_respiration = clone_f64_array(
		sp.organ_maintenance_respiration,
		allocator,
	)
	out.organ_growth_respiration = clone_f64_array(sp.organ_growth_respiration, allocator)
	out.stage_max_root_n_concentration = clone_f64_array(
		sp.stage_max_root_n_concentration,
		allocator,
	)
	out.initial_organ_biomass = clone_f64_array(sp.initial_organ_biomass, allocator)
	out.critical_oxygen_content = clone_f64_array(sp.critical_oxygen_content, allocator)
	out.stage_mobil_from_storage_coeff = clone_f64_array(
		sp.stage_mobil_from_storage_coeff,
		allocator,
	)
	out.aboveground_organ = clone_bool_array(sp.aboveground_organ, allocator)
	out.storage_organ = clone_bool_array(sp.storage_organ, allocator)
	return out
}

@(private)
clone_cultivar_parameters :: proc(
	cp: p.Cultivar_Parameters,
	allocator := context.allocator,
) -> p.Cultivar_Parameters {
	out := cp
	out.assimilate_partitioning_coeff = clone_f64_2d_array(
		cp.assimilate_partitioning_coeff,
		allocator,
	)
	out.organ_senescence_rate = clone_f64_2d_array(cp.organ_senescence_rate, allocator)
	out.base_daylength = clone_f64_array(cp.base_daylength, allocator)
	out.optimum_temperature = clone_f64_array(cp.optimum_temperature, allocator)
	out.daylength_requirement = clone_f64_array(cp.daylength_requirement, allocator)
	out.drought_stress_threshold = clone_f64_array(cp.drought_stress_threshold, allocator)
	out.specific_leaf_area = clone_f64_array(cp.specific_leaf_area, allocator)
	out.stage_kc_factor = clone_f64_array(cp.stage_kc_factor, allocator)
	out.stage_temperature_sum = clone_f64_array(cp.stage_temperature_sum, allocator)
	out.vernalisation_requirement = clone_f64_array(cp.vernalisation_requirement, allocator)
	out.organ_ids_for_primary_yield = clone_yield_component_array(
		cp.organ_ids_for_primary_yield,
		allocator,
	)
	out.organ_ids_for_secondary_yield = clone_yield_component_array(
		cp.organ_ids_for_secondary_yield,
		allocator,
	)
	out.organ_ids_for_cutting = clone_yield_component_array(cp.organ_ids_for_cutting, allocator)
	return out
}

// C++'s CropModule constructor does `cm->crop_params = *crop_params;`, which
// deep-copies via CropParameters' implicit copy constructor (every
// std::vector member is deep-copied). Odin's plain struct assignment only
// copies [dynamic]T headers, aliasing the backing storage. This is not
// hypothetical: `crop_params` typically points at a Sowing workstep's own,
// long-lived CropParameters, reused every time that workstep fires again
// across a multi-year crop rotation (see src/worksteps/sowing.cpp). Without
// this clone, a later checkpoint mutating cm.crop_params (perennial-crop
// handling, cutting) would corrupt the workstep's source object for the next
// season instead of only this crop's own copy.
clone_crop_parameters :: proc(
	cp: p.Crop_Parameters,
	allocator := context.allocator,
) -> p.Crop_Parameters {
	out := cp
	out.species_params = clone_species_parameters(cp.species_params, allocator)
	out.cultivar_params = clone_cultivar_parameters(cp.cultivar_params, allocator)
	return out
}

// C++: kj::Own<CropModule> monica::makeCropModule(SoilColumn*, const
// CropParameters*, const CropResidueParameters*, const SiteParameters*,
// const CropModuleParameters*, const SimulationParameters*,
// std::function<void(std::string)>, std::function<void(std::map<size_t,
// double>, double)>, std::function<std::pair<double, double>(double)>,
// Intercropping*) - the first overload only; the deserialize overload is
// dropped (Cap'n Proto).
make_crop_module :: proc(
	soil_column: ^Soil_Column,
	crop_params: ^p.Crop_Parameters,
	residue_params: ^p.Crop_Residue_Parameters,
	site_params: ^p.Site_Parameters,
	crop_module_params: ^p.Crop_Module_Parameters,
	sim_params: ^p.Simulation_Parameters,
	intercropping: rawptr,
	allocator := context.allocator,
) -> Crop_Module {
	cm := make_crop_module_defaults()
	cm.intercropping = intercropping
	cm.soil_column = soil_column
	cm.mod_params = crop_module_params
	cm.crop_params = clone_crop_parameters(crop_params^, allocator)
	cm.site_params = site_params
	cm.sim_params = sim_params
	cm.residue_params = residue_params^

	cm.no_of_organs = p.species_parameters_number_of_organs(&cm.crop_params.species_params)
	cm.no_of_dev_stages = p.species_parameters_number_of_developmental_stages(
		&cm.crop_params.species_params,
	)
	cm.current_temperature_sum = make([dynamic]f64, cm.no_of_dev_stages, allocator)
	cm.sunlit_leaf_area_index = make([dynamic]f64, 24, allocator)
	cm.shaded_leaf_area_index = make([dynamic]f64, 24, allocator)
	cm.n_uptake_from_layer = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	cm.organ_biomass = make([dynamic]f64, cm.no_of_organs, allocator)
	cm.organ_dead_biomass = make([dynamic]f64, cm.no_of_organs, allocator)
	cm.organ_green_biomass = make([dynamic]f64, cm.no_of_organs, allocator)
	cm.organ_growth_increment = make([dynamic]f64, cm.no_of_organs, allocator)
	cm.organ_senescence_increment = make([dynamic]f64, cm.no_of_organs, allocator)
	cm.root_density = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	cm.root_diameter = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	cm.root_effectivity = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	// cm.vs_soil_mineral_n_content = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	cm.transpiration = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	cm.transpiration_redux = make([dynamic]f64, len(cm.soil_column.layers), allocator)
	for &v in cm.transpiration_redux {
		v = 1.0
	}
	cm.rad24 = make([dynamic]f64, cm.step_size24, allocator)
	cm.rad240 = make([dynamic]f64, cm.step_size240, allocator)
	cm.tfol24 = make([dynamic]f64, cm.step_size24, allocator)
	cm.tfol240 = make([dynamic]f64, cm.step_size240, allocator)

	// Determining the total temperature sum of all developmental stages after
	// emergence (that's why i_Stage starts with 1) until before senescence
	stage_temp_sum := cm.crop_params.cultivar_params.stage_temperature_sum
	for i_stage := 1; i_stage < cm.no_of_dev_stages - 1; i_stage += 1 {
		cm.total_temperature_sum += stage_temp_sum[i_stage]
		if i_stage < cm.no_of_dev_stages - 3 {
			cm.temperature_sum_to_flowering += stage_temp_sum[i_stage]
		}
	}

	cm.final_developmental_stage = cm.no_of_dev_stages - 1

	// Determining the initial crop organ's biomass
	for i_organ := 0; i_organ < cm.no_of_organs; i_organ += 1 {
		cm.organ_biomass[i_organ] = cm.crop_params.species_params.initial_organ_biomass[i_organ] // [kg ha-1]

		if cm.crop_params.species_params.aboveground_organ[i_organ] {
			cm.aboveground_biomass += cm.crop_params.species_params.initial_organ_biomass[i_organ] // [kg ha-1]
		}

		cm.total_biomass += cm.crop_params.species_params.initial_organ_biomass[i_organ] // [kg ha-1]

		// Define storage organ
		if cm.crop_params.species_params.storage_organ[i_organ] {
			cm.storage_organ = i_organ
		}
	}

	cm.organ_green_biomass = clone_f64_array(cm.organ_biomass, allocator)

	cm.root_biomass = cm.crop_params.species_params.initial_organ_biomass[0] // [kg ha-1]

	// Initialisisng the leaf area index
	cm.leaf_area_index =
		cm.organ_biomass[Organ_Leaf] *
		cm.crop_params.cultivar_params.specific_leaf_area[cm.developmental_stage] // [ha ha-1]

	if cm.leaf_area_index <= 0.0 {
		cm.leaf_area_index = 0.001
	}

	// Initialising the root
	cm.root_biomass = cm.organ_biomass[Organ_Root]

	// @todo Christian: Umrechnung korrekt wenn Biomasse in [kg m-2]?
	PI :: 3.14159265358979323
	cm.total_root_length = (cm.root_biomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * PI)

	cm.n_concentration_aboveground_biomass =
		cm.crop_params.species_params.n_concentration_aboveground_biomass
	cm.n_concentration_root = cm.crop_params.species_params.n_concentration_root
	cm.total_biomass_n_content =
		(cm.aboveground_biomass * cm.n_concentration_aboveground_biomass) +
		(cm.root_biomass * cm.n_concentration_root)

	// Initialising the initial maximum rooting depth
	if cm.mod_params.adjust_root_depth_for_soil_props {
		r_p_max := cm.crop_params.cultivar_params.crop_specific_max_rooting_depth
		f_s := cm.soil_column.layers[0].soil_sand_content // [kg kg-1]
		r_s := (f_s - 0.5) * -0.6

		rho_b := soil_bulk_density(&cm.soil_column.layers[0]) // [kg m-3]
		r_d := (rho_b / 1000.0 - 1) * -0.3

		cm.max_rooting_depth =
			r_p_max *
			((r_p_max + (r_p_max * r_s)) / r_p_max) *
			((r_p_max + (r_p_max * r_d)) / r_p_max)
	} else {
		cm.max_rooting_depth = cm.crop_params.cultivar_params.crop_specific_max_rooting_depth // [m]
	}

	if cm.site_params.impenetrable_layer_depth > 0 {
		cm.max_rooting_depth = min(cm.max_rooting_depth, cm.site_params.impenetrable_layer_depth)
	}

	// FAO-56 Dual Kc: Initialize GDD-based trapezoidal Kcb curve from
	// pc_StageKcFactor. kcb_ini defaults to 0.15 (FAO-56 Table 17 bare
	// soil); setInitialKcb() may override it.
	stage_kc_factor := cm.crop_params.cultivar_params.stage_kc_factor
	if len(stage_kc_factor) > 0 {
		max_kc := stage_kc_factor[0]
		for v in stage_kc_factor {
			if v > max_kc {
				max_kc = v
			}
		}
		if max_kc >= 1.0 {
			cm.kcb_mid = max(0.15, max_kc - 0.05) // high-coverage crop (FAO-56 par 7)
		} else {
			cm.kcb_mid = max(0.15, max_kc - 0.10) // low-coverage crop (FAO-56 par 7)
		}
		cm.kcb_end = max(0.0, stage_kc_factor[len(stage_kc_factor) - 1] - 0.05)
	}
	cm.kcb_factor = cm.kcb_ini // start at initial value

	return cm
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 3: phenology + canopy geometry
// ---------------------------------------------------------------------------

// C++: std::pair<const vector<double>&, const vector<double>&>
// monica::cropmodule::sunlitAndShadedLAI(const CropModule*)
sunlit_and_shaded_lai :: proc(cm: ^Crop_Module) -> ([dynamic]f64, [dynamic]f64) {
	return cm.sunlit_leaf_area_index, cm.shaded_leaf_area_index
}

// C++: void monica::cropmodule::setOtherCropHeightAndLAIt(CropModule*, double
// cropHeight, double lait)
set_other_crop_height_and_lait :: proc(cm: ^Crop_Module, crop_height, lait: f64) {
	cm.intercropping_other_crop_height = crop_height
	cm.intercropping_other_lait = lait
}

// C++: double monica::cropmodule::getFractionOfInterceptedRadiation1(const CropModule*)
get_fraction_of_intercepted_radiation1 :: proc(cm: ^Crop_Module) -> f64 {
	return cm.fraction_of_intercepted_radiation1
}

// C++: double monica::cropmodule::getFractionOfInterceptedRadiation2(const CropModule*)
get_fraction_of_intercepted_radiation2 :: proc(cm: ^Crop_Module) -> f64 {
	return cm.fraction_of_intercepted_radiation2
}

// C++: double monica::cropmodule::getCurrentTotalTemperatureSum(const CropModule*)
get_current_total_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	return cm.current_total_temperature_sum
}

// C++: double monica::cropmodule::getCurrentStageTemperatureSum(const CropModule*)
get_current_stage_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	if cm.developmental_stage < len(cm.current_temperature_sum) {
		return cm.current_temperature_sum[cm.developmental_stage]
	}
	return 0.0
}

// C++: double monica::cropmodule::getTotalTemperatureSum(const CropModule*)
get_total_temperature_sum :: proc(cm: ^Crop_Module) -> f64 {
	return cm.total_temperature_sum
}

// C++: double monica::cropmodule::sumStageTemperatureSums(const CropModule*,
// int startAtStage, int endAtInclStage)
sum_stage_temperature_sums :: proc(cm: ^Crop_Module, start_at_stage, end_at_incl_stage: int) -> f64 {
	ts := 0.0
	end_at_incl_stage2 :=
		end_at_incl_stage < 0 ? f64(cm.no_of_dev_stages + end_at_incl_stage + 1) : f64(end_at_incl_stage)
	for s := start_at_stage; f64(s) < end_at_incl_stage2; s += 1 {
		ts += cm.crop_params.cultivar_params.stage_temperature_sum[s]
	}
	return ts
}

// C++: void monica::cropmodule::fcRadiation(CropModule*, double julianDay,
// double globalRadiation, double sunshineHours)
//
// Taken from the original HERMES model - a separate, independent
// implementation of day-length/declination/radiation from soilmoisture.odin's
// own copy (soilmoisture computes its own for a different consumer); not
// deduplicated, matching the C++.
//
// A pure function of the day, the latitude and the weather (D1): returns the
// values instead of writing them into the crop.
crop_radiation :: proc(input: Radiation_In) -> (r: Crop_Radiation) {
	julian_day := input.julian_day
	global_radiation := input.global_radiation
	sunshine_hours := input.sunshine_hours

	PI :: 3.14159265358979323

	// Calculation of declination - old DEC
	r.declination = -23.4 * cr_math.cos(2.0 * PI * ((julian_day + 10.0) / 365.0))

	declination_sinus :=
		cr_math.sin(r.declination * PI / 180.0) * cr_math.sin(input.latitude * PI / 180.0) // old SINLD
	declination_cosinus :=
		cr_math.cos(r.declination * PI / 180.0) * cr_math.cos(input.latitude * PI / 180.0) // old COSLD

	// Calculation of the atmospheric day lenght - old DL
	arg_astro_day_length := declination_sinus / declination_cosinus
	arg_astro_day_length = tl.bound(-1.0, arg_astro_day_length, 1.0)
	r.astronomic_day_lenght = 12.0 * (PI + 2.0 * cr_math.asin(arg_astro_day_length)) / PI

	// Calculation of the effective day length - old DLE
	edl_helper := (-cr_math.sin(f64(8.0 * PI / 180.0)) + declination_sinus) / declination_cosinus

	if edl_helper < -1.0 || edl_helper > 1.0 {
		r.effective_day_length = 0.01
	} else {
		r.effective_day_length = 12.0 * (PI + 2.0 * cr_math.asin(edl_helper)) / PI
	}

	// old DLP
	arg_photo_day_length :=
		(-cr_math.sin(f64(-6.0 * PI / 180.0)) + declination_sinus) / declination_cosinus
	arg_photo_day_length = tl.bound(-1.0, arg_photo_day_length, 1.0)
	r.photoperiodic_daylength = 12.0 * (PI + 2.0 * cr_math.asin(arg_photo_day_length)) / PI

	// Calculation of the mean photosynthetically active radiation [J m-2] - old RDN
	arg_phot_act := min(
		1.0,
		(declination_sinus / declination_cosinus) *
		(declination_sinus / declination_cosinus),
	)
	r.phot_act_radiation_mean =
		3600.0 *
		(declination_sinus * r.astronomic_day_lenght +
				24.0 / PI * declination_cosinus * libc.sqrt(1.0 - arg_phot_act))

	// Calculation of radiation on a clear day [J m-2] - old DRC
	if r.phot_act_radiation_mean > 0 && r.astronomic_day_lenght > 0 {
		r.clear_day_radiation =
			0.5 *
			1300.0 *
			r.phot_act_radiation_mean *
			cr_math.exp(-0.14 / (r.phot_act_radiation_mean / (r.astronomic_day_lenght * 3600.0)))
	} else {
		r.clear_day_radiation = 0
	}

	// Calculation of radiation on an overcast day [J m-2] - old DRO
	r.overcast_day_radiation = 0.2 * r.clear_day_radiation

	// Calculation of extraterrestrial radiation - old EXT
	solar_constant := 0.082
	sc :=
		24.0 *
		60.0 /
		PI *
		solar_constant *
		(1.0 + 0.033 * cr_math.cos(2.0 * PI * julian_day / 365.0))

	arg_solar_angle := -cr_math.tan(input.latitude * PI / 180.0) * cr_math.tan(r.declination * PI / 180.0)
	arg_solar_angle = tl.bound(-1.0, arg_solar_angle, 1.0)
	sunset_solar_angle := cr_math.acos(arg_solar_angle)
	r.extraterrestrial_radiation =
		sc *
		(sunset_solar_angle * declination_sinus +
				declination_cosinus * cr_math.sin(sunset_solar_angle)) // [MJ m-2]

	if global_radiation > 0.0 {
		r.global_radiation = global_radiation
	} else if r.astronomic_day_lenght > 0 {
		r.global_radiation =
			r.extraterrestrial_radiation *
			(0.19 + 0.55 * sunshine_hours / r.astronomic_day_lenght)
	} else {
		r.global_radiation = 0
	}	return
}

// C++: void monica::cropmodule::fcUpdateCropParametersForPerennial(CropModule*)
fc_update_crop_parameters_for_perennial :: proc(cm: ^Crop_Module, allocator := context.allocator) {
	if cm.perennial_crop_params == nil {
		return
	}
	cm.crop_params = clone_crop_parameters(cm.perennial_crop_params^, allocator)
	cm.no_of_organs = p.species_parameters_number_of_organs(&cm.crop_params.species_params)
	cm.no_of_dev_stages = p.species_parameters_number_of_developmental_stages(
		&cm.crop_params.species_params,
	)
}

// C++: bool monica::cropmodule::isAnthesisDay(const CropModule*, size_t
// old_dev_stage, size_t new_dev_stage)
//
// Method is called after calculation of the developmental stage.
is_anthesis_day :: proc(cm: ^Crop_Module, old_dev_stage, new_dev_stage: int) -> bool {
	return is_anthesis_stage_change(cm.no_of_dev_stages, old_dev_stage, new_dev_stage)
}

// C++: kj::Tuple<int,int> monica::cropmodule::anthesisBetweenStages(const CropModule*)
anthesis_between_stages :: proc(cm: ^Crop_Module) -> (int, int) {
	if cm.no_of_dev_stages == 6 {
		return 3, 4
	} else if cm.no_of_dev_stages == 7 {
		return 4, 5
	}
	return -1, -1
}

// C++: bool monica::cropmodule::isMaturityDay(const CropModule*, size_t
// old_dev_stage, size_t new_dev_stage)
//
// Method is called after calculation of the developmental stage.
is_maturity_day :: proc(cm: ^Crop_Module, old_dev_stage, new_dev_stage: int) -> bool {
	return is_maturity_stage_change(cm.no_of_dev_stages, old_dev_stage, new_dev_stage)
}

// C++: int monica::cropmodule::getAnthesisDay(const CropModule*)
get_anthesis_day :: proc(cm: ^Crop_Module) -> int {
	return cm.anthesis_day
}

// C++: int monica::cropmodule::getMaturityDay(const CropModule*)
get_maturity_day :: proc(cm: ^Crop_Module) -> int {
	return cm.maturity_day
}

// C++: bool monica::cropmodule::maturityReached(const CropModule*)
maturity_reached :: proc(cm: ^Crop_Module) -> bool {
	return cm.maturity_reached
}

// C++: void monica::cropmodule::setStage(CropModule*, size_t newStage)
set_stage :: proc(cm: ^Crop_Module, new_stage: int) {
	cm.current_total_temperature_sum = 0.0
	for stage := 0; stage < cm.no_of_dev_stages; stage += 1 {
		if stage < new_stage {
			cm.current_total_temperature_sum += cm.current_temperature_sum[stage]
		} else {
			cm.current_temperature_sum[stage] = 0.0
		}
	}

	cm.developmental_stage = new_stage
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 4: photosynthesis + assimilation
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::fcCropPhotosynthesis(CropModule*, double
// vw_MeanAirTemperature, double vw_MaxAirTemperature, double
// vw_MinAirTemperature, double vw_AtmosphericCO2Concentration, double
// vw_AtmosphericO3Concentration, Tools::Date currentDate)
//
// The largest function in the port (~1100 lines in C++). Two departures from
// a literal transliteration, both because the eliminated code is provably
// dead or undefined, not because it's inconvenient to port:
//   - the Intercropping branch of the C++ (a second, alternate call to its
//     `code` lambda with a different fraction-of-intercepted-radiation
//     function, gated on `intercropping_other_crop_height > zeroHeightEps`) is
//     unreachable in this port: Intercropping itself is a dropped feature
//     (Cap'n Proto RPC, see plan-odin.md's "Explicitly dropped" table), and
//     `intercropping_other_crop_height` starts at -1 and nothing in this port
//     ever sets it positive (setOtherCropHeightAndLAIt is only ever called by
//     intercropping wiring). With only one surviving call to `code`, its body
//     is inlined directly rather than reproduced as a `std::function`-taking
//     closure (Odin's `proc` type has no capture).
//   - the hourly sunrise-detection check reads C++'s `hourlyGlobrads.back()`
//     on a still-empty, freshly-constructed `std::vector` at hour 0 -
//     undefined behaviour (likely a null-pointer dereference), not a
//     reproducible quirk. Reimplemented as an explicit "previous hour"
//     tracker seeded at 0.0, which is what the logic clearly intends: "is
//     this the first hour with positive radiation".
fc_crop_photosynthesis :: proc(
	cm: ^Crop_Module,
	mean_air_temperature, max_air_temperature, min_air_temperature: f64,
	atmospheric_co2_concentration, atmospheric_o3_concentration: f64,
	current_date: d.Date,
	// yesterday's (Crop_Carry_Over): water uptake, crop N status, frost kill, the
	// root system and the crop's ET0 (for the O3 uptake) run later in the step
	water_stress, n_stress, frost_redux: f64,
	rooting_depth: int,
	reference_et0: f64,
	allocator := context.allocator,
) {
	crop_ps := cm.mod_params
	soil_column := cm.soil_column
	species_ps := &cm.crop_params.species_params
	cultivar_ps := &cm.crop_params.cultivar_params
	assimilate_partitioning_coeff := cultivar_ps.assimilate_partitioning_coeff
	minimum_temperature_for_assimilation := species_ps.minimum_temperature_for_assimilation
	maximum_temperature_for_assimilation := species_ps.maximum_temperature_for_assimilation
	optimum_temperature_for_assimilation := species_ps.optimum_temperature_for_assimilation
	organ_growth_respiration := species_ps.organ_growth_respiration
	organ_maintenance_respiration := species_ps.organ_maintenance_respiration
	specific_leaf_area := cultivar_ps.specific_leaf_area
	water_deficit_response_on := cm.sim_params.water_deficit_response_on
	latitude := cm.site_params.latitude

	assimilation_rate_reference := 0.0

	radiation_use_efficiency := species_ps.default_radiation_use_efficiency
	radiation_use_efficiency_reference := species_ps.default_radiation_use_efficiency

	D_IN_K :: VOC_D_IN_K
	RGAS :: VOC_RGAS
	TK25 :: VOC_TK25

	if species_ps.carboxylation_pathway == 1 {
		// Calculation of CO2 impact on crop growth
		if cm.co2_method == 3 {
			// --------------------------------------------------------------------
			// Method 3:
			// Long, S.P. 1991. Modification of the response of photosynthetic
			// productivity to rising temperature by atmospheric CO2
			// concentrations - Has its importance been underestimated. Plant
			// Cell Environ. 14(8): 729-739.
			// and
			// Mitchell, R.A.C., D.W. Lawlor, V.J. Mitchell, C.L. Gibbard, E.M.
			// White, and J.R. Porter. 1995. Effects of elevated CO2
			// concentration and increased temperature on winter-wheat - Test
			// of ARCWHEAT1 simulation model. Plant Cell Environ. 18(7):736-748.
			// ----------------------------------------------------------------------
			temp_k := mean_air_temperature + D_IN_K
			term1 := (temp_k - TK25) / (TK25 * temp_k * RGAS)
			term2 := libc.sqrt(temp_k / TK25)
			cm.k_tkc = cr_math.exp(species_ps.aekc * term1) * term2
			cm.k_tko = cr_math.exp(species_ps.aeko * term1) * term2
			mkc := species_ps.kc25 * cm.k_tkc // [umol mol-1]
			cm.crop_photosynthesis_results.kc = mkc
			mko := species_ps.ko25 * cm.k_tko // [mmol mol-1]
			cm.crop_photosynthesis_results.ko = mko * 1000.0 // mmol -> umol

			// OLD exponential response
			k_tvmax: f64
			if crop_ps.__enable_Photosynthesis_WangEngelTemperatureResponse__ {
				k_tvmax = max(
					0.00001,
					wang_engel_temperature_response(
						mean_air_temperature,
						minimum_temperature_for_assimilation,
						optimum_temperature_for_assimilation,
						maximum_temperature_for_assimilation,
						1.0,
					),
				)
			} else {
				k_tvmax = cr_math.exp(species_ps.aevc * term1) * term2
			}

			// Berechnung des Transformationsfaktors fuer pflanzenspez. AMAX bei
			// 25 grad - old fakamax
			amax_factor := cultivar_ps.max_assimilation_rate / 34.668
			amax_factor_reference := crop_ps.reference_max_assimilation_rate / 34.668
			// old vcmax
			vcmax := 98.0 * amax_factor * k_tvmax
			cm.crop_photosynthesis_results.vc_max = vcmax
			vcmax_reference := 98.0 * amax_factor_reference * k_tvmax

			oi :=
				210.0 *
				(0.047 -
						0.0013087 * mean_air_temperature +
						0.000025603 * (mean_air_temperature * mean_air_temperature) -
						0.00000021441 *
							(mean_air_temperature *
									mean_air_temperature *
									mean_air_temperature)) /
				0.026934 // [mmol mol-1]
			cm.crop_photosynthesis_results.oi = oi * 1000.0 // mmol -> umol

			ci :=
				atmospheric_co2_concentration *
				0.7 *
				(1.674 -
						0.061294 * mean_air_temperature +
						0.0011688 * (mean_air_temperature * mean_air_temperature) -
						0.0000088741 *
							(mean_air_temperature *
									mean_air_temperature *
									mean_air_temperature)) /
				0.73547 // [umol mol-1]
			cm.crop_photosynthesis_results.ci = ci

			// similar to LDNDC::jarvis.cpp:217 - old COcomp
			co2_compensation_point := 0.5 * 0.21 * vcmax * mkc * oi / (vcmax * mko) // [umol mol-1]
			co2_compensation_point_reference :=
				0.5 * 0.21 * vcmax_reference * mkc * oi / (vcmax_reference * mko) // [umol mol-1]
			cm.crop_photosynthesis_results.comp = co2_compensation_point

			// Mitchell et al. 1995 - old EFF
			radiation_use_efficiency = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(ci - co2_compensation_point) /
					(4.5 * ci + 10.5 * co2_compensation_point) *
					8.3769,
					0.5,
				),
			)
			radiation_use_efficiency_reference = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(ci - co2_compensation_point_reference) /
					(4.5 * ci + 10.5 * co2_compensation_point_reference) *
					8.3769,
					0.5,
				),
			)

			cm.assimilation_rate =
				(ci - co2_compensation_point) * vcmax / (ci + mkc * (1.0 + oi / mko)) * 1.656
			assimilation_rate_reference =
				(ci - co2_compensation_point_reference) *
				vcmax_reference /
				(ci + mkc * (1.0 + oi / mko)) *
				1.656

			if mean_air_temperature < minimum_temperature_for_assimilation {
				cm.assimilation_rate = 0.0 // MP: warum gibt es fuer C3-Pflanzen keine maximale Temperatur
				assimilation_rate_reference = 0.0
			}
		} else if cm.co2_method == 2 {
			// -----------------------------------------------------------------
			// Method 2:
			// Hoffmann, F. 1995. Fagus, a model for growth and development of
			// beech. Ecol. Mod. 83 (3):327-348.
			// -----------------------------------------------------------------
			t_response := wang_engel_temperature_response(
				mean_air_temperature,
				minimum_temperature_for_assimilation,
				optimum_temperature_for_assimilation,
				maximum_temperature_for_assimilation,
				1.0,
			)

			cm.assimilation_rate = cultivar_ps.max_assimilation_rate * t_response
			assimilation_rate_reference = crop_ps.reference_max_assimilation_rate * t_response

			// old KCo1
			hoffmann_k1 := 220.0 + 0.158 * (cm.global_radiation * 86400.0 / 1000000.0)
			// PAR [MJ m-2], Hoffmann's model requires [W m-2] ->
			// conversion of [MJ m-2] to [W m-2]
			// old coco
			hoffmann_c0 := 80.0 - 0.036 * (cm.global_radiation * 86400.0 / 1000000.0)
			// old KCO2
			hoffmann_kco2 :=
				((atmospheric_co2_concentration - hoffmann_c0) /
					(hoffmann_k1 + atmospheric_co2_concentration - hoffmann_c0)) /
				((350.0 - hoffmann_c0) / (hoffmann_k1 + 350.0 - hoffmann_c0))

			cm.assimilation_rate = cm.assimilation_rate * hoffmann_kco2
			assimilation_rate_reference = assimilation_rate_reference * hoffmann_kco2
		}
	} else { 	// pc_CarboxylationPathway == 2
		t_response := wang_engel_temperature_response(
			mean_air_temperature,
			minimum_temperature_for_assimilation,
			optimum_temperature_for_assimilation,
			maximum_temperature_for_assimilation,
			1.0,
		)

		cm.assimilation_rate = cultivar_ps.max_assimilation_rate * t_response
		assimilation_rate_reference = crop_ps.reference_max_assimilation_rate * t_response
	}

	if cm.cutting_delay_days > 0 {
		cm.assimilation_rate = 0.1
	}

	cm.assimilation_rate = max(0.1, cm.assimilation_rate)
	assimilation_rate_reference = max(0.1, assimilation_rate_reference)

	// ---------------------------------------------------------------------
	// Calculation of light interception in the crop (Penning De Vries & van
	// Laar 1982)
	// ---------------------------------------------------------------------

	PI :: 3.14159265358979323

	// old EFFE, crop_ps.canopy_reflection_coefficient: old REFLC
	net_radiation_use_efficiency :=
		(1.0 - crop_ps.canopy_reflection_coefficient) * radiation_use_efficiency
	net_radiation_use_efficiency_reference :=
		(1.0 - crop_ps.canopy_reflection_coefficient) * radiation_use_efficiency_reference

	sslae := cr_math.sin((90.0 + cm.declination - latitude) * PI / 180.0) // = HERMES

	x := cr_math.log(
		1.0 +
		0.45 *
			cm.clear_day_radiation /
			(cm.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			(sslae * cm.assimilation_rate),
	) // = HERMES
	x_reference := cr_math.log(
		1.0 +
		0.45 *
			cm.clear_day_radiation /
			(cm.effective_day_length * 3600.0) *
			net_radiation_use_efficiency_reference /
			(sslae * assimilation_rate_reference),
	)

	phch1 := sslae * cm.assimilation_rate * cm.effective_day_length * x / (1.0 + x) // = HERMES
	phch1_reference :=
		sslae *
		assimilation_rate_reference *
		cm.effective_day_length *
		x_reference /
		(1.0 + x_reference)

	y := cr_math.log(
		1.0 +
		0.55 *
			cm.clear_day_radiation /
			(cm.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			((5.0 - sslae) * cm.assimilation_rate),
	) // = HERMES
	y_reference := cr_math.log(
		1.0 +
		0.55 *
			cm.clear_day_radiation /
			(cm.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			((5.0 - sslae) * assimilation_rate_reference),
	)

	phch2 := (5.0 - sslae) * cm.assimilation_rate * cm.effective_day_length * y / (1.0 + y) // = HERMES
	phch2_reference :=
		(5.0 - sslae) *
		assimilation_rate_reference *
		cm.effective_day_length *
		y_reference /
		(1.0 + y_reference)

	phch := 0.95 * (phch1 + phch2) + 20.5 // = HERMES
	phch_reference := 0.95 * (phch1_reference + phch2_reference) + 20.5

	// oxygen_deficit separates drought stress (ETa/Etp) from saturation
	// stress - old VSWELL
	drought_stress_threshold :=
		cm.oxygen_deficit < 1.0 ? 0.0 : cultivar_ps.drought_stress_threshold[cm.developmental_stage]

	// Calculation of time fraction for overcast sky situations by comparing
	// clear day radiation and measured PAR in [J m-2]. HERMES uses PAR as 50%
	// of global radiation - old FOV
	overcast_sky_time_fraction := 0.0
	if cm.clear_day_radiation != 0 {
		overcast_sky_time_fraction =
			(cm.clear_day_radiation - (1000000.0 * cm.global_radiation * 0.50)) /
			(0.8 * cm.clear_day_radiation)
	}
	overcast_sky_time_fraction = max(0.0, min(overcast_sky_time_fraction, 1.0))

	// C++'s `code` lambda inlined at its one surviving call site (see the
	// function comment above) - only ever invoked with F_t1/leaf_area_index.
	lai := cm.leaf_area_index
	fraction_of_intercepted_radiation :=
		1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * lai)

	phc3 := phch * fraction_of_intercepted_radiation
	phc3_reference :=
		phch_reference *
		(1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * crop_ps.reference_leaf_area_index))

	phc4 := cm.astronomic_day_lenght * lai * cm.assimilation_rate
	phc4_reference :=
		cm.astronomic_day_lenght * crop_ps.reference_leaf_area_index * assimilation_rate_reference

	phcl :=
		phc3 < phc4 ? phc3 * (1.0 - cr_math.exp(-phc4 / phc3)) : phc4 * (1.0 - cr_math.exp(-phc3 / phc4))

	phcl_reference :=
		phc3_reference < phc4_reference ? phc3_reference * (1.0 - cr_math.exp(-phc4_reference / phc3_reference)) : phc4_reference * (1.0 - cr_math.exp(-phc3_reference / phc4_reference))

	z :=
		cm.overcast_day_radiation /
		(cm.effective_day_length * 3600.0) *
		net_radiation_use_efficiency /
		(5.0 * cm.assimilation_rate)

	phoh1 := 5.0 * cm.assimilation_rate * cm.effective_day_length * z / (1.0 + z)
	phoh := 0.9935 * phoh1 + 1.1
	pho3 := phoh * fraction_of_intercepted_radiation
	pho3_reference :=
		phoh *
		(1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * crop_ps.reference_leaf_area_index))

	phol :=
		pho3 < phc4 ? pho3 * (1.0 - cr_math.exp(-phc4 / pho3)) : phc4 * (1.0 - cr_math.exp(-pho3 / phc4))

	phol_reference :=
		pho3_reference < phc4_reference ? pho3_reference * (1.0 - cr_math.exp(-phc4_reference / pho3_reference)) : phc4_reference * (1.0 - cr_math.exp(-pho3_reference / phc4_reference))

	clear_day_co2_assimilation := lai < 5.0 ? phcl : phch // [J m-2]
	overcast_day_co2_assimilation := lai < 5.0 ? phol : phoh // [J m-2]

	clear_day_co2_assimilation_reference := phcl_reference
	overcast_day_co2_assimilation_reference := phol_reference

	// old DTGA
	gross_co2_assimilation :=
		overcast_sky_time_fraction * overcast_day_co2_assimilation +
		(1.0 - overcast_sky_time_fraction) * clear_day_co2_assimilation

	// used for ET0 calculation
	gross_co2_assimilation_reference :=
		overcast_sky_time_fraction * overcast_day_co2_assimilation_reference +
		(1.0 - overcast_sky_time_fraction) * clear_day_co2_assimilation_reference

	// Gross CO2 assimilation is used for reference evapotranspiration
	// calculation. For this purpose it must not be affected by drought
	// stress, as the grass reference is defined as being always well
	// supplied with water. Water stress is acting at a later stage.
	// NOTE(c++-quirk): the C++ multiplies vc_GrossCO2Assimilation by itself
	// here (a no-op, per the source comment "* transpiration_deficit" being
	// commented out) - reproduced as the no-op it is, not simplified away.
	if water_stress < drought_stress_threshold {
		gross_co2_assimilation = gross_co2_assimilation
	}

	julian_day := int(d.julian_day(current_date))
	daily_gp := 0.0
	if crop_ps.__enable_hourly_FvCB_photosynthesis__ && species_ps.carboxylation_pathway == 1 {
		hourly_globrads := make([dynamic]f64, 0, 24, context.temp_allocator)
		hourly_extrarad := make([dynamic]f64, 0, 24, context.temp_allocator)
		sunrise_h := 0
		// see the function comment above re: this vs. C++'s hourlyGlobrads.back()
		prev_hgr := 0.0

		for h := 0; h < 24; h += 1 {
			hgr := tl.hourly_rad(cm.global_radiation, latitude, julian_day, f64(h))
			if hgr > 0 && prev_hgr == 0.0 {
				sunrise_h = h
			}
			append(&hourly_globrads, hgr)
			prev_hgr = hgr

			append(
				&hourly_extrarad,
				tl.hourly_rad(cm.extraterrestrial_radiation, latitude, julian_day, f64(h)),
			)
		}

		voc_hours: [24]Voc_Hour
		for h := 0; h < 24; h += 1 {
			// hourly photosynthesis
			fvcb_in: Fvcb_Canopy_Hourly_In

			hourly_temp := tl.hourly_t(min_air_temperature, max_air_temperature, f64(h), f64(sunrise_h))
			fvcb_in.leaf_temp = hourly_temp
			fvcb_in.global_rad = hourly_globrads[h]
			fvcb_in.extra_terr_rad = hourly_extrarad[h]
			fvcb_in.lai = lai
			fvcb_in.solar_el = tl.solar_elevation(f64(h), latitude, julian_day)
			fvcb_in.vpd = tl.hourly_vapor_pressure_deficit(
				hourly_temp,
				min_air_temperature,
				mean_air_temperature,
				max_air_temperature,
			)
			fvcb_in.ca = atmospheric_co2_concentration

			hps := make_fvcb_canopy_hourly_params()
			hps.vcmax_25 = species_ps.vcmax25 * cm.o3_short_term_damage * cm.o3_senescence

			fvcb_res := fvcb_canopy_hourly_c3(fvcb_in, hps)

			cm.sunlit_leaf_area_index[h] = fvcb_res.sunlit.lai
			cm.shaded_leaf_area_index[h] = fvcb_res.shaded.lai

			// [umol CO2 m-2 (h-1)] -> [kg CO2 ha-1 (d-1)]
			daily_gp += fvcb_res.canopy_gross_photos * 44.0 / 100.0 / 1000.0

			// hourly O3 uptake and damage: its damage lowers the next hour's vcmax
			// (above), so the O3 component is stepped inside the hourly loop
			o3_hourly_step(
				&cm.o3,
				O3_Hourly_In {
					hour = h,
					sunlit = fvcb_res.sunlit,
					shaded = fvcb_res.shaded,
					rooting_depth = rooting_depth,
					soil_layers = soil_column.layers[:],
					reference_et0 = reference_et0,
					atmospheric_o3_concentration = atmospheric_o3_concentration,
					relative_total_development = cm.relative_total_development,
					temperature_sum_to_flowering = cm.temperature_sum_to_flowering,
					total_temperature_sum = cm.total_temperature_sum,
					water_deficit_response_on = water_deficit_response_on,
				},
			)

			// for the VOC emissions after the loop
			voc_hours[h] = Voc_Hour {
				global_rad = fvcb_in.global_rad,
				leaf_temp  = fvcb_in.leaf_temp,
				sunlit     = fvcb_res.sunlit,
				shaded     = fvcb_res.shaded,
			}
		}

		// VOC emissions: only reported, nothing in the crop reads them, so they
		// are computed from the day's 24 hours after the loop
		voc_step(
			&cm.voc,
			&cm.crop_params,
			Voc_In {
				hours = voc_hours,
				lai = lai,
				leaf_green_biomass = cm.organ_green_biomass[Organ_Leaf],
				specific_leaf_area = specific_leaf_area[cm.developmental_stage],
				atmospheric_co2_concentration = atmospheric_co2_concentration,
				n_stress = n_stress,
				water_stress = water_stress,
			},
			allocator,
		)
	}

	gross_co2_assimilation =
		crop_ps.__enable_hourly_FvCB_photosynthesis__ && species_ps.carboxylation_pathway == 1 ? daily_gp : gross_co2_assimilation

	cm.fraction_of_intercepted_radiation1 = fraction_of_intercepted_radiation

	// [TRANSPLANT SHOCK] Photosynthesis Limitation.
	if cm.transplant_efficiency < 1.0 {
		gross_co2_assimilation *= cm.transplant_efficiency
	}

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [kg CH2O ha-1 d-1]
	cm.gross_photosynthesis = gross_co2_assimilation * 30.0 / 44.0

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [mol m-2 s-1]
	cm.gross_photosynthesis_mol = gross_co2_assimilation * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)
	cm.gross_photosynthesis_reference_mol =
		gross_co2_assimilation_reference * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)

	// Converting photosynthesis rate from [kg CO2 ha leaf-1 d-1] to [kg CH2O ha-1 d-1]
	cm.assimilates = gross_co2_assimilation * 30.0 / 44.0

	// reduction value for assimilate amount to simulate field conditions
	cm.assimilates *= species_ps.field_condition_modifier
	// reduction value for assimilate amount to simulate frost damage
	cm.assimilates *= frost_redux
	// MP: added reduction value for assimilate amount to simulate waterlogging
	cm.assimilates *= cm.oxygen_deficit

	if water_stress < drought_stress_threshold { 	// MP: Access point for drought optimisation
		cm.assimilates = cm.assimilates * water_stress
	}

	cm.gross_assimilates = cm.assimilates

	// ---------------------------------------------------------------------
	// AGROSIM night and day maintenance and growth respiration
	// ---------------------------------------------------------------------

	photo_temperature :=
		max_air_temperature - ((max_air_temperature - min_air_temperature) / 4.0)
	night_temperature :=
		min_air_temperature + ((max_air_temperature - min_air_temperature) / 4.0)

	maintenance_respiration_sum := 0.0
	for i_organ := 0; i_organ < cm.no_of_organs; i_organ += 1 {
		maintenance_respiration_sum +=
			cm.organ_green_biomass[i_organ] * organ_maintenance_respiration[i_organ] // [kg CH2O ha-1]
	}

	normalised_day_length := 2.0 - (cm.photoperiodic_daylength / 12.0)

	photo_maintenance_respiration :=
		maintenance_respiration_sum *
		cr_math.pow(
			2.0,
			(crop_ps.maintenance_respiration_parameter1 *
				(photo_temperature - crop_ps.maintenance_respiration_parameter2)),
		) *
		(2.0 - normalised_day_length) // @todo: [g m-2] --> [kg ha-1]

	dark_maintenance_respiration :=
		maintenance_respiration_sum *
		cr_math.pow(
			2.0,
			(crop_ps.maintenance_respiration_parameter1 *
				(night_temperature - crop_ps.maintenance_respiration_parameter2)),
		) *
		normalised_day_length // @todo: [g m-2] --> [kg ha-1]

	cm.maintenance_respiration_as = photo_maintenance_respiration + dark_maintenance_respiration // [kg CH2O ha-1]

	cm.assimilates -= photo_maintenance_respiration + dark_maintenance_respiration // [kg CH2O ha-1]

	growth_respiration_sum := 0.0
	if cm.assimilates > 0 {
		for i_organ := 0; i_organ < cm.no_of_organs; i_organ += 1 {
			growth_respiration_sum +=
				assimilate_partitioning_coeff[cm.developmental_stage][i_organ] *
				cm.assimilates *
				organ_growth_respiration[i_organ]
		}
	}

	photo_growth_respiration := 0.0
	if cm.assimilates > 0.0 {
		photo_growth_respiration =
			growth_respiration_sum *
			cr_math.pow(
				2.0,
				(crop_ps.growth_respiration_parameter1 *
					(photo_temperature - crop_ps.growth_respiration_parameter2)),
			) *
			(2.0 - normalised_day_length) // [kg CH2O ha-1]

		if cm.assimilates > photo_growth_respiration {
			cm.assimilates -= photo_growth_respiration
		} else {
			photo_growth_respiration = cm.assimilates // in this case the plant will be restricted in growth!
			cm.assimilates = 0.0
		}
	}

	// NOTE(c++-quirk): uses vc_PhotoTemperature here too, not
	// vc_NightTemperature - asymmetric with the maintenance-respiration split
	// just above (which correctly uses Photo/Night respectively). Reproduced
	// exactly, not "fixed".
	dark_growth_respiration := 0.0
	if cm.assimilates > 0.0 {
		dark_growth_respiration =
			growth_respiration_sum *
			cr_math.pow(
				2.0,
				(crop_ps.growth_respiration_parameter1 *
					(photo_temperature - crop_ps.growth_respiration_parameter2)),
			) *
			normalised_day_length // [kg CH2O ha-1]

		if cm.assimilates > dark_growth_respiration {
			cm.assimilates -= dark_growth_respiration
		} else {
			dark_growth_respiration = cm.assimilates // in this case the plant will be restricted in growth!
			cm.assimilates = 0.0
		}
	}
	cm.growth_respiration_as = photo_growth_respiration + dark_growth_respiration // [kg CH2O ha-1]
	cm.total_respired = cm.gross_assimilates - cm.assimilates // [kg CH2O ha-1]

	// ---------------------------------------------------------------------
	// HERMES calculation of maintenance respiration in dependence of
	// temperature (to reactivate, net_photosynthesis needs to be used
	// instead of assimilates in the subsequent methods)
	// ---------------------------------------------------------------------

	// old TEFF
	maintenance_temperature_dependency := cr_math.pow(2.0, (0.1 * mean_air_temperature - 2.5))

	// old MAINTS
	maintenance_respiration := 0.0
	for i_organ := 0; i_organ < cm.no_of_organs; i_organ += 1 {
		maintenance_respiration +=
			cm.organ_green_biomass[i_organ] * organ_maintenance_respiration[i_organ]
	}

	if cm.gross_photosynthesis <
	   (maintenance_respiration * maintenance_temperature_dependency) {
		cm.net_maintenance_respiration = cm.gross_photosynthesis
	} else {
		cm.net_maintenance_respiration =
			maintenance_respiration * maintenance_temperature_dependency
	}

	if mean_air_temperature < minimum_temperature_for_assimilation {
		cm.gross_photosynthesis = cm.net_maintenance_respiration
	}
}

// C++: double monica::cropmodule::fcGrossPrimaryProduction(const CropModule*)
fc_gross_primary_production :: proc(cm: ^Crop_Module) -> f64 {
	// Converting photosynthesis rate from [kg CH2O ha-1 d-1] back to [kg C ha-1 d-1]
	return cm.gross_assimilates / 30.0 * 12.0
}

// C++: double monica::cropmodule::fcNetPrimaryProduction(CropModule*, double total_respired)
fc_net_primary_production :: proc(cm: ^Crop_Module, total_respired: f64) -> f64 {
	// Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
	cm.respiration = total_respired / 30.0 * 12.0
	return cm.gross_primary_production - cm.respiration
}

// C++: void monica::cropmodule::calculateVOCEmissions(CropModule*, const Voc::MicroClimateData&)
calculate_voc_emissions :: proc(
	cm: ^Crop_Module,
	mcd: ^Voc_Micro_Climate_Data,
	allocator := context.allocator,
) {
	species_ps := &cm.crop_params.species_params

	species: Voc_Species_Data
	species.lai = cm.leaf_area_index
	species.m_fol = cm.organ_biomass[Organ_Leaf] / (100.0 * 100.0) // kg/ha -> kg/m2
	species.sla = cm.crop_params.cultivar_params.specific_leaf_area[cm.developmental_stage] * 100.0 * 100.0 // ha/kg -> m2/kg

	species.ef_mono = species_ps.ef_mono
	species.ef_monos = species_ps.ef_monos
	species.ef_iso = species_ps.ef_iso
	species.vcmax25 = species_ps.vcmax25
	species.aekc = species_ps.aekc
	species.aeko = species_ps.aeko
	species.aevc = species_ps.aevc
	species.kc25 = species_ps.kc25

	cm.guenther_emissions = voc_guenther_emissions(species, mcd, 1.0, allocator)
	cm.jjv_emissions = voc_jjv_emissions(
		species,
		mcd,
		cm.crop_photosynthesis_results,
		1.0,
		false,
		allocator,
	)
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 5: biomass/dry matter + stress
//
// Ported: fcHeatStressImpact, fcFrostKill, fcDroughtImpactOnFertility,
// fcCropNitrogen (a prerequisite pulled forward from the "water + nitrogen"
// bucket - it computes crop_n_redux/root_n_redux and, despite its name, most
// of the crop's root-growth-rate machinery, which fcCropDryMatter's root
// distribution genuinely depends on to be worth testing), fcCropDryMatter,
// fcMoveDeadRootBiomassToSoil, addAndDistributeRootBiomassInSoil,
// calcRootDensityFactorAndSum. fcCropNUptake (actual N uptake amounts from
// soil layers, a separate and larger concern) stays in checkpoint 6 as
// planned. Deliberately not ported here: the ~15 yield/N-content getters
// (getFruitBiomassNContent, getPrimaryCropYield, ...) and
// numberOfAbovegroundOrgans/organIdsForPrimaryYield - none of them are
// called anywhere inside crop-module.cpp itself (checked directly), they're
// pure output-API surface for build-output.cpp, so they can wait for
// whichever checkpoint actually needs them (phase 7's output table).
// ---------------------------------------------------------------------------

// C++: pair<vector<double>,double> monica::cropmodule::calcRootDensityFactorAndSum(const CropModule*)
calc_root_density_factor_and_sum :: proc(
	cm: ^Crop_Module,
	allocator := context.allocator,
) -> (
	[dynamic]f64,
	f64,
) {
	return root_density_factor_and_sum(
		cm.rooting_depth,
		cm.rooting_zone,
		cm.crop_params.species_params.root_form_factor,
		cm.soil_column.layers[:],
		allocator,
	)
}

// C++: void monica::cropmodule::addAndDistributeRootBiomassInSoil(CropModule*, double rootBiomass)
// - returns the root biomass per organic layer; the caller adds it to soil
// organic (with cm.n_concentration_root).
add_and_distribute_root_biomass_in_soil :: proc(
	cm: ^Crop_Module,
	root_biomass: f64,
	allocator := context.allocator,
) -> map[int]f64 {
	root_density_factor, root_density_factor_sum := calc_root_density_factor_and_sum(cm, allocator)
	return dead_roots_per_layer(
		root_biomass,
		root_density_factor_sum,
		root_density_factor,
		cm.rooting_zone,
		cm.soil_column.number_of_organic_layers,
		allocator,
	)
}

// C++: void monica::cropmodule::fcCropDryMatter(CropModule*, double vw_MeanAirTemperature)
//
// Dry matter allocation within the crop: the result from crop photosynthesis
// is allocated to the different crop organs under consideration of stress
// factors (water, nitrogen, temperature), plus root growth (rooting depth,
// root density/diameter per layer) and handing daily dead root biomass off
// to soil organic matter via fcMoveDeadRootBiomassToSoil/add_organic_matter.
fc_crop_dry_matter :: proc(
	cm: ^Crop_Module,
	// yesterday's (Crop_Carry_Over): N uptake runs later in the step
	n_concentration_root: f64,
) -> (
	dead_root_biomass: f64, // the day's dead root biomass, for the root system
) {
	crop_ps := cm.mod_params
	species_ps := &cm.crop_params.species_params
	cultivar_ps := &cm.crop_params.cultivar_params
	aboveground_organ := species_ps.aboveground_organ
	assimilate_partitioning_coeff_arr := cultivar_ps.assimilate_partitioning_coeff
	assimilate_reallocation := species_ps.assimilate_reallocation
	organ_senescence_rate := cultivar_ps.organ_senescence_rate
	perennial := cultivar_ps.perennial
	stage_temperature_sum := cultivar_ps.stage_temperature_sum
	storage_organ := species_ps.storage_organ


	max_root_n_concentration := 0.0 // old WGM
	assimilate_partitioning_coeff_old := 0.0
	assimilate_partitioning_coeff := 0.0

	// Assuming that growth respiration takes 30% of total assimilation - from
	// AGROSIM algorithms (the HERMES alternative is commented out in the C++)
	cm.net_photosynthesis = cm.assimilates
	// TMP_Regulatory_factor: computed but never read again afterward in the
	// C++ either - a genuine dead store, kept for fidelity.
	tmp_regulatory_factor := species_ps.stage_mobil_from_storage_coeff[cm.developmental_stage]
	if cm.developmental_stage == 1 {
		tmp_regulatory_factor =
			species_ps.stage_mobil_from_storage_coeff[cm.developmental_stage] * cm.k_tkc
	}
	_ = tmp_regulatory_factor

	mobilization_from_storage :=
		cm.organ_biomass[cm.storage_organ] *
		species_ps.stage_mobil_from_storage_coeff[cm.developmental_stage] *
		cm.k_tkc

	cm.reserve_assimilate_pool = 0.0

	cm.aboveground_biomass_old = cm.aboveground_biomass
	cm.aboveground_biomass = 0.0
	cm.belowground_biomass_old = cm.belowground_biomass
	cm.belowground_biomass = 0.0
	cm.total_biomass = 0.0

	// Dry matter production - old NRKOM
	assimilate_partition_leaf := 0.05
	daily_dead_root_biomass_increment := 0.0
	for i_organ := 0; i_organ < cm.no_of_organs; i_organ += 1 {
		// Prevent out-of-bounds array access on developmental stage indices
		// during early phases. If developmental_stage is 0, fall back to
		// index 0 as the previous stage index.
		prev_stage := cm.developmental_stage > 0 ? cm.developmental_stage - 1 : 0
		assimilate_partitioning_coeff_old = assimilate_partitioning_coeff_arr[prev_stage][i_organ]
		assimilate_partitioning_coeff =
			assimilate_partitioning_coeff_arr[cm.developmental_stage][i_organ]

		// Identify storage organ and reduce assimilate flux in case of heat stress
		if storage_organ[i_organ] {
			assimilate_partitioning_coeff_old =
				assimilate_partitioning_coeff_old *
				cm.crop_heat_redux *
				cm.drought_impact_on_fertility
			assimilate_partitioning_coeff =
				assimilate_partitioning_coeff *
				cm.crop_heat_redux *
				cm.drought_impact_on_fertility
		}

		if (cm.current_temperature_sum[cm.developmental_stage] /
			   stage_temperature_sum[cm.developmental_stage]) >
		   1 {
			// Pflanze ist ausgewachsen
			cm.organ_growth_increment[i_organ] = 0.0
			cm.organ_senescence_increment[i_organ] = 0.0
			if perennial {
				cm.growth_cycle_ended = true
			}
		} else {
			// test if there is a positive balance of produced assimilates; if
			// net_photosynthesis is negative, the crop needs more for
			// maintenance than for building new biomass
			if cm.net_photosynthesis < 0.0 {
				// reduce biomass from leaf and shoot because of negative assimilate
				if i_organ == Organ_Leaf {
					incr := assimilate_partition_leaf * cm.net_photosynthesis
					if libc.fabs(incr) <= cm.organ_biomass[i_organ] {
						cm.organ_growth_increment[i_organ] = incr
					} else {
						// temporary hack because complex algorithm produces
						// questionable results - reduce only what is available
						cm.organ_growth_increment[i_organ] = (-1) * cm.organ_biomass[i_organ]
					}
				} else if i_organ == Organ_Shoot {
					incr := assimilate_partition_leaf * cm.net_photosynthesis // should be negative
					if libc.fabs(incr) <= cm.organ_biomass[i_organ] {
						cm.organ_growth_increment[i_organ] = incr
					} else {
						cm.organ_growth_increment[i_organ] = (-1) * cm.organ_biomass[i_organ]
					}
				} else {
					// root or storage organ - do nothing in case of negative photosynthesis
					cm.organ_growth_increment[i_organ] = 0
				}
			} else { 	// net_photosynthesis >= 0.0
				cm.organ_growth_increment[i_organ] =
					cm.net_photosynthesis *
					(assimilate_partitioning_coeff_old +
							((assimilate_partitioning_coeff - assimilate_partitioning_coeff_old) *
									(cm.current_temperature_sum[cm.developmental_stage] /
											stage_temperature_sum[cm.developmental_stage]))) *
					cm.crop_n_redux // [kg CH2O ha-1]

				use_mobilization_from_storage := true
				if use_mobilization_from_storage {
					if i_organ != cm.storage_organ {
						cm.organ_growth_increment[i_organ] +=
							mobilization_from_storage *
							(assimilate_partitioning_coeff_old +
									((assimilate_partitioning_coeff -
												assimilate_partitioning_coeff_old) *
											(cm.current_temperature_sum[cm.developmental_stage] /
													stage_temperature_sum[cm.developmental_stage]))) *
							cm.crop_n_redux
					} else {
						cm.organ_growth_increment[i_organ] -=
							mobilization_from_storage * cm.crop_n_redux
						cm.organ_growth_increment[i_organ] +=
							mobilization_from_storage *
							(assimilate_partitioning_coeff_old +
									((assimilate_partitioning_coeff -
												assimilate_partitioning_coeff_old) *
											(cm.current_temperature_sum[cm.developmental_stage] /
													stage_temperature_sum[cm.developmental_stage]))) *
							cm.crop_n_redux
					}
				}
			}
			cm.organ_senescence_increment[i_organ] =
				cm.organ_green_biomass[i_organ] *
				(organ_senescence_rate[prev_stage][i_organ] +
						((organ_senescence_rate[cm.developmental_stage][i_organ] -
									organ_senescence_rate[prev_stage][i_organ]) *
								(cm.current_temperature_sum[cm.developmental_stage] /
										stage_temperature_sum[cm.developmental_stage]))) // [kg CH2O ha-1]
		}

		cm.organ_biomass[i_organ] += cm.organ_growth_increment[i_organ] * cm.time_step // [kg CH2O ha-1]
		if i_organ == cm.storage_organ {
			cm.organ_dead_biomass[i_organ] += cm.organ_senescence_increment[i_organ] * cm.time_step
		} else {
			// root, shoot, leaf
			reallocation_rate :=
				assimilate_reallocation * cm.organ_senescence_increment[i_organ] * cm.time_step
			cm.organ_biomass[cm.storage_organ] += reallocation_rate
			daily_dead_biomass_increment := cm.organ_senescence_increment[i_organ] - reallocation_rate

			if i_organ == Organ_Root {
				cm.organ_biomass[Organ_Root] -= cm.organ_senescence_increment[Organ_Root]
				cm.total_biomass_n_content -= daily_dead_biomass_increment * n_concentration_root
				daily_dead_root_biomass_increment = daily_dead_biomass_increment
			} else {
				// shoot or leaf
				cm.organ_biomass[i_organ] -= reallocation_rate
				cm.organ_dead_biomass[i_organ] += daily_dead_biomass_increment // [kg CH2O ha-1]
			}
		}

		cm.organ_green_biomass[i_organ] =
			cm.organ_biomass[i_organ] - cm.organ_dead_biomass[i_organ]
		if cm.organ_green_biomass[i_organ] < 0.0 {
			cm.organ_dead_biomass[i_organ] = cm.organ_biomass[i_organ]
			cm.organ_green_biomass[i_organ] = 0.0
		}

		if aboveground_organ[i_organ] {
			cm.aboveground_biomass += cm.organ_biomass[i_organ]
		} else if i_organ != Organ_Root {
			cm.belowground_biomass += cm.organ_biomass[i_organ]
		}

		cm.total_biomass += cm.organ_biomass[i_organ]
	}

	// old @todo: N redux noch ausgeschaltet
	cm.reserve_assimilate_pool = 0.0

	if cm.developmental_stage > 0 {
		max_root_n_concentration =
			species_ps.stage_max_root_n_concentration[cm.developmental_stage - 1] -
			(species_ps.stage_max_root_n_concentration[cm.developmental_stage - 1] -
					species_ps.stage_max_root_n_concentration[cm.developmental_stage]) *
				cm.current_temperature_sum[cm.developmental_stage] /
				stage_temperature_sum[cm.developmental_stage] // [kg kg-1]
	} else {
		max_root_n_concentration = species_ps.stage_max_root_n_concentration[cm.developmental_stage]
	}

	cm.crop_n_demand =
		((cm.target_n_concentration * cm.aboveground_biomass) +
			(cm.organ_biomass[Organ_Root] * max_root_n_concentration) +
			(cm.target_n_concentration * cm.belowground_biomass / cultivar_ps.residue_n_ratio) -
			cm.total_biomass_n_content) *
		cm.time_step // [kg ha-1]

	// vc_NConcentrationOptimum: computed but unused elsewhere in the C++ too -
	// a genuine dead store, kept for fidelity.
	n_concentration_optimum :=
		((cm.target_n_concentration -
					(cm.target_n_concentration - cm.critical_n_concentration) * 0.15) *
				cm.aboveground_biomass +
			(cm.target_n_concentration -
					(cm.target_n_concentration - cm.critical_n_concentration) * 0.15) *
				cm.belowground_biomass /
				cultivar_ps.residue_n_ratio +
			(cm.organ_biomass[Organ_Root] * max_root_n_concentration) -
			cm.total_biomass_n_content) *
		cm.time_step // [kg ha-1]
	_ = n_concentration_optimum

	if cm.crop_n_demand > (crop_ps.max_crop_n_demand * cm.time_step) {
		// Not more than 6kg N per day to be taken up.
		cm.crop_n_demand = crop_ps.max_crop_n_demand * cm.time_step
	}

	if cm.crop_n_demand < 0 {
		cm.crop_n_demand = 0.0
	}


	dead_root_biomass = daily_dead_root_biomass_increment
	return
}

// The end of the C++ fcCropDryMatter: the maximum N uptake, and the N demand
// limited by the root length the root system computed before.
fc_crop_n_demand_by_root_length :: proc(cm: ^Crop_Module) {
	species_ps := &cm.crop_params.species_params

	// Limiting the maximum N-uptake to 26-13*10^-14 mol/cm root/sec
	cm.max_n_uptake =
		species_ps.max_n_uptake_param -
		(cm.current_total_temperature_sum / cm.total_temperature_sum) // [kg m root-1]

	if (cm.crop_n_demand / 10000.0) > (cm.total_root_length * cm.max_n_uptake * cm.time_step) {
		cm.crop_n_demand = cm.total_root_length * cm.max_n_uptake * cm.time_step // [kg m-2]
	} else {
		cm.crop_n_demand = cm.crop_n_demand / 10000.0 // [kg ha-1 -> kg m-2]
	}}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 6: water + nitrogen uptake
//
// Ported: fcReferenceEvapotranspiration, fcCropWaterUptake, fcCropNUptake,
// getEffectiveRootingDepth (deferred here from checkpoint 3 - it reads
// root_effectivity, populated by fcCropWaterUptake below, so this is where
// it's actually meaningful to test). Together with checkpoint 5's
// fcCropNitrogen (root growth + N-redux factor) and fcCropDryMatter, this
// closes out every cropmodule:: function step() calls in its
// developmental_stage>0 block except the fire_event-driven bookkeeping
// (checkpoint 7).
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::fcCropWaterUptake(CropModule*, size_t
// vc_GroundwaterTable, double vw_GrossPrecipitation, double, double)
//
// Water uptake by the crop: potential transpiration from potential
// evapotranspiration by soil cover fraction, reduced by water availability
// in the soil according to actual water contents, root distribution and root
// effectivity. The trailing two C++ parameters are unused in the C++ body
// too (their names are commented out there) - kept for signature fidelity
// with step()'s call site.
fc_crop_water_uptake :: proc(
	cm: ^Crop_Module,
	groundwater_table_layer_idx: int,
	gross_precipitation: f64,
	current_total_temperature_sum: f64,
	total_temperature_sum: f64,
) {
	soil_column := cm.soil_column
	max_effective_rooting_depth := cm.site_params.max_effective_rooting_depth

	nols := len(soil_column.layers)
	layer_thickness := soil_column.layers[0].layer_thickness_m
	cm.potential_transpiration_deficit = 0.0 // [mm]
	cm.potential_transpiration = 0.0 // old TRAMAX [mm]
	potential_evapotranspiration := 0.0 // [mm]
	cm.transpiration_reduced = 0.0 // old TDRED [mm]
	cm.actual_transpiration = 0.0 // [mm]
	remaining_total_root_effectivity := 0.0 // old WEFFREST [m]
	crop_water_uptake_from_groundwater := 0.0 // old GAUF [mm]
	total_root_effectivity := 0.0 // old WEFF [m]
	cm.actual_transpiration_deficit = 0.0 // old TREST [mm]
	interception := 0.0
	cm.remaining_evapotranspiration = 0.0

	for i_layer := 0; i_layer < nols; i_layer += 1 {
		cm.transpiration[i_layer] = 0.0 // old TP [mm]
		cm.transpiration_redux[i_layer] = 0.0 // old TRRED []
		cm.root_effectivity[i_layer] = 0.0 // old WUEFF [?]
	}

	// --- Interception ---
	interception_storage_old := cm.interception_storage

	// Interception in [mm d-1]
	interception = (2.5 * cm.crop_height * cm.soil_coverage) - cm.interception_storage
	if interception < 0 {
		interception = 0.0
	}
	// If no precipitation occurs, interception = 0
	if gross_precipitation <= 0 {
		interception = 0.0
	}

	// Calculating net precipitation and adding to surface water
	if gross_precipitation <= interception {
		interception = gross_precipitation
		cm.net_precipitation = 0.0
	} else {
		cm.net_precipitation = gross_precipitation - interception
	}

	// add intercepted precipitation to the virtual interception water storage
	cm.interception_storage = interception_storage_old + interception

	// --- Transpiration ---
	potential_evapotranspiration = cm.reference_evapotranspiration * cm.kc_factor // [mm]

	// from HERMES
	if potential_evapotranspiration > 6.5 {
		potential_evapotranspiration = 6.5
	}

	cm.remaining_evapotranspiration = potential_evapotranspiration // [mm]

	// If crop holds intercepted water, first evaporation from crop surface
	if cm.interception_storage > 0.0 {
		if cm.remaining_evapotranspiration >= cm.interception_storage {
			cm.remaining_evapotranspiration -= cm.interception_storage
			cm.evaporated_from_intercept = cm.interception_storage
			cm.interception_storage = 0.0
		} else {
			cm.interception_storage -= cm.remaining_evapotranspiration
			cm.evaporated_from_intercept = cm.remaining_evapotranspiration
			cm.remaining_evapotranspiration = 0.0
		}
	} else {
		cm.evaporated_from_intercept = 0.0
	}

	// if the plant has matured, no transpiration occurs!
	if cm.developmental_stage < cm.final_developmental_stage {
		cm.potential_transpiration = cm.remaining_evapotranspiration * cm.soil_coverage // [mm]

		for i_layer := 0; i_layer < cm.rooting_zone; i_layer += 1 {
			available_water :=
				soil_column.layers[i_layer].field_capacity -
				soil_column.layers[i_layer].permanent_wilting_point
			available_water_percentage :=
				(soil_column.layers[i_layer].soil_moisture_m3 -
					soil_column.layers[i_layer].permanent_wilting_point) /
				available_water
			if available_water_percentage < 0.0 {
				available_water_percentage = 0.0
			}
			// MP: access point for drought optimisation (this could be
			// extended for waterlogging); an alternative approach for
			// compensatory effects is a soil water-dependent root
			// penetration rate.
			switch {
			case available_water_percentage < 0.15:
				cm.transpiration_redux[i_layer] = available_water_percentage * 3.0 // []
				cm.root_effectivity[i_layer] = 0.15 + 0.45 * available_water_percentage / 0.15 // [] MP: essentially *3
			case available_water_percentage < 0.3:
				cm.transpiration_redux[i_layer] =
					0.45 + (0.25 * (available_water_percentage - 0.15) / 0.15)
				cm.root_effectivity[i_layer] =
					0.6 + (0.2 * (available_water_percentage - 0.15) / 0.15)
			case available_water_percentage < 0.5:
				// MP: ab hier hat das fast keinen Effekt mehr
				cm.transpiration_redux[i_layer] =
					0.7 + (0.275 * (available_water_percentage - 0.3) / 0.2)
				cm.root_effectivity[i_layer] =
					0.8 + (0.2 * (available_water_percentage - 0.3) / 0.2)
			case available_water_percentage < 0.75:
				// MP: ab hier ist nur mehr die Transpiration betroffen
				cm.transpiration_redux[i_layer] =
					0.975 + (0.025 * (available_water_percentage - 0.5) / 0.25)
				cm.root_effectivity[i_layer] = 1.0
			case:
				cm.transpiration_redux[i_layer] = 1.0
				cm.root_effectivity[i_layer] = 1.0
			}
			if cm.transpiration_redux[i_layer] < 0 {
				cm.transpiration_redux[i_layer] = 0.0
			}
			if cm.root_effectivity[i_layer] < 0 {
				cm.root_effectivity[i_layer] = 0.0
			}
			if i_layer == groundwater_table_layer_idx { 	// old GRW
				cm.root_effectivity[i_layer] = 0.5
			}
			if i_layer > groundwater_table_layer_idx { 	// old GRW
				cm.root_effectivity[i_layer] = 0.0
			}
			if f64(i_layer + 1) * layer_thickness >= max_effective_rooting_depth {
				cm.root_effectivity[i_layer] = 0.0
			}

			total_root_effectivity += cm.root_effectivity[i_layer] * cm.root_density[i_layer] // [m m-3]
			remaining_total_root_effectivity = total_root_effectivity
		}

		// [TRANSPLANT SHOCK] Water Uptake Limitation.
		if cm.transplant_efficiency < 1.0 {
			total_root_effectivity *= cm.transplant_efficiency
			remaining_total_root_effectivity = total_root_effectivity
		}

		for i_layer := 0; i_layer < nols; i_layer += 1 {
			if i_layer > min(cm.rooting_zone, groundwater_table_layer_idx + 1) {
				cm.transpiration[i_layer] = 0.0 // [mm]
			} else {
				if total_root_effectivity != 0.0 {
					cm.transpiration[i_layer] =
						cm.potential_transpiration *
						((cm.root_effectivity[i_layer] * cm.root_density[i_layer]) /
								total_root_effectivity) *
						cm.oxygen_deficit
				} else {
					// MP: why is this not changing anything? (probably only
					// matters for too-dry conditions)
					cm.transpiration[i_layer] = 0
				}
			}
		}

		for i_layer := 0;
		    i_layer < min(cm.rooting_zone, groundwater_table_layer_idx + 1);
		    i_layer += 1 {
			remaining_total_root_effectivity -=
				cm.root_effectivity[i_layer] * cm.root_density[i_layer] // [m m-3]

			if remaining_total_root_effectivity <= 0.0 {
				remaining_total_root_effectivity = 0.00001
			}
			if ((cm.transpiration[i_layer] / 1000.0) / layer_thickness) >
			   (soil_column.layers[i_layer].soil_moisture_m3 -
					   soil_column.layers[i_layer].permanent_wilting_point) {
				cm.potential_transpiration_deficit =
					(((cm.transpiration[i_layer] / 1000.0) / layer_thickness) -
						(soil_column.layers[i_layer].soil_moisture_m3 -
								soil_column.layers[i_layer].permanent_wilting_point)) *
					layer_thickness *
					1000.0 // [mm]
				if cm.potential_transpiration_deficit < 0.0 {
					cm.potential_transpiration_deficit = 0.0
				}
				if cm.potential_transpiration_deficit > cm.transpiration[i_layer] {
					cm.potential_transpiration_deficit = cm.transpiration[i_layer] // [mm]
				}
			} else {
				cm.potential_transpiration_deficit = 0.0
			}
			cm.transpiration_reduced =
				cm.transpiration[i_layer] * (1.0 - cm.transpiration_redux[i_layer])

			// MP: this is a key line for water stress response
			cm.actual_transpiration_deficit = max(
				cm.transpiration_reduced,
				cm.potential_transpiration_deficit,
			) // [mm]
			if cm.actual_transpiration_deficit > 0.0 {
				if i_layer < min(cm.rooting_zone, groundwater_table_layer_idx + 1) {
					for i_layer2 := i_layer + 1;
					    i_layer2 < min(cm.rooting_zone, groundwater_table_layer_idx + 1);
					    i_layer2 += 1 {
						cm.transpiration[i_layer2] +=
							cm.actual_transpiration_deficit *
							(cm.root_effectivity[i_layer2] *
									cm.root_density[i_layer2] /
									remaining_total_root_effectivity)
					}
				}
			}
			cm.transpiration[i_layer] = cm.transpiration[i_layer] - cm.actual_transpiration_deficit
			if cm.transpiration[i_layer] < 0.0 {
				cm.transpiration[i_layer] = 0.0
			}
			cm.actual_transpiration += cm.transpiration[i_layer]
			if i_layer == groundwater_table_layer_idx {
				crop_water_uptake_from_groundwater =
					(cm.transpiration[i_layer] / 1000.0) / layer_thickness // [m3 m-3]
			}
		}
		if cm.potential_transpiration > 0 {
			cm.transpiration_deficit = cm.actual_transpiration / cm.potential_transpiration
		} else {
			cm.transpiration_deficit = 1.0
		}

		groundwater_distance := groundwater_table_layer_idx - cm.rooting_depth
		if groundwater_distance <= 1 {
			cm.transpiration_deficit = 1.0
		}
		if !cm.sim_params.water_deficit_response_on {
			cm.transpiration_deficit = 1.0
		}
	}
	// vc_CropWaterUptakeFromGroundwater: computed but unused elsewhere in the
	// C++ too - a genuine dead store, kept for fidelity.
	_ = crop_water_uptake_from_groundwater
}

// C++: void monica::cropmodule::fcCropNUptake(CropModule*, size_t
// vc_GroundwaterTable, double, double)
//
// The trailing two C++ parameters are unused in the C++ body too (their
// names are commented out there) - kept for signature fidelity.
fc_crop_n_uptake :: proc(
	cm: ^Crop_Module,
	groundwater_table: int,
	current_total_temperature_sum: f64,
	total_temperature_sum: f64,
) {
	crop_ps := cm.mod_params
	soil_column := cm.soil_column
	tortuosity := crop_ps.tortuosity

	nols := len(soil_column.layers)
	layer_thickness := soil_column.layers[0].layer_thickness_m

	convective_n_uptake := 0.0 // old TRNSUM
	diffusive_n_uptake := 0.0 // old SUMDIFF
	convective_n_uptake_from_layer := make([dynamic]f64, nols, context.temp_allocator) // old MASS

	diffusion_coeff := make([dynamic]f64, nols, context.temp_allocator) // old D
	diffusive_n_uptake_from_layer := make([dynamic]f64, nols, context.temp_allocator) // old DIFF
	convective_n_uptake_1 := 0.0 // old MASSUM
	diffusive_n_uptake_1 := 0.0 // old DIFFSUM
	minimum_available_n := crop_ps.minimum_available_n // kg m-3
	minimum_n_concentration_root := crop_ps.minimum_n_concentration_root // kg kg-1
	max_crop_n_demand := crop_ps.max_crop_n_demand

	cm.total_n_uptake = 0.0
	cm.total_n_input = 0.0
	cm.fixed_n = 0.0
	for &v in cm.n_uptake_from_layer {
		v = 0.0
	}

	PI :: 3.14159265358979323

	// if the plant has matured, no N uptake occurs!
	if cm.developmental_stage < cm.final_developmental_stage {
		for i_layer := 0; i_layer < min(cm.rooting_zone, groundwater_table); i_layer += 1 {
			soil_mineral_n_content_i := soil_column.layers[i_layer].soil_no3 // [kg m-3]

			// Convective N uptake per layer
			// ([mm -> m]) * ([kg m-3] / old WG [m3 m-3]) -> [kg m-2]
			convective_n_uptake_from_layer[i_layer] =
				(cm.transpiration[i_layer] / 1000.0) *
				(soil_mineral_n_content_i / soil_column.layers[i_layer].soil_moisture_m3) *
				cm.time_step

			convective_n_uptake += convective_n_uptake_from_layer[i_layer] // [kg m-2]

			diffusion_coeff[i_layer] =
				0.000214 *
				(tortuosity * cr_math.exp(soil_column.layers[i_layer].soil_moisture_m3 * 10)) /
				soil_column.layers[i_layer].soil_moisture_m3 // [m2 d-1]

			// ([m2 d-1] * [m3 m-3] * [m] * ([kg m-3])=[m3 m-3])=[m m-3] -> [kg m-2]
			diffusive_n_uptake_from_layer[i_layer] =
				(diffusion_coeff[i_layer] *
					soil_column.layers[i_layer].soil_moisture_m3 *
					2.0 *
					PI *
					cm.root_diameter[i_layer] *
					(soil_mineral_n_content_i /
								1000.0 /
								soil_column.layers[i_layer].soil_moisture_m3 -
							0.000014) *
					libc.sqrt(PI * cm.root_density[i_layer])) *
				cm.root_density[i_layer] *
				1000.0 *
				cm.time_step

			if diffusive_n_uptake_from_layer[i_layer] < 0.0 {
				diffusive_n_uptake_from_layer[i_layer] = 0
			}

			diffusive_n_uptake += diffusive_n_uptake_from_layer[i_layer] // [kg m-2]
		}

		for i_layer := 0; i_layer < min(cm.rooting_zone, groundwater_table); i_layer += 1 {
			if cm.crop_n_demand > 0.0 {
				if convective_n_uptake >= cm.crop_n_demand {
					// convective N uptake is sufficient
					cm.n_uptake_from_layer[i_layer] =
						cm.crop_n_demand *
						convective_n_uptake_from_layer[i_layer] /
						convective_n_uptake
				} else {
					// N demand is not covered
					if (cm.crop_n_demand - convective_n_uptake) < diffusive_n_uptake {
						cm.n_uptake_from_layer[i_layer] =
							convective_n_uptake_from_layer[i_layer] +
							((cm.crop_n_demand - convective_n_uptake) *
									diffusive_n_uptake_from_layer[i_layer] /
									diffusive_n_uptake)
					} else {
						cm.n_uptake_from_layer[i_layer] =
							convective_n_uptake_from_layer[i_layer] +
							diffusive_n_uptake_from_layer[i_layer]
					}
				}

				convective_n_uptake_1 += convective_n_uptake_from_layer[i_layer]
				diffusive_n_uptake_1 += diffusive_n_uptake_from_layer[i_layer]

				soil_mineral_n_content_i := soil_column.layers[i_layer].soil_no3 // [kg m-3]
				if cm.n_uptake_from_layer[i_layer] >
				   ((soil_mineral_n_content_i * layer_thickness) - minimum_available_n) {
					cm.n_uptake_from_layer[i_layer] =
						(soil_mineral_n_content_i * layer_thickness) - minimum_available_n
				}

				if cm.n_uptake_from_layer[i_layer] > (max_crop_n_demand / 10000.0 * 0.75) {
					cm.n_uptake_from_layer[i_layer] = max_crop_n_demand / 10000.0 * 0.75
				}

				if cm.n_uptake_from_layer[i_layer] < 0.0 {
					cm.n_uptake_from_layer[i_layer] = 0.0
				}
			} else {
				cm.n_uptake_from_layer[i_layer] = 0.0
			}

			cm.total_n_uptake += cm.n_uptake_from_layer[i_layer] * 10000.0 // [kg m-2] -> [kg ha-1]
		}

		// Biological N Fixation - part of the deficit which can be covered
		// by biological N fixation.
		cm.fixed_n =
			cm.crop_params.species_params.part_biological_n_fixation *
			cm.crop_n_demand *
			10000.0 // [kg N ha-1]

		if ((cm.crop_n_demand * 10000.0) - cm.total_n_uptake) < cm.fixed_n {
			cm.total_n_input = cm.crop_n_demand * 10000.0
			cm.fixed_n = (cm.crop_n_demand * 10000.0) - cm.total_n_uptake
		} else {
			cm.total_n_input = cm.total_n_uptake + cm.fixed_n
		}
	}
	// vc_ConvectiveNUptake_1/vc_DiffusiveNUptake_1: computed but unused
	// elsewhere in the C++ too - genuine dead stores, kept for fidelity.
	_ = convective_n_uptake_1
	_ = diffusive_n_uptake_1

	cm.sum_total_n_uptake += cm.total_n_uptake
	cm.total_biomass_n_content += cm.total_n_input

	if cm.root_biomass > cm.root_biomass_old {
		// root has been growing
		cm.n_concentration_root =
			((cm.root_biomass_old * cm.n_concentration_root) +
				((cm.root_biomass - cm.root_biomass_old) /
						(cm.aboveground_biomass -
								cm.aboveground_biomass_old +
								cm.belowground_biomass -
								cm.belowground_biomass_old +
								cm.root_biomass -
								cm.root_biomass_old) *
						cm.total_n_input)) /
			cm.root_biomass

		cm.n_concentration_root = tl.bound(
			minimum_n_concentration_root,
			cm.n_concentration_root,
			cm.crop_params.species_params.stage_max_root_n_concentration[cm.developmental_stage],
		)
	}

	cm.n_concentration_aboveground_biomass =
		(cm.total_biomass_n_content - (cm.root_biomass * cm.n_concentration_root)) /
		(cm.aboveground_biomass +
				(cm.belowground_biomass / cm.crop_params.cultivar_params.residue_n_ratio))

	if (cm.n_concentration_aboveground_biomass * cm.aboveground_biomass) <
	   (cm.n_concentration_aboveground_biomass_old * cm.aboveground_biomass_old) {
		temp_n_concentration_aboveground_biomass :=
			cm.n_concentration_aboveground_biomass_old *
			cm.aboveground_biomass_old /
			cm.aboveground_biomass

		temp_n_concentration_root :=
			(cm.total_biomass_n_content -
				(cm.n_concentration_aboveground_biomass * cm.aboveground_biomass) -
				(cm.n_concentration_aboveground_biomass *
						cm.belowground_biomass /
						cm.crop_params.cultivar_params.residue_n_ratio)) /
			cm.root_biomass

		if temp_n_concentration_root >= minimum_n_concentration_root {
			cm.n_concentration_aboveground_biomass = temp_n_concentration_aboveground_biomass
			cm.n_concentration_root = temp_n_concentration_root
		}
	}
}

// C++: double monica::cropmodule::getEffectiveRootingDepth(const CropModule*)
get_effective_rooting_depth :: proc(cm: ^Crop_Module) -> f64 {
	nols := len(cm.soil_column.layers)

	for i_layer := 0; i_layer < nols; i_layer += 1 {
		if cm.root_effectivity[i_layer] == 0.0 {
			return f64(i_layer + 1) / 10.0
		}
	}

	return f64(nols + 1) / 10.0
}

// ---------------------------------------------------------------------------
// Phase 5 checkpoint 7: step() orchestration
//
// Ported: step() itself (crop_module_step - "step" collides with the other
// core/*.odin files' own step functions, so it gets the same owning-struct
// prefix soil_temperature_step/soil_moisture_step/soil_organic_step already
// established), the FAO-56 dual-Kc block (never a separate cropmodule::
// function in C++ - inline in step() there too), forceTransplantState,
// setPerennialCropParameters. This is where every function ported in
// checkpoints 3-6 is finally wired together into one real, callable daily
// entry point, and where fire_event stops being a no-op stub in the oracle.
//
// Deliberately not ported here: applyCutting. It takes a
// map<int, CuttingData::Value> - CuttingData is declared in
// src/worksteps/cutting.h, which this port hasn't reached yet (worksteps are
// phase 6, a separate later phase from this phase-5 checkpoint). Deferred to
// whichever phase-6 checkpoint ports the Cutting workstep itself.
// ---------------------------------------------------------------------------

// C++: void monica::cropmodule::setPerennialCropParameters(CropModule*, const CropParameters&)
//
// kj::heap<CropParameters>(cps) copy-constructs onto the heap - deep-copies
// via CropParameters' implicit copy constructor, same as makeCropModule's
// constructor and fcUpdateCropParametersForPerennial. Reuses
// clone_crop_parameters for the same reason those do.
set_perennial_crop_parameters :: proc(
	cm: ^Crop_Module,
	cps: p.Crop_Parameters,
	allocator := context.allocator,
) {
	ptr := new(p.Crop_Parameters, allocator)
	ptr^ = clone_crop_parameters(cps, allocator)
	cm.perennial_crop_params = ptr
}

// C++: void monica::cropmodule::forceTransplantState(CropModule*, double
// temperatureSum, double lai, size_t stage, double rootMass, double
// leafMass, double shootMass, int postTransplantDelay)
//
// --- BEGIN TRANSPLANT MODIFICATION --- (matching the C++ source's own
// begin/end markers around this function)
force_transplant_state :: proc(
	cm: ^Crop_Module,
	temperature_sum, lai: f64,
	stage: int,
	root_mass, leaf_mass, shoot_mass: f64,
	post_transplant_delay: int,
) {
	// Initialize transplant shock duration parameters
	cm.transplant_shock_duration = post_transplant_delay
	cm.days_since_transplant = 0

	// --- Step 1: Force developmental stage and LAI ---
	set_stage(cm, stage)
	cm.leaf_area_index = lai

	// --- Step 2: Force cumulative and stage-specific GDD temperature sums ---
	cm.current_total_temperature_sum = temperature_sum

	remaining_gdd := temperature_sum
	for i := 0; i < len(cm.current_temperature_sum); i += 1 {
		if i < stage {
			threshold := cm.crop_params.cultivar_params.stage_temperature_sum[i]
			cm.current_temperature_sum[i] = threshold
			remaining_gdd -= threshold
		} else if i == stage {
			cm.current_temperature_sum[i] = max(0.0, remaining_gdd)
		} else {
			cm.current_temperature_sum[i] = 0.0
		}
	}

	// --- Step 3: Initialize organ biomass pools ---
	if len(cm.organ_biomass) > 2 {
		cm.organ_biomass[0] = root_mass
		cm.organ_biomass[1] = leaf_mass
		cm.organ_biomass[2] = shoot_mass
		cm.organ_green_biomass[0] = root_mass
		cm.organ_green_biomass[1] = leaf_mass
		cm.organ_green_biomass[2] = shoot_mass
	}

	// --- Step 4: Update carbon balance and rooting depth state variables ---
	cm.root_biomass = root_mass
	cm.aboveground_biomass = leaf_mass + shoot_mass
	cm.total_biomass = root_mass + leaf_mass + shoot_mass

	// Settle rooting zone and layers based on standard species parameters
	nols := len(cm.soil_column.layers)
	layer_thickness := cm.soil_column.layers[0].layer_thickness_m
	cm.rooting_depth_m = cm.crop_params.species_params.initial_rooting_depth
	cm.rooting_depth = min(int(libc.round(cm.rooting_depth_m / layer_thickness)), nols)
	cm.rooting_zone = min(int(libc.round(1.3 * cm.rooting_depth_m / layer_thickness)), nols)

	// Force total root length based on physical constants
	cm.total_root_length =
		(cm.root_biomass * 100000.0 * 100.0 / 7.0) / (0.015 * 0.015 * 3.14159265358979323)

	// Force nitrogen pools to prevent severe immediate starvation stress in
	// new seedlings
	cm.n_concentration_aboveground_biomass =
		cm.crop_params.species_params.n_concentration_aboveground_biomass
	cm.n_concentration_root = cm.crop_params.species_params.n_concentration_root
	cm.total_biomass_n_content =
		(cm.aboveground_biomass * cm.n_concentration_aboveground_biomass) +
		(cm.root_biomass * cm.n_concentration_root)
}

// --- END TRANSPLANT MODIFICATION ---

// C++: the icSendRcv lambda inside step(). The intercropping RPC exchange
// body is unreachable (Intercropping is a dropped feature, see
// plan-odin.md's "Explicitly dropped" table; crop_mod_params.is_intercropping
// is false in every fixture in this repo) and is not ported, matching
// checkpoint 4's fcCropPhotosynthesis precedent. Kept as a real, callable
// no-op so step()'s call sites and control flow stay literal.
@(private)
crop_module_ic_send_rcv :: proc(cm: ^Crop_Module, outmsg: string) {
	if cm.mod_params.is_intercropping {
		// intercropping RPC exchange - dropped feature, not ported.
	}
}

// Fills Frost_Kill_In from the crop and its soil column (crop_module_step and
// the test drivers).
frost_kill_in :: proc(cm: ^Crop_Module, max_air_temperature, min_air_temperature: f64, snow: Snow_For_Crop) -> Frost_Kill_In {
	return {
		max_air_temperature = max_air_temperature,
		min_air_temperature = min_air_temperature,
		snow = snow,
		snow_depth_mm = cm.soil_column.snow_depth_mm,
		soil_surface_temperature = cm.soil_column.soil_surface_temperature,
		top_soil_temperature = cm.soil_column.layers[0].soil_temperature,
		developmental_stage = cm.developmental_stage,
		vernalisation_factor = cm.vernalisation_factor,
		current_temperature_sum = cm.current_temperature_sum[:],
	}
}

// C++: void monica::cropmodule::step(CropModule*, double meanAirTemperature,
// double maxAirTemperature, double minAirTemperature, double globalRadiation,
// double sunshineHours, Tools::Date currentDate, double relativeHumidity,
// double windSpeed, double windSpeedHeight, double
// atmosphericCO2Concentration, double atmosphericO3Concentration, double
// grossPrecipitation, double referenceEvapotranspiration)
//
// The daily orchestration entry point: calls every cropmodule:: function
// ported in checkpoints 3-6 in step()'s real order, plus the FAO-56 dual-Kc
// block below.
//
// Returns the day's events instead of the C++'s `fireEvent` callback
// (plan-component-interfaces.md A4); the model adds them to its current
// events (monica_model_add_crop_events). Nothing in the step reads them.
crop_module_step :: proc(
	cm: ^Crop_Module,
	mean_air_temperature, max_air_temperature, min_air_temperature: f64,
	global_radiation, sunshine_hours: f64,
	current_date: d.Date,
	relative_humidity, wind_speed, wind_speed_height: f64,
	atmospheric_co2_concentration, atmospheric_o3_concentration: f64,
	gross_precipitation, reference_evapotranspiration: f64,
	snow: Snow_For_Crop,
	// where the dead roots go during the step (the C++ addOrganicMatter
	// callback); nil: nowhere (test drivers without soil organic)
	soil_organic: ^Soil_Organic,
	allocator := context.allocator,
) -> (
	events: Crop_Events,
) {
	soil_column := cm.soil_column
	// Crop parameters are read through pointers, never cached by value: like
	// the C++'s `const auto&` references, they must see the perennial reset in
	// fc_crop_developmental_stage, which reassigns cm.crop_params mid-step (a
	// by-value copy of e.g. stage_temperature_sum would keep the old arrays).
	species_ps := &cm.crop_params.species_params
	cultivar_ps := &cm.crop_params.cultivar_params

	// The values the parts below read from yesterday (crop_carry_over.odin).
	prev := crop_carry_over_of(cm)

	julian_day := int(d.julian_day(current_date))

	if cm.cutting_delay_days > 0 {
		cm.cutting_delay_days -= 1
	}

	// [TRANSPLANT SHOCK] Daily stress recovery calculation. The daily
	// transplant efficiency factors in transplant shock, increasing linearly
	// from a baseline of 0.2 (80% initial stress) to 1.0 (no stress) over
	// the post-transplant delay duration.
	cm.transplant_efficiency = 1.0
	if cm.days_since_transplant >= 0 && cm.days_since_transplant < cm.transplant_shock_duration {
		cm.transplant_efficiency =
			0.2 + 0.8 * (f64(cm.days_since_transplant) / f64(cm.transplant_shock_duration))
		cm.days_since_transplant += 1
	} else if cm.days_since_transplant >= cm.transplant_shock_duration {
		cm.days_since_transplant = -1 // Recovery period has successfully concluded
	}

	cm.radiation = crop_radiation(
		Radiation_In {
			julian_day = f64(julian_day),
			latitude = cm.site_params.latitude,
			global_radiation = global_radiation,
			sunshine_hours = sunshine_hours,
		},
	)

	stress_oxygen_deficiency(
		&cm.stress,
		Oxygen_Deficiency_In {
			critical_oxygen_content = species_ps.critical_oxygen_content[prev.developmental_stage],
			developmental_stage = prev.developmental_stage,
			rooting_depth = prev.rooting_depth,
			time_under_anoxia_threshold = cm.mod_params.time_under_anoxia_threshold[:],
			time_step = cm.time_step,
			soil_layers = soil_column.layers[:],
		},
	)

	old_developmental_stage := prev.developmental_stage

	phenology_start_dormancy(&cm.phenology, &cm.crop_params, current_date)
	if !cultivar_ps.perennial || d.ge(current_date, cm.perennial_crop_dormancy_period_end_date) {
		reset_perennial, irregular := phenology_development(
			&cm.phenology,
			&cm.crop_params,
			phenology_in(
				cm,
				mean_air_temperature,
				current_date,
				prev.water_stress,
				prev.n_stress,
				prev.daylength_factor,
				prev.vernalisation_factor,
			),
		)
		if irregular {
			cm.error_status = true
			cm.error_message = "irregular developmental stage"
		}
		if reset_perennial {
			// the perennial reset replaces the crop parameters (parameters as
			// state, plan-component-interfaces.md F6), then resets phenology
			// with the new ones - the C++ order
			fc_update_crop_parameters_for_perennial(cm, allocator)
			phenology_reset_perennial(&cm.phenology, &cm.crop_params, cm.no_of_dev_stages, current_date)
		}
	}

	events = phenology_events(
		&cm.phenology,
		&cm.crop_params,
		Phenology_Events_In {
			old_developmental_stage = old_developmental_stage,
			julian_day = julian_day,
			no_of_dev_stages = cm.no_of_dev_stages,
		},
	)

	// NOTE(c++-quirk): the daylength and vernalisation factors are computed
	// AFTER the stage, so fc_crop_developmental_stage above used yesterday's
	// (prev.daylength_factor, prev.vernalisation_factor), and today's factors
	// are for today's - possibly new - stage. Probably not intended, but it is
	// the C++ order, kept for identical results; it is visible in
	// Crop_Carry_Over now instead of being a consequence of the call order.
	cm.daylength_factor = crop_daylength_factor(
		Daylength_Factor_In {
			daylength_requirement = cultivar_ps.daylength_requirement[cm.developmental_stage],
			base_daylength = cultivar_ps.base_daylength[cm.developmental_stage],
			effective_day_length = cm.effective_day_length,
			photoperiodic_daylength = cm.photoperiodic_daylength,
		},
	)

	cm.vernalisation_factor, cm.vernalisation_days = crop_vernalisation_factor(
		Vernalisation_In {
			mean_air_temperature = mean_air_temperature,
			vernalisation_requirement = cultivar_ps.vernalisation_requirement[cm.developmental_stage],
			vernalisation_days = cm.vernalisation_days,
			time_step = cm.time_step,
			enable_fix = vernalisation_factor_fix_enabled(cm),
		},
	)

	if cm.total_temperature_sum == 0.0 {
		cm.relative_total_development = 0.0
	} else {
		cm.relative_total_development = cm.current_total_temperature_sum / cm.total_temperature_sum
	}

	canopy_kc_in := Canopy_Kc_In {
		developmental_stage     = cm.developmental_stage,
		current_temperature_sum = cm.current_temperature_sum[:],
		bare_soil_kc_factor     = cm.site_params.bare_soil_kc_factor,
	}
	canopy_kc_factor(&cm.canopy, &cm.crop_params, canopy_kc_in)
	canopy_kcb_factor(&cm.canopy, &cm.crop_params, canopy_kc_in)

	if cm.developmental_stage > 0 {
		max_crop_height :=
			cm.mod_params.is_intercropping && cm.intercropping_other_crop_height > prev.crop_height ? cultivar_ps.max_crop_height * cm.mod_params.intercropping_ph_redux : cultivar_ps.max_crop_height

		// the intercropping exchange (a no-op stub here) sits between crop size and
		// green area in the C++
		crop_module_ic_send_rcv(cm, "devstage > 0: ")

		canopy_growth(
			&cm.canopy,
			&cm.crop_params,
			Canopy_Growth_In {
				developmental_stage = cm.developmental_stage,
				mean_air_temperature = mean_air_temperature,
				max_crop_height = max_crop_height,
				current_total_temperature_sum = cm.current_total_temperature_sum,
				current_temperature_sum = cm.current_temperature_sum[:],
				leaf_growth_increment = prev.leaf_growth_increment,
				leaf_senescence_increment = prev.leaf_senescence_increment,
				enable_t_response_leaf_expansion = cm.mod_params.__enable_T_response_leaf_expansion__,
				time_step = cm.time_step,
			},
		)

		fc_crop_photosynthesis(
			cm,
			mean_air_temperature,
			max_air_temperature,
			min_air_temperature,
			atmospheric_co2_concentration,
			atmospheric_o3_concentration,
			current_date,
			prev.water_stress,
			prev.n_stress,
			prev.frost_redux,
			prev.rooting_depth,
			prev.reference_evapotranspiration,
			allocator,
		)

		stress_heat(
			&cm.stress,
			&cm.crop_params,
			Heat_Stress_In {
				max_air_temperature = max_air_temperature,
				min_air_temperature = min_air_temperature,
				current_total_temperature_sum = cm.current_total_temperature_sum,
			},
		)

		if cm.sim_params.frost_kill_on {
			stress_frost_kill(&cm.stress, &cm.crop_params, frost_kill_in(cm, max_air_temperature, min_air_temperature, snow))
		}

		// The C++ clamps the water uptake's transpiration deficit (yesterday's)
		// to >= 0 inside fcDroughtImpactOnFertility. The write stays, but here:
		// at the final developmental stage water uptake doesn't overwrite it,
		// so the clamped value is what the next day and the model's stress sum
		// see.
		water_stress := prev.water_stress < 0.0 ? 0.0 : prev.water_stress
		cm.transpiration_deficit = water_stress

		stress_drought_impact_on_fertility(
			&cm.stress,
			&cm.crop_params,
			Drought_Impact_In {
				water_stress = water_stress,
				developmental_stage = cm.developmental_stage,
				storage_organ = cm.storage_organ,
			},
		)

		n_status_step(
			&cm.n_status,
			&cm.crop_params,
			N_Status_In {
				aboveground_biomass = prev.aboveground_biomass,
				belowground_biomass = prev.belowground_biomass,
				n_concentration_aboveground_biomass = prev.n_concentration_aboveground_biomass,
				n_concentration_root = prev.n_concentration_root,
				nitrogen_response_on = cm.sim_params.nitrogen_response_on,
			},
		)

		// growth, root system, the N demand limited by root length: the three
		// parts of the C++ fcCropDryMatter
		dead_root_biomass := fc_crop_dry_matter(cm, prev.n_concentration_root)

		dead_roots := root_system_step(
			&cm.roots,
			&cm.crop_params,
			Root_System_In {
				root_biomass = cm.organ_biomass[Organ_Root],
				dead_root_biomass = dead_root_biomass,
				n_concentration_root = prev.n_concentration_root,
				water_stress = water_stress,
				developmental_stage = cm.developmental_stage,
				no_of_dev_stages = cm.no_of_dev_stages,
				mean_air_temperature = mean_air_temperature,
				soil_layers = soil_column.layers[:],
				number_of_organic_layers = soil_column.number_of_organic_layers,
				impenetrable_layer_depth = cm.site_params.impenetrable_layer_depth,
				max_effective_rooting_depth = cm.site_params.max_effective_rooting_depth,
				enable_pasw_root_penetration = cm.mod_params.__enable_PASW_root_penetration__,
				disable_daily_root_biomass_to_soil = cm.mod_params.__disable_daily_root_biomass_to_soil__,
			},
			allocator,
		)
		// the dead roots go to soil organic now, before N uptake reads the soil
		// N (the residues' NH4 and NO3 are added to the layers)
		if len(dead_roots) > 0 && soil_organic != nil {
			soil_organic_add_organic_matter(
				soil_organic,
				&cm.residue_params.base,
				dead_roots,
				prev.n_concentration_root,
			)
		}

		fc_crop_n_demand_by_root_length(cm)

		// calculate reference evapotranspiration if not provided directly
		// via climate files
		if reference_evapotranspiration < 0 {
			et0, terms := crop_et0(
				crop_et0_in(
					cm,
					max_air_temperature,
					min_air_temperature,
					relative_humidity,
					mean_air_temperature,
					wind_speed,
					wind_speed_height,
					atmospheric_co2_concentration,
				),
			)
			cm.reference_evapotranspiration = et0
			cm.stomata_resistance = terms.stomata_resistance
		} else {
			// use reference evapotranspiration from climate file
			cm.reference_evapotranspiration = reference_evapotranspiration
		}
		fc_crop_water_uptake(
			cm,
			soil_column.groundwater_table_layer_idx,
			gross_precipitation,
			cm.current_total_temperature_sum,
			cm.total_temperature_sum,
		)

		fc_crop_n_uptake(
			cm,
			soil_column.groundwater_table_layer_idx,
			cm.current_total_temperature_sum,
			cm.total_temperature_sum,
		)

		cm.gross_primary_production = fc_gross_primary_production(cm)

		cm.net_primary_production = fc_net_primary_production(cm, cm.total_respired)
	} else {
		crop_module_ic_send_rcv(cm, "devstage 0: ")
	}

	cm.no_of_crop_steps += 1
	return
}

// ---------------------------------------------------------------------------
// Phase 6 checkpoint 2 prerequisite: the yield/N-content getters and
// applyCutting, deferred from phase 5 (checkpoint 5's writeup: "whichever
// checkpoint actually needs them") - the Harvest workstep (harvestCurrentCrop,
// monica_model.odin) and the Cutting workstep both need these.
//
// getRawProteinConcentration is NOT ported: grepped and confirmed it has no
// callers anywhere in crop-module.cpp, harvestCurrentCrop, or any workstep in
// this port's scope - pure build-output.cpp API surface (phase 7), same
// "port on demand" call phase 5 checkpoint 5 already made for its siblings.
// ---------------------------------------------------------------------------

// C++: std::set<int> monica::cropmodule::organIdsForPrimaryYield(const CropModule*)
//
// std::set<int> -> map[int]bool, the same set idiom used elsewhere in this
// port (e.g. MonicaModel.currentEvents, phase 6 checkpoint 1).
organ_ids_for_primary_yield :: proc(
	cm: ^Crop_Module,
	allocator := context.allocator,
) -> map[int]bool {
	ids := make(map[int]bool, 0, allocator)
	for yc in cm.crop_params.cultivar_params.organ_ids_for_primary_yield {
		ids[yc.organ_id] = true
	}
	return ids
}

// C++: double calculateCropYield(const VYC&, const vector<double>&) - anonymous namespace
@(private)
calculate_crop_yield :: proc(ycs: [dynamic]p.Yield_Component, bmv: [dynamic]f64) -> f64 {
	yield := 0.0
	for yc in ycs {
		yield += bmv[yc.organ_id - 1] * yc.yield_percentage
	}
	return yield
}

// C++: double monica::cropmodule::getPrimaryCropYield(const CropModule*)
get_primary_crop_yield :: proc(cm: ^Crop_Module) -> f64 {
	return calculate_crop_yield(
		cm.crop_params.cultivar_params.organ_ids_for_primary_yield,
		cm.organ_biomass,
	)
}

// C++: double monica::cropmodule::getSecondaryCropYield(const CropModule*)
get_secondary_crop_yield :: proc(cm: ^Crop_Module) -> f64 {
	return calculate_crop_yield(
		cm.crop_params.cultivar_params.organ_ids_for_secondary_yield,
		cm.organ_biomass,
	)
}

// C++: double monica::cropmodule::getResidueBiomass(const CropModule*, bool, double)
get_residue_biomass :: proc(
	cm: ^Crop_Module,
	use_secondary_crop_yields: bool = true,
	alternative_crop_yield: f64 = -1,
) -> f64 {
	crop_yield :=
		alternative_crop_yield >= 0 ? alternative_crop_yield : get_primary_crop_yield(cm) + (use_secondary_crop_yields ? get_secondary_crop_yield(cm) : 0)

	return cm.total_biomass - cm.organ_biomass[0] - crop_yield
}

// C++: double monica::cropmodule::getResiduesNConcentration(const CropModule*, double)
get_residues_n_concentration :: proc(
	cm: ^Crop_Module,
	alternative_primary_crop_yield: f64 = -1,
) -> f64 {
	primary_crop_yield :=
		alternative_primary_crop_yield >= 0 ? alternative_primary_crop_yield : get_primary_crop_yield(cm)
	root_biomass := cm.organ_biomass[0]

	return(
		(cm.total_biomass_n_content - (root_biomass * cm.n_concentration_root)) /
		((primary_crop_yield / cm.crop_params.cultivar_params.residue_n_ratio) +
				(cm.total_biomass - root_biomass - primary_crop_yield)) \
	)
}

// C++: double monica::cropmodule::getPrimaryYieldNConcentration(const CropModule*, double)
get_primary_yield_n_concentration :: proc(
	cm: ^Crop_Module,
	alternative_primary_crop_yield: f64 = -1,
) -> f64 {
	primary_crop_yield :=
		alternative_primary_crop_yield >= 0 ? alternative_primary_crop_yield : get_primary_crop_yield(cm)
	root_biomass := cm.organ_biomass[0]

	return(
		(cm.total_biomass_n_content - (root_biomass * cm.n_concentration_root)) /
		(primary_crop_yield +
				(cm.crop_params.cultivar_params.residue_n_ratio *
						(cm.total_biomass - root_biomass - primary_crop_yield))) \
	)
}

// C++: double monica::cropmodule::getResiduesNContent(const CropModule*, bool, double, double)
get_residues_n_content :: proc(
	cm: ^Crop_Module,
	use_secondary_crop_yields: bool = true,
	alternative_primary_crop_yield: f64 = -1,
	alternative_crop_yield: f64 = -1,
) -> f64 {
	return(
		get_residue_biomass(cm, use_secondary_crop_yields, alternative_crop_yield) *
		get_residues_n_concentration(cm, alternative_primary_crop_yield) \
	)
}

// C++: double monica::cropmodule::getPrimaryYieldNContent(const CropModule*, double)
get_primary_yield_n_content :: proc(
	cm: ^Crop_Module,
	alternative_primary_crop_yield: f64 = -1,
) -> f64 {
	primary_crop_yield :=
		alternative_primary_crop_yield >= 0 ? alternative_primary_crop_yield : get_primary_crop_yield(cm)
	return primary_crop_yield * get_primary_yield_n_concentration(cm, alternative_primary_crop_yield)
}

// C++: double monica::cropmodule::getSecondaryYieldNContent(const CropModule*, double, double)
get_secondary_yield_n_content :: proc(
	cm: ^Crop_Module,
	alternative_primary_crop_yield: f64 = -1,
	alternative_secondary_crop_yield: f64 = -1,
) -> f64 {
	secondary_crop_yield :=
		alternative_secondary_crop_yield >= 0 ? alternative_secondary_crop_yield : get_secondary_crop_yield(cm)
	return secondary_crop_yield * get_residues_n_concentration(cm, alternative_primary_crop_yield)
}

// C++: double monica::cropmodule::getAbovegroundBiomassNContent(const CropModule*)
get_aboveground_biomass_n_content :: proc(cm: ^Crop_Module) -> f64 {
	return cm.aboveground_biomass * cm.n_concentration_aboveground_biomass
}

// C++: double monica::cropmodule::getFruitBiomassNContent(const CropModule*)
get_fruit_biomass_n_content :: proc(cm: ^Crop_Module) -> f64 {
	root_biomass := cm.organ_biomass[0]
	fruit_biomass := cm.organ_biomass[3]
	fruit_n_concentration :=
		(cm.total_biomass_n_content - (root_biomass * cm.n_concentration_root)) /
		(fruit_biomass +
				(cm.crop_params.cultivar_params.residue_n_ratio *
						(cm.total_biomass - root_biomass - fruit_biomass)))
	return fruit_biomass * fruit_n_concentration
}

// C++: double monica::cropmodule::getRawProteinConcentration(const CropModule*)
get_raw_protein_concentration :: proc(cm: ^Crop_Module) -> f64 {
	// Assuming an average N concentration of raw protein of 16%
	return get_primary_yield_n_concentration(cm) * 6.25
}

// C++: double monica::cropmodule::getAutotrophicRespiration(const CropModule*)
get_autotrophic_respiration :: proc(cm: ^Crop_Module) -> f64 {
	return cm.total_respired / 30.0 * 12.0 // Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
}

// C++: double monica::cropmodule::getOrganSpecificTotalRespired(const CropModule*, int)
get_organ_specific_total_respired :: proc(cm: ^Crop_Module, organ: int) -> f64 {
	// get total amount of actual biomass
	total_biomass := cm.total_biomass
	// get biomass of specific organ and calculates ratio
	organ_percentage := cm.organ_biomass[organ] / total_biomass
	return get_autotrophic_respiration(cm) * organ_percentage
}

// C++: double monica::cropmodule::getOrganSpecificNPP(const CropModule*, int)
get_organ_specific_npp :: proc(cm: ^Crop_Module, organ: int) -> f64 {
	// get total amount of actual biomass
	total_biomass := cm.total_biomass
	// get biomass of specific organ and calculates ratio
	organ_percentage := cm.organ_biomass[organ] / total_biomass
	return cm.net_primary_production * organ_percentage
}

// ---------------------------------------------------------------------------
// applyCutting - deferred from phase 5 checkpoint 7 to whichever checkpoint
// ports the Cutting workstep (this one). Its payload types (CuttingData::
// Value's Unit/CL enums) are hoisted into `core` here, the same
// circular-dependency-breaking move phase 6 checkpoint 1 used for
// HarvestData::Spec - the Cutting workstep itself lives in the `run` package,
// which imports `core`, so `core` cannot import it back.
// ---------------------------------------------------------------------------

// C++: enum CuttingData::Unit { percentage, biomass, LAI }
Cutting_Unit :: enum {
	Percentage,
	Biomass,
	LAI,
}

// C++: enum CuttingData::CL { cut, left, none }
Cutting_Cl :: enum {
	Cut,
	Left,
	None,
}

// C++: struct CuttingData::Value
Cutting_Value :: struct {
	value:       f64,
	unit:        Cutting_Unit, // C++ in-class default: percentage
	cut_or_left: Cutting_Cl, // C++ in-class default: cut
}

// C++: void monica::cropmodule::applyCutting(CropModule*,
//        std::map<int, CuttingData::Value>&, std::map<int, double>&, double)
//
// `organs`/`exports` are genuine in-out maps in the C++ (organs gets
// populated from pc_OrganIdsForCutting when passed in empty) - Odin's
// map[K]V parameters already alias the caller's backing store like a C++
// reference, so no extra indirection is needed to reproduce that.
//
// Iterates `organs` in ascending key order to match C++ std::map's sorted
// iteration - Odin map iteration order is unspecified and floating-point
// addition (aboveground_biomass -=, sumCutBiomass +=, sumResidueBiomass +=)
// is not associative, the same "sort keys first" fix phase 5 checkpoint 6's
// oracle regression needed.
apply_cutting :: proc(
	cm: ^Crop_Module,
	organs: map[int]Cutting_Value,
	exports: map[int]f64,
	cut_max_assimilation_fraction: f64,
	allocator := context.allocator,
) -> (
	// for soil organic (the caller adds them; the C++ called addOrganicMatter
	// here) - nothing if nothing was left on the field
	residues: map[int]f64,
	residue_n_concentration: f64,
) {
	old_aboveground_biomass := cm.aboveground_biomass
	old_agb_n_content := cm.aboveground_biomass * cm.n_concentration_aboveground_biomass
	sum_cut_biomass := 0.0
	current_sla := cm.leaf_area_index / cm.organ_green_biomass[Organ_Leaf]

	organs := organs
	if len(organs) == 0 {
		for yc in cm.crop_params.cultivar_params.organ_ids_for_cutting {
			organs[yc.organ_id - 1] = Cutting_Value {
				value = yc.yield_percentage,
			}
		}
	}

	keys := make([dynamic]int, 0, len(organs), allocator)
	for k in organs {
		append(&keys, k)
	}
	slice.sort(keys[:])

	sum_residue_biomass := 0.0
	for organ_id in keys {
		organ_spec := organs[organ_id]

		old_organ_biomass := cm.organ_biomass[organ_id]
		old_organ_dead_biomass := cm.organ_dead_biomass[organ_id]
		old_organ_green_biomass := old_organ_biomass - old_organ_dead_biomass
		new_organ_biomass := 0.0
		cut_organ_biomass := 0.0

		if organ_spec.unit == .Biomass {
			if organ_spec.cut_or_left == .Cut {
				cut_organ_biomass = min(organ_spec.value, old_organ_biomass)
				new_organ_biomass = old_organ_biomass - cut_organ_biomass
			} else if organ_spec.cut_or_left == .Left {
				new_organ_biomass = min(organ_spec.value, old_organ_biomass)
				cut_organ_biomass = old_organ_biomass - new_organ_biomass
			}

			if old_organ_biomass == 0 {
				cm.organ_dead_biomass[organ_id] = 0
			} else {
				cm.organ_dead_biomass[organ_id] =
					new_organ_biomass * min(old_organ_dead_biomass / old_organ_biomass, 1.0)
			}
		} else if organ_spec.unit == .Percentage {
			if organ_spec.cut_or_left == .Cut {
				cut_organ_biomass = organ_spec.value * old_organ_biomass
				new_organ_biomass = old_organ_biomass - cut_organ_biomass
			} else if organ_spec.cut_or_left == .Left {
				new_organ_biomass = organ_spec.value * old_organ_biomass
				cut_organ_biomass = old_organ_biomass - new_organ_biomass
			}

			if old_organ_biomass == 0 {
				cm.organ_dead_biomass[organ_id] = 0
			} else {
				cm.organ_dead_biomass[organ_id] =
					new_organ_biomass * min(old_organ_dead_biomass / old_organ_biomass, 1.0)
			}
		} else if organ_spec.unit == .LAI {
			// only "left" is supported for LAI
			if organ_spec.value > cm.leaf_area_index {
				new_organ_biomass = old_organ_green_biomass
				cut_organ_biomass = old_organ_dead_biomass
				cm.organ_dead_biomass[organ_id] = 0 // all the dead biomass is assumed to be cut
			} else {
				new_organ_biomass = min(organ_spec.value / current_sla, old_organ_green_biomass)
				cut_organ_biomass = old_organ_biomass - new_organ_biomass
				cm.organ_dead_biomass[organ_id] = 0 // all the dead biomass is assumed to be cut
			}
		}

		export_biomass := cut_organ_biomass * exports[organ_id]

		cm.aboveground_biomass -= cut_organ_biomass
		sum_cut_biomass += cut_organ_biomass
		sum_residue_biomass += cut_organ_biomass - export_biomass
		cm.organ_biomass[organ_id] = new_organ_biomass
		cm.organ_green_biomass[organ_id] =
			cm.organ_biomass[organ_id] - cm.organ_dead_biomass[organ_id]
	}

	cm.exported_cut_biomass = sum_cut_biomass - sum_residue_biomass
	cm.sum_exported_cut_biomass += cm.exported_cut_biomass
	cm.residue_cut_biomass = sum_residue_biomass
	cm.sum_residue_cut_biomass += cm.residue_cut_biomass

	if sum_residue_biomass > 0 {
		// prepare to add crop residues to soilorganic (AOMs)
		residues = make(map[int]f64, 1, allocator)
		residues[0] = sum_residue_biomass
		// residues get the aboveground biomass N concentration
		residue_n_concentration = cm.n_concentration_aboveground_biomass
	}

	// update LAI
	if cm.organ_green_biomass[Organ_Leaf] > 0 {
		cm.leaf_area_index = cm.organ_green_biomass[Organ_Leaf] * current_sla
	}

	// reset stage and temperature sum after cutting
	set_stage(cm, cm.crop_params.species_params.stage_after_cut)

	cm.cutting_delay_days = cm.crop_params.species_params.cutting_delay_days
	cm.crop_params.cultivar_params.max_assimilation_rate *= cut_max_assimilation_fraction

	if old_aboveground_biomass > 0.0 {
		cm.total_biomass_n_content -=
			(1 - cm.aboveground_biomass / old_aboveground_biomass) * old_agb_n_content
	}
	return
}
