package monica_evapotranspiration

// - Name: StomataResistance -Version: 1, -Time step: 1
// - Description:
//             * Title: MONICA stomata resistance
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference:
//
//             * Institution: ZALF e.V.
//             * ExtendedDescription:
//
//             * ShortDescription: Calculates the MONICA reference evapotranspiration
Stomata_Resistance :: struct {
	saturation_beta: f64, // Original: Yu et al. 2001; beta = 3.5
	stomata_conductance_alpha: f64, // Original: Yu et al. 2001; alpha = 0.06 mol/m2/s [mmol/m2/s]
	carboxylation_pathway: int, // Is it a C3 (=1) or C4 (=2) crop. (0 = no crop) [dimensionless]
}

make_stomata_resistance :: proc(allocator := context.allocator) -> (m: Stomata_Resistance) {
	context.allocator = allocator
	m.saturation_beta = 2.5
	m.stomata_conductance_alpha = 40
	m.carboxylation_pathway = 0
	return
}

destroy_stomata_resistance :: proc(m: ^Stomata_Resistance) {
}

// - inputs:
//             * name: saturation_beta
//                           ** description : Original: Yu et al. 2001; beta = 3.5
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 2.5
//                           ** unit :
//             * name: stomata_conductance_alpha
//                           ** description : Original: Yu et al. 2001; alpha = 0.06 mol/m2/s
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min :
//                           ** default : 40
//                           ** unit : mmol/m2/s
//             * name: carboxylation_pathway
//                           ** description : Is it a C3 (=1) or C4 (=2) crop. (0 = no crop)
//                           ** inputtype : parameter
//                           ** parametercategory : constant
//                           ** datatype : INT
//                           ** max : 2
//                           ** min : 0
//                           ** default : 0
//                           ** unit : dimensionless
//             * name: atmospheric_co2_concentration
//                           ** description : CO2 concentration in the atmosphere. Optional if no crop.
//                           ** inputtype : variable
//                           ** variablecategory : exogenous
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : 430
//                           ** unit : ppm
//             * name: saturation_vapor_pressure_deficit
//                           ** description : saturation_vapor_pressure_deficit
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** unit : kPa
//             * name: gross_photosynthesis_reference_mol
//                           ** description : gross_photosynthesis_reference_mol (-1 = not supplied = no crop)
//                           ** inputtype : variable
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max :
//                           ** min : 0
//                           ** default : -1
//                           ** unit : [mol m-2 s-1] or [cm3 cm-2 s-1]
// - outputs:
//             * name: stomata_resistance
//                           ** description : stomata resistance (FAO default value is 100)
//                           ** variablecategory : auxiliary
//                           ** datatype : DOUBLE
//                           ** max : 10000
//                           ** min : 0
//                           ** unit : s/m
stomata_resistance_calculate_model :: proc(m: ^Stomata_Resistance, s: ^Et_State, s1: ^Et_State, r: ^Et_Rate, a: ^Et_Auxiliary, ex: ^Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	if m.carboxylation_pathway > 0 {
		if a.gross_photosynthesis_reference_mol <= 0.0 {
			a.stomata_resistance = 999999.9
		} else if m.carboxylation_pathway == 1 {
			a.stomata_resistance = ex.atmospheric_co2_concentration * (1.0 + a.saturation_vapor_pressure_deficit / m.saturation_beta) / (m.stomata_conductance_alpha * a.gross_photosynthesis_reference_mol)
		} else {
			a.stomata_resistance = ex.atmospheric_co2_concentration * (1.0 + a.saturation_vapor_pressure_deficit / m.saturation_beta) / (m.stomata_conductance_alpha * a.gross_photosynthesis_reference_mol)
		}
	}
}