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

import libc "core:c/libc"
import p "../params"
import "../soil"
import tl "../../support/tools"

// C++: struct monica::AOM_Properties
Aom_Properties :: struct {
	vo_AOM_Slow:                    f64, // C in slowly decomposing AOM pool [kgC m-3]
	vo_AOM_Fast:                    f64, // C in rapidly decomposing AOM pool [kgC m-3]
	vo_AOM_SlowDecRate_to_SMB_Slow: f64,
	vo_AOM_SlowDecRate_to_SMB_Fast: f64,
	vo_AOM_FastDecRate_to_SMB_Slow: f64,
	vo_AOM_FastDecRate_to_SMB_Fast: f64,
	vo_AOM_SlowDecCoeff:            f64,
	vo_AOM_FastDecCoeff:            f64,
	vo_AOM_SlowDecCoeffStandard:    f64,
	vo_AOM_FastDecCoeffStandard:    f64,
	vo_PartAOM_Slow_to_SMB_Slow:    f64,
	vo_PartAOM_Slow_to_SMB_Fast:    f64,
	vo_CN_Ratio_AOM_Slow:           f64,
	vo_CN_Ratio_AOM_Fast:           f64,
	vo_DaysAfterApplication:        int,
	vo_AOM_DryMatterContent:        f64,
	vo_AOM_NH4Content:              f64,
	vo_AOM_SlowDelta:               f64,
	vo_AOM_FastDelta:               f64,
	incorporation:                  bool,
	noVolatilization:               bool,
}

// C++ in-class initialisers
make_aom_properties :: proc() -> Aom_Properties {
	return Aom_Properties {
		vo_AOM_SlowDecCoeffStandard = 1.0,
		vo_AOM_FastDecCoeffStandard = 1.0,
		vo_CN_Ratio_AOM_Slow        = 1.0,
		vo_CN_Ratio_AOM_Fast        = 1.0,
		noVolatilization            = true,
	}
}

// C++: struct monica::SoilLayer (formerly composed via `Soil::SoilParameters
// sps;`, flattened directly - see the C++ comment at soilcolumn.h:139)
Soil_Layer :: struct {
	vs_LayerThickness: f64, // [m]
	vs_SoilWaterFlux:  f64, // water flux at the upper boundary [l m-2]

	vo_AOM_Pool: [dynamic]Aom_Properties,

	vs_SOM_Slow: f64, // [kg C m-3]
	vs_SOM_Fast: f64, // [kg C m-3]
	vs_SMB_Slow: f64, // [kg C m-3]
	vs_SMB_Fast: f64, // [kg C m-3]

	vs_SoilCarbamid: f64, // [kg Carbamide-N m-3]
	vs_SoilNH4:      f64, // [kg NH4-N m-3]
	vs_SoilNO2:      f64, // [kg NO2-N m-3]
	vs_SoilNO3:      f64, // [kg NO3-N m-3]
	vs_SoilFrozen:   bool,

	vs_SoilSandContent:       f64,
	vs_SoilClayContent:       f64,
	vs_SoilpH:                f64,
	vs_SoilStoneContent:      f64,
	vs_Lambda:                f64,
	vs_FieldCapacity:         f64,
	vs_Saturation:            f64,
	vs_PermanentWiltingPoint: f64,
	vs_SoilTexture:           string,
	vs_SoilAmmonium:          f64,
	vs_SoilNitrate:           f64,
	vs_Soil_CN_Ratio:         f64,
	vs_SoilMoisturePercentFC: f64,
	// Raw/override values; -1 means "unset" and the resolved value has to be
	// computed via the corresponding soil_xyz() proc below.
	vs_SoilRawDensity:    f64,
	vs_SoilBulkDensity:   f64,
	vs_SoilOrganicCarbon: f64,
	vs_SoilOrganicMatter: f64,

	vs_SoilMoisture_m3: f64, // [m3 m-3]
	vs_SoilTemperature: f64, // [degC]
}

// C++ in-class initialisers
make_default_soil_layer :: proc(allocator := context.allocator) -> Soil_Layer {
	return Soil_Layer {
		vs_LayerThickness = 0.1,
		vs_SoilNH4 = 0.0001,
		vs_SoilNO2 = 0.001,
		vs_SoilNO3 = 0.0001,
		vs_SoilSandContent = -1.0,
		vs_SoilClayContent = -1.0,
		vs_SoilpH = 6.9,
		vs_Lambda = -1.0,
		vs_FieldCapacity = -1.0,
		vs_Saturation = -1.0,
		vs_PermanentWiltingPoint = -1.0,
		vs_SoilAmmonium = 0.0005,
		vs_SoilNitrate = 0.005,
		vs_Soil_CN_Ratio = 10.0,
		vs_SoilMoisturePercentFC = 100.0,
		vs_SoilRawDensity = -1.0,
		vs_SoilBulkDensity = -1.0,
		vs_SoilOrganicCarbon = -1.0,
		vs_SoilOrganicMatter = -1.0,
		vs_SoilMoisture_m3 = 0.25,
	}
}

// C++: SoilLayer monica::makeSoilLayer(double vs_LayerThickness, const
//        SoilParameters& sps)
//
// NOTE: vs_SoilNO2 is NOT set from sps here (SoilParameters has no matching
// field) - it keeps make_default_soil_layer's 0.001 in-class default, exactly
// like the C++ (which default-constructs `SoilLayer sl;` and never touches
// sl.vs_SoilNO2 in this function body).
make_soil_layer :: proc(vs_layer_thickness: f64, sps: ^soil.Soil_Parameters) -> Soil_Layer {
	sl := make_default_soil_layer()
	sl.vs_LayerThickness = vs_layer_thickness
	sl.vs_SoilNH4 = sps.vs_SoilAmmonium
	sl.vs_SoilNO3 = sps.vs_SoilNitrate
	sl.vs_SoilSandContent = sps.vs_SoilSandContent
	sl.vs_SoilClayContent = sps.vs_SoilClayContent
	sl.vs_SoilpH = sps.vs_SoilpH
	sl.vs_SoilStoneContent = sps.vs_SoilStoneContent
	sl.vs_Lambda = sps.vs_Lambda
	sl.vs_FieldCapacity = sps.vs_FieldCapacity
	sl.vs_Saturation = sps.vs_Saturation
	sl.vs_PermanentWiltingPoint = sps.vs_PermanentWiltingPoint
	sl.vs_SoilTexture = sps.vs_SoilTexture
	sl.vs_SoilAmmonium = sps.vs_SoilAmmonium
	sl.vs_SoilNitrate = sps.vs_SoilNitrate
	sl.vs_Soil_CN_Ratio = sps.vs_Soil_CN_Ratio
	sl.vs_SoilMoisturePercentFC = sps.vs_SoilMoisturePercentFC
	sl.vs_SoilRawDensity = sps._vs_SoilRawDensity
	sl.vs_SoilBulkDensity = sps._vs_SoilBulkDensity
	sl.vs_SoilOrganicCarbon = sps._vs_SoilOrganicCarbon
	sl.vs_SoilOrganicMatter = sps._vs_SoilOrganicMatter
	sl.vs_SoilMoisture_m3 = sps.vs_FieldCapacity * sps.vs_SoilMoisturePercentFC / 100.0
	return sl
}

// C++: double soillayer::soilMoisturePF(const SoilLayer*)
//
// Soil layer's moisture content, expressed as the common logarithm of the
// matric head in cm water column (Van Genuchten / Vereecken 1989).
soil_moisture_pf :: proc(sl: ^Soil_Layer) -> f64 {
	ps := soil.calc_van_genuchten_vereecken_params(
		sl.vs_PermanentWiltingPoint,
		sl.vs_Saturation,
		sl.vs_SoilSandContent,
		sl.vs_SoilClayContent,
		soil_bulk_density(sl),
		soil_organic_carbon(sl),
	)

	sm := sl.vs_SoilMoisture_m3
	matric_head: f64
	if sm <= ps.thetaR {
		matric_head = 5.0e7
	} else {
		// libc.pow, not core:math.pow: the C++ reference build's pow()/log10()
		// resolve to the MSVC CRT, which is not always bit-identical to Odin's
		// pure-Odin core:math implementation (confirmed here: this nested
		// pow(pow(...)-1,...) chain diverged in the last 2 ULP against the
		// reference on the Hohenfinow2 fixture, while every other of the ~1,440
		// values in this checkpoint's oracle matched exactly). libc.pow/log10 are
		// FFI bindings to the platform C runtime, so on Windows they call the
		// exact same function the C++ build does. See plan-odin.md phase 3 /
		// CONVENTIONS.md for the general rule this establishes for phases 4-6.
		matric_head =
			(1.0 / ps.alpha) *
			libc.pow(libc.pow((ps.thetaS - ps.thetaR) / (sm - ps.thetaR), 1 / ps.m) - 1, 1 / ps.n)
	}
	pf := libc.log10(matric_head)

	// set to a "small" number when vs_SoilMoisture_m3 is close to vs_Saturation
	// (matric_head < 1 -> log10(matric_head) < 0)
	return pf < 0.0 ? 5.0e-7 : pf
}

// C++: double soillayer::soilNmin(const SoilLayer*) - soil mineral N content [kg m-3]
soil_nmin :: proc(sl: ^Soil_Layer) -> f64 {
	return sl.vs_SoilNO3 + sl.vs_SoilNO2 + sl.vs_SoilNH4
}

// C++: double soillayer::soilSiltContent(const SoilLayer*) - (Schluff)
soil_silt_content :: proc(sl: ^Soil_Layer) -> f64 {
	return 1.0 - sl.vs_SoilSandContent - sl.vs_SoilClayContent
}

// C++: double soillayer::soilRawDensity(const SoilLayer*)
soil_raw_density :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.vs_SoilRawDensity < 0 {
		return ((sl.vs_SoilBulkDensity / 1000.0) - (0.009 * 100.0 * sl.vs_SoilClayContent)) * 1000.0
	}
	return sl.vs_SoilRawDensity
}

// C++: double soillayer::soilBulkDensity(const SoilLayer*)
soil_bulk_density :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.vs_SoilBulkDensity < 0 {
		return ((sl.vs_SoilRawDensity / 1000.0) + (0.009 * 100.0 * sl.vs_SoilClayContent)) * 1000.0
	}
	return sl.vs_SoilBulkDensity
}

// C++: double soillayer::soilOrganicCarbon(const SoilLayer*)
soil_organic_carbon :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.vs_SoilOrganicCarbon < 0 {
		return sl.vs_SoilOrganicMatter * soil.PO_SOM_TO_C
	}
	return sl.vs_SoilOrganicCarbon
}

// C++: double soillayer::soilOrganicMatter(const SoilLayer*)
soil_organic_matter :: proc(sl: ^Soil_Layer) -> f64 {
	if sl.vs_SoilOrganicMatter < 0 {
		return sl.vs_SoilOrganicCarbon / soil.PO_SOM_TO_C
	}
	return sl.vs_SoilOrganicMatter
}

// C++: struct monica::SoilColumn::DelayedNMinApplicationParams
Delayed_N_Min_Application_Params :: struct {
	fp:                          p.Mineral_Fertilizer_Parameters,
	vf_SamplingDepth:            f64,
	vf_CropNTarget:              f64,
	vf_CropNTarget30:            f64,
	vf_FertiliserMinApplication: f64,
	vf_FertiliserMaxApplication: f64,
	vf_TopDressingDelay:         int,
}

// C++: struct monica::SoilColumn
Soil_Column :: struct {
	layers: [dynamic]Soil_Layer,

	vs_SurfaceWaterStorage:     f64, // [mm]
	vs_InterceptionStorage:     f64, // [mm]
	vm_GroundwaterTableLayer:   int,
	vs_FluxAtLowerBoundary:     f64,
	vq_CropNUptake:             f64, // [kg m-2]
	vt_SoilSurfaceTemperature:  f64,
	vm_SnowDepth:               f64,

	ps_MaxMineralisationDepth: f64,

	vs_NumberOfOrganicLayers: int,
	vf_TopDressing:           f64,
	vf_TopDressingPartition:  p.Mineral_Fertilizer_Parameters,
	vf_TopDressingDelay:      int,

	_delayedNMinApplications: [dynamic]Delayed_N_Min_Application_Params,

	// C++: CropModule *cropModule{nullptr}
	cropModule: ^Crop_Module,
}

// C++ in-class initialisers
make_default_soil_column :: proc(allocator := context.allocator) -> Soil_Column {
	sc: Soil_Column
	sc.layers = make([dynamic]Soil_Layer, 0, allocator)
	sc.ps_MaxMineralisationDepth = 0.4
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
	sc.ps_MaxMineralisationDepth = max_mineralisation_depth
	for sp_in in soil_params {
		sp := sp_in
		append(&sc.layers, make_soil_layer(layer_thickness, &sp))
	}
	sc.vs_NumberOfOrganicLayers = calculate_number_of_organic_layers(&sc)
	return sc
}

// C++: int monica::soilcolumn::calculateNumberOfOrganicLayers(const SoilColumn*)
//
// Number of organic layers, usually the number of layers in the first
// ps_MaxMineralisationDepth of soil.
calculate_number_of_organic_layers :: proc(sc: ^Soil_Column) -> int {
	lsum := 0.0
	count := 0
	for i := 0; i < len(sc.layers); i += 1 {
		count += 1
		lsum += sc.layers[i].vs_LayerThickness
		if lsum >= sc.ps_MaxMineralisationDepth {
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
	return sc.vs_NumberOfOrganicLayers
}

// C++: inline double monica::soilcolumn::layerThickness(const SoilColumn*)
//
// By definition all layers have the same size, so only the first layer's
// thickness is returned.
soil_column_layer_thickness :: proc(sc: ^Soil_Column) -> f64 {
	return sc.layers[0].vs_LayerThickness
}

// C++: inline double monica::soilcolumn::dailyCropNUptake(const SoilColumn*)
// [kg N ha-1 d-1]
daily_crop_n_uptake :: proc(sc: ^Soil_Column) -> f64 {
	return sc.vq_CropNUptake * 10000.0
}

// C++: size_t monica::soilcolumn::getLayerNumberForDepth(const SoilColumn*, double)
//
// Index of the layer that lies at the given depth [m].
get_layer_number_for_depth :: proc(sc: ^Soil_Column, depth: f64) -> int {
	layer := 0
	accu_depth := 0.0
	lt := sc.layers[0].vs_LayerThickness
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
		accu += sc.layers[i].vs_SoilTemperature
	}
	return accu
}

// ---------------------------------------------------------------------------
// Phase 6 checkpoint 1: workstep/orchestration-facing SoilColumn mutators.
// ---------------------------------------------------------------------------

// C++: void monica::soilcolumn::putCrop(SoilColumn*, CropModule*)
put_crop :: proc(sc: ^Soil_Column, cm: ^Crop_Module) {
	sc.cropModule = cm
}

// C++: void monica::soilcolumn::removeCrop(SoilColumn*)
remove_crop :: proc(sc: ^Soil_Column) {
	sc.cropModule = nil
}

// C++: void monica::soilcolumn::clearTopDressingParams(SoilColumn*)
clear_top_dressing_params :: proc(sc: ^Soil_Column) {
	sc.vf_TopDressing = 0.0
	sc.vf_TopDressingDelay = 0
}

// C++: void monica::soilcolumn::deleteAOMPool(SoilColumn*)
//
// Checks the content of each AOM pool; if the sum over all organic layers of
// a given pool is negligible, the pool is dropped from every organic layer's
// vo_AOM_Pool list.
delete_aom_pool :: proc(sc: ^Soil_Column) {
	i_AOMPool := 0
	for i_AOMPool < len(sc.layers[0].vo_AOM_Pool) {
		vo_SumAOM_Slow := 0.0
		vo_SumAOM_Fast := 0.0

		for i_Layer := 0; i_Layer < sc.vs_NumberOfOrganicLayers; i_Layer += 1 {
			vo_SumAOM_Slow += sc.layers[i_Layer].vo_AOM_Pool[i_AOMPool].vo_AOM_Slow
			vo_SumAOM_Fast += sc.layers[i_Layer].vo_AOM_Pool[i_AOMPool].vo_AOM_Fast
		}

		if (vo_SumAOM_Slow + vo_SumAOM_Fast) < 0.00001 {
			for i_Layer := 0; i_Layer < sc.vs_NumberOfOrganicLayers; i_Layer += 1 {
				ordered_remove(&sc.layers[i_Layer].vo_AOM_Pool, i_AOMPool)
			}
		} else {
			i_AOMPool += 1
		}
	}
}

// C++: void monica::soilcolumn::applyMineralFertiliser(SoilColumn*,
//        MineralFertilizerParameters, double)
apply_mineral_fertiliser :: proc(sc: ^Soil_Column, fp: p.Mineral_Fertilizer_Parameters, amount: f64) {
	// [kg N ha-1 -> kg m-3]
	kgHaTokgm3 := 10000.0 * sc.layers[0].vs_LayerThickness
	sc.layers[0].vs_SoilNO3 += amount * fp.vo_NO3 / kgHaTokgm3
	sc.layers[0].vs_SoilNH4 += amount * fp.vo_NH4 / kgHaTokgm3
	sc.layers[0].vs_SoilCarbamid += amount * fp.vo_Carbamid / kgHaTokgm3
}

// C++: double monica::soilcolumn::applyMineralFertiliserViaNMinMethod(...)
//
// NOTE(c++-quirk): callers of apply_possible_delayed_fertilizer below pass
// da.vf_FertiliserMinApplication into this proc's fertiliserMaxApplication
// parameter and da.vf_FertiliserMaxApplication into fertiliserMinApplication
// - looks swapped by name, but it exactly undoes the same positional swap
// this proc's own delayed-application append (further down, in
// apply_mineral_fertiliser_via_n_min_method's caller-adjacent code) performs
// when building a Delayed_N_Min_Application_Params from
// (fertiliserMaxApplication, fertiliserMinApplication) in that order. Net
// effect: correct values round-trip through the delayed-application queue:
// the storage field *names* are misleading, but reproduced exactly since the
// two swaps cancel out.
apply_mineral_fertiliser_via_n_min_method :: proc(
	sc: ^Soil_Column,
	fertiliserPartition: p.Mineral_Fertilizer_Parameters,
	samplingDepth: f64,
	cropNTargetValue: f64,
	cropNTargetValue30: f64,
	fertiliserMaxApplication: f64,
	fertiliserMinApplication: f64,
	topDressingDelay: int,
) -> f64 {
	if sc.layers[0].vs_SoilMoisture_m3 > sc.layers[0].vs_FieldCapacity {
		append(
			&sc._delayedNMinApplications,
			Delayed_N_Min_Application_Params {
				fp = fertiliserPartition,
				vf_SamplingDepth = samplingDepth,
				vf_CropNTarget = cropNTargetValue,
				vf_CropNTarget30 = cropNTargetValue30,
				// NOTE(c++-quirk): positions 5/6 of the C++ aggregate-init are
				// (fertiliserMaxApplication, fertiliserMinApplication), which land
				// on struct fields 5/6 (vf_FertiliserMinApplication,
				// vf_FertiliserMaxApplication) - i.e. swapped relative to the field
				// names. Reproduced exactly; see this proc's doc comment above.
				vf_FertiliserMinApplication = fertiliserMaxApplication,
				vf_FertiliserMaxApplication = fertiliserMinApplication,
				vf_TopDressingDelay = topDressingDelay,
			},
		)
		return 0.0
	}

	vf_Layer30cm := get_layer_number_for_depth(sc, 0.3)
	layerSamplingDepth := get_layer_number_for_depth(sc, samplingDepth)

	vf_SoilNO3Sum := 0.0
	vf_SoilNH4Sum := 0.0
	for i_Layer := 0; i_Layer < layerSamplingDepth; i_Layer += 1 {
		vf_SoilNO3Sum += sc.layers[i_Layer].vs_SoilNO3
		vf_SoilNH4Sum += sc.layers[i_Layer].vs_SoilNH4
	}

	vf_SoilNO3Sum30 := 0.0
	vf_SoilNH4Sum30 := 0.0
	for i_Layer := 0; i_Layer < vf_Layer30cm; i_Layer += 1 {
		vf_SoilNO3Sum30 += sc.layers[i_Layer].vs_SoilNO3
		vf_SoilNH4Sum30 += sc.layers[i_Layer].vs_SoilNH4
	}

	// Converts [kg N ha-1] to [kg N m-3]
	vf_CropNTargetValue := cropNTargetValue / 10000.0 / sc.layers[0].vs_LayerThickness
	vf_CropNTargetValue30 := cropNTargetValue30 / 10000.0 / sc.layers[0].vs_LayerThickness

	vf_FertiliserDemandVol := vf_CropNTargetValue - (vf_SoilNO3Sum + vf_SoilNH4Sum)
	vf_FertiliserDemandVol30 := vf_CropNTargetValue30 - (vf_SoilNO3Sum30 + vf_SoilNH4Sum30)

	// Converts fertiliser demand back from [kg N m-3] to [kg N ha-1]
	vf_FertiliserDemand := vf_FertiliserDemandVol * 10000.0 * sc.layers[0].vs_LayerThickness
	vf_FertiliserDemand30 := vf_FertiliserDemandVol30 * 10000.0 * sc.layers[0].vs_LayerThickness

	vf_FertiliserRecommendation := max(vf_FertiliserDemand, vf_FertiliserDemand30)

	if vf_FertiliserRecommendation < fertiliserMaxApplication {
		vf_FertiliserRecommendation = 0.0
	}

	if vf_FertiliserRecommendation > fertiliserMinApplication {
		sc.vf_TopDressing = vf_FertiliserRecommendation - fertiliserMinApplication
		sc.vf_TopDressingPartition = fertiliserPartition
		sc.vf_TopDressingDelay = topDressingDelay
		vf_FertiliserRecommendation = fertiliserMinApplication
	}

	apply_mineral_fertiliser(sc, fertiliserPartition, vf_FertiliserRecommendation)

	return vf_FertiliserRecommendation
}

// C++: double monica::soilcolumn::applyMineralFertiliserViaNDemand(...)
apply_mineral_fertiliser_via_n_demand :: proc(
	sc: ^Soil_Column,
	fp: p.Mineral_Fertilizer_Parameters,
	demandDepth: f64,
	NdemandKgHa: f64,
) -> f64 {
	sumSoilNkgHa := 0.0
	depthCm := 0
	i := 0
	for &layer in sc.layers {
		layerSize := layer.vs_LayerThickness
		depthCm += int(layerSize * 100.0)

		// convert [kg N m-3] to [kg N ha-1]
		sumSoilNkgHa += (sc.layers[i].vs_SoilNO3 + sc.layers[i].vs_SoilNH4) * 10000.0 * layerSize

		if depthCm >= int(demandDepth * 100) {
			break
		}

		i += 1
	}

	fertilizerRecommendation := max(0.0, NdemandKgHa - sumSoilNkgHa)
	if fertilizerRecommendation > 0 {
		apply_mineral_fertiliser(sc, fp, fertilizerRecommendation)
	}

	return fertilizerRecommendation
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
	original_len := len(sc._delayedNMinApplications)
	for _ in 0 ..< original_len {
		da := sc._delayedNMinApplications[0]
		n_amount += apply_mineral_fertiliser_via_n_min_method(
			sc,
			da.fp,
			da.vf_SamplingDepth,
			da.vf_CropNTarget,
			da.vf_CropNTarget30,
			da.vf_FertiliserMinApplication,
			da.vf_FertiliserMaxApplication,
			da.vf_TopDressingDelay,
		)
		ordered_remove(&sc._delayedNMinApplications, 0)
	}
	return n_amount
}

// C++: double monica::soilcolumn::applyPossibleTopDressing(SoilColumn*)
apply_possible_top_dressing :: proc(sc: ^Soil_Column) -> f64 {
	amount := 0.0

	if sc.vf_TopDressingDelay > 0 {
		sc.vf_TopDressingDelay -= 1
	} else if sc.vf_TopDressingDelay == 0 && sc.vf_TopDressing > 0.0 {
		amount = sc.vf_TopDressing
		apply_mineral_fertiliser(sc, sc.vf_TopDressingPartition, amount)
		sc.vf_TopDressing = 0
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
	if sc.cropModule == nil {
		return false, 0
	}

	s := sc.cropModule.cropParams.cultivarParams.pc_HeatSumIrrigationStart
	e := sc.cropModule.cropParams.cultivarParams.pc_HeatSumIrrigationEnd
	cts := sc.cropModule.vc_CurrentTotalTemperatureSum
	if cts < s || cts > e || aips.threshold < 0.0 {
		return false, 0
	}

	actPAW := 0.0 // actualPlantAvailableWater
	maxPAW := 0.0 // maxPlantAvailableWater
	layerDepthM := 0.0
	for i := 0; i < len(sc.layers) && layerDepthM < aips.criticalMoistureDepthM; i += 1 {
		li := &sc.layers[i]
		smi := li.vs_SoilMoisture_m3
		fci := li.vs_FieldCapacity
		pwpi := li.vs_PermanentWiltingPoint
		lti := li.vs_LayerThickness

		actPAW += (smi - pwpi) * lti * 1000.0 // [mm]
		maxPAW += (fci - pwpi) * lti * 1000.0 // [mm]

		layerDepthM += lti
	}
	if tl.flt_equal_zero(maxPAW) {
		return false, 0
	}
	fractPAW := actPAW / maxPAW
	if fractPAW <= aips.threshold {
		addedIrrigationWater := 0.0
		if aips.amount > 0.0 {
			apply_irrigation(sc, aips.amount, aips.nitrateConcentration)
			addedIrrigationWater = aips.amount
		} else if aips.percentNFC > 0.0 {
			layerDepthM = 0.0
			for i := 0; i < len(sc.layers) && layerDepthM < aips.criticalMoistureDepthM; i += 1 {
				li := &sc.layers[i]
				smi := li.vs_SoilMoisture_m3
				fci := li.vs_FieldCapacity
				pwpi := li.vs_PermanentWiltingPoint
				lti := li.vs_LayerThickness

				percentNFCi := (fci - pwpi) * aips.percentNFC / 100.0
				pawi := smi - pwpi
				addedIrrigationWaterAtLayer := max(0.0, percentNFCi - pawi)
				addedIrrigationWater += addedIrrigationWaterAtLayer
				li.vs_SoilMoisture_m3 = percentNFCi + pwpi
				nitrateAddedViaIrrigation := // -> //[kg m-3]
					aips.nitrateConcentration * // [mg dm-3]
					addedIrrigationWaterAtLayer / //[dm3 m-2]
					li.vs_LayerThickness / 1000000.0 // [m]
				li.vs_SoilNO3 += nitrateAddedViaIrrigation

				layerDepthM += lti
			}
		} else {
			return false, 0
		}

		return true, addedIrrigationWater
	}

	return false, 0
}

// C++: void monica::soilcolumn::applyIrrigation(SoilColumn*, double, double)
apply_irrigation :: proc(sc: ^Soil_Column, amount: f64, nitrateConcentration: f64 = 0) {
	// Adding irrigation water amount to surface water storage
	sc.vs_SurfaceWaterStorage += amount // [mm]
	nitrateAddedViaIrrigation := // -> //[kg m-3]
		nitrateConcentration * // [mg dm-3]
		amount / //[dm3 m-2]
		sc.layers[0].vs_LayerThickness / 1000000.0 // [m]

	// adding N from irrigation water to top soil nitrate pool
	sc.layers[0].vs_SoilNO3 += nitrateAddedViaIrrigation
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
	vs_soc := 0.0
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
		vs_soc += soil_organic_carbon(&sc.layers[i])
		soil_temperature += sc.layers[i].vs_SoilTemperature
		soil_moisture += sc.layers[i].vs_SoilMoisture_m3
		som_slow += sc.layers[i].vs_SOM_Slow
		som_fast += sc.layers[i].vs_SOM_Fast
		smb_slow += sc.layers[i].vs_SMB_Slow
		smb_fast += sc.layers[i].vs_SMB_Fast
		carbamid += sc.layers[i].vs_SoilCarbamid
		nh4 += sc.layers[i].vs_SoilNH4
		no2 += sc.layers[i].vs_SoilNO2
		no3 += sc.layers[i].vs_SoilNO3
	}

	li := f64(layer_index)

	vs_soc /= li
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
		sc.layers[i].vs_SoilOrganicCarbon = vs_soc
		sc.layers[i].vs_SoilTemperature = soil_temperature
		sc.layers[i].vs_SoilMoisture_m3 = soil_moisture
		sc.layers[i].vs_SOM_Slow = som_slow
		sc.layers[i].vs_SOM_Fast = som_fast
		sc.layers[i].vs_SMB_Slow = smb_slow
		sc.layers[i].vs_SMB_Fast = smb_fast
		sc.layers[i].vs_SoilCarbamid = carbamid
		sc.layers[i].vs_SoilNH4 = nh4
		sc.layers[i].vs_SoilNO2 = no2
		sc.layers[i].vs_SoilNO3 = no3
	}

	// NOTE(c++-quirk): the C++ "merge aom pool" block (computes per-pool-index
	// mean vo_AOM_Slow/vo_AOM_Fast across the affected layers, then tries to
	// write the means back) uses `for (auto aomp : layer.vo_AOM_Pool)` for the
	// write-back loop - a by-value range-for copy, not `auto &aomp`. Every
	// `aomp.vo_AOM_Slow = ...` mutates only that loop-local copy, never
	// `layer.vo_AOM_Pool[pool_index]` itself, so the entire block is a provable
	// no-op: it computes averages and then discards them without touching any
	// SoilColumn state (no debug output either - that block's cout lines are
	// all commented out in the reference source). Omitted here rather than
	// reproduced as dead computation with an unreachable write-back.
}
