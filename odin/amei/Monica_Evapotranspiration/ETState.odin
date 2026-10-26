package monica_evapotranspiration

Et_State :: struct {
	surface_water_storage: f64, // Simulates a virtual layer that contains the surface water [mm]
	evaporated_from_surface: f64, // evaporated_from_surface [mm]
	actual_evaporation: f64, // actual evaporation [mm]
	actual_transpiration: f64, // actual transpiration [mm]
	soil_moisture: [dynamic]f64, // soil moisture array [m3/m3]
	evaporation: [dynamic]f64, // evaporation array [mm]
	transpiration: [dynamic]f64, // transpiration array [mm]
	evapotranspiration: [dynamic]f64, // evapotranspiration array [mm]
	actual_evapotranspiration: f64, // actual evapotranspiration [mm]
}

destroy_et_state :: proc(x: ^Et_State) {
	delete(x.soil_moisture)
	delete(x.evaporation)
	delete(x.transpiration)
	delete(x.evapotranspiration)
}