package sq_soil_temperature

Soil_Temperature_Rate :: struct {
	heat_flux: f64 `unit:"g m-2 d-1"`, // Soil Heat Flux from Energy Balance Component
}

destroy_soil_temperature_rate :: proc(x: ^Soil_Temperature_Rate) {
}