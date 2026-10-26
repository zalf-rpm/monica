package simplace_soil_temperature

Soil_Temperature_Exogenous :: struct {
	i_air_temperature_max: f64, // Daily maximum air temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	i_temp_max: f64, // Daily maximum air temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	i_air_temperature_min: f64, // Daily minimum air temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	i_temp_min: f64, // Daily minimum air temperature [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius]
	i_global_solar_radiation: f64, // Global Solar radiation [http://www.wurvoc.org/vocabularies/om-1.8/megajoule_per_square_metre]
	i_radiation: f64, // Global Solar radiation [http://www.wurvoc.org/vocabularies/om-1.8/megajoule_per_square_metre]
	i_rain: f64, // Rain amount [http://www.wurvoc.org/vocabularies/om-1.8/millimetre]
	i_crop_residues: f64, // Crop residues plus above ground biomass [http://www.wurvoc.org/vocabularies/om-1.8/gram_per_square_metre]
	i_potential_soil_evaporation: f64, // Potenial Evaporation [http://www.wurvoc.org/vocabularies/om-1.8/millimetre]
	i_leaf_area_index: f64, // Leaf area index [http://www.wurvoc.org/vocabularies/om-1.8/square_metre_per_square_metre]
	i_soil_water_content: f64, // Water content, sum of whole soil profile [http://www.wurvoc.org/vocabularies/om-1.8/millimetre]
}

destroy_soil_temperature_exogenous :: proc(x: ^Soil_Temperature_Exogenous) {
}