package monica_reference_evapotranspiration

// - Name: AMEIRefET -Version: 1, -Time step: 1
// - Description:
//             * Title: AMEI ET MONICA reference evapotranspiration model
//             * Authors: Claas Nendel (transcription into Crop2ML by Michael Berg-Mohnicke)
//             * Reference: None
//             * Institution: ZALF e.V.
//             * ExtendedDescription: None
//             * ShortDescription: Calculates the MONICA reference evapotranspiration for AMEI ET exercise
Amei_Ref_Et_Component :: struct {
	reference_evapotranspiration: Reference_Evapotranspiration,
}

make_amei_ref_et_component :: proc(allocator := context.allocator) -> (c: Amei_Ref_Et_Component) {
	context.allocator = allocator
	c.reference_evapotranspiration = make_reference_evapotranspiration()
	return
}

destroy_amei_ref_et_component :: proc(c: ^Amei_Ref_Et_Component) {
	destroy_reference_evapotranspiration(&c.reference_evapotranspiration)
}

amei_ref_et_component_set_height_nn :: proc(c: ^Amei_Ref_Et_Component, value: f64) {
	c.reference_evapotranspiration.height_nn = value
}

amei_ref_et_component_set_latent_heat_of_vaporization :: proc(c: ^Amei_Ref_Et_Component, value: f64) {
	c.reference_evapotranspiration.latent_heat_of_vaporization = value
}

amei_ref_et_component_calculate_model :: proc(c: ^Amei_Ref_Et_Component, s: ^Amei_Ref_Et_State, s1: ^Amei_Ref_Et_State, r: ^Amei_Ref_Et_Rate, a: ^Amei_Ref_Et_Auxiliary, ex: ^Amei_Ref_Et_Exogenous, allocator := context.allocator) {
	context.allocator = allocator
	reference_evapotranspiration_calculate_model(&c.reference_evapotranspiration, s, s1, r, a, ex)
}