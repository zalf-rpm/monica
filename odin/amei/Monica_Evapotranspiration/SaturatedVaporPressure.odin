package monica_evapotranspiration

import cr_math "../../support/cr_math"

// - Name: SaturatedVaporPressure -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA saturated vapor pressure
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//
//             * ShortDescription: Calculates saturated vapor pressure as in the MONICA model
Saturated_Vapor_Pressure :: struct {
}

make_saturated_vapor_pressure :: proc(allocator := context.allocator) -> (m: Saturated_Vapor_Pressure) {
	context.allocator = allocator
	return
}

destroy_saturated_vapor_pressure :: proc(m: ^Saturated_Vapor_Pressure) {
}

// - inputs:
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
//             * name: relative_humidity
//                           ** description : relative humidity
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 1
//                           ** min : 0
//                           ** default : 0
//                           ** unit : fraction
// - outputs:
//             * name: saturated_vapor_pressure
//                           ** description : saturated vapor pressure
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : kPa
//             * name: vapor_pressure
//                           ** description : vapor pressure from relative humidity and saturated vapor pressure
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : kPa
saturated_vapor_pressure_calculate_model :: proc(m: ^Saturated_Vapor_Pressure, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	saturated_vapor_pressure_max: f64
	saturated_vapor_pressure_max = 0.6108 * cr_math.exp(17.27 * ex.max_air_temperature / (237.3 + ex.max_air_temperature))
	saturated_vapor_pressure_min: f64
	saturated_vapor_pressure_min = 0.6108 * cr_math.exp(17.27 * ex.min_air_temperature / (237.3 + ex.min_air_temperature))
	a.saturated_vapor_pressure = (saturated_vapor_pressure_max + saturated_vapor_pressure_min) / 2.0
	if ex.relative_humidity <= 0.0 {
		a.vapor_pressure = saturated_vapor_pressure_min
	} else {
		a.vapor_pressure = ex.relative_humidity * a.saturated_vapor_pressure
	}
}