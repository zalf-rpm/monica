package biomasurfaceswatsoilswatc

Surface_Swat_Soil_Swatc_Exogenous :: struct {
	air_temperature_maximum: f64, // Maximum daily air temperature
	air_temperature_minimum: f64, // Minimum daily air temperature
	global_solar_radiation: f64, // Daily global solar radiation [Mj m-2 d-1]
	water_equivalent_of_snow_pack: f64, // Water equivalent of snow pack [mm]
	albedo: f64, // Albedo of soil [unitless]
	volumetric_water_content: [dynamic]f64, // Volumetric soil water content [m3 m-3]
}

destroy_surface_swat_soil_swatc_exogenous :: proc(x: ^Surface_Swat_Soil_Swatc_Exogenous) {
	delete(x.volumetric_water_content)
}