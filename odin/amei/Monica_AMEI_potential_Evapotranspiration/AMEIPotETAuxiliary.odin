package monica_amei_potential_evapotranspiration

Amei_Pot_Et_Auxiliary :: struct {
	vapor_pressure: f64 `unit:"kPa"`, // vapor pressure
	saturated_vapor_pressure: f64 `unit:"kPa"`, // saturated vapor pressure
	stomata_resistance: f64 `unit:"s/m"`, // stomata resistance (FAO default value is 100)
	net_radiation: f64 `unit:"MJ/m2"`, // net radiation
	reference_evapotranspiration: f64 `unit:"mm"`, // reference evapotranspiration (ET0)
	potential_evapotranspiration: f64 `unit:"mm"`, // the potential evapotranspiration
	saturation_vapor_pressure_deficit: f64 `unit:"kPa"`, // saturation vapor pressure deficit
}

destroy_amei_pot_et_auxiliary :: proc(x: ^Amei_Pot_Et_Auxiliary) {
}