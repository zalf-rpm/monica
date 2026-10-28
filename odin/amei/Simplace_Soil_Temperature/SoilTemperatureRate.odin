package simplace_soil_temperature

Soil_Temperature_Rate :: struct {
	r_snow_water_content_rate: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/millimetre_per_day"`, // daily snow water content change rate
	r_soil_surface_temperature_rate: f64 `unit:"http://www.wurvoc.org/vocabularies/om-1.8/degree_Celsius_per_day"`, // daily soil surface temperature change rate
	r_age_of_snow_rate: int `unit:"http://www.wurvoc.org/vocabularies/om-1.8/one"`, // daily age of snow change rate
}

destroy_soil_temperature_rate :: proc(x: ^Soil_Temperature_Rate) {
}