package monica_evapotranspiration

Et_Auxiliary :: struct {
	use_external_vapor_pressure: bool `unit:"mm"`, // boolean condition to be met
	external_vapor_pressure: f64 `unit:"kPa"`, // value to be returned if condition is true
	calc_stomata_resistance: bool `unit:""`, // boolean condition to be met
	fixed_stomata_resistance: f64 `unit:"s/m"`, // value to be returned if condition is false
	gross_photosynthesis_reference_mol: f64 `unit:"[mol m-2 s-1] or [cm3 cm-2 s-1]"`, // gross_photosynthesis_reference_mol (-1 = not supplied = no crop)
	stomata_resistance: f64 `unit:"s/m"`, // stomata resistance (FAO default value is 100)
	net_radiation: f64 `unit:"MJ/m2"`, // net radiation
	use_external_et0: bool `unit:""`, // boolean condition to be met
	external_et0: f64 `unit:"mm"`, // value to be returned if condition is true
	declination: f64 `unit:""`, // declination
	astronomic_daylength: f64 `unit:"mm"`, // astronomic_daylength
	effective_daylength: f64 `unit:"mm"`, // effective_daylength
	photoperiodic_daylength: f64 `unit:"mm"`, // photoperiodic_daylength
	sunshine_hours_global_radiation: f64 `unit:"MJ/m2"`, // global radiation calculated from sunshine hours
	extraterrestrial_radiation: f64 `unit:"MJ/m2"`, // extraterrestrial_radiation
	clear_day_radiation: f64 `unit:"MJ/m2"`, // clear_day_radiation
	overcast_day_radiation: f64 `unit:"MJ/m2"`, // overcast_day_radiation
	phot_act_radiation_mean: f64 `unit:"J/m2"`, // phot_act_radiation_mean
	reference_evapotranspiration: f64 `unit:"mm"`, // reference evapotranspiration (ET0)
	potential_evapotranspiration: f64 `unit:"mm"`, // the potential evapotranspiration
	vapor_pressure: f64 `unit:"kPa"`, // vapor pressure from relative humidity and saturated vapor pressure
	internal_vapor_pressure: f64 `unit:"kPa"`, // value to be returned if condition is false
	internal_et0: f64 `unit:"mm"`, // value to be returned if condition is false
	saturated_vapor_pressure: f64 `unit:"kPa"`, // saturated vapor pressure
	saturation_vapor_pressure_deficit: f64 `unit:"kPa"`, // saturation vapor pressure deficit
	calculated_stomata_resistance: f64 `unit:"s/m"`, // value to be returned if condition is true
	et0: f64 `unit:"mm"`, // the output value
}

destroy_et_auxiliary :: proc(x: ^Et_Auxiliary) {
}