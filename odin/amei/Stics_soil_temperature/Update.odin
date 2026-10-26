package stics_soil_temperature

// - Name: update -Version: 1.0, -Time step: 1
// - Description:
//             * Title: update soil temp model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Update :: struct {
}

make_update :: proc(allocator := context.allocator) -> (m: Update) {
	context.allocator = allocator
	return
}

destroy_update :: proc(m: ^Update) {
}

// - inputs:
//             * name: canopy_temp_avg
//                           ** description : current canopy mean temperature
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: temp_profile
//                           ** description : current soil profile temperature (for 1 cm layers)
//                           ** inputtype : variable
//                           ** variablecategory : state
//                           ** datatype : DOUBLEARRAY
//                           ** len :
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default :
//                           ** unit : degC
// - outputs:
//             * name: prev_canopy_temp
//                           ** description : previous crop temperature
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 50.0
//                           ** min : 0.0
//                           ** unit : degC
//             * name: prev_temp_profile
//                           ** description : previous soil temperature profile (for 1 cm layers)
//                           ** datatype : DOUBLEARRAY
//                           ** variablecategory : state
//                           ** len : 1
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** unit : degC
update_calculate_model :: proc(m: ^Update, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	resize(&s.prev_temp_profile, 1)
	n: int
	s.prev_canopy_temp = s.canopy_temp_avg
	n = len(s.temp_profile)
	resize(&s.prev_temp_profile, n)
	resize(&s.prev_temp_profile, len(s.temp_profile[:]))
	copy(s.prev_temp_profile[:], s.temp_profile[:])
}