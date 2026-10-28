package monica_amei_potential_evapotranspiration

Amei_Pot_Et_Exogenous :: struct {
	mean_air_temperature: f64 `unit:"°C"`, // daily average air temperature
	wind_speed: f64 `unit:"m/s"`, // wind speed measured at wind speed height
	wind_speed_height: f64 `unit:"m"`, // height at which the wind speed has been measured
	kc_factor: f64 `unit:"dimensionless"`, // crop coefficient ETc/ET0
	developmental_stage: int `unit:"dimensionless"`, // MONICA crop developmental stage
	crop_remaining_evapotranspiration: f64 `unit:"mm"`, // crop remaining evapotranspiration
}

destroy_amei_pot_et_exogenous :: proc(x: ^Amei_Pot_Et_Exogenous) {
}