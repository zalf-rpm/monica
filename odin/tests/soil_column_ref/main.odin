// Odin side of the phase 3 checkpoint 3b differential test: SoilLayer/
// SoilColumn construction.
//
// Must emit byte-identical output to
// odin/tests/cpp_ref/soil_column_ref_main.cpp. Run
// odin/tests/cpp_ref/run_soil_column.sh to build both and diff them.
package soil_column_ref

import "core:fmt"
import "core:os"
import "core:strconv"
import "core:strings"
import core "../../monica/core"
import soil "../../monica/soil"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: soil_column_ref <pathToSiteJson>")
		os.exit(2)
	}
	path_to_site_json := args[1]

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	site_r := jx.read_and_parse_json_file(path_to_site_json, a)
	if tl.failure(site_r.errs) {
		tl.print_possible_errors(site_r.errs)
		os.exit(1)
	}

	site_params := jx.get(site_r.result, "SiteParameters")
	soil_profile_params := jx.array_items(jx.get(site_params, "SoilProfileParameters"))

	path_to_soil_dir := tl.fix_system_separator(
		tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a),
		a,
	)

	layer_thickness := jx.double_value_key_d(site_params, "LayerThickness", 0.1)
	number_of_layers := jx.int_value_key_d(site_params, "NumberOfLayers", 20)

	pms_res := soil.create_equal_sized_soil_pms(
		.WESSOLEK2009,
		path_to_soil_dir,
		soil_profile_params,
		layer_thickness,
		number_of_layers,
		a,
	)
	if tl.failure(pms_res.errs) {
		tl.print_possible_errors(pms_res.errs)
	}

	max_mineralisation_depth := 0.4
	sc := core.make_soil_column(layer_thickness, max_mineralisation_depth, pms_res.result[:], a)

	g :: proc(v: f64, a: jx.Allocator) -> string {
		return jx.dump(jx.f(v), a)
	}
	gi :: proc(v: int, a: jx.Allocator) -> string {
		buf: [32]byte
		return strings.clone(strconv.write_int(buf[:], i64(v), 10), a)
	}

	// Builds and prints one tab-separated row - avoids hand-counting printf
	// format specifiers against a long, error-prone argument list.
	row :: proc(fields: ..string) {
		fmt.println(strings.join(fields, "\t"))
	}

	row("SC", gi(core.number_of_layers(&sc), a), gi(sc.vs_NumberOfOrganicLayers, a), g(core.soil_column_layer_thickness(&sc), a))

	for i := 0; i < len(sc.layers); i += 1 {
		sl := &sc.layers[i]
		row(
			"L",
			gi(i, a),
			g(sl.layer_thickness, a),
			g(sl.soil_water_flux, a),
			g(sl.som_slow, a),
			g(sl.som_fast, a),
			g(sl.smb_slow, a),
			g(sl.smb_fast, a),
			g(sl.soil_carbamid, a),
			g(sl.soil_nh4, a),
			g(sl.soil_no2, a),
			g(sl.soil_no3, a),
			gi(sl.soil_frozen ? 1 : 0, a),
			g(sl.soil_sand_content, a),
			g(sl.soil_clay_content, a),
			g(sl.soil_ph, a),
			g(sl.soil_stone_content, a),
			g(sl.lambda, a),
			g(sl.field_capacity, a),
			g(sl.saturation, a),
			g(sl.permanent_wilting_point, a),
			sl.soil_texture,
			g(sl.soil_ammonium, a),
			g(sl.soil_nitrate, a),
			g(sl.soil_cn_ratio, a),
			g(sl.soil_moisture_percent_fc, a),
			g(sl.soil_raw_density, a),
			g(sl.soil_bulk_density, a),
			g(sl.soil_organic_carbon, a),
			g(sl.soil_organic_matter, a),
			g(sl.soil_moisture_m3, a),
			g(sl.soil_temperature, a),
			g(core.soil_moisture_pf(sl), a),
			g(core.soil_nmin(sl), a),
			g(core.soil_silt_content(sl), a),
			g(core.soil_raw_density(sl), a),
			g(core.soil_bulk_density(sl), a),
		)
		row("L2", gi(i, a), g(core.soil_organic_carbon(sl), a), g(core.soil_organic_matter(sl), a))
	}

	depths := []f64{0.05, 0.35, 1.0, 1.99, 2.5}
	for depth in depths {
		row("DEPTH", g(depth, a), gi(core.get_layer_number_for_depth(&sc, depth), a))
	}
	row("SUMTEMP", g(core.sum_soil_temperature(&sc, len(sc.layers)), a))
	row("CALCORG", gi(core.calculate_number_of_organic_layers(&sc), a))
}
