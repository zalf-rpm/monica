package simplace_soil_temperature

Soil_Temperature_State :: struct {
	p_internal_albedo: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // Albedo privat
	snow_water_content: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/millimetre"`, // Snow water content
	soil_surface_temperature: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Soil surface temperature
	age_of_snow: int `unit:"http://www.wurvoc.org/vocabularies/om-1.8/day"`, // Age of snow
	r_soil_temp_array_rate: [dynamic]f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day"`, // Array of daily temperature change
	p_soil_layer_depth: [dynamic]f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/metre"`, // Depth of soil layer plus additional depth
	soil_temp_array: [dynamic]f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Array of soil temperatures in layers
}

destroy_soil_temperature_state :: proc(x: ^Soil_Temperature_State) {
	delete(x.r_soil_temp_array_rate)
	delete(x.p_soil_layer_depth)
	delete(x.soil_temp_array)
}