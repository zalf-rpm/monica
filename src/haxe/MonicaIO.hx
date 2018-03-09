// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Authors:
// Michael Berg-Mohnicke <michael.berg-mohnicke@zalf.de>
//
// Maintainers:
// Currently maintained by the authors.
//
// This file is part of the MONICA model created at the Institute of
// Landscape Systems Analysis at the ZALF.
// Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

package monica.io;

import thx.Set;
import haxe.DynamicAccess;
import haxe.Http;
import haxe.Json;

using Lambda;
using MonicaIO.EResultTools;
using StringTools;

typedef EResult<T> = {
  ?result: T,
  ?errors: Array<String>,
}

class EResultTools<T> {
  static public function success<T>(r: EResult<T>) return r.result != null && r.errors != null && r.errors.length == 0;
  static public function failure<T>(r: EResult<T>) return !r.success();

  static public function result<T>(value: T) : EResult<T> return {result: value} 
  static public function error<T>(error: String) : EResult<T> return {errors: [error]} 
}




class MonicaIO {
  static public function main(): Void {
   
	trace("Hello World");
	  
  #if python
  var crop = sys.io.File.getContent("C:/Users/berg.ZALF-AD/GitHub/monica/installer/Hohenfinow2/crop.json");
  #elseif js
  var crop = Http.requestUrl("https://raw.githubusercontent.com/zalf-rpm/monica/master/installer/Hohenfinow2/crop.json");
  #end
	//trace(crop);
	//trace("----------------------------------");
	
	var cropj = Json.parse(crop);
	//trace(cropj);
	//trace("-----------------------------------");
	
	//trace(Json.stringify(cropj, null, "  "));
	//trace("--------------------------------------");	
	
	var res = findAndReplaceReferences(cropj, cropj);

	trace("\nResult:");
	trace(Json.stringify(res.result, null, "  "));
	trace("\nErrors:");
	trace(res.errors);
    
   
   
    //var t2: Test2 = new Hello();
    //trace(t2.hello());
  }

  static private function isArray(t) return Std.is(t, Type.getClass([]));
  static private function isObject(t) return Type.typeof(t) == TObject;
  static private function isString(t) return Std.is(t, Type.getClass(""));
  
  static public function findAndReplaceReferences(root: Dynamic, j: Dynamic): EResult<Dynamic> {
	  
    var sp = supportedPatterns();

    //auto jstr = j.dump();
    var success = true;
    var errors = [];

    if(isArray(j) && j.length > 0)
    {
      var arr = [];

      var arrayIsReferenceFunction = false;
      if(isString(j[0]))
      {
        var func = sp[j[0]];
        if(func != null)
        {
          arrayIsReferenceFunction = true;

          //check for nested function invocations in the arguments
          var funcArr = [];
          for(i in (j : Array<Dynamic>))
          {
            var r = findAndReplaceReferences(root, i);
            success = success && r.success();
            if(!r.success())
              for(e in r.errors)
                errors.push(e);
            funcArr.push(r.result);
          }

          //invoke function
          var jaes = func(root, funcArr);

          success = success && jaes.success();
          if(!jaes.success())
            for(e in jaes.errors)
              errors.push(e);

          //if successful try to recurse into result for functions in result
          if(jaes.success())
          {
            var r = findAndReplaceReferences(root, jaes.result);
            success = success && r.success();
            if(!r.success())
              for(e in r.errors)
                errors.push(e);
            return {result: r, errors: errors};
          }
          else
            return {errors: errors};
        }
      }

      if(!arrayIsReferenceFunction)
        for(jv in (j : Array<Dynamic>))
        {
          var r = findAndReplaceReferences(root, jv);
          success = success && r.success();
          if(!r.success())
            for(e in r.errors)
              errors.push(e);
          arr.push(r.result);
        }

      return {result: arr, errors: errors};
    }
    else if(isObject(j))
    {
      var jo : haxe.DynamicAccess<Dynamic> = j;
      var obj : haxe.DynamicAccess<Dynamic> = {};

      for(k in jo.keys())
      {
        var r = findAndReplaceReferences(root, jo[k]);
        success = success && r.success();
        if(!r.success())
          for(e in r.errors)
            errors.push(e);
        obj[k] = r.result;
      }

      return {result: obj, errors: errors};
    }

    return {result: j, errors: errors};
  }

  static private function isAbsolutePath(p : String) : Bool
  {
    return p.startsWith("/") 
        || (p.length == 2 && p.charAt(1) == ":") 
        || (p.length > 2 && p.charAt(1) == ":" 
          && (p.charAt(2) == "\\" 
            || p.charAt(2) == "/"));
  }

  static private function replaceEnvVars(path : String) : String
  {
    //"replace ${ENV_VAR} in path"
    var start_token = "${";
    var end_token = "}";
    var start_pos = path.indexOf(start_token);
    while(start_pos > -1)
    {
        var end_pos = path.indexOf(end_token, start_pos + 1);
        if(end_pos > -1)
        {
            var name_start = start_pos + 2;
            var env_var_name = path.substring(name_start, end_pos);
            var env_var_content = Sys.getEnv(env_var_name);
            if(env_var_content.length > 0)
            {
                path = path.replace(path.substring(start_pos, end_pos + 1), env_var_content);
                start_pos = path.indexOf(start_token);
            }
            else
                start_pos = path.indexOf(start_token, end_pos + 1);
        }
        else
            break;
    }

    return path;
  }

  private static function fixSystemSeparator(path : String) : String
  {
    //"fix system separator"
    var path = path.replace("\\", "/");
    var new_path = path;
    while(true)
    {
      new_path = path.replace("//", "/");
      if(new_path == path)
        break;
      path = new_path;
    }
    return new_path;
  }

  private static function readAndParseJsonFile(path : String) : EResult<Dynamic>
  {
    var res = {result: null, errors: ['Error opening file with path: $path !']};
    #if python
    res = {result: sys.io.File.getContent(path), errors: []};
    #end
    return res;
  }

def parse_json_string(jsonString):
    return {"result": json.loads(jsonString), "errors": [], "success": True}

  static private var _supportedPatterns : Map<String, Dynamic -> Dynamic -> EResult<Dynamic>>;
  static private var _refCache = new Map<String, EResult<Dynamic>>();

  public static function supportedPatterns() : Map<String, Dynamic -> Dynamic -> EResult<Dynamic>>
  {
    var ref = function(root : DynamicAccess<Dynamic>, j : Array<Dynamic>) : EResult<Dynamic>
    {
      if(j.length == 3
        && isString(j[1])
        && isString(j[2]))
      {
        var key1 : String = j[1];
        var key2 : String = j[2];

        var cres = _refCache['$key1|$key2'];
        if (cres != null)
				  return cres;
        
        var rk1 : DynamicAccess<Dynamic> = root[key1];
        if(rk1 != null 
          && isObject(rk1))
        {
					var rk1k2 = rk1[key2];
          if(rk1k2 != null)
          {
						var res = findAndReplaceReferences(root, rk1k2);
            _refCache['$key1|$key2'] = res; 
            return res;
          }
        }
      }
      return {result: j, errors: ['Couldn\'t resolve reference: $j !']};
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

    var fromFile = function(root : DynamicAccess<Dynamic>, j : Array<Dynamic>) : EResult<Dynamic>
    {
      if(j.length == 2
        && isString(j[1]))
      {
        var basePath : String = if(root.exists("include-file-base-path")) root["include-file-base-path"] else ".";
        var pathToFile : String = j[1];
        if(!isAbsolutePath(pathToFile))
          pathToFile = basePath + "/" + pathToFile;
        pathToFile = replaceEnvVars(pathToFile);
        pathToFile = fixSystemSeparator(pathToFile);
        var jo = readAndParseJsonFile(pathToFile);
        if(jo.success() && jo.result != null)
          return {result: jo.result};
        
        return {result: j, errors: ['Couldn\'t include file with path: $pathToFile !']};
      }

      return {result: j, errors: ['Couldn\'t include file with function: $j !']};
    };

  /*
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
    */
    /*
    auto percent = [](const Json&, const Json& j) -> EResult<Json>
    {
      if(j.array_items().size() == 2
        && j[1].is_number())
        return{j[1].number_value() / 100.0};
      return{j, string("Couldn't convert percent to decimal percent value: ") + j.dump() + "!"};
    };
    */

	if(_supportedPatterns == null)
      _supportedPatterns = [
        //{"include-from-db", fromDb},
        "include-from-file" => fromFile,
        "ref" => ref,
        //"humus_st2corg" => humus2corg,
        //"humus-class->corg" => humus2corg,
        //"ld_eff2trd" => bdc2rd,
        //"bulk-density-class->raw-density" => bdc2rd,
        //"KA5TextureClass2clay" => KA52clay,
        //"KA5-texture-class->clay" => KA52clay,
        //"KA5TextureClass2sand" => KA52sand,
        //"KA5-texture-class->sand" => KA52sand,
        //"sandAndClay2lambda" => sandClay2lambda,
        //"sand-and-clay->lambda" => sandClay2lambda,
        //"%" => percent
      ];

    return _supportedPatterns;
  }

  public static function createEnvJsonFromSimSiteCropJson(sim : haxe.DynamicAccess<Dynamic> 
                                                         ,site : haxe.DynamicAccess<Dynamic> 
                                                         ,crop : haxe.DynamicAccess<Dynamic> 
                                                         ,?climate : haxe.DynamicAccess<Dynamic>) : 
                                                          {env : Dynamic 
                                                          ,errors : Array<String>} 
  {
    var pathToParameters = sim["include-file-base-path"];

    var addBasePath = function(j : haxe.DynamicAccess<Dynamic>, base_path : String) : Void
    {
        if(j.exists("include-file-base-path"))
            j["include-file-base-path"] = base_path;
    };

    //collect all errors in all files and don't stop as early as possible
    var errors = Set.createString();
    var cropSiteSim2 = [];
    for(j in [crop, site, sim]) {
      addBasePath(j, pathToParameters);
      var r = findAndReplaceReferences(j, j);
      if(r.success())
        cropSiteSim2.push(r.result);
      else
        errors.pushMany(r.errors);
    }

    //if(!errors.empty())
    //{
    //  for(e in errors)
    //    cerr << e << endl;
    //  return Json();
    //}

    var cropj : DynamicAccess<Dynamic> = cropSiteSim2[0];
    var sitej : DynamicAccess<Dynamic> = cropSiteSim2[1];
    var simj : DynamicAccess<Dynamic> = cropSiteSim2[2];

    var env : DynamicAccess<Dynamic> = {};
    env["type"] = "Env";

    //store debug mode in env, take from sim.json, but prefer params map
    env["debugMode"] = simj["debug?"];

    var cpp = {
        type : "CentralParameterProvider"
      , userCropParameters : cropj["CropParameters"]
      , userEnvironmentParameters : sitej["EnvironmentParameters"]
      , userSoilMoistureParameters : sitej["SoilMoistureParameters"]
      , userSoilTemperatureParameters : sitej["SoilTemperatureParameters"]
      , userSoilTransportParameters : sitej["SoilTransportParameters"]
      , userSoilOrganicParameters : sitej["SoilOrganicParameters"]
      , simulationParameters : simj
      , siteParameters : sitej["SiteParameters"]
    };

    env["params"] = cpp;
    env["cropRotation"] = cropj["cropRotation"];
    env["events"] = simj["output"].get("events");
    env["outputs"] = simj["output"];

    env["pathToClimateCSV"] = simj["climate.csv"];
    env["csvViaHeaderOptions"] = simj["climate.csv-options"];
    
    //env["climateData"] = readClimateDataFromCSVFileViaHeaders(simj["climate.csv"].string_value(),
    //                                                simj["climate.csv-options"]);

    return {env: env, errors: errors};
  }





}