package biomasurfacepartonsoilswatc

Surface_Parton_Soil_Swatc_Exogenous :: struct {
	air_temperature_minimum: f64, // Minimum daily air temperature
	air_temperature_maximum: f64, // Maximum daily air temperature
	above_ground_biomass: f64, // Above ground biomass [Kg ha-1]
	day_length: f64, // Length of the day [h]
	global_solar_radiation: f64, // Daily global solar radiation [Mj m-2 d-1]
	volumetric_water_content: [dynamic]f64, // Volumetric soil water content [m3 m-3]
}

destroy_surface_parton_soil_swatc_exogenous :: proc(x: ^Surface_Parton_Soil_Swatc_Exogenous) {
	delete(x.volumetric_water_content)
}