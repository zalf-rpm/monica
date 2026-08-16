// Port of the construction-time half of src/core/soilcolumn.{h,cpp}:
// AOM_Properties, SoilLayer, SoilColumn, makeSoilLayer, makeSoilColumn (the
// Soil::SoilPMs overload; the Cap'n Proto Reader overload is dropped, like
// every capnp deserialize/serialize in this port), the soillayer:: resolved
// getters, and the handful of soilcolumn:: getters that don't depend on a
// running CropModule/worksteps.
//
// Everything else soilcolumn.h declares (applyMineralFertiliser*,
// applyIrrigation*, applyTillage, the delayed-N-min machinery, putCrop/
// removeCrop, clearTopDressingParams, deleteAOMPool) is workstep/orchestration
// logic that runs against a live MonicaModel - phase 6 (see plan-odin.md), not
// phase 3 (soil setup). SoilColumn.DelayedNMinApplicationParams and the
// _delayedNMinApplications list are deliberately not ported here either: they
// are read and written exclusively by that phase-6 code, always start empty,
// and are not part of the phase-3 oracle (initial per-layer state right after
// construction). CropModule *cropModule is kept as a `rawptr` placeholder
// (never dereferenced by anything in this file) since CropModule itself is
// phase 5.
package core

import libc "core:c/libc"
import p "../params"
import "../soil"

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

// C++: struct monica::SoilColumn (see the package comment for the fields
// deliberately not ported yet: DelayedNMinApplicationParams,
// _delayedNMinApplications)
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

	// C++: CropModule *cropModule{nullptr} - CropModule is phase 5; never
	// dereferenced by anything ported so far.
	cropModule: rawptr,
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
