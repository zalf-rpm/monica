package biomasurfaceswatsoilswatc

// - Name: SurfaceSWATSoilSWATC -Version: 001, -Time step: 1
// - Description:
//             * Title: SurfaceSWATSoilSWATC model
//             * Authors: simone.bregaglio@unimi.it
//             * Reference: ('http://bioma.jrc.ec.europa.eu/ontology/JRC_MARS_biophysical_domain.owl',)
//             * Institution: Universiy Of Milan
//             * ExtendedDescription: Composite strategy for the calculation of surface and soil temperature with SWAT method. Neitsch,S.L., Arnold, J.G., Kiniry, J.R., Williams, J.R., King, K.W. Soil and Water Assessment Tool. Theoretical documentation. Version 2000. http://swatmodel.tamu.edu/media/1290/swat2000theory.pdf.  Composite strategy. See also references of the associated strategies.
//             * ShortDescription: None
Surface_Swat_Soil_Swatc_Component :: struct {
	surface_temperature_swat: Surface_Temperature_Swat,
	soil_temperature_swat: Soil_Temperature_Swat,
}

make_surface_swat_soil_swatc_component :: proc(allocator := context.allocator) -> (c: Surface_Swat_Soil_Swatc_Component) {
	context.allocator = allocator
	c.surface_temperature_swat = make_surface_temperature_swat()
	c.soil_temperature_swat = make_soil_temperature_swat()
	return
}

destroy_surface_swat_soil_swatc_component :: proc(c: ^Surface_Swat_Soil_Swatc_Component) {
	destroy_surface_temperature_swat(&c.surface_temperature_swat)
	destroy_soil_temperature_swat(&c.soil_temperature_swat)
}

surface_swat_soil_swatc_component_set_bulk_density :: proc(c: ^Surface_Swat_Soil_Swatc_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature_swat.bulk_density, len(value))
	copy(c.soil_temperature_swat.bulk_density[:], value)
}

surface_swat_soil_swatc_component_set_air_temperature_annual_average :: proc(c: ^Surface_Swat_Soil_Swatc_Component, value: f64) {
	c.soil_temperature_swat.air_temperature_annual_average = value
}

surface_swat_soil_swatc_component_set_soil_profile_depth :: proc(c: ^Surface_Swat_Soil_Swatc_Component, value: f64) {
	c.soil_temperature_swat.soil_profile_depth = value
}

surface_swat_soil_swatc_component_set_lag_coefficient :: proc(c: ^Surface_Swat_Soil_Swatc_Component, value: f64) {
	c.soil_temperature_swat.lag_coefficient = value
}

surface_swat_soil_swatc_component_set_layer_thickness :: proc(c: ^Surface_Swat_Soil_Swatc_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature_swat.layer_thickness, len(value))
	copy(c.soil_temperature_swat.layer_thickness[:], value)
}

surface_swat_soil_swatc_component_calculate_model :: proc(c: ^Surface_Swat_Soil_Swatc_Component, s: ^Surface_Swat_Soil_Swatc_State, s1: ^Surface_Swat_Soil_Swatc_State, r: ^Surface_Swat_Soil_Swatc_Rate, a: ^Surface_Swat_Soil_Swatc_Auxiliary, ex: ^Surface_Swat_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	surface_temperature_swat_calculate_model(&c.surface_temperature_swat, s, s1, r, a, ex)
	soil_temperature_swat_calculate_model(&c.soil_temperature_swat, s, s1, r, a, ex)
}

surface_swat_soil_swatc_component_init :: proc(c: ^Surface_Swat_Soil_Swatc_Component, s: ^Surface_Swat_Soil_Swatc_State, s1: ^Surface_Swat_Soil_Swatc_State, r: ^Surface_Swat_Soil_Swatc_Rate, a: ^Surface_Swat_Soil_Swatc_Auxiliary, ex: ^Surface_Swat_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	soil_temperature_swat_init(&c.soil_temperature_swat, s, s1, r, a, ex)
}