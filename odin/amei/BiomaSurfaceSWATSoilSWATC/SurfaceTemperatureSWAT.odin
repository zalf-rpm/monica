package biomasurfaceswatsoilswatc

import cr_math "../../support/cr_math"

// - Name: SurfaceTemperatureSWAT -Version: 001, -Time step: 1
// - Description:
//             * Title: SurfaceTemperatureSWAT model
//             * Authors: simone.bregaglio
//             * Reference: http://bioma.jrc.ec.europa.eu/ontology/JRC_MARS_biophysical_domain.owl
//             * Institution: University Of Milan
//             * ExtendedDescription: Strategy for the calculation of surface soil temperature with SWAT method. Reference: Neitsch,S.L., Arnold, J.G., Kiniry, J.R., Williams, J.R., King, K.W. Soil and Water Assessment Tool. Theoretical documentation. Version 2000. http://swatmodel.tamu.edu/media/1290/swat2000theory.pdf
//             * ShortDescription: Strategy for the calculation of surface soil temperature with SWAT method
Surface_Temperature_Swat :: struct {
}

make_surface_temperature_swat :: proc(allocator := context.allocator) -> (m: Surface_Temperature_Swat) {
	context.allocator = allocator
	return
}

destroy_surface_temperature_swat :: proc(m: ^Surface_Temperature_Swat) {
}

// - inputs:
//             * name: GlobalSolarRadiation
//                           ** description : Daily global solar radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 15
//                           ** unit : Mj m-2 d-1
//             * name: SoilTemperatureByLayers
//                           ** description : Soil temperature of each layer
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 60
//                           ** min : -60
//                           ** default : 15
//                           ** unit :
//             * name: AirTemperatureMaximum
//                           ** description : Maximum daily air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 60
//                           ** min : -40
//                           ** default : 15
//                           ** unit :
//             * name: AirTemperatureMinimum
//                           ** description : Minimum daily air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : -60
//                           ** default : 5
//                           ** unit :
//             * name: Albedo
//                           ** description : Albedo of soil
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 1
//                           ** min : 0
//                           ** default : 0.2
//                           ** unit : unitless
//             * name: AboveGroundBiomass
//                           ** description : Above ground biomass
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 60
//                           ** min : 0
//                           ** default : 3
//                           ** unit : Kg ha-1
//             * name: WaterEquivalentOfSnowPack
//                           ** description : Water equivalent of snow pack
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 1000
//                           ** min : 0
//                           ** default : 10
//                           ** unit : mm
// - outputs:
//             * name: SurfaceSoilTemperature
//                           ** description : Average surface soil temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : auxiliary
//                           ** max : 60
//                           ** min : -60
//                           ** unit : degC
surface_temperature_swat_calculate_model :: proc(m: ^Surface_Temperature_Swat, s: ^Surface_Swat_Soil_Swatc_State, s1: ^Surface_Swat_Soil_Swatc_State, r: ^Surface_Swat_Soil_Swatc_Rate, a: ^Surface_Swat_Soil_Swatc_Auxiliary, ex: ^Surface_Swat_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	_tavg: f64
	_hterm: f64
	_tbare: f64
	_weighting_cover: f64
	_weighting_snow: f64
	_weighting_actual: f64
	_tavg = (ex.air_temperature_maximum + ex.air_temperature_minimum) / 2
	_hterm = (ex.global_solar_radiation * (1 - ex.albedo) - 14) / 20
	_tbare = _tavg + _hterm * (ex.air_temperature_maximum - ex.air_temperature_minimum) / 2
	_weighting_cover = a.above_ground_biomass / (a.above_ground_biomass + cr_math.exp(7.563 - 0.0001297 * a.above_ground_biomass))
	_weighting_snow = ex.water_equivalent_of_snow_pack / (ex.water_equivalent_of_snow_pack + cr_math.exp(6.055 - 0.3002 * ex.water_equivalent_of_snow_pack))
	_weighting_actual = max(_weighting_cover, _weighting_snow)
	a.surface_soil_temperature = _weighting_actual * s.soil_temperature_by_layers[0] + (1 - _weighting_actual) * _tbare
}