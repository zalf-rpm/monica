package biomasurfaceswatsoilswatc

Surface_Swat_Soil_Swatc_Auxiliary :: struct {
	above_ground_biomass: f64 `unit:"Kg ha-1"`, // Above ground biomass
	surface_soil_temperature: f64 `unit:"degC"`, // Average surface soil temperature
}

destroy_surface_swat_soil_swatc_auxiliary :: proc(x: ^Surface_Swat_Soil_Swatc_Auxiliary) {
}