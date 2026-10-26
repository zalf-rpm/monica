package sq_soil_temperature

// - Name: SoilTemperature -Version: 001, -Time step: 1
// - Description:
//             * Title: SoilTemperature model
//             * Authors: loic.manceau@inra.fr
//             * Reference: ('http://biomamodelling.org',)
//             * Institution: INRA
//             * ExtendedDescription: Composite Class for soil temperature
//             * ShortDescription: None
Soil_Temperature_Component :: struct {
	calculate_soil_temperature: Calculate_Soil_Temperature,
	calculate_hourly_soil_temperature: Calculate_Hourly_Soil_Temperature,
}

make_soil_temperature_component :: proc(allocator := context.allocator) -> (c: Soil_Temperature_Component) {
	context.allocator = allocator
	c.calculate_soil_temperature = make_calculate_soil_temperature()
	c.calculate_hourly_soil_temperature = make_calculate_hourly_soil_temperature()
	return
}

destroy_soil_temperature_component :: proc(c: ^Soil_Temperature_Component) {
	destroy_calculate_soil_temperature(&c.calculate_soil_temperature)
	destroy_calculate_hourly_soil_temperature(&c.calculate_hourly_soil_temperature)
}

soil_temperature_component_set_lambda_ :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.calculate_soil_temperature.lambda_ = value
}

soil_temperature_component_set_b :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.calculate_hourly_soil_temperature.b = value
}

soil_temperature_component_set_c :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.calculate_hourly_soil_temperature.c = value
}

soil_temperature_component_set_a :: proc(c: ^Soil_Temperature_Component, value: f64) {
	c.calculate_hourly_soil_temperature.a = value
}

soil_temperature_component_calculate_model :: proc(c_: ^Soil_Temperature_Component, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a_: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	calculate_soil_temperature_calculate_model(&c_.calculate_soil_temperature, s, s1, r, a_, ex)
	calculate_hourly_soil_temperature_calculate_model(&c_.calculate_hourly_soil_temperature, s, s1, r, a_, ex)
}

soil_temperature_component_init :: proc(c_: ^Soil_Temperature_Component, s: ^Soil_Temperature_State, s1: ^Soil_Temperature_State, r: ^Soil_Temperature_Rate, a_: ^Soil_Temperature_Auxiliary, ex: ^Soil_Temperature_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	calculate_soil_temperature_init(&c_.calculate_soil_temperature, s, s1, r, a_, ex)
}