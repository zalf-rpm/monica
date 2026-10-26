// Binding of the Crop2ML package SQ_Soil_Temperature (amei/SQ_Soil_Temperature/),
// SiriusQuality's soil temperature: no layers, one minimum and maximum.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// SQ_Soil_Temperature_monica_interface.cpp): a 0.5, b 1.81, c 0.49, lambda
// 2.454; the day length; the climate's annual average temperature; no soil
// heat flux (it would come from SiriusQuality's energy balance). The mean of
// the minimum and maximum is the surface's and every layer's temperature.
package soil_temperature

import sq "../../../amei/SQ_Soil_Temperature"

Sq :: struct {
	c:     sq.Soil_Temperature_Component,
	s, s1: sq.Soil_Temperature_State, // no ModelUnit reads s1
	r:     sq.Soil_Temperature_Rate,
	a:     sq.Soil_Temperature_Auxiliary,
	ex:    sq.Soil_Temperature_Exogenous,
}

make_sq :: proc(allocator := context.allocator) -> Module {
	b := new(Sq, allocator)
	b.c = sq.make_soil_temperature_component(allocator)
	return b
}

destroy_sq :: proc(b: ^Sq, allocator := context.allocator) {
	context.allocator = allocator
	sq.destroy_soil_temperature_component(&b.c)
	sq.destroy_soil_temperature_state(&b.s)
	sq.destroy_soil_temperature_state(&b.s1)
	sq.destroy_soil_temperature_rate(&b.r)
	sq.destroy_soil_temperature_auxiliary(&b.a)
	sq.destroy_soil_temperature_exogenous(&b.ex)
	free(b)
}

sq_init :: proc(b: ^Sq, in_: ^Input, allocator := context.allocator) {
	c := &b.c
	sq.soil_temperature_component_set_a(c, 0.5)
	sq.soil_temperature_component_set_b(c, 1.81)
	sq.soil_temperature_component_set_c(c, 0.49)
	sq.soil_temperature_component_set_lambda_(c, 2.454)
	sq_exogenous(b, in_)
	sq.soil_temperature_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

sq_step :: proc(b: ^Sq, in_: ^Input, out: ^Output, allocator := context.allocator) {
	sq_exogenous(b, in_)
	sq.soil_temperature_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	avg := (b.s.min_t_soil + b.s.max_t_soil) / 2.0
	out.soil_surface_temperature = avg
	for &t in out.layer_temperature {
		t = avg
	}
}

@(private = "file")
sq_exogenous :: proc(b: ^Sq, in_: ^Input) {
	b.ex.day_length = in_.day_length
	b.ex.max_t_air = in_.tmax
	b.ex.min_t_air = in_.tmin
	b.ex.mean_t_air = in_.tavg
	b.ex.mean_annual_air_temp = in_.tav
	b.r.heat_flux = 0
}
