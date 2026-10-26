// Binding of the Crop2ML package DSSAT_EPICST_standalone
// (amei/DSSAT_EPICST_standalone/), DSSAT's EPIC soil temperature.
//
// After the C++ AMEI adapter (monica_amei, src/modules/soil_temperature/
// DSSAT_EPICST_standalone_monica_interface.cpp): the soil as for
// DSSAT_ST_standalone (dssat_st.odin, thicknesses rounded instead of
// truncated), SW set anew every day; SNOW is MONICA's snow depth, DEPIR the
// day's irrigation, BIOMAS the crop's above-ground biomass, MULCHMASS 0.
package soil_temperature

import epic "../../../amei/DSSAT_EPICST_standalone"

Dssat_Epicst :: struct {
	c:            epic.Stemp_Epic_Component,
	s, s1:        epic.Stemp_Epic_State, // no ModelUnit reads s1
	r:            epic.Stemp_Epic_Rate,
	a:            epic.Stemp_Epic_Auxiliary,
	ex:           epic.Stemp_Epic_Exogenous,
	no_of_layers: int,
}

make_dssat_epicst :: proc(allocator := context.allocator) -> Module {
	b := new(Dssat_Epicst, allocator)
	b.c = epic.make_stemp_epic_component(allocator)
	return b
}

destroy_dssat_epicst :: proc(b: ^Dssat_Epicst, allocator := context.allocator) {
	context.allocator = allocator
	epic.destroy_stemp_epic_component(&b.c)
	epic.destroy_stemp_epic_state(&b.s)
	epic.destroy_stemp_epic_state(&b.s1)
	epic.destroy_stemp_epic_rate(&b.r)
	epic.destroy_stemp_epic_auxiliary(&b.a)
	epic.destroy_stemp_epic_exogenous(&b.ex)
	free(b)
}

dssat_epicst_init :: proc(b: ^Dssat_Epicst, in_: ^Input, allocator := context.allocator) {
	n := len(in_.layer_thickness)
	b.no_of_layers = n
	c := &b.c
	epic.stemp_epic_component_set_iswwat(c, "Y")
	epic.stemp_epic_component_set_nl(c, n)
	epic.stemp_epic_component_set_nlayr(c, n)

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
	epic.stemp_epic_component_set_dlayr(c, dlayr, allocator)
	epic.stemp_epic_component_set_ds(c, ds, allocator)
	epic.stemp_epic_component_set_bd(c, bd, allocator)
	epic.stemp_epic_component_set_ll(c, in_.permanent_wilting_point, allocator)
	epic.stemp_epic_component_set_dul(c, in_.field_capacity, allocator)
	epic.stemp_epic_component_set_sw(c, in_.soil_moisture, allocator)

	dssat_epicst_exogenous(b, in_)
	epic.stemp_epic_component_init(c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
}

dssat_epicst_step :: proc(b: ^Dssat_Epicst, in_: ^Input, out: ^Output, allocator := context.allocator) {
	dssat_epicst_exogenous(b, in_)
	epic.stemp_epic_component_set_sw(&b.c, in_.soil_moisture, allocator)
	epic.stemp_epic_component_calculate_model(&b.c, &b.s, &b.s1, &b.r, &b.a, &b.ex, allocator)
	out.soil_surface_temperature = b.s.srftemp
	copy(out.layer_temperature, b.s.st[:b.no_of_layers])
}

@(private = "file")
dssat_epicst_exogenous :: proc(b: ^Dssat_Epicst, in_: ^Input) {
	b.ex.tmin = in_.tmin
	b.ex.tavg = in_.tavg
	b.ex.tmax = in_.tmax
	b.ex.rain = in_.precip
	b.ex.snow = in_.snow_depth
	b.ex.depir = in_.irrigation
	b.ex.mulchmass = 0
	b.ex.biomas = in_.above_ground_biomass
	b.ex.tav = in_.tav
	b.ex.tamp = in_.tamp
}
