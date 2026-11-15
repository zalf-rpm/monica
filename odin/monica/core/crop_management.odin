package core

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
