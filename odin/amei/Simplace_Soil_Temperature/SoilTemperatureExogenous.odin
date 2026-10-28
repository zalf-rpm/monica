package simplace_soil_temperature

Soil_Temperature_Exogenous :: struct {
	i_air_temperature_max: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Daily maximum air temperature
	i_temp_max: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Daily maximum air temperature
	i_air_temperature_min: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Daily minimum air temperature
	i_temp_min: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius"`, // Daily minimum air temperature
	i_global_solar_radiation: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/megajoule_per_square_metre"`, // Global Solar radiation
	i_radiation: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/megajoule_per_square_metre"`, // Global Solar radiation
	i_rain: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/millimetre"`, // Rain amount
	i_crop_residues: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/gram_per_square_metre"`, // Crop residues plus above ground biomass
	i_potential_soil_evaporation: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/millimetre"`, // Potenial Evaporation
	i_leaf_area_index: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/square_metre_per_square_metre"`, // Leaf area index
	i_soil_water_content: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/millimetre"`, // Water content, sum of whole soil profile
}

destroy_soil_temperature_exogenous :: proc(x: ^Soil_Temperature_Exogenous) {
}