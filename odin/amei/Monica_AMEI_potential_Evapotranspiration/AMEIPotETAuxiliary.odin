package monica_amei_potential_evapotranspiration

Amei_Pot_Et_Auxiliary :: struct {
	vapor_pressure: f64, // vapor pressure [kPa]
	saturated_vapor_pressure: f64, // saturated vapor pressure [kPa]
	stomata_resistance: f64, // stomata resistance (FAO default value is 100) [s/m]
	net_radiation: f64, // net radiation [MJ/m2]
	reference_evapotranspiration: f64, // reference evapotranspiration (ET0) [mm]
	potential_evapotranspiration: f64, // the potential evapotranspiration [mm]
	saturation_vapor_pressure_deficit: f64, // saturation vapor pressure deficit [kPa]
}

destroy_amei_pot_et_auxiliary :: proc(x: ^Amei_Pot_Et_Auxiliary) {
}