package monica_soiltemp

// - Name: WithSnowSoilSurfaceTemperature -Version: 1, -Time step: 1
// - Description:
//             * Title: Soil surface temperature with potential snow cover
//             * Authors: Michael Berg-Mohnicke
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: It calculates the soil surface temperature taking a potential snow cover into account
//
With_Snow_Soil_Surface_Temperature :: struct {
}

make_with_snow_soil_surface_temperature :: proc(allocator := context.allocator) -> (m: With_Snow_Soil_Surface_Temperature) {
	context.allocator = allocator
	return
}

destroy_with_snow_soil_surface_temperature :: proc(m: ^With_Snow_Soil_Surface_Temperature) {
}

// - inputs:
//             * name: noSnowSoilSurfaceTemperature
//                           ** description : soilSurfaceTemperature without snow
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.0
//                           ** unit : °C
//             * name: soilSurfaceTemperatureBelowSnow
//                           ** description : soilSurfaceTemperature below snow cover
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 0.0
//                           ** unit : °C
//             * name: hasSnowCover
//                           ** description : is soil covered by snow
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : BOOLEAN
//                           ** max :
//                           ** min :
//                           ** default : false
//                           ** unit : dimensionless
// - outputs:
//             * name: soilSurfaceTemperature
//                           ** description : soilSurfaceTemperature
//                           ** variablecategory : state
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** unit : °C
with_snow_soil_surface_temperature_calculate_model :: proc(m: ^With_Snow_Soil_Surface_Temperature, s: ^Soil_Temperature_Comp_State, s1: ^Soil_Temperature_Comp_State, r: ^Soil_Temperature_Comp_Rate, a: ^Soil_Temperature_Comp_Auxiliary, ex: ^Soil_Temperature_Comp_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if ex.has_snow_cover {
		s.soil_surface_temperature = ex.soil_surface_temperature_below_snow
	} else {
		s.soil_surface_temperature = a.no_snow_soil_surface_temperature
	}
}