// The soil mineral N owner's limiting rule (core/soil_mineral_n.odin, plan
// phase I): demands within the pools are applied as they are, the soil's own
// transformations share what is left after the committed claims, the pools
// stay non-negative and the mass balance closes.
package tests

import "core:testing"
import mcore "../monica/core"

@(private = "file")
one_layer :: proc(carbamid, nh4, no2, no3: f64) -> [1]mcore.Soil_Layer {
	l: [1]mcore.Soil_Layer
	l[0].layer_thickness_m = 0.1
	l[0].soil_carbamid = carbamid
	l[0].soil_nh4 = nh4
	l[0].soil_no2 = no2
	l[0].soil_no3 = no3
	return l
}

@(private = "file")
EPS :: 1e-15

@(test)
test_mineral_n_unlimited :: proc(t: ^testing.T) {
	layers := one_layer(0.002, 0.010, 0.001, 0.020)
	d := mcore.make_mineral_n_rates(1, context.temp_allocator)
	a := mcore.make_mineral_n_rates(1, context.temp_allocator)
	d.hydrolysis[0] = 0.001
	d.mineralisation[0] = 0.0005
	d.volatilisation_urea[0] = 0.0002
	d.ammonia_oxidation[0] = 0.001
	d.nitrite_oxidation[0] = 0.0009
	d.denitrification[0] = 0.0003
	d.crop_uptake[0] = 0.002
	d.deposition[0] = 0.00001

	err := mcore.mineral_n_apply(layers[:], &d, &a)
	testing.expect(t, err < EPS)
	testing.expect_value(t, a.ammonia_oxidation[0], d.ammonia_oxidation[0])
	testing.expect_value(t, a.crop_uptake[0], d.crop_uptake[0])
	testing.expect(t, abs(layers[0].soil_carbamid - 0.001) < EPS)
	testing.expect(t, abs(layers[0].soil_nh4 - (0.010 + 0.001 + 0.0005 - 0.0002 - 0.001)) < EPS)
	testing.expect(t, abs(layers[0].soil_no2 - (0.001 + 0.001 - 0.0009)) < EPS)
	testing.expect(t, abs(layers[0].soil_no3 - (0.020 + 0.0009 + 0.00001 - 0.0003 - 0.002)) < EPS)
}

// NH4 doesn't suffice: immobilisation (committed) is served first, the
// transformations share the rest in proportion, NH4 ends at 0 - whatever the
// order the processes ran in.
@(test)
test_mineral_n_limited_proportionally :: proc(t: ^testing.T) {
	layers := one_layer(0, 0.010, 0, 0.020)
	d := mcore.make_mineral_n_rates(1, context.temp_allocator)
	a := mcore.make_mineral_n_rates(1, context.temp_allocator)
	d.immobilisation_nh4[0] = 0.004
	d.volatilisation_manure[0] = 0.003
	d.ammonia_oxidation[0] = 0.009 // with volatilisation 0.012 > the 0.006 left

	err := mcore.mineral_n_apply(layers[:], &d, &a)
	testing.expect(t, err < EPS)
	testing.expect_value(t, a.immobilisation_nh4[0], 0.004)
	testing.expect(t, abs(a.volatilisation_manure[0] - 0.0015) < EPS)
	testing.expect(t, abs(a.ammonia_oxidation[0] - 0.0045) < EPS)
	testing.expect_value(t, layers[0].soil_nh4, 0.0)
	// the ammonia oxidised so far reaches NO2, nothing else
	testing.expect(t, abs(layers[0].soil_no2 - 0.0045) < EPS)
}

// NO3: the committed claims (immobilisation, crop uptake) exceed the pool
// together - both scaled, denitrification gets nothing, NO3 ends at 0.
@(test)
test_mineral_n_committed_exceed :: proc(t: ^testing.T) {
	layers := one_layer(0, 0, 0, 0.010)
	d := mcore.make_mineral_n_rates(1, context.temp_allocator)
	a := mcore.make_mineral_n_rates(1, context.temp_allocator)
	d.immobilisation_no3[0] = 0.005
	d.crop_uptake[0] = 0.015
	d.denitrification[0] = 0.001

	err := mcore.mineral_n_apply(layers[:], &d, &a)
	testing.expect(t, err < EPS)
	testing.expect(t, abs(a.immobilisation_no3[0] - 0.0025) < EPS)
	testing.expect(t, abs(a.crop_uptake[0] - 0.0075) < EPS)
	testing.expect_value(t, a.denitrification[0], 0.0)
	testing.expect_value(t, layers[0].soil_no3, 0.0)
}

// Same-day inflows count: NO2 produced by ammonia oxidation can be oxidised
// on, and the NO3 from it can be denitrified, the same day.
@(test)
test_mineral_n_same_day_inflows :: proc(t: ^testing.T) {
	layers := one_layer(0, 0.010, 0, 0)
	d := mcore.make_mineral_n_rates(1, context.temp_allocator)
	a := mcore.make_mineral_n_rates(1, context.temp_allocator)
	d.ammonia_oxidation[0] = 0.002
	d.nitrite_oxidation[0] = 0.003 // more than the 0.002 arriving
	d.denitrification[0] = 0.001

	err := mcore.mineral_n_apply(layers[:], &d, &a)
	testing.expect(t, err < EPS)
	testing.expect_value(t, a.nitrite_oxidation[0], 0.002)
	testing.expect_value(t, layers[0].soil_no2, 0.0)
	testing.expect_value(t, a.denitrification[0], 0.001)
	testing.expect(t, abs(layers[0].soil_no3 - 0.001) < EPS)
}

@(test)
test_mineral_n_limit_factor :: proc(t: ^testing.T) {
	testing.expect_value(t, mcore.mineral_n_limit_factor(1, 0), 1.0)
	testing.expect_value(t, mcore.mineral_n_limit_factor(1, 0.5), 1.0)
	testing.expect_value(t, mcore.mineral_n_limit_factor(1, 4), 0.25)
	testing.expect_value(t, mcore.mineral_n_limit_factor(-1, 4), 0.0)
}
