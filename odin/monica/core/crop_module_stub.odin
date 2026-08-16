// Phase-5 stub for src/core/crop-module.h's CropModule struct (~4,750 lines,
// ported in full in phase 5 - see plan-odin.md). Modules 2b (soilmoisture) and
// soiltransport read exactly these fields through a *CropModule pointer (for
// soilmoisture, both via its own `cropModule` field and, in the C++, via
// `monica.currentCropModule.get()` - see soil_moisture.odin's package comment
// for why the Odin port unifies both onto this one field). Phase 5 replaces
// this struct wholesale with the real one; every field here keeps its exact
// C++ name so that replacement is a drop-in, not a rename.
package core

// C++: struct monica::CropModule (11-field subset)
Crop_Module :: struct {
	vc_SoilCoverage:               f64,
	vc_KcFactor:                   f64,
	vc_KcbFactor:                  f64,
	vc_CropHeight:                 f64,
	vc_DevelopmentalStage:         int, // C++ size_t
	vc_NetPrecipitation:           f64,
	vc_RootingDepth:               int, // C++ size_t
	vc_ReferenceEvapotranspiration: f64,
	vc_RemainingEvapotranspiration: f64,
	vc_EvaporatedFromIntercept:    f64,
	vc_Transpiration:              [dynamic]f64,
	vc_NUptakeFromLayer:           [dynamic]f64, // soiltransport.cpp
}
