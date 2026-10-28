package biomasurfacepartonsoilswatc

Surface_Parton_Soil_Swatc_State :: struct {
	soil_temperature_by_layers: [dynamic]f64 `unit:"degC"`, // Soil temperature of each layer
}

destroy_surface_parton_soil_swatc_state :: proc(x: ^Surface_Parton_Soil_Swatc_State) {
	delete(x.soil_temperature_by_layers)
}