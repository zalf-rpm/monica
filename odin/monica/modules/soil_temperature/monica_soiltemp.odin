// Binding of the Crop2ML package Monica_SoilTemp (amei/Monica_SoilTemp/), the
// Crop2ML version of MONICA's own soil temperature (core/soil_temperature.odin).
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// Monica_SoilTemp_monica_interface.cpp), with these differences:
//   - the Component's init, not the ModelUnit's (plan-crop2ml-modules.md §1.3);
//     it calls only that ModelUnit's init, so the result is the same
//   - the constant soil moisture is MONICA's own, params.soil_moisture (0.25
//     m3 m-3) in every layer, as in core/soil_temperature.odin. The adapter
//     used pwp + 0.25 * (fc - pwp) instead.
//   - the damping factor is MONICA's 0.8 (core/soil_temperature.odin); the
//     AMEI branch read it from its own parameter, which MONICA doesn't have
package soil_temperature

import mst "../../../amei/Monica_SoilTemp"

// The temperature layers are MONICA's layers plus a ground and a bottom layer.
Monica_Soil_Temp :: struct {
	c:             mst.Soil_Temperature_Comp_Component,
	s, s1:         mst.Soil_Temperature_Comp_State, // no ModelUnit reads s1
	r:             mst.Soil_Temperature_Comp_Rate,
	a:             mst.Soil_Temperature_Comp_Auxiliary,
	ex:            mst.Soil_Temperature_Comp_Exogenous,
	no_of_layers:  int,
}

make_monica_soil_temp :: proc(allocator := context.allocator) -> Module {
	b := new(Monica_Soil_Temp, allocator)
	b.c = mst.make_soil_temperature_comp_component(allocator)
	return b
}

destroy_monica_soil_temp :: proc(b: ^Monica_Soil_Temp, allocator := context.allocator) {
	context.allocator = allocator
	mst.destroy_soil_temperature_comp_component(&b.c)
	mst.destroy_soil_temperature_comp_state(&b.s)
	mst.destroy_soil_temperature_comp_state(&b.s1)
	mst.destroy_soil_temperature_comp_rate(&b.r)
	mst.destroy_soil_temperature_comp_auxiliary(&b.a)
	mst.destroy_soil_temperature_comp_exogenous(&b.ex)
	free(b)
}

monica_soil_temp_init :: proc(b: ^Monica_Soil_Temp, in_: ^Input, allocator := context.allocator) {
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	ps := &in_.params
	mst.soil_temperature_comp_component_set_time_step(c, in_.time_step)
	mst.soil_temperature_comp_component_set_base_temp(c, ps.base_temperature)
	mst.soil_temperature_comp_component_set_initial_surface_temp(c, ps.initial_surface_temperature)
	mst.soil_temperature_comp_component_set_density_air(c, ps.density_air)
	mst.soil_temperature_comp_component_set_specific_heat_capacity_air(c, ps.specific_heat_capacity_air)
	mst.soil_temperature_comp_component_set_density_humus(c, ps.density_humus)
	mst.soil_temperature_comp_component_set_specific_heat_capacity_humus(c, ps.specific_heat_capacity_humus)
	mst.soil_temperature_comp_component_set_density_water(c, ps.density_water)
	mst.soil_temperature_comp_component_set_specific_heat_capacity_water(c, ps.specific_heat_capacity_water)
	mst.soil_temperature_comp_component_set_quartz_raw_density(c, ps.quartz_raw_density)
	mst.soil_temperature_comp_component_set_specific_heat_capacity_quartz(c, ps.specific_heat_capacity_quartz)
	mst.soil_temperature_comp_component_set_n_tau(c, ps.n_tau)
	mst.soil_temperature_comp_component_set_damping_factor(c, 0.8)
	mst.soil_temperature_comp_component_set_no_of_soil_layers(c, n)
	mst.soil_temperature_comp_component_set_no_of_temp_layers(c, n + 2)
	mst.soil_temperature_comp_component_set_no_of_temp_layers_plus1(c, n + 3)

	// ground and bottom layer: init overwrites their thickness (2 * the last
	// layer's, and 1 m), as MONICA's own does
	lts := make([]f64, n + 2, context.temp_allocator)
	copy(lts, in_.layer_thickness)
	lts[n], lts[n + 1] = in_.layer_thickness[n - 1], in_.layer_thickness[n - 1]
	smcs := make([]f64, n, context.temp_allocator)
	for &smc in smcs {
		smc = ps.soil_moisture
	}
	mst.soil_temperature_comp_component_set_layer_thickness(c, lts, allocator)
	mst.soil_temperature_comp_component_set_soil_moisture_const(c, smcs, allocator)
	mst.soil_temperature_comp_component_set_soil_bulk_density(c, in_.bulk_density, allocator)
	mst.soil_temperature_comp_component_set_saturation(c, in_.saturation, allocator)
	mst.soil_temperature_comp_component_set_soil_organic_matter(c, in_.organic_matter, allocator)

	monica_soil_temp_exogenous(b, in_)
	mst.soil_temperature_comp_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

monica_soil_temp_step :: proc(
	b: ^Monica_Soil_Temp,
	in_: ^Input,
	out: ^Output,
	allocator := context.allocator,
) {
	monica_soil_temp_exogenous(b, in_)
	mst.soil_temperature_comp_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.soil_surface_temperature
	copy(out.layer_temperature, b.s.soil_temperature[:b.no_of_layers])
}

@(private = "file")
monica_soil_temp_exogenous :: proc(b: ^Monica_Soil_Temp, in_: ^Input) {
	b.ex.tmin = in_.tmin
	b.ex.tmax = in_.tmax
	b.ex.globrad = in_.globrad
	b.ex.soil_coverage = in_.soil_coverage
	// MONICA's own: if snow_depth > 0, the temperature under the snow
	b.ex.has_snow_cover = in_.snow_depth > 0.0
	b.ex.soil_surface_temperature_below_snow = in_.temperature_under_snow
}
