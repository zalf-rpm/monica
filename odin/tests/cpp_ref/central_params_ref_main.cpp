/* Differential-test driver for the Phase 1 capstone checkpoint (extended in
 * phase 3 checkpoint 3c to cover the now-populated soil profile).
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
 * parent key is misspelled. Since checkpoint 3c it also exercises the real
 * SiteParameters.vs_SoilParameters build (siteparameters::merge ->
 * createEqualSizedSoilPMs -> fcSatPwpFromKA5textureClass), which
 * centralparameterprovider::merge alone doesn't reach: SiteParameters.
 * calculateAndSetPwpFcSatFunctions must be pre-populated first, exactly as
 * monica-run-main.cpp:239-249 does before calling env_merge.
 *
 * `groundwaterInformation` remains untested here: centralparameterprovider::
 * to_json doesn't emit it on either side (it's commented out in the C++).
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
#include "soil/soil.h"
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

  // set available functions to calculate pwp, fc and sat before merging, exactly
  // as monica-run-main.cpp:238-249 does before env_merge
  CentralParameterProvider cpp;
  std::string pathToSoilDir = fixSystemSeparator(replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["Wessolek2009"] =
      Soil::getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(pathToSoilDir);
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchten"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenVereecken"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["VanGenuchtenToth"] =
      Soil::updateUnsetPwpFcSatFromVanGenuchtenToth;
  cpp.siteParameters.calculateAndSetPwpFcSatFunctions["Toth"] = Soil::updateUnsetPwpFcSatFromToth;

  centralparameterprovider::merge(&cpp, envParams);

  auto cppJson = centralparameterprovider::to_json(&cpp);
  printf("CPP\t%s\n", cppJson.dump().c_str());

  return 0;
}
