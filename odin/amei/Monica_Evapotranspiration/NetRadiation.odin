package monica_evapotranspiration

import "core:math"
import cr_math "../../support/cr_math"

// - Name: NetRadiation -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA net radiation calculation
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//
//             * ShortDescription: None
Net_Radiation :: struct {
	height_nn: f64 `unit:"m"`, // height above sea level
	reference_albedo: f64 `unit:"dimensionless"`, // reference albedo
}

make_net_radiation :: proc(allocator := context.allocator) -> (m: Net_Radiation) {
	context.allocator = allocator
	m.height_nn = 0
	m.reference_albedo = 0
	return
}

destroy_net_radiation :: proc(m: ^Net_Radiation) {
}

// - inputs:
//             * name: height_nn
//                           ** description : height above sea level
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 9999
//                           ** min : -9999
//                           ** default : 0
//                           ** unit : m
//             * name: reference_albedo
//                           ** description : reference albedo
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max : 1
//                           ** min : 0
//                           ** default : 0
//                           ** unit : dimensionless
//             * name: global_radiation
//                           ** description : global radiation
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 0
//                           ** unit : MJ/m2
//             * name: max_air_temperature
//                           ** description : daily maximum air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 100
//                           ** min : -100
//                           ** default : 0
//                           ** unit : °C
//             * name: min_air_temperature
//                           ** description : daily minimum air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 100
//                           ** min : -100
//                           ** default : 0
//                           ** unit : °C
//             * name: extraterrestrial_radiation
//                           ** description : extraterrestrial_radiation
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 50
//                           ** min : 0
//                           ** default : 0
//                           ** unit : MJ/m2
//             * name: vapor_pressure
//                           ** description : vapor pressure
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : kPa
// - outputs:
//             * name: net_radiation
//                           ** description : net_radiation
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
net_radiation_calculate_model :: proc(m: ^Net_Radiation, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	clear_sky_solar_radiation: f64
	clear_sky_solar_radiation = (0.75 + 0.00002 * m.height_nn) * a.extraterrestrial_radiation
	relative_shortwave_radiation: f64
	relative_shortwave_radiation = clear_sky_solar_radiation > 0.0 ? min(ex.global_radiation / clear_sky_solar_radiation, 1.0) : 1.0
	bolzmann_constant: f64 = 0.0000000049
	shortwave_radiation: f64
	shortwave_radiation = (1.0 - m.reference_albedo) * ex.global_radiation
	longwave_radiation: f64
	longwave_radiation = bolzmann_constant * ((cr_math.pow(ex.min_air_temperature + 273.16, f64(4.0)) + cr_math.pow(ex.max_air_temperature + 273.16, f64(4.0))) / 2.0) * (1.35 * relative_shortwave_radiation - 0.35) * (0.34 - 0.14 * math.sqrt(a.vapor_pressure))
	a.net_radiation = shortwave_radiation - longwave_radiation
}