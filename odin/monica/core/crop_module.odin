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

		photosynthesis_step(
			&cm.photosynthesis,
			&cm.o3,
			&cm.voc,
			&cm.crop_params,
			cm.mod_params,
			photosynthesis_in(
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
			),
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
		dead_root_biomass, total_biomass_n_content, growth_cycle_ended := growth_step(
			&cm.growth,
			&cm.crop_params,
			growth_in(cm, prev.n_concentration_root),
		)
		cm.total_biomass_n_content = total_biomass_n_content
		if growth_cycle_ended {
			cm.growth_cycle_ended = true
		}

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

		growth_limit_n_demand(
			&cm.growth,
			&cm.crop_params,
			Growth_N_Limit_In {
				total_root_length = cm.total_root_length,
				current_total_temperature_sum = cm.current_total_temperature_sum,
				total_temperature_sum = cm.total_temperature_sum,
				time_step = cm.time_step,
			},
		)

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
		water_uptake_step(&cm.water_uptake, water_uptake_in(cm, gross_precipitation))

		n_uptake_step(&cm.n_uptake, &cm.crop_params, n_uptake_in(cm))

		carbon_step(&cm.carbon, Carbon_In{gross_assimilates = cm.gross_assimilates, total_respired = cm.total_respired})
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
