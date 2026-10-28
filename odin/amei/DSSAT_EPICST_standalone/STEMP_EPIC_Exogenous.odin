package dssat_epicst_standalone

Stemp_Epic_Exogenous :: struct {
	tav: f64 `unit:"degC"`, // Average annual soil temperature, used with TAMP to calculate soil temperature.
	rain: f64 `unit:"mm"`, // daily rainfall
	biomas: f64 `unit:"kg/ha"`, // Biomass
	snow: f64 `unit:"mm"`, // Snow cover
	tavg: f64 `unit:"degC"`, // Average daily temperature
	depir: f64 `unit:"mm"`, // Depth of irrigation
	mulchmass: f64 `unit:"kg/ha"`, // Mulch Mass
	tmax: f64 `unit:"degC"`, // Maximum daily temperature
	tmin: f64 `unit:"degC"`, // Minimum Temperature
	tamp: f64 `unit:"degC"`, // Annual amplitude of the average air temperature
}

destroy_stemp_epic_exogenous :: proc(x: ^Stemp_Epic_Exogenous) {
}