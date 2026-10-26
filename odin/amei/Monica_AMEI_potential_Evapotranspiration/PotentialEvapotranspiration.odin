package monica_amei_potential_evapotranspiration

// - Name: PotentialEvapotranspiration -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA potential evapotranspiration calculation
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the MONICA potential evapotranspiration
Potential_Evapotranspiration :: struct {
	potential_evapotranspiration_cap: f64, // cap ET0 to this value [mm]
}

make_potential_evapotranspiration :: proc(allocator := context.allocator) -> (m: Potential_Evapotranspiration) {
	context.allocator = allocator
	m.potential_evapotranspiration_cap = 6.5
	return
}

destroy_potential_evapotranspiration :: proc(m: ^Potential_Evapotranspiration) {
}

// - inputs:
//             * name: potential_evapotranspiration_cap
//                           ** description : cap ET0 to this value
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 6.5
//                           ** unit : mm
//             * name: kc_factor
//                           ** description : crop coefficient ETc/ET0
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0.75
//                           ** unit : dimensionless
//             * name: developmental_stage
//                           ** description : MONICA crop developmental stage
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : INT
//                           ** max : 6
//                           ** min : 0
//                           ** default : 0
//                           ** unit : dimensionless
//             * name: crop_remaining_evapotranspiration
//                           ** description : crop remaining evapotranspiration
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default :
//                           ** unit : mm
//             * name: reference_evapotranspiration
//                           ** description : ET0 either from external ET0 or crop specific ET0
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 0
//                           ** unit : mm
// - outputs:
//             * name: potential_evapotranspiration
//                           ** description : the potential evapotranspiration
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : mm
potential_evapotranspiration_calculate_model :: proc(m: ^Potential_Evapotranspiration, s: ^Amei_Pot_Et_State, s1: ^Amei_Pot_Et_State, r: ^Amei_Pot_Et_Rate, a: ^Amei_Pot_Et_Auxiliary, ex: ^Amei_Pot_Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if ex.developmental_stage > 0 {
		a.potential_evapotranspiration = ex.crop_remaining_evapotranspiration
	} else {
		a.potential_evapotranspiration = a.reference_evapotranspiration * ex.kc_factor
	}
	if a.potential_evapotranspiration > m.potential_evapotranspiration_cap {
		a.potential_evapotranspiration = m.potential_evapotranspiration_cap
	}
}