/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "env-json-from-json-config.h"
#include "tools/debug.h"
//#include "../run/run-monica.h"
//#include "../io/database-io.h"
#include "json11/json11-helper.h"
#include "tools/helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "soil/soil-from-db.h"
#include "../io/output.h"

using namespace std;
using namespace monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

//-----------------------------------------------------------------------------

const map<string, function<EResult<Json>(const Json&, const Json&)>>& supportedPatterns();

EResult<Json> monica::findAndReplaceReferences(const Json& root, const Json& j)
{
  auto sp = supportedPatterns();

  //auto jstr = j.dump();
  bool success = true;
  vector<string> errors;

  if(j.is_array() && j.array_items().size() > 0)
  {
    J11Array arr;

    bool arrayIsReferenceFunction = false;
    if(j[0].is_string())
    {
      auto p = sp.find(j[0].string_value());
      if(p != sp.end())
      {
        arrayIsReferenceFunction = true;

        //check for nested function invocations in the arguments
        J11Array funcArr;
        for(auto i : j.array_items())
        {
          auto r = findAndReplaceReferences(root, i);
          success = success && r.success();
          if(!r.success())
            for(auto e : r.errors)
              errors.push_back(e);
          funcArr.push_back(r.result);
        }

        //invoke function
        auto jaes = (p->second)(root, funcArr);

        success = success && jaes.success();
        if(!jaes.success())
          for(auto e : jaes.errors)
            errors.push_back(e);

        //if successful try to recurse into result for functions in result
        if(jaes.success())
        {
          auto r = findAndReplaceReferences(root, jaes.result);
          success = success && r.success();
          if(!r.success())
            for(auto e : r.errors)
              errors.push_back(e);
          return{r.result, errors};
        }
        else
          return{J11Object(), errors};
      }
    }

    if(!arrayIsReferenceFunction)
      for(auto jv : j.array_items())
      {
        auto r = findAndReplaceReferences(root, jv);
        success = success && r.success();
        if(!r.success())
          for(auto e : r.errors)
            errors.push_back(e);
        arr.push_back(r.result);
      }

    return{arr, errors};
  }
  else if(j.is_object())
  {
    J11Object obj;

    for(auto p : j.object_items())
    {
      auto r = findAndReplaceReferences(root, p.second);
      success = success && r.success();
      if(!r.success())
        for(auto e : r.errors)
          errors.push_back(e);
      obj[p.first] = r.result;
    }

    return{obj, errors};
  }

  return{j, errors};
}

//-----------------------------------------------------------------------------

const map<string, function<EResult<Json>(const Json&, const Json&)>>& supportedPatterns()
{
  auto ref = [](const Json& root, const Json& j) -> EResult<Json>
  {
    static map<pair<string, string>, EResult<Json>> cache;
    if(j.array_items().size() == 3
       && j[1].is_string()
       && j[2].is_string())
    {
      string key1 = j[1].string_value();
      string key2 = j[2].string_value();

      auto it = cache.find(make_pair(key1, key2));
      if(it != cache.end())
        return it->second;
      
      auto res = findAndReplaceReferences(root, root[key1][key2]);
      cache[make_pair(key1, key2)] = res;
      return res;
    }
    return{j, string("Couldn't resolve reference: ") + j.dump() + "!"};
  };

  /*
  auto fromDb = [](const Json&, const Json& j) -> EResult<Json>
  {
    if((j.array_items().size() >= 3 && j[1].is_string())
       || (j.array_items().size() == 2 && j[1].is_object()))
    {
      bool isParamMap = j[1].is_object();

      auto type = isParamMap ? j[1]["type"].string_value() :j[1].string_value();
      string err;
      string db = isParamMap && j[1].has_shape({{"db", Json::STRING}}, err)
            ? j[1]["db"].string_value()
            : "";
      if(type == "mineral_fertiliser")
      {
        if(db.empty())
          db = "monica";
        auto name = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{getMineralFertiliserParametersFromMonicaDB(name, db).to_json()};
      }
      else if(type == "organic_fertiliser")
      {
        if(db.empty())
          db = "monica";
        auto name = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{getOrganicFertiliserParametersFromMonicaDB(name, db)->to_json()};
      }
      else if(type == "crop_residue"
          && j.array_items().size() >= 3)
      {
        if(db.empty())
          db = "monica";
        auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
        auto residueType = isParamMap ? j[1]["residue-type"].string_value()
                        : j.array_items().size() == 4 ? j[3].string_value()
                                      : "";
        return{getResidueParametersFromMonicaDB(species, residueType, db)->to_json()};
      }
      else if(type == "species")
      {
        if(db.empty())
          db = "monica";
        auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
        return{getSpeciesParametersFromMonicaDB(species, db)->to_json()};
      }
      else if(type == "cultivar"
          && j.array_items().size() >= 3)
      {
        if(db.empty())
          db = "monica";
        auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
        auto cultivar = isParamMap ? j[1]["cultivar"].string_value()
                       : j.array_items().size() == 4 ? j[3].string_value()
                                     : "";
        return{getCultivarParametersFromMonicaDB(species, cultivar, db)->to_json()};
      }
      else if(type == "crop"
          && j.array_items().size() >= 3)
      {
        if(db.empty())
          db = "monica";
        auto species = isParamMap ? j[1]["species"].string_value() : j[2].string_value();
        auto cultivar = isParamMap ? j[1]["cultivar"].string_value()
                       : j.array_items().size() == 4 ? j[3].string_value()
                                     : "";
        return{getCropParametersFromMonicaDB(species, cultivar, db)->to_json()};
      }
      else if(type == "soil-temperature-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserSoilTemperatureParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "environment-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserEnvironmentParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "soil-organic-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserSoilOrganicParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "soil-transport-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserSoilTransportParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "soil-moisture-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserSoilTemperatureParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "crop-params")
      {
        if(db.empty())
          db = "monica";
        auto module = isParamMap ? j[1]["name"].string_value() : j[2].string_value();
        return{readUserCropParametersFromDatabase(module, db).to_json()};
      }
      else if(type == "soil-profile"
          && (isParamMap
            || (!isParamMap && j[2].is_number())))
      {
        if(db.empty())
          db = "soil";
        //vector<Json> spjs;
        int profileId = isParamMap ? j[1]["id"].int_value() : j[2].int_value();
        auto spjs = Soil::jsonSoilParameters(db, profileId);
        //auto sps = Soil::soilParameters(db, profileId);
        //for(auto sp : *sps)
        //	spjs.push_back(sp.to_json());

        return{spjs};
      }
      else if(type == "soil-layer"
          && (isParamMap
            || (j.array_items().size() == 4
              && j[2].is_number()
              && j[3].is_number())))
      {
        if(db.empty())
          db = "soil";
        int profileId = isParamMap ? j[1]["id"].int_value() : j[2].int_value();
        size_t layerNo = size_t(isParamMap ? j[1]["no"].int_value() : j[3].int_value());
        auto sps = Soil::soilParameters(db, profileId);
        if(0 < layerNo && layerNo <= sps->size())
          return{sps->at(layerNo - 1).to_json()};

        return{j, string("Couldn't load soil-layer from database: ") + j.dump() + "!"};
      }
    }

    return{j, string("Couldn't load data from DB: ") + j.dump() + "!"};
  };
  */

  auto fromFile = [](const Json& root, const Json& j) -> EResult<Json>
  {
    string error;

    if(j.array_items().size() == 2
       && j[1].is_string())
    {
      string basePath = string_valueD(root, "include-file-base-path", ".");
      string pathToFile = j[1].string_value();
      if(!isAbsolutePath(pathToFile))
        pathToFile = basePath + "/" + pathToFile;
      pathToFile = replaceEnvVars(pathToFile);
      pathToFile = fixSystemSeparator(pathToFile);
      auto jo = readAndParseJsonFile(pathToFile);
      if(jo.success() && !jo.result.is_null())
        return{jo.result};
      
      return{j, string("Couldn't include file with path: '") + pathToFile + "'!"};
    }

    return{j, string("Couldn't include file with function: ") + j.dump() + "!"};
  };

  auto humus2corg = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 2
       && j[1].is_number())
    {
      auto ecorg = Soil::humusClass2corg(j[1].int_value());
      if(ecorg.success())
        return{ecorg.result};
      else
        return{j, ecorg.errors};
    }
    return{j, string("Couldn't convert humus level to corg: ") + j.dump() + "!"};
  };

  auto bdc2rd = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 3
       && j[1].is_number()
       && j[2].is_number())
    {
      auto erd = Soil::bulkDensityClass2rawDensity(j[1].int_value(), j[2].number_value());
      if(erd.success())
        return{erd.result};
      else
        return{j, erd.errors};
    }
    return{j, string("Couldn't convert bulk density class to raw density using function: ") + j.dump() + "!"};
  };

  auto KA52clay = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 2
       && j[1].is_string())
    {
      auto ec = Soil::KA5texture2clay(j[1].string_value());
      if(ec.success())
        return{ec.result};
      else 
        return{j, ec.errors};
    }
    return{j, string("Couldn't get soil clay content from KA5 soil class: ") + j.dump() + "!"};
  };

  auto KA52sand = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 2
       && j[1].is_string())
    {
      auto es = Soil::KA5texture2sand(j[1].string_value());
      if(es.success())
        return{es.result};
      else
        return{j, es.errors};
    }
    return{j, string("Couldn't get soil sand content from KA5 soil class: ") + j.dump() + "!"};;
  };

  auto sandClay2lambda = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 3
       && j[1].is_number()
       && j[2].is_number())
      return{Soil::sandAndClay2lambda(j[1].number_value(), j[2].number_value())};
    return{j, string("Couldn't get lambda value from soil sand and clay content: ") + j.dump() + "!"};
  };

  auto percent = [](const Json&, const Json& j) -> EResult<Json>
  {
    if(j.array_items().size() == 2
       && j[1].is_number())
      return{j[1].number_value() / 100.0};
    return{j, string("Couldn't convert percent to decimal percent value: ") + j.dump() + "!"};
  };

  static map<string, function<EResult<Json>(const Json&,const Json&)>> m{
      //{"include-from-db", fromDb},
      {"include-from-file", fromFile},
      {"ref", ref},
      {"humus_st2corg", humus2corg},
      {"humus-class->corg", humus2corg},
      {"ld_eff2trd", bdc2rd},
      {"bulk-density-class->raw-density", bdc2rd},
      {"KA5TextureClass2clay", KA52clay},
      {"KA5-texture-class->clay", KA52clay},
      {"KA5TextureClass2sand", KA52sand},
      {"KA5-texture-class->sand", KA52sand},
      {"sandAndClay2lambda", sandClay2lambda},
      {"sand-and-clay->lambda", sandClay2lambda},
      {"%", percent}};
  return m;
}

//-----------------------------------------------------------------------------

Json monica::createEnvJsonFromJsonStrings(std::map<std::string, std::string> params)
{
  map<string, Json> ps;
  for(const auto& p : map<string, string>({{"crop-json-str", "crop"}, {"site-json-str", "site"}, {"sim-json-str", "sim"}}))
    ps[p.second] = printPossibleErrors(parseJsonString(params[p.first]));

  return createEnvJsonFromJsonObjects(ps);
}

Json monica::createEnvJsonFromJsonObjects(std::map<std::string, json11::Json> params)
{
  vector<Json> cropSiteSim;
  for (auto name : { "crop", "site", "sim" })
    cropSiteSim.push_back(params[name]);

  for (auto& j : cropSiteSim)
    if (j.is_null())
      return Json();

  string pathToParameters = cropSiteSim.at(2)["include-file-base-path"].string_value();

  auto addBasePath = [&](Json& j, string basePath)
  {
    string err;
    if (!j.has_shape({ {"include-file-base-path", Json::STRING} }, err))
    {
      auto m = j.object_items();
      m["include-file-base-path"] = pathToParameters;
      j = m;
    }
  };

  vector<Json> cropSiteSim2;
  //collect all errors in all files and don't stop as early as possible
  set<string> errors;
  for (auto& j : cropSiteSim)
  {
    addBasePath(j, pathToParameters);
    auto r = findAndReplaceReferences(j, j);
    if (r.success())
      cropSiteSim2.push_back(r.result);
    else
      errors.insert(r.errors.begin(), r.errors.end());
  }

  if (!errors.empty())
  {
    for (auto e : errors)
      cerr << e << endl;
    return Json();
  }

  auto cropj = cropSiteSim2.at(0);
  auto sitej = cropSiteSim2.at(1);
  auto simj = cropSiteSim2.at(2);

  J11Object env;
  env["type"] = "Env";

  //store debug mode in env, take from sim.json, but prefer params map
  env["debugMode"] = simj["debug?"].bool_value();

  J11Object cpp = {
      {"type", "CentralParameterProvider"}
    , {"userCropParameters", cropj["CropParameters"]}
    , {"userEnvironmentParameters", sitej["EnvironmentParameters"]}
    , {"userSoilMoistureParameters", sitej["SoilMoistureParameters"]}
    , {"userSoilTemperatureParameters", sitej["SoilTemperatureParameters"]}
    , {"userSoilTransportParameters", sitej["SoilTransportParameters"]}
    , {"userSoilOrganicParameters", sitej["SoilOrganicParameters"]}
    , {"simulationParameters", simj}
    , {"siteParameters", sitej["SiteParameters"]}
  };

  if (!sitej["groundwaterInformation"].is_null()) {
    cpp["groundwaterInformation"] = sitej["groundwaterInformation"];
  }

  env["params"] = cpp;
  env["cropRotation"] = cropj["cropRotation"];
  if(cropj["cropRotation2"].is_array()) env["cropRotation2"] = cropj["cropRotation2"];
  env["cropRotations"] = cropj["cropRotations"];
  if(cropj["cropRotations2"].is_array()) env["cropRotations2"] = cropj["cropRotations2"];
  env["events"] = simj["output"]["events"];
  if(simj["output"]["events"].is_array()) env["events2"] = simj["output"]["events2"];
  env["outputs"] = J11Object{ {"output", J11Object{ {"obj-outputs?", simj["output"]["obj-outputs"].bool_value() } } } };

  env["pathToClimateCSV"] = simj["climate.csv"];
  auto csvos = simj["climate.csv-options"].object_items();
  csvos["latitude"] = double_valueD(sitej["SiteParameters"], "Latitude", 0.0);
  env["csvViaHeaderOptions"] = csvos;
    
  if(simj["climate.csv"].is_string() && !simj["climate.csv"].string_value().empty())
    env["climateData"] = printPossibleErrors(readClimateDataFromCSVFileViaHeaders(simj["climate.csv"].string_value(),
                                                              env["csvViaHeaderOptions"]));
  else if(simj["climate.csv"].is_array() && !simj["climate.csv"].array_items().empty())
    env["climateData"] = printPossibleErrors(readClimateDataFromCSVFilesViaHeaders(toStringVector(simj["climate.csv"].array_items()),
                                                               env["csvViaHeaderOptions"]));

  return env;
}


