package stics_soil_temperature

Soil_Temp_Exogenous :: struct {
	min_temp: f64 `unit:"degC"`, // current minimum temperature
	max_temp: f64 `unit:"degC"`, // current maximum temperature
	min_air_temp: f64 `unit:"degC"`, // current minimum air temperature
	min_canopy_temp: f64 `unit:"degC"`, // current minimum temperature
	max_canopy_temp: f64 `unit:"degC"`, // current maximum temperature
}

destroy_soil_temp_exogenous :: proc(x: ^Soil_Temp_Exogenous) {
}