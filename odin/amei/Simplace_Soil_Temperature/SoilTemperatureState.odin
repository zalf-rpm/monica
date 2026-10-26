package simplace_soil_temperature

Soil_Temperature_State :: struct {
	p_internal_albedo: f64, // Albedo privat [http://www.wurvoc.org/vocabularies/om-1.8/one]
	snow_water_content: f64, // Snow water content [http://www.wurvoc.org/vocabularies/om-1.8/millimetre]
	soil_surface_temperature: f64, // Soil surface temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	age_of_snow: int, // Age of snow [http://www.wurvoc.org/vocabularies/om-1.8/day]
	r_soil_temp_array_rate: [dynamic]f64, // Array of daily temperature change [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day]
	p_soil_layer_depth: [dynamic]f64, // Depth of soil layer plus additional depth [http://www.wurvoc.org/vocabularies/om-1.8/metre]
	soil_temp_array: [dynamic]f64, // Array of soil temperatures in layers [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
}

destroy_soil_temperature_state :: proc(x: ^Soil_Temperature_State) {
	delete(x.r_soil_temp_array_rate)
	delete(x.p_soil_layer_depth)
	delete(x.soil_temp_array)
}