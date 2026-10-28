package biomasurfacepartonsoilswatc

Surface_Parton_Soil_Swatc_Exogenous :: struct {
	air_temperature_minimum: f64 `unit:""`, // Minimum daily air temperature
	air_temperature_maximum: f64 `unit:""`, // Maximum daily air temperature
	above_ground_biomass: f64 `unit:"Kg ha-1"`, // Above ground biomass
	day_length: f64 `unit:"h"`, // Length of the day
	global_solar_radiation: f64 `unit:"Mj m-2 d-1"`, // Daily global solar radiation
	volumetric_water_content: [dynamic]f64 `unit:"m3 m-3"`, // Volumetric soil water content
}

destroy_surface_parton_soil_swatc_exogenous :: proc(x: ^Surface_Parton_Soil_Swatc_Exogenous) {
	delete(x.volumetric_water_content)
}