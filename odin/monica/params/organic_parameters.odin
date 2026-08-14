// Tranche 3a of the src/core/monica-parameters.{h,cpp} port:
// YieldComponent, AutomaticHarvestParameters, NMinCropParameters,
// OrganicMatterParameters, OrganicFertilizerParameters, CropResidueParameters.
package params

import jx "../../support/jsonx"
import tl "../../support/tools"

// ---------------------------------------------------------------------------
// YieldComponent
// ---------------------------------------------------------------------------

// C++: struct monica::YieldComponent
Yield_Component :: struct {
	organId:         int,
	yieldPercentage: f64,
	yieldDryMatter:  f64,
}

// C++ default: organId{-1}
make_yield_component :: proc() -> Yield_Component {
	return Yield_Component{organId = -1}
}

// C++: Errors yieldcomponent::merge(YieldComponent*, Json)
//
// Note: unlike every other merge in this file, this one does NOT call
// defaultMerge, and it always returns an empty Errors.
yield_component_merge :: proc(yc: ^Yield_Component, j: jx.Value) -> tl.Errors {
	jx.set_int_value(&yc.organId, j, "organId")
	jx.set_double_value(&yc.yieldPercentage, j, "yieldPercentage")
	jx.set_double_value(&yc.yieldDryMatter, j, "yieldDryMatter")

	res: tl.Errors
	return res
}

// C++: json11::Json yieldcomponent::to_json(const YieldComponent*)
yield_component_to_json :: proc(yc: ^Yield_Component, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("YieldComponent")},
		{"organId", jx.i(yc.organId)},
		{"yieldPercentage", jx.f(yc.yieldPercentage)},
		{"yieldDryMatter", jx.f(yc.yieldDryMatter)},
	)
}

// ---------------------------------------------------------------------------
// AutomaticHarvestParameters
// ---------------------------------------------------------------------------

// C++: enum AutomaticHarvestParameters::HarvestTime
Harvest_Time :: enum {
	maturity = 0, // crop is harvested when maturity is reached
	unknown  = 1, // default error value
}

// C++: struct monica::AutomaticHarvestParameters
Automatic_Harvest_Parameters :: struct {
	_harvestTime:      Harvest_Time,
	_latestHarvestDOY: int,
}

// C++ defaults: _harvestTime{unknown}, _latestHarvestDOY{-1}
make_automatic_harvest_parameters :: proc() -> Automatic_Harvest_Parameters {
	return Automatic_Harvest_Parameters{_harvestTime = .unknown, _latestHarvestDOY = -1}
}

// C++: Errors automaticharvestparameters::merge(...)
//
// harvestTime is only assigned when the key yields a value > -1, so an absent
// key leaves the default in place.
automatic_harvest_parameters_merge :: proc(
	ahp: ^Automatic_Harvest_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(ahp, j, automatic_harvest_parameters_merge)

	ht := -1
	jx.set_int_value(&ht, j, "harvestTime")
	if ht > -1 {
		ahp._harvestTime = Harvest_Time(ht)
	}
	jx.set_int_value(&ahp._latestHarvestDOY, j, "latestHarvestDOY")

	return res
}

// C++: json11::Json automaticharvestparameters::to_json(...)
//
// NOTE(c++-quirk): the emitted key is "latestHavestDOY" - missing the 'r' -
// while merge reads "latestHarvestDOY". A to_json/merge round trip therefore
// silently loses the value. Reproduced; do not "fix" the spelling.
// Note also that this to_json emits no "type" key, unlike its neighbours.
automatic_harvest_parameters_to_json :: proc(
	ahp: ^Automatic_Harvest_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.obj(
		a,
		{"harvestTime", jx.i(int(ahp._harvestTime))},
		{"latestHavestDOY", jx.i(ahp._latestHarvestDOY)},
	)
}

// ---------------------------------------------------------------------------
// NMinCropParameters
// ---------------------------------------------------------------------------

// C++: struct monica::NMinCropParameters
NMin_Crop_Parameters :: struct {
	samplingDepth: f64,
	nTarget:       f64,
	nTarget30:     f64,
}

// C++: Errors nmincropparameters::merge(...)
nmin_crop_parameters_merge :: proc(ncp: ^NMin_Crop_Parameters, j: jx.Value) -> tl.Errors {
	res := default_merge(ncp, j, nmin_crop_parameters_merge)

	jx.set_double_value(&ncp.samplingDepth, j, "samplingDepth")
	jx.set_double_value(&ncp.nTarget, j, "nTarget")
	jx.set_double_value(&ncp.nTarget30, j, "nTarget30")

	return res
}

// C++: json11::Json nmincropparameters::to_json(...)
nmin_crop_parameters_to_json :: proc(ncp: ^NMin_Crop_Parameters, a: Allocator) -> jx.Value {
	return jx.obj(
		a,
		{"type", jx.sl("NMinCropParameters")},
		{"samplingDepth", jx.f(ncp.samplingDepth)},
		{"nTarget", jx.f(ncp.nTarget)},
		{"nTarget30", jx.f(ncp.nTarget30)},
	)
}

// ---------------------------------------------------------------------------
// OrganicMatterParameters
// ---------------------------------------------------------------------------

// C++: struct monica::OrganicMatterParameters
Organic_Matter_Parameters :: struct {
	vo_AOM_DryMatterContent:     f64, // [kg DM kg FM-1]
	vo_AOM_NH4Content:           f64, // [kg N kg DM-1]
	vo_AOM_NO3Content:           f64, // [kg N kg DM-1]
	vo_AOM_CarbamidContent:      f64, // [kg N kg DM-1]
	vo_CorgContent:              f64, // [kg C kg DM-1]
	vo_AOM_SlowDecCoeffStandard: f64,
	vo_AOM_FastDecCoeffStandard: f64,
	vo_PartAOM_to_AOM_Slow:      f64,
	vo_PartAOM_to_AOM_Fast:      f64,
	vo_CN_Ratio_AOM_Slow:        f64,
	vo_CN_Ratio_AOM_Fast:        f64,
	vo_PartAOM_Slow_to_SMB_Slow: f64,
	vo_PartAOM_Slow_to_SMB_Fast: f64,
	vo_NConcentration:           f64,
}
// all C++ defaults are 0.0, so the Odin zero value matches

// C++: Errors organicmatterparameters::merge(OrganicMatterParameters*, Json)
organic_matter_parameters_merge :: proc(
	omp: ^Organic_Matter_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(omp, j, organic_matter_parameters_merge)

	jx.set_double_value(&omp.vo_AOM_DryMatterContent, j, "AOM_DryMatterContent")
	jx.set_double_value(&omp.vo_AOM_NH4Content, j, "AOM_NH4Content")
	jx.set_double_value(&omp.vo_AOM_NO3Content, j, "AOM_NO3Content")
	jx.set_double_value(&omp.vo_AOM_CarbamidContent, j, "AOM_CarbamidContent")
	jx.set_double_value(&omp.vo_AOM_SlowDecCoeffStandard, j, "AOM_SlowDecCoeffStandard")
	jx.set_double_value(&omp.vo_AOM_FastDecCoeffStandard, j, "AOM_FastDecCoeffStandard")
	jx.set_double_value(&omp.vo_PartAOM_to_AOM_Slow, j, "PartAOM_to_AOM_Slow")
	jx.set_double_value(&omp.vo_PartAOM_to_AOM_Fast, j, "PartAOM_to_AOM_Fast")
	jx.set_double_value(&omp.vo_CN_Ratio_AOM_Slow, j, "CN_Ratio_AOM_Slow")
	jx.set_double_value(&omp.vo_CN_Ratio_AOM_Fast, j, "CN_Ratio_AOM_Fast")
	jx.set_double_value(&omp.vo_PartAOM_Slow_to_SMB_Slow, j, "PartAOM_Slow_to_SMB_Slow")
	jx.set_double_value(&omp.vo_PartAOM_Slow_to_SMB_Fast, j, "PartAOM_Slow_to_SMB_Fast")
	jx.set_double_value(&omp.vo_NConcentration, j, "NConcentration")
	jx.set_double_value(&omp.vo_CorgContent, j, "CorgContent")

	return res
}

// C++: json11::Json organicmatterparameters::to_json(const OrganicMatterParameters*)
//
// NOTE(c++-quirk): the C++ initialiser list contains the key "AOM_NO3Content"
// TWICE. The second entry carries the description "Carbamide content in added
// organic matter" and was clearly meant to be "AOM_CarbamidContent", but it
// repeats both the key and the value (vo_AOM_NO3Content). Because
// json11::Json::object is a std::map and map's initializer-list constructor
// inserts rather than assigns, the FIRST entry wins - so the duplicate is
// silently dropped and vo_AOM_CarbamidContent is never emitted at all.
//
// Reproduced exactly: only one AOM_NO3Content entry, with the "Nitrate content"
// description, and no AOM_CarbamidContent key. Note this makes to_json lossy -
// a to_json/merge round trip zeroes vo_AOM_CarbamidContent.
organic_matter_parameters_to_json_object :: proc(
	omp: ^Organic_Matter_Parameters,
	a: Allocator,
) -> jx.Object {
	vud :: proc(v: f64, unit, desc: string, a: Allocator) -> jx.Value {
		return jx.arr(a, jx.f(v), jx.sl(unit), jx.sl(desc))
	}

	o := jx.obj(
		a,
		{"type", jx.sl("OrganicMatterParameters")},
		{
			"AOM_DryMatterContent",
			vud(
				omp.vo_AOM_DryMatterContent,
				"kg DM kg FM-1",
				"Dry matter content of added organic matter",
				a,
			),
		},
		{
			"AOM_NH4Content",
			vud(
				omp.vo_AOM_NH4Content,
				"kg N kg DM-1",
				"Ammonium content in added organic matter",
				a,
			),
		},
		// the surviving one of the two duplicate AOM_NO3Content entries
		{
			"AOM_NO3Content",
			vud(
				omp.vo_AOM_NO3Content,
				"kg N kg DM-1",
				"Nitrate content in added organic matter",
				a,
			),
		},
		{
			"AOM_SlowDecCoeffStandard",
			vud(
				omp.vo_AOM_SlowDecCoeffStandard,
				"d-1",
				"Decomposition rate coefficient of slow AOM at standard conditions",
				a,
			),
		},
		{
			"AOM_FastDecCoeffStandard",
			vud(
				omp.vo_AOM_FastDecCoeffStandard,
				"d-1",
				"Decomposition rate coefficient of fast AOM at standard conditions",
				a,
			),
		},
		{
			"PartAOM_to_AOM_Slow",
			vud(
				omp.vo_PartAOM_to_AOM_Slow,
				"kg kg-1",
				"Part of AOM that is assigned to the slowly decomposing pool",
				a,
			),
		},
		{
			"PartAOM_to_AOM_Fast",
			vud(
				omp.vo_PartAOM_to_AOM_Fast,
				"kg kg-1",
				"Part of AOM that is assigned to the rapidly decomposing pool",
				a,
			),
		},
		{
			"CN_Ratio_AOM_Slow",
			vud(
				omp.vo_CN_Ratio_AOM_Slow,
				"",
				"C to N ratio of the slowly decomposing AOM pool",
				a,
			),
		},
		{
			"CN_Ratio_AOM_Fast",
			vud(
				omp.vo_CN_Ratio_AOM_Fast,
				"",
				"C to N ratio of the rapidly decomposing AOM pool",
				a,
			),
		},
		{
			"PartAOM_Slow_to_SMB_Slow",
			vud(
				omp.vo_PartAOM_Slow_to_SMB_Slow,
				"kg kg-1",
				"Part of AOM slow consumed by slow soil microbial biomass",
				a,
			),
		},
		{
			"PartAOM_Slow_to_SMB_Fast",
			vud(
				omp.vo_PartAOM_Slow_to_SMB_Fast,
				"kg kg-1",
				"Part of AOM slow consumed by fast soil microbial biomass",
				a,
			),
		},
		{
			"NConcentration",
			vud(
				omp.vo_NConcentration,
				"kg N kg DM-1",
				"Nitrogen content in added organic matter",
				a,
			),
		},
		{
			"CorgContent",
			vud(omp.vo_CorgContent, "kg C kg DM-1", "Carbon content in added organic matter", a),
		},
	)
	return o.(jx.Object)
}

organic_matter_parameters_to_json :: proc(
	omp: ^Organic_Matter_Parameters,
	a: Allocator,
) -> jx.Value {
	return jx.Value(organic_matter_parameters_to_json_object(omp, a))
}

// ---------------------------------------------------------------------------
// OrganicFertilizerParameters
// ---------------------------------------------------------------------------

// C++: struct monica::OrganicFertilizerParameters : public OrganicMatterParameters
Organic_Fertilizer_Parameters :: struct {
	using base: Organic_Matter_Parameters,
	id:         string,
	name:       string,
}

// C++: Errors organicfertilizerparameters::merge(...)
organic_fertilizer_parameters_merge :: proc(
	ofp: ^Organic_Fertilizer_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(ofp, j, organic_fertilizer_parameters_merge)

	tl.append_errors(&res, organic_matter_parameters_merge(&ofp.base, j))

	jx.set_string_value(&ofp.id, j, "id")
	jx.set_string_value(&ofp.name, j, "name")

	return res
}

// C++: json11::Json organicfertilizerparameters::to_json(...)
//
// The C++ takes the base object_items() and overwrites "type", then adds id/name.
organic_fertilizer_parameters_to_json :: proc(
	ofp: ^Organic_Fertilizer_Parameters,
	a: Allocator,
) -> jx.Value {
	o := organic_matter_parameters_to_json_object(&ofp.base, a)
	jx.obj_set(&o, "type", jx.sl("OrganicFertilizerParameters"), a)
	jx.obj_set(&o, "id", jx.s(ofp.id, a), a)
	jx.obj_set(&o, "name", jx.s(ofp.name, a), a)
	return jx.Value(o)
}

// ---------------------------------------------------------------------------
// CropResidueParameters
// ---------------------------------------------------------------------------

// C++: struct monica::CropResidueParameters : public OrganicMatterParameters
Crop_Residue_Parameters :: struct {
	using base:  Organic_Matter_Parameters,
	species:     string,
	residueType: string,
}

// C++: Errors cropresidueparameters::merge(...)
crop_residue_parameters_merge :: proc(
	crp: ^Crop_Residue_Parameters,
	j: jx.Value,
) -> tl.Errors {
	res := default_merge(crp, j, crop_residue_parameters_merge)

	tl.append_errors(&res, organic_matter_parameters_merge(&crp.base, j))
	jx.set_string_value(&crp.species, j, "species")
	jx.set_string_value(&crp.residueType, j, "residueType")

	return res
}

// C++: json11::Json cropresidueparameters::to_json(...)
crop_residue_parameters_to_json :: proc(
	crp: ^Crop_Residue_Parameters,
	a: Allocator,
) -> jx.Value {
	o := organic_matter_parameters_to_json_object(&crp.base, a)
	jx.obj_set(&o, "type", jx.sl("CropResidueParameters"), a)
	jx.obj_set(&o, "species", jx.s(crp.species, a), a)
	jx.obj_set(&o, "residueType", jx.s(crp.residueType, a), a)
	return jx.Value(o)
}
