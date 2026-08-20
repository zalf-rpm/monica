// The alias table's safety net (plan-reflective-outputs.md §5).
//
// The reflection tier restores ~125 legacy output names for the cost of one
// table row each. The failure mode that buys is silent: a typo in a path, or
// a field the Odin port spells differently from the C++, compiles fine and
// then emits 0.0 for every day of every run. Nothing downstream notices.
//
// So: every row is asserted to compile against the real Monica_Model and to
// land on a leaf the CSV writer can print. That is a weaker claim than "this
// column matches the C++" - only the rows exercised by
// odin/tests/diff_outputs.sh get that - but it is the claim that turns a
// mistyped path from a silent zero into a failing test.
package tests

import "core:strings"
import "core:testing"
import mio "../monica/io"
import mcore "../monica/core"
import rp "../support/reflectpath"

@(test)
test_every_alias_compiles :: proc(t: ^testing.T) {
	for e in mio.g_alias_table {
		plan, err := rp.compile(
			typeid_of(mcore.Monica_Model),
			e.alias.path,
			context.temp_allocator,
		)
		if !testing.expectf(
			t,
			!rp.failed(err),
			"alias %q -> %q: %s (at %q)",
			e.name,
			e.alias.path,
			err.msg,
			err.at,
		) {
			continue
		}
		testing.expectf(
			t,
			rp.leaf_is_numeric(&plan),
			"alias %q -> %q: leaf is %v, which the CSV writer cannot print as a number",
			e.name,
			e.alias.path,
			plan.leaf,
		)
	}
}

// A layer-based alias must leave exactly one dimension open, because that
// dimension is what OId.fromLayer/toLayer/organ drives. An alias that acquired
// a stray index (or lost one) would silently stop responding to ["Mois",[1,3]].
@(test)
test_layer_aliases_have_one_open_dimension :: proc(t: ^testing.T) {
	// paths whose open dimension the fixture actually exercises
	layered := []string{"Mois", "Fc", "Pwp", "Sat", "Sand", "Clay", "pH", "STemp", "AOM_Fast"}
	for name in layered {
		e, found := find_alias(name)
		if !testing.expectf(t, found, "alias %q went missing from the table", name) {
			continue
		}
		plan, err := rp.compile(typeid_of(mcore.Monica_Model), e.path, context.temp_allocator)
		testing.expectf(t, !rp.failed(err), "%q: %s", name, err.msg)
		testing.expectf(t, plan.open_step >= 0, "%q -> %q has no open dimension", name, e.path)
	}

	// ... and a scalar alias must not have one, or get_complex_values would
	// wrap it in a one-element array and change the column's shape
	for name in ([]string{"Kc", "Irrig", "LAI", "Tavg", "AbBiom"}) {
		e, found := find_alias(name)
		if !testing.expectf(t, found, "alias %q went missing from the table", name) {
			continue
		}
		plan, err := rp.compile(typeid_of(mcore.Monica_Model), e.path, context.temp_allocator)
		testing.expectf(t, !rp.failed(err), "%q: %s", name, err.msg)
		testing.expectf(t, plan.open_step < 0, "%q -> %q is unexpectedly open", name, e.path)
	}
}

// The table is keyed by name at runtime, so a duplicate row is a silently
// dropped alias.
@(test)
test_alias_names_are_unique :: proc(t: ^testing.T) {
	seen := make(map[string]bool, len(mio.g_alias_table), context.temp_allocator)
	for e in mio.g_alias_table {
		testing.expectf(t, !seen[e.name], "duplicate alias row for %q", e.name)
		seen[e.name] = true
	}
}

// Tier 1 outranks tier 2, so an alias that shadows a *computed* id would
// replace a correct value with a plausible wrong one. The ids build_output.odin
// keeps as lambdas because they are computed must not appear in the table.
@(test)
test_no_alias_shadows_a_computed_id :: proc(t: ^testing.T) {
	computed := []string{"Date", "Year", "Crop", "Stage", "OrgBiom", "Yield", "SOC", "N", "ETa/ETc"}
	for name in computed {
		_, found := find_alias(name)
		testing.expectf(
			t,
			!found,
			"%q is computed (a proc call, arithmetic or an organ-count guard) but has an alias row, which now outranks its lambda",
			name,
		)
	}
}

@(private = "file")
find_alias :: proc(name: string) -> (mio.Alias, bool) {
	for e in mio.g_alias_table {
		if e.name == name {
			return e.alias, true
		}
	}
	return {}, false
}

_ :: strings
