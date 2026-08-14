/* Differential-test driver for the Odin parameter-struct port.
 *
 * Reads the general/*.json parameter files the same way MONICA does, merges each
 * into its struct, and dumps the struct back out with to_json:
 *
 *     <structName><TAB><dump>
 *
 * Each struct is dumped twice: once from a default-constructed instance (which
 * pins the C++ in-class initialisers) and once after merging the real parameter
 * file. The Odin side (odin/tests/params_ref/) does the same.
 *
 * Usage: params_ref <pathToGeneralDir>
 * See odin/tests/cpp_ref/run_params.sh.
 */

#include <cstdio>
#include <string>

#include "core/monica-parameters.h"
#include "json11/json11-helper.h"

using namespace Tools;
using namespace json11;
using namespace monica;

static Json load(const std::string &path) {
  auto r = readAndParseJsonFile(path);
  if (r.failure()) {
    for (const auto &e : r.errors) fprintf(stderr, "%s\n", e.c_str());
    return Json();
  }
  return r.result;
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    fprintf(stderr, "usage: params_ref <pathToGeneralDir>\n");
    return 2;
  }
  std::string dir = argv[1];

#define DUMP(label, expr) printf("%s\t%s\n", label, (expr).dump().c_str())

  // defaults first - these pin the C++ in-class initialisers
  {
    SoilMoistureModuleParameters d;
    DUMP("default-SoilMoisture", soilmoisturemoduleparameters::to_json(&d));
  }
  {
    SoilTemperatureModuleParameters d;
    DUMP("default-SoilTemperature", soiltemperaturemoduleparameters::to_json(&d));
  }
  {
    SoilTransportModuleParameters d;
    DUMP("default-SoilTransport", soiltransportmoduleparameters::to_json(&d));
  }
  {
    SticsParameters d;
    DUMP("default-Stics", sticsparameters::to_json(&d));
  }
  {
    SoilOrganicModuleParameters d;
    DUMP("default-SoilOrganic", soilorganicmoduleparameters::to_json(&d));
  }

  // then merged from the real parameter files
  {
    SoilMoistureModuleParameters p;
    soilmoisturemoduleparameters::merge(&p, load(dir + "/soil-moisture.json"));
    DUMP("merged-SoilMoisture", soilmoisturemoduleparameters::to_json(&p));
  }
  {
    SoilTemperatureModuleParameters p;
    soiltemperaturemoduleparameters::merge(&p, load(dir + "/soil-temperature.json"));
    DUMP("merged-SoilTemperature", soiltemperaturemoduleparameters::to_json(&p));
  }
  {
    SoilTransportModuleParameters p;
    soiltransportmoduleparameters::merge(&p, load(dir + "/soil-transport.json"));
    DUMP("merged-SoilTransport", soiltransportmoduleparameters::to_json(&p));
  }
  {
    SoilOrganicModuleParameters p;
    auto j = load(dir + "/soil-organic.json");
    soilorganicmoduleparameters::merge(&p, j);
    DUMP("merged-SoilOrganic", soilorganicmoduleparameters::to_json(&p));
    // soilorganic's to_json drops the nested stics params, so dump them directly
    DUMP("merged-SoilOrganic-stics", sticsparameters::to_json(&p.sticsParams));
  }

#undef DUMP
  return 0;
}
