package dssat_epicst_standalone

Stemp_Epic_Exogenous :: struct {
	tav: f64, // Average annual soil temperature, used with TAMP to calculate soil temperature. [degC]
	rain: f64, // daily rainfall [mm]
	biomas: f64, // Biomass [kg/ha]
	snow: f64, // Snow cover [mm]
	tavg: f64, // Average daily temperature [degC]
	depir: f64, // Depth of irrigation [mm]
	mulchmass: f64, // Mulch Mass [kg/ha]
	tmax: f64, // Maximum daily temperature [degC]
	tmin: f64, // Minimum Temperature [degC]
	tamp: f64, // Annual amplitude of the average air temperature [degC]
}

destroy_stemp_epic_exogenous :: proc(x: ^Stemp_Epic_Exogenous) {
}