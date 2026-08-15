/* Differential-test driver for the Phase 1 capstone checkpoint.
 *
 * Runs the Hohenfinow2 fixture through the whole reference-resolution /
 * Env-assembly machinery (same as env_ref_main.cpp), then merges the assembled
 * `env["params"]` sub-object into a CentralParameterProvider and dumps
 * centralparameterprovider::to_json.
 *
 * This is the checkpoint plan-odin.md calls "the CentralParameterProvider
 * checkpoint": it wires all 26 tranche-1c parameter structs together against
 * real fixture data, which per-struct tests (run_params.sh) cannot catch - a
 * field read under the wrong key, or a sub-struct never reached because its
 * parent key is misspelled.
 *
 * PHASE 3 GAP: SiteParameters.vs_SoilParameters (-> SoilProfileParameters) is
 * not built on the Odin side yet (see site_parameters_to_json's "PHASE SCOPE"
 * comment) - its to_json always emits an empty array there. The C++ side here
 * is normalised to match (SoilProfileParameters forced to []) so the rest of
 * the merge is actually exercised instead of failing on a known, expected gap.
 * `groundwaterInformation` is a second known gap, but needs no normalisation:
 * centralparameterprovider::to_json doesn't emit it on either side (it's
 * commented out in the C++), so it is simply untested here, not a source of
 * mismatch.
 *
 * Usage: central_params_ref <pathToSimJson>
 * See odin/tests/cpp_ref/run_central_params.sh.
 */

#include <cstdio>
#include <map>
#include <string>

#include "core/monica-parameters.h"
#include "json11/json11-helper.h"
#include "run/create-env-from-json-config.h"
#include "tools/helper.h"

using namespace Tools;
using namespace json11;
using namespace monica;

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: central_params_ref <pathToSimJson>\n");
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

  std::map<std::string, Json> ps;
  ps["sim"] = Json(simm);
  ps["crop"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["crop.json"].string_value()), false)),
      false);
  ps["site"] = printPossibleErrors(
      parseJsonString(printPossibleErrors(readFile(simm["site.json"].string_value()), false)),
      false);

  auto env = createEnvJsonFromJsonObjects(ps);
  auto envParams = env["params"];

  CentralParameterProvider cpp;
  centralparameterprovider::merge(&cpp, envParams);

  auto cppJson = centralparameterprovider::to_json(&cpp).object_items();
  auto sitej = cppJson["siteParameters"].object_items();
  sitej["SoilProfileParameters"] = Json::array{}; // phase 3 gap - see header comment
  cppJson["siteParameters"] = sitej;

  printf("CPP\t%s\n", Json(cppJson).dump().c_str());

  return 0;
}
