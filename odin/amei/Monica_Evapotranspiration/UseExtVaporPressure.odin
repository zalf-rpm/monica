package monica_evapotranspiration

// - Name: UseExtVaporPressure -Version: 1, -Time step: 1
// - Description:
//             * Title: If Else unit
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: switches between two input values
Use_Ext_Vapor_Pressure :: struct {
}

make_use_ext_vapor_pressure :: proc(allocator := context.allocator) -> (m: Use_Ext_Vapor_Pressure) {
	context.allocator = allocator
	return
}

destroy_use_ext_vapor_pressure :: proc(m: ^Use_Ext_Vapor_Pressure) {
}

// - inputs:
//             * name: use_external_vapor_pressure
//                           ** description : boolean condition to be met
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : BOOLEAN
//                           ** default : false
//                           ** unit : mm
//             * name: external_vapor_pressure
//                           ** description : value to be returned if condition is true
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : kPa
//             * name: internal_vapor_pressure
//                           ** description : value to be returned if condition is false
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : kPa
// - outputs:
//             * name: vapor_pressure
//                           ** description : the output value
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
use_ext_vapor_pressure_calculate_model :: proc(m: ^Use_Ext_Vapor_Pressure, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if a.use_external_vapor_pressure {
		a.vapor_pressure = a.external_vapor_pressure
	} else {
		a.vapor_pressure = a.internal_vapor_pressure
	}
}