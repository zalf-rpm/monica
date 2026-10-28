package monica_soiltemp

Soil_Temperature_Comp_Exogenous :: struct {
	tmin: f64 `unit:"°C"`, // the days min air temperature
	tmax: f64 `unit:"°C"`, // the days max air temperature
	globrad: f64 `unit:"MJ/m**2/d"`, // the days global radiation
	soil_coverage: f64 `unit:"dimensionless"`, // soilCoverage
	soil_surface_temperature_below_snow: f64 `unit:"°C"`, // soilSurfaceTemperature below snow cover
	has_snow_cover: bool `unit:"dimensionless"`, // is soil covered by snow
}

destroy_soil_temperature_comp_exogenous :: proc(x: ^Soil_Temperature_Comp_Exogenous) {
}