/* Differential-test driver for phase 7 checkpoint 4: run/run-monica.cpp's
 * runMonica - the full crop-rotation-cycling day loop (run/run-monica.h's
 * runMonicaIC, isIC=false path) wired to real output storage
 * (setupStorage/store_data_store_results_if_spec_applies/
 * store_data_aggregate_results) and dumped through the already
 * oracle-verified writeOutputHeaderRows/writeOutput (checkpoint 3), using
 * the real installer/Hohenfinow2/sim-min.json + crop-min.json + site-min.json
 * end to end - same config monica-run's CLI (checkpoint 5, not yet ported)
 * will use. This is effectively phase 8's regression check run in-process
 * instead of through file I/O, done early to de-risk checkpoint 5.
 *
 * Usage: monica_run_ref <pathToSimJson>
 * See odin/tests/cpp_ref/run_monica_run.sh.
 */
#include <cstdio>
#include <iostream>
#include <map>
#include <string>

#include "core/monica-model.h"
#include "core/monica-parameters.h"
#include "io/csv-format.h"
#include "io/output.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "run/run-monica.h"
#include "soil/soil.h"
#include "tools/helper.h"

using namespace std;
using namespace Tools;
using namespace Climate;
using namespace json11;
using namespace monica;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: monica_run_ref <pathToSimJson>\n");
    return 2;
  }
  std::string pathToSimJson = argv[1];

  std::string pathOfSimJson, simFileName;
  std::tie(pathOfSimJson, simFileName) = splitPathToFile(pathToSimJson);

  auto simj = readAndParseJsonFile(pathToSimJson);
  if (simj.failure()) {
    for (const auto &e : simj.errors) fprintf(stderr, "%s\n", e.c_str());
    return 1;
  }
  auto simm = simj.result.object_items();
  simm["sim.json"] = pathToSimJson;

  auto pathToCropJson = simm["crop.json"].string_value();
  if (!isAbsolutePath(pathToCropJson)) simm["crop.json"] = pathOfSimJson + pathToCropJson;
  auto pathToSiteJson = simm["site.json"].string_value();
  if (!isAbsolutePath(pathToSiteJson)) simm["site.json"] = pathOfSimJson + pathToSiteJson;
  auto pathToClimateCSVInSim = simm["climate.csv"].string_value();
  if (!isAbsolutePath(pathToClimateCSVInSim))
    simm["climate.csv"] = pathOfSimJson + pathToClimateCSVInSim;

  std::map<std::string, Json> ps;
  ps["sim"] = Json(simm);
  ps["crop"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["crop.json"].string_value()), false)),
      false);
  ps["site"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["site.json"].string_value()), false)),
      false);

  Env env;
  std::string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Wessolek2009"] =
      Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);
  env.params.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchten"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  env.params.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenVereecken"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  env.params.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenToth"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenToth;
  env.params.siteParameters.calculateAndSetPwpFcSatFunctions["Toth"] = Soil::updateUnsetPwpFcSatFromToth;

  auto mergeResult = env_merge(&env, createEnvJsonFromJsonObjects(ps));
  printPossibleErrors(mergeResult, false);
  if (mergeResult.failure()) return 1;

  Output out = runMonica(env);

  string csvSep = simm["output"]["csv-options"]["csv-separator"].string_value();
  bool includeHeaderRow = simm["output"]["csv-options"]["include-header-row"].bool_value();
  bool includeUnitsRow = simm["output"]["csv-options"]["include-units-row"].bool_value();
  bool includeAggRows = simm["output"]["csv-options"]["include-aggregation-rows"].bool_value();

  for (const auto &d : out.data) {
    cout << "== section: " << replace(d.origSpec, "\"", "") << "\n";
    writeOutputHeaderRows(cout, d.outputIds, csvSep, includeHeaderRow, includeUnitsRow, includeAggRows);
    writeOutput(cout, d.outputIds, d.results, csvSep);
  }

  return 0;
}
