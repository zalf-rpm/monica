package monica_soiltemp

Soil_Temperature_Comp_Auxiliary :: struct {
	no_snow_soil_surface_temperature: f64 `unit:"°C"`, // soilSurfaceTemperature without snow
}

destroy_soil_temperature_comp_auxiliary :: proc(x: ^Soil_Temperature_Comp_Auxiliary) {
}