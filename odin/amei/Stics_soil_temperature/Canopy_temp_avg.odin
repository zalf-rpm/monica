package stics_soil_temperature

// - Name: canopy_temp_avg -Version: 1.0, -Time step: 1
// - Description:
//             * Title: canopy_temp_avg model
//             * Authors: None
//             * Reference: doi:http://dx.doi.org/10.1016/j.agrformet.2014.05.002
//             * Institution: INRAE
//             * ExtendedDescription: None
//             * ShortDescription: None
Canopy_Temp_Avg :: struct {
}

make_canopy_temp_avg :: proc(allocator := context.allocator) -> (m: Canopy_Temp_Avg) {
	context.allocator = allocator
	return
}

destroy_canopy_temp_avg :: proc(m: ^Canopy_Temp_Avg) {
}

// - inputs:
//             * name: min_canopy_temp
//                           ** description : current minimum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
//             * name: max_canopy_temp
//                           ** description : current maximum temperature
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max : 50.0
//                           ** min : -50.0
//                           ** default : 0.0
//                           ** unit : degC
// - outputs:
//             * name: canopy_temp_avg
//                           ** description : current temperature amplitude
//                           ** datatype : DOUBLE
//                           ** variablecategory : state
//                           ** max : 100.0
//                           ** min : 0.0
//                           ** unit : degC
canopy_temp_avg_calculate_model :: proc(m: ^Canopy_Temp_Avg, s: ^Soil_Temp_State, s1: ^Soil_Temp_State, r: ^Soil_Temp_Rate, a: ^Soil_Temp_Auxiliary, ex: ^Soil_Temp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	s.canopy_temp_avg = (ex.max_canopy_temp + ex.min_canopy_temp) / 2
}