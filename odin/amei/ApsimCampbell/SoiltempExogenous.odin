package apsimcampbell

Soiltemp_Exogenous :: struct {
	water_balance_eo: f64 `unit:"mm"`, // Potential soil evapotranspiration from soil surface
	water_balance_salb: f64 `unit:"0-1"`, // Fraction of incoming radiation reflected from bare soil
	organic_carbon: [dynamic]f64 `unit:"%"`, // Total organic carbon
	water_balance_es: f64 `unit:"mm"`, // Actual (realised) soil water evaporation
	weather_wind: f64 `unit:"m/s"`, // Wind speed
	physical_particle_size_sand: [dynamic]f64 `unit:"%"`, // Particle size sand
	weather_air_pressure: f64 `unit:"hPa"`, // Air pressure
	clock_today_day_of_year: int `unit:"day number"`, // Day of year
	micro_climate_canopy_height: f64 `unit:"mm"`, // Canopy height
	water_balance_eos: f64 `unit:"mm"`, // Potential soil evaporation from soil surface
	water_balance_sw: [dynamic]f64 `unit:"mm/mm"`, // Volumetric water content
	weather_amp: f64 `unit:"oC"`, // Annual average temperature amplitude
	weather_min_t: f64 `unit:"oC"`, // Minimum temperature
	weather_radn: f64 `unit:"MJ/m2/day"`, // Solar radiation
	physical_rocks: [dynamic]f64 `unit:"%"`, // Rocks
	weather_tav: f64 `unit:"oC"`, // Annual average temperature
	weather_max_t: f64 `unit:"oC"`, // Maximum temperature
	weather_mean_t: f64 `unit:"oC"`, // Mean temperature
	physical_particle_size_silt: [dynamic]f64 `unit:"%"`, // Particle size silt
	physical_particle_size_clay: [dynamic]f64 `unit:"%"`, // Particle size clay
}

destroy_soiltemp_exogenous :: proc(x: ^Soiltemp_Exogenous) {
	delete(x.organic_carbon)
	delete(x.physical_particle_size_sand)
	delete(x.water_balance_sw)
	delete(x.physical_rocks)
	delete(x.physical_particle_size_silt)
	delete(x.physical_particle_size_clay)
}