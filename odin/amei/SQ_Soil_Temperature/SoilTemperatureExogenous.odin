package sq_soil_temperature

Soil_Temperature_Exogenous :: struct {
	mean_t_air: f64 `unit:"Â°C"`, // Mean Air Temperature
	min_t_air: f64 `unit:"Â°C"`, // Minimum Air Temperature from Weather files
	mean_annual_air_temp: f64 `unit:"Â°C"`, // Annual Mean Air Temperature
	max_t_air: f64 `unit:"Â°C"`, // Maximum Air Temperature from Weather Files
	day_length: f64 `unit:"hour"`, // Length of the day
}

destroy_soil_temperature_exogenous :: proc(x: ^Soil_Temperature_Exogenous) {
}