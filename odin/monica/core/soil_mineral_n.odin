// Soil mineral N (urea/carbamid, NH4, NO2, NO3) as rate/state - phase I of
// plan-component-interfaces.md, architecture.md section 11.
//
// The default path (`mineral_n_as_rates` off, the C++'s behaviour) changes the
// pools in place, in sequence: urea hydrolysis, mineralisation/immobilisation,
// volatilisation, nitrification, denitrification (soil_organic_step), then N
// deposition and crop uptake (soil_transport_step). Each process sees what the
// previous ones left and is capped at it, so the call order is a priority
// nobody decided.
//
// With `mineral_n_as_rates` on (soil_n_step_as_rates):
//
// 1. Every process computes its rate from the state at the start of the
//    general step and records it as a demand in `Mineral_N_Rates` - no
//    process changes a pool. The processes keep their own rules (the
//    immobilisation split between NH4 and NO3, the cap of NH3 volatilisation
//    at the NH4 present, the crop's cap at NO3 minus the minimum available N,
//    denitrification at most `transport_rate_coeff` * NO3).
// 2. The owner, mineral_n_apply, limits the demands pool by pool along the
//    chain carbamid -> NH4 -> NO2 -> NO3 with one explicit rule:
//    - what a pool can give is its start content plus what flows into it the
//      same day (hydrolysis and mineralisation into NH4, ammonia oxidation into
//      NO2, nitrite oxidation, STICS nitrification and deposition into NO3);
//    - committed claims are served first: immobilisation (its N is booked in
//      the microbial biomass by the C pools' deltas) and crop uptake (the crop
//      has already added it to its N content). They are scaled only if they
//      alone exceed the pool;
//    - the soil's own transformations and losses (volatilisation,
//      nitrification, denitrification) share the rest in proportion to their
//      demands.
//    It applies the actual rates and checks the mass balance.
// 3. Nitrate transport (leaching) then runs on the new state, as before - a
//    reaction/transport operator split.
//
// The order of the processes no longer changes the result, every pool stays
// non-negative, and the sum of the actual rates is the change of the pools.
// It changes MONICA's results: a model change, validated rather than diffed
// (tests/divergence_report.py).
package core

// Per layer, kg N m-3 d-1. Used twice in a step: as the processes' demands
// and as the actual rates after limiting.
Mineral_N_Rates :: struct {
	hydrolysis:            [dynamic]f64 `unit:"kgN m-3 d-1"`, // carbamid -> NH4
	mineralisation:        [dynamic]f64 `unit:"kgN m-3 d-1"`, // organic matter -> NH4
	immobilisation_nh4:    [dynamic]f64 `unit:"kgN m-3 d-1"`, // NH4 -> microbial biomass (committed)
	immobilisation_no3:    [dynamic]f64 `unit:"kgN m-3 d-1"`, // NO3 -> microbial biomass (committed)
	volatilisation_urea:   [dynamic]f64 `unit:"kgN m-3 d-1"`, // NH4 -> NH3, top layer
	volatilisation_manure: [dynamic]f64 `unit:"kgN m-3 d-1"`, // NH4 -> NH3 after manure (ALFAM), top layer
	ammonia_oxidation:     [dynamic]f64 `unit:"kgN m-3 d-1"`, // NH4 -> NO2
	nitrite_oxidation:     [dynamic]f64 `unit:"kgN m-3 d-1"`, // NO2 -> NO3
	nitrification_direct:  [dynamic]f64 `unit:"kgN m-3 d-1"`, // NH4 -> NO3 (STICS nitrification)
	denitrification:       [dynamic]f64 `unit:"kgN m-3 d-1"`, // NO3 -> N2, N2O
	crop_uptake:           [dynamic]f64 `unit:"kgN m-3 d-1"`, // NO3 -> crop (committed)
	deposition:            [dynamic]f64 `unit:"kgN m-3 d-1"`, // atmosphere -> NO3
}

make_mineral_n_rates :: proc(number_of_layers: int, allocator := context.allocator) -> Mineral_N_Rates {
	r: Mineral_N_Rates
	r.hydrolysis = make([dynamic]f64, number_of_layers, allocator)
	r.mineralisation = make([dynamic]f64, number_of_layers, allocator)
	r.immobilisation_nh4 = make([dynamic]f64, number_of_layers, allocator)
	r.immobilisation_no3 = make([dynamic]f64, number_of_layers, allocator)
	r.volatilisation_urea = make([dynamic]f64, number_of_layers, allocator)
	r.volatilisation_manure = make([dynamic]f64, number_of_layers, allocator)
	r.ammonia_oxidation = make([dynamic]f64, number_of_layers, allocator)
	r.nitrite_oxidation = make([dynamic]f64, number_of_layers, allocator)
	r.nitrification_direct = make([dynamic]f64, number_of_layers, allocator)
	r.denitrification = make([dynamic]f64, number_of_layers, allocator)
	r.crop_uptake = make([dynamic]f64, number_of_layers, allocator)
	r.deposition = make([dynamic]f64, number_of_layers, allocator)
	return r
}

mineral_n_rates_clear :: proc(r: ^Mineral_N_Rates) {
	for i in 0 ..< len(r.hydrolysis) {
		r.hydrolysis[i] = 0
		r.mineralisation[i] = 0
		r.immobilisation_nh4[i] = 0
		r.immobilisation_no3[i] = 0
		r.volatilisation_urea[i] = 0
		r.volatilisation_manure[i] = 0
		r.ammonia_oxidation[i] = 0
		r.nitrite_oxidation[i] = 0
		r.nitrification_direct[i] = 0
		r.denitrification[i] = 0
		r.crop_uptake[i] = 0
		r.deposition[i] = 0
	}
}

// The factor in [0, 1] by which demands summing to `demand` are scaled so
// that together they take at most `available`.
mineral_n_limit_factor :: proc(available, demand: f64) -> f64 {
	if demand <= 0 || demand <= available {
		return 1
	}
	if available <= 0 {
		return 0
	}
	return available / demand
}

// Limits the demands `d` by the rule in the header, writes the actual rates
// into `a` and applies them to the layers' pools. Returns the largest
// absolute mass-balance error over the layers [kg N m-3]: the change of
// carbamid + NH4 + NO2 + NO3 minus (sources - sinks) - rounding only.
mineral_n_apply :: proc(layers: []Soil_Layer, d, a: ^Mineral_N_Rates) -> (balance_error: f64) {
	for i in 0 ..< min(len(layers), len(d.hydrolysis)) {
		l := &layers[i]
		before := l.soil_carbamid + l.soil_nh4 + l.soil_no2 + l.soil_no3

		// carbamid: hydrolysis is its only consumer
		a.hydrolysis[i] = min(d.hydrolysis[i], max(l.soil_carbamid, 0))
		l.soil_carbamid -= a.hydrolysis[i]

		// NH4
		a.mineralisation[i] = d.mineralisation[i]
		nh4_available := l.soil_nh4 + a.hydrolysis[i] + a.mineralisation[i]
		fc := mineral_n_limit_factor(nh4_available, d.immobilisation_nh4[i])
		a.immobilisation_nh4[i] = d.immobilisation_nh4[i] * fc
		nh4_rest := nh4_available - a.immobilisation_nh4[i]
		nh4_demand :=
			d.volatilisation_urea[i] +
			d.volatilisation_manure[i] +
			d.ammonia_oxidation[i] +
			d.nitrification_direct[i]
		f := mineral_n_limit_factor(nh4_rest, nh4_demand)
		a.volatilisation_urea[i] = d.volatilisation_urea[i] * f
		a.volatilisation_manure[i] = d.volatilisation_manure[i] * f
		a.ammonia_oxidation[i] = d.ammonia_oxidation[i] * f
		a.nitrification_direct[i] = d.nitrification_direct[i] * f
		if fc < 1 || f < 1 {
			l.soil_nh4 = 0
		} else {
			l.soil_nh4 =
				nh4_rest -
				(a.volatilisation_urea[i] +
						a.volatilisation_manure[i] +
						a.ammonia_oxidation[i] +
						a.nitrification_direct[i])
		}

		// NO2
		no2_available := l.soil_no2 + a.ammonia_oxidation[i]
		f2 := mineral_n_limit_factor(no2_available, d.nitrite_oxidation[i])
		a.nitrite_oxidation[i] = d.nitrite_oxidation[i] * f2
		l.soil_no2 = f2 < 1 ? 0 : no2_available - a.nitrite_oxidation[i]

		// NO3
		a.deposition[i] = d.deposition[i]
		no3_available :=
			l.soil_no3 + a.nitrite_oxidation[i] + a.nitrification_direct[i] + a.deposition[i]
		fc3 := mineral_n_limit_factor(no3_available, d.immobilisation_no3[i] + d.crop_uptake[i])
		a.immobilisation_no3[i] = d.immobilisation_no3[i] * fc3
		a.crop_uptake[i] = d.crop_uptake[i] * fc3
		no3_rest := no3_available - (a.immobilisation_no3[i] + a.crop_uptake[i])
		f3 := mineral_n_limit_factor(no3_rest, d.denitrification[i])
		a.denitrification[i] = d.denitrification[i] * f3
		l.soil_no3 = fc3 < 1 || f3 < 1 ? 0 : no3_rest - a.denitrification[i]

		after := l.soil_carbamid + l.soil_nh4 + l.soil_no2 + l.soil_no3
		expected :=
			before +
			a.mineralisation[i] +
			a.deposition[i] -
			(a.immobilisation_nh4[i] +
					a.immobilisation_no3[i] +
					a.volatilisation_urea[i] +
					a.volatilisation_manure[i] +
					a.denitrification[i] +
					a.crop_uptake[i])
		balance_error = max(balance_error, abs(after - expected))
	}
	return
}

// The general step's soil N part with `mineral_n_as_rates` on - replaces
// soil_organic_step + soil_transport_step (monica_model_general_step).
soil_n_step_as_rates :: proc(
	so: ^Soil_Organic,
	st: ^Soil_Transport,
	mean_air_temperature, precipitation, wind_speed: f64,
	crop: Crop_To_Soil,
) {
	sc := so.soil_column
	demand := &so.mineral_n_demand
	actual := &so.mineral_n_actual
	mineral_n_rates_clear(demand)

	// 1. the processes' demands, all from the start-of-step state
	soil_organic_n_processes(so, mean_air_temperature, wind_speed, demand)
	min_time_step_factor := soil_transport_prepare(st, crop)
	demand.deposition[0] = soil_transport_deposition_rate(st)
	for i in 0 ..< min(len(sc.layers), len(demand.crop_uptake)) {
		demand.crop_uptake[i] = st.n_uptake_from_layer[i] / sc.layers[i].layer_thickness_m
	}

	// 2. limit and apply
	so.mineral_n_balance_error = mineral_n_apply(sc.layers[:], demand, actual)
	// rounding is ~1e-18; 1e-9 kg N m-3 is 1e-5 kg N ha-1 in a 10 cm layer
	assert(so.mineral_n_balance_error < 1e-9, "soil mineral N mass balance violated")

	// 3. the actual rates back to their reporters, then the rest of the step
	crop_n_uptake := 0.0
	for i in 0 ..< min(len(sc.layers), len(actual.crop_uptake)) {
		st.n_uptake_from_layer[i] = actual.crop_uptake[i] * sc.layers[i].layer_thickness_m
		crop_n_uptake += st.n_uptake_from_layer[i]
	}
	sc.crop_n_uptake = crop_n_uptake // [kg m-2]
	soil_organic_take_actual_rates(so, actual)
	soil_organic_finish(so, crop)
	soil_transport_set_no3_aq(st)
	soil_transport_transport(st, min_time_step_factor)
}
