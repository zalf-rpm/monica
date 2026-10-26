package simplace_soil_temperature

// - Name: SoilTemperature -Version: 001, -Time step: 1
// - Description:
//             * Title: SoilTemperature model
//             * Authors: Gunther Krauss
//             * Reference: ('http://www.simplace.net/doc/simplace_modules/',)
//             * Institution: INRES Pflanzenbau, Uni Bonn
//             * ExtendedDescription: as given in the documentation
//             * ShortDescription: None
Soil_Temperature_Component :: struct {
	snow_cover_calculator: Snow_Cover_Calculator,
	stm_psim_calculator: Stm_Psim_Calculator,
}

make_soil_temperature_component :: proc(allocator := context.allocator) -> (c: Soil_Temperature_Component) {
	context.allocator = allocator
	c.snow_cover_calculator = make_snow_cover_calculator()
	c.stm_psim_calculator = make_stm_psim_calculator()
	return
}

destroy_soil_temperature_component :: proc(c: ^Soil_Temperature_Component) {
	destroy_snow_cover_calculator(&c.snow_cover_calculator)
	destroy_stm_psim_calculator(&c.stm_psim_calculator)
}

soil_temperature_component_set_c_carbon_content :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.snow_cover_calculator.c_carbon_content = value
}

soil_temperature_component_set_c_albedo :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.snow_cover_calculator.albedo = value
}

soil_temperature_component_set_c_initial_age_of_snow :: proc(c: ^Soil_Temperature_Component, value: int) {
	c.snow_cover_calculator.c_initial_age_of_snow = value
}

soil_temperature_component_set_c_initial_snow_water_content :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.snow_cover_calculator.c_initial_snow_water_content = value
}

soil_temperature_component_set_c_snow_isolation_factor_a :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.snow_cover_calculator.c_snow_isolation_factor_a = value
}

soil_temperature_component_set_c_snow_isolation_factor_b :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.snow_cover_calculator.c_snow_isolation_factor_b = value
}

soil_temperature_component_set_c_soil_layer_depth :: proc(c: ^Soil_Temperature_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stm_psim_calculator.c_soil_layer_depth, len(value))
	copy(c.stm_psim_calculator.c_soil_layer_depth[:], value)
}

soil_temperature_component_set_c_first_day_mean_temp :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.stm_psim_calculator.c_first_day_mean_temp = value
}

soil_temperature_component_set_c_average_ground_temperature :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.stm_psim_calculator.c_avt = value
}

soil_temperature_component_set_c_average_bulk_density :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.stm_psim_calculator.c_abd = value
}

soil_temperature_component_set_c_damping_depth :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.stm_psim_calculator.c_damping_depth = value
}

soil_temperature_component_calculate_model :: proc(c: ^Soil_Temperature_Component, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	ex.i_temp_max = ex.i_air_temperature_max
	ex.i_temp_min = ex.i_air_temperature_min
	ex.i_radiation = ex.i_global_solar_radiation
	resize(&a.i_soil_temp_array, len(s.soil_temp_array[:]))
	copy(a.i_soil_temp_array[:], s.soil_temp_array[:])
	snow_cover_calculator_calculate_model(&c.snow_cover_calculator, s, s1, r, a, ex)
	a.i_soil_surface_temperature = s.soil_surface_temperature
	stm_psim_calculator_calculate_model(&c.stm_psim_calculator, s, s1, r, a, ex)
}

soil_temperature_component_init :: proc(c: ^Soil_Temperature_Component, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	ex.i_temp_max = ex.i_air_temperature_max
	ex.i_temp_min = ex.i_air_temperature_min
	ex.i_radiation = ex.i_global_solar_radiation
	resize(&a.i_soil_temp_array, len(s.soil_temp_array))
	copy(a.i_soil_temp_array[:], s.soil_temp_array[:])
	snow_cover_calculator_init(&c.snow_cover_calculator, s, s1, r, a, ex)
	stm_psim_calculator_init(&c.stm_psim_calculator, s, s1, r, a, ex)
}