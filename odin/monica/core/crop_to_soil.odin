package core

// The crop -> soil interface (architecture.md section 9, plan-component-
// interfaces.md phase A): what the soil components read from the crop,
// declared in one place instead of each holding a ^Crop_Module.

// What the crop gives the soil components on a day. Owned by Monica_Model,
// filled by crop_to_soil_fill at the start of monica_model_general_step -
// after the crop step and the worksteps before it, right where the soil
// components used to read the crop - so the values are exactly those they
// read before.
//
// Bare soil (no crop module): all zero, which is what every reader used as
// its default.
//
// A crop that is being cleared (harvested or incorporated today, gone
// tomorrow; Monica_Model.clear_crop_upon_next_day) still has_crop: its step
// didn't run today, and the values are those it was left with. As in the
// C++, soil moisture treats such a crop as absent (`being_cleared`), while
// soil transport (N uptake), soil organic (NPP) and soil moisture's rooting
// depth read the values anyway.
Crop_To_Soil :: struct {
	has_crop:                     bool,
	being_cleared:                bool,
	developmental_stage:          int,
	// canopy - soil moisture, soil temperature
	soil_coverage:                f64,
	kc_factor:                    f64 `unit:"1"`,
	kcb_factor:                   f64 `unit:"1"`,
	crop_height:                  f64 `unit:"m"`,
	leaf_area_index:              f64 `unit:"m2 m-2"`,
	aboveground_biomass:          f64 `unit:"kgDM ha-1"`,
	// water - soil moisture
	reference_evapotranspiration: f64 `unit:"mm"`,
	remaining_evapotranspiration: f64 `unit:"mm"`,
	net_precipitation:            f64,
	evaporated_from_intercept:    f64 `unit:"mm"`,
	rooting_depth:                int `unit:"layer#"`,
	// per layer; views of the crop's arrays (valid for the day they are filled)
	transpiration:                []f64 `unit:"mm"`,
	// nitrogen - soil transport; per layer, a view like transpiration
	n_uptake_from_layer:          []f64,
	// carbon - soil organic
	net_primary_production:       f64 `unit:"kgC ha-1"`,
}

crop_to_soil_fill :: proc(cts: ^Crop_To_Soil, cm: ^Crop_Module, being_cleared: bool) {
	cts^ = {}
	if cm == nil {
		return
	}
	cts.has_crop = true
	cts.being_cleared = being_cleared
	cts.developmental_stage = cm.developmental_stage
	cts.soil_coverage = cm.soil_coverage
	cts.kc_factor = cm.kc_factor
	cts.kcb_factor = cm.kcb_factor
	cts.crop_height = cm.crop_height
	cts.leaf_area_index = cm.leaf_area_index
	cts.aboveground_biomass = cm.aboveground_biomass
	cts.reference_evapotranspiration = cm.reference_evapotranspiration
	cts.remaining_evapotranspiration = cm.remaining_evapotranspiration
	cts.net_precipitation = cm.net_precipitation
	cts.evaporated_from_intercept = cm.evaporated_from_intercept
	cts.rooting_depth = cm.rooting_depth
	cts.transpiration = cm.transpiration[:]
	cts.n_uptake_from_layer = cm.n_uptake_from_layer[:]
	cts.net_primary_production = cm.net_primary_production
}

// The crop's side of the automatic irrigation trigger
// (apply_irrigation_via_trigger). Separate from Crop_To_Soil because the
// trigger is checked at other times of the day: in the crop step and by the
// AutomaticIrrigation workstep.
Irrigation_Crop_Window :: struct {
	has_crop:                      bool,
	current_total_temperature_sum: f64 `unit:"°C d"`,
	heat_sum_irrigation_start:     f64 `unit:"°C d"`,
	heat_sum_irrigation_end:       f64 `unit:"°C d"`,
}

irrigation_crop_window :: proc(cm: ^Crop_Module) -> Irrigation_Crop_Window {
	if cm == nil {
		return {}
	}
	return {
		has_crop = true,
		current_total_temperature_sum = cm.current_total_temperature_sum,
		heat_sum_irrigation_start = cm.crop_params.cultivar_params.heat_sum_irrigation_start,
		heat_sum_irrigation_end = cm.crop_params.cultivar_params.heat_sum_irrigation_end,
	}
}
