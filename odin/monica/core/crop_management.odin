package core

import p "../params"

// Management of a growing crop (plan-component-interfaces.md F6): what a
// Transplant or Cutting workstep does to it, as commands each owning
// component applies to its own state - instead of one procedure writing
// phenology, canopy, growth, root and N state directly. State:
// Crop_Management (embedded in Crop_Module): transplant shock, cut biomass.

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

// Transplanting: the transplant shock starts.
management_transplant :: proc(m: ^Crop_Management, post_transplant_delay: int) {
	// Initialize transplant shock duration parameters
	m.transplant_shock_duration = post_transplant_delay
	m.days_since_transplant = 0
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
	management_transplant(&cm.management, post_transplant_delay)
	// --- Step 1: Force developmental stage and LAI ---
	set_stage(cm, stage)
	canopy_transplant(&cm.canopy, lai)
	// --- Step 2: Force cumulative and stage-specific GDD temperature sums ---
	phenology_transplant(&cm.phenology, &cm.crop_params, stage, temperature_sum)
	// --- Step 3: Initialize organ biomass pools ---
	growth_transplant(&cm.growth, root_mass, leaf_mass, shoot_mass)
	// --- Step 4: Update carbon balance and rooting depth state variables ---
	root_system_transplant(&cm.roots, &cm.crop_params, root_mass, cm.soil_column.layers[:])
	// nitrogen pools, from the new biomass
	n_uptake_transplant(&cm.n_uptake, &cm.crop_params, cm.aboveground_biomass, cm.root_biomass)
}

// --- END TRANSPLANT MODIFICATION ---

// Cutting: the cut biomass, exported and left on the field, and the delay
// before the crop develops again.
management_cut :: proc(m: ^Crop_Management, ps: ^p.Crop_Parameters, sum_cut_biomass, sum_residue_biomass: f64) {
	m.exported_cut_biomass = sum_cut_biomass - sum_residue_biomass
	m.sum_exported_cut_biomass += m.exported_cut_biomass
	m.residue_cut_biomass = sum_residue_biomass
	m.sum_residue_cut_biomass += m.residue_cut_biomass
	m.cutting_delay_days = ps.species_params.cutting_delay_days
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
	current_sla := cm.leaf_area_index / cm.organ_green_biomass[Organ_Leaf]

	sum_cut_biomass, sum_residue_biomass := growth_cut(
		&cm.growth,
		&cm.crop_params,
		organs,
		exports,
		cm.leaf_area_index,
		current_sla,
		allocator,
	)
	management_cut(&cm.management, &cm.crop_params, sum_cut_biomass, sum_residue_biomass)

	if sum_residue_biomass > 0 {
		// prepare to add crop residues to soilorganic (AOMs)
		residues = make(map[int]f64, 1, allocator)
		residues[0] = sum_residue_biomass
		// residues get the aboveground biomass N concentration
		residue_n_concentration = cm.n_concentration_aboveground_biomass
	}

	canopy_after_cut(&cm.canopy, cm.organ_green_biomass[Organ_Leaf], current_sla)

	// reset stage and temperature sum after cutting
	set_stage(cm, cm.crop_params.species_params.stage_after_cut)

	cm.crop_params.cultivar_params.max_assimilation_rate *= cut_max_assimilation_fraction

	n_uptake_after_cut(&cm.n_uptake, cm.aboveground_biomass, old_aboveground_biomass, old_agb_n_content)
	return
}
