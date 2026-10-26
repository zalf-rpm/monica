package monica_evapotranspiration

Et_Auxiliary :: struct {
	use_external_vapor_pressure: bool, // boolean condition to be met [mm]
	external_vapor_pressure: f64, // value to be returned if condition is true [kPa]
	calc_stomata_resistance: bool, // boolean condition to be met
	fixed_stomata_resistance: f64, // value to be returned if condition is false [s/m]
	gross_photosynthesis_reference_mol: f64, // gross_photosynthesis_reference_mol (-1 = not supplied = no crop) [[mol m-2 s-1] or [cm3 cm-2 s-1]]
	stomata_resistance: f64, // stomata resistance (FAO default value is 100) [s/m]
	net_radiation: f64, // net radiation [MJ/m2]
	use_external_et0: bool, // boolean condition to be met
	external_et0: f64, // value to be returned if condition is true [mm]
	declination: f64, // declination
	astronomic_daylength: f64, // astronomic_daylength [mm]
	effective_daylength: f64, // effective_daylength [mm]
	photoperiodic_daylength: f64, // photoperiodic_daylength [mm]
	sunshine_hours_global_radiation: f64, // global radiation calculated from sunshine hours [MJ/m2]
	extraterrestrial_radiation: f64, // extraterrestrial_radiation [MJ/m2]
	clear_day_radiation: f64, // clear_day_radiation [MJ/m2]
	overcast_day_radiation: f64, // overcast_day_radiation [MJ/m2]
	phot_act_radiation_mean: f64, // phot_act_radiation_mean [J/m2]
	reference_evapotranspiration: f64, // reference evapotranspiration (ET0) [mm]
	potential_evapotranspiration: f64, // the potential evapotranspiration [mm]
	vapor_pressure: f64, // vapor pressure from relative humidity and saturated vapor pressure [kPa]
	internal_vapor_pressure: f64, // value to be returned if condition is false [kPa]
	internal_et0: f64, // value to be returned if condition is false [mm]
	saturated_vapor_pressure: f64, // saturated vapor pressure [kPa]
	saturation_vapor_pressure_deficit: f64, // saturation vapor pressure deficit [kPa]
	calculated_stomata_resistance: f64, // value to be returned if condition is true [s/m]
	et0: f64, // the output value [mm]
}

destroy_et_auxiliary :: proc(x: ^Et_Auxiliary) {
}