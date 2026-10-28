package sq_soil_temperature

Soil_Temperature_State :: struct {
	min_t_soil: f64 `unit:"Â°C"`, // Minimum Soil Temperature
	deep_layer_t: f64 `unit:"Â°C"`, // Temperature of the last soil layer
	max_t_soil: f64 `unit:"Â°C"`, // Maximum Soil Temperature
	hourly_soil_t: [dynamic]f64 `unit:"Â°C"`, // Hourly Soil Temperature
}

destroy_soil_temperature_state :: proc(x: ^Soil_Temperature_State) {
	delete(x.hourly_soil_t)
}