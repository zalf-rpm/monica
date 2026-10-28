package stics_soil_temperature

Soil_Temp_State :: struct {
	prev_temp_profile: [dynamic]f64 `unit:"degC"`, // previous soil temperature profile (for 1 cm layers)
	prev_canopy_temp: f64 `unit:"degC"`, // previous crop temperature
	temp_amp: f64 `unit:"degC"`, // current temperature amplitude
	temp_profile: [dynamic]f64 `unit:"degC"`, // current soil profile temperature (for 1 cm layers)
	layer_temp: [dynamic]f64 `unit:"degC"`, // soil layers temperature
	canopy_temp_avg: f64 `unit:"degC"`, // current temperature amplitude
}

destroy_soil_temp_state :: proc(x: ^Soil_Temp_State) {
	delete(x.prev_temp_profile)
	delete(x.temp_profile)
	delete(x.layer_temp)
}