package dssat_st_standalone

Stemp_Exogenous :: struct {
	tmax: f64, // Maximum daily temperature [degC]
	srad: f64, // Solar radiation [MJ/m2-d]
	tamp: f64, // Amplitude of temperature function used to calculate soil temperatures [degC]
	tavg: f64, // Average daily temperature [degC]
	tav: f64, // Average annual soil temperature, used with TAMP to calculate soil temperature. [degC]
	doy: int, // Current day of simulation [d]
}

destroy_stemp_exogenous :: proc(x: ^Stemp_Exogenous) {
}