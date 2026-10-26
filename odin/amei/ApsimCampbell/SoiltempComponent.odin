package apsimcampbell

// - Name: Soiltemp -Version: 2.0, -Time step: 1.0
// - Description:
//             * Title: Soiltemp model
//             * Authors: Cyrille
//             * Reference: None
//             * Institution: INRAE
//             * ExtendedDescription: Soil Temperature
//             * ShortDescription: Soil Temperature
Soiltemp_Component :: struct {
	soil_temperature: Soil_Temperature,
}

make_soiltemp_component :: proc(allocator := context.allocator) -> (c: Soiltemp_Component) {
	context.allocator = allocator
	c.soil_temperature = make_soil_temperature()
	return
}

destroy_soiltemp_component :: proc(c: ^Soiltemp_Component) {
	destroy_soil_temperature(&c.soil_temperature)
}

soiltemp_component_set_therm_cond_par1 :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.therm_cond_par1, len(value))
	copy(c.soil_temperature.therm_cond_par1[:], value)
}

soiltemp_component_set_topsoil_node :: proc(c: ^Soiltemp_Component, value: int) {
	c.soil_temperature.topsoil_node = value
}

soiltemp_component_set_surface_node :: proc(c: ^Soiltemp_Component, value: int) {
	c.soil_temperature.surface_node = value
}

soiltemp_component_set_num_phantom_nodes :: proc(c: ^Soiltemp_Component, value: int) {
	c.soil_temperature.num_phantom_nodes = value
}

soiltemp_component_set_soil_constituent_names :: proc(c: ^Soiltemp_Component, value: []string, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.soil_constituent_names, len(value))
	copy(c.soil_temperature.soil_constituent_names[:], value)
}

soiltemp_component_set_physical_thickness :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.physical_thickness, len(value))
	copy(c.soil_temperature.physical_thickness[:], value)
}

soiltemp_component_set_missing_value :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.missing_value = value
}

soiltemp_component_set_timestep :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.timestep = value
}

soiltemp_component_set_soil_roughness_height :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.soil_roughness_height = value
}

soiltemp_component_set_num_iterations_for_boundary_layer_conductance :: proc(c: ^Soiltemp_Component, value: int) {
	c.soil_temperature.num_iterations_for_boundary_layer_conductance = value
}

soiltemp_component_set_default_time_of_maximum_temperature :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.default_time_of_maximum_temperature = value
}

soiltemp_component_set_pom :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.pom = value
}

soiltemp_component_set_depth_to_constant_temperature :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.depth_to_constant_temperature = value
}

soiltemp_component_set_constant_boundary_layer_conductance :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.constant_boundary_layer_conductance = value
}

soiltemp_component_set_therm_cond_par4 :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.therm_cond_par4, len(value))
	copy(c.soil_temperature.therm_cond_par4[:], value)
}

soiltemp_component_set_node_depth :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.node_depth, len(value))
	copy(c.soil_temperature.node_depth[:], value)
}

soiltemp_component_set_nu :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.nu = value
}

soiltemp_component_set_p_initial_values :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.p_initial_values, len(value))
	copy(c.soil_temperature.p_initial_values[:], value)
}

soiltemp_component_set_ps :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.ps = value
}

soiltemp_component_set_net_radiation_source :: proc(c: ^Soiltemp_Component, value: string) {
	c.soil_temperature.net_radiation_source = value
}

soiltemp_component_set_air_node :: proc(c: ^Soiltemp_Component, value: int) {
	c.soil_temperature.air_node = value
}

soiltemp_component_set_bare_soil_roughness :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.bare_soil_roughness = value
}

soiltemp_component_set_therm_cond_par2 :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.therm_cond_par2, len(value))
	copy(c.soil_temperature.therm_cond_par2[:], value)
}

soiltemp_component_set_default_instrument_height :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.default_instrument_height = value
}

soiltemp_component_set_physical_bd :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.physical_bd, len(value))
	copy(c.soil_temperature.physical_bd[:], value)
}

soiltemp_component_set_latent_heat_of_vapourisation :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.latent_heat_of_vapourisation = value
}

soiltemp_component_set_weather_latitude :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.weather_latitude = value
}

soiltemp_component_set_stefan_boltzmann_constant :: proc(c: ^Soiltemp_Component, value: f64) {
	c.soil_temperature.stefan_boltzmann_constant = value
}

soiltemp_component_set_boundar_layer_conductance_source :: proc(c: ^Soiltemp_Component, value: string) {
	c.soil_temperature.boundar_layer_conductance_source = value
}

soiltemp_component_set_therm_cond_par3 :: proc(c: ^Soiltemp_Component, value: []f64, allocator := context.allocator) {
	context.allocator = allocator
	resize(&c.soil_temperature.therm_cond_par3, len(value))
	copy(c.soil_temperature.therm_cond_par3[:], value)
}

soiltemp_component_calculate_model :: proc(c: ^Soiltemp_Component, s: ^Soiltemp_State, s1: ^Soiltemp_State, r: ^Soiltemp_Rate, a: ^Soiltemp_Auxiliary, ex: ^Soiltemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	soil_temperature_calculate_model(&c.soil_temperature, s, s1, r, a, ex)
}

soiltemp_component_init :: proc(c: ^Soiltemp_Component, s: ^Soiltemp_State, s1: ^Soiltemp_State, r: ^Soiltemp_Rate, a: ^Soiltemp_Auxiliary, ex: ^Soiltemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	soil_temperature_init(&c.soil_temperature, s, s1, r, a, ex)
}