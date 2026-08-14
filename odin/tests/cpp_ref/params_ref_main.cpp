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

  // ---- tranche 2 -----------------------------------------------------------
  // defaults, pinning the C++ in-class initialisers
  {
    MineralFertilizerParameters d;
    DUMP("default-MineralFertilizer", mineralfertilizerparameters::to_json(&d));
  }
  {
    NMinApplicationParameters d;
    DUMP("default-NMinApplication", nminapplicationparameters::to_json(&d));
  }
  {
    IrrigationParameters d;
    DUMP("default-Irrigation", irrigationparameters::to_json(&d));
  }
  {
    AutomaticIrrigationParameters d;
    DUMP("default-AutomaticIrrigation", automaticirrigationparameters::to_json(&d));
  }
  {
    SiteParameters d;
    DUMP("default-Site", siteparameters::to_json(&d));
  }
  {
    SimulationParameters d;
    DUMP("default-Simulation", simulationparameters::to_json(&d));
  }
  {
    CropModuleParameters d;
    DUMP("default-CropModule", cropmoduleparameters::to_json(&d));
  }
  {
    EnvironmentParameters d;
    DUMP("default-Environment", environmentparameters::to_json(&d));
  }

  // merged from the real crop/site/sim documents
  {
    CropModuleParameters p;
    cropmoduleparameters::merge(&p, load(dir + "/crop.json"));
    DUMP("merged-CropModule", cropmoduleparameters::to_json(&p));
  }
  {
    EnvironmentParameters p;
    environmentparameters::merge(&p, load(dir + "/environment.json"));
    DUMP("merged-Environment", environmentparameters::to_json(&p));
  }

  // A few synthetic merges for the structs no general/*.json feeds, so the
  // merge paths (not just the defaults) are exercised.
  {
    std::string err;
    auto j = json11::Json::parse(
        R"({"id": "AN", "name": "ammonium nitrate", "Carbamid": 0.0, "NH4": 0.5, "NO3": 0.5})", err);
    MineralFertilizerParameters p;
    mineralfertilizerparameters::merge(&p, j);
    DUMP("merged-MineralFertilizer", mineralfertilizerparameters::to_json(&p));
  }
  {
    std::string err;
    auto j = json11::Json::parse(R"({"min": 40, "max": 120, "delayInDays": 10})", err);
    NMinApplicationParameters p;
    nminapplicationparameters::merge(&p, j);
    DUMP("merged-NMinApplication", nminapplicationparameters::to_json(&p));
  }
  {
    // exercises the endDate-from-"stopDate" quirk, the threshold double-write,
    // and the unit transforms
    std::string err;
    auto j = json11::Json::parse(
        R"({"irrigationParameters": {"nitrateConcentration": [5, "mg dm-3"], "fw": 0.7},
            "startDate": "1992-05-01", "stopDate": "1992-09-01",
            "amount": [17, "mm"], "threshold": [50, "%"],
            "trigger_if_nFC_below_%": [90, "%"],
            "calc_nFC_until_depth_m": [30, "cm"],
            "minDaysBetweenIrrigationEvents": 3})",
        err);
    AutomaticIrrigationParameters p;
    automaticirrigationparameters::merge(&p, j);
    DUMP("merged-AutomaticIrrigation", automaticirrigationparameters::to_json(&p));
    // endDate is not emitted by to_json, so check it separately
    printf("merged-AutomaticIrrigation-endDate\t%s\n", p.endDate.toIsoDateString().c_str());
  }
  {
    // every accepted rcp spelling
    for (const char *r : {"85", "8.5", "rcp85", "rcp8.5", "19", "nonsense"}) {
      std::string err;
      auto j = json11::Json::parse(std::string(R"({"rcp": ")") + r + R"("})", err);
      EnvironmentParameters p;
      environmentparameters::merge(&p, j);
      printf("rcp-str-%s\t%s\n", r, environmentparameters::to_json(&p)["rcp"].dump().c_str());
    }
    for (double r : {8.5, 85.0, 1.9, 19.0, 3.4}) {
      EnvironmentParameters p;
      environmentparameters::merge(&p, json11::Json::object{{"rcp", r}});
      printf("rcp-num-%g\t%s\n", r, environmentparameters::to_json(&p)["rcp"].dump().c_str());
    }
  }
  {
    std::string err;
    auto j = json11::Json::parse(
        R"({"groundwaterInformationAvailable": true,
            "groundwaterInfo": {"1991-01-01": 1.5, "1991-06-15": 2.25}})",
        err);
    MeasuredGroundwaterTableInformation p;
    measuredgroundwatertableinformation::merge(&p, j);
    DUMP("merged-Groundwater", measuredgroundwatertableinformation::to_json(&p));
  }

  // ---- tranche 3a ----------------------------------------------------------
  {
    YieldComponent d;
    DUMP("default-YieldComponent", yieldcomponent::to_json(&d));
  }
  {
    AutomaticHarvestParameters d;
    DUMP("default-AutomaticHarvest", automaticharvestparameters::to_json(&d));
  }
  {
    NMinCropParameters d;
    DUMP("default-NMinCrop", nmincropparameters::to_json(&d));
  }
  {
    OrganicMatterParameters d;
    DUMP("default-OrganicMatter", organicmatterparameters::to_json(&d));
  }
  {
    OrganicFertilizerParameters d;
    DUMP("default-OrganicFertilizer", organicfertilizerparameters::to_json(&d));
  }
  {
    CropResidueParameters d;
    DUMP("default-CropResidue", cropresidueparameters::to_json(&d));
  }

  {
    std::string err;
    auto j = json11::Json::parse(
        R"({"organId": 3, "yieldPercentage": 0.85, "yieldDryMatter": 0.86})", err);
    YieldComponent p;
    yieldcomponent::merge(&p, j);
    DUMP("merged-YieldComponent", yieldcomponent::to_json(&p));
  }
  {
    // harvestTime 0 == maturity; also shows that to_json emits "latestHavestDOY"
    // (sic) while merge reads "latestHarvestDOY"
    std::string err;
    auto j = json11::Json::parse(R"({"harvestTime": 0, "latestHarvestDOY": 300})", err);
    AutomaticHarvestParameters p;
    automaticharvestparameters::merge(&p, j);
    DUMP("merged-AutomaticHarvest", automaticharvestparameters::to_json(&p));
  }
  {
    std::string err;
    auto j = json11::Json::parse(
        R"({"samplingDepth": [0.9, "m"], "nTarget": [50, "kg"], "nTarget30": [30, "kg"]})", err);
    NMinCropParameters p;
    nmincropparameters::merge(&p, j);
    DUMP("merged-NMinCrop", nmincropparameters::to_json(&p));
  }
  {
    // a real crop-residue file, which exercises the whole OrganicMatterParameters
    // merge including AOM_CarbamidContent (which to_json then drops - see the
    // duplicate-key note in the Odin port)
    CropResidueParameters p;
    cropresidueparameters::merge(&p, load(dir + "/../crop-residues/wheat.json"));
    DUMP("merged-CropResidue", cropresidueparameters::to_json(&p));
    // the value to_json cannot show, printed directly to pin the data loss
    printf("merged-CropResidue-carbamid\t%.17g\n", p.vo_AOM_CarbamidContent);
  }
  {
    OrganicFertilizerParameters p;
    organicfertilizerparameters::merge(&p, load(dir + "/../organic-fertilisers/CAM.json"));
    DUMP("merged-OrganicFertilizer", organicfertilizerparameters::to_json(&p));
    printf("merged-OrganicFertilizer-carbamid\t%.17g\n", p.vo_AOM_CarbamidContent);
  }

#undef DUMP
  return 0;
}
