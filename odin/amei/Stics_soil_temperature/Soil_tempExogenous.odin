package stics_soil_temperature

Soil_Temp_Exogenous :: struct {
	min_temp: f64, // current minimum temperature [degC]
	max_temp: f64, // current maximum temperature [degC]
	min_air_temp: f64, // current minimum air temperature [degC]
	min_canopy_temp: f64, // current minimum temperature [degC]
	max_canopy_temp: f64, // current maximum temperature [degC]
}

destroy_soil_temp_exogenous :: proc(x: ^Soil_Temp_Exogenous) {
}