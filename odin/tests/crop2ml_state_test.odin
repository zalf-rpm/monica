// The previous-day state of Crop2ML bindings (monica/modules/crop2ml):
// copy_state copies `s` into `s1` deeply, without daily allocations, and so
// makes `_t1` inputs (generated as reads of `s1`) see yesterday's values.
package tests

import "core:mem"
import "core:testing"
import c2m "../monica/modules/crop2ml"

@(private = "file")
Date_Like :: struct {
	year, month, day: int,
}

// A State as the Odin generator emits them, plus a nested value type
@(private = "file")
Test_State :: struct {
	x:      f64,
	n:      int,
	flag:   bool,
	name:   string,
	fixed:  [3]f64,
	date:   Date_Like,
	layers: [dynamic]f64,
	ids:    [dynamic]int,
}

@(private = "file")
destroy_test_state :: proc(x: ^Test_State) {
	delete(x.layers)
	delete(x.ids)
}

@(test)
crop2ml_copy_state_is_deep :: proc(t: ^testing.T) {
	track: mem.Tracking_Allocator
	mem.tracking_allocator_init(&track, context.allocator)
	defer mem.tracking_allocator_destroy(&track)
	alloc := mem.tracking_allocator(&track)

	s, s1: Test_State
	s.layers = make([dynamic]f64, alloc)
	s.ids = make([dynamic]int, alloc)
	s.x, s.n, s.flag, s.name = 1.5, 7, true, "wheat"
	s.fixed = {1, 2, 3}
	s.date = {2024, 5, 1}
	append(&s.layers, 10, 20, 30)
	append(&s.ids, 4, 5)

	c2m.copy_state(&s1, &s, alloc)
	testing.expect_value(t, s1.x, 1.5)
	testing.expect_value(t, s1.n, 7)
	testing.expect_value(t, s1.flag, true)
	testing.expect_value(t, s1.name, "wheat")
	testing.expect_value(t, s1.fixed, [3]f64{1, 2, 3})
	testing.expect_value(t, s1.date, Date_Like{2024, 5, 1})
	testing.expect_value(t, len(s1.layers), 3)
	testing.expect_value(t, s1.layers[2], 30.0)
	testing.expect_value(t, len(s1.ids), 2)
	testing.expect(t, raw_data(s1.layers) != raw_data(s.layers), "s1 must have its own list")

	// today's model writes s - yesterday's state stays
	s.layers[0] = 99
	s.x = 2.5
	testing.expect_value(t, s1.layers[0], 10.0)
	testing.expect_value(t, s1.x, 1.5)

	// the lists grow and shrink between days
	append(&s.layers, 40, 50)
	c2m.copy_state(&s1, &s, alloc)
	testing.expect_value(t, len(s1.layers), 5)
	testing.expect_value(t, s1.layers[0], 99.0)
	testing.expect_value(t, s1.layers[4], 50.0)
	resize(&s.layers, 1)
	c2m.copy_state(&s1, &s, alloc)
	testing.expect_value(t, len(s1.layers), 1)

	destroy_test_state(&s)
	destroy_test_state(&s1)
	testing.expect_value(t, len(track.allocation_map), 0)
	testing.expect_value(t, len(track.bad_free_array), 0)
}

// MONICA's first module step gets the model's allocator, the later ones a
// daily scratch arena: s1's lists must stay in the first one, also when they
// grow on a later day.
@(test)
crop2ml_copy_state_keeps_first_allocator :: proc(t: ^testing.T) {
	track: mem.Tracking_Allocator
	mem.tracking_allocator_init(&track, context.allocator)
	defer mem.tracking_allocator_destroy(&track)
	alloc := mem.tracking_allocator(&track)

	scratch: mem.Dynamic_Arena
	mem.dynamic_arena_init(&scratch)
	defer mem.dynamic_arena_destroy(&scratch)

	s, s1: Test_State
	s.layers = make([dynamic]f64, alloc)
	append(&s.layers, 1, 2)
	c2m.copy_state(&s1, &s, alloc) // day 1
	for _ in 0 ..< 3 {
		append(&s.layers, 3, 4, 5, 6) // longer than s1's capacity
		c2m.copy_state(&s1, &s, mem.dynamic_arena_allocator(&scratch))
		mem.dynamic_arena_reset(&scratch) // would leave s1 dangling if it had used the arena
	}
	testing.expect_value(t, len(s1.layers), 14)
	testing.expect_value(t, s1.layers[13], 6.0)
	testing.expect(t, s1.layers.allocator.data == alloc.data, "s1's list must keep the first allocator")

	destroy_test_state(&s)
	destroy_test_state(&s1)
	testing.expect_value(t, len(track.allocation_map), 0)
}

// What the Odin generator makes of SiriusQuality's LeafNumber (an input
// `leafNumber_t1` becomes `s1.leaf_number`), and a host loop as the bindings
// do it: with the copy the leaf number accumulates, without it it wouldn't.
@(private = "file")
Leaf_State :: struct {
	phyllochron: f64,
	leaf_number: f64,
}

@(private = "file")
leaf_number_calculate_model :: proc(s, s1: ^Leaf_State, delta_tt: f64) {
	s.leaf_number = s1.leaf_number
	s.leaf_number = s1.leaf_number + min(delta_tt / s1.phyllochron, 0.999)
}

@(test)
crop2ml_t1_inputs_see_yesterday :: proc(t: ^testing.T) {
	s, s1: Leaf_State
	s.phyllochron = 100
	for day in 1 ..= 4 {
		c2m.copy_state(&s1, &s)
		leaf_number_calculate_model(&s, &s1, 25)
		testing.expect_value(t, s.leaf_number, 0.25 * f64(day))
	}
}
