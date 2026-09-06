// Phase 7 checkpoint 5: src/run/monica-run-main.cpp - the monica-run CLI.
//
// Dropped entirely, all "port on demand" / out of this port's scope (see
// plan-odin.md's "Explicitly dropped" table and run_monica.odin's own header
// comment): Cap'n Proto/ZeroMQ RPC (kj::setupAsyncIo, ConnectionManager,
// sturdy-ref soil-profile/climate-timeseries/intercropping connections -
// -icrsr/-icwsr and the whole "check if there were sturdy refs to
// time-series" block), Intercropping (isIC/isAsyncIC, the entire `output2`
// half, -o2/--path-to-output-file2), and the getCapillaryRiseRate closure
// wiring (already baked directly into soil_moisture.odin's
// read_capillary_rise_rates, no per-run wiring needed - same "closure
// becomes a package-level lookup" pattern already used for
// calculateAndSetPwpFcSatFunctions in central_parameter_provider_merge).
//
// activateDebug (the C++ global controlling debug()<<... output) is not
// ported: nothing in this port's call graph reads it - see monica_model.odin
// and run_monica.odin, neither of which carry a debug-trace facility.
package main

import "core:bufio"
import "core:fmt"
import "core:io"
import "core:os"
import "core:strings"
import mio "../../monica/io"
import run "../../monica/run"
import jx "../../support/jsonx"
import tl "../../support/tools"

APP_NAME :: "monica-run"
VERSION :: "3.6.59.0"

print_help :: proc() {
	fmt.printfln("%s [options] path-to-sim-json", APP_NAME)
	fmt.println()
	fmt.println("options:")
	fmt.println()
	fmt.println(" -h   | --help ... this help output")
	fmt.printfln(" -v   | --version ... outputs %s version", APP_NAME)
	fmt.println()
	fmt.println(" -d   | --debug ... show debug outputs")
	fmt.println(
		" -sd  | --start-date ISO-DATE (default: start of given climate data) ... date in iso-date-format yyyy-mm-dd",
	)
	fmt.println(
		" -ed  | --end-date ISO-DATE (default: end of given climate data) ... date in iso-date-format yyyy-mm-dd",
	)
	fmt.println(" -m   | --write-multiple-output-files ... write one output file per output section ")
	fmt.println(" -op  | --path-to-output DIRECTORY (default: .) ... path to output directory")
	fmt.println(" -o   | --path-to-output-file FILE ... path to output file")
	fmt.println(" -c   | --path-to-crop FILE (default: ./crop.json) ... path to crop.json file")
	fmt.println(" -s   | --path-to-site FILE (default: ./site.json) ... path to site.json file")
	fmt.println(" -w   | --path-to-climate FILE (default: ./climate.csv) ... path to climate.csv")
}

@(private)
sanitize_section_filename :: proc(origSpec: string, allocator := context.allocator) -> string {
	s, _ := strings.replace_all(origSpec, "\"", "", allocator)
	s, _ = strings.replace_all(s, "*", "_star_", allocator)
	s, _ = strings.replace_all(s, "?", "_qm_", allocator)
	s, _ = strings.replace_all(s, "|", "_bar_", allocator)
	s, _ = strings.replace_all(s, "<", "_lb_", allocator)
	s, _ = strings.replace_all(s, ">", "_rb_", allocator)
	s, _ = strings.replace_all(s, ":", "_colon_", allocator)
	return s
}

main :: proc() {
	args := os.args

	debug := false
	debugSet := false
	startDate := ""
	endDate := ""
	pathToOutput := ""
	pathToOutputFile := ""
	pathToOutputDir := "."
	writeMultipleOutputFiles := false
	pathToSimJson := "./sim.json"
	crop := ""
	site := ""
	climate := ""

	if len(args) <= 1 {
		print_help()
		return
	}

	i := 1
	for i < len(args) {
		arg := args[i]
		switch {
		case arg == "-d" || arg == "--debug":
			debug = true
			debugSet = true
		case (arg == "-sd" || arg == "--start-date") && i + 1 < len(args):
			i += 1
			startDate = args[i]
		case (arg == "-ed" || arg == "--end-date") && i + 1 < len(args):
			i += 1
			endDate = args[i]
		case (arg == "-op" || arg == "--path-to-output") && i + 1 < len(args):
			i += 1
			pathToOutput = args[i]
		case (arg == "-o" || arg == "--path-to-output-file") && i + 1 < len(args):
			i += 1
			pathToOutputFile = args[i]
		case (arg == "-m" || arg == "--write-multiple-output-files"):
			writeMultipleOutputFiles = true
		case (arg == "-c" || arg == "--path-to-crop") && i + 1 < len(args):
			i += 1
			crop = args[i]
		case (arg == "-s" || arg == "--path-to-site") && i + 1 < len(args):
			i += 1
			site = args[i]
		case (arg == "-w" || arg == "--path-to-climate") && i + 1 < len(args):
			i += 1
			climate = args[i]
		case arg == "-h" || arg == "--help":
			print_help()
			os.exit(0)
		case arg == "-v" || arg == "--version":
			fmt.printfln("%s version %s", APP_NAME, VERSION)
			os.exit(0)
		case:
			pathToSimJson = arg
		}
		i += 1
	}

	a := context.allocator

	path_of_sim_json, _ := tl.split_path_to_file(pathToSimJson, a)

	simr := jx.read_and_parse_json_file(pathToSimJson, a)
	if tl.failure(simr.errs) {
		tl.print_possible_errors(simr.errs)
	}

	simm := make(jx.Object, 0, a)
	for k, v in jx.object_items(simr.result) {
		simm[strings.clone(k, a)] = v
	}

	// climate.csv-options: overlay CLI start-date/end-date onto whatever the
	// sim.json already had
	csvos := make(jx.Object, 0, a)
	for k, v in jx.object_items(jx.get(jx.Value(simm), "climate.csv-options")) {
		csvos[strings.clone(k, a)] = v
	}
	if startDate != "" {
		csvos[strings.clone("start-date", a)] = jx.s(startDate, a)
	}
	if endDate != "" {
		csvos[strings.clone("end-date", a)] = jx.s(endDate, a)
	}
	simm[strings.clone("climate.csv-options", a)] = jx.Value(csvos)

	if debugSet {
		simm[strings.clone("debug?", a)] = jx.b(debug)
	}

	if pathToOutput != "" {
		simm[strings.clone("path-to-output", a)] = jx.s(pathToOutput, a)
	}

	simm[strings.clone("sim.json", a)] = jx.s(pathToSimJson, a)

	if crop != "" {
		simm[strings.clone("crop.json", a)] = jx.s(crop, a)
	}
	path_to_crop_json := jx.string_value(jx.Value(simm), "crop.json")
	if !tl.is_absolute_path(path_to_crop_json) {
		simm[strings.clone("crop.json", a)] = jx.s(strings.concatenate({path_of_sim_json, path_to_crop_json}, a), a)
	}

	if site != "" {
		simm[strings.clone("site.json", a)] = jx.s(site, a)
	}
	path_to_site_json := jx.string_value(jx.Value(simm), "site.json")
	if !tl.is_absolute_path(path_to_site_json) {
		simm[strings.clone("site.json", a)] = jx.s(strings.concatenate({path_of_sim_json, path_to_site_json}, a), a)
	}

	if climate != "" {
		simm[strings.clone("climate.csv", a)] = jx.s(climate, a)
	}
	climate_csv_v := jx.get(jx.Value(simm), "climate.csv")
	if jx.is_string(climate_csv_v) {
		path_to_climate_csv := jx.string_value_of(climate_csv_v)
		if !tl.is_absolute_path(path_to_climate_csv) {
			simm[strings.clone("climate.csv", a)] = jx.s(
				strings.concatenate({path_of_sim_json, path_to_climate_csv}, a),
				a,
			)
		}
	} else if jx.is_array(climate_csv_v) {
		ps := make(jx.Array, 0, a)
		for jv in jx.array_items(climate_csv_v) {
			path_to_climate_csv := jx.string_value_of(jv)
			if tl.is_absolute_path(path_to_climate_csv) {
				append(&ps, jx.s(path_to_climate_csv, a))
			} else {
				append(&ps, jx.s(strings.concatenate({path_of_sim_json, path_to_climate_csv}, a), a))
			}
		}
		simm[strings.clone("climate.csv", a)] = jx.Value(ps)
	}

	sim_v := jx.Value(simm)

	cropr := jx.read_and_parse_json_file(jx.string_value(sim_v, "crop.json"), a)
	tl.print_possible_errors(cropr.errs)
	siter := jx.read_and_parse_json_file(jx.string_value(sim_v, "site.json"), a)
	tl.print_possible_errors(siter.errs)

	env_json := run.create_env_json_from_json_objects(cropr.result, siter.result, sim_v, a)

	path_to_soil_dir := tl.fix_system_separator(tl.replace_env_vars("${MONICA_PARAMETERS}/soil/", a), a)

	env: run.Env
	mergeErrs := run.env_merge(&env, env_json, path_to_soil_dir, a)
	tl.print_possible_errors(mergeErrs)
	if tl.failure(mergeErrs) {
		os.exit(1)
	}

	out := run.run_monica(&env, a)

	// NOTE(deviation): the C++ main ignores runMonica's Output.errors, because
	// there runMonica never sets any. run_monica.odin's empty-soil-profile guard
	// does (see its comment), and silently writing an empty CSV instead of saying
	// why would be worse than the crash it replaces.
	if len(out.errors) > 0 {
		for e in out.errors {
			fmt.eprintfln("%s", e)
		}
		os.exit(1)
	}

	if pathToOutputFile == "" && jx.bool_value_of(jx.get_path(sim_v, "output", "write-file?")) {
		pathToOutputDir = tl.fix_system_separator(jx.string_value(jx.get(sim_v, "output"), "path-to-output"), a)
		pathToOutputFile = tl.fix_system_separator(
			strings.concatenate({pathToOutputDir, "/", jx.string_value(jx.get(sim_v, "output"), "file-name")}, a),
			a,
		)
	}

	csvSep := jx.string_value(jx.get_path(sim_v, "output", "csv-options"), "csv-separator")
	includeHeaderRow := jx.bool_value_of(jx.get_path(sim_v, "output", "csv-options", "include-header-row"))
	includeUnitsRow := jx.bool_value_of(jx.get_path(sim_v, "output", "csv-options", "include-units-row"))
	includeAggRows := jx.bool_value_of(jx.get_path(sim_v, "output", "csv-options", "include-aggregation-rows"))

	if writeMultipleOutputFiles {
		filename := jx.string_value(jx.get(sim_v, "output"), "file-name")
		filenameWithoutExt := filename
		if pathToOutputFile != "" {
			path, fn := tl.split_path_to_file(pathToOutputFile, a)
			if len(path) > 0 {
				trimmed := strings.trim_suffix(path, "/")
				trimmed = strings.trim_suffix(trimmed, "\\")
				pathToOutputDir = strings.clone(trimmed, a)
			}
			filename = fn
		}
		if dot := strings.last_index(filename, "."); dot != -1 {
			filenameWithoutExt = filename[:dot]
		}

		writeOutputFile := true
		if pathToOutputDir != "" && !tl.ensure_dir_exists(pathToOutputDir, a) {
			fmt.eprintfln("Error failed to create path: '%s'.", pathToOutputDir)
			writeOutputFile = false
		}

		for section in out.data {
			sanitized := sanitize_section_filename(section.origSpec, a)

			usingFile := false
			fout: ^os.File
			if writeOutputFile {
				sectionPath := tl.fix_system_separator(
					strings.concatenate({pathToOutputDir, "/", filenameWithoutExt, "_section_", sanitized, ".csv"}, a),
					a,
				)
				ferr: os.Error
				fout, ferr = os.open(sectionPath, os.O_WRONLY | os.O_CREATE | os.O_TRUNC)
				if ferr != nil {
					fmt.eprintfln("Error while opening output file \"%s\"", sectionPath)
				} else {
					usingFile = true
				}
			}

			// buffered: sim-min-out_section_daily.csv alone is thousands of rows -
			// unbuffered os.File writes were one write() syscall per cell.
			bw: bufio.Writer
			bufio.writer_init(&bw, usingFile ? os.to_stream(fout) : os.to_stream(os.stdout), 1 << 16, a)
			w := bufio.writer_to_stream(&bw)

			if !usingFile {
				io.write_string(w, "\"")
				io.write_string(w, sanitized)
				io.write_string(w, "\"\r\n")
			}

			mio.write_output_header_rows(w, section.outputIds[:], csvSep, includeHeaderRow, includeUnitsRow, includeAggRows)
			mio.write_output(w, section.outputIds[:], section.results[:], csvSep)

			bufio.writer_flush(&bw)
			bufio.writer_destroy(&bw)
			if usingFile {
				os.close(fout)
			}
		}
	} else {
		writeOutputFile := pathToOutputFile != ""
		fout: ^os.File
		if writeOutputFile {
			path, _ := tl.split_path_to_file(pathToOutputFile, a)
			if !tl.ensure_dir_exists(path, a) {
				fmt.eprintfln("Error failed to create path: '%s'.", path)
			}
			ferr: os.Error
			fout, ferr = os.open(pathToOutputFile, os.O_WRONLY | os.O_CREATE | os.O_TRUNC)
			if ferr != nil {
				fmt.eprintfln("Error while opening output file \"%s\"", pathToOutputFile)
				writeOutputFile = false
			}
		}

		// buffered: sim-min-out.csv alone is thousands of rows - unbuffered
		// os.File writes were one write() syscall per cell.
		bw: bufio.Writer
		bufio.writer_init(&bw, writeOutputFile ? os.to_stream(fout) : os.to_stream(os.stdout), 1 << 16, a)
		w := bufio.writer_to_stream(&bw)

		for section in out.data {
			io.write_string(w, "\"")
			io.write_string(w, sanitize_section_filename(section.origSpec, a))
			io.write_string(w, "\"\r\n")
			mio.write_output_header_rows(w, section.outputIds[:], csvSep, includeHeaderRow, includeUnitsRow, includeAggRows)
			mio.write_output(w, section.outputIds[:], section.results[:], csvSep)
			io.write_string(w, "\r\n")
		}

		bufio.writer_flush(&bw)
		bufio.writer_destroy(&bw)
		if writeOutputFile {
			os.close(fout)
		}
	}
}
