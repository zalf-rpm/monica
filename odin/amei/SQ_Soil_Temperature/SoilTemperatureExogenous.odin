package sq_soil_temperature

Soil_Temperature_Exogenous :: struct {
	mean_t_air: f64, // Mean Air Temperature [Â°C]
	min_t_air: f64, // Minimum Air Temperature from Weather files [Â°C]
	mean_annual_air_temp: f64, // Annual Mean Air Temperature [Â°C]
	max_t_air: f64, // Maximum Air Temperature from Weather Files [Â°C]
	day_length: f64, // Length of the day [hour]
}

destroy_soil_temperature_exogenous :: proc(x: ^Soil_Temperature_Exogenous) {
}