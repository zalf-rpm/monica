package monica_evapotranspiration

import cr_math "../../support/cr_math"

// - Name: ReferenceEvapotranspiration -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA reference evapotranspiration
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//             Allen RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration. Guidelines for computing crop water requirements. FAO Irrigation and
//             Drainage Paper 56, FAO, Roma
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//             A method following Penman-Monteith as described by the FAO in Allen
//             RG, Pereira LS, Raes D, Smith M. (1998) Crop evapotranspiration.
//             Guidelines for computing crop water requirements. FAO Irrigation and
//             Drainage Paper 56, FAO, Roma
//
//             * ShortDescription: Calculates the MONICA reference evapotranspiration
Reference_Evapotranspiration :: struct {
	height_nn: f64, // height above sea level [m]
}

make_reference_evapotranspiration :: proc(allocator := context.allocator) -> (m: Reference_Evapotranspiration) {
	context.allocator = allocator
	m.height_nn = 0
	return
}

destroy_reference_evapotranspiration :: proc(m: ^Reference_Evapotranspiration) {
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
//             * name: mean_air_temperature
//                           ** description : daily average air temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 100
//                           ** min : -100
//                           ** default : 0
//                           ** unit : °C
//             * name: wind_speed
//                           ** description : wind speed measured at wind speed height
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 9999
//                           ** min : 0
//                           ** default : 0
//                           ** unit : m/s
//             * name: wind_speed_height
//                           ** description : height at which the wind speed has been measured
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 9999
//                           ** min : -9999
//                           ** default : 2
//                           ** unit : m
//             * name: net_radiation
//                           ** description : net radiation
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : MJ/m2
//             * name: stomata_resistance
//                           ** description : stomata resistance (FAO default value is 100)
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 10000
//                           ** min : 0
//                           ** default : 100
//                           ** unit : s/m
//             * name: saturation_vapor_pressure_deficit
//                           ** description : saturation_vapor_pressure_deficit
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : kPa
// - outputs:
//             * name: reference_evapotranspiration
//                           ** description : reference evapotranspiration (ET0)
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
reference_evapotranspiration_calculate_model :: proc(m: ^Reference_Evapotranspiration, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	atmospheric_pressure: f64
	atmospheric_pressure = 101.3 * cr_math.pow((293.0 - 0.0065 * m.height_nn) / 293.0, f64(5.26))
	psycrometer_constant: f64
	psycrometer_constant = 0.000665 * atmospheric_pressure
	saturated_vapour_pressure_slope: f64
	saturated_vapour_pressure_slope = 4098.0 * (0.6108 * cr_math.exp(17.27 * ex.mean_air_temperature / (ex.mean_air_temperature + 237.3))) / ((ex.mean_air_temperature + 237.3) * (ex.mean_air_temperature + 237.3))
	wind_speed_2m: f64
	wind_speed_2m = max(0.5, ex.wind_speed * (4.87 / cr_math.log(67.8 * ex.wind_speed_height - 5.42)))
	aerodynamic_resistance: f64
	aerodynamic_resistance = 208.0 / wind_speed_2m
	surface_resistance: f64
	surface_resistance = a.stomata_resistance / 1.44
	a.reference_evapotranspiration = (0.408 * saturated_vapour_pressure_slope * a.net_radiation + psycrometer_constant * (900.0 / (ex.mean_air_temperature + 273.0)) * wind_speed_2m * a.saturation_vapor_pressure_deficit) / (saturated_vapour_pressure_slope + psycrometer_constant * (1.0 + surface_resistance / aerodynamic_resistance))
	if a.reference_evapotranspiration < 0.0 {
		a.reference_evapotranspiration = 0.0
	}
}