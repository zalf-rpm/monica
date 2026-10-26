package monica_evapotranspiration

// - Name: ET -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA evapotranspiration model
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the MONICA evapotranspiration
Et_Component :: struct {
	radiation: Radiation,
	net_radiation: Net_Radiation,
	stomata_resistance: Stomata_Resistance,
	calc_stomata_resistance: Calc_Stomata_Resistance,
	saturated_vapor_pressure: Saturated_Vapor_Pressure,
	saturation_vapor_pressure_deficit: Saturation_Vapor_Pressure_Deficit,
	reference_evapotranspiration: Reference_Evapotranspiration,
	potential_evapotranspiration: Potential_Evapotranspiration,
	evapotranspiration: Evapotranspiration,
	use_ext_et0: Use_Ext_Et0,
	use_ext_vapor_pressure: Use_Ext_Vapor_Pressure,
}

make_et_component :: proc(allocator := context.allocator) -> (c: Et_Component) {
	context.allocator = allocator
	c.radiation = make_radiation()
	c.net_radiation = make_net_radiation()
	c.stomata_resistance = make_stomata_resistance()
	c.calc_stomata_resistance = make_calc_stomata_resistance()
	c.saturated_vapor_pressure = make_saturated_vapor_pressure()
	c.saturation_vapor_pressure_deficit = make_saturation_vapor_pressure_deficit()
	c.reference_evapotranspiration = make_reference_evapotranspiration()
	c.potential_evapotranspiration = make_potential_evapotranspiration()
	c.evapotranspiration = make_evapotranspiration()
	c.use_ext_et0 = make_use_ext_et0()
	c.use_ext_vapor_pressure = make_use_ext_vapor_pressure()
	return
}

destroy_et_component :: proc(c: ^Et_Component) {
	destroy_radiation(&c.radiation)
	destroy_net_radiation(&c.net_radiation)
	destroy_stomata_resistance(&c.stomata_resistance)
	destroy_calc_stomata_resistance(&c.calc_stomata_resistance)
	destroy_saturated_vapor_pressure(&c.saturated_vapor_pressure)
	destroy_saturation_vapor_pressure_deficit(&c.saturation_vapor_pressure_deficit)
	destroy_reference_evapotranspiration(&c.reference_evapotranspiration)
	destroy_potential_evapotranspiration(&c.potential_evapotranspiration)
	destroy_evapotranspiration(&c.evapotranspiration)
	destroy_use_ext_et0(&c.use_ext_et0)
	destroy_use_ext_vapor_pressure(&c.use_ext_vapor_pressure)
}

et_component_set_latitude :: proc(c: ^Et_Component, value: f64) {
	c.radiation.latitude = value
}

et_component_set_reference_albedo :: proc(c: ^Et_Component, value: f64) {
	c.net_radiation.reference_albedo = value
}

et_component_set_height_nn :: proc(c: ^Et_Component, value: f64) {
	c.net_radiation.height_nn = value
	c.reference_evapotranspiration.height_nn = value
}

et_component_set_saturation_beta :: proc(c: ^Et_Component, value: f64) {
	c.stomata_resistance.saturation_beta = value
}

et_component_set_stomata_conductance_alpha :: proc(c: ^Et_Component, value: f64) {
	c.stomata_resistance.stomata_conductance_alpha = value
}

et_component_set_carboxylation_pathway :: proc(c: ^Et_Component, value: int) {
	c.stomata_resistance.carboxylation_pathway = value
}

et_component_set_evaporation_zeta :: proc(c: ^Et_Component, value: f64) {
	c.evapotranspiration.evaporation_zeta = value
}

et_component_set_maximum_evaporation_impact_depth :: proc(c: ^Et_Component, value: f64) {
	c.evapotranspiration.maximum_evaporation_impact_depth = value
}

et_component_set_evaporation_reduction_method :: proc(c: ^Et_Component, value: int) {
	c.evapotranspiration.evaporation_reduction_method = value
}

et_component_set_xsa_critical_soil_moisture :: proc(c: ^Et_Component, value: f64) {
	c.evapotranspiration.xsa_critical_soil_moisture = value
}

et_component_set_no_of_soil_layers :: proc(c: ^Et_Component, value: int) {
	c.evapotranspiration.no_of_soil_layers = value
}

et_component_set_no_of_soil_moisture_layers :: proc(c: ^Et_Component, value: int) {
	c.evapotranspiration.no_of_soil_moisture_layers = value
}

et_component_set_layer_thickness :: proc(c: ^Et_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.evapotranspiration.layer_thickness, len(value))
	copy(c.evapotranspiration.layer_thickness[:], value)
}

et_component_set_permanent_wilting_point :: proc(c: ^Et_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.evapotranspiration.permanent_wilting_point, len(value))
	copy(c.evapotranspiration.permanent_wilting_point[:], value)
}

et_component_set_field_capacity :: proc(c: ^Et_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.evapotranspiration.field_capacity, len(value))
	copy(c.evapotranspiration.field_capacity[:], value)
}

et_component_calculate_model :: proc(c: ^Et_Component, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	radiation_calculate_model(&c.radiation, s, s1, r, a, ex)
	saturated_vapor_pressure_calculate_model(&c.saturated_vapor_pressure, s, s1, r, a, ex)
	a.internal_vapor_pressure = a.vapor_pressure
	use_ext_vapor_pressure_calculate_model(&c.use_ext_vapor_pressure, s, s1, r, a, ex)
	net_radiation_calculate_model(&c.net_radiation, s, s1, r, a, ex)
	saturation_vapor_pressure_deficit_calculate_model(&c.saturation_vapor_pressure_deficit, s, s1, r, a, ex)
	stomata_resistance_calculate_model(&c.stomata_resistance, s, s1, r, a, ex)
	a.calculated_stomata_resistance = a.stomata_resistance
	calc_stomata_resistance_calculate_model(&c.calc_stomata_resistance, s, s1, r, a, ex)
	reference_evapotranspiration_calculate_model(&c.reference_evapotranspiration, s, s1, r, a, ex)
	a.internal_et0 = a.reference_evapotranspiration
	use_ext_et0_calculate_model(&c.use_ext_et0, s, s1, r, a, ex)
	a.reference_evapotranspiration = a.et0
	potential_evapotranspiration_calculate_model(&c.potential_evapotranspiration, s, s1, r, a, ex)
	evapotranspiration_calculate_model(&c.evapotranspiration, s, s1, r, a, ex)
}

et_component_init :: proc(c: ^Et_Component, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	evapotranspiration_init(&c.evapotranspiration, s, s1, r, a, ex)
}