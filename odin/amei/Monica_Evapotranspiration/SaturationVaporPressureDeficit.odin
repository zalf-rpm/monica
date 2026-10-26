package monica_evapotranspiration

// - Name: SaturationVaporPressureDeficit -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA saturation vapor pressure deficit
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//
//             * ShortDescription: Calculates saturation vapor pressure deficit as in the MONICA model
Saturation_Vapor_Pressure_Deficit :: struct {
}

make_saturation_vapor_pressure_deficit :: proc(allocator := context.allocator) -> (m: Saturation_Vapor_Pressure_Deficit) {
	context.allocator = allocator
	return
}

destroy_saturation_vapor_pressure_deficit :: proc(m: ^Saturation_Vapor_Pressure_Deficit) {
}

// - inputs:
//             * name: saturated_vapor_pressure
//                           ** description : saturated vapor pressure
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : kPa
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
//             * name: saturation_vapor_pressure_deficit
//                           ** description : saturation vapor pressure deficit
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : kPa
saturation_vapor_pressure_deficit_calculate_model :: proc(m: ^Saturation_Vapor_Pressure_Deficit, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	a.saturation_vapor_pressure_deficit = a.saturated_vapor_pressure - a.vapor_pressure
}