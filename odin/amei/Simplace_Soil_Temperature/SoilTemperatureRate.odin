package simplace_soil_temperature

Soil_Temperature_Rate :: struct {
	r_snow_water_content_rate: f64, // daily snow water content change rate [http://www.wurvoc.org/vocabularies/om-1.8/millimetre_per_day]
	r_soil_surface_temperature_rate: f64, // daily soil surface temperature change rate [http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day]
	r_age_of_snow_rate: int, // daily age of snow change rate [http://www.wurvoc.org/vocabularies/om-1.8/one]
}

destroy_soil_temperature_rate :: proc(x: ^Soil_Temperature_Rate) {
}