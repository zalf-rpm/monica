package monica_reference_evapotranspiration

Amei_Ref_Et_Auxiliary :: struct {
	stomata_resistance: f64 `unit:"s/m"`, // stomata resistance (FAO default value is 100)
	net_radiation: f64 `unit:"MJ/m2"`, // net radiation
	saturation_vapor_pressure_deficit: f64 `unit:"kPa"`, // saturation_vapor_pressure_deficit
	reference_evapotranspiration: f64 `unit:"mm"`, // reference evapotranspiration (ET0)
}

destroy_amei_ref_et_auxiliary :: proc(x: ^Amei_Ref_Et_Auxiliary) {
}