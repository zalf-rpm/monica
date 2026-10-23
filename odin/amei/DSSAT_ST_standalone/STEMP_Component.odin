package dssat_st_standalone

// - Name: STEMP_ -Version:  1.0, -Time step:  1
// - Description:
//             * Title: STEMP_ model
//             * Authors: DSSAT
//             * Reference: https://doi.org/10.2134/agronj1994.00021962008600060014x
//             * Institution: DSSAT Florida
//             * ExtendedDescription: None
//             * ShortDescription: Determines soil temperature by layer
Stemp_Component :: struct {
	stemp: Stemp,
}

make_stemp_component :: proc(allocator := context.allocator) -> (c: Stemp_Component) {
	context.allocator = allocator
	c.stemp = make_stemp()
	return
}

destroy_stemp_component :: proc(c: ^Stemp_Component) {
	destroy_stemp(&c.stemp)
}

stemp_component_set_msalb :: proc(c: ^Stemp_Component, value: f64) {
	c.stemp.msalb = value
}

stemp_component_set_nl :: proc(c: ^Stemp_Component, value: int) {
	c.stemp.nl = value
}

stemp_component_set_ll :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.ll, len(value))
	copy(c.stemp.ll[:], value)
}

stemp_component_set_nlayr :: proc(c: ^Stemp_Component, value: int) {
	c.stemp.nlayr = value
}

stemp_component_set_ds :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.ds, len(value))
	copy(c.stemp.ds[:], value)
}

stemp_component_set_dlayr :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.dlayr, len(value))
	copy(c.stemp.dlayr[:], value)
}

stemp_component_set_iswwat :: proc(c: ^Stemp_Component, value: string) {
	c.stemp.iswwat = value
}

stemp_component_set_bd :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.bd, len(value))
	copy(c.stemp.bd[:], value)
}

stemp_component_set_sw :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.sw, len(value))
	copy(c.stemp.sw[:], value)
}

stemp_component_set_xlat :: proc(c: ^Stemp_Component, value: f64) {
	c.stemp.xlat = value
}

stemp_component_set_dul :: proc(c: ^Stemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp.dul, len(value))
	copy(c.stemp.dul[:], value)
}

stemp_component_calculate_model :: proc(c: ^Stemp_Component, s: ^Stemp_State, s1: ^Stemp_State, r: ^Stemp_Rate, a: ^Stemp_Auxiliary, ex: ^Stemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	stemp_calculate_model(&c.stemp, s, s1, r, a, ex)
}

stemp_component_init :: proc(c: ^Stemp_Component, s: ^Stemp_State, s1: ^Stemp_State, r: ^Stemp_Rate, a: ^Stemp_Auxiliary, ex: ^Stemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	stemp_init(&c.stemp, s, s1, r, a, ex)
}