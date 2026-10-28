package monica_soiltemp

Soil_Temperature_Comp_State :: struct {
	v: [dynamic]f64 `unit:"°C"`, // V
	b: [dynamic]f64 `unit:"°C"`, // B
	volume_matrix: [dynamic]f64 `unit:"°C"`, // volumeMatrix
	volume_matrix_old: [dynamic]f64 `unit:"°C"`, // volumeMatrixOld
	matrix_primary_diagonal: [dynamic]f64 `unit:"°C"`, // matrixPrimaryDiagonal
	matrix_secondary_diagonal: [dynamic]f64 `unit:"°C"`, // matrixSecondaryDiagonal
	heat_conductivity: [dynamic]f64 `unit:"°C"`, // heatConductivity
	heat_conductivity_mean: [dynamic]f64 `unit:"°C"`, // heatConductivityMean
	heat_capacity: [dynamic]f64 `unit:"°C"`, // heatCapacity
	solution: [dynamic]f64 `unit:"°C"`, // solution
	matrix_diagonal: [dynamic]f64 `unit:"°C"`, // matrixDiagonal
	matrix_lower_triangle: [dynamic]f64 `unit:"°C"`, // matrixLowerTriangle
	heat_flow: [dynamic]f64 `unit:"°C"`, // heatFlow
	soil_surface_temperature: f64 `unit:"°C"`, // soilSurfaceTemperature
	soil_temperature: [dynamic]f64 `unit:"°C"`, // soilTemperature next day
}

destroy_soil_temperature_comp_state :: proc(x: ^Soil_Temperature_Comp_State) {
	delete(x.v)
	delete(x.b)
	delete(x.volume_matrix)
	delete(x.volume_matrix_old)
	delete(x.matrix_primary_diagonal)
	delete(x.matrix_secondary_diagonal)
	delete(x.heat_conductivity)
	delete(x.heat_conductivity_mean)
	delete(x.heat_capacity)
	delete(x.solution)
	delete(x.matrix_diagonal)
	delete(x.matrix_lower_triangle)
	delete(x.heat_flow)
	delete(x.soil_temperature)
}