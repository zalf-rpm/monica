package core

// The crop's carbon balance (plan-component-interfaces.md F; architecture.md
// section 5): GPP, autotrophic respiration, NPP - the NPP goes to soil
// organic through Crop_To_Soil. State: Crop_Carbon (embedded in Crop_Module).

// Carbon balance (carbon_step).
Crop_Carbon :: struct {
	gross_primary_production: f64 `unit:"kgC ha-1"`,
	net_primary_production:   f64 `unit:"kgC ha-1"`,
	respiration:              f64,
}

// What carbon_step reads on a day.
Carbon_In :: struct {
	gross_assimilates: f64 `unit:"kgCH2O ha-1" src:"Photosynthesis"`,
	total_respired:    f64 `unit:"kgCH2O ha-1" src:"Photosynthesis"`,
}

// C++: double monica::cropmodule::fcGrossPrimaryProduction(const CropModule*)
// and fcNetPrimaryProduction(CropModule*, double total_respired).
carbon_step :: proc(c: ^Crop_Carbon, input: Carbon_In) {
	// Converting photosynthesis rate from [kg CH2O ha-1 d-1] back to [kg C ha-1 d-1]
	c.gross_primary_production = input.gross_assimilates / 30.0 * 12.0
	// Convert [kg CH2O ha-1 d-1] to [kg C ha-1 d-1]
	c.respiration = input.total_respired / 30.0 * 12.0
	c.net_primary_production = c.gross_primary_production - c.respiration
}
