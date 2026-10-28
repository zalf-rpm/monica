package monica_evapotranspiration

Et_Exogenous :: struct {
	global_radiation: f64 `unit:"MJ/m2"`, // global radiation
	julian_day: int `unit:"day"`, // day of year
	sunshine_hours: f64 `unit:"h"`, // number of sunshine hours
	max_air_temperature: f64 `unit:"°C"`, // daily maximum air temperature
	min_air_temperature: f64 `unit:"°C"`, // daily minimum air temperature
	relative_humidity: f64 `unit:"fraction"`, // relative humidity
	atmospheric_co2_concentration: f64 `unit:"ppm"`, // CO2 concentration in the atmosphere. Optional if no crop.
	mean_air_temperature: f64 `unit:"°C"`, // daily average air temperature
	wind_speed: f64 `unit:"m/s"`, // wind speed measured at wind speed height
	wind_speed_height: f64 `unit:"m"`, // height at which the wind speed has been measured
	kc_factor: f64 `unit:"dimensionless"`, // crop coefficient ETc/ET0
	developmental_stage: int `unit:"dimensionless"`, // MONICA crop developmental stage
	crop_remaining_evapotranspiration: f64 `unit:"mm"`, // crop remaining evapotranspiration
	has_snow_cover: bool `unit:"dimensionless"`, // is the soil covered by snow
	crop_transpiration: [dynamic]f64 `unit:"mm"`, // crop transpiration array
	crop_evaporated_from_intercepted: f64 `unit:"mm"`, // crop evaporated water from intercepted water
	percentage_soil_coverage: f64 `unit:"m2/m2"`, // fraction of soil covered by crop
}

destroy_et_exogenous :: proc(x: ^Et_Exogenous) {
	delete(x.crop_transpiration)
}