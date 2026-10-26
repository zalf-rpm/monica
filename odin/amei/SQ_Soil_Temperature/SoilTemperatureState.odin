package sq_soil_temperature

Soil_Temperature_State :: struct {
	min_t_soil: f64, // Minimum Soil Temperature [Â°C]
	deep_layer_t: f64, // Temperature of the last soil layer [Â°C]
	max_t_soil: f64, // Maximum Soil Temperature [Â°C]
	hourly_soil_t: [dynamic]f64, // Hourly Soil Temperature [Â°C]
}

destroy_soil_temperature_state :: proc(x: ^Soil_Temperature_State) {
	delete(x.hourly_soil_t)
}