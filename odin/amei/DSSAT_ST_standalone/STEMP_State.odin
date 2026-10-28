package dssat_st_standalone

Stemp_State :: struct {
	hday: f64 `unit:"day"`, // Haverst day
	srftemp: f64 `unit:"degC"`, // Temperature of soil surface litter
	st: [dynamic]f64 `unit:"degC"`, // Soil temperature in soil layer L
	tma: [dynamic]f64 `unit:"degC"`, // Array of previous 5 days of average soil temperatures
	tdl: f64 `unit:"cm"`, // Total water content of soil at drained upper limit
	cumdpt: f64 `unit:"mm"`, // Cumulative depth of soil profile
	atot: f64 `unit:"degC"`, // Sum of TMA array (last 5 days soil temperature)
	dsmid: [dynamic]f64 `unit:"cm"`, // Depth to midpoint of soil layer L
}

destroy_stemp_state :: proc(x: ^Stemp_State) {
	delete(x.st)
	delete(x.tma)
	delete(x.dsmid)
}