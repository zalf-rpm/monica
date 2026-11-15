package core

// Photosynthesis and respiration (plan-component-interfaces.md F5;
// architecture.md section 5): daily (classic) or hourly FvCB assimilation,
// growth and maintenance respiration. State: Crop_Photosynthesis (embedded
// in Crop_Module). It also steps the O3 component hourly (the O3 damage
// lowers the next hour's vcmax) and the VOC emissions after the hourly loop,
// so it gets their states too.

import d "../../support/date"
import tl "../../support/tools"
import p "../params"
import libc "core:c/libc"
import "../../support/cr_math"

// Photosynthesis and respiration (fc_crop_photosynthesis, classic or FvCB).
Crop_Photosynthesis :: struct {
	co2_method:                         int,
	assimilation_rate:                  f64, //! old AMAX
	gross_photosynthesis:               f64 `unit:"kgCH2O ha-1"`, //! old GPHOT
	gross_photosynthesis_mol:           f64,
	gross_photosynthesis_reference_mol: f64,
	gross_assimilates:                  f64,
	assimilates:                        f64 `unit:"kgCH2O ha-1"`,
	growth_respiration_as:              f64 `unit:"kgCH2O ha-1"`,
	maintenance_respiration_as:         f64 `unit:"kgCH2O ha-1"`,
	net_maintenance_respiration:        f64, //! old MAINT
	total_respired:                     f64,
	sunlit_leaf_area_index:             [dynamic]f64,
	shaded_leaf_area_index:             [dynamic]f64,
	k_tkc:                              f64, // old KTkc
	k_tko:                              f64, // old KTkc
	fraction_of_intercepted_radiation1: f64,
	fraction_of_intercepted_radiation2: f64,
}

// What photosynthesis_step reads on a day.
Photosynthesis_In :: struct {
	mean_air_temperature:          f64 `unit:"°C" src:"weather"`,
	max_air_temperature:           f64 `unit:"°C" src:"weather"`,
	min_air_temperature:           f64 `unit:"°C" src:"weather"`,
	atmospheric_co2_concentration: f64 `unit:"ppm" src:"weather"`,
	atmospheric_o3_concentration:  f64 `unit:"ppb" src:"weather"`,
	current_date:                  d.Date,
	latitude:                      f64 `unit:"°" src:"site"`,
	radiation:                     Crop_Radiation `src:"Radiation"`, // today's
	// yesterday's (Crop_Carry_Over): water uptake, crop N status, frost kill,
	// the root system and the crop's ET0 (for the O3 uptake) run later
	water_stress:                  f64 `unit:"0-1" src:"Water_Uptake" lag:"1d"`,
	n_stress:                      f64 `unit:"0-1" src:"N_Status" lag:"1d"`,
	frost_redux:                   f64 `unit:"0-1" src:"Stress" lag:"1d"`,
	rooting_depth:                 int `unit:"layer#" src:"Root_System" lag:"1d"`,
	reference_et0:                 f64 `unit:"mm" src:"Et0" lag:"1d"`,
	organ_green_biomass:           []f64 `unit:"kgDM ha-1" src:"Growth" lag:"1d"`, // per organ
	developmental_stage:           int `unit:"" src:"Phenology"`,
	relative_total_development:    f64 `unit:"0-1" src:"Phenology"`,
	temperature_sum_to_flowering:  f64 `unit:"°C d" src:"Phenology"`,
	total_temperature_sum:         f64 `unit:"°C d" src:"Phenology"`,
	leaf_area_index:               f64 `unit:"m2 m-2" src:"Canopy"`,
	oxygen_deficit:                f64 `unit:"0-1" src:"Stress"`,
	cutting_delay_days:            int `unit:"d" src:"Management"`,
	transplant_efficiency:         f64 `unit:"0-1" src:"Management"`,
	no_of_organs:                  int `unit:"" src:"crop parameters"`,
	water_deficit_response_on:     bool `unit:"" src:"simulation parameters"`,
	soil_layers:                   []Soil_Layer `src:"soil"`, // for the O3 uptake
}

// Fills Photosynthesis_In from the crop (crop_module_step and the test
// drivers); the five stress values are yesterday's.
photosynthesis_in :: proc(
	cm: ^Crop_Module,
	mean_air_temperature, max_air_temperature, min_air_temperature: f64,
	atmospheric_co2_concentration, atmospheric_o3_concentration: f64,
	current_date: d.Date,
	water_stress, n_stress, frost_redux: f64,
	rooting_depth: int,
	reference_et0: f64,
) -> Photosynthesis_In {
	return {
		mean_air_temperature = mean_air_temperature,
		max_air_temperature = max_air_temperature,
		min_air_temperature = min_air_temperature,
		atmospheric_co2_concentration = atmospheric_co2_concentration,
		atmospheric_o3_concentration = atmospheric_o3_concentration,
		current_date = current_date,
		latitude = cm.site_params.latitude,
		radiation = cm.radiation,
		water_stress = water_stress,
		n_stress = n_stress,
		frost_redux = frost_redux,
		rooting_depth = rooting_depth,
		reference_et0 = reference_et0,
		organ_green_biomass = cm.organ_green_biomass[:],
		developmental_stage = cm.developmental_stage,
		relative_total_development = cm.relative_total_development,
		temperature_sum_to_flowering = cm.temperature_sum_to_flowering,
		total_temperature_sum = cm.total_temperature_sum,
		leaf_area_index = cm.leaf_area_index,
		oxygen_deficit = cm.oxygen_deficit,
		cutting_delay_days = cm.cutting_delay_days,
		transplant_efficiency = cm.transplant_efficiency,
		no_of_organs = cm.no_of_organs,
		water_deficit_response_on = cm.sim_params.water_deficit_response_on,
		soil_layers = cm.soil_column.layers[:],
	}
}

// C++: void monica::cropmodule::fcCropPhotosynthesis(CropModule*, double
// vw_MeanAirTemperature, double vw_MaxAirTemperature, double
// vw_MinAirTemperature, double vw_AtmosphericCO2Concentration, double
// vw_AtmosphericO3Concentration, Tools::Date currentDate)
//
// The largest function in the port (~1100 lines in C++). Two departures from
// a literal transliteration, both because the eliminated code is provably
// dead or undefined, not because it's inconvenient to port:
//   - the Intercropping branch of the C++ (a second, alternate call to its
//     `code` lambda with a different fraction-of-intercepted-radiation
//     function, gated on `intercropping_other_crop_height > zeroHeightEps`) is
//     unreachable in this port: Intercropping itself is a dropped feature
//     (Cap'n Proto RPC, see plan-odin.md's "Explicitly dropped" table), and
//     `intercropping_other_crop_height` starts at -1 and nothing in this port
//     ever sets it positive (setOtherCropHeightAndLAIt is only ever called by
//     intercropping wiring). With only one surviving call to `code`, its body
//     is inlined directly rather than reproduced as a `std::function`-taking
//     closure (Odin's `proc` type has no capture).
//   - the hourly sunrise-detection check reads C++'s `hourlyGlobrads.back()`
//     on a still-empty, freshly-constructed `std::vector` at hour 0 -
//     undefined behaviour (likely a null-pointer dereference), not a
//     reproducible quirk. Reimplemented as an explicit "previous hour"
//     tracker seeded at 0.0, which is what the logic clearly intends: "is
//     this the first hour with positive radiation".
photosynthesis_step :: proc(
	ph: ^Crop_Photosynthesis,
	o3: ^Crop_O3,
	voc: ^Crop_Voc,
	ps: ^p.Crop_Parameters,
	mps: ^p.Crop_Module_Parameters,
	input: Photosynthesis_In,
	allocator := context.allocator,
) {
	mean_air_temperature := input.mean_air_temperature
	max_air_temperature := input.max_air_temperature
	min_air_temperature := input.min_air_temperature
	atmospheric_co2_concentration := input.atmospheric_co2_concentration
	atmospheric_o3_concentration := input.atmospheric_o3_concentration
	current_date := input.current_date
	water_stress := input.water_stress
	n_stress := input.n_stress
	frost_redux := input.frost_redux
	rooting_depth := input.rooting_depth
	reference_et0 := input.reference_et0
	crop_ps := mps
	species_ps := &ps.species_params
	cultivar_ps := &ps.cultivar_params
	assimilate_partitioning_coeff := cultivar_ps.assimilate_partitioning_coeff
	minimum_temperature_for_assimilation := species_ps.minimum_temperature_for_assimilation
	maximum_temperature_for_assimilation := species_ps.maximum_temperature_for_assimilation
	optimum_temperature_for_assimilation := species_ps.optimum_temperature_for_assimilation
	organ_growth_respiration := species_ps.organ_growth_respiration
	organ_maintenance_respiration := species_ps.organ_maintenance_respiration
	specific_leaf_area := cultivar_ps.specific_leaf_area
	water_deficit_response_on := input.water_deficit_response_on
	latitude := input.latitude

	assimilation_rate_reference := 0.0

	radiation_use_efficiency := species_ps.default_radiation_use_efficiency
	radiation_use_efficiency_reference := species_ps.default_radiation_use_efficiency

	D_IN_K :: VOC_D_IN_K
	RGAS :: VOC_RGAS
	TK25 :: VOC_TK25

	if species_ps.carboxylation_pathway == 1 {
		// Calculation of CO2 impact on crop growth
		if ph.co2_method == 3 {
			// --------------------------------------------------------------------
			// Method 3:
			// Long, S.P. 1991. Modification of the response of photosynthetic
			// productivity to rising temperature by atmospheric CO2
			// concentrations - Has its importance been underestimated. Plant
			// Cell Environ. 14(8): 729-739.
			// and
			// Mitchell, R.A.C., D.W. Lawlor, V.J. Mitchell, C.L. Gibbard, E.M.
			// White, and J.R. Porter. 1995. Effects of elevated CO2
			// concentration and increased temperature on winter-wheat - Test
			// of ARCWHEAT1 simulation model. Plant Cell Environ. 18(7):736-748.
			// ----------------------------------------------------------------------
			temp_k := mean_air_temperature + D_IN_K
			term1 := (temp_k - TK25) / (TK25 * temp_k * RGAS)
			term2 := libc.sqrt(temp_k / TK25)
			ph.k_tkc = cr_math.exp(species_ps.aekc * term1) * term2
			ph.k_tko = cr_math.exp(species_ps.aeko * term1) * term2
			mkc := species_ps.kc25 * ph.k_tkc // [umol mol-1]
			voc.crop_photosynthesis_results.kc = mkc
			mko := species_ps.ko25 * ph.k_tko // [mmol mol-1]
			voc.crop_photosynthesis_results.ko = mko * 1000.0 // mmol -> umol

			// OLD exponential response
			k_tvmax: f64
			if crop_ps.__enable_Photosynthesis_WangEngelTemperatureResponse__ {
				k_tvmax = max(
					0.00001,
					wang_engel_temperature_response(
						mean_air_temperature,
						minimum_temperature_for_assimilation,
						optimum_temperature_for_assimilation,
						maximum_temperature_for_assimilation,
						1.0,
					),
				)
			} else {
				k_tvmax = cr_math.exp(species_ps.aevc * term1) * term2
			}

			// Berechnung des Transformationsfaktors fuer pflanzenspez. AMAX bei
			// 25 grad - old fakamax
			amax_factor := cultivar_ps.max_assimilation_rate / 34.668
			amax_factor_reference := crop_ps.reference_max_assimilation_rate / 34.668
			// old vcmax
			vcmax := 98.0 * amax_factor * k_tvmax
			voc.crop_photosynthesis_results.vc_max = vcmax
			vcmax_reference := 98.0 * amax_factor_reference * k_tvmax

			oi :=
				210.0 *
				(0.047 -
						0.0013087 * mean_air_temperature +
						0.000025603 * (mean_air_temperature * mean_air_temperature) -
						0.00000021441 *
							(mean_air_temperature *
									mean_air_temperature *
									mean_air_temperature)) /
				0.026934 // [mmol mol-1]
			voc.crop_photosynthesis_results.oi = oi * 1000.0 // mmol -> umol

			ci :=
				atmospheric_co2_concentration *
				0.7 *
				(1.674 -
						0.061294 * mean_air_temperature +
						0.0011688 * (mean_air_temperature * mean_air_temperature) -
						0.0000088741 *
							(mean_air_temperature *
									mean_air_temperature *
									mean_air_temperature)) /
				0.73547 // [umol mol-1]
			voc.crop_photosynthesis_results.ci = ci

			// similar to LDNDC::jarvis.cpp:217 - old COcomp
			co2_compensation_point := 0.5 * 0.21 * vcmax * mkc * oi / (vcmax * mko) // [umol mol-1]
			co2_compensation_point_reference :=
				0.5 * 0.21 * vcmax_reference * mkc * oi / (vcmax_reference * mko) // [umol mol-1]
			voc.crop_photosynthesis_results.comp = co2_compensation_point

			// Mitchell et al. 1995 - old EFF
			radiation_use_efficiency = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(ci - co2_compensation_point) /
					(4.5 * ci + 10.5 * co2_compensation_point) *
					8.3769,
					0.5,
				),
			)
			radiation_use_efficiency_reference = max(
				0.0,
				min(
					0.77 /
					2.1 *
					(ci - co2_compensation_point_reference) /
					(4.5 * ci + 10.5 * co2_compensation_point_reference) *
					8.3769,
					0.5,
				),
			)

			ph.assimilation_rate =
				(ci - co2_compensation_point) * vcmax / (ci + mkc * (1.0 + oi / mko)) * 1.656
			assimilation_rate_reference =
				(ci - co2_compensation_point_reference) *
				vcmax_reference /
				(ci + mkc * (1.0 + oi / mko)) *
				1.656

			if mean_air_temperature < minimum_temperature_for_assimilation {
				ph.assimilation_rate = 0.0 // MP: warum gibt es fuer C3-Pflanzen keine maximale Temperatur
				assimilation_rate_reference = 0.0
			}
		} else if ph.co2_method == 2 {
			// -----------------------------------------------------------------
			// Method 2:
			// Hoffmann, F. 1995. Fagus, a model for growth and development of
			// beech. Ecol. Mod. 83 (3):327-348.
			// -----------------------------------------------------------------
			t_response := wang_engel_temperature_response(
				mean_air_temperature,
				minimum_temperature_for_assimilation,
				optimum_temperature_for_assimilation,
				maximum_temperature_for_assimilation,
				1.0,
			)

			ph.assimilation_rate = cultivar_ps.max_assimilation_rate * t_response
			assimilation_rate_reference = crop_ps.reference_max_assimilation_rate * t_response

			// old KCo1
			hoffmann_k1 := 220.0 + 0.158 * (input.radiation.global_radiation * 86400.0 / 1000000.0)
			// PAR [MJ m-2], Hoffmann's model requires [W m-2] ->
			// conversion of [MJ m-2] to [W m-2]
			// old coco
			hoffmann_c0 := 80.0 - 0.036 * (input.radiation.global_radiation * 86400.0 / 1000000.0)
			// old KCO2
			hoffmann_kco2 :=
				((atmospheric_co2_concentration - hoffmann_c0) /
					(hoffmann_k1 + atmospheric_co2_concentration - hoffmann_c0)) /
				((350.0 - hoffmann_c0) / (hoffmann_k1 + 350.0 - hoffmann_c0))

			ph.assimilation_rate = ph.assimilation_rate * hoffmann_kco2
			assimilation_rate_reference = assimilation_rate_reference * hoffmann_kco2
		}
	} else { 	// pc_CarboxylationPathway == 2
		t_response := wang_engel_temperature_response(
			mean_air_temperature,
			minimum_temperature_for_assimilation,
			optimum_temperature_for_assimilation,
			maximum_temperature_for_assimilation,
			1.0,
		)

		ph.assimilation_rate = cultivar_ps.max_assimilation_rate * t_response
		assimilation_rate_reference = crop_ps.reference_max_assimilation_rate * t_response
	}

	if input.cutting_delay_days > 0 {
		ph.assimilation_rate = 0.1
	}

	ph.assimilation_rate = max(0.1, ph.assimilation_rate)
	assimilation_rate_reference = max(0.1, assimilation_rate_reference)

	// ---------------------------------------------------------------------
	// Calculation of light interception in the crop (Penning De Vries & van
	// Laar 1982)
	// ---------------------------------------------------------------------

	PI :: 3.14159265358979323

	// old EFFE, crop_ps.canopy_reflection_coefficient: old REFLC
	net_radiation_use_efficiency :=
		(1.0 - crop_ps.canopy_reflection_coefficient) * radiation_use_efficiency
	net_radiation_use_efficiency_reference :=
		(1.0 - crop_ps.canopy_reflection_coefficient) * radiation_use_efficiency_reference

	sslae := cr_math.sin((90.0 + input.radiation.declination - latitude) * PI / 180.0) // = HERMES

	x := cr_math.log(
		1.0 +
		0.45 *
			input.radiation.clear_day_radiation /
			(input.radiation.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			(sslae * ph.assimilation_rate),
	) // = HERMES
	x_reference := cr_math.log(
		1.0 +
		0.45 *
			input.radiation.clear_day_radiation /
			(input.radiation.effective_day_length * 3600.0) *
			net_radiation_use_efficiency_reference /
			(sslae * assimilation_rate_reference),
	)

	phch1 := sslae * ph.assimilation_rate * input.radiation.effective_day_length * x / (1.0 + x) // = HERMES
	phch1_reference :=
		sslae *
		assimilation_rate_reference *
		input.radiation.effective_day_length *
		x_reference /
		(1.0 + x_reference)

	y := cr_math.log(
		1.0 +
		0.55 *
			input.radiation.clear_day_radiation /
			(input.radiation.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			((5.0 - sslae) * ph.assimilation_rate),
	) // = HERMES
	y_reference := cr_math.log(
		1.0 +
		0.55 *
			input.radiation.clear_day_radiation /
			(input.radiation.effective_day_length * 3600.0) *
			net_radiation_use_efficiency /
			((5.0 - sslae) * assimilation_rate_reference),
	)

	phch2 := (5.0 - sslae) * ph.assimilation_rate * input.radiation.effective_day_length * y / (1.0 + y) // = HERMES
	phch2_reference :=
		(5.0 - sslae) *
		assimilation_rate_reference *
		input.radiation.effective_day_length *
		y_reference /
		(1.0 + y_reference)

	phch := 0.95 * (phch1 + phch2) + 20.5 // = HERMES
	phch_reference := 0.95 * (phch1_reference + phch2_reference) + 20.5

	// oxygen_deficit separates drought stress (ETa/Etp) from saturation
	// stress - old VSWELL
	drought_stress_threshold :=
		input.oxygen_deficit < 1.0 ? 0.0 : cultivar_ps.drought_stress_threshold[input.developmental_stage]

	// Calculation of time fraction for overcast sky situations by comparing
	// clear day radiation and measured PAR in [J m-2]. HERMES uses PAR as 50%
	// of global radiation - old FOV
	overcast_sky_time_fraction := 0.0
	if input.radiation.clear_day_radiation != 0 {
		overcast_sky_time_fraction =
			(input.radiation.clear_day_radiation - (1000000.0 * input.radiation.global_radiation * 0.50)) /
			(0.8 * input.radiation.clear_day_radiation)
	}
	overcast_sky_time_fraction = max(0.0, min(overcast_sky_time_fraction, 1.0))

	// C++'s `code` lambda inlined at its one surviving call site (see the
	// function comment above) - only ever invoked with F_t1/leaf_area_index.
	lai := input.leaf_area_index
	fraction_of_intercepted_radiation :=
		1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * lai)

	phc3 := phch * fraction_of_intercepted_radiation
	phc3_reference :=
		phch_reference *
		(1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * crop_ps.reference_leaf_area_index))

	phc4 := input.radiation.astronomic_day_lenght * lai * ph.assimilation_rate
	phc4_reference :=
		input.radiation.astronomic_day_lenght * crop_ps.reference_leaf_area_index * assimilation_rate_reference

	phcl :=
		phc3 < phc4 ? phc3 * (1.0 - cr_math.exp(-phc4 / phc3)) : phc4 * (1.0 - cr_math.exp(-phc3 / phc4))

	phcl_reference :=
		phc3_reference < phc4_reference ? phc3_reference * (1.0 - cr_math.exp(-phc4_reference / phc3_reference)) : phc4_reference * (1.0 - cr_math.exp(-phc3_reference / phc4_reference))

	z :=
		input.radiation.overcast_day_radiation /
		(input.radiation.effective_day_length * 3600.0) *
		net_radiation_use_efficiency /
		(5.0 * ph.assimilation_rate)

	phoh1 := 5.0 * ph.assimilation_rate * input.radiation.effective_day_length * z / (1.0 + z)
	phoh := 0.9935 * phoh1 + 1.1
	pho3 := phoh * fraction_of_intercepted_radiation
	pho3_reference :=
		phoh *
		(1.0 - cr_math.exp(-cultivar_ps.light_extinction_coefficient * crop_ps.reference_leaf_area_index))

	phol :=
		pho3 < phc4 ? pho3 * (1.0 - cr_math.exp(-phc4 / pho3)) : phc4 * (1.0 - cr_math.exp(-pho3 / phc4))

	phol_reference :=
		pho3_reference < phc4_reference ? pho3_reference * (1.0 - cr_math.exp(-phc4_reference / pho3_reference)) : phc4_reference * (1.0 - cr_math.exp(-pho3_reference / phc4_reference))

	clear_day_co2_assimilation := lai < 5.0 ? phcl : phch // [J m-2]
	overcast_day_co2_assimilation := lai < 5.0 ? phol : phoh // [J m-2]

	clear_day_co2_assimilation_reference := phcl_reference
	overcast_day_co2_assimilation_reference := phol_reference

	// old DTGA
	gross_co2_assimilation :=
		overcast_sky_time_fraction * overcast_day_co2_assimilation +
		(1.0 - overcast_sky_time_fraction) * clear_day_co2_assimilation

	// used for ET0 calculation
	gross_co2_assimilation_reference :=
		overcast_sky_time_fraction * overcast_day_co2_assimilation_reference +
		(1.0 - overcast_sky_time_fraction) * clear_day_co2_assimilation_reference

	// Gross CO2 assimilation is used for reference evapotranspiration
	// calculation. For this purpose it must not be affected by drought
	// stress, as the grass reference is defined as being always well
	// supplied with water. Water stress is acting at a later stage.
	// NOTE(c++-quirk): the C++ multiplies vc_GrossCO2Assimilation by itself
	// here (a no-op, per the source comment "* transpiration_deficit" being
	// commented out) - reproduced as the no-op it is, not simplified away.
	if water_stress < drought_stress_threshold {
		gross_co2_assimilation = gross_co2_assimilation
	}

	julian_day := int(d.julian_day(current_date))
	daily_gp := 0.0
	if crop_ps.__enable_hourly_FvCB_photosynthesis__ && species_ps.carboxylation_pathway == 1 {
		hourly_globrads := make([dynamic]f64, 0, 24, context.temp_allocator)
		hourly_extrarad := make([dynamic]f64, 0, 24, context.temp_allocator)
		sunrise_h := 0
		// see the function comment above re: this vs. C++'s hourlyGlobrads.back()
		prev_hgr := 0.0

		for h := 0; h < 24; h += 1 {
			hgr := tl.hourly_rad(input.radiation.global_radiation, latitude, julian_day, f64(h))
			if hgr > 0 && prev_hgr == 0.0 {
				sunrise_h = h
			}
			append(&hourly_globrads, hgr)
			prev_hgr = hgr

			append(
				&hourly_extrarad,
				tl.hourly_rad(input.radiation.extraterrestrial_radiation, latitude, julian_day, f64(h)),
			)
		}

		voc_hours: [24]Voc_Hour
		for h := 0; h < 24; h += 1 {
			// hourly photosynthesis
			fvcb_in: Fvcb_Canopy_Hourly_In

			hourly_temp := tl.hourly_t(min_air_temperature, max_air_temperature, f64(h), f64(sunrise_h))
			fvcb_in.leaf_temp = hourly_temp
			fvcb_in.global_rad = hourly_globrads[h]
			fvcb_in.extra_terr_rad = hourly_extrarad[h]
			fvcb_in.lai = lai
			fvcb_in.solar_el = tl.solar_elevation(f64(h), latitude, julian_day)
			fvcb_in.vpd = tl.hourly_vapor_pressure_deficit(
				hourly_temp,
				min_air_temperature,
				mean_air_temperature,
				max_air_temperature,
			)
			fvcb_in.ca = atmospheric_co2_concentration

			hps := make_fvcb_canopy_hourly_params()
			hps.vcmax_25 = species_ps.vcmax25 * o3.o3_short_term_damage * o3.o3_senescence

			fvcb_res := fvcb_canopy_hourly_c3(fvcb_in, hps)

			ph.sunlit_leaf_area_index[h] = fvcb_res.sunlit.lai
			ph.shaded_leaf_area_index[h] = fvcb_res.shaded.lai

			// [umol CO2 m-2 (h-1)] -> [kg CO2 ha-1 (d-1)]
			daily_gp += fvcb_res.canopy_gross_photos * 44.0 / 100.0 / 1000.0

			// hourly O3 uptake and damage: its damage lowers the next hour's vcmax
			// (above), so the O3 component is stepped inside the hourly loop
			o3_hourly_step(
				o3,
				O3_Hourly_In {
					hour = h,
					sunlit = fvcb_res.sunlit,
					shaded = fvcb_res.shaded,
					rooting_depth = rooting_depth,
					soil_layers = input.soil_layers[:],
					reference_et0 = reference_et0,
					atmospheric_o3_concentration = atmospheric_o3_concentration,
					relative_total_development = input.relative_total_development,
					temperature_sum_to_flowering = input.temperature_sum_to_flowering,
					total_temperature_sum = input.total_temperature_sum,
					water_deficit_response_on = water_deficit_response_on,
				},
			)

			// for the VOC emissions after the loop
			voc_hours[h] = Voc_Hour {
				global_rad = fvcb_in.global_rad,
				leaf_temp  = fvcb_in.leaf_temp,
				sunlit     = fvcb_res.sunlit,
				shaded     = fvcb_res.shaded,
			}
		}

		// VOC emissions: only reported, nothing in the crop reads them, so they
		// are computed from the day's 24 hours after the loop
		voc_step(
			voc,
			ps,
			Voc_In {
				hours = voc_hours,
				lai = lai,
				leaf_green_biomass = input.organ_green_biomass[Organ_Leaf],
				specific_leaf_area = specific_leaf_area[input.developmental_stage],
				atmospheric_co2_concentration = atmospheric_co2_concentration,
				n_stress = n_stress,
				water_stress = water_stress,
			},
			allocator,
		)
	}

	gross_co2_assimilation =
		crop_ps.__enable_hourly_FvCB_photosynthesis__ && species_ps.carboxylation_pathway == 1 ? daily_gp : gross_co2_assimilation

	ph.fraction_of_intercepted_radiation1 = fraction_of_intercepted_radiation

	// [TRANSPLANT SHOCK] Photosynthesis Limitation.
	if input.transplant_efficiency < 1.0 {
		gross_co2_assimilation *= input.transplant_efficiency
	}

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [kg CH2O ha-1 d-1]
	ph.gross_photosynthesis = gross_co2_assimilation * 30.0 / 44.0

	// Calculation of photosynthesis rate from [kg CO2 ha-1 d-1] to [mol m-2 s-1]
	ph.gross_photosynthesis_mol = gross_co2_assimilation * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)
	ph.gross_photosynthesis_reference_mol =
		gross_co2_assimilation_reference * 22414.0 / (10.0 * 3600.0 * 24.0 * 44.0)

	// Converting photosynthesis rate from [kg CO2 ha leaf-1 d-1] to [kg CH2O ha-1 d-1]
	ph.assimilates = gross_co2_assimilation * 30.0 / 44.0

	// reduction value for assimilate amount to simulate field conditions
	ph.assimilates *= species_ps.field_condition_modifier
	// reduction value for assimilate amount to simulate frost damage
	ph.assimilates *= frost_redux
	// MP: added reduction value for assimilate amount to simulate waterlogging
	ph.assimilates *= input.oxygen_deficit

	if water_stress < drought_stress_threshold { 	// MP: Access point for drought optimisation
		ph.assimilates = ph.assimilates * water_stress
	}

	ph.gross_assimilates = ph.assimilates

	// ---------------------------------------------------------------------
	// AGROSIM night and day maintenance and growth respiration
	// ---------------------------------------------------------------------

	photo_temperature :=
		max_air_temperature - ((max_air_temperature - min_air_temperature) / 4.0)
	night_temperature :=
		min_air_temperature + ((max_air_temperature - min_air_temperature) / 4.0)

	maintenance_respiration_sum := 0.0
	for i_organ := 0; i_organ < input.no_of_organs; i_organ += 1 {
		maintenance_respiration_sum +=
			input.organ_green_biomass[i_organ] * organ_maintenance_respiration[i_organ] // [kg CH2O ha-1]
	}

	normalised_day_length := 2.0 - (input.radiation.photoperiodic_daylength / 12.0)

	photo_maintenance_respiration :=
		maintenance_respiration_sum *
		cr_math.pow(
			2.0,
			(crop_ps.maintenance_respiration_parameter1 *
				(photo_temperature - crop_ps.maintenance_respiration_parameter2)),
		) *
		(2.0 - normalised_day_length) // @todo: [g m-2] --> [kg ha-1]

	dark_maintenance_respiration :=
		maintenance_respiration_sum *
		cr_math.pow(
			2.0,
			(crop_ps.maintenance_respiration_parameter1 *
				(night_temperature - crop_ps.maintenance_respiration_parameter2)),
		) *
		normalised_day_length // @todo: [g m-2] --> [kg ha-1]

	ph.maintenance_respiration_as = photo_maintenance_respiration + dark_maintenance_respiration // [kg CH2O ha-1]

	ph.assimilates -= photo_maintenance_respiration + dark_maintenance_respiration // [kg CH2O ha-1]

	growth_respiration_sum := 0.0
	if ph.assimilates > 0 {
		for i_organ := 0; i_organ < input.no_of_organs; i_organ += 1 {
			growth_respiration_sum +=
				assimilate_partitioning_coeff[input.developmental_stage][i_organ] *
				ph.assimilates *
				organ_growth_respiration[i_organ]
		}
	}

	photo_growth_respiration := 0.0
	if ph.assimilates > 0.0 {
		photo_growth_respiration =
			growth_respiration_sum *
			cr_math.pow(
				2.0,
				(crop_ps.growth_respiration_parameter1 *
					(photo_temperature - crop_ps.growth_respiration_parameter2)),
			) *
			(2.0 - normalised_day_length) // [kg CH2O ha-1]

		if ph.assimilates > photo_growth_respiration {
			ph.assimilates -= photo_growth_respiration
		} else {
			photo_growth_respiration = ph.assimilates // in this case the plant will be restricted in growth!
			ph.assimilates = 0.0
		}
	}

	// NOTE(c++-quirk): uses vc_PhotoTemperature here too, not
	// vc_NightTemperature - asymmetric with the maintenance-respiration split
	// just above (which correctly uses Photo/Night respectively). Reproduced
	// exactly, not "fixed".
	dark_growth_respiration := 0.0
	if ph.assimilates > 0.0 {
		dark_growth_respiration =
			growth_respiration_sum *
			cr_math.pow(
				2.0,
				(crop_ps.growth_respiration_parameter1 *
					(photo_temperature - crop_ps.growth_respiration_parameter2)),
			) *
			normalised_day_length // [kg CH2O ha-1]

		if ph.assimilates > dark_growth_respiration {
			ph.assimilates -= dark_growth_respiration
		} else {
			dark_growth_respiration = ph.assimilates // in this case the plant will be restricted in growth!
			ph.assimilates = 0.0
		}
	}
	ph.growth_respiration_as = photo_growth_respiration + dark_growth_respiration // [kg CH2O ha-1]
	ph.total_respired = ph.gross_assimilates - ph.assimilates // [kg CH2O ha-1]

	// ---------------------------------------------------------------------
	// HERMES calculation of maintenance respiration in dependence of
	// temperature (to reactivate, net_photosynthesis needs to be used
	// instead of assimilates in the subsequent methods)
	// ---------------------------------------------------------------------

	// old TEFF
	maintenance_temperature_dependency := cr_math.pow(2.0, (0.1 * mean_air_temperature - 2.5))

	// old MAINTS
	maintenance_respiration := 0.0
	for i_organ := 0; i_organ < input.no_of_organs; i_organ += 1 {
		maintenance_respiration +=
			input.organ_green_biomass[i_organ] * organ_maintenance_respiration[i_organ]
	}

	if ph.gross_photosynthesis <
	   (maintenance_respiration * maintenance_temperature_dependency) {
		ph.net_maintenance_respiration = ph.gross_photosynthesis
	} else {
		ph.net_maintenance_respiration =
			maintenance_respiration * maintenance_temperature_dependency
	}

	if mean_air_temperature < minimum_temperature_for_assimilation {
		ph.gross_photosynthesis = ph.net_maintenance_respiration
	}
}
