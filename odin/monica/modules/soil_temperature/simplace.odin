// Binding of the Crop2ML package Simplace_Soil_Temperature
// (amei/Simplace_Soil_Temperature/), SIMPLACE's snow cover and STMPsim.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// Simplace_Soil_Temperature_monica_interface.cpp): snow isolation factors 2.3
// and 0.22, damping depth 6 m, MONICA's albedo; the snow's water equivalent
// at the start; the top layer's organic carbon in %, the mean bulk density in
// t m-3, the layers' depths in m; the climate's annual average temperature
// and the first day's mean temperature. Daily: the rain ("so that no snow
// cover will build up" - the module builds its own from it), MONICA's LAI,
// ET0 * KcFactor (the bare soil's) as potential soil evaporation, the
// profile's water in mm, no crop residues.
//
// The package declares the initial snow water content in percent; the
// adapter passes mm, which is what the module computes with.
package soil_temperature

import simp "../../../amei/Simplace_Soil_Temperature"

Simplace :: struct {
	c:            simp.Soil_Temperature_Component,
	s, s1:        simp.Soil_Temperature_State, // no ModelUnit reads s1
	r:            simp.Soil_Temperature_Rate,
	a:            simp.Soil_Temperature_Auxiliary,
	ex:           simp.Soil_Temperature_Exogenous,
	no_of_layers: int,
}

make_simplace :: proc(allocator: Allocator) -> Module {
	b := new(Simplace, allocator)
	b.c = simp.make_soil_temperature_component(allocator)
	return b
}

destroy_simplace :: proc(b: ^Simplace, allocator := context.allocator) {
	context.allocator = allocator
	simp.destroy_soil_temperature_component(&b.c)
	simp.destroy_soil_temperature_state(&b.s)
	simp.destroy_soil_temperature_state(&b.s1)
	simp.destroy_soil_temperature_rate(&b.r)
	simp.destroy_soil_temperature_auxiliary(&b.a)
	simp.destroy_soil_temperature_exogenous(&b.ex)
	free(b)
}

simplace_init :: proc(b: ^Simplace, in_: ^Input, allocator := context.allocator) {
	context.allocator = allocator
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	simp.soil_temperature_component_set_c_initial_age_of_snow(c, 0)
	simp.soil_temperature_component_set_c_snow_isolation_factor_a(c, 2.3)
	simp.soil_temperature_component_set_c_snow_isolation_factor_b(c, 0.22)
	simp.soil_temperature_component_set_c_initial_snow_water_content(c, in_.snow_water_equivalent)
	simp.soil_temperature_component_set_c_albedo(c, in_.albedo)
	simp.soil_temperature_component_set_c_damping_depth(c, 6)
	simp.soil_temperature_component_set_c_carbon_content(c, in_.organic_carbon[0] * 100.0)
	depths := make([]f64, n, context.temp_allocator)
	depth, bd := 0.0, 0.0
	for i in 0 ..< n {
		depth += in_.layer_thickness[i]
		depths[i] = depth
		bd += in_.bulk_density[i] / 1000.0 // kg m-3 -> t m-3
	}
	simp.soil_temperature_component_set_c_average_bulk_density(c, bd / f64(n))
	simp.soil_temperature_component_set_c_soil_layer_depth(c, depths, allocator)
	simp.soil_temperature_component_set_c_average_ground_temperature(c, in_.tav)
	simp.soil_temperature_component_set_c_first_day_mean_temp(c, in_.tavg)
	simplace_exogenous(b, in_)
	simp.soil_temperature_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

simplace_step :: proc(b: ^Simplace, in_: ^Input, out: ^Output, allocator := context.allocator) {
	simplace_exogenous(b, in_)
	simp.soil_temperature_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.soil_surface_temperature
	copy(out.layer_temperature, b.s.soil_temp_array[:b.no_of_layers])
}

@(private = "file")
simplace_exogenous :: proc(b: ^Simplace, in_: ^Input) {
	ex := &b.ex
	ex.i_air_temperature_min = in_.tmin
	ex.i_air_temperature_max = in_.tmax
	ex.i_global_solar_radiation = in_.globrad
	ex.i_rain = in_.precip
	ex.i_leaf_area_index = in_.lai
	ex.i_potential_soil_evaporation = in_.et0 * in_.kc_factor_bare_soil
	water := 0.0
	for sm, i in in_.soil_moisture {
		water += sm * 10 * in_.layer_thickness[i] * 100 // m3 m-3 -> mm, as the adapter
	}
	ex.i_soil_water_content = water
	ex.i_crop_residues = 0
}

// The table row (soil_temperature.odin's `modules`)
simplace_entry :: Module_Entry {
	name    = "Simplace_Soil_Temperature",
	make    = make_simplace,
	init    = proc(m: Module, in_: ^Input, a: Allocator) {simplace_init(m.(^Simplace), in_, a)},
	step    = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {simplace_step(m.(^Simplace), in_, out, a)},
	destroy = proc(m: Module, a: Allocator) {destroy_simplace(m.(^Simplace), a)},
}
