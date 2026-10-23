package dssat_st_standalone

Stemp_State :: struct {
	hday: f64, // Haverst day [day]
	srftemp: f64, // Temperature of soil surface litter [degC]
	st: [dynamic]f64, // Soil temperature in soil layer L [degC]
	tma: [dynamic]f64, // Array of previous 5 days of average soil temperatures [degC]
	tdl: f64, // Total water content of soil at drained upper limit [cm]
	cumdpt: f64, // Cumulative depth of soil profile [mm]
	atot: f64, // Sum of TMA array (last 5 days soil temperature) [degC]
	dsmid: [dynamic]f64, // Depth to midpoint of soil layer L [cm]
}

destroy_stemp_state :: proc(x: ^Stemp_State) {
	delete(x.st)
	delete(x.tma)
	delete(x.dsmid)
}