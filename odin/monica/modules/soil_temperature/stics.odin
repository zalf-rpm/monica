// Binding of the Crop2ML package Stics_soil_temperature
// (amei/Stics_soil_temperature/), STICS' soil temperature.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// Stics_soil_temperature_monica_interface.cpp): the layers in whole cm
// (rounded here, truncated there - thickness_cm), the first day's mean
// temperature; the air temperatures stand in for the canopy's. The first
// value of the 1 cm profile is the surface temperature.
package soil_temperature

import stics "../../../amei/Stics_soil_temperature"

Stics :: struct {
	c:            stics.Soil_Temp_Component,
	s, s1:        stics.Soil_Temp_State, // no ModelUnit reads s1
	r:            stics.Soil_Temp_Rate,
	a:            stics.Soil_Temp_Auxiliary,
	ex:           stics.Soil_Temp_Exogenous,
	no_of_layers: int,
}

make_stics :: proc(allocator: Allocator) -> Module {
	b := new(Stics, allocator)
	b.c = stics.make_soil_temp_component(allocator)
	return b
}

destroy_stics :: proc(b: ^Stics, allocator := context.allocator) {
	context.allocator = allocator
	stics.destroy_soil_temp_component(&b.c)
	stics.destroy_soil_temp_state(&b.s)
	stics.destroy_soil_temp_state(&b.s1)
	stics.destroy_soil_temp_rate(&b.r)
	stics.destroy_soil_temp_auxiliary(&b.a)
	stics.destroy_soil_temp_exogenous(&b.ex)
	free(b)
}

stics_init :: proc(b: ^Stics, in_: ^Input, allocator := context.allocator) {
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	thick := make([]int, n, context.temp_allocator)
	for i in 0 ..< n {
		thick[i] = int(thickness_cm(in_.layer_thickness[i]))
	}
	stics.soil_temp_component_set_layer_thick(c, thick, allocator)
	stics.soil_temp_component_set_air_temp_day1(c, in_.tavg)
	stics_exogenous(b, in_)
	stics.soil_temp_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

stics_step :: proc(b: ^Stics, in_: ^Input, out: ^Output, allocator := context.allocator) {
	stics_exogenous(b, in_)
	stics.soil_temp_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.temp_profile[0]
	copy(out.layer_temperature, b.s.layer_temp[:b.no_of_layers])
}

@(private = "file")
stics_exogenous :: proc(b: ^Stics, in_: ^Input) {
	b.ex.min_temp = in_.tmin
	b.ex.max_temp = in_.tmax
	b.ex.min_canopy_temp = in_.tmin
	b.ex.max_canopy_temp = in_.tmax
	b.ex.min_air_temp = in_.tmin
}

// The table row (soil_temperature.odin's `modules`)
stics_entry :: Module_Entry {
	name    = "Stics_soil_temperature",
	make    = make_stics,
	init    = proc(m: Module, in_: ^Input, a: Allocator) {stics_init(m.(^Stics), in_, a)},
	step    = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {stics_step(m.(^Stics), in_, out, a)},
	destroy = proc(m: Module, a: Allocator) {destroy_stics(m.(^Stics), a)},
}
