package core

// Crop N status (plan-component-interfaces.md D5; architecture.md section 5):
// critical and target N concentrations and the N stress factors. State:
// Crop_N_Status (embedded in Crop_Module).

import p "../params"
import libc "core:c/libc"
import "../../support/cr_math"

// Crop N status: critical and target N concentrations, N stress (n_status_step).
Crop_N_Status :: struct {
	critical_n_concentration:                f64 `unit:"kgN kgDM-1"`, //! old GEHMIN
	target_n_concentration:                  f64 `unit:"kgN kgDM-1"`, //! old GEHMAX
	crop_n_redux:                            f64 `unit:"0-1"`, //! old REDUK
	root_n_redux:                            f64 `unit:"0-1"`, //! old REDWU
	n_concentration_aboveground_biomass_old: f64 `unit:"kgN kgDM-1" unit_guess:"high"`, //! old GEHALT
	n_concentration_root_old:                f64 `unit:"kgN kgDM-1" unit_guess:"high"`, //! old
}

// What n_status_step reads: yesterday's biomass and N concentrations -
// growth and N uptake run later in the step (Crop_Carry_Over).
N_Status_In :: struct {
	aboveground_biomass:                 f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`,
	belowground_biomass:                 f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`,
	n_concentration_aboveground_biomass: f64 `unit:"kgN kgDM-1" src:"N_Uptake" lag:"1d"`,
	n_concentration_root:                f64 `unit:"kgN kgDM-1" src:"N_Uptake" lag:"1d"`,
	nitrogen_response_on:                bool `unit:"" src:"simulation parameters"`,
}

// C++: void monica::cropmodule::fcCropNitrogen(CropModule*)
n_status_step :: proc(s: ^Crop_N_Status, ps: ^p.Crop_Parameters, input: N_Status_In) {
	aboveground_biomass := input.aboveground_biomass
	belowground_biomass := input.belowground_biomass
	n_concentration_aboveground_biomass := input.n_concentration_aboveground_biomass
	n_concentration_root := input.n_concentration_root
	species_ps := &ps.species_params

	s.critical_n_concentration =
		species_ps.n_concentration_pn *
		(1.0 +
				(species_ps.n_concentration_b0 *
						cr_math.exp(
							-0.26 * (aboveground_biomass + belowground_biomass) / 1000.0,
						))) /
		100.0 // [kg ha-1 -> t ha-1]
	s.target_n_concentration = s.critical_n_concentration * species_ps.luxury_n_coeff
	s.n_concentration_aboveground_biomass_old = n_concentration_aboveground_biomass
	s.n_concentration_root_old = n_concentration_root

	if n_concentration_root < 0.01 {
		if n_concentration_root <= 0.005 {
			s.root_n_redux = 0.0
		} else {
			// old WUX
			root_n_redux_helper := (n_concentration_root - 0.005) / 0.005
			s.root_n_redux = 1.0 - libc.sqrt(1.0 - root_n_redux_helper * root_n_redux_helper)
		}
	} else {
		s.root_n_redux = 1.0
	}

	if n_concentration_aboveground_biomass < s.critical_n_concentration {
		if n_concentration_aboveground_biomass <= species_ps.minimum_n_concentration {
			s.crop_n_redux = 0.0
		} else {
			crop_n_redux_helper :=
				(n_concentration_aboveground_biomass - species_ps.minimum_n_concentration) /
				(s.critical_n_concentration - species_ps.minimum_n_concentration)

			// New Monica approach
			s.crop_n_redux =
				1.0 - cr_math.exp(species_ps.minimum_n_concentration - (5.0 * crop_n_redux_helper))
		}
	} else {
		s.crop_n_redux = 1.0
	}

	if !input.nitrogen_response_on {
		s.crop_n_redux = 1.0
	}
}
