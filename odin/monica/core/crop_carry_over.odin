package core

// The crop's previous-day values (architecture.md section 8,
// plan-component-interfaces.md phase B).
//
// Within crop_module_step, some parts read a value that another part only
// computes later in the step - so they get yesterday's value. In place, that
// is only visible from the order of the calls. Crop_Carry_Over makes these
// links explicit: crop_module_step takes it at its start, and the parts get
// the values they read from it as parameters.
//
// Taken at the START of the step, not at the end of the previous one:
// between two steps, management changes the crop (cutting, set_stage,
// transplanting) and today's parts see those changes. At the start of the
// step the values are exactly what the parts read before, because they read
// before the writer runs.
//
// Not in here:
// - a part's own state it updates itself (temperature sums, counters,
//   organ biomass of growth, ...);
// - arrays: photosynthesis reads growth's whole organ_green_biomass of the
//   day before (maintenance respiration), which stays a read of growth's
//   state; only single elements are copied here (a struct copy would share a
//   dynamic array's memory, a deep copy allocate).
Crop_Carry_Over :: struct {
	// phenology, read by oxygen deficiency (and its parameter index)
	developmental_stage:                 int,
	// water uptake's transpiration_deficit, read by development and photosynthesis
	water_stress:                        f64 `unit:"0-1"`,
	// crop N status's crop_n_redux, read by development and photosynthesis
	n_stress:                            f64 `unit:"0-1"`,
	// frost kill's crop_frost_redux, read by photosynthesis
	frost_redux:                         f64 `unit:"0-1"`,
	// read by development; computed later in the step, after the stage
	daylength_factor:                    f64 `unit:"0-1"`,
	vernalisation_factor:                f64 `unit:"0-1"`,
	// the root system's (dry matter), read by oxygen deficiency and photosynthesis
	rooting_depth:                       int `unit:"layer#"`,
	// growth's leaf increments, read by the green area
	leaf_growth_increment:               f64 `unit:"kgDM ha-1"`,
	leaf_senescence_increment:           f64,
	// the crop's ET0, read by photosynthesis (O3 uptake)
	reference_evapotranspiration:        f64 `unit:"mm"`,
	// the crop size's, read for the intercropping height reduction
	crop_height:                         f64 `unit:"m"`,
	// growth's and N uptake's, read by crop N status (and root N by growth)
	aboveground_biomass:                 f64 `unit:"kgDM ha-1"`,
	belowground_biomass:                 f64,
	n_concentration_aboveground_biomass: f64 `unit:"kgN kgDM-1"`,
	n_concentration_root:                f64 `unit:"kgN kgDM-1"`,
}

crop_carry_over_of :: proc(cm: ^Crop_Module) -> Crop_Carry_Over {
	leaf_growth, leaf_senescence: f64
	if len(cm.organ_growth_increment) > Organ_Leaf {
		leaf_growth = cm.organ_growth_increment[Organ_Leaf]
	}
	if len(cm.organ_senescence_increment) > Organ_Leaf {
		leaf_senescence = cm.organ_senescence_increment[Organ_Leaf]
	}
	return {
		developmental_stage = cm.developmental_stage,
		water_stress = cm.transpiration_deficit,
		n_stress = cm.crop_n_redux,
		frost_redux = cm.crop_frost_redux,
		daylength_factor = cm.daylength_factor,
		vernalisation_factor = cm.vernalisation_factor,
		rooting_depth = cm.rooting_depth,
		leaf_growth_increment = leaf_growth,
		leaf_senescence_increment = leaf_senescence,
		reference_evapotranspiration = cm.reference_evapotranspiration,
		crop_height = cm.crop_height,
		aboveground_biomass = cm.aboveground_biomass,
		belowground_biomass = cm.belowground_biomass,
		n_concentration_aboveground_biomass = cm.n_concentration_aboveground_biomass,
		n_concentration_root = cm.n_concentration_root,
	}
}
