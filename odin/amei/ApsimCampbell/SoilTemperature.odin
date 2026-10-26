package apsimcampbell

import "core:math"
import "core:slice"
import cr_math "../../support/cr_math"

// - Name: SoilTemperature -Version:  1.0, -Time step:  1
// - Description:
//             * Title: SoilTemperature
//             * Authors: APSIM
//             * Reference: None
//             * Institution: APSIM Initiative
//             * ExtendedDescription:  Soil Temperature
//             * ShortDescription: Heat flux and temperatures over the surface and soil profile (based on Campbell, 1985)
Soil_Temperature :: struct {
	weather_latitude: f64, // Latitude [deg]
	physical_thickness: [dynamic]f64, // Soil layer thickness [mm]
	physical_bd: [dynamic]f64, // Bulk density [g/cc]
	ps: f64, // ps
	p_initial_values: [dynamic]f64, // Initial soil temperature [oC]
	depth_to_constant_temperature: f64, // Soil depth to constant temperature [mm]
	timestep: f64, // Internal time-step (minutes) [minutes]
	latent_heat_of_vapourisation: f64, // Latent heat of vapourisation for water [miJ/kg]
	stefan_boltzmann_constant: f64, // The Stefan-Boltzmann constant [W/m2/K4]
	air_node: int, // The index of the node in the atmosphere (aboveground)
	surface_node: int, // The index of the node on the soil surface (depth = 0)
	topsoil_node: int, // The index of the first node within the soil (top layer)
	num_phantom_nodes: int, // The number of phantom nodes below the soil profile
	constant_boundary_layer_conductance: f64, // Boundary layer conductance, if constant [K/W]
	num_iterations_for_boundary_layer_conductance: int, // Number of iterations to calculate atmosphere boundary layer conductance
	default_time_of_maximum_temperature: f64, // Time of maximum temperature [minutes]
	default_instrument_height: f64, // Default instrument height [m]
	bare_soil_roughness: f64, // Roughness element height of bare soil [mm]
	node_depth: [dynamic]f64, // Depths of nodes [mm]
	therm_cond_par1: [dynamic]f64, // Parameter 1 for computing thermal conductivity of soil solids
	therm_cond_par2: [dynamic]f64, // Parameter 2 for computing thermal conductivity of soil solids
	therm_cond_par3: [dynamic]f64, // Parameter 3 for computing thermal conductivity of soil solids
	therm_cond_par4: [dynamic]f64, // Parameter 4 for computing thermal conductivity of soil solids
	pom: f64, // Particle density of organic matter [Mg/m3]
	soil_roughness_height: f64, // Height of soil roughness [mm]
	nu: f64, // Forward/backward differencing coefficient [0-1]
	boundar_layer_conductance_source: string, // Flag whether boundary layer conductance is calculated or gotten from inpu
	net_radiation_source: string, // Flag whether net radiation is calculated or gotten from input [m]
	missing_value: f64, // missing value [m]
	soil_constituent_names: [dynamic]string, // soilConstituentNames [m]
}

make_soil_temperature :: proc(allocator := context.allocator) -> (m: Soil_Temperature) {
	context.allocator = allocator
	m.depth_to_constant_temperature = 10000
	m.timestep = 24.0 * 60.0 * 60.0
	m.latent_heat_of_vapourisation = 2465000
	m.stefan_boltzmann_constant = 0.0000000567
	m.air_node = 0
	m.surface_node = 1
	m.topsoil_node = 2
	m.num_phantom_nodes = 5
	m.constant_boundary_layer_conductance = 20
	m.num_iterations_for_boundary_layer_conductance = 1
	m.default_time_of_maximum_temperature = 14.0
	m.default_instrument_height = 1.2
	m.bare_soil_roughness = 57
	m.soil_roughness_height = 0
	m.nu = 0.6
	m.boundar_layer_conductance_source = "calc"
	m.net_radiation_source = "calc"
	m.missing_value = 99999
	return
}

destroy_soil_temperature :: proc(m: ^Soil_Temperature) {
	delete(m.physical_thickness)
	delete(m.physical_bd)
	delete(m.p_initial_values)
	delete(m.node_depth)
	delete(m.therm_cond_par1)
	delete(m.therm_cond_par2)
	delete(m.therm_cond_par3)
	delete(m.therm_cond_par4)
	delete(m.soil_constituent_names)
}

soil_temperature_init :: proc(m: ^Soil_Temperature, s: ^Soiltemp_State, s1: ^Soiltemp_State, r: ^Soiltemp_Rate, a: ^Soiltemp_Auxiliary, ex: ^Soiltemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.do_initialisation_stuff = false
	s.internal_time_step = 0.0
	s.time_of_day_secs = 0.0
	s.num_nodes = 0
	s.num_layers = 0
	s.boundary_layer_conductance = 0.0
	s.air_temperature = 0.0
	s.max_temp_yesterday = 0.0
	s.min_temp_yesterday = 0.0
	s.instrument_height = 0.0
	s.net_radiation = 0.0
	s.canopy_height = 0.0
	s.instrum_height = 0.0
	clear(&s.initial_values)
	clear(&s.vol_spec_heat_soil)
	clear(&s.soil_temp)
	clear(&s.morning_soil_temp)
	clear(&s.heat_storage)
	clear(&s.thermal_conductance)
	clear(&s.thermal_conductivity)
	clear(&s.new_temperature)
	clear(&s.soil_water)
	clear(&s.min_soil_temp)
	clear(&s.max_soil_temp)
	clear(&s.ave_soil_temp)
	clear(&s.ave_soil_water)
	clear(&s.thickness)
	clear(&s.bulk_density)
	clear(&s.rocks)
	clear(&s.carbon)
	clear(&s.sand)
	clear(&s.silt)
	clear(&s.clay)
	s.do_initialisation_stuff = true
	s.instrument_height = get_ini_variables(s.instrument_height, s.instrum_height, m.default_instrument_height)
	{
		_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _r10, _r11, _r12, _r13, _r14, _r15, _r16, _r17, _r18, _r19, _r20 := get_profile_variables(s.heat_storage[:], s.min_soil_temp[:], s.bulk_density[:], s.num_nodes, m.physical_bd[:], s.max_soil_temp[:], ex.water_balance_sw[:], ex.organic_carbon[:], ex.physical_rocks[:], m.node_depth[:], m.topsoil_node, s.new_temperature[:], m.surface_node, s.soil_water[:], s.thermal_conductance[:], s.thermal_conductivity[:], s.sand[:], s.carbon[:], s.thickness[:], m.num_phantom_nodes, ex.physical_particle_size_sand[:], s.rocks[:], s.clay[:], ex.physical_particle_size_silt[:], m.air_node, ex.physical_particle_size_clay[:], s.soil_temp[:], s.num_layers, m.physical_thickness[:], s.silt[:], s.vol_spec_heat_soil[:], s.ave_soil_temp[:], s.morning_soil_temp[:], m.depth_to_constant_temperature, m.missing_value)
		resize(&s.heat_storage, len(_r0))
		copy(s.heat_storage[:], _r0)
		resize(&s.min_soil_temp, len(_r1))
		copy(s.min_soil_temp[:], _r1)
		resize(&s.bulk_density, len(_r2))
		copy(s.bulk_density[:], _r2)
		resize(&s.max_soil_temp, len(_r3))
		copy(s.max_soil_temp[:], _r3)
		resize(&m.node_depth, len(_r4))
		copy(m.node_depth[:], _r4)
		resize(&s.new_temperature, len(_r5))
		copy(s.new_temperature[:], _r5)
		resize(&s.soil_water, len(_r6))
		copy(s.soil_water[:], _r6)
		resize(&s.thermal_conductance, len(_r7))
		copy(s.thermal_conductance[:], _r7)
		resize(&s.thermal_conductivity, len(_r8))
		copy(s.thermal_conductivity[:], _r8)
		resize(&s.sand, len(_r9))
		copy(s.sand[:], _r9)
		resize(&s.carbon, len(_r10))
		copy(s.carbon[:], _r10)
		resize(&s.thickness, len(_r11))
		copy(s.thickness[:], _r11)
		resize(&s.rocks, len(_r12))
		copy(s.rocks[:], _r12)
		resize(&s.clay, len(_r13))
		copy(s.clay[:], _r13)
		resize(&s.soil_temp, len(_r14))
		copy(s.soil_temp[:], _r14)
		resize(&s.silt, len(_r15))
		copy(s.silt[:], _r15)
		resize(&s.vol_spec_heat_soil, len(_r16))
		copy(s.vol_spec_heat_soil[:], _r16)
		resize(&s.ave_soil_temp, len(_r17))
		copy(s.ave_soil_temp[:], _r17)
		resize(&s.morning_soil_temp, len(_r18))
		copy(s.morning_soil_temp[:], _r18)
		s.num_nodes = _r19
		s.num_layers = _r20
		delete(_r0)
		delete(_r1)
		delete(_r2)
		delete(_r3)
		delete(_r4)
		delete(_r5)
		delete(_r6)
		delete(_r7)
		delete(_r8)
		delete(_r9)
		delete(_r10)
		delete(_r11)
		delete(_r12)
		delete(_r13)
		delete(_r14)
		delete(_r15)
		delete(_r16)
		delete(_r17)
		delete(_r18)
	}
	{
		_r0, _r1, _r2, _r3, _r4, _r5, _r6 := read_param(m.bare_soil_roughness, s.new_temperature[:], m.soil_roughness_height, s.soil_temp[:], m.therm_cond_par2[:], s.num_layers, s.bulk_density[:], s.num_nodes, m.therm_cond_par3[:], m.therm_cond_par4[:], s.clay[:], m.therm_cond_par1[:], ex.weather_tav, ex.clock_today_day_of_year, m.surface_node, ex.weather_amp, s.thickness[:], m.weather_latitude)
		resize(&s.new_temperature, len(_r0))
		copy(s.new_temperature[:], _r0)
		resize(&s.soil_temp, len(_r1))
		copy(s.soil_temp[:], _r1)
		resize(&m.therm_cond_par2, len(_r2))
		copy(m.therm_cond_par2[:], _r2)
		resize(&m.therm_cond_par3, len(_r3))
		copy(m.therm_cond_par3[:], _r3)
		resize(&m.therm_cond_par4, len(_r4))
		copy(m.therm_cond_par4[:], _r4)
		resize(&m.therm_cond_par1, len(_r5))
		copy(m.therm_cond_par1[:], _r5)
		m.soil_roughness_height = _r6
		delete(_r1)
		delete(_r2)
		delete(_r3)
		delete(_r4)
		delete(_r5)
	}
	resize(&s.initial_values, len(m.p_initial_values[:]))
	copy(s.initial_values[:], m.p_initial_values[:])
}

// - inputs:
//             * name: weather_MinT
//                           ** description : Minimum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: weather_MaxT
//                           ** description : Maximum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: weather_MeanT
//                           ** description : Mean temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: weather_Tav
//                           ** description : Annual average temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: weather_Amp
//                           ** description : Annual average temperature amplitude
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: weather_AirPressure
//                           ** description : Air pressure
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : hPa
//             * name: weather_Wind
//                           ** description : Wind speed
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m/s
//             * name: weather_Latitude
//                           ** description : Latitude
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : deg
//             * name: weather_Radn
//                           ** description : Solar radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : MJ/m2/day
//             * name: clock_Today_DayOfYear
//                           ** description : Day of year
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : day number
//             * name: microClimate_CanopyHeight
//                           ** description : Canopy height
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: physical_Thickness
//                           ** description : Soil layer thickness
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: physical_BD
//                           ** description : Bulk density
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : g/cc
//             * name: ps
//                           ** description : ps
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit :
//             * name: physical_Rocks
//                           ** description : Rocks
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: physical_ParticleSizeSand
//                           ** description : Particle size sand
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: physical_ParticleSizeSilt
//                           ** description : Particle size silt
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: physical_ParticleSizeClay
//                           ** description : Particle size clay
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: organic_Carbon
//                           ** description : Total organic carbon
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: waterBalance_SW
//                           ** description : Volumetric water content
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm/mm
//             * name: waterBalance_Eos
//                           ** description : Potential soil evaporation from soil surface
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: waterBalance_Eo
//                           ** description : Potential soil evapotranspiration from soil surface
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: waterBalance_Es
//                           ** description : Actual (realised) soil water evaporation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: waterBalance_Salb
//                           ** description : Fraction of incoming radiation reflected from bare soil
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : 0-1
//             * name: InitialValues
//                           ** description : Initial soil temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: pInitialValues
//                           ** description : Initial soil temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: DepthToConstantTemperature
//                           ** description : Soil depth to constant temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 10000
//                           ** unit : mm
//             * name: timestep
//                           ** description : Internal time-step (minutes)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 24.0 * 60.0 * 60.0
//                           ** unit : minutes
//             * name: latentHeatOfVapourisation
//                           ** description : Latent heat of vapourisation for water
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 2465000
//                           ** unit : miJ/kg
//             * name: stefanBoltzmannConstant
//                           ** description : The Stefan-Boltzmann constant
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.0000000567
//                           ** unit : W/m2/K4
//             * name: airNode
//                           ** description : The index of the node in the atmosphere (aboveground)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit :
//             * name: surfaceNode
//                           ** description : The index of the node on the soil surface (depth = 0)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 1
//                           ** unit :
//             * name: topsoilNode
//                           ** description : The index of the first node within the soil (top layer)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 2
//                           ** unit :
//             * name: numPhantomNodes
//                           ** description : The number of phantom nodes below the soil profile
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 5
//                           ** unit :
//             * name: constantBoundaryLayerConductance
//                           ** description : Boundary layer conductance, if constant
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 20
//                           ** unit : K/W
//             * name: numIterationsForBoundaryLayerConductance
//                           ** description : Number of iterations to calculate atmosphere boundary layer conductance
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** min :
//                           ** default : 1
//                           ** unit :
//             * name: defaultTimeOfMaximumTemperature
//                           ** description : Time of maximum temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 14.0
//                           ** unit : minutes
//             * name: defaultInstrumentHeight
//                           ** description : Default instrument height
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1.2
//                           ** unit : m
//             * name: bareSoilRoughness
//                           ** description : Roughness element height of bare soil
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 57
//                           ** unit : mm
//             * name: doInitialisationStuff
//                           ** description : Flag whether initialisation is needed
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : BOOLEAN
//                           ** max :
//                           ** min :
//                           ** default : false
//                           ** unit : mintes
//             * name: internalTimeStep
//                           ** description : Internal time-step
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : sec
//             * name: timeOfDaySecs
//                           ** description : Time of day from midnight
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : sec
//             * name: numNodes
//                           ** description : Number of nodes over the soil profile
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit :
//             * name: numLayers
//                           ** description : Number of layers in the soil profile
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit :
//             * name: nodeDepth
//                           ** description : Depths of nodes
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: thermCondPar1
//                           ** description : Parameter 1 for computing thermal conductivity of soil solids
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit :
//             * name: thermCondPar2
//                           ** description : Parameter 2 for computing thermal conductivity of soil solids
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit :
//             * name: thermCondPar3
//                           ** description : Parameter 3 for computing thermal conductivity of soil solids
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit :
//             * name: thermCondPar4
//                           ** description : Parameter 4 for computing thermal conductivity of soil solids
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit :
//             * name: volSpecHeatSoil
//                           ** description : Volumetric specific heat over the soil profile
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : J/K/m3
//             * name: soilTemp
//                           ** description : Soil temperature over the soil profile at morning
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: morningSoilTemp
//                           ** description : Soil temperature over the soil profile at morning
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: heatStorage
//                           ** description : CP, heat storage between nodes
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : J/K
//             * name: thermalConductance
//                           ** description : K, conductance of element between nodes
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : W/K
//             * name: thermalConductivity
//                           ** description : Thermal conductivity
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : W.m/K
//             * name: boundaryLayerConductance
//                           ** description : Average daily atmosphere boundary layer conductance
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit :
//             * name: newTemperature
//                           ** description : Soil temperature at the end of this iteration
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: airTemperature
//                           ** description : Air temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : oC
//             * name: maxTempYesterday
//                           ** description : Yesterday's maximum daily air temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : oC
//             * name: minTempYesterday
//                           ** description : Yesterday's minimum daily air temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : oC
//             * name: soilWater
//                           ** description : Volumetric water content of each soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm3/mm3
//             * name: minSoilTemp
//                           ** description : Minimum soil temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: maxSoilTemp
//                           ** description : Maximum soil temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: aveSoilTemp
//                           ** description : average soil temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: aveSoilWater
//                           ** description : Average soil temperaturer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : oC
//             * name: thickness
//                           ** description : Thickness of each soil, includes phantom layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: bulkDensity
//                           ** description : Soil bulk density, includes phantom layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : g/cm3
//             * name: rocks
//                           ** description : Volumetric fraction of rocks in each soil laye
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: carbon
//                           ** description : Volumetric fraction of carbon (CHECK, OM?) in each soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: sand
//                           ** description : Volumetric fraction of sand in each soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: pom
//                           ** description : Particle density of organic matter
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : Mg/m3
//             * name: silt
//                           ** description : Volumetric fraction of silt in each soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: clay
//                           ** description : Volumetric fraction of clay in each soil layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : %
//             * name: soilRoughnessHeight
//                           ** description : Height of soil roughness
//                           ** inputtype : parameter
//                           ** parametercategory : private
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : mm
//             * name: instrumentHeight
//                           ** description : Height of instruments above soil surface
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : mm
//             * name: netRadiation
//                           ** description : Net radiation per internal time-step
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : MJ
//             * name: canopyHeight
//                           ** description : Height of canopy above ground
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : mm
//             * name: instrumHeight
//                           ** description : Height of instruments above ground
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0
//                           ** unit : mm
//             * name: nu
//                           ** description : Forward/backward differencing coefficient
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.6
//                           ** unit : 0-1
//             * name: boundarLayerConductanceSource
//                           ** description : Flag whether boundary layer conductance is calculated or gotten from inpu
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : STRING
//                           ** max :
//                           ** min :
//                           ** default : calc
//                           ** unit :
//             * name: netRadiationSource
//                           ** description : Flag whether net radiation is calculated or gotten from input
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : STRING
//                           ** max :
//                           ** min :
//                           ** default : calc
//                           ** unit : m
//             * name: MissingValue
//                           ** description : missing value
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 99999
//                           ** unit : m
//             * name: soilConstituentNames
//                           ** description : soilConstituentNames
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : STRINGARRAY
//                           ** len : 8
//                           ** max :
//                           ** min :
//                           ** default : ["Rocks", "OrganicMatter", "Sand", "Silt", "Clay", "Water", "Ice", "Air"]
//                           ** unit : m
// - outputs:
//             * name: heatStorage
//                           ** description : CP, heat storage between nodes
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : J/K
//             * name: instrumentHeight
//                           ** description : Height of instruments above soil surface
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : mm
//             * name: canopyHeight
//                           ** description : Height of canopy above ground
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : mm
//             * name: minSoilTemp
//                           ** description : Minimum soil temperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: maxSoilTemp
//                           ** description : Maximum soil temperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: aveSoilTemp
//                           ** description : average soil temperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: volSpecHeatSoil
//                           ** description : Volumetric specific heat over the soil profile
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : J/K/m3
//             * name: soilTemp
//                           ** description : Soil temperature over the soil profile at morning
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: morningSoilTemp
//                           ** description : Soil temperature over the soil profile at morning
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: newTemperature
//                           ** description : Soil temperature at the end of this iteration
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: thermalConductivity
//                           ** description : Thermal conductivity
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : W.m/K
//             * name: thermalConductance
//                           ** description : K, conductance of element between nodes
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : W/K
//             * name: boundaryLayerConductance
//                           ** description : Average daily atmosphere boundary layer conductance
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit :
//             * name: soilWater
//                           ** description : Volumetric water content of each soil layer
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : mm3/mm3
//             * name: doInitialisationStuff
//                           ** description : Flag whether initialisation is needed
//                           ** variablecategory : state
//                           ** datatype : BOOLEAN
//                           ** max :
//                           ** min :
//                           ** unit :
//             * name: maxTempYesterday
//                           ** description : Yesterday's maximum daily air temperature (oC)
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: minTempYesterday
//                           ** description : Yesterday's minimum daily air temperature (oC)
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** len :
//                           ** max :
//                           ** unit : oC
//                           ** min :
//             * name: airTemperature
//                           ** description : Air temperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : oC
//             * name: internalTimeStep
//                           ** description : Internal time-step
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : sec
//             * name: timeOfDaySecs
//                           ** description : Time of day from midnight
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : sec
//             * name: netRadiation
//                           ** description : Net radiation per internal time-step
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : MJ
//             * name: InitialValues
//                           ** description : Initial soil temperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max :
//                           ** min :
//                           ** unit : oC
soil_temperature_calculate_model :: proc(m: ^Soil_Temperature, s: ^Soiltemp_State, s1: ^Soiltemp_State, r: ^Soiltemp_Rate, a: ^Soiltemp_Auxiliary, ex: ^Soiltemp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	i: int
	{
		_r0, _r1, _r2 := get_other_variables(s.num_layers, s.num_nodes, s.soil_water[:], s.instrument_height, m.soil_roughness_height, ex.water_balance_sw[:], ex.micro_climate_canopy_height, s.canopy_height)
		resize(&s.soil_water, len(_r0))
		copy(s.soil_water[:], _r0)
		s.instrument_height = _r1
		s.canopy_height = _r2
	}
	if s.do_initialisation_stuff {
		if values_in_array(s.initial_values[:], m.missing_value) {
			resize(&s.soil_temp, s.num_nodes + 1 + 1)
			slice.fill(s.soil_temp[:], 0.0)
			copy(s.soil_temp[m.topsoil_node:m.topsoil_node + len(s.initial_values)], s.initial_values[0:0 + len(s.initial_values)])
		} else {
			{
				_r0 := calc_soil_temperature(s.soil_temp[:], ex.weather_tav, ex.clock_today_day_of_year, m.surface_node, s.num_nodes, ex.weather_amp, s.thickness[:], m.weather_latitude)
				resize(&s.soil_temp, len(_r0))
				copy(s.soil_temp[:], _r0)
			}
			resize(&s.initial_values, s.num_layers)
			slice.fill(s.initial_values[:], 0.0)
			copy(s.initial_values[0:0 + s.num_layers], s.soil_temp[m.topsoil_node:m.topsoil_node + s.num_layers])
		}
		s.soil_temp[m.air_node] = ex.weather_mean_t
		s.soil_temp[m.surface_node] = calc_surface_temperature(ex.weather_mean_t, ex.weather_max_t, ex.water_balance_salb, ex.weather_radn)
		for i = s.num_nodes + 1; i < len(s.soil_temp); i += 1 {
			s.soil_temp[i] = ex.weather_tav
		}
		copy(s.new_temperature[0:0 + len(s.soil_temp)], s.soil_temp[:])
		s.max_temp_yesterday = ex.weather_max_t
		s.min_temp_yesterday = ex.weather_min_t
		s.do_initialisation_stuff = false
	}
	{
		_r0, _r1, _r2, _r3, _r4, _r5, _r6, _r7, _r8, _r9, _r10, _r11, _r12, _r13, _r14, _r15, _r16 := do_process(s.time_of_day_secs, s.net_radiation, s.min_soil_temp[:], s.max_soil_temp[:], m.num_iterations_for_boundary_layer_conductance, m.timestep, s.boundary_layer_conductance, s.max_temp_yesterday, m.air_node, s.soil_temp[:], s.air_temperature, s.new_temperature[:], ex.weather_max_t, s.internal_time_step, m.boundar_layer_conductance_source, s.thermal_conductivity[:], s.min_temp_yesterday, s.ave_soil_temp[:], s.morning_soil_temp[:], ex.weather_mean_t, m.constant_boundary_layer_conductance, ex.weather_min_t, ex.clock_today_day_of_year, ex.weather_radn, m.weather_latitude, m.soil_constituent_names[:], s.num_nodes, s.vol_spec_heat_soil[:], s.soil_water[:], m.node_depth[:], s.thickness[:], m.surface_node, m.missing_value, s.carbon[:], s.bulk_density[:], m.pom, s.rocks[:], s.sand[:], m.ps, s.silt[:], s.clay[:], m.default_time_of_maximum_temperature, ex.water_balance_eo, ex.water_balance_eos, ex.water_balance_salb, m.stefan_boltzmann_constant, ex.weather_air_pressure, ex.weather_wind, s.instrument_height, s.canopy_height, s.heat_storage[:], m.net_radiation_source, m.latent_heat_of_vapourisation, ex.water_balance_es, s.thermal_conductance[:], m.nu)
		resize(&s.min_soil_temp, len(_r0))
		copy(s.min_soil_temp[:], _r0)
		resize(&s.max_soil_temp, len(_r1))
		copy(s.max_soil_temp[:], _r1)
		resize(&s.soil_temp, len(_r2))
		copy(s.soil_temp[:], _r2)
		resize(&s.new_temperature, len(_r3))
		copy(s.new_temperature[:], _r3)
		resize(&s.thermal_conductivity, len(_r4))
		copy(s.thermal_conductivity[:], _r4)
		resize(&s.ave_soil_temp, len(_r5))
		copy(s.ave_soil_temp[:], _r5)
		resize(&s.morning_soil_temp, len(_r6))
		copy(s.morning_soil_temp[:], _r6)
		resize(&s.vol_spec_heat_soil, len(_r7))
		copy(s.vol_spec_heat_soil[:], _r7)
		resize(&s.heat_storage, len(_r8))
		copy(s.heat_storage[:], _r8)
		resize(&s.thermal_conductance, len(_r9))
		copy(s.thermal_conductance[:], _r9)
		s.time_of_day_secs = _r10
		s.net_radiation = _r11
		s.air_temperature = _r12
		s.internal_time_step = _r13
		s.min_temp_yesterday = _r14
		s.boundary_layer_conductance = _r15
		s.max_temp_yesterday = _r16
		delete(_r0)
		delete(_r1)
		delete(_r2)
		delete(_r3)
		delete(_r4)
		delete(_r5)
		delete(_r7)
		delete(_r8)
		delete(_r9)
	}
}

@(private="file")
get_ini_variables :: proc(instrument_height: f64, instrum_height: f64, default_instrument_height: f64) -> f64 {
	instrument_height := instrument_height
	if instrum_height > 0.00001 {
		instrument_height = instrum_height
	} else {
		instrument_height = default_instrument_height
	}
	return instrument_height
}

@(private="file")
get_profile_variables :: proc(heat_storage: []f64, min_soil_temp: []f64, bulk_density: []f64, num_nodes: int, physical_bd: []f64, max_soil_temp: []f64, water_balance_sw: []f64, organic_carbon: []f64, physical_rocks: []f64, node_depth: []f64, topsoil_node: int, new_temperature: []f64, surface_node: int, soil_water: []f64, thermal_conductance: []f64, thermal_conductivity: []f64, sand: []f64, carbon: []f64, thickness: []f64, num_phantom_nodes: int, physical_particle_size_sand: []f64, rocks: []f64, clay: []f64, physical_particle_size_silt: []f64, air_node: int, physical_particle_size_clay: []f64, soil_temp: []f64, num_layers: int, physical_thickness: []f64, silt: []f64, vol_spec_heat_soil: []f64, ave_soil_temp: []f64, morning_soil_temp: []f64, depth_to_constant_temperature: f64, missing_value: f64) -> ([]f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, int, int) {
	heat_storage := slice.clone_to_dynamic(heat_storage)
	min_soil_temp := slice.clone_to_dynamic(min_soil_temp)
	bulk_density := slice.clone_to_dynamic(bulk_density)
	num_nodes := num_nodes
	max_soil_temp := slice.clone_to_dynamic(max_soil_temp)
	node_depth := slice.clone_to_dynamic(node_depth)
	new_temperature := slice.clone_to_dynamic(new_temperature)
	soil_water := slice.clone_to_dynamic(soil_water)
	thermal_conductance := slice.clone_to_dynamic(thermal_conductance)
	thermal_conductivity := slice.clone_to_dynamic(thermal_conductivity)
	sand := slice.clone_to_dynamic(sand)
	carbon := slice.clone_to_dynamic(carbon)
	thickness := slice.clone_to_dynamic(thickness)
	rocks := slice.clone_to_dynamic(rocks)
	clay := slice.clone_to_dynamic(clay)
	soil_temp := slice.clone_to_dynamic(soil_temp)
	num_layers := num_layers
	silt := slice.clone_to_dynamic(silt)
	vol_spec_heat_soil := slice.clone_to_dynamic(vol_spec_heat_soil)
	ave_soil_temp := slice.clone_to_dynamic(ave_soil_temp)
	morning_soil_temp := slice.clone_to_dynamic(morning_soil_temp)
	layer: int
	node: int
	i: int
	below_profile_depth: f64
	thickness_for_phantom_nodes: f64
	first_phantom_node: int
	old_depth: [dynamic]f64
	defer delete(old_depth)
	old_bulk_density: [dynamic]f64
	defer delete(old_bulk_density)
	old_soil_water: [dynamic]f64
	defer delete(old_soil_water)
	num_layers = len(physical_thickness)
	num_nodes = num_layers + num_phantom_nodes
	resize(&thickness, num_layers + num_phantom_nodes + 1)
	slice.fill(thickness[:], 0.0)
	copy(thickness[1:1 + len(physical_thickness)], physical_thickness[:])
	below_profile_depth = max(depth_to_constant_temperature - sum(thickness[:], 1, num_layers, missing_value), 1000.0)
	thickness_for_phantom_nodes = below_profile_depth * 2.0 / f64(num_phantom_nodes)
	first_phantom_node = num_layers
	for i = first_phantom_node; i < first_phantom_node + num_phantom_nodes; i += 1 {
		thickness[i] = thickness_for_phantom_nodes
	}
	resize(&old_depth, len(node_depth[:]))
	copy(old_depth[:], node_depth[:])
	resize(&node_depth, num_nodes + 1 + 1)
	slice.fill(node_depth[:], 0.0)
	if len(old_depth) != 0 {
		copy(node_depth[0:min(num_nodes + 1 + 1, len(old_depth))], old_depth[0:min(num_nodes + 1 + 1, len(old_depth))])
	}
	node_depth[air_node] = 0.0
	node_depth[surface_node] = 0.0
	node_depth[topsoil_node] = 0.5 * thickness[1] / 1000.0
	for node = topsoil_node; node < num_nodes + 1; node += 1 {
		node_depth[node + 1] = (sum(thickness[:], 1, node - 1, missing_value) + 0.5 * thickness[node]) / 1000.0
	}
	resize(&old_bulk_density, len(bulk_density[:]))
	copy(old_bulk_density[:], bulk_density[:])
	resize(&bulk_density, num_layers + 1 + num_phantom_nodes)
	slice.fill(bulk_density[:], 0.0)
	if len(old_bulk_density) != 0 {
		copy(bulk_density[0:min(num_layers + 1 + num_phantom_nodes, len(old_bulk_density))], old_bulk_density[0:min(num_layers + 1 + num_phantom_nodes, len(old_bulk_density))])
	}
	copy(bulk_density[1:1 + len(physical_bd)], physical_bd[:])
	bulk_density[num_nodes] = bulk_density[num_layers]
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		bulk_density[layer] = bulk_density[num_layers]
	}
	resize(&old_soil_water, len(soil_water[:]))
	copy(old_soil_water[:], soil_water[:])
	resize(&soil_water, num_layers + 1 + num_phantom_nodes)
	slice.fill(soil_water[:], 0.0)
	if len(old_soil_water) != 0 {
		copy(soil_water[0:min(num_layers + 1 + num_phantom_nodes, len(old_soil_water))], old_soil_water[0:min(num_layers + 1 + num_phantom_nodes, len(old_soil_water))])
	}
	if len(water_balance_sw) != 0 {
		for layer = 1; layer < num_layers + 1; layer += 1 {
			soil_water[layer] = divide(water_balance_sw[layer - 1] * thickness[layer - 1], thickness[layer], f64(0))
		}
		for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
			soil_water[layer] = soil_water[num_layers]
		}
	}
	resize(&carbon, num_layers + 1 + num_phantom_nodes)
	slice.fill(carbon[:], 0.0)
	for layer = 1; layer < num_layers + 1; layer += 1 {
		carbon[layer] = organic_carbon[layer - 1]
	}
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		carbon[layer] = carbon[num_layers]
	}
	resize(&rocks, num_layers + 1 + num_phantom_nodes)
	slice.fill(rocks[:], 0.0)
	for layer = 1; layer < num_layers + 1; layer += 1 {
		rocks[layer] = physical_rocks[layer - 1]
	}
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		rocks[layer] = rocks[num_layers]
	}
	resize(&sand, num_layers + 1 + num_phantom_nodes)
	slice.fill(sand[:], 0.0)
	for layer = 1; layer < num_layers + 1; layer += 1 {
		sand[layer] = physical_particle_size_sand[layer - 1]
	}
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		sand[layer] = sand[num_layers]
	}
	resize(&silt, num_layers + 1 + num_phantom_nodes)
	slice.fill(silt[:], 0.0)
	for layer = 1; layer < num_layers + 1; layer += 1 {
		silt[layer] = physical_particle_size_silt[layer - 1]
	}
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		silt[layer] = silt[num_layers]
	}
	resize(&clay, num_layers + 1 + num_phantom_nodes)
	slice.fill(clay[:], 0.0)
	for layer = 1; layer < num_layers + 1; layer += 1 {
		clay[layer] = physical_particle_size_clay[layer - 1]
	}
	for layer = num_layers + 1; layer < num_layers + num_phantom_nodes + 1; layer += 1 {
		clay[layer] = clay[num_layers]
	}
	resize(&max_soil_temp, num_layers + 1 + num_phantom_nodes)
	slice.fill(max_soil_temp[:], 0.0)
	resize(&min_soil_temp, num_layers + 1 + num_phantom_nodes)
	slice.fill(min_soil_temp[:], 0.0)
	resize(&ave_soil_temp, num_layers + 1 + num_phantom_nodes)
	slice.fill(ave_soil_temp[:], 0.0)
	resize(&vol_spec_heat_soil, num_nodes + 1)
	slice.fill(vol_spec_heat_soil[:], 0.0)
	resize(&soil_temp, num_nodes + 1 + 1)
	slice.fill(soil_temp[:], 0.0)
	resize(&morning_soil_temp, num_nodes + 1 + 1)
	slice.fill(morning_soil_temp[:], 0.0)
	resize(&new_temperature, num_nodes + 1 + 1)
	slice.fill(new_temperature[:], 0.0)
	resize(&thermal_conductivity, num_nodes + 1)
	slice.fill(thermal_conductivity[:], 0.0)
	resize(&heat_storage, num_nodes + 1)
	slice.fill(heat_storage[:], 0.0)
	resize(&thermal_conductance, num_nodes + 1 + 1)
	slice.fill(thermal_conductance[:], 0.0)
	return heat_storage[:], min_soil_temp[:], bulk_density[:], max_soil_temp[:], node_depth[:], new_temperature[:], soil_water[:], thermal_conductance[:], thermal_conductivity[:], sand[:], carbon[:], thickness[:], rocks[:], clay[:], soil_temp[:], silt[:], vol_spec_heat_soil[:], ave_soil_temp[:], morning_soil_temp[:], num_nodes, num_layers
}

@(private="file")
do_thermal_conductivity_coeffs :: proc(therm_cond_par2: []f64, num_layers: int, bulk_density: []f64, num_nodes: int, therm_cond_par3: []f64, therm_cond_par4: []f64, clay: []f64, therm_cond_par1: []f64) -> ([]f64, []f64, []f64, []f64) {
	therm_cond_par2 := slice.clone_to_dynamic(therm_cond_par2)
	therm_cond_par3 := slice.clone_to_dynamic(therm_cond_par3)
	therm_cond_par4 := slice.clone_to_dynamic(therm_cond_par4)
	therm_cond_par1 := slice.clone_to_dynamic(therm_cond_par1)
	layer: int
	old_gc1: [dynamic]f64
	defer delete(old_gc1)
	old_gc2: [dynamic]f64
	defer delete(old_gc2)
	old_gc3: [dynamic]f64
	defer delete(old_gc3)
	old_gc4: [dynamic]f64
	defer delete(old_gc4)
	element: int
	resize(&old_gc1, len(therm_cond_par1[:]))
	copy(old_gc1[:], therm_cond_par1[:])
	resize(&therm_cond_par1, num_nodes + 1)
	slice.fill(therm_cond_par1[:], 0.0)
	if len(old_gc1) != 0 {
		copy(therm_cond_par1[0:min(num_nodes + 1, len(old_gc1))], old_gc1[0:min(num_nodes + 1, len(old_gc1))])
	}
	resize(&old_gc2, len(therm_cond_par2[:]))
	copy(old_gc2[:], therm_cond_par2[:])
	resize(&therm_cond_par2, num_nodes + 1)
	slice.fill(therm_cond_par2[:], 0.0)
	if len(old_gc2) != 0 {
		copy(therm_cond_par2[0:min(num_nodes + 1, len(old_gc2))], old_gc2[0:min(num_nodes + 1, len(old_gc2))])
	}
	resize(&old_gc3, len(therm_cond_par3[:]))
	copy(old_gc3[:], therm_cond_par3[:])
	resize(&therm_cond_par3, num_nodes + 1)
	slice.fill(therm_cond_par3[:], 0.0)
	if len(old_gc3) != 0 {
		copy(therm_cond_par3[0:min(num_nodes + 1, len(old_gc3))], old_gc3[0:min(num_nodes + 1, len(old_gc3))])
	}
	resize(&old_gc4, len(therm_cond_par4[:]))
	copy(old_gc4[:], therm_cond_par4[:])
	resize(&therm_cond_par4, num_nodes + 1)
	slice.fill(therm_cond_par4[:], 0.0)
	if len(old_gc4) != 0 {
		copy(therm_cond_par4[0:min(num_nodes + 1, len(old_gc4))], old_gc4[0:min(num_nodes + 1, len(old_gc4))])
	}
	for layer = 1; layer < num_layers + 1 + 1; layer += 1 {
		element = layer
		therm_cond_par1[element] = 0.65 - 0.78 * bulk_density[layer] + 0.6 * cr_math.pow(bulk_density[layer], f64(2))
		therm_cond_par2[element] = 1.06 * bulk_density[layer]
		therm_cond_par3[element] = 1.0 + divide(2.6, math.sqrt(clay[layer]), f64(0))
		therm_cond_par4[element] = 0.03 + 0.1 * cr_math.pow(bulk_density[layer], f64(2))
	}
	return therm_cond_par2[:], therm_cond_par3[:], therm_cond_par4[:], therm_cond_par1[:]
}

@(private="file")
read_param :: proc(bare_soil_roughness: f64, new_temperature: []f64, soil_roughness_height: f64, soil_temp: []f64, therm_cond_par2: []f64, num_layers: int, bulk_density: []f64, num_nodes: int, therm_cond_par3: []f64, therm_cond_par4: []f64, clay: []f64, therm_cond_par1: []f64, weather_tav: f64, clock_today_day_of_year: int, surface_node: int, weather_amp: f64, thickness: []f64, weather_latitude: f64) -> ([]f64, []f64, []f64, []f64, []f64, []f64, f64) {
	soil_roughness_height := soil_roughness_height
	soil_temp := slice.clone_to_dynamic(soil_temp)
	therm_cond_par2 := slice.clone_to_dynamic(therm_cond_par2)
	therm_cond_par3 := slice.clone_to_dynamic(therm_cond_par3)
	therm_cond_par4 := slice.clone_to_dynamic(therm_cond_par4)
	therm_cond_par1 := slice.clone_to_dynamic(therm_cond_par1)
	{
		_r0, _r1, _r2, _r3 := do_thermal_conductivity_coeffs(therm_cond_par2[:], num_layers, bulk_density[:], num_nodes, therm_cond_par3[:], therm_cond_par4[:], clay[:], therm_cond_par1[:])
		resize(&therm_cond_par2, len(_r0))
		copy(therm_cond_par2[:], _r0)
		resize(&therm_cond_par3, len(_r1))
		copy(therm_cond_par3[:], _r1)
		resize(&therm_cond_par4, len(_r2))
		copy(therm_cond_par4[:], _r2)
		resize(&therm_cond_par1, len(_r3))
		copy(therm_cond_par1[:], _r3)
		delete(_r0)
		delete(_r1)
		delete(_r2)
		delete(_r3)
	}
	{
		_r0 := calc_soil_temperature(soil_temp[:], weather_tav, clock_today_day_of_year, surface_node, num_nodes, weather_amp, thickness[:], weather_latitude)
		resize(&soil_temp, len(_r0))
		copy(soil_temp[:], _r0)
	}
	copy(new_temperature[0:0 + len(soil_temp)], soil_temp[:])
	soil_roughness_height = bare_soil_roughness
	return new_temperature, soil_temp[:], therm_cond_par2[:], therm_cond_par3[:], therm_cond_par4[:], therm_cond_par1[:], soil_roughness_height
}

@(private="file")
get_other_variables :: proc(num_layers: int, num_nodes: int, soil_water: []f64, instrument_height: f64, soil_roughness_height: f64, water_balance_sw: []f64, micro_climate_canopy_height: f64, canopy_height: f64) -> ([]f64, f64, f64) {
	instrument_height := instrument_height
	canopy_height := canopy_height
	copy(soil_water[1:1 + len(water_balance_sw)], water_balance_sw[:])
	soil_water[num_nodes] = soil_water[num_layers]
	canopy_height = max(micro_climate_canopy_height, soil_roughness_height) / 1000.0
	instrument_height = max(instrument_height, canopy_height + 0.5)
	return soil_water, instrument_height, canopy_height
}

@(private="file")
volumetric_fraction_organic_matter :: proc(layer: int, carbon: []f64, bulk_density: []f64, pom: f64) -> f64 {
	return carbon[layer] / 100.0 * 2.5 * bulk_density[layer] / pom
}

@(private="file")
volumetric_fraction_rocks :: proc(layer: int, rocks: []f64) -> f64 {
	return rocks[layer] / 100.0
}

@(private="file")
volumetric_fraction_ice :: proc(layer: int) -> f64 {
	return 0.0
}

@(private="file")
volumetric_fraction_water :: proc(layer: int, soil_water: []f64, carbon: []f64, bulk_density: []f64, pom: f64) -> f64 {
	return (1 - volumetric_fraction_organic_matter(layer, carbon[:], bulk_density[:], pom)) * soil_water[layer]
}

@(private="file")
volumetric_fraction_clay :: proc(layer: int, bulk_density: []f64, ps: f64, clay: []f64, carbon: []f64, pom: f64, rocks: []f64) -> f64 {
	return (1 - volumetric_fraction_organic_matter(layer, carbon[:], bulk_density[:], pom) - volumetric_fraction_rocks(layer, rocks[:])) * clay[layer] / 100.0 * bulk_density[layer] / ps
}

@(private="file")
volumetric_fraction_silt :: proc(layer: int, bulk_density: []f64, silt: []f64, ps: f64, carbon: []f64, pom: f64, rocks: []f64) -> f64 {
	return (1 - volumetric_fraction_organic_matter(layer, carbon[:], bulk_density[:], pom) - volumetric_fraction_rocks(layer, rocks[:])) * silt[layer] / 100.0 * bulk_density[layer] / ps
}

@(private="file")
volumetric_fraction_sand :: proc(layer: int, bulk_density: []f64, sand: []f64, ps: f64, carbon: []f64, pom: f64, rocks: []f64) -> f64 {
	return (1 - volumetric_fraction_organic_matter(layer, carbon[:], bulk_density[:], pom) - volumetric_fraction_rocks(layer, rocks[:])) * sand[layer] / 100.0 * bulk_density[layer] / ps
}

@(private="file")
kelvin_t :: proc(celcius_t: f64) -> f64 {
	celcius_to_kelvin: f64
	celcius_to_kelvin = 273.18
	return celcius_t + celcius_to_kelvin
}

@(private="file")
divide :: proc(value1: f64, value2: f64, err_val: f64) -> f64 {
	if value2 != f64(0) {
		return value1 / value2
	}
	return err_val
}

@(private="file")
sum :: proc(values: []f64, start_index: int, end_index: int, missing_value: f64) -> f64 {
	value: f64
	result: f64
	index: int
	result = 0.0
	index = -1
	for value_it in values {
		value = value_it
		index = index + 1
		if index >= start_index && value != missing_value {
			result = result + value
		}
		if index == end_index {
			break
		}
	}
	return result
}

@(private="file")
volumetric_specific_heat :: proc(name: string, layer: int) -> f64 {
	specific_heat_rocks: f64
	specific_heat_om: f64
	specific_heat_sand: f64
	specific_heat_silt: f64
	specific_heat_clay: f64
	specific_heat_water: f64
	specific_heat_ice: f64
	specific_heat_air: f64
	result: f64
	specific_heat_rocks = 7.7
	specific_heat_om = 0.25
	specific_heat_sand = 7.7
	specific_heat_silt = 2.74
	specific_heat_clay = 2.92
	specific_heat_water = 0.57
	specific_heat_ice = 2.18
	specific_heat_air = 0.025
	result = 0.0
	if name == "Rocks" {
		result = specific_heat_rocks
	} else if name == "OrganicMatter" {
		result = specific_heat_om
	} else if name == "Sand" {
		result = specific_heat_sand
	} else if name == "Silt" {
		result = specific_heat_silt
	} else if name == "Clay" {
		result = specific_heat_clay
	} else if name == "Water" {
		result = specific_heat_water
	} else if name == "Ice" {
		result = specific_heat_ice
	} else if name == "Air" {
		result = specific_heat_air
	}
	return result
}

@(private="file")
volumetric_fraction_air :: proc(layer: int, rocks: []f64, carbon: []f64, bulk_density: []f64, pom: f64, sand: []f64, ps: f64, silt: []f64, clay: []f64, soil_water: []f64) -> f64 {
	return 1.0 - volumetric_fraction_rocks(layer, rocks[:]) - volumetric_fraction_organic_matter(layer, carbon[:], bulk_density[:], pom) - volumetric_fraction_sand(layer, bulk_density[:], sand[:], ps, carbon[:], pom, rocks[:]) - volumetric_fraction_silt(layer, bulk_density[:], silt[:], ps, carbon[:], pom, rocks[:]) - volumetric_fraction_clay(layer, bulk_density[:], ps, clay[:], carbon[:], pom, rocks[:]) - volumetric_fraction_water(layer, soil_water[:], carbon[:], bulk_density[:], pom) - volumetric_fraction_ice(layer)
}

@(private="file")
air_density :: proc(temperature: f64, air_pressure: f64) -> f64 {
	m_wair: f64
	rgas: f64
	hpa2_pa: f64
	m_wair = 0.02897
	rgas = 8.3143
	hpa2_pa = 100.0
	return divide(m_wair * air_pressure * hpa2_pa, kelvin_t(temperature) * rgas, 0.0)
}

@(private="file")
long_wave_radn :: proc(emissivity: f64, t_deg_c: f64, stefan_boltzmann_constant: f64) -> f64 {
	return stefan_boltzmann_constant * emissivity * cr_math.pow(kelvin_t(t_deg_c), f64(4))
}

@(private="file")
map_layer2_node :: proc(layer_array: []f64, node_array: []f64, node_depth: []f64, num_nodes: int, thickness: []f64, surface_node: int, missing_value: f64) -> []f64 {
	node: int
	layer: int
	depth_layer_above: f64
	d1: f64
	d2: f64
	d_sum: f64
	for node = surface_node; node < num_nodes + 1; node += 1 {
		layer = node - 1
		depth_layer_above = layer >= 1 ? sum(thickness[:], 1, layer, missing_value) : 0.0
		d1 = depth_layer_above - node_depth[node] * 1000.0
		d2 = node_depth[node + 1] * 1000.0 - depth_layer_above
		d_sum = d1 + d2
		node_array[node] = divide(layer_array[layer] * d1, d_sum, f64(0)) + divide(layer_array[layer + 1] * d2, d_sum, f64(0))
	}
	return node_array
}

@(private="file")
thermal_conductance :: proc(name: string, layer: int, rocks: []f64, bulk_density: []f64, sand: []f64, ps: f64, carbon: []f64, pom: f64, silt: []f64, clay: []f64) -> f64 {
	thermal_conductance_rocks: f64
	thermal_conductance_om: f64
	thermal_conductance_sand: f64
	thermal_conductance_silt: f64
	thermal_conductance_clay: f64
	thermal_conductance_water: f64
	thermal_conductance_ice: f64
	thermal_conductance_air: f64
	result: f64
	thermal_conductance_rocks = 0.182
	thermal_conductance_om = 2.50
	thermal_conductance_sand = 0.182
	thermal_conductance_silt = 2.39
	thermal_conductance_clay = 1.39
	thermal_conductance_water = 4.18
	thermal_conductance_ice = 1.73
	thermal_conductance_air = 0.0012
	result = 0.0
	if name == "Rocks" {
		result = thermal_conductance_rocks
	} else if name == "OrganicMatter" {
		result = thermal_conductance_om
	} else if name == "Sand" {
		result = thermal_conductance_sand
	} else if name == "Silt" {
		result = thermal_conductance_silt
	} else if name == "Clay" {
		result = thermal_conductance_clay
	} else if name == "Water" {
		result = thermal_conductance_water
	} else if name == "Ice" {
		result = thermal_conductance_ice
	} else if name == "Air" {
		result = thermal_conductance_air
	} else if name == "Minerals" {
		result = cr_math.pow(thermal_conductance_rocks, volumetric_fraction_rocks(layer, rocks[:])) * cr_math.pow(thermal_conductance_sand, volumetric_fraction_sand(layer, bulk_density[:], sand[:], ps, carbon[:], pom, rocks[:])) + cr_math.pow(thermal_conductance_silt, volumetric_fraction_silt(layer, bulk_density[:], silt[:], ps, carbon[:], pom, rocks[:])) + cr_math.pow(thermal_conductance_clay, volumetric_fraction_clay(layer, bulk_density[:], ps, clay[:], carbon[:], pom, rocks[:]))
	}
	result = volumetric_specific_heat(name, layer)
	return result
}

@(private="file")
shape_factor :: proc(name: string, layer: int, soil_water: []f64, carbon: []f64, bulk_density: []f64, pom: f64, rocks: []f64, sand: []f64, ps: f64, silt: []f64, clay: []f64) -> f64 {
	shape_factor_rocks: f64
	shape_factor_om: f64
	shape_factor_sand: f64
	shape_factor_silt: f64
	shape_factor_clay: f64
	shape_factor_water: f64
	result: f64
	shape_factor_rocks = 0.182
	shape_factor_om = 0.5
	shape_factor_sand = 0.182
	shape_factor_silt = 0.125
	shape_factor_clay = 0.007755
	shape_factor_water = 1.0
	result = 0.0
	if name == "Rocks" {
		result = shape_factor_rocks
	} else if name == "OrganicMatter" {
		result = shape_factor_om
	} else if name == "Sand" {
		result = shape_factor_sand
	} else if name == "Silt" {
		result = shape_factor_silt
	} else if name == "Clay" {
		result = shape_factor_clay
	} else if name == "Water" {
		result = shape_factor_water
	} else if name == "Ice" {
		result = 0.333 - 0.333 * volumetric_fraction_ice(layer) / (volumetric_fraction_water(layer, soil_water[:], carbon[:], bulk_density[:], pom) + volumetric_fraction_ice(layer) + volumetric_fraction_air(layer, rocks[:], carbon[:], bulk_density[:], pom, sand[:], ps, silt[:], clay[:], soil_water[:]))
		return result
	} else if name == "Air" {
		result = 0.333 - 0.333 * volumetric_fraction_air(layer, rocks[:], carbon[:], bulk_density[:], pom, sand[:], ps, silt[:], clay[:], soil_water[:]) / (volumetric_fraction_water(layer, soil_water[:], carbon[:], bulk_density[:], pom) + volumetric_fraction_ice(layer) + volumetric_fraction_air(layer, rocks[:], carbon[:], bulk_density[:], pom, sand[:], ps, silt[:], clay[:], soil_water[:]))
		return result
	} else if name == "Minerals" {
		result = shape_factor_rocks * volumetric_fraction_rocks(layer, rocks[:]) + shape_factor_sand * volumetric_fraction_sand(layer, bulk_density[:], sand[:], ps, carbon[:], pom, rocks[:]) + shape_factor_silt * volumetric_fraction_silt(layer, bulk_density[:], silt[:], ps, carbon[:], pom, rocks[:]) + shape_factor_clay * volumetric_fraction_clay(layer, bulk_density[:], ps, clay[:], carbon[:], pom, rocks[:])
	}
	result = volumetric_specific_heat(name, layer)
	return result
}

@(private="file")
do_update :: proc(num_interations_per_day: int, time_of_day_secs: f64, boundary_layer_conductance: f64, min_soil_temp: []f64, air_node: int, soil_temp: []f64, new_temperature: []f64, num_nodes: int, surface_node: int, internal_time_step: f64, max_soil_temp: []f64, ave_soil_temp: []f64, thermal_conductivity: []f64) -> ([]f64, []f64, []f64, []f64, f64) {
	boundary_layer_conductance := boundary_layer_conductance
	node: int
	copy(soil_temp[0:0 + len(new_temperature)], new_temperature[:])
	if time_of_day_secs < internal_time_step * 1.2 {
		for node = surface_node; node < num_nodes + 1; node += 1 {
			min_soil_temp[node] = soil_temp[node]
			max_soil_temp[node] = soil_temp[node]
		}
	}
	for node = surface_node; node < num_nodes + 1; node += 1 {
		if soil_temp[node] < min_soil_temp[node] {
			min_soil_temp[node] = soil_temp[node]
		} else if soil_temp[node] > max_soil_temp[node] {
			max_soil_temp[node] = soil_temp[node]
		}
		ave_soil_temp[node] = ave_soil_temp[node] + divide(soil_temp[node], f64(num_interations_per_day), f64(0))
	}
	boundary_layer_conductance = boundary_layer_conductance + divide(thermal_conductivity[air_node], f64(num_interations_per_day), f64(0))
	return min_soil_temp, soil_temp, max_soil_temp, ave_soil_temp, boundary_layer_conductance
}

@(private="file")
do_thomas :: proc(new_temps: []f64, net_radiation: f64, heat_storage: []f64, water_balance_eos: f64, num_nodes: int, timestep: f64, net_radiation_source: string, latent_heat_of_vapourisation: f64, node_depth: []f64, water_balance_es: f64, air_node: int, soil_temp: []f64, surface_node: int, internal_time_step: f64, thermal_conductance: []f64, thermal_conductivity: []f64, nu: f64, vol_spec_heat_soil: []f64) -> ([]f64, []f64, []f64) {
	node: int
	a := make([dynamic]f64, num_nodes + 1 + 1)
	defer delete(a)
	b := make([dynamic]f64, num_nodes + 1)
	defer delete(b)
	c := make([dynamic]f64, num_nodes + 1)
	defer delete(c)
	d := make([dynamic]f64, num_nodes + 1)
	defer delete(d)
	volume_of_soil_at_node: f64
	element_length: f64
	g: f64
	sensible_heat_flux: f64
	radn_net: f64
	latent_heat_flux: f64
	soil_surface_heat_flux: f64
	thermal_conductance[air_node] = thermal_conductivity[air_node]
	for node = surface_node; node < num_nodes + 1; node += 1 {
		volume_of_soil_at_node = 0.5 * (node_depth[node + 1] - node_depth[node - 1])
		heat_storage[node] = divide(vol_spec_heat_soil[node] * volume_of_soil_at_node, internal_time_step, f64(0))
		element_length = node_depth[node + 1] - node_depth[node]
		thermal_conductance[node] = divide(thermal_conductivity[node], element_length, f64(0))
	}
	g = 1 - nu
	for node = surface_node; node < num_nodes + 1; node += 1 {
		c[node] = -nu * thermal_conductance[node]
		a[node + 1] = c[node]
		b[node] = nu * (thermal_conductance[node] + thermal_conductance[node - 1]) + heat_storage[node]
		d[node] = g * thermal_conductance[node - 1] * soil_temp[node - 1] + (heat_storage[node] - g * (thermal_conductance[node] + thermal_conductance[node - 1])) * soil_temp[node] + g * thermal_conductance[node] * soil_temp[node + 1]
	}
	a[surface_node] = 0.0
	sensible_heat_flux = nu * thermal_conductance[air_node] * new_temps[air_node]
	radn_net = 0.0
	if net_radiation_source == "calc" {
		radn_net = divide(net_radiation * 1000000.0, internal_time_step, f64(0))
	} else if net_radiation_source == "eos" {
		radn_net = divide(water_balance_eos * latent_heat_of_vapourisation, timestep, f64(0))
	}
	latent_heat_flux = divide(water_balance_es * latent_heat_of_vapourisation, timestep, f64(0))
	soil_surface_heat_flux = sensible_heat_flux + radn_net - latent_heat_flux
	d[surface_node] = d[surface_node] + soil_surface_heat_flux
	d[num_nodes] = d[num_nodes] + nu * thermal_conductance[num_nodes] * new_temps[num_nodes + 1]
	for node = surface_node; node < num_nodes - 1 + 1; node += 1 {
		c[node] = divide(c[node], b[node], f64(0))
		d[node] = divide(d[node], b[node], f64(0))
		b[node + 1] = b[node + 1] - a[node + 1] * c[node]
		d[node + 1] = d[node + 1] - a[node + 1] * d[node]
	}
	new_temps[num_nodes] = divide(d[num_nodes], b[num_nodes], f64(0))
	for node = num_nodes - 1; node > surface_node - 1; node += -1 {
		new_temps[node] = d[node] - c[node] * new_temps[node + 1]
	}
	return new_temps, heat_storage, thermal_conductance
}

@(private="file")
get_boundary_layer_conductance :: proc(t_new_zb: []f64, weather_air_pressure: f64, stefan_boltzmann_constant: f64, water_balance_eos: f64, weather_wind: f64, air_temperature: f64, surface_node: int, water_balance_eo: f64, instrument_height: f64, canopy_height: f64) -> (f64, []f64) {
	iteration: int
	von_karman_constant: f64
	gravitational_constant: f64
	specific_heat_of_air: f64
	surface_emissivity: f64
	specific_heat_air: f64
	roughness_factor_momentum: f64
	roughness_factor_heat: f64
	d: f64
	surface_temperature: f64
	diffuse_penetration_constant: f64
	radiative_conductance: f64
	friction_velocity: f64
	boundary_layer_cond: f64
	stability_parammeter: f64
	stability_correction_momentum: f64
	stability_correction_heat: f64
	heat_flux_density: f64
	von_karman_constant = 0.41
	gravitational_constant = 9.8
	specific_heat_of_air = 1010.0
	surface_emissivity = 0.98
	specific_heat_air = specific_heat_of_air * air_density(air_temperature, weather_air_pressure)
	roughness_factor_momentum = 0.13 * canopy_height
	roughness_factor_heat = 0.2 * roughness_factor_momentum
	d = 0.77 * canopy_height
	surface_temperature = t_new_zb[surface_node]
	diffuse_penetration_constant = max(0.1, water_balance_eos) / max(0.1, water_balance_eo)
	radiative_conductance = 4.0 * stefan_boltzmann_constant * surface_emissivity * diffuse_penetration_constant * cr_math.pow(kelvin_t(air_temperature), f64(3))
	friction_velocity = 0.0
	boundary_layer_cond = 0.0
	stability_parammeter = 0.0
	stability_correction_momentum = 0.0
	stability_correction_heat = 0.0
	heat_flux_density = 0.0
	for iteration = 1; iteration < 3 + 1; iteration += 1 {
		friction_velocity = divide(weather_wind * von_karman_constant, cr_math.log(divide(instrument_height - d + roughness_factor_momentum, roughness_factor_momentum, f64(0))) + stability_correction_momentum, f64(0))
		boundary_layer_cond = divide(specific_heat_air * von_karman_constant * friction_velocity, cr_math.log(divide(instrument_height - d + roughness_factor_heat, roughness_factor_heat, f64(0))) + stability_correction_heat, f64(0))
		boundary_layer_cond = boundary_layer_cond + radiative_conductance
		heat_flux_density = boundary_layer_cond * (surface_temperature - air_temperature)
		stability_parammeter = divide(-von_karman_constant * instrument_height * gravitational_constant * heat_flux_density, specific_heat_air * kelvin_t(air_temperature) * cr_math.pow(friction_velocity, f64(3.0)), f64(0))
		if stability_parammeter > 0.0 {
			stability_correction_heat = 4.7 * stability_parammeter
			stability_correction_momentum = stability_correction_heat
		} else {
			stability_correction_heat = -2.0 * cr_math.log((1.0 + math.sqrt(1.0 - 16.0 * stability_parammeter)) / 2.0)
			stability_correction_momentum = 0.6 * stability_correction_heat
		}
	}
	return boundary_layer_cond, t_new_zb
}

@(private="file")
interpolate_net_radiation :: proc(solar_radn: f64, cloud_fr: f64, cva: f64, water_balance_eo: f64, water_balance_eos: f64, water_balance_salb: f64, soil_temp: []f64, air_temperature: f64, surface_node: int, internal_time_step: f64, stefan_boltzmann_constant: f64) -> f64 {
	surface_emissivity: f64
	w2_mj: f64
	emissivity_atmos: f64
	penetration_constant: f64
	lw_rin_soil: f64
	lw_rout_soil: f64
	lw_rnet_soil: f64
	sw_rin: f64
	sw_rout: f64
	sw_rnet_soil: f64
	surface_emissivity = 0.96
	w2_mj = internal_time_step / 1000000.0
	emissivity_atmos = (1 - 0.84 * cloud_fr) * 0.58 * cr_math.pow(cva, f64(1.0 / 7.0)) + 0.84 * cloud_fr
	penetration_constant = divide(max(0.1, water_balance_eos), max(0.1, water_balance_eo), f64(0))
	lw_rin_soil = long_wave_radn(emissivity_atmos, air_temperature, stefan_boltzmann_constant) * penetration_constant * w2_mj
	lw_rout_soil = long_wave_radn(surface_emissivity, soil_temp[surface_node], stefan_boltzmann_constant) * penetration_constant * w2_mj
	lw_rnet_soil = lw_rin_soil - lw_rout_soil
	sw_rin = solar_radn
	sw_rout = water_balance_salb * solar_radn
	sw_rnet_soil = (sw_rin - sw_rout) * penetration_constant
	return sw_rnet_soil + lw_rnet_soil
}

@(private="file")
interpolate_temperature :: proc(time_hours: f64, min_temp_yesterday: f64, max_temp_yesterday: f64, weather_mean_t: f64, weather_max_t: f64, weather_min_t: f64, default_time_of_maximum_temperature: f64) -> f64 {
	time: f64
	max_t_time: f64
	min_t_time: f64
	midnight_t: f64
	t_scale: f64
	current_temperature: f64
	time = time_hours / 24.0
	max_t_time = default_time_of_maximum_temperature / 24.0
	min_t_time = max_t_time - 0.5
	if time < min_t_time {
		midnight_t = cr_math.sin((0.0 + 0.25 - max_t_time) * 2.0 * math.PI) * (max_temp_yesterday - min_temp_yesterday) / 2.0 + (max_temp_yesterday + min_temp_yesterday) / 2.0
		t_scale = (min_t_time - time) / min_t_time
		if t_scale > 1.0 {
			t_scale = 1.0
		} else if t_scale < f64(0) {
			t_scale = f64(0)
		}
		current_temperature = weather_min_t + t_scale * (midnight_t - weather_min_t)
		return current_temperature
	} else {
		current_temperature = cr_math.sin((time + 0.25 - max_t_time) * 2.0 * math.PI) * (weather_max_t - weather_min_t) / 2.0 + weather_mean_t
		return current_temperature
	}
}

@(private="file")
do_thermal_conductivity :: proc(soil_constituent_names: []string, num_nodes: int, soil_water: []f64, thermal_conductivity: []f64, carbon: []f64, bulk_density: []f64, pom: f64, rocks: []f64, sand: []f64, ps: f64, silt: []f64, clay: []f64, node_depth: []f64, thickness: []f64, surface_node: int, missing_value: f64) -> []f64 {
	thermal_conductivity := slice.clone_to_dynamic(thermal_conductivity)
	node: int
	constituent_name: string
	therm_cond_layers := make([dynamic]f64, num_nodes + 1)
	defer delete(therm_cond_layers)
	numerator: f64
	denominator: f64
	shape_factor_constituent: f64
	thermal_conductance_constituent: f64
	thermal_conductance_water: f64
	k: f64
	for node = 1; node < num_nodes + 1; node += 1 {
		numerator = 0.0
		denominator = 0.0
		for constituent_name_it in soil_constituent_names {
			constituent_name = constituent_name_it
			shape_factor_constituent = shape_factor(constituent_name, node, soil_water[:], carbon[:], bulk_density[:], pom, rocks[:], sand[:], ps, silt[:], clay[:])
			thermal_conductance_constituent = thermal_conductance(constituent_name, node, rocks[:], bulk_density[:], sand[:], ps, carbon[:], pom, silt[:], clay[:])
			thermal_conductance_water = thermal_conductance("Water", node, rocks[:], bulk_density[:], sand[:], ps, carbon[:], pom, silt[:], clay[:])
			k = 2.0 / 3.0 * cr_math.pow(1 + shape_factor_constituent * (thermal_conductance_constituent / thermal_conductance_water - 1.0), f64(-1)) + 1.0 / 3.0 * cr_math.pow(1 + shape_factor_constituent * (thermal_conductance_constituent / thermal_conductance_water - 1.0) * (1 - 2 * shape_factor_constituent), f64(-1))
			numerator = numerator + thermal_conductance_constituent * soil_water[node] * k
			denominator = denominator + soil_water[node] * k
		}
		therm_cond_layers[node] = numerator / denominator
	}
	{
		_r0 := map_layer2_node(therm_cond_layers[:], thermal_conductivity[:], node_depth[:], num_nodes, thickness[:], surface_node, missing_value)
		resize(&thermal_conductivity, len(_r0))
		copy(thermal_conductivity[:], _r0)
	}
	return thermal_conductivity[:]
}

@(private="file")
do_volumetric_specific_heat :: proc(soil_constituent_names: []string, num_nodes: int, vol_spec_heat_soil: []f64, soil_water: []f64, node_depth: []f64, thickness: []f64, surface_node: int, missing_value: f64) -> []f64 {
	vol_spec_heat_soil := slice.clone_to_dynamic(vol_spec_heat_soil)
	node: int
	constituent_name: string
	volspec_heat_soil_ := make([dynamic]f64, num_nodes + 1)
	defer delete(volspec_heat_soil_)
	for node = 1; node < num_nodes + 1; node += 1 {
		volspec_heat_soil_[node] = f64(0)
		for constituent_name_it in soil_constituent_names {
			constituent_name = constituent_name_it
			if !slice.contains([]string{"Minerals"}, constituent_name) {
				volspec_heat_soil_[node] = volspec_heat_soil_[node] + volumetric_specific_heat(constituent_name, node) * 1000000.0 * soil_water[node]
			}
		}
	}
	{
		_r0 := map_layer2_node(volspec_heat_soil_[:], vol_spec_heat_soil[:], node_depth[:], num_nodes, thickness[:], surface_node, missing_value)
		resize(&vol_spec_heat_soil, len(_r0))
		copy(vol_spec_heat_soil[:], _r0)
	}
	return vol_spec_heat_soil[:]
}

@(private="file")
zero :: proc(arr: []f64) -> []f64 {
	i: int
	if len(arr) != 0 {
		for i = 0; i < len(arr); i += 1 {
			arr[i] = f64(0)
		}
	}
	return arr
}

@(private="file")
do_net_radiation :: proc(solar_radn: []f64, cloud_fr: f64, cva: f64, iteration_sper_day: int, weather_min_t: f64, clock_today_day_of_year: int, weather_radn: f64, weather_latitude: f64) -> ([]f64, f64, f64) {
	cloud_fr := cloud_fr
	cva := cva
	timestep_number: int
	tsteps2_rad: f64
	solar_constant: f64
	solar_declination: f64
	c_d: f64
	m1 := make([dynamic]f64, iteration_sper_day + 1)
	defer delete(m1)
	m1_tot: f64
	psr: f64
	fr: f64
	tsteps2_rad = divide(2.0 * math.PI, f64(iteration_sper_day), f64(0))
	solar_constant = 1360.0
	solar_declination = 0.3985 * cr_math.sin(4.869 + f64(clock_today_day_of_year) * 2.0 * math.PI / 365.25 + 0.03345 * cr_math.sin(6.224 + f64(clock_today_day_of_year) * 2.0 * math.PI / 365.25))
	c_d = math.sqrt(1.0 - solar_declination * solar_declination)
	m1_tot = 0.0
	for timestep_number = 1; timestep_number < iteration_sper_day + 1; timestep_number += 1 {
		m1[timestep_number] = (solar_declination * cr_math.sin(weather_latitude * math.PI / 180.0) + c_d * cr_math.cos(weather_latitude * math.PI / 180.0) * cr_math.cos(tsteps2_rad * (f64(timestep_number) - f64(iteration_sper_day) / 2.0))) * 24.0 / f64(iteration_sper_day)
		if m1[timestep_number] > 0.0 {
			m1_tot = m1_tot + m1[timestep_number]
		} else {
			m1[timestep_number] = 0.0
		}
	}
	psr = m1_tot * solar_constant * 3600.0 / 1000000.0
	fr = divide(max(weather_radn, 0.1), psr, f64(0))
	cloud_fr = 2.33 - 3.33 * fr
	cloud_fr = min(max(cloud_fr, 0.0), 1.0)
	for timestep_number = 1; timestep_number < iteration_sper_day + 1; timestep_number += 1 {
		solar_radn[timestep_number] = max(weather_radn, 0.1) * divide(m1[timestep_number], m1_tot, f64(0))
	}
	cva = cr_math.exp(31.3716 - 6014.79 / kelvin_t(weather_min_t) - 0.00792495 * kelvin_t(weather_min_t)) / kelvin_t(weather_min_t)
	return solar_radn, cloud_fr, cva
}

@(private="file")
do_process :: proc(time_of_day_secs: f64, net_radiation: f64, min_soil_temp: []f64, max_soil_temp: []f64, num_iterations_for_boundary_layer_conductance: int, timestep: f64, boundary_layer_conductance: f64, max_temp_yesterday: f64, air_node: int, soil_temp: []f64, air_temperature: f64, new_temperature: []f64, weather_max_t: f64, internal_time_step: f64, boundar_layer_conductance_source: string, thermal_conductivity: []f64, min_temp_yesterday: f64, ave_soil_temp: []f64, morning_soil_temp: []f64, weather_mean_t: f64, constant_boundary_layer_conductance: f64, weather_min_t: f64, clock_today_day_of_year: int, weather_radn: f64, weather_latitude: f64, soil_constituent_names: []string, num_nodes: int, vol_spec_heat_soil: []f64, soil_water: []f64, node_depth: []f64, thickness: []f64, surface_node: int, missing_value: f64, carbon: []f64, bulk_density: []f64, pom: f64, rocks: []f64, sand: []f64, ps: f64, silt: []f64, clay: []f64, default_time_of_maximum_temperature: f64, water_balance_eo: f64, water_balance_eos: f64, water_balance_salb: f64, stefan_boltzmann_constant: f64, weather_air_pressure: f64, weather_wind: f64, instrument_height: f64, canopy_height: f64, heat_storage: []f64, net_radiation_source: string, latent_heat_of_vapourisation: f64, water_balance_es: f64, thermal_conductance: []f64, nu: f64) -> ([]f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, []f64, f64, f64, f64, f64, f64, f64, f64) {
	time_of_day_secs := time_of_day_secs
	net_radiation := net_radiation
	min_soil_temp := slice.clone_to_dynamic(min_soil_temp)
	max_soil_temp := slice.clone_to_dynamic(max_soil_temp)
	boundary_layer_conductance := boundary_layer_conductance
	max_temp_yesterday := max_temp_yesterday
	soil_temp := slice.clone_to_dynamic(soil_temp)
	air_temperature := air_temperature
	new_temperature := slice.clone_to_dynamic(new_temperature)
	internal_time_step := internal_time_step
	thermal_conductivity := slice.clone_to_dynamic(thermal_conductivity)
	min_temp_yesterday := min_temp_yesterday
	ave_soil_temp := slice.clone_to_dynamic(ave_soil_temp)
	vol_spec_heat_soil := slice.clone_to_dynamic(vol_spec_heat_soil)
	heat_storage := slice.clone_to_dynamic(heat_storage)
	thermal_conductance := slice.clone_to_dynamic(thermal_conductance)
	time_step_iteration: int
	iteration: int
	interactions_per_day: int
	cva: f64
	cloud_fr: f64
	solar_radn := make([dynamic]f64, 49)
	defer delete(solar_radn)
	interactions_per_day = 48
	cva = 0.0
	cloud_fr = 0.0
	{
		_r0, _r1, _r2 := do_net_radiation(solar_radn[:], cloud_fr, cva, interactions_per_day, weather_min_t, clock_today_day_of_year, weather_radn, weather_latitude)
		resize(&solar_radn, len(_r0))
		copy(solar_radn[:], _r0)
		cloud_fr = _r1
		cva = _r2
	}
	{
		_r0 := zero(min_soil_temp[:])
		resize(&min_soil_temp, len(_r0))
		copy(min_soil_temp[:], _r0)
	}
	{
		_r0 := zero(max_soil_temp[:])
		resize(&max_soil_temp, len(_r0))
		copy(max_soil_temp[:], _r0)
	}
	{
		_r0 := zero(ave_soil_temp[:])
		resize(&ave_soil_temp, len(_r0))
		copy(ave_soil_temp[:], _r0)
	}
	boundary_layer_conductance = 0.0
	internal_time_step = math.round(timestep / f64(interactions_per_day))
	{
		_r0 := do_volumetric_specific_heat(soil_constituent_names[:], num_nodes, vol_spec_heat_soil[:], soil_water[:], node_depth[:], thickness[:], surface_node, missing_value)
		resize(&vol_spec_heat_soil, len(_r0))
		copy(vol_spec_heat_soil[:], _r0)
		delete(_r0)
	}
	{
		_r0 := do_thermal_conductivity(soil_constituent_names[:], num_nodes, soil_water[:], thermal_conductivity[:], carbon[:], bulk_density[:], pom, rocks[:], sand[:], ps, silt[:], clay[:], node_depth[:], thickness[:], surface_node, missing_value)
		resize(&thermal_conductivity, len(_r0))
		copy(thermal_conductivity[:], _r0)
		delete(_r0)
	}
	for time_step_iteration = 1; time_step_iteration < interactions_per_day + 1; time_step_iteration += 1 {
		time_of_day_secs = internal_time_step * f64(time_step_iteration)
		if timestep < 24.0 * 60.0 * 60.0 {
			air_temperature = weather_mean_t
		} else {
			air_temperature = interpolate_temperature(time_of_day_secs / 3600.0, min_temp_yesterday, max_temp_yesterday, weather_mean_t, weather_max_t, weather_min_t, default_time_of_maximum_temperature)
		}
		new_temperature[air_node] = air_temperature
		net_radiation = interpolate_net_radiation(solar_radn[time_step_iteration], cloud_fr, cva, water_balance_eo, water_balance_eos, water_balance_salb, soil_temp[:], air_temperature, surface_node, internal_time_step, stefan_boltzmann_constant)
		if boundar_layer_conductance_source == "constant" {
			thermal_conductivity[air_node] = constant_boundary_layer_conductance
		} else if boundar_layer_conductance_source == "calc" {
			{
				_r0, _r1 := get_boundary_layer_conductance(new_temperature[:], weather_air_pressure, stefan_boltzmann_constant, water_balance_eos, weather_wind, air_temperature, surface_node, water_balance_eo, instrument_height, canopy_height)
				thermal_conductivity[air_node] = _r0
				resize(&new_temperature, len(_r1))
				copy(new_temperature[:], _r1)
			}
			for iteration = 1; iteration < num_iterations_for_boundary_layer_conductance + 1; iteration += 1 {
				{
					_r0, _r1, _r2 := do_thomas(new_temperature[:], net_radiation, heat_storage[:], water_balance_eos, num_nodes, timestep, net_radiation_source, latent_heat_of_vapourisation, node_depth[:], water_balance_es, air_node, soil_temp[:], surface_node, internal_time_step, thermal_conductance[:], thermal_conductivity[:], nu, vol_spec_heat_soil[:])
					resize(&new_temperature, len(_r0))
					copy(new_temperature[:], _r0)
					resize(&heat_storage, len(_r1))
					copy(heat_storage[:], _r1)
					resize(&thermal_conductance, len(_r2))
					copy(thermal_conductance[:], _r2)
				}
				{
					_r0, _r1 := get_boundary_layer_conductance(new_temperature[:], weather_air_pressure, stefan_boltzmann_constant, water_balance_eos, weather_wind, air_temperature, surface_node, water_balance_eo, instrument_height, canopy_height)
					thermal_conductivity[air_node] = _r0
					resize(&new_temperature, len(_r1))
					copy(new_temperature[:], _r1)
				}
			}
		}
		{
			_r0, _r1, _r2 := do_thomas(new_temperature[:], net_radiation, heat_storage[:], water_balance_eos, num_nodes, timestep, net_radiation_source, latent_heat_of_vapourisation, node_depth[:], water_balance_es, air_node, soil_temp[:], surface_node, internal_time_step, thermal_conductance[:], thermal_conductivity[:], nu, vol_spec_heat_soil[:])
			resize(&new_temperature, len(_r0))
			copy(new_temperature[:], _r0)
			resize(&heat_storage, len(_r1))
			copy(heat_storage[:], _r1)
			resize(&thermal_conductance, len(_r2))
			copy(thermal_conductance[:], _r2)
		}
		{
			_r0, _r1, _r2, _r3, _r4 := do_update(interactions_per_day, time_of_day_secs, boundary_layer_conductance, min_soil_temp[:], air_node, soil_temp[:], new_temperature[:], num_nodes, surface_node, internal_time_step, max_soil_temp[:], ave_soil_temp[:], thermal_conductivity[:])
			resize(&min_soil_temp, len(_r0))
			copy(min_soil_temp[:], _r0)
			resize(&soil_temp, len(_r1))
			copy(soil_temp[:], _r1)
			resize(&max_soil_temp, len(_r2))
			copy(max_soil_temp[:], _r2)
			resize(&ave_soil_temp, len(_r3))
			copy(ave_soil_temp[:], _r3)
			boundary_layer_conductance = _r4
		}
		if abs(time_of_day_secs - 5.0 * 3600.0) <= min(time_of_day_secs, 5.0 * 3600.0) * 0.0001 {
			copy(morning_soil_temp[0:0 + len(soil_temp)], soil_temp[:])
		}
	}
	min_temp_yesterday = weather_min_t
	max_temp_yesterday = weather_max_t
	return min_soil_temp[:], max_soil_temp[:], soil_temp[:], new_temperature[:], thermal_conductivity[:], ave_soil_temp[:], morning_soil_temp, vol_spec_heat_soil[:], heat_storage[:], thermal_conductance[:], time_of_day_secs, net_radiation, air_temperature, internal_time_step, min_temp_yesterday, boundary_layer_conductance, max_temp_yesterday
}

@(private="file")
to_cum_thickness :: proc(thickness: []f64) -> []f64 {
	layer: int
	cum_thickness := make([dynamic]f64, len(thickness))
	if len(thickness) > 0 {
		cum_thickness[0] = thickness[0]
		for layer = 1; layer < len(thickness); layer += 1 {
			cum_thickness[layer] = thickness[layer] + cum_thickness[layer - 1]
		}
	}
	return cum_thickness[:]
}

@(private="file")
calc_soil_temperature :: proc(soil_temp_io: []f64, weather_tav: f64, clock_today_day_of_year: int, surface_node: int, num_nodes: int, weather_amp: f64, thickness: []f64, weather_latitude: f64) -> []f64 {
	nodes: int
	cumulative_depth: [dynamic]f64
	defer delete(cumulative_depth)
	w: f64
	dh: f64
	zd: f64
	offset: f64
	soil_temp := make([dynamic]f64, num_nodes + 1 + 1)
	defer delete(soil_temp)
	{
		_r0 := to_cum_thickness(thickness[:])
		resize(&cumulative_depth, len(_r0))
		copy(cumulative_depth[:], _r0)
		delete(_r0)
	}
	w = 2 * math.PI / (365.25 * 24 * 3600)
	dh = 0.6
	zd = math.sqrt(2 * dh / w)
	offset = 0.25
	if weather_latitude > 0.0 {
		offset = -0.25
	}
	for nodes = 1; nodes < num_nodes + 1; nodes += 1 {
		soil_temp[nodes] = weather_tav + weather_amp * cr_math.exp(-1 * cumulative_depth[nodes] / zd) * cr_math.sin((f64(clock_today_day_of_year) / 365.0 + offset) * 2.0 * math.PI - cumulative_depth[nodes] / zd)
	}
	copy(soil_temp_io[surface_node:surface_node + num_nodes], soil_temp[0:0 + num_nodes])
	return soil_temp_io
}

@(private="file")
calc_surface_temperature :: proc(weather_mean_t: f64, weather_max_t: f64, water_balance_salb: f64, weather_radn: f64) -> f64 {
	surface_t: f64
	surface_t = (1.0 - water_balance_salb) * (weather_mean_t + (weather_max_t - weather_mean_t) * math.sqrt(max(weather_radn, 0.1) * 23.8846 / 800.0)) + water_balance_salb * weather_mean_t
	return surface_t
}

@(private="file")
values_in_array :: proc(values: []f64, missing_value: f64) -> bool {
	value: f64
	if len(values) != 0 {
		for value_it in values {
			value = value_it
			if value != missing_value && !math.is_nan(value) {
				return true
			}
		}
	}
	return false
}