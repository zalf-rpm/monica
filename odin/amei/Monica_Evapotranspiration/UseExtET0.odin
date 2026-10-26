package monica_evapotranspiration

// - Name: UseExtET0 -Version: 1, -Time step: 1
// - Description:
//             * Title: If Else unit
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: switches between two input values
Use_Ext_Et0 :: struct {
}

make_use_ext_et0 :: proc(allocator := context.allocator) -> (m: Use_Ext_Et0) {
	context.allocator = allocator
	return
}

destroy_use_ext_et0 :: proc(m: ^Use_Ext_Et0) {
}

// - inputs:
//             * name: use_external_et0
//                           ** description : boolean condition to be met
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : BOOLEAN
//                           ** default : false
//                           ** unit :
//             * name: external_et0
//                           ** description : value to be returned if condition is true
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
//             * name: internal_et0
//                           ** description : value to be returned if condition is false
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
// - outputs:
//             * name: et0
//                           ** description : the output value
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
use_ext_et0_calculate_model :: proc(m: ^Use_Ext_Et0, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if a.use_external_et0 {
		a.et0 = a.external_et0
	} else {
		a.et0 = a.internal_et0
	}
}