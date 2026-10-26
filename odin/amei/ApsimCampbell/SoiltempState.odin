package apsimcampbell

Soiltemp_State :: struct {
	net_radiation: f64, // Net radiation per internal time-step [MJ]
	ave_soil_water: [dynamic]f64, // Average soil temperaturer [oC]
	bulk_density: [dynamic]f64, // Soil bulk density, includes phantom layer [g/cm3]
	internal_time_step: f64, // Internal time-step [sec]
	thermal_conductance: [dynamic]f64, // K, conductance of element between nodes [W/K]
	thickness: [dynamic]f64, // Thickness of each soil, includes phantom layer [mm]
	do_initialisation_stuff: bool, // Flag whether initialisation is needed [mintes]
	max_temp_yesterday: f64, // Yesterday's maximum daily air temperature [oC]
	time_of_day_secs: f64, // Time of day from midnight [sec]
	num_nodes: int, // Number of nodes over the soil profile
	soil_water: [dynamic]f64, // Volumetric water content of each soil layer [mm3/mm3]
	clay: [dynamic]f64, // Volumetric fraction of clay in each soil layer [%]
	soil_temp: [dynamic]f64, // Soil temperature over the soil profile at morning [oC]
	silt: [dynamic]f64, // Volumetric fraction of silt in each soil layer [%]
	instrument_height: f64, // Height of instruments above soil surface [mm]
	sand: [dynamic]f64, // Volumetric fraction of sand in each soil layer [%]
	num_layers: int, // Number of layers in the soil profile
	vol_spec_heat_soil: [dynamic]f64, // Volumetric specific heat over the soil profile [J/K/m3]
	instrum_height: f64, // Height of instruments above ground [mm]
	canopy_height: f64, // Height of canopy above ground [mm]
	heat_storage: [dynamic]f64, // CP, heat storage between nodes [J/K]
	min_soil_temp: [dynamic]f64, // Minimum soil temperature [oC]
	max_soil_temp: [dynamic]f64, // Maximum soil temperature [oC]
	new_temperature: [dynamic]f64, // Soil temperature at the end of this iteration [oC]
	air_temperature: f64, // Air temperature [oC]
	thermal_conductivity: [dynamic]f64, // Thermal conductivity [W.m/K]
	min_temp_yesterday: f64, // Yesterday's minimum daily air temperature [oC]
	carbon: [dynamic]f64, // Volumetric fraction of carbon (CHECK, OM?) in each soil layer [%]
	rocks: [dynamic]f64, // Volumetric fraction of rocks in each soil laye [%]
	initial_values: [dynamic]f64, // Initial soil temperature [oC]
	boundary_layer_conductance: f64, // Average daily atmosphere boundary layer conductance
	ave_soil_temp: [dynamic]f64, // average soil temperature [oC]
	morning_soil_temp: [dynamic]f64, // Soil temperature over the soil profile at morning [oC]
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