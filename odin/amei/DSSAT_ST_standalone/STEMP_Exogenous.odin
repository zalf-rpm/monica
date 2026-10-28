package dssat_st_standalone

Stemp_Exogenous :: struct {
	tmax: f64 `unit:"degC"`, // Maximum daily temperature
	srad: f64 `unit:"MJ/m2-d"`, // Solar radiation
	tamp: f64 `unit:"degC"`, // Amplitude of temperature function used to calculate soil temperatures
	tavg: f64 `unit:"degC"`, // Average daily temperature
	tav: f64 `unit:"degC"`, // Average annual soil temperature, used with TAMP to calculate soil temperature.
	doy: int `unit:"d"`, // Current day of simulation
}

destroy_stemp_exogenous :: proc(x: ^Stemp_Exogenous) {
}