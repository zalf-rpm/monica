// The crop components' declared inputs (plan-component-interfaces.md D,
// CONVENTIONS.md §9b): every number they read has a unit, and every `src` and
// `lag` tag uses the documented vocabulary.
package tests

import "core:reflect"
import "core:slice"
import "core:testing"
import mcore "../monica/core"

@(private = "file")
COMPONENT_INPUTS := []typeid {
	mcore.Radiation_In,
	mcore.Daylength_Factor_In,
	mcore.Vernalisation_In,
	mcore.Canopy_Kc_In,
	mcore.Canopy_Growth_In,
	mcore.Oxygen_Deficiency_In,
	mcore.Heat_Stress_In,
	mcore.Frost_Kill_In,
	mcore.Drought_Impact_In,
	mcore.N_Status_In,
	mcore.Root_System_In,
	mcore.O3_Hourly_In,
	mcore.Voc_In,
}

// Where a value comes from: a crop component (its state struct without the
// Crop_ prefix) or one of the model's other sources.
@(private = "file")
SOURCES := []string {
	// crop components
	"Radiation", "Phenology", "Canopy", "Photosynthesis", "O3", "Voc", "Stress", "N_Status", "Growth",
	"Root_System", "Et0", "Water_Uptake", "N_Uptake", "Carbon", "Management", "Intercropping",
	// the rest of the model
	"weather", "site", "soil", "soil moisture", "species", "cultivar", "crop parameters",
	"crop module parameters", "simulation parameters",
}

@(test)
test_component_inputs_are_tagged :: proc(t: ^testing.T) {
	for T in COMPONENT_INPUTS {
		for f in reflect.struct_fields_zipped(T) {
			ti := reflect.type_info_base(f.type)
			if s, is_slice := ti.variant.(reflect.Type_Info_Slice); is_slice {
				ti = reflect.type_info_base(s.elem)
			}
			#partial switch _ in ti.variant {
			case reflect.Type_Info_Float, reflect.Type_Info_Integer, reflect.Type_Info_Boolean:
				_, ok := reflect.struct_tag_lookup(f.tag, "unit")
				testing.expectf(t, ok, "%v.%s has no unit tag", T, f.name)
			}
			if src, ok := reflect.struct_tag_lookup(f.tag, "src"); ok {
				testing.expectf(t, slice.contains(SOURCES, src), "%v.%s: unknown src %q", T, f.name, src)
			}
			if lag, ok := reflect.struct_tag_lookup(f.tag, "lag"); ok {
				testing.expectf(t, lag == "1d", "%v.%s: lag %q, only \"1d\" is used", T, f.name, lag)
				_, has_src := reflect.struct_tag_lookup(f.tag, "src")
				testing.expectf(t, has_src, "%v.%s: a lag without src", T, f.name)
			}
		}
	}
}
