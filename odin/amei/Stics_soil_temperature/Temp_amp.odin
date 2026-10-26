package stics_soil_temperature

// - Name: temp_amp -Version: 1.0, -Time step: 1
// - Description:
//             * Title: temp_amp model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Temp_Amp :: struct {
}

make_temp_amp :: proc(allocator := context.allocator) -> (m: Temp_Amp) {
	context.allocator = allocator
	return
}

destroy_temp_amp :: proc(m: ^Temp_Amp) {
}

// - inputs:
//             * name: min_temp
//                           ** description : current minimum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: max_temp
//                           ** description : current maximum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
// - outputs:
//             * name: temp_amp
//                           ** description : current temperature amplitude
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 100.0
//                           ** min : 0.0
//                           ** unit : degC
temp_amp_calculate_model :: proc(m: ^Temp_Amp, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.temp_amp = ex.max_temp - ex.min_temp
}