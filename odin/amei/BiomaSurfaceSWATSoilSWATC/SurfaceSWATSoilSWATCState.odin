package biomasurfaceswatsoilswatc

Surface_Swat_Soil_Swatc_State :: struct {
	soil_temperature_by_layers: [dynamic]f64 `unit:"degC"`, // Soil temperature of each layer
}

destroy_surface_swat_soil_swatc_state :: proc(x: ^Surface_Swat_Soil_Swatc_State) {
	delete(x.soil_temperature_by_layers)
}