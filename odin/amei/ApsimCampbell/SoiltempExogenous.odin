package apsimcampbell

Soiltemp_Exogenous :: struct {
	water_balance_eo: f64, // Potential soil evapotranspiration from soil surface [mm]
	water_balance_salb: f64, // Fraction of incoming radiation reflected from bare soil [0-1]
	organic_carbon: [dynamic]f64, // Total organic carbon [%]
	water_balance_es: f64, // Actual (realised) soil water evaporation [mm]
	weather_wind: f64, // Wind speed [m/s]
	physical_particle_size_sand: [dynamic]f64, // Particle size sand [%]
	weather_air_pressure: f64, // Air pressure [hPa]
	clock_today_day_of_year: int, // Day of year [day number]
	micro_climate_canopy_height: f64, // Canopy height [mm]
	water_balance_eos: f64, // Potential soil evaporation from soil surface [mm]
	water_balance_sw: [dynamic]f64, // Volumetric water content [mm/mm]
	weather_amp: f64, // Annual average temperature amplitude [oC]
	weather_min_t: f64, // Minimum temperature [oC]
	weather_radn: f64, // Solar radiation [MJ/m2/day]
	physical_rocks: [dynamic]f64, // Rocks [%]
	weather_tav: f64, // Annual average temperature [oC]
	weather_max_t: f64, // Maximum temperature [oC]
	weather_mean_t: f64, // Mean temperature [oC]
	physical_particle_size_silt: [dynamic]f64, // Particle size silt [%]
	physical_particle_size_clay: [dynamic]f64, // Particle size clay [%]
}

destroy_soiltemp_exogenous :: proc(x: ^Soiltemp_Exogenous) {
	delete(x.organic_carbon)
	delete(x.physical_particle_size_sand)
	delete(x.water_balance_sw)
	delete(x.physical_rocks)
	delete(x.physical_particle_size_silt)
	delete(x.physical_particle_size_clay)
}