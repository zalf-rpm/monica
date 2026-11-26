package monica_reference_evapotranspiration

Amei_Ref_Et_Exogenous :: struct {
	mean_air_temperature: f64 `unit:"°C"`, // daily average air temperature
	wind_speed: f64 `unit:"m/s"`, // wind speed measured at wind speed height
	wind_speed_height: f64 `unit:"m"`, // height at which the wind speed has been measured
}

destroy_amei_ref_et_exogenous :: proc(x: ^Amei_Ref_Et_Exogenous) {
}