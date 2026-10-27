// Binding of the Crop2ML package DSSAT_ST_standalone (amei/DSSAT_ST_standalone/),
// DSSAT's STEMP.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// DSSAT_ST_standalone_monica_interface.cpp): one DSSAT layer per MONICA layer,
// bulk density kg m-3 -> g cm-3, thicknesses and depths m -> cm, the soil
// moisture (SW, a parameter of the module) set anew every day, MONICA's
// environment albedo as the soil albedo. One difference: the adapter
// truncated the thicknesses to whole cm; they are rounded here (thickness_cm).
package soil_temperature

import dst "../../../amei/DSSAT_ST_standalone"

Dssat_St :: struct {
	c:            dst.Stemp_Component,
	s, s1:        dst.Stemp_State, // no ModelUnit reads s1
	r:            dst.Stemp_Rate,
	a:            dst.Stemp_Auxiliary,
	ex:           dst.Stemp_Exogenous,
	no_of_layers: int,
}

make_dssat_st :: proc(allocator: Allocator) -> Module {
	b := new(Dssat_St, allocator)
	b.c = dst.make_stemp_component(allocator)
	return b
}

destroy_dssat_st :: proc(b: ^Dssat_St, allocator := context.allocator) {
	context.allocator = allocator
	dst.destroy_stemp_component(&b.c)
	dst.destroy_stemp_state(&b.s)
	dst.destroy_stemp_state(&b.s1)
	dst.destroy_stemp_rate(&b.r)
	dst.destroy_stemp_auxiliary(&b.a)
	dst.destroy_stemp_exogenous(&b.ex)
	free(b)
}

dssat_st_init :: proc(b: ^Dssat_St, in_: ^Input, allocator := context.allocator) {
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	dst.stemp_component_set_iswwat(c, "Y")
	dst.stemp_component_set_nl(c, n)
	dst.stemp_component_set_nlayr(c, n)
	dst.stemp_component_set_xlat(c, in_.latitude)
	dst.stemp_component_set_msalb(c, in_.albedo)

	dlayr := make([]f64, n, context.temp_allocator)
	ds := make([]f64, n, context.temp_allocator)
	bd := make([]f64, n, context.temp_allocator)
	depth := 0.0
	for i in 0 ..< n {
		dlayr[i] = thickness_cm(in_.layer_thickness[i])
		depth += dlayr[i]
		ds[i] = depth
		bd[i] = in_.bulk_density[i] / 1000.0 // kg m-3 -> g cm-3
	}
	dst.stemp_component_set_dlayr(c, dlayr, allocator)
	dst.stemp_component_set_ds(c, ds, allocator)
	dst.stemp_component_set_bd(c, bd, allocator)
	dst.stemp_component_set_ll(c, in_.permanent_wilting_point, allocator)
	dst.stemp_component_set_dul(c, in_.field_capacity, allocator)
	dst.stemp_component_set_sw(c, in_.soil_moisture, allocator)

	dssat_st_exogenous(b, in_)
	dst.stemp_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

dssat_st_step :: proc(b: ^Dssat_St, in_: ^Input, out: ^Output, allocator := context.allocator) {
	dssat_st_exogenous(b, in_)
	// same length every day, so this doesn't allocate
	dst.stemp_component_set_sw(&b.c, in_.soil_moisture, allocator)
	dst.stemp_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.srftemp
	copy(out.layer_temperature, b.s.st[:b.no_of_layers])
}

@(private = "file")
dssat_st_exogenous :: proc(b: ^Dssat_St, in_: ^Input) {
	b.ex.doy = in_.julian_day
	b.ex.srad = in_.globrad
	b.ex.tavg = in_.tavg
	b.ex.tmax = in_.tmax
	b.ex.tav = in_.tav
	b.ex.tamp = in_.tamp
}

// The table row (soil_temperature.odin's `modules`)
dssat_st_entry :: Module_Entry {
	name    = "DSSAT_ST_standalone",
	make    = make_dssat_st,
	init    = proc(m: Module, in_: ^Input, a: Allocator) {dssat_st_init(m.(^Dssat_St), in_, a)},
	step    = proc(m: Module, in_: ^Input, out: ^Output, a: Allocator) {dssat_st_step(m.(^Dssat_St), in_, out, a)},
	destroy = proc(m: Module, a: Allocator) {destroy_dssat_st(m.(^Dssat_St), a)},
}
