package monica_soiltemp

import "core:slice"
import cr_math "../../support/cr_math"

// - Name: SoilTemperature -Version: 1, -Time step: 1
// - Description:
//             * Title: Model of soil temperature
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the soil temperature at all soil layers
Soil_Temperature :: struct {
	no_of_soil_layers: int `unit:"dimensionless"`, // noOfSoilLayers
	no_of_temp_layers: int `unit:"dimensionless"`, // noOfTempLayers=noOfSoilLayers+2
	no_of_temp_layers_plus1: int `unit:"dimensionless"`, // for matrixSecondaryDiagonal
	time_step: f64 `unit:"dimensionless"`, // timeStep
	soil_moisture_const: [dynamic]f64 `unit:"m**3/m**3"`, // constant soilmoisture during the model run
	base_temp: f64 `unit:"°C"`, // baseTemperature
	initial_surface_temp: f64 `unit:"°C"`, // initialSurfaceTemperature
	density_air: f64 `unit:"kg/m**3"`, // DensityAir
	specific_heat_capacity_air: f64 `unit:"J/kg/K"`, // SpecificHeatCapacityAir
	density_humus: f64 `unit:"kg/m**3"`, // DensityHumus
	specific_heat_capacity_humus: f64 `unit:"J/kg/K"`, // SpecificHeatCapacityHumus
	density_water: f64 `unit:"kg/m**3"`, // DensityWater
	specific_heat_capacity_water: f64 `unit:"J/kg/K"`, // SpecificHeatCapacityWater
	quartz_raw_density: f64 `unit:"kg/m**3"`, // QuartzRawDensity
	specific_heat_capacity_quartz: f64 `unit:"J/kg/K"`, // SpecificHeatCapacityQuartz
	n_tau: f64 `unit:"?"`, // NTau
	layer_thickness: [dynamic]f64 `unit:"m"`, // layerThickness
	soil_bulk_density: [dynamic]f64 `unit:"kg/m**3"`, // bulkDensity
	saturation: [dynamic]f64 `unit:"m**3/m**3"`, // saturation
	soil_organic_matter: [dynamic]f64 `unit:"m**3/m**3"`, // soilOrganicMatter
}

make_soil_temperature :: proc(allocator := context.allocator) -> (m: Soil_Temperature) {
	context.allocator = allocator
	m.no_of_soil_layers = 20
	m.no_of_temp_layers = 22
	m.no_of_temp_layers_plus1 = 23
	m.time_step = 1.0
	m.base_temp = 9.5
	m.initial_surface_temp = 10.0
	m.density_air = 1.25
	m.specific_heat_capacity_air = 1005.0
	m.density_humus = 1300.0
	m.specific_heat_capacity_humus = 1920.0
	m.density_water = 1000.0
	m.specific_heat_capacity_water = 4192.0
	m.quartz_raw_density = 2650.0
	m.specific_heat_capacity_quartz = 750.0
	m.n_tau = 0.65
	return
}

destroy_soil_temperature :: proc(m: ^Soil_Temperature) {
	delete(m.soil_moisture_const)
	delete(m.layer_thickness)
	delete(m.soil_bulk_density)
	delete(m.saturation)
	delete(m.soil_organic_matter)
}

soil_temperature_init :: proc(m: ^Soil_Temperature, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.soil_surface_temperature = 0.0
	resize(&s.soil_temperature, m.no_of_temp_layers)
	resize(&s.v, m.no_of_temp_layers)
	resize(&s.b, m.no_of_temp_layers)
	resize(&s.volume_matrix, m.no_of_temp_layers)
	resize(&s.volume_matrix_old, m.no_of_temp_layers)
	resize(&s.matrix_primary_diagonal, m.no_of_temp_layers)
	resize(&s.matrix_secondary_diagonal, m.no_of_temp_layers_plus1)
	resize(&s.heat_conductivity, m.no_of_temp_layers)
	resize(&s.heat_conductivity_mean, m.no_of_temp_layers)
	resize(&s.heat_capacity, m.no_of_temp_layers)
	resize(&s.solution, m.no_of_temp_layers)
	resize(&s.matrix_diagonal, m.no_of_temp_layers)
	resize(&s.matrix_lower_triangle, m.no_of_temp_layers)
	resize(&s.heat_flow, m.no_of_temp_layers)
	resize(&s.soil_temperature, m.no_of_temp_layers)
	slice.fill(s.soil_temperature[:], 0.0)
	resize(&s.v, m.no_of_temp_layers)
	slice.fill(s.v[:], 0.0)
	resize(&s.b, m.no_of_temp_layers)
	slice.fill(s.b[:], 0.0)
	resize(&s.volume_matrix, m.no_of_temp_layers)
	slice.fill(s.volume_matrix[:], 0.0)
	resize(&s.volume_matrix_old, m.no_of_temp_layers)
	slice.fill(s.volume_matrix_old[:], 0.0)
	resize(&s.matrix_primary_diagonal, m.no_of_temp_layers)
	slice.fill(s.matrix_primary_diagonal[:], 0.0)
	resize(&s.matrix_secondary_diagonal, m.no_of_temp_layers_plus1)
	slice.fill(s.matrix_secondary_diagonal[:], 0.0)
	resize(&s.heat_conductivity, m.no_of_temp_layers)
	slice.fill(s.heat_conductivity[:], 0.0)
	resize(&s.heat_conductivity_mean, m.no_of_temp_layers)
	slice.fill(s.heat_conductivity_mean[:], 0.0)
	resize(&s.heat_capacity, m.no_of_temp_layers)
	slice.fill(s.heat_capacity[:], 0.0)
	resize(&s.solution, m.no_of_temp_layers)
	slice.fill(s.solution[:], 0.0)
	resize(&s.matrix_diagonal, m.no_of_temp_layers)
	slice.fill(s.matrix_diagonal[:], 0.0)
	resize(&s.matrix_lower_triangle, m.no_of_temp_layers)
	slice.fill(s.matrix_lower_triangle[:], 0.0)
	resize(&s.heat_flow, m.no_of_temp_layers)
	slice.fill(s.heat_flow[:], 0.0)
	ground_layer: int
	bottom_layer: int
	lti_1: f64
	lti: f64
	ts: f64
	dw: f64
	cw: f64
	dq: f64
	cq: f64
	da: f64
	ca: f64
	dh: f64
	ch: f64
	sbdi: f64
	smi: f64
	sati: f64
	somi: f64
	hci_1: f64
	hci: f64
	i: int
	for i = 0; i < m.no_of_soil_layers; i += 1 {
		s.soil_temperature[i] = (1.0 - f64(i) / f64(m.no_of_soil_layers)) * m.initial_surface_temp + f64(i) / f64(m.no_of_soil_layers) * m.base_temp
	}
	ground_layer = m.no_of_temp_layers - 2
	bottom_layer = m.no_of_temp_layers - 1
	m.layer_thickness[ground_layer] = 2.0 * m.layer_thickness[ground_layer - 1]
	m.layer_thickness[bottom_layer] = 1.0
	s.soil_temperature[ground_layer] = (s.soil_temperature[ground_layer - 1] + m.base_temp) * 0.5
	s.soil_temperature[bottom_layer] = m.base_temp
	s.v[0] = m.layer_thickness[0]
	s.b[0] = 2.0 / m.layer_thickness[0]
	for i = 1; i < m.no_of_temp_layers; i += 1 {
		lti_1 = m.layer_thickness[i - 1]
		lti = m.layer_thickness[i]
		s.b[i] = 2.0 / (lti + lti_1)
		s.v[i] = lti * m.n_tau
	}
	ts = m.time_step
	dw = m.density_water
	cw = m.specific_heat_capacity_water
	dq = m.quartz_raw_density
	cq = m.specific_heat_capacity_quartz
	da = m.density_air
	ca = m.specific_heat_capacity_air
	dh = m.density_humus
	ch = m.specific_heat_capacity_humus
	for i = 0; i < m.no_of_soil_layers; i += 1 {
		sbdi = m.soil_bulk_density[i]
		smi = m.soil_moisture_const[i]
		s.heat_conductivity[i] = (3.0 * (sbdi / 1000.0) - 1.7) * 0.001 / (1.0 + (11.5 - 5.0 * (sbdi / 1000.0)) * cr_math.exp(-50.0 * cr_math.pow(smi / (sbdi / 1000.0), f64(1.5)))) * 86400.0 * ts * 100.0 * 4.184
		sati = m.saturation[i]
		somi = m.soil_organic_matter[i] / da * sbdi
		s.heat_capacity[i] = smi * dw * cw + (sati - smi) * da * ca + somi * dh * ch + (1.0 - sati - somi) * dq * cq
	}
	s.heat_capacity[ground_layer] = s.heat_capacity[ground_layer - 1]
	s.heat_capacity[bottom_layer] = s.heat_capacity[ground_layer]
	s.heat_conductivity[ground_layer] = s.heat_conductivity[ground_layer - 1]
	s.heat_conductivity[bottom_layer] = s.heat_conductivity[ground_layer]
	s.soil_surface_temperature = m.initial_surface_temp
	s.heat_conductivity_mean[0] = s.heat_conductivity[0]
	for i = 1; i < m.no_of_temp_layers; i += 1 {
		lti_1 = m.layer_thickness[i - 1]
		lti = m.layer_thickness[i]
		hci_1 = s.heat_conductivity[i - 1]
		hci = s.heat_conductivity[i]
		s.heat_conductivity_mean[i] = (lti_1 * hci_1 + lti * hci) / (lti + lti_1)
	}
	for i = 0; i < m.no_of_temp_layers; i += 1 {
		s.volume_matrix[i] = s.v[i] * s.heat_capacity[i]
		s.volume_matrix_old[i] = s.volume_matrix[i]
		s.matrix_secondary_diagonal[i] = -s.b[i] * s.heat_conductivity_mean[i]
	}
	s.matrix_secondary_diagonal[bottom_layer + 1] = 0.0
	for i = 0; i < m.no_of_temp_layers; i += 1 {
		s.matrix_primary_diagonal[i] = s.volume_matrix[i] - s.matrix_secondary_diagonal[i] - s.matrix_secondary_diagonal[i + 1]
	}
}

// - inputs:
//             * name: noOfSoilLayers
//                           ** description : noOfSoilLayers
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 20
//                           ** unit : dimensionless
//             * name: noOfTempLayers
//                           ** description : noOfTempLayers=noOfSoilLayers+2
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 22
//                           ** unit : dimensionless
//             * name: noOfTempLayersPlus1
//                           ** description : for matrixSecondaryDiagonal
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max :
//                           ** min :
//                           ** default : 23
//                           ** unit : dimensionless
//             * name: soilSurfaceTemperature
//                           ** description : current soilSurfaceTemperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 80.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : °C
//             * name: timeStep
//                           ** description : timeStep
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1.0
//                           ** unit : dimensionless
//             * name: soilMoistureConst
//                           ** description : constant soilmoisture during the model run
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfSoilLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m**3/m**3
//             * name: baseTemp
//                           ** description : baseTemperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 9.5
//                           ** unit : °C
//             * name: initialSurfaceTemp
//                           ** description : initialSurfaceTemperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 10.0
//                           ** unit : °C
//             * name: densityAir
//                           ** description : DensityAir
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1.25
//                           ** unit : kg/m**3
//             * name: specificHeatCapacityAir
//                           ** description : SpecificHeatCapacityAir
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1005.0
//                           ** unit : J/kg/K
//             * name: densityHumus
//                           ** description : DensityHumus
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1300.0
//                           ** unit : kg/m**3
//             * name: specificHeatCapacityHumus
//                           ** description : SpecificHeatCapacityHumus
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1920.0
//                           ** unit : J/kg/K
//             * name: densityWater
//                           ** description : DensityWater
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 1000.0
//                           ** unit : kg/m**3
//             * name: specificHeatCapacityWater
//                           ** description : SpecificHeatCapacityWater
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 4192.0
//                           ** unit : J/kg/K
//             * name: quartzRawDensity
//                           ** description : QuartzRawDensity
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 2650.0
//                           ** unit : kg/m**3
//             * name: specificHeatCapacityQuartz
//                           ** description : SpecificHeatCapacityQuartz
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 750.0
//                           ** unit : J/kg/K
//             * name: nTau
//                           ** description : NTau
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.65
//                           ** unit : ?
//             * name: layerThickness
//                           ** description : layerThickness
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m
//             * name: soilBulkDensity
//                           ** description : bulkDensity
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfSoilLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : kg/m**3
//             * name: saturation
//                           ** description : saturation
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfSoilLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m**3/m**3
//             * name: soilOrganicMatter
//                           ** description : soilOrganicMatter
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfSoilLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : m**3/m**3
//             * name: soilTemperature
//                           ** description : soilTemperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: V
//                           ** description : V
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: B
//                           ** description : B
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: volumeMatrix
//                           ** description : volumeMatrix
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: volumeMatrixOld
//                           ** description : volumeMatrixOld
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: matrixPrimaryDiagonal
//                           ** description : matrixPrimaryDiagonal
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: matrixSecondaryDiagonal
//                           ** description : matrixSecondaryDiagonal
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayersPlus1
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: heatConductivity
//                           ** description : heatConductivity
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: heatConductivityMean
//                           ** description : heatConductivityMean
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: heatCapacity
//                           ** description : heatCapacity
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: solution
//                           ** description : solution
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: matrixDiagonal
//                           ** description : matrixDiagonal
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: matrixLowerTriangle
//                           ** description : matrixLowerTriangle
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
//             * name: heatFlow
//                           ** description : heatFlow
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : noOfTempLayers
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : °C
// - outputs:
//             * name: soilTemperature
//                           ** description : soilTemperature next day
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len : 22
//                           ** max :
//                           ** min :
//                           ** unit : °C
soil_temperature_calculate_model :: proc(m: ^Soil_Temperature, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	ground_layer: int
	bottom_layer: int
	i: int
	j: int
	j_1: int
	ground_layer = m.no_of_temp_layers - 2
	bottom_layer = m.no_of_temp_layers - 1
	s.heat_flow[0] = s.soil_surface_temperature * s.b[0] * s.heat_conductivity_mean[0]
	for i = 0; i < m.no_of_temp_layers; i += 1 {
		s.solution[i] = (s.volume_matrix_old[i] + (s.volume_matrix[i] - s.volume_matrix_old[i]) / m.layer_thickness[i]) * s.soil_temperature[i] + s.heat_flow[i]
	}
	s.matrix_diagonal[0] = s.matrix_primary_diagonal[0]
	for i = 1; i < m.no_of_temp_layers; i += 1 {
		s.matrix_lower_triangle[i] = s.matrix_secondary_diagonal[i] / s.matrix_diagonal[i - 1]
		s.matrix_diagonal[i] = s.matrix_primary_diagonal[i] - s.matrix_lower_triangle[i] * s.matrix_secondary_diagonal[i]
	}
	for i = 1; i < m.no_of_temp_layers; i += 1 {
		s.solution[i] = s.solution[i] - s.matrix_lower_triangle[i] * s.solution[i - 1]
	}
	s.solution[bottom_layer] = s.solution[bottom_layer] / s.matrix_diagonal[bottom_layer]
	for i = 0; i < bottom_layer; i += 1 {
		j = bottom_layer - 1 - i
		j_1 = j + 1
		s.solution[j] = s.solution[j] / s.matrix_diagonal[j] - s.matrix_lower_triangle[j_1] * s.solution[j_1]
	}
	for i = 0; i < m.no_of_temp_layers; i += 1 {
		s.soil_temperature[i] = s.solution[i]
	}
	for i = 0; i < m.no_of_soil_layers; i += 1 {
		s.volume_matrix_old[i] = s.volume_matrix[i]
	}
	s.volume_matrix_old[ground_layer] = s.volume_matrix[ground_layer]
	s.volume_matrix_old[bottom_layer] = s.volume_matrix[bottom_layer]
}