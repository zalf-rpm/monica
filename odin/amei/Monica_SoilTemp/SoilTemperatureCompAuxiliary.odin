package monica_soiltemp

Soil_Temperature_Comp_Auxiliary :: struct {
	no_snow_soil_surface_temperature: f64, // soilSurfaceTemperature without snow [°C]
}

destroy_soil_temperature_comp_auxiliary :: proc(x: ^Soil_Temperature_Comp_Auxiliary) {
}