package biomasurfacepartonsoilswatc

import "core:slice"
import cr_math "../../support/cr_math"

// - Name: SoilTemperatureSWAT -Version: 001, -Time step: 1
// - Description:
//             * Title: SoilTemperatureSWAT model
//             * Authors: simone.bregaglio
//             * Reference: ('http://bioma.jrc.ec.europa.eu/ontology/JRC_MARS_biophysical_domain.owl',)
//             * Institution: University Of Milan
//             * ExtendedDescription: Strategy for the calculation of soil temperature with SWAT method. Reference: Neitsch,S.L., Arnold, J.G., Kiniry, J.R., Williams, J.R., King, K.W. Soil and Water Assessment Tool. Theoretical documentation. Version 2000. http://swatmodel.tamu.edu/media/1290/swat2000theory.pdf
//             * ShortDescription: None
Soil_Temperature_Swat :: struct {
	layer_thickness: [dynamic]f64, // Soil layer thickness [m]
	lag_coefficient: f64, // Lag coefficient that controls the influence of the previous day's temperature on the current day's temperature [dimensionless]
	air_temperature_annual_average: f64, // Annual average air temperature [degC]
	bulk_density: [dynamic]f64, // Bulk density [t m-3]
	soil_profile_depth: f64, // Soil profile depth [m]
}

make_soil_temperature_swat :: proc(allocator := context.allocator) -> (m: Soil_Temperature_Swat) {
	context.allocator = allocator
	m.lag_coefficient = 0.8
	m.air_temperature_annual_average = 15
	m.soil_profile_depth = 3
	return
}

destroy_soil_temperature_swat :: proc(m: ^Soil_Temperature_Swat) {
	delete(m.layer_thickness)
	delete(m.bulk_density)
}

soil_temperature_swat_init :: proc(m: ^Soil_Temperature_Swat, s: ^Surface_Parton_Soil_Swatc_State, s1: ^Surface_Parton_Soil_Swatc_State, r: ^Surface_Parton_Soil_Swatc_Rate, a: ^Surface_Parton_Soil_Swatc_Auxiliary, ex: ^Surface_Parton_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	i: int
	resize(&s.soil_temperature_by_layers, len(m.layer_thickness))
	slice.fill(s.soil_temperature_by_layers[:], 0.0)
	for i = 0; i < len(m.layer_thickness); i += 1 {
		s.soil_temperature_by_layers[i] = f64(15)
	}
}

// - inputs:
//             * name: VolumetricWaterContent
//                           ** description : Volumetric soil water content
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 0.8
//                           ** min : 0
//                           ** default : 0.25
//                           ** unit : m3 m-3
//             * name: SurfaceSoilTemperature
//                           ** description : Average surface soil temperature
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 60
//                           ** min : -60
//                           ** default : 25
//                           ** unit : degC
//             * name: LayerThickness
//                           ** description : Soil layer thickness
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 3
//                           ** min : 0.005
//                           ** default : 0.05
//                           ** unit : m
//             * name: LagCoefficient
//                           ** description : Lag coefficient that controls the influence of the previous day's temperature on the current day's temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1
//                           ** min : 0
//                           ** default : 0.8
//                           ** unit : dimensionless
//             * name: SoilTemperatureByLayers
//                           ** description : Soil temperature of each layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 60
//                           ** min : -60
//                           ** default : 15
//                           ** unit : degC
//             * name: AirTemperatureAnnualAverage
//                           ** description : Annual average air temperature
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : -40
//                           ** default : 15
//                           ** unit : degC
//             * name: BulkDensity
//                           ** description : Bulk density
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 1.8
//                           ** min : 0.9
//                           ** default : 1.3
//                           ** unit : t m-3
//             * name: SoilProfileDepth
//                           ** description : Soil profile depth
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 3
//                           ** unit : m
// - outputs:
//             * name: SoilTemperatureByLayers
//                           ** description : Soil temperature of each layer
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len :
//                           ** max : 60
//                           ** min : -60
//                           ** unit : degC
soil_temperature_swat_calculate_model :: proc(m: ^Soil_Temperature_Swat, s: ^Surface_Parton_Soil_Swatc_State, s1: ^Surface_Parton_Soil_Swatc_State, r: ^Surface_Parton_Soil_Swatc_Rate, a: ^Surface_Parton_Soil_Swatc_Auxiliary, ex: ^Surface_Parton_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	i: int
	_soil_profile_depthmm: f64
	_total_water_contentmm: f64
	_maximum_dumping_depth: f64
	_dumping_depth: f64
	_scaling_factor: f64
	_depth_bottom: f64
	_ratio_center: f64
	_depth_factor: f64
	_depth_center_layer: f64
	_soil_profile_depthmm = m.soil_profile_depth * 1000
	_total_water_contentmm = f64(0)
	for i = 0; i < len(m.layer_thickness); i += 1 {
		_total_water_contentmm = _total_water_contentmm + ex.volumetric_water_content[i] * m.layer_thickness[i]
	}
	_total_water_contentmm = _total_water_contentmm * 1000
	_maximum_dumping_depth = f64(0)
	_dumping_depth = f64(0)
	_scaling_factor = f64(0)
	_depth_bottom = f64(0)
	_ratio_center = f64(0)
	_depth_factor = f64(0)
	_depth_center_layer = m.layer_thickness[0] * 1000 / 2
	_maximum_dumping_depth = 1000 + 2500 * m.bulk_density[0] / (m.bulk_density[0] + 686 * cr_math.exp(-5.63 * m.bulk_density[0]))
	_scaling_factor = _total_water_contentmm / ((0.356 - 0.144 * m.bulk_density[0]) * _soil_profile_depthmm)
	_dumping_depth = _maximum_dumping_depth * cr_math.exp(cr_math.log(500 / _maximum_dumping_depth) * cr_math.pow((1 - _scaling_factor) / (1 + _scaling_factor), f64(2)))
	_ratio_center = _depth_center_layer / _dumping_depth
	_depth_factor = _ratio_center / (_ratio_center + cr_math.exp(-0.867 - 2.078 * _ratio_center))
	s.soil_temperature_by_layers[0] = m.lag_coefficient * s.soil_temperature_by_layers[0] + (1 - m.lag_coefficient) * (_depth_factor * (m.air_temperature_annual_average - a.surface_soil_temperature) + a.surface_soil_temperature)
	for i = 1; i < len(m.layer_thickness); i += 1 {
		_depth_bottom = _depth_bottom + m.layer_thickness[i - 1] * 1000
		_depth_center_layer = _depth_bottom + m.layer_thickness[i] * 1000 / 2
		_maximum_dumping_depth = 1000 + 2500 * m.bulk_density[i] / (m.bulk_density[i] + 686 * cr_math.exp(-5.63 * m.bulk_density[i]))
		_scaling_factor = _total_water_contentmm / ((0.356 - 0.144 * m.bulk_density[i]) * _soil_profile_depthmm)
		_dumping_depth = _maximum_dumping_depth * cr_math.exp(cr_math.log(500 / _maximum_dumping_depth) * cr_math.pow((1 - _scaling_factor) / (1 + _scaling_factor), f64(2)))
		_ratio_center = _depth_center_layer / _dumping_depth
		_depth_factor = _ratio_center / (_ratio_center + cr_math.exp(-0.867 - 2.078 * _ratio_center))
		s.soil_temperature_by_layers[i] = m.lag_coefficient * s.soil_temperature_by_layers[i] + (1 - m.lag_coefficient) * (_depth_factor * (m.air_temperature_annual_average - a.surface_soil_temperature) + a.surface_soil_temperature)
	}
}