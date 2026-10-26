package monica_evapotranspiration

// - Name: CalcStomataResistance -Version: 1, -Time step: 1
// - Description:
//             * Title: If Else unit
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: switches between two input values
Calc_Stomata_Resistance :: struct {
}

make_calc_stomata_resistance :: proc(allocator := context.allocator) -> (m: Calc_Stomata_Resistance) {
	context.allocator = allocator
	return
}

destroy_calc_stomata_resistance :: proc(m: ^Calc_Stomata_Resistance) {
}

// - inputs:
//             * name: calc_stomata_resistance
//                           ** description : boolean condition to be met
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : BOOLEAN
//                           ** default : false
//                           ** unit :
//             * name: calculated_stomata_resistance
//                           ** description : value to be returned if condition is true
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 10000
//                           ** min : 0
//                           ** default : 100
//                           ** unit : s/m
//             * name: fixed_stomata_resistance
//                           ** description : value to be returned if condition is false
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 10000
//                           ** min : 0
//                           ** default : 100
//                           ** unit : s/m
// - outputs:
//             * name: stomata_resistance
//                           ** description : the output value
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 10000
//                           ** min : 0
//                           ** unit : s/m
calc_stomata_resistance_calculate_model :: proc(m: ^Calc_Stomata_Resistance, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if a.calc_stomata_resistance {
		a.stomata_resistance = a.calculated_stomata_resistance
	} else {
		a.stomata_resistance = a.fixed_stomata_resistance
	}
}