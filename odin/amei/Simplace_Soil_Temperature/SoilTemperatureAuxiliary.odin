package simplace_soil_temperature

Soil_Temperature_Auxiliary :: struct {
	soil_temp_array: [dynamic]f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Soil Temp array of last day
	i_soil_temp_array: [dynamic]f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Soil Temp array of last day
	snow_isolation_index: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // Snow isolation index
	i_soil_surface_temperature: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Temperature at soil surface
}

destroy_soil_temperature_auxiliary :: proc(x: ^Soil_Temperature_Auxiliary) {
	delete(x.soil_temp_array)
	delete(x.i_soil_temp_array)
}