package biomasurfacepartonsoilswatc

import cr_math "../../support/cr_math"

// - Name: SurfaceTemperatureParton -Version: 001, -Time step: 1
// - Description:
//             * Title: SurfaceTemperatureParton model
//             * Authors: simone.bregaglio
//             * Reference: ('http://bioma.jrc.ec.europa.eu/ontology/JRC_MARS_biophysical_domain.owl',)
//             * Institution: University Of Milan
//             * ExtendedDescription: Strategy for the calculation of soil surface temperature with Parton's method. Reference: Parton, W. J. 1984. Predicting soil temperatures in a shortgrass steppe. Soil Science 138:93-101.
//             * ShortDescription: None
Surface_Temperature_Parton :: struct {
}

make_surface_temperature_parton :: proc(allocator := context.allocator) -> (m: Surface_Temperature_Parton) {
	context.allocator = allocator
	return
}

destroy_surface_temperature_parton :: proc(m: ^Surface_Temperature_Parton) {
}

// - inputs:
//             * name: DayLength
//                           ** description : Length of the day
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 24
//                           ** min : 0
//                           ** default : 10
//                           ** unit : h
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
//             * name: AboveGroundBiomass
//                           ** description : Above ground biomass
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 60
//                           ** min : 0
//                           ** default : 3
//                           ** unit : Kg ha-1
//             * name: GlobalSolarRadiation
//                           ** description : Daily global solar radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 15
//                           ** unit : Mj m-2 d-1
// - outputs:
//             * name: SurfaceTemperatureMinimum
//                           ** description : Minimum surface soil temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : auxiliary
//                           ** max : 60
//                           ** min : -60
//                           ** unit : degC
//             * name: SurfaceTemperatureMaximum
//                           ** description : Maximum surface soil temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : auxiliary
//                           ** max : 60
//                           ** min : -60
//                           ** unit : degC
//             * name: SurfaceSoilTemperature
//                           ** description : Average surface soil temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : auxiliary
//                           ** max : 60
//                           ** min : -60
//                           ** unit : degC
surface_temperature_parton_calculate_model :: proc(m: ^Surface_Temperature_Parton, s: ^Surface_Parton_Soil_Swatc_State, s1: ^Surface_Parton_Soil_Swatc_State, r: ^Surface_Parton_Soil_Swatc_Rate, a: ^Surface_Parton_Soil_Swatc_Auxiliary, ex: ^Surface_Parton_Soil_Swatc_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	_agb: f64
	_air_t_max: f64
	_air_tmin: f64
	_solar_rad: f64
	_agb = ex.above_ground_biomass / 10000
	_air_t_max = ex.air_temperature_maximum
	_air_tmin = ex.air_temperature_minimum
	_solar_rad = ex.global_solar_radiation
	if _agb > 0.15 {
		a.surface_temperature_maximum = _air_t_max + (24 * (1 - cr_math.exp(-0.038 * _solar_rad)) + 0.35 * _air_t_max) * (cr_math.exp(-4.8 * _agb) - 0.13)
		a.surface_temperature_minimum = _air_tmin + 6 * _agb - 1.82
	} else {
		a.surface_temperature_maximum = ex.air_temperature_maximum
		a.surface_temperature_minimum = ex.air_temperature_minimum
	}
	a.surface_soil_temperature = 0.41 * a.surface_temperature_maximum + 0.59 * a.surface_temperature_minimum
	if ex.day_length != f64(0) {
		a.surface_soil_temperature = ex.day_length / 24 * _air_t_max + (1 - ex.day_length / 24) * _air_tmin
	}
}