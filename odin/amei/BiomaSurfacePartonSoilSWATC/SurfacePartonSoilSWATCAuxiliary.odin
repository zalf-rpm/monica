package biomasurfacepartonsoilswatc

Surface_Parton_Soil_Swatc_Auxiliary :: struct {
	surface_temperature_minimum: f64 `unit:"degC"`, // Minimum surface soil temperature
	surface_temperature_maximum: f64 `unit:"degC"`, // Maximum surface soil temperature
	surface_soil_temperature: f64 `unit:"degC"`, // Average surface soil temperature
}

destroy_surface_parton_soil_swatc_auxiliary :: proc(x: ^Surface_Parton_Soil_Swatc_Auxiliary) {
}