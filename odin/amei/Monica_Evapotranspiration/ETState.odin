package monica_evapotranspiration

Et_State :: struct {
	surface_water_storage: f64 `unit:"mm"`, // Simulates a virtual layer that contains the surface water
	evaporated_from_surface: f64 `unit:"mm"`, // evaporated_from_surface
	actual_evaporation: f64 `unit:"mm"`, // actual evaporation
	actual_transpiration: f64 `unit:"mm"`, // actual transpiration
	soil_moisture: [dynamic]f64 `unit:"m3/m3"`, // soil moisture array
	evaporation: [dynamic]f64 `unit:"mm"`, // evaporation array
	transpiration: [dynamic]f64 `unit:"mm"`, // transpiration array
	evapotranspiration: [dynamic]f64 `unit:"mm"`, // evapotranspiration array
	actual_evapotranspiration: f64 `unit:"mm"`, // actual evapotranspiration
}

destroy_et_state :: proc(x: ^Et_State) {
	delete(x.soil_moisture)
	delete(x.evaporation)
	delete(x.transpiration)
	delete(x.evapotranspiration)
}