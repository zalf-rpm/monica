package monica_evapotranspiration

Et_Exogenous :: struct {
	global_radiation: f64, // global radiation [MJ/m2]
	julian_day: int, // day of year [day]
	sunshine_hours: f64, // number of sunshine hours [h]
	max_air_temperature: f64, // daily maximum air temperature [°C]
	min_air_temperature: f64, // daily minimum air temperature [°C]
	relative_humidity: f64, // relative humidity [fraction]
	atmospheric_co2_concentration: f64, // CO2 concentration in the atmosphere. Optional if no crop. [ppm]
	mean_air_temperature: f64, // daily average air temperature [°C]
	wind_speed: f64, // wind speed measured at wind speed height [m/s]
	wind_speed_height: f64, // height at which the wind speed has been measured [m]
	kc_factor: f64, // crop coefficient ETc/ET0 [dimensionless]
	developmental_stage: int, // MONICA crop developmental stage [dimensionless]
	crop_remaining_evapotranspiration: f64, // crop remaining evapotranspiration [mm]
	has_snow_cover: bool, // is the soil covered by snow [dimensionless]
	crop_transpiration: [dynamic]f64, // crop transpiration array [mm]
	crop_evaporated_from_intercepted: f64, // crop evaporated water from intercepted water [mm]
	percentage_soil_coverage: f64, // fraction of soil covered by crop [m2/m2]
}

destroy_et_exogenous :: proc(x: ^Et_Exogenous) {
	delete(x.crop_transpiration)
}