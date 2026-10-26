package stics_soil_temperature

Soil_Temp_State :: struct {
	prev_temp_profile: [dynamic]f64, // previous soil temperature profile (for 1 cm layers) [degC]
	prev_canopy_temp: f64, // previous crop temperature [degC]
	temp_amp: f64, // current temperature amplitude [degC]
	temp_profile: [dynamic]f64, // current soil profile temperature (for 1 cm layers) [degC]
	layer_temp: [dynamic]f64, // soil layers temperature [degC]
	canopy_temp_avg: f64, // current temperature amplitude [degC]
}

destroy_soil_temp_state :: proc(x: ^Soil_Temp_State) {
	delete(x.prev_temp_profile)
	delete(x.temp_profile)
	delete(x.layer_temp)
}