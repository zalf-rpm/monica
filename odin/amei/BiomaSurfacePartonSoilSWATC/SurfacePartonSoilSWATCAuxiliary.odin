package biomasurfacepartonsoilswatc

Surface_Parton_Soil_Swatc_Auxiliary :: struct {
	surface_temperature_minimum: f64, // Minimum surface soil temperature [degC]
	surface_temperature_maximum: f64, // Maximum surface soil temperature [degC]
	surface_soil_temperature: f64, // Average surface soil temperature [degC]
}

destroy_surface_parton_soil_swatc_auxiliary :: proc(x: ^Surface_Parton_Soil_Swatc_Auxiliary) {
}