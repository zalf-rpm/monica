// Odin side of the Phase 1c differential test for monica/params.
//
// Must emit byte-identical output to odin/tests/cpp_ref/params_ref_main.cpp.
// Run odin/tests/cpp_ref/run_params.sh to build both and diff them.
package params_ref

import "core:fmt"
import "core:os"
import "core:strings"
import d "../../support/date"
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

	// ---- tranche 2 -----------------------------------------------------------
	// defaults, pinning the C++ in-class initialisers
	{
		d: p.Mineral_Fertilizer_Parameters
		dump("default-MineralFertilizer", p.mineral_fertilizer_parameters_to_json(&d, a), a)
	}
	{
		d: p.NMin_Application_Parameters
		dump("default-NMinApplication", p.nmin_application_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_irrigation_parameters()
		dump("default-Irrigation", p.irrigation_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_automatic_irrigation_parameters()
		dump("default-AutomaticIrrigation", p.automatic_irrigation_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_site_parameters()
		dump("default-Site", p.site_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_simulation_parameters()
		dump("default-Simulation", p.simulation_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_crop_module_parameters(a)
		dump("default-CropModule", p.crop_module_parameters_to_json(&d, a), a)
	}
	{
		d := p.make_environment_parameters()
		dump("default-Environment", p.environment_parameters_to_json(&d, a), a)
	}

	// merged from the real crop/site/sim documents
	{
		v := p.make_crop_module_parameters(a)
		_ = p.crop_module_parameters_merge(&v, load(dir, "crop.json", a))
		dump("merged-CropModule", p.crop_module_parameters_to_json(&v, a), a)
	}
	{
		v := p.make_environment_parameters()
		_ = p.environment_parameters_merge(&v, load(dir, "environment.json", a), a)
		dump("merged-Environment", p.environment_parameters_to_json(&v, a), a)
	}

	// A few synthetic merges for the structs no general/*.json feeds, so the
	// merge paths (not just the defaults) are exercised.
	parse :: proc(s: string, a: jx.Allocator) -> jx.Value {
		r := jx.parse_json_string(s, a)
		tl.print_possible_errors(r.errs)
		return r.result
	}
	{
		j := parse(
			`{"id": "AN", "name": "ammonium nitrate", "Carbamid": 0.0, "NH4": 0.5, "NO3": 0.5}`,
			a,
		)
		v: p.Mineral_Fertilizer_Parameters
		_ = p.mineral_fertilizer_parameters_merge(&v, j)
		dump("merged-MineralFertilizer", p.mineral_fertilizer_parameters_to_json(&v, a), a)
	}
	{
		j := parse(`{"min": 40, "max": 120, "delayInDays": 10}`, a)
		v: p.NMin_Application_Parameters
		_ = p.nmin_application_parameters_merge(&v, j)
		dump("merged-NMinApplication", p.nmin_application_parameters_to_json(&v, a), a)
	}
	{
		// exercises the endDate-from-"stopDate" quirk, the threshold double-write,
		// and the unit transforms
		j := parse(
			`{"irrigationParameters": {"nitrateConcentration": [5, "mg dm-3"], "fw": 0.7},
			  "startDate": "1992-05-01", "stopDate": "1992-09-01",
			  "amount": [17, "mm"], "threshold": [50, "%"],
			  "trigger_if_nFC_below_%": [90, "%"],
			  "calc_nFC_until_depth_m": [30, "cm"],
			  "minDaysBetweenIrrigationEvents": 3}`,
			a,
		)
		v := p.make_automatic_irrigation_parameters()
		_ = p.automatic_irrigation_parameters_merge(&v, j)
		dump("merged-AutomaticIrrigation", p.automatic_irrigation_parameters_to_json(&v, a), a)
		// endDate is not emitted by to_json, so check it separately
		fmt.printf(
			"merged-AutomaticIrrigation-endDate\t%s\n",
			d.to_iso_date_string(v.endDate, "", a),
		)
	}
	{
		// every accepted rcp spelling
		rcp_strs := []string{"85", "8.5", "rcp85", "rcp8.5", "19", "nonsense"}
		for r in rcp_strs {
			j := parse(strings.concatenate({`{"rcp": "`, r, `"}`}, a), a)
			v := p.make_environment_parameters()
			_ = p.environment_parameters_merge(&v, j, a)
			ej := p.environment_parameters_to_json(&v, a)
			fmt.printf("rcp-str-%s\t%s\n", r, jx.dump(jx.get(ej, "rcp"), a))
		}
		rcp_nums := []f64{8.5, 85.0, 1.9, 19.0, 3.4}
		rcp_labels := []string{"8.5", "85", "1.9", "19", "3.4"}
		for r, i in rcp_nums {
			o := make(jx.Object, 0, a)
			jx.obj_set(&o, "rcp", jx.f(r), a)
			v := p.make_environment_parameters()
			_ = p.environment_parameters_merge(&v, jx.Value(o), a)
			ej := p.environment_parameters_to_json(&v, a)
			fmt.printf("rcp-num-%s\t%s\n", rcp_labels[i], jx.dump(jx.get(ej, "rcp"), a))
		}
	}
	{
		j := parse(
			`{"groundwaterInformationAvailable": true,
			  "groundwaterInfo": {"1991-01-01": 1.5, "1991-06-15": 2.25}}`,
			a,
		)
		v: p.Measured_Groundwater_Table_Information
		_ = p.measured_groundwater_table_information_merge(&v, j, a)
		dump("merged-Groundwater", p.measured_groundwater_table_information_to_json(&v, a), a)
	}
}
