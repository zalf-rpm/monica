package biomasurfaceswatsoilswatc

Surface_Swat_Soil_Swatc_Auxiliary :: struct {
	above_ground_biomass: f64, // Above ground biomass [Kg ha-1]
	surface_soil_temperature: f64, // Average surface soil temperature [degC]
}

destroy_surface_swat_soil_swatc_auxiliary :: proc(x: ^Surface_Swat_Soil_Swatc_Auxiliary) {
}