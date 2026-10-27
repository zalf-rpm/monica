// The module tables (monica/modules/<process>): a module is its file, a line in
// the Module union and a row in `modules` (R3, plan-crop2ml-modules.md phase 6).
// The rows carry the procedures MONICA calls, so a row with a forgotten
// procedure, a missing row or one copied with another module's type would
// only show at run time. These tests show it here.
package tests

import "base:runtime"
import "core:reflect"
import "core:testing"
import etm "../monica/modules/evapotranspiration"
import stm "../monica/modules/soil_temperature"

// Every row complete, names unique, one row per union variant and each row
// making its own variant.
@(private = "file")
check_table :: proc(t: ^testing.T, table: string, module_type: typeid, rows: []$E, make_variant: proc(e: E) -> typeid) {
	variants := runtime.type_info_base(type_info_of(module_type)).variant.(runtime.Type_Info_Union).variants
	testing.expectf(t, len(rows) == len(variants), "%s: %d rows for %d union variants", table, len(rows), len(variants))
	seen_names := make(map[string]bool, context.temp_allocator)
	seen_variants := make(map[typeid]string, context.temp_allocator)
	for &row in rows {
		name := reflect.struct_field_value_by_name(row, "name").(string)
		for f in reflect.struct_fields_zipped(E) {
			if reflect.is_procedure(f.type) {
				p := (^rawptr)(uintptr(&row) + f.offset)^
				testing.expectf(t, p != nil, "%s: %q has no %s", table, name, f.name)
			}
		}
		testing.expectf(t, !seen_names[name], "%s: %q twice", table, name)
		seen_names[name] = true
		v := make_variant(row)
		if other, dup := seen_variants[v]; dup {
			testing.expectf(t, false, "%s: %q and %q make the same variant %v", table, name, other, v)
		}
		seen_variants[v] = name
	}
}

@(test)
test_soil_temperature_module_table :: proc(t: ^testing.T) {
	check_table(t, "soil_temperature", stm.Module, stm.modules[:], proc(e: stm.Module_Entry) -> typeid {
		m := e.make(context.allocator)
		defer e.destroy(m, context.allocator)
		return reflect.union_variant_typeid(m)
	})
}

@(test)
test_evapotranspiration_module_table :: proc(t: ^testing.T) {
	check_table(t, "evapotranspiration", etm.Module, etm.modules[:], proc(e: etm.Module_Entry) -> typeid {
		m := e.make(context.allocator)
		defer e.destroy(m, context.allocator)
		return reflect.union_variant_typeid(m)
	})
}
