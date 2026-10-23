package monica_soiltemp

Soil_Temperature_Comp_Exogenous :: struct {
	tmin: f64, // the days min air temperature [°C]
	tmax: f64, // the days max air temperature [°C]
	globrad: f64, // the days global radiation [MJ/m**2/d]
	soil_coverage: f64, // soilCoverage [dimensionless]
	soil_surface_temperature_below_snow: f64, // soilSurfaceTemperature below snow cover [°C]
	has_snow_cover: bool, // is soil covered by snow [dimensionless]
}

destroy_soil_temperature_comp_exogenous :: proc(x: ^Soil_Temperature_Comp_Exogenous) {
}