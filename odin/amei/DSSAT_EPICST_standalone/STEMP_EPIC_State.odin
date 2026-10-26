package dssat_epicst_standalone

Stemp_Epic_State :: struct {
	wet_day: [dynamic]int, // Wet Days [day]
	srftemp: f64, // Temperature of soil surface litter [degC]
	n_days: int, // Number of days ... [day]
	st: [dynamic]f64, // Soil temperature in soil layer NL [degC]
	tma: [dynamic]f64, // Array of previous 5 days of average soil temperatures. [degC]
	tdl: f64, // Total water content of soil at drained upper limit [cm]
	x2_prev: f64, // Temperature of soil surface at precedent timestep [degC]
	dsmid: [dynamic]f64, // Depth to midpoint of soil layer NL [cm]
	cumdpt: f64, // Cumulative depth of soil profile [mm]
}

destroy_stemp_epic_state :: proc(x: ^Stemp_Epic_State) {
	delete(x.wet_day)
	delete(x.st)
	delete(x.tma)
	delete(x.dsmid)
}