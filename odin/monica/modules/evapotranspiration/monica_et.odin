// Binding of the Crop2ML package Monica_Evapotranspiration
// (amei/Monica_Evapotranspiration/), MONICA's own evapotranspiration
// extracted into Crop2ML units.
//
// After the C++ AMEI adapter (monica_amei, src/modules/evapotranspiration/
// Monica_Evapotranspiration_monica_interface.cpp): MONICA's soil moisture and
// crop module parameters, one more soil moisture "layer" than soil layers
// (the arrays repeat the last layer), the climate's ET0 and vapour pressure
// where it has them, the crop's transpiration, remaining ET and intercepted
// evaporation, the stomata resistance calculated with a crop.
//
// Set here, where the adapter left them at 0: CO2, the crop's gross
// photosynthesis and carboxylation pathway, stomata conductance alpha and
// saturation beta (which the module's stomata resistance needs with a crop),
// and the fixed stomata resistance without one, MONICA's 100 s m-1.
package evapotranspiration

import et "../../../amei/Monica_Evapotranspiration"

Monica_Et :: struct {
	c:            et.Et_Component,
	s, s1:        et.Et_State, // no ModelUnit reads s1
	r:            et.Et_Rate,
	a:            et.Et_Auxiliary,
	ex:           et.Et_Exogenous,
	no_of_layers: int,
}

make_monica_et :: proc(allocator: Allocator) -> Module {
	b := new(Monica_Et, allocator)
	b.c = et.make_et_component(allocator)
	return b
}

destroy_monica_et :: proc(b: ^Monica_Et, allocator := context.allocator) {
	context.allocator = allocator
	et.destroy_et_component(&b.c)
	et.destroy_et_state(&b.s)
	et.destroy_et_state(&b.s1)
	et.destroy_et_rate(&b.r)
	et.destroy_et_auxiliary(&b.a)
	et.destroy_et_exogenous(&b.ex)
	free(b)
}

monica_et_init :: proc(b: ^Monica_Et, in_: ^Input, allocator := context.allocator) {
	context.allocator = allocator
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	et.et_component_set_latitude(c, in_.latitude)
	et.et_component_set_height_nn(c, in_.height_nn)
	et.et_component_set_reference_albedo(c, in_.reference_albedo)
	et.et_component_set_evaporation_zeta(c, in_.evaporation_zeta)
	et.et_component_set_maximum_evaporation_impact_depth(c, in_.maximum_evaporation_impact_depth)
	et.et_component_set_xsa_critical_soil_moisture(c, in_.xsa_critical_soil_moisture)
	et.et_component_set_stomata_conductance_alpha(c, in_.stomata_conductance_alpha)
	et.et_component_set_saturation_beta(c, in_.saturation_beta)
	et.et_component_set_no_of_soil_layers(c, n)
	et.et_component_set_no_of_soil_moisture_layers(c, n + 1)
	plus_one :: proc(xs: []f64) -> []f64 {
		ys := make([]f64, len(xs) + 1, context.temp_allocator)
		copy(ys, xs)
		ys[len(xs)] = xs[len(xs) - 1]
		return ys
	}
	et.et_component_set_layer_thickness(c, plus_one(in_.layer_thickness), allocator)
	et.et_component_set_permanent_wilting_point(c, plus_one(in_.permanent_wilting_point), allocator)
	et.et_component_set_field_capacity(c, plus_one(in_.field_capacity), allocator)
	resize(&b.ex.crop_transpiration, n)
	b.a.fixed_stomata_resistance = 100 // MONICA's STOMATA_RESISTANCE (soil_moisture.odin)
	monica_et_inputs(b, in_)
	et.et_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

monica_et_step :: proc(b: ^Monica_Et, in_: ^Input, out: ^Output, allocator := context.allocator) {
	monica_et_inputs(b, in_)
	n := b.no_of_layers
	// wilting point and field capacity as MONICA has them today; into the
	// unit's arrays directly, the setters would copy (one more "layer", as in init)
	pwp, fc := b.c.evapotranspiration.permanent_wilting_point[:], b.c.evapotranspiration.field_capacity[:]
	copy(pwp[:n], in_.permanent_wilting_point)
	copy(fc[:n], in_.field_capacity)
	pwp[n], fc[n] = in_.permanent_wilting_point[n - 1], in_.field_capacity[n - 1]
	// the state MONICA has before the step
	copy(b.s.soil_moisture[:n], in_.soil_moisture)
	b.s.soil_moisture[n] = in_.soil_moisture[n - 1]
	b.s.surface_water_storage = in_.surface_water_storage
	et.et_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.reference_evapotranspiration = b.a.reference_evapotranspiration
	out.potential_evapotranspiration = b.a.potential_evapotranspiration
	out.net_radiation = b.a.net_radiation
	out.evaporated_from_surface = b.s.evaporated_from_surface
	out.actual_evaporation = b.s.actual_evaporation
	out.actual_transpiration = b.s.actual_transpiration
	out.actual_evapotranspiration = b.s.actual_evapotranspiration
	out.surface_water_storage = b.s.surface_water_storage
	copy(out.soil_moisture, b.s.soil_moisture[:n])
	copy(out.evaporation, b.s.evaporation[:n])
	copy(out.transpiration, b.s.transpiration[:n])
	copy(out.evapotranspiration, b.s.evapotranspiration[:n])
}

@(private = "file")
monica_et_inputs :: proc(b: ^Monica_Et, in_: ^Input) {
	c := &b.c
	et.et_component_set_carboxylation_pathway(c, in_.carboxylation_pathway)
	ex := &b.ex
	ex.global_radiation = in_.globrad
	ex.julian_day = in_.julian_day
	ex.max_air_temperature = in_.tmax
	ex.min_air_temperature = in_.tmin
	ex.mean_air_temperature = in_.tavg
	ex.relative_humidity = in_.relative_humidity
	ex.atmospheric_co2_concentration = in_.co2
	ex.wind_speed = in_.wind
	ex.wind_speed_height = in_.wind_speed_height
	ex.kc_factor = in_.kc_factor
	ex.developmental_stage = in_.developmental_stage
	ex.crop_remaining_evapotranspiration = in_.crop_remaining_evapotranspiration
	ex.has_snow_cover = in_.snow_depth > 0.0
	copy(ex.crop_transpiration[:], in_.crop_transpiration)
	ex.crop_evaporated_from_intercepted = in_.crop_evaporated_from_intercept
	ex.percentage_soil_coverage = in_.soil_coverage
	a := &b.a
	a.use_external_et0 = in_.has_external_et0
	a.external_et0 = in_.external_et0
	a.use_external_vapor_pressure = in_.has_external_vapor_pressure
	a.external_vapor_pressure = in_.external_vapor_pressure
	a.calc_stomata_resistance = in_.developmental_stage > 0
	a.gross_photosynthesis_reference_mol = in_.gross_photosynthesis_reference_mol
}

// The table row (evapotranspiration.odin's `modules`)
monica_et_entry :: Module_Entry {
	name    = "Monica_Evapotranspiration",
	make    = make_monica_et,
	init    = proc(m: Module, in_: ^Input, a: Allocator) {monica_et_init(m.(^Monica_Et), in_, a)},
	step    = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {monica_et_step(m.(^Monica_Et), in_, out, a)},
	destroy = proc(m: Module, a: Allocator) {destroy_monica_et(m.(^Monica_Et), a)},
	integrated = true,
}
