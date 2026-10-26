package dssat_epicst_standalone

// - Name: STEMP_EPIC_ -Version:  1.0, -Time step:  1
// - Description:
//             * Title: STEMP_EPIC_ model
//             * Authors: Kenneth N. Potter , Jimmy R. Williams
//             * Reference: https://doi.org/10.2134/agronj1994.00021962008600060014x
//             * Institution: USDA-ARS, USDA-ARS
//             * ExtendedDescription: None
//             * ShortDescription: Determines soil temperature by layer test encore
Stemp_Epic_Component :: struct {
	stemp_epic: Stemp_Epic,
}

make_stemp_epic_component :: proc(allocator := context.allocator) -> (c: Stemp_Epic_Component) {
	context.allocator = allocator
	c.stemp_epic = make_stemp_epic()
	return
}

destroy_stemp_epic_component :: proc(c: ^Stemp_Epic_Component) {
	destroy_stemp_epic(&c.stemp_epic)
}

stemp_epic_component_set_dul :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.dul, len(value))
	copy(c.stemp_epic.dul[:], value)
}

stemp_epic_component_set_nl :: proc(c: ^Stemp_Epic_Component, value: int) {
	c.stemp_epic.nl = value
}

stemp_epic_component_set_nlayr :: proc(c: ^Stemp_Epic_Component, value: int) {
	c.stemp_epic.nlayr = value
}

stemp_epic_component_set_ds :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.ds, len(value))
	copy(c.stemp_epic.ds[:], value)
}

stemp_epic_component_set_iswwat :: proc(c: ^Stemp_Epic_Component, value: string) {
	c.stemp_epic.iswwat = value
}

stemp_epic_component_set_bd :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.bd, len(value))
	copy(c.stemp_epic.bd[:], value)
}

stemp_epic_component_set_ll :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.ll, len(value))
	copy(c.stemp_epic.ll[:], value)
}

stemp_epic_component_set_dlayr :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.dlayr, len(value))
	copy(c.stemp_epic.dlayr[:], value)
}

stemp_epic_component_set_sw :: proc(c: ^Stemp_Epic_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.stemp_epic.sw, len(value))
	copy(c.stemp_epic.sw[:], value)
}

stemp_epic_component_calculate_model :: proc(c: ^Stemp_Epic_Component, s: ^Stemp_Epic_State, s1: ^Stemp_Epic_State, r: ^Stemp_Epic_Rate, a: ^Stemp_Epic_Auxiliary, ex: ^Stemp_Epic_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	stemp_epic_calculate_model(&c.stemp_epic, s, s1, r, a, ex)
}

stemp_epic_component_init :: proc(c: ^Stemp_Epic_Component, s: ^Stemp_Epic_State, s1: ^Stemp_Epic_State, r: ^Stemp_Epic_Rate, a: ^Stemp_Epic_Auxiliary, ex: ^Stemp_Epic_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	stemp_epic_init(&c.stemp_epic, s, s1, r, a, ex)
}