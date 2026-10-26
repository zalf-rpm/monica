package monica_amei_potential_evapotranspiration

Amei_Pot_Et_Exogenous :: struct {
	mean_air_temperature: f64, // daily average air temperature [°C]
	wind_speed: f64, // wind speed measured at wind speed height [m/s]
	wind_speed_height: f64, // height at which the wind speed has been measured [m]
	kc_factor: f64, // crop coefficient ETc/ET0 [dimensionless]
	developmental_stage: int, // MONICA crop developmental stage [dimensionless]
	crop_remaining_evapotranspiration: f64, // crop remaining evapotranspiration [mm]
}

destroy_amei_pot_et_exogenous :: proc(x: ^Amei_Pot_Et_Exogenous) {
}