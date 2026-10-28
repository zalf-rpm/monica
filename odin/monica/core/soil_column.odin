// Port of the construction-time half of src/core/soilcolumn.{h,cpp}:
// AOM_Properties, SoilLayer, SoilColumn, makeSoilLayer, makeSoilColumn (the
// Soil::SoilPMs overload; the Cap'n Proto Reader overload is dropped, like
// every capnp deserialize/serialize in this port), the soillayer:: resolved
// getters, and the handful of soilcolumn:: getters that don't depend on a
// running CropModule/worksteps.
//
// Phase 6 checkpoint 1 adds the rest of soilcolumn.h's declared surface -
// applyMineralFertiliser*, applyIrrigation*, applyTillage, the delayed-N-min
// machinery, putCrop/removeCrop, clearTopDressingParams, deleteAOMPool - now
// that a live MonicaModel exists to call them. cropModule is upgraded from
// the phase-3 `rawptr` placeholder to a real `^Crop_Module` (safe: same
// package, no import cycle, and every other phase-4 module's own cropModule
// field was already typed this way since phase 5 checkpoint 2).
package core

import tl "../../support/tools"
import p "../params"
import "../soil"
import libc "core:c/libc"
import "../../support/cr_math"

// C++: struct monica::AOM_Properties
Aom_Properties :: struct {
	aom_slow:                      f64 `unit:"kgC m-3"`, // C in slowly decomposing AOM pool
	aom_fast:                      f64 `unit:"kgC m-3"`, // C in rapidly decomposing AOM pool
	aom_slow_dec_rate_to_smb_slow: f64,
	aom_slow_dec_rate_to_smb_fast: f64,
	aom_fast_dec_rate_to_smb_slow: f64,
	aom_fast_dec_rate_to_smb_fast: f64,
	aom_slow_dec_coeff:            f64,
	aom_fast_dec_coeff:            f64,
	aom_slow_dec_coeff_standard:   f64,
	aom_fast_dec_coeff_standard:   f64,
	part_aom_slow_to_smb_slow:     f64,
	part_aom_slow_to_smb_fast:     f64,
	cn_ratio_aom_slow:             f64 `unit:"1"`,
	cn_ratio_aom_fast:             f64 `unit:"1"`,
	days_after_application:        int,
	aom_dry_matter_content:        f64,
	aom_nh4_content:               f64,
	aom_slow_delta:                f64,
	aom_fast_delta:                f64,
	incorporation:                 bool,
	no_volatilization:             bool,
}

// C++ in-class initialisers
make_aom_properties :: proc() -> Aom_Properties {
	return Aom_Properties {
		aom_slow_dec_coeff_standard = 1.0,
		aom_fast_dec_coeff_standard = 1.0,
		cn_ratio_aom_slow = 1.0,
		cn_ratio_aom_fast = 1.0,
		no_volatilization = true,
	}
}

// C++: struct monica::SoilLayer (formerly composed via `Soil::SoilParameters
// sps;`, flattened directly - see the C++ comment at soilcolumn.h:139)
Soil_Layer :: struct {
	layer_thickness_m:        f64 `unit:"m"`,
	soil_water_flux:          f64 `unit:"mm"`, // water flux at the upper boundary [l m-2]
	aom_pool:                 [dynamic]Aom_Properties,
	som_slow:                 f64 `unit:"kgC m-3"`,
	som_fast:                 f64 `unit:"kgC m-3"`,
	smb_slow:                 f64 `unit:"kgC m-3"`,
	smb_fast:                 f64 `unit:"kgC m-3"`,
	soil_carbamid:            f64 `unit:"kgN m-3"`,
	soil_nh4:                 f64 `unit:"kgN m-3"`,
	soil_no2:                 f64 `unit:"kgN m-3"`,
	soil_no3:                 f64 `unit:"kgN m-3"`,
	soil_frozen:              bool,
	soil_sand_content:        f64 `unit:"kg kg-1"`,
	soil_clay_content:        f64 `unit:"kg kg-1"`,
	soil_ph:                  f64 `unit:"1"`,
	soil_stone_content:       f64 `unit:"m3 m-3"`,
	lambda:                   f64 `unit:"1"`,
	field_capacity:           f64 `unit:"m3 m-3"`,
	saturation:               f64 `unit:"m3 m-3"`,
	permanent_wilting_point:  f64 `unit:"m3 m-3"`,
	soil_texture:             string,
	soil_ammonium:            f64 `unit:"kgN m-3"`,
	soil_nitrate:             f64 `unit:"kgN m-3"`,
	soil_cn_ratio:            f64,
	soil_moisture_percent_fc: f64,
	// Raw/override values; -1 means "unset" and the resolved value has to be
	// computed via the corresponding soil_xyz() proc below.
	soil_raw_density:         f64 `unit:"kg m-3"`,
	soil_bulk_density:        f64 `unit:"kg m-3"`,
	soil_organic_carbon:      f64 `unit:"kg kg-1"`,
	soil_organic_matter:      f64 `unit:"kg kg-1"`,
	soil_moisture_m3:         f64 `unit:"m3 m-3"`,
	soil_temperature:         f64 `unit:"°C"`,
}

// C++ in-class initialisers
make_default_soil_layer :: proc(allocator := context.allocator) -> Soil_Layer {
	return Soil_Layer {
		layer_thickness_m = 0.1,
		soil_nh4 = 0.0001,
		soil_no2 = 0.001,
		soil_no3 = 0.0001,
		soil_sand_content = -1.0,
		soil_clay_content = -1.0,
		soil_ph = 6.9,
		lambda = -1.0,
		field_capacity = -1.0,
		saturation = -1.0,
		permanent_wilting_point = -1.0,
		soil_ammonium = 0.0005,
		soil_nitrate = 0.005,
		soil_cn_ratio = 10.0,
		soil_moisture_percent_fc = 100.0,
		soil_raw_density = -1.0,
		soil_bulk_density = -1.0,
		soil_organic_carbon = -1.0,
		soil_organic_matter = -1.0,
		soil_moisture_m3 = 0.25,
	}
}

// C++: SoilLayer monica::makeSoilLayer(double layer_thickness, const
//        SoilParameters& sps)
//
// NOTE: soil_no2 is NOT set from sps here (SoilParameters has no matching
// field) - it keeps make_default_soil_layer's 0.001 in-class default, exactly
// like the C++ (which default-constructs `SoilLayer sl;` and never touches
// sl.soil_no2 in this function body).
make_soil_layer :: proc(layer_thickness: f64, sps: ^soil.Soil_Parameters) -> Soil_Layer {
	sl := make_default_soil_layer()
	sl.layer_thickness_m = layer_thickness
	sl.soil_nh4 = sps.soil_ammonium
	sl.soil_no3 = sps.soil_nitrate
	sl.soil_sand_content = sps.soil_sand_content
	sl.soil_clay_content = sps.soil_clay_content
	sl.soil_ph = sps.soil_ph
	sl.soil_stone_content = sps.soil_stone_content
	sl.lambda = sps.lambda
	sl.field_capacity = sps.field_capacity
	sl.saturation = sps.saturation
	sl.permanent_wilting_point = sps.permanent_wilting_point
	sl.soil_texture = sps.soil_texture
	sl.soil_ammonium = sps.soil_ammonium
	sl.soil_nitrate = sps.soil_nitrate
	sl.soil_cn_ratio = sps.soil_cn_ratio
	sl.soil_moisture_percent_fc = sps.soil_moisture_percent_fc
	sl.soil_raw_density = sps.soil_raw_density
	sl.soil_bulk_density = sps.soil_bulk_density
	sl.soil_organic_carbon = sps.soil_organic_carbon
	sl.soil_organic_matter = sps.soil_organic_matter
	sl.soil_moisture_m3 = sps.field_capacity * sps.soil_moisture_percent_fc / 100.0
	return sl
}

// C++: double soillayer::soilMoisturePF(const SoilLayer*)
//
// Soil layer's moisture content, expressed as the common logarithm of the
// matric head in cm water column (Van Genuchten / Vereecken 1989).
soil_moisture_pf :: proc(sl: ^Soil_Layer) -> f64 {
	ps := soil.calc_van_genuchten_vereecken_params(
		sl.permanent_wilting_point,
		sl.saturation,
		sl.soil_sand_content,
		sl.soil_clay_content,
		soil_bulk_density(sl),
		soil_organic_carbon(sl),
	)

	sm := sl.soil_moisture_m3
	matric_head: f64
	if sm <= ps.theta_r {
		matric_head = 5.0e7
	} else {
		// cr_math, not core:math or libc: this nested pow(pow(...)-1,...) chain
		// is where the port first diverged from the C++ (last 2 ULP on the
		// Hohenfinow2 fixture, phase 3) - core:math and each platform's libm
		// round differently. cr_math is correctly rounded, and the C++ calls
		// the same library. See CONVENTIONS.md §1.
		matric_head =
			(1.0 / ps.alpha) *
			cr_math.pow(cr_math.pow((ps.theta_s - ps.theta_r) / (sm - ps.theta_r), 1 / ps.m) - 1, 1 / ps.n)
	}
	pf := cr_math.log10(matric_head)

	// set to a "small" number when soil_moisture_m3 is close to saturation
	// (matric_head < 1 -> log10(matric_head) < 0)
	return pf < 0.0 ? 5.0e-7 : pf
}

// C++: double soillayer::soilNmin(const SoilLayer*) - soil mineral N content [kg m-3]
soil_nmin :: proc(sl: ^Soil_Layer) -> f64 {
	return sl.soil_no3 + sl.soil_no2 + sl.soil_nh4
}

// C++: double soillayer::soilSiltContent(const SoilLayer*) - (Schluff)
soil_silt_content :: proc(sl: ^Soil_Layer) -> f64 {
	return 1.0 - sl.soil_sand_content - sl.soil_clay_content
}

// C++: double soillayer::soilRawDensity(const SoilLayer*)
soil_raw_density :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.soil_raw_density < 0 {
		return ((sl.soil_bulk_density / 1000.0) - (0.009 * 100.0 * sl.soil_clay_content)) * 1000.0
	}
	return sl.soil_raw_density
}

// C++: double soillayer::soilBulkDensity(const SoilLayer*)
soil_bulk_density :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.soil_bulk_density < 0 {
		return ((sl.soil_raw_density / 1000.0) + (0.009 * 100.0 * sl.soil_clay_content)) * 1000.0
	}
	return sl.soil_bulk_density
}

// C++: double soillayer::soilOrganicCarbon(const SoilLayer*)
soil_organic_carbon :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.soil_organic_carbon < 0 {
		return sl.soil_organic_matter * soil.PO_SOM_TO_C
	}
	return sl.soil_organic_carbon
}

// C++: double soillayer::soilOrganicMatter(const SoilLayer*)
soil_organic_matter :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.soil_organic_matter < 0 {
		return sl.soil_organic_carbon / soil.PO_SOM_TO_C
	}
	return sl.soil_organic_matter
}

// C++: struct monica::SoilColumn::DelayedNMinApplicationParams
Delayed_N_Min_Application_Params :: struct {
	fp:                         p.Mineral_Fertilizer_Parameters,
	sampling_depth:             f64,
	crop_n_target:              f64,
	crop_n_target_30:           f64,
	fertilizer_min_application: f64,
	fertilizer_max_application: f64,
	top_dressing_delay:         int,
}

// C++: struct monica::SoilColumn
Soil_Column :: struct {
	layers:                      [dynamic]Soil_Layer,
	surface_water_storage:       f64 `unit:"mm"`,
	// vs_InterceptionStorage:    f64, // [mm]
	groundwater_table_layer_idx: int,
	flux_at_lower_boundary:      f64 `unit:"mm"`,
	crop_n_uptake:               f64 `unit:"kgN m-2"`,
	soil_surface_temperature:    f64 `unit:"°C"`,
	snow_depth_mm:               f64,
	max_mineralisation_depth_m:  f64,
	number_of_organic_layers:    int,
	top_dressing:                f64,
	top_dressing_partition:      p.Mineral_Fertilizer_Parameters,
	top_dressing_delay:          int,
	delayed_n_min_applications:  [dynamic]Delayed_N_Min_Application_Params,

	// C++: CropModule *cropModule{nullptr}
	crop_module:                 ^Crop_Module,
}

// C++ in-class initialisers
make_default_soil_column :: proc(allocator := context.allocator) -> Soil_Column {
	sc: Soil_Column
	sc.layers = make([dynamic]Soil_Layer, 0, allocator)
	sc.max_mineralisation_depth_m = 0.4
	return sc
}

// C++: kj::Own<SoilColumn> monica::makeSoilColumn(double layerThickness,
//        double maxMineralisationDepth, const Soil::SoilPMs& soilParams)
//
// Returns by value rather than a heap-owned pointer (kj::Own<T> -> per
// CONVENTIONS prep 3 this would be std::unique_ptr<T>): nothing yet holds a
// back-pointer into a SoilColumn from elsewhere (that starts in phase 6, when
// MonicaModel's submodules are wired up - see plan-odin.md's ownership note).
make_soil_column :: proc(
	layer_thickness: f64,
	max_mineralisation_depth: f64,
	soil_params: []soil.Soil_Parameters,
	allocator := context.allocator,
) -> Soil_Column {
	sc := make_default_soil_column(allocator)
	sc.max_mineralisation_depth_m = max_mineralisation_depth
	for sp_in in soil_params {
		sp := sp_in
		append(&sc.layers, make_soil_layer(layer_thickness, &sp))
	}
	sc.number_of_organic_layers = calculate_number_of_organic_layers(&sc)
	return sc
}

// C++: int monica::soilcolumn::calculateNumberOfOrganicLayers(const SoilColumn*)
//
// Number of organic layers, usually the number of layers in the first
// max_mineralisation_depth of soil.
calculate_number_of_organic_layers :: proc(sc: ^Soil_Column) -> int {
	lsum := 0.0
	count := 0
	for i := 0; i < len(sc.layers); i += 1 {
		count += 1
		lsum += sc.layers[i].layer_thickness_m
		if lsum >= sc.max_mineralisation_depth_m {
			break
		}
	}
	return count
}

// C++: inline size_t monica::soilcolumn::numberOfLayers(const SoilColumn*)
number_of_layers :: proc(sc: ^Soil_Column) -> int {
	return len(sc.layers)
}

// C++: inline size_t monica::soilcolumn::numberOfOrganicLayers(const SoilColumn*)
number_of_organic_layers :: proc(sc: ^Soil_Column) -> int {
	return sc.number_of_organic_layers
}

// C++: inline double monica::soilcolumn::layerThickness(const SoilColumn*)
//
// By definition all layers have the same size, so only the first layer's
// thickness is returned.
soil_column_layer_thickness :: proc(sc: ^Soil_Column) -> f64 {
	return sc.layers[0].layer_thickness_m
}

// C++: inline double monica::soilcolumn::dailyCropNUptake(const SoilColumn*)
// [kg N ha-1 d-1]
daily_crop_n_uptake :: proc(sc: ^Soil_Column) -> f64 {
	return sc.crop_n_uptake * 10000.0
}

// C++: size_t monica::soilcolumn::getLayerNumberForDepth(const SoilColumn*, double)
//
// Index of the layer that lies at the given depth [m].
get_layer_number_for_depth :: proc(sc: ^Soil_Column, depth: f64) -> int {
	layer := 0
	accu_depth := 0.0
	lt := sc.layers[0].layer_thickness_m
	for i := 0; i < len(sc.layers); i += 1 {
		accu_depth += lt
		if depth <= accu_depth {
			break
		}
		layer += 1
	}
	return layer
}

// C++: double monica::soilcolumn::sumSoilTemperature(const SoilColumn*, int)
//
// Sum of soil temperature over the first `layers` layers.
sum_soil_temperature :: proc(sc: ^Soil_Column, layers: int) -> f64 {
	accu := 0.0
	for i := 0; i < layers; i += 1 {
		accu += sc.layers[i].soil_temperature
	}
	return accu
}

// ---------------------------------------------------------------------------
// Phase 6 checkpoint 1: workstep/orchestration-facing SoilColumn mutators.
// ---------------------------------------------------------------------------

// C++: void monica::soilcolumn::putCrop(SoilColumn*, CropModule*)
put_crop :: proc(sc: ^Soil_Column, cm: ^Crop_Module) {
	sc.crop_module = cm
}

// C++: void monica::soilcolumn::removeCrop(SoilColumn*)
remove_crop :: proc(sc: ^Soil_Column) {
	sc.crop_module = nil
}

// C++: void monica::soilcolumn::clearTopDressingParams(SoilColumn*)
clear_top_dressing_params :: proc(sc: ^Soil_Column) {
	sc.top_dressing = 0.0
	sc.top_dressing_delay = 0
}

// C++: void monica::soilcolumn::deleteAOMPool(SoilColumn*)
//
// Checks the content of each AOM pool; if the sum over all organic layers of
// a given pool is negligible, the pool is dropped from every organic layer's
// vo_AOM_Pool list.
delete_aom_pool :: proc(sc: ^Soil_Column) {
	i_aom_pool := 0
	for i_aom_pool < len(sc.layers[0].aom_pool) {
		sum_aom_slow := 0.0
		sum_aom_fast := 0.0

		for i_layer := 0; i_layer < sc.number_of_organic_layers; i_layer += 1 {
			sum_aom_slow += sc.layers[i_layer].aom_pool[i_aom_pool].aom_slow
			sum_aom_fast += sc.layers[i_layer].aom_pool[i_aom_pool].aom_fast
		}

		if (sum_aom_slow + sum_aom_fast) < 0.00001 {
			for i_layer := 0; i_layer < sc.number_of_organic_layers; i_layer += 1 {
				ordered_remove(&sc.layers[i_layer].aom_pool, i_aom_pool)
			}
		} else {
			i_aom_pool += 1
		}
	}
}

// C++: void monica::soilcolumn::applyMineralFertiliser(SoilColumn*,
//        MineralFertilizerParameters, double)
apply_mineral_fertiliser :: proc(
	sc: ^Soil_Column,
	fp: p.Mineral_Fertilizer_Parameters,
	amount: f64,
) {
	// [kg N ha-1 -> kg m-3]
	kg_ha_to_kg_m3 := 10000.0 * sc.layers[0].layer_thickness_m
	sc.layers[0].soil_no3 += amount * fp.no3 / kg_ha_to_kg_m3
	sc.layers[0].soil_nh4 += amount * fp.nh4 / kg_ha_to_kg_m3
	sc.layers[0].soil_carbamid += amount * fp.carbamid / kg_ha_to_kg_m3
}

// C++: double monica::soilcolumn::applyMineralFertiliserViaNMinMethod(...)
//
// Parameter order (..., min, max, ...) and conditions as in the original C++
// (SoilColumn::applyMineralFertiliserViaNMinMethod): below the minimum -> no
// fertilisation; above the maximum -> apply the maximum, the rest later as
// top dressing. The converted C++ this was ported from had min and max
// swapped in both conditions (commit d740453), so it never fertilised less
// than the maximum and capped everything at the minimum - fixed in both.
apply_mineral_fertiliser_via_n_min_method :: proc(
	sc: ^Soil_Column,
	fertiliser_partition: p.Mineral_Fertilizer_Parameters,
	sampling_depth: f64,
	crop_n_target_value: f64,
	crop_n_target_value30: f64,
	fertiliser_min_application: f64,
	fertiliser_max_application: f64,
	top_dressing_delay: int,
) -> f64 {
	if sc.layers[0].soil_moisture_m3 > sc.layers[0].field_capacity {
		append(
			&sc.delayed_n_min_applications,
			Delayed_N_Min_Application_Params {
				fp                         = fertiliser_partition,
				sampling_depth             = sampling_depth,
				crop_n_target              = crop_n_target_value,
				crop_n_target_30           = crop_n_target_value30,
				fertilizer_min_application = fertiliser_min_application,
				fertilizer_max_application = fertiliser_max_application,
				top_dressing_delay         = top_dressing_delay,
			},
		)
		return 0.0
	}

	layer30cm := get_layer_number_for_depth(sc, 0.3)
	layer_sampling_depth := get_layer_number_for_depth(sc, sampling_depth)

	soil_no3_sum := 0.0
	soil_nh4_sum := 0.0
	for i_layer := 0; i_layer < layer_sampling_depth; i_layer += 1 {
		soil_no3_sum += sc.layers[i_layer].soil_no3
		soil_nh4_sum += sc.layers[i_layer].soil_nh4
	}

	soil_no3_sum30 := 0.0
	soil_nh4_sum30 := 0.0
	for i_layer := 0; i_layer < layer30cm; i_layer += 1 {
		soil_no3_sum30 += sc.layers[i_layer].soil_no3
		soil_nh4_sum30 += sc.layers[i_layer].soil_nh4
	}

	// Converts [kg N ha-1] to [kg N m-3]
	crop_n_target_vol := crop_n_target_value / 10000.0 / sc.layers[0].layer_thickness_m
	crop_n_target_vol30 := crop_n_target_value30 / 10000.0 / sc.layers[0].layer_thickness_m

	fertiliser_demand_vol := crop_n_target_vol - (soil_no3_sum + soil_nh4_sum)
	fertiliser_demand_vol30 := crop_n_target_vol30 - (soil_no3_sum30 + soil_nh4_sum30)

	// Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
	fertiliser_demand := fertiliser_demand_vol * 10000.0 * sc.layers[0].layer_thickness_m
	fertiliser_demand30 := fertiliser_demand_vol30 * 10000.0 * sc.layers[0].layer_thickness_m

	fertiliser_recommendation := max(fertiliser_demand, fertiliser_demand30)

	if fertiliser_recommendation < fertiliser_min_application {
		fertiliser_recommendation = 0.0
	}

	if fertiliser_recommendation > fertiliser_max_application {
		sc.top_dressing = fertiliser_recommendation - fertiliser_max_application
		sc.top_dressing_partition = fertiliser_partition
		sc.top_dressing_delay = top_dressing_delay
		fertiliser_recommendation = fertiliser_max_application
	}

	apply_mineral_fertiliser(sc, fertiliser_partition, fertiliser_recommendation)

	return fertiliser_recommendation
}

// C++: double monica::soilcolumn::applyMineralFertiliserViaNDemand(...)
apply_mineral_fertiliser_via_n_demand :: proc(
	sc: ^Soil_Column,
	fp: p.Mineral_Fertilizer_Parameters,
	demand_depth: f64,
	ndemand_kg_ha: f64,
) -> f64 {
	sum_soil_nkg_ha := 0.0
	depth_cm := 0
	i := 0
	for &layer in sc.layers {
		layer_size := layer.layer_thickness_m
		depth_cm += int(layer_size * 100.0)

		// convert [kg N m-3] to [kg N ha-1]
		sum_soil_nkg_ha += (sc.layers[i].soil_no3 + sc.layers[i].soil_nh4) * 10000.0 * layer_size

		if depth_cm >= int(demand_depth * 100) {
			break
		}

		i += 1
	}

	fertilizer_recommendation := max(0.0, ndemand_kg_ha - sum_soil_nkg_ha)
	if fertilizer_recommendation > 0 {
		apply_mineral_fertiliser(sc, fp, fertilizer_recommendation)
	}

	return fertilizer_recommendation
}

// C++: double monica::soilcolumn::applyPossibleDelayedFerilizer(SoilColumn*)
//
// NOTE(c++-quirk): the C++ takes `auto delayedApps = sc->_delayedNMinApplications;`
// - a full copy of the list - and loops `while (!delayedApps.empty())`,
// popping from *both* the copy and the real list once per iteration. Since
// apply_mineral_fertiliser_via_n_min_method can itself re-append to the real
// list (still-too-wet re-delay), looping on the real list's own length
// directly is not equivalent - it can loop forever (each iteration removes
// one entry and, if still too wet, adds one back, net zero). The C++ avoids
// this by bounding the loop on the copy's size instead, which only shrinks:
// re-appended entries land on the real list and simply roll over to the next
// day's call, never extending *this* call's iteration count. Reproduced by
// snapshotting the original length up front instead of copying the list.
apply_possible_delayed_fertilizer :: proc(sc: ^Soil_Column) -> f64 {
	n_amount := 0.0
	original_len := len(sc.delayed_n_min_applications)
	for _ in 0 ..< original_len {
		da := sc.delayed_n_min_applications[0]
		n_amount += apply_mineral_fertiliser_via_n_min_method(
			sc,
			da.fp,
			da.sampling_depth,
			da.crop_n_target,
			da.crop_n_target_30,
			da.fertilizer_min_application,
			da.fertilizer_max_application,
			da.top_dressing_delay,
		)
		ordered_remove(&sc.delayed_n_min_applications, 0)
	}
	return n_amount
}

// C++: double monica::soilcolumn::applyPossibleTopDressing(SoilColumn*)
apply_possible_top_dressing :: proc(sc: ^Soil_Column) -> f64 {
	amount := 0.0

	if sc.top_dressing_delay > 0 {
		sc.top_dressing_delay -= 1
	} else if sc.top_dressing_delay == 0 && sc.top_dressing > 0.0 {
		amount = sc.top_dressing
		apply_mineral_fertiliser(sc, sc.top_dressing_partition, amount)
		sc.top_dressing = 0
	}
	return amount
}

// C++: std::pair<bool,double> monica::soilcolumn::applyIrrigationViaTrigger(
//        SoilColumn*, const AutomaticIrrigationParameters&)
apply_irrigation_via_trigger :: proc(
	sc: ^Soil_Column,
	aips: ^p.Automatic_Irrigation_Parameters,
) -> (
	triggered: bool,
	amount: f64,
) {
	if sc.crop_module == nil {
		return false, 0
	}

	s := sc.crop_module.crop_params.cultivar_params.heat_sum_irrigation_start
	e := sc.crop_module.crop_params.cultivar_params.heat_sum_irrigation_end
	cts := sc.crop_module.current_total_temperature_sum
	if cts < s || cts > e || aips.threshold < 0.0 {
		return false, 0
	}

	act_paw := 0.0 // actualPlantAvailableWater
	max_paw := 0.0 // maxPlantAvailableWater
	layer_depth_m := 0.0
	for i := 0; i < len(sc.layers) && layer_depth_m < aips.critical_moisture_depth_m; i += 1 {
		li := &sc.layers[i]
		smi := li.soil_moisture_m3
		fci := li.field_capacity
		pwpi := li.permanent_wilting_point
		lti := li.layer_thickness_m

		act_paw += (smi - pwpi) * lti * 1000.0 // [mm]
		max_paw += (fci - pwpi) * lti * 1000.0 // [mm]

		layer_depth_m += lti
	}
	if tl.flt_equal_zero(max_paw) {
		return false, 0
	}
	fract_paw := act_paw / max_paw
	if fract_paw <= aips.threshold {
		added_irrigation_water := 0.0
		if aips.amount > 0.0 {
			apply_irrigation(sc, aips.amount, aips.nitrate_concentration)
			added_irrigation_water = aips.amount
		} else if aips.percent_nfc > 0.0 {
			layer_depth_m = 0.0
			for i := 0; i < len(sc.layers) && layer_depth_m < aips.critical_moisture_depth_m; i += 1 {
				li := &sc.layers[i]
				smi := li.soil_moisture_m3
				fci := li.field_capacity
				pwpi := li.permanent_wilting_point
				lti := li.layer_thickness_m

				percent_nfc_i := (fci - pwpi) * aips.percent_nfc / 100.0
				pawi := smi - pwpi
				added_irrigation_water_at_layer := max(0.0, percent_nfc_i - pawi)
				added_irrigation_water += added_irrigation_water_at_layer
				li.soil_moisture_m3 = percent_nfc_i + pwpi
				// [kg m-3] = [mg dm-3] * [dm3 m-2] / [m]
				nitrate_added_via_irrigation :=
					aips.nitrate_concentration *
					added_irrigation_water_at_layer /
					li.layer_thickness_m /
					1000000.0
				li.soil_no3 += nitrate_added_via_irrigation

				layer_depth_m += lti
			}
		} else {
			return false, 0
		}

		return true, added_irrigation_water
	}

	return false, 0
}

// C++: void monica::soilcolumn::applyIrrigation(SoilColumn*, double, double)
apply_irrigation :: proc(sc: ^Soil_Column, amount: f64, nitrate_concentration: f64 = 0) {
	// Adding irrigation water amount to surface water storage
	sc.surface_water_storage += amount // [mm]
	nitrate_added_via_irrigation :=
		nitrate_concentration * amount / sc.layers[0].layer_thickness_m / 1000000.0 // -> //[kg m-3]// [mg dm-3]//[dm3 m-2]// [m]

	// adding N from irrigation water to top soil nitrate pool
	sc.layers[0].soil_no3 += nitrate_added_via_irrigation
}

// C++: void monica::soilcolumn::applyTillage(SoilColumn*, double)
//
// Averages every affected layer's mutable soil parameters (tillage mixes the
// soil down to `depth`).
apply_tillage :: proc(sc: ^Soil_Column, depth: f64) {
	layer_index := get_layer_number_for_depth(sc, depth) + 1

	// C++ local var is named soil_organic_carbon too; renamed vs_soc here only
	// to avoid shadowing the soil_organic_carbon(sl) resolved-getter proc this
	// same package already defines (C++ has separate namespaces for the two).
	soc := 0.0
	soil_temperature := 0.0
	soil_moisture := 0.0
	som_slow := 0.0
	som_fast := 0.0
	smb_slow := 0.0
	smb_fast := 0.0
	carbamid := 0.0
	nh4 := 0.0
	no2 := 0.0
	no3 := 0.0

	for i := 0; i < layer_index; i += 1 {
		soc += soil_organic_carbon(&sc.layers[i])
		soil_temperature += sc.layers[i].soil_temperature
		soil_moisture += sc.layers[i].soil_moisture_m3
		som_slow += sc.layers[i].som_slow
		som_fast += sc.layers[i].som_fast
		smb_slow += sc.layers[i].smb_slow
		smb_fast += sc.layers[i].smb_fast
		carbamid += sc.layers[i].soil_carbamid
		nh4 += sc.layers[i].soil_nh4
		no2 += sc.layers[i].soil_no2
		no3 += sc.layers[i].soil_no3
	}

	li := f64(layer_index)

	soc /= li
	soil_temperature /= li
	soil_moisture /= li
	som_slow /= li
	som_fast /= li
	smb_slow /= li
	smb_fast /= li
	carbamid /= li
	nh4 /= li
	no2 /= li
	no3 /= li

	for i := 0; i < layer_index; i += 1 {
		sc.layers[i].soil_organic_carbon = soc
		sc.layers[i].soil_temperature = soil_temperature
		sc.layers[i].soil_moisture_m3 = soil_moisture
		sc.layers[i].som_slow = som_slow
		sc.layers[i].som_fast = som_fast
		sc.layers[i].smb_slow = smb_slow
		sc.layers[i].smb_fast = smb_fast
		sc.layers[i].soil_carbamid = carbamid
		sc.layers[i].soil_nh4 = nh4
		sc.layers[i].soil_no2 = no2
		sc.layers[i].soil_no3 = no3
	}

	// NOTE(c++-quirk): the C++ "merge aom pool" block (computes per-pool-index
	// mean aom_slow/aom_fast across the affected layers, then tries to
	// write the means back) uses `for (auto aomp : layer.vo_AOM_Pool)` for the
	// write-back loop - a by-value range-for copy, not `auto &aomp`. Every
	// `aomp.aom_slow = ...` mutates only that loop-local copy, never
	// `layer.vo_AOM_Pool[pool_index]` itself, so the entire block is a provable
	// no-op: it computes averages and then discards them without touching any
	// SoilColumn state (no debug output either - that block's cout lines are
	// all commented out in the reference source). Omitted here rather than
	// reproduced as dead computation with an unreachable write-back.
}
