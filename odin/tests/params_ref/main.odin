// Odin side of the Phase 1c differential test for monica/params.
//
// Must emit byte-identical output to odin/tests/cpp_ref/params_ref_main.cpp.
// Run odin/tests/cpp_ref/run_params.sh to build both and diff them.
package params_ref

import "core:fmt"
import "core:os"
import "core:strings"
import p "../../monica/params"
import jx "../../support/jsonx"
import tl "../../support/tools"

main :: proc() {
	args := os.args
	if len(args) < 2 {
		fmt.eprintln("usage: params_ref <pathToGeneralDir>")
		os.exit(2)
	}
	dir := args[1]

	arena: jx.Arena
	if !jx.arena_init(&arena) {
		fmt.eprintln("arena init failed")
		os.exit(1)
	}
	defer jx.arena_destroy(&arena)
	a := jx.arena_allocator(&arena)

	load :: proc(dir, name: string, a: jx.Allocator) -> jx.Value {
		path := strings.concatenate({dir, "/", name}, a)
		r := jx.read_and_parse_json_file(path, a)
		if tl.failure(r.errs) {
			tl.print_possible_errors(r.errs)
			return jx.Value{}
		}
		return r.result
	}

	dump :: proc(label: string, v: jx.Value, a: jx.Allocator) {
		fmt.printf("%s\t%s\n", label, jx.dump(v, a))
	}

	// defaults first - these pin the C++ in-class initialisers
	{
		d: p.Soil_Moisture_Module_Parameters
		dump("default-SoilMoisture", p.soil_moisture_module_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_soil_temperature_module_parameters()
		dump("default-SoilTemperature", p.soil_temperature_module_parameters_to_json(&d, a), a)
	}
	{
		d: p.Soil_Transport_Module_Parameters
		dump("default-SoilTransport", p.soil_transport_module_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_stics_parameters()
		dump("default-Stics", p.stics_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_soil_organic_module_parameters()
		dump("default-SoilOrganic", p.soil_organic_module_parameters_to_json(&d, a), a)
	}

	// then merged from the real parameter files
	{
		v: p.Soil_Moisture_Module_Parameters
		e := p.soil_moisture_module_parameters_merge(&v, load(dir, "soil-moisture.json", a))
		_ = e
		dump("merged-SoilMoisture", p.soil_moisture_module_parameters_to_json(&v, a), a)
	}
	{
		v := p.make_soil_temperature_module_parameters()
		e := p.soil_temperature_module_parameters_merge(&v, load(dir, "soil-temperature.json", a))
		_ = e
		dump("merged-SoilTemperature", p.soil_temperature_module_parameters_to_json(&v, a), a)
	}
	{
		v: p.Soil_Transport_Module_Parameters
		e := p.soil_transport_module_parameters_merge(&v, load(dir, "soil-transport.json", a))
		_ = e
		dump("merged-SoilTransport", p.soil_transport_module_parameters_to_json(&v, a), a)
	}
	{
		v := p.make_soil_organic_module_parameters()
		e := p.soil_organic_module_parameters_merge(&v, load(dir, "soil-organic.json", a))
		_ = e
		dump("merged-SoilOrganic", p.soil_organic_module_parameters_to_json(&v, a), a)
		// soilorganic's to_json drops the nested stics params, so dump them directly
		dump("merged-SoilOrganic-stics", p.stics_parameters_to_json(&v.sticsParams, a), a)
	}
}
