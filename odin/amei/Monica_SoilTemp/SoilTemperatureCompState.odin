package monica_soiltemp

Soil_Temperature_Comp_State :: struct {
	v: [dynamic]f64, // V [°C]
	b: [dynamic]f64, // B [°C]
	volume_matrix: [dynamic]f64, // volumeMatrix [°C]
	volume_matrix_old: [dynamic]f64, // volumeMatrixOld [°C]
	matrix_primary_diagonal: [dynamic]f64, // matrixPrimaryDiagonal [°C]
	matrix_secondary_diagonal: [dynamic]f64, // matrixSecondaryDiagonal [°C]
	heat_conductivity: [dynamic]f64, // heatConductivity [°C]
	heat_conductivity_mean: [dynamic]f64, // heatConductivityMean [°C]
	heat_capacity: [dynamic]f64, // heatCapacity [°C]
	solution: [dynamic]f64, // solution [°C]
	matrix_diagonal: [dynamic]f64, // matrixDiagonal [°C]
	matrix_lower_triangle: [dynamic]f64, // matrixLowerTriangle [°C]
	heat_flow: [dynamic]f64, // heatFlow [°C]
	soil_surface_temperature: f64, // soilSurfaceTemperature [°C]
	soil_temperature: [dynamic]f64, // soilTemperature next day [°C]
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