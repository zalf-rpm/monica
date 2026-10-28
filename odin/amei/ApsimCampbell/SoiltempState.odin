package apsimcampbell

Soiltemp_State :: struct {
	net_radiation: f64 `unit:"MJ"`, // Net radiation per internal time-step
	ave_soil_water: [dynamic]f64 `unit:"oC"`, // Average soil temperaturer
	bulk_density: [dynamic]f64 `unit:"g/cm3"`, // Soil bulk density, includes phantom layer
	internal_time_step: f64 `unit:"sec"`, // Internal time-step
	thermal_conductance: [dynamic]f64 `unit:"W/K"`, // K, conductance of element between nodes
	thickness: [dynamic]f64 `unit:"mm"`, // Thickness of each soil, includes phantom layer
	do_initialisation_stuff: bool `unit:"mintes"`, // Flag whether initialisation is needed
	max_temp_yesterday: f64 `unit:"oC"`, // Yesterday's maximum daily air temperature
	time_of_day_secs: f64 `unit:"sec"`, // Time of day from midnight
	num_nodes: int `unit:""`, // Number of nodes over the soil profile
	soil_water: [dynamic]f64 `unit:"mm3/mm3"`, // Volumetric water content of each soil layer
	clay: [dynamic]f64 `unit:"%"`, // Volumetric fraction of clay in each soil layer
	soil_temp: [dynamic]f64 `unit:"oC"`, // Soil temperature over the soil profile at morning
	silt: [dynamic]f64 `unit:"%"`, // Volumetric fraction of silt in each soil layer
	instrument_height: f64 `unit:"mm"`, // Height of instruments above soil surface
	sand: [dynamic]f64 `unit:"%"`, // Volumetric fraction of sand in each soil layer
	num_layers: int `unit:""`, // Number of layers in the soil profile
	vol_spec_heat_soil: [dynamic]f64 `unit:"J/K/m3"`, // Volumetric specific heat over the soil profile
	instrum_height: f64 `unit:"mm"`, // Height of instruments above ground
	canopy_height: f64 `unit:"mm"`, // Height of canopy above ground
	heat_storage: [dynamic]f64 `unit:"J/K"`, // CP, heat storage between nodes
	min_soil_temp: [dynamic]f64 `unit:"oC"`, // Minimum soil temperature
	max_soil_temp: [dynamic]f64 `unit:"oC"`, // Maximum soil temperature
	new_temperature: [dynamic]f64 `unit:"oC"`, // Soil temperature at the end of this iteration
	air_temperature: f64 `unit:"oC"`, // Air temperature
	thermal_conductivity: [dynamic]f64 `unit:"W.m/K"`, // Thermal conductivity
	min_temp_yesterday: f64 `unit:"oC"`, // Yesterday's minimum daily air temperature
	carbon: [dynamic]f64 `unit:"%"`, // Volumetric fraction of carbon (CHECK, OM?) in each soil layer
	rocks: [dynamic]f64 `unit:"%"`, // Volumetric fraction of rocks in each soil laye
	initial_values: [dynamic]f64 `unit:"oC"`, // Initial soil temperature
	boundary_layer_conductance: f64 `unit:""`, // Average daily atmosphere boundary layer conductance
	ave_soil_temp: [dynamic]f64 `unit:"oC"`, // average soil temperature
	morning_soil_temp: [dynamic]f64 `unit:"oC"`, // Soil temperature over the soil profile at morning
}

destroy_soiltemp_state :: proc(x: ^Soiltemp_State) {
	delete(x.ave_soil_water)
	delete(x.bulk_density)
	delete(x.thermal_conductance)
	delete(x.thickness)
	delete(x.soil_water)
	delete(x.clay)
	delete(x.soil_temp)
	delete(x.silt)
	delete(x.sand)
	delete(x.vol_spec_heat_soil)
	delete(x.heat_storage)
	delete(x.min_soil_temp)
	delete(x.max_soil_temp)
	delete(x.new_temperature)
	delete(x.thermal_conductivity)
	delete(x.carbon)
	delete(x.rocks)
	delete(x.initial_values)
	delete(x.ave_soil_temp)
	delete(x.morning_soil_temp)
}