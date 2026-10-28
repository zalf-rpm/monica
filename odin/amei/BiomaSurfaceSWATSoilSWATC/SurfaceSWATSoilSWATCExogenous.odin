package biomasurfaceswatsoilswatc

Surface_Swat_Soil_Swatc_Exogenous :: struct {
	air_temperature_maximum: f64 `unit:""`, // Maximum daily air temperature
	air_temperature_minimum: f64 `unit:""`, // Minimum daily air temperature
	global_solar_radiation: f64 `unit:"Mj m-2 d-1"`, // Daily global solar radiation
	water_equivalent_of_snow_pack: f64 `unit:"mm"`, // Water equivalent of snow pack
	albedo: f64 `unit:"unitless"`, // Albedo of soil
	volumetric_water_content: [dynamic]f64 `unit:"m3 m-3"`, // Volumetric soil water content
}

destroy_surface_swat_soil_swatc_exogenous :: proc(x: ^Surface_Swat_Soil_Swatc_Exogenous) {
	delete(x.volumetric_water_content)
}