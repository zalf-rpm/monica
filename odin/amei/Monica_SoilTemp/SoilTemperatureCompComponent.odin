package monica_soiltemp

// - Name: SoilTemperatureComp -Version: 1, -Time step: 1
// - Description:
//             * Title: SoilTemperature model
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the soil temperature in all layers and soil surface temperature
Soil_Temperature_Comp_Component :: struct {
	no_snow_soil_surface_temperature: No_Snow_Soil_Surface_Temperature,
	with_snow_soil_surface_temperature: With_Snow_Soil_Surface_Temperature,
	soil_temperature: Soil_Temperature,
}

make_soil_temperature_comp_component :: proc(allocator := context.allocator) -> (c: Soil_Temperature_Comp_Component) {
	context.allocator = allocator
	c.no_snow_soil_surface_temperature = make_no_snow_soil_surface_temperature()
	c.with_snow_soil_surface_temperature = make_with_snow_soil_surface_temperature()
	c.soil_temperature = make_soil_temperature()
	return
}

destroy_soil_temperature_comp_component :: proc(c: ^Soil_Temperature_Comp_Component) {
	destroy_no_snow_soil_surface_temperature(&c.no_snow_soil_surface_temperature)
	destroy_with_snow_soil_surface_temperature(&c.with_snow_soil_surface_temperature)
	destroy_soil_temperature(&c.soil_temperature)
}

soil_temperature_comp_component_set_damping_factor :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.no_snow_soil_surface_temperature.damping_factor = value
}

soil_temperature_comp_component_set_time_step :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.time_step = value
}

soil_temperature_comp_component_set_soil_moisture_const :: proc(c: ^Soil_Temperature_Comp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.soil_moisture_const, len(value))
	copy(c.soil_temperature.soil_moisture_const[:], value)
}

soil_temperature_comp_component_set_base_temp :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.base_temp = value
}

soil_temperature_comp_component_set_initial_surface_temp :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.initial_surface_temp = value
}

soil_temperature_comp_component_set_density_air :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.density_air = value
}

soil_temperature_comp_component_set_specific_heat_capacity_air :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.specific_heat_capacity_air = value
}

soil_temperature_comp_component_set_density_humus :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.density_humus = value
}

soil_temperature_comp_component_set_specific_heat_capacity_humus :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.specific_heat_capacity_humus = value
}

soil_temperature_comp_component_set_density_water :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.density_water = value
}

soil_temperature_comp_component_set_specific_heat_capacity_water :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.specific_heat_capacity_water = value
}

soil_temperature_comp_component_set_quartz_raw_density :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.quartz_raw_density = value
}

soil_temperature_comp_component_set_specific_heat_capacity_quartz :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.specific_heat_capacity_quartz = value
}

soil_temperature_comp_component_set_n_tau :: proc(c: ^Soil_Temperature_Comp_Component, value: f64) {
	c.soil_temperature.n_tau = value
}

soil_temperature_comp_component_set_no_of_temp_layers :: proc(c: ^Soil_Temperature_Comp_Component, value: int) {
	c.soil_temperature.no_of_temp_layers = value
}

soil_temperature_comp_component_set_no_of_temp_layers_plus1 :: proc(c: ^Soil_Temperature_Comp_Component, value: int) {
	c.soil_temperature.no_of_temp_layers_plus1 = value
}

soil_temperature_comp_component_set_no_of_soil_layers :: proc(c: ^Soil_Temperature_Comp_Component, value: int) {
	c.soil_temperature.no_of_soil_layers = value
}

soil_temperature_comp_component_set_layer_thickness :: proc(c: ^Soil_Temperature_Comp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.layer_thickness, len(value))
	copy(c.soil_temperature.layer_thickness[:], value)
}

soil_temperature_comp_component_set_soil_bulk_density :: proc(c: ^Soil_Temperature_Comp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.soil_bulk_density, len(value))
	copy(c.soil_temperature.soil_bulk_density[:], value)
}

soil_temperature_comp_component_set_saturation :: proc(c: ^Soil_Temperature_Comp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.saturation, len(value))
	copy(c.soil_temperature.saturation[:], value)
}

soil_temperature_comp_component_set_soil_organic_matter :: proc(c: ^Soil_Temperature_Comp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.soil_organic_matter, len(value))
	copy(c.soil_temperature.soil_organic_matter[:], value)
}

soil_temperature_comp_component_calculate_model :: proc(c: ^Soil_Temperature_Comp_Component, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	no_snow_soil_surface_temperature_calculate_model(&c.no_snow_soil_surface_temperature, s, s1, r, a, ex)
	a.no_snow_soil_surface_temperature = s.soil_surface_temperature
	with_snow_soil_surface_temperature_calculate_model(&c.with_snow_soil_surface_temperature, s, s1, r, a, ex)
	soil_temperature_calculate_model(&c.soil_temperature, s, s1, r, a, ex)
}

soil_temperature_comp_component_init :: proc(c: ^Soil_Temperature_Comp_Component, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	soil_temperature_init(&c.soil_temperature, s, s1, r, a, ex)
}