package simplace_soil_temperature

Soil_Temperature_Auxiliary :: struct {
	soil_temp_array: [dynamic]f64, // Soil Temp array of last day [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	i_soil_temp_array: [dynamic]f64, // Soil Temp array of last day [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	snow_isolation_index: f64, // Snow isolation index [http://www.wurvoc.org/vocabularies/om-1.8/one]
	i_soil_surface_temperature: f64, // Temperature at soil surface [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
}

destroy_soil_temperature_auxiliary :: proc(x: ^Soil_Temperature_Auxiliary) {
	delete(x.soil_temp_array)
	delete(x.i_soil_temp_array)
}