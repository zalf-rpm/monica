// The module tables (monica/modules/<process>): a module is its binding package
// with a Module_Entry, registered by core/builtin_modules.odin (R3,
// plan-crop2ml-modules.md phase 6, plan-module-plugins.md).
// The rows carry the procedures MONICA calls, so a row with a forgotten
// procedure, a missing row or one copied with another module's type would
// only show at run time. These tests show it here.
package tests

import "base:runtime"
import "core:reflect"
import "core:testing"
import etm "../monica/modules/evapotranspiration"
import pl "../monica/modules/plugin"
import stm "../monica/modules/soil_temperature"

// Every row complete, names unique, and each row's `make` making a module of
// the row's `type`, distinct from the other rows'.
@(private = "file")
check_table :: proc(t: ^testing.T, table: string, rows: []$E, make_type: proc(e: E) -> typeid) {
	testing.expectf(t, len(rows) > 0, "%s: no built-in modules registered", table)
	seen_names := make(map[string]bool, context.temp_allocator)
	seen_types := make(map[typeid]string, context.temp_allocator)
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
		type := reflect.struct_field_value_by_name(row, "type").(typeid)
		made := make_type(row)
		testing.expectf(t, made == type, "%s: %q makes a %v, its row says %v", table, name, made, type)
		if other, dup := seen_types[type]; dup {
			testing.expectf(t, false, "%s: %q and %q have the same type %v", table, name, other, type)
		}
		seen_types[type] = name
	}
}

@(test)
test_soil_temperature_module_table :: proc(t: ^testing.T) {
	check_table(t, "soil_temperature", stm.modules[:], proc(e: stm.Module_Entry) -> typeid {
		m := e.make(context.allocator)
		defer e.destroy(m, context.allocator)
		return m.type
	})
}

@(test)
test_evapotranspiration_module_table :: proc(t: ^testing.T) {
	check_table(t, "evapotranspiration", etm.modules[:], proc(e: etm.Module_Entry) -> typeid {
		m := e.make(context.allocator)
		defer e.destroy(m, context.allocator)
		return m.type
	})
}

// The layout hash plugins are checked with (plan-module-plugins.md): stable,
// different for the two processes' interfaces, and changed by a renamed or
// moved field or another field type.
@(test)
test_plugin_layout_hash :: proc(t: ^testing.T) {
	testing.expect(t, pl.layout_hash(stm.Input) == pl.layout_hash(stm.Input))
	testing.expect(t, pl.layout_hash(stm.Input) != pl.layout_hash(etm.Input))
	testing.expect(t, pl.layout_hash(stm.Output) != pl.layout_hash(etm.Output))
	// anonymous structs: only the fields differ, not a type name
	a := pl.layout_hash(struct {
			x: f64,
			y: []f64,
		})
	testing.expect(t, a == pl.layout_hash(struct {
			x: f64,
			y: []f64,
		}), "the same fields")
	testing.expect(t, a != pl.layout_hash(struct {
			x: f64,
			z: []f64,
		}), "renamed field")
	testing.expect(t, a != pl.layout_hash(struct {
			y: []f64,
			x: f64,
		}), "moved field")
	testing.expect(t, a != pl.layout_hash(struct {
			x: f32,
			y: []f64,
		}), "other field type")
	testing.expect(t, a != pl.layout_hash(struct {
			x: f64,
			y: []f32,
		}), "other element type")
}
