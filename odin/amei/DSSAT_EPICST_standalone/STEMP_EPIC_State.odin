package dssat_epicst_standalone

Stemp_Epic_State :: struct {
	wet_day: [dynamic]int `unit:"day"`, // Wet Days
	srftemp: f64 `unit:"degC"`, // Temperature of soil surface litter
	n_days: int `unit:"day"`, // Number of days ...
	st: [dynamic]f64 `unit:"degC"`, // Soil temperature in soil layer NL
	tma: [dynamic]f64 `unit:"degC"`, // Array of previous 5 days of average soil temperatures.
	tdl: f64 `unit:"cm"`, // Total water content of soil at drained upper limit
	x2_prev: f64 `unit:"degC"`, // Temperature of soil surface at precedent timestep
	dsmid: [dynamic]f64 `unit:"cm"`, // Depth to midpoint of soil layer NL
	cumdpt: f64 `unit:"mm"`, // Cumulative depth of soil profile
}

destroy_stemp_epic_state :: proc(x: ^Stemp_Epic_State) {
	delete(x.wet_day)
	delete(x.st)
	delete(x.tma)
	delete(x.dsmid)
}