package stics_soil_temperature

// - Name: soil_temp -Version: 1.0, -Time step: 1
// - Description:
//             * Title: soil temperature model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Soil_Temp_Component :: struct {
	temp_amp: Temp_Amp,
	temp_profile: Temp_Profile,
	layers_temp: Layers_Temp,
	canopy_temp_avg: Canopy_Temp_Avg,
	update: Update,
}

make_soil_temp_component :: proc(allocator := context.allocator) -> (c: Soil_Temp_Component) {
	context.allocator = allocator
	c.temp_amp = make_temp_amp()
	c.temp_profile = make_temp_profile()
	c.layers_temp = make_layers_temp()
	c.canopy_temp_avg = make_canopy_temp_avg()
	c.update = make_update()
	return
}

destroy_soil_temp_component :: proc(c: ^Soil_Temp_Component) {
	destroy_temp_amp(&c.temp_amp)
	destroy_temp_profile(&c.temp_profile)
	destroy_layers_temp(&c.layers_temp)
	destroy_canopy_temp_avg(&c.canopy_temp_avg)
	destroy_update(&c.update)
}

soil_temp_component_set_air_temp_day1 :: proc(c: ^Soil_Temp_Component, value: f64) {
	c.temp_profile.air_temp_day1 = value
}

soil_temp_component_set_layer_thick :: proc(c: ^Soil_Temp_Component, value: []int, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.temp_profile.layer_thick, len(value))
	copy(c.temp_profile.layer_thick[:], value)
	resize(&c.layers_temp.layer_thick, len(value))
	copy(c.layers_temp.layer_thick[:], value)
}

soil_temp_component_calculate_model :: proc(c: ^Soil_Temp_Component, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	temp_amp_calculate_model(&c.temp_amp, s, s1, r, a, ex)
	temp_profile_calculate_model(&c.temp_profile, s, s1, r, a, ex)
	layers_temp_calculate_model(&c.layers_temp, s, s1, r, a, ex)
	canopy_temp_avg_calculate_model(&c.canopy_temp_avg, s, s1, r, a, ex)
	update_calculate_model(&c.update, s, s1, r, a, ex)
}

soil_temp_component_init :: proc(c: ^Soil_Temp_Component, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	temp_profile_init(&c.temp_profile, s, s1, r, a, ex)
}