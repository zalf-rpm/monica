package monica_amei_potential_evapotranspiration

// - Name: AMEIPotET -Version: 1, -Time step: 1
// - Description:
//             * Title: AMEI ET MONICA potential evapotranspiration model
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the MONICA potential evapotranspiration for AMEI ET exercise
Amei_Pot_Et_Component :: struct {
	saturation_vapor_pressure_deficit: Saturation_Vapor_Pressure_Deficit,
	reference_evapotranspiration: Reference_Evapotranspiration,
	potential_evapotranspiration: Potential_Evapotranspiration,
}

make_amei_pot_et_component :: proc(allocator := context.allocator) -> (c: Amei_Pot_Et_Component) {
	context.allocator = allocator
	c.saturation_vapor_pressure_deficit = make_saturation_vapor_pressure_deficit()
	c.reference_evapotranspiration = make_reference_evapotranspiration()
	c.potential_evapotranspiration = make_potential_evapotranspiration()
	return
}

destroy_amei_pot_et_component :: proc(c: ^Amei_Pot_Et_Component) {
	destroy_saturation_vapor_pressure_deficit(&c.saturation_vapor_pressure_deficit)
	destroy_reference_evapotranspiration(&c.reference_evapotranspiration)
	destroy_potential_evapotranspiration(&c.potential_evapotranspiration)
}

amei_pot_et_component_set_height_nn :: proc(c: ^Amei_Pot_Et_Component, value: f64) {
	c.reference_evapotranspiration.height_nn = value
}

amei_pot_et_component_calculate_model :: proc(c: ^Amei_Pot_Et_Component, s: ^Amei_Pot_Et_State, s1: ^Amei_Pot_Et_State, r: ^Amei_Pot_Et_Rate, a: ^Amei_Pot_Et_Auxiliary, ex: ^Amei_Pot_Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	saturation_vapor_pressure_deficit_calculate_model(&c.saturation_vapor_pressure_deficit, s, s1, r, a, ex)
	reference_evapotranspiration_calculate_model(&c.reference_evapotranspiration, s, s1, r, a, ex)
	potential_evapotranspiration_calculate_model(&c.potential_evapotranspiration, s, s1, r, a, ex)
}