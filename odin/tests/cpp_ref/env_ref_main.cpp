/* Differential-test driver for the Odin create-env-from-json-config port.
 *
 * Mirrors what monica-run does up to (but not including) the model run:
 *   1. read+parse sim.json / crop.json / site.json
 *   2. resolve every ["include-from-file"|"ref"|"%"|KA5...] reference
 *   3. assemble the Env JSON
 *
 * and prints:
 *     RESOLVED-<crop|site|sim><TAB><dump>
 *     ENV<TAB><dump>
 *
 * The Odin side (odin/tests/env_ref/) does the same through monica/run.
 *
 * "climateData" is stripped from the Env before dumping: reading climate CSV is
 * phase 2 of the port, so the Odin side does not produce it yet. Remove the
 * strip (here and in the Odin driver) once phase 2 lands.
 *
 * Usage: env_ref <pathToSimJson>
 * See odin/tests/cpp_ref/run_env.sh.
 */

#include <cstdio>
#include <map>
#include <string>

#include "run/create-env-from-json-config.h"
#include "json11/json11-helper.h"
#include "tools/helper.h"

using namespace Tools;
using namespace json11;
using namespace monica;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: env_ref <pathToSimJson>\n");
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

  // resolve crop.json / site.json relative to the sim.json location, exactly as
  // monica-run-main.cpp does
  auto pathToCropJson = simm["crop.json"].string_value();
  if (!isAbsolutePath(pathToCropJson)) simm["crop.json"] = pathOfSimJson + pathToCropJson;
  auto pathToSiteJson = simm["site.json"].string_value();
  if (!isAbsolutePath(pathToSiteJson)) simm["site.json"] = pathOfSimJson + pathToSiteJson;
  auto pathToClimateCSV = simm["climate.csv"].string_value();
  if (!isAbsolutePath(pathToClimateCSV)) simm["climate.csv"] = pathOfSimJson + pathToClimateCSV;

  std::map<std::string, Json> ps;
  ps["sim"] = Json(simm);
  ps["crop"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["crop.json"].string_value()), false)),
      false);
  ps["site"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["site.json"].string_value()), false)),
      false);

  // --- 1. the three documents after reference resolution --------------------
  for (const char *name : {"crop", "site", "sim"}) {
    Json j = ps[name];
    // createEnvJsonFromJsonObjects adds this before resolving; do the same here
    // so the two dumps line up
    std::string err;
    if (!j.has_shape({{"include-file-base-path", Json::STRING}}, err)) {
      auto m = j.object_items();
      m["include-file-base-path"] = ps["sim"]["include-file-base-path"];
      j = m;
    }
    auto r = findAndReplaceReferences(j, j);
    printf("RESOLVED-%s\t%s\n", name, r.result.dump().c_str());
  }

  // --- 2. the assembled Env -------------------------------------------------
  auto env = createEnvJsonFromJsonObjects(ps);
  auto envm = env.object_items();
  envm.erase("climateData"); // phase 2, see the header comment
  printf("ENV\t%s\n", Json(envm).dump().c_str());

  return 0;
}
