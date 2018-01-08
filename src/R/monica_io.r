# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# Authors:
# Michael Berg-Mohnicke <michael.berg@zalf.de>
#
# Maintainers:
# Currently maintained by the authors.
#
# This file is part of the MONICA model created at the Institute of
# Landscape Systems Analysis at the ZALF.
# Copyright (C: Leibniz Centre for Agricultural Landscape Research (ZALF)

#import os
#import time
#import json
#import types
#import sys
#sys.path.insert(0, "C:\\Users\\berg.ZALF-AD\\GitHub\\monica\\project-files\\Win32\\Debug")
#sys.path.insert(0, "C:\\Users\\berg.ZALF-AD\\GitHub\\monica\\project-files\\Win32\\Release")	 # path to monica_python.pyd or monica_python.so
#print "sys.path: ", sys.path

#import soil_io
#import monica_python
#print "path to monica_python: ", monica_python.__file__

#print "sys.version: ", sys.version

library("stringr")

print("local monica_io.py")

OP_AVG = 1
OP_MEDIAN = 2
OP_SUM = 3
OP_MIN = 4
OP_MAX = 5
OP_FIRST = 6
OP_LAST = 7
OP_NONE = 8
OP_UNDEFINED_OP_ = 9


ORGAN_ROOT = 1
ORGAN_LEAF = 2
ORGAN_SHOOT = 3
ORGAN_FRUIT = 4
ORGAN_STRUCT = 5
ORGAN_SUGAR = 6
ORGAN_UNDEFINED_ORGAN_ = 7


oid_is_organ <- function(oid)
{
  oid["organ"] != ORGAN_UNDEFINED_ORGAN_
}

oid_is_range <- function(oid)
{
  oid["fromLayer"] >= 0 && oid["toLayer"] >= 0 #\
    #&& oid["fromLayer"] < oid["toLayer"]
}

op_to_string <- function(op)
{
  switch(op,
    OP_AVG: "AVG",
    OP_MEDIAN: "MEDIAN",
    OP_SUM: "SUM",
    OP_MIN: "MIN",
    OP_MAX: "MAX",
    OP_FIRST: "FIRST",
    OP_LAST: "LAST",
    OP_NONE: "NONE",
    OP_UNDEFINED_OP_: "undef",
    "undef"
  )
}
  

organ_to_string <- function(organ)
{
  switch(organ, 
    ORGAN_ROOT: "Root",
    ORGAN_LEAF: "Leaf",
    ORGAN_SHOOT: "Shoot",
    ORGAN_FRUIT: "Fruit",
    ORGAN_STRUCT: "Struct",
    ORGAN_SUGAR: "Sugar",
    ORGAN_UNDEFINED_ORGAN_: "undef",
    "undef"
  )
}


oid_to_string <- function(oid, include_time_agg)
{
  paste(
    "[", 
    oid["name"],
    if(oid_is_organ(oid)) 
      paste(",", organ_to_string(oid["organ"]))
    else {
      if(oid_is_range(oid))
        paste(", [", 
              oid["fromLayer"] + 1,
              ", ", 
              oid["toLayer"] + 1,
              if(oid["layerAggOp"] != OP_NONE)
                paste(",", op_to_string(oid["layerAggOp"]))
              else
                "",
              "]",
              sep = "")
      else {
        if(oid["fromLayer"] >= 0)
          paste(",", oid["fromLayer"] + 1)
        else
          ""
      }
    },
    if(include_time_agg)
      paste(",", op_to_string(oid["timeAggOp"]))
    else
     "",
    "]"
  )
}

write_output_header_rows <- function(output_ids,
                                     include_header_row=TRUE,
                                     include_units_row=TRUE,
                                     include_time_agg=FALSE)
{
  row1 = list()
  row2 = list()
  row3 = list()
  row4 = list()
  for(oid in output_ids)
  {
    from_layer <- oid["fromLayer"]
    to_layer <- oid["toLayer"]
    is_organ <- oid_is_organ(oid)
    is_range <- oid_is_range(oid) && oid["layerAggOp"] == OP_NONE
    if(is_organ)
        # organ is being represented just by the value of fromLayer currently
        to_layer <- from_layer <- oid["organ"]
    else
    { 
      if(is_range)
      {
        from_layer <- from_layer + 1
        to_layer <- to_layer + 1     # display 1-indexed layer numbers to users
      }
      else
        to_layer <- from_layer # for aggregated ranges, which aren't being displayed as range
    }
      
    for(i in from_layer : to_layer)
    {
      str1 <- ""
      if(is_organ)
        str1 <- paste(str1, 
                      if(nchar(oid["displayName"]) == 0) 
                        paste(oid["name"], "/", organ_to_string(oid["organ"]), sep="")
                      else 
                        oid["displayName"], sep="")
      else {
        if(is_range)
          str1 <- paste(str1, 
                        if(nchar(oid["displayName"]) == 0)
                          paste(oid["name"], "_", i, sep="")
                        else 
                          oid["displayName"], sep="")
        else
          str1 <- paste(str1, 
                        if(nchar(oid["displayName"]) == 0)
                          oid["name"] 
                        else 
                          oid["displayName"], sep="")
      }
      list.append(row1, str1)
      list.append(row4, paste("j:", gsub("\"", "", oid["jsonInput"]), sep=""))
      list.append(row3, paste("m:",  oid_to_string(oid, include_time_agg), sep=""))
      list.append(row2, paste("[", oid["unit"], "]", sep=""))
    }
  }

  out = list()
  if(include_header_row)
    list.append(out, row1)
  if(include_units_row)
    list.append(out, row4)
  if(include_time_agg)
  {
    list.append(out, row3)
    list.append(out, row2)
  }
  
  out
}


write_output <- function(output_ids, values)
{
  #"write actual output lines"
  out <- list()
  if(length(values) > 0)
  {
    for(k in 0 : length(values[1]))
    {
      i <- 0
      row <- list()
      for(. in output_ids)
      {
        j__ <- values[i][k]
        if(typeof(j__, "list"))
        {
          for(jv_ in j__)
            list.append(row, jv_)
        }
        else
          list.append(row, j__)
        i <- i + 1
      }
      out.append(row)
    }
  }
  out
}

is_absolute_path <- function(p)
{
  #"is absolute path"
  str_sub(p, 1, 1) == "/" || 
    (nchar(p) == 2 && str_sub(p, 2, 2) == ":") ||
    (nchar(p) > 2 && str_sub(p, 2, 2) == ":" &&
      (str_sub(p, 3, 3) == "\\" || 
        str_sub(p, 3, 3) == "/"))
}

fix_system_separator <- function(path)
{
  #"fix system separator"
  str_replace_all(path, "\\", "/")
}

replace_env_vars <- function(path)
{
  #"replace ${ENV_VAR} in path"
  start_token <- "\\$\\{"
  end_token <- "\\}"
  start_pos <- str_locate(path, start_token)["start"]
  print("start_pos:", start_pos)
  while(!is.na(start_pos))
  {
    end_pos = str_locate(str_sub(path, start_pos+1), end_token)["start"] + start_pos
    if(is.na(end_pos)){
      name_start = start_pos + 2
      env_var_name = str_sub(path, name_start, end_pos)
      env_var_content <- Sys.getenv(env_var_name)
      if(nchar(env_var_content) > 0){
        path = str_replace(path, paste(start_token, env_var_name, end_token, sep=""), env_var_content)
        start_pos = str_locate(path, start_token)["start"]
      }
      else
        start_pos = str_locate(str_sub(path, end_pos + 1), start_token)
    }
  }

  path
}

default_value <- function(dic, key, default)
{
  "return default value if key not there"
  if(is.null(dic[key][[1]])) default else dic[key]
}

read_and_parse_json_file <- function(path)
{
  tryCatch(
    list("result" = fromJSON(file = path), "errors" = list(), "success" = TRUE),
    error = function(e) 
    {
      list("result" = list(),
           "errors" = list(paste("Error opening file with path : '", path, "'!", sep="")),
           "success" = FALSE)
    }
  )
}

parse_json_string <- function(jsonString)
{
  list("result" = fromJSON(jsonString), "errors" = list(), "success" = TRUE)
}

is_string_type <- function(j)
{
  typeof(j) == "character"
}

find_and_replace_references <- function(root, j)
{
  sp <- supported_patterns()

  success <- TRUE
  errors <- list()

  if(typeof(j) == "list" && length(j) > 0)
  {
      arr <- list()
      array_is_reference_function <- FALSE

      if is_string_type(j[0]):
          if j[0] in sp:
              f = sp[j[0]]
              array_is_reference_function = TRUE

              #check for nested function invocations in the arguments
              funcArr = []
              for i in j:
                  res = find_and_replace_references(root, i)
                  success = success and res["success"]
                  if not res["success"]:
                      for err in res["errors"]:
                          errors.append(err)
                  funcArr.append(res["result"])

              #invoke function
              jaes = f(root, funcArr)

              success = success and jaes["success"]
              if not jaes["success"]:
                  for err in jaes["errors"]:
                      errors.append(err)

              #if successful try to recurse into result for functions in result
              if jaes["success"]:
                  res = find_and_replace_references(root, jaes["result"])
                  success = success and res["success"]
                  if not res["success"]:
                      for err in res["errors"]:
                          errors.append(err)
                  return {"result": res["result"], "errors": errors, "success": len(errors) == 0}
              else:
                  return {"result": {}, "errors": errors, "success": len(errors) == 0}

      if not array_is_reference_function:
          for jv in j:
              res = find_and_replace_references(root, jv)
              success = success and res["success"]
              if not res["success"]:
                  for err in res["errors"]:
                      errors.append(err)
              arr.append(res["result"])

      return list("result" = arr, "errors" = errors, "success" = length(errors) == 0)

  else 
  {
    if(isinstance(j, types.DictType))
    {
      obj = {}

      for k, v in j.iteritems():
          r = find_and_replace_references(root, v)
          success = success and r["success"]
          if not r["success"]:
              for e in r["errors"]:
                  errors.append(e)
          obj[k] = r["result"]

      return list("result" = obj, "errors" = errors, "success" = length(errors) == 0)
    }
  }

  list("result" = j, "errors" = errors, "success" = length(errors) == 0)


def supported_patterns():

    def ref(root, j):
        if "cache" not in ref.__dict__:
            ref.cache = {}

        if len(j) == 3 \
         and is_string_type(j[1]) \
         and is_string_type(j[2]):

            key1 = j[1]
            key2 = j[2]

            if (key1, key2) in ref.cache:
                return ref.cache[(key1, key2)]

            res = find_and_replace_references(root, root[key1][key2])
            ref.cache[(key1, key2)] = res
            return res

        return {"result": j,
                "errors": ["Couldn't resolve reference: " + json.dumps(j) + "!"],
                "success" : False}

    

    def from_file(root, j__):
        "include from file"
        if len(j__) == 2 and is_string_type(j__[1]):

            base_path = default_value(root, "include-file-base-path", ".")
            path_to_file = j__[1]
            if not is_absolute_path(path_to_file):
                path_to_file = base_path + "/" + path_to_file
            path_to_file = replace_env_vars(path_to_file)
            path_to_file = fix_system_separator(path_to_file)
            jo_ = read_and_parse_json_file(path_to_file)
            if jo_["success"] and not isinstance(jo_["result"], types.NoneType):
                return {"result": jo_["result"], "errors": [], "success": True}

            return {"result": j__,
                    "errors": ["Couldn't include file with path: '" + path_to_file + "'!"],
                    "success": False}

        return {"result": j__,
                "errors": ["Couldn't include file with function: " + json.dumps(j__) + "!"],
                "success": False}

    def humus_to_corg(_, j__):
        "convert humus level to corg"
        if len(j__) == 2 \
            and isinstance(j__[1], types.IntType):
            return {"result": soil_io.humus_class_to_corg(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't convert humus level to corg: " + json.dumps(j__) + "!"],
                "success": False}

    def ld_to_trd(_, j__):
        if len(j__) == 3 \
            and isinstance(j__[1], types.IntType) \
            and isinstance(j__[2], types.FloatType):
            return {"result": soil_io.bulk_density_class_to_raw_density(j__[1], j__[2]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't convert bulk density class to raw density using function: " + json.dumps(j__) + "!"],
                "success": False}

    def ka5_to_clay(_, j__):
        if len(j__) == 2 and is_string_type(j__[1]):
            return {"result": soil_io.ka5_texture_to_clay(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get soil clay content from KA5 soil class: " + json.dumps(j__) + "!"],
                "success": False}

    def ka5_to_sand(_, j__):
        if len(j__) == 2 and is_string_type(j__[1]):
            return {"result": soil_io.ka5_texture_to_sand(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get soil sand content from KA5 soil class: " + json.dumps(j__) + "!"],
                "success": False}

    def sand_clay_to_lambda(_, j__):
        if len(j__) == 3 \
            and isinstance(j__[1], types.FloatType) \
            and isinstance(j__[2], types.FloatType):
            return {"result": soil_io.sand_and_clay_to_lambda(j__[1], j__[2]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get lambda value from soil sand and clay content: " + json.dumps(j__) + "!"],
                "success": False}

    def percent(_, j__):
        if len(j__) == 2 and isinstance(j__[1], types.FloatType):
            return {"result": j__[1] / 100.0, "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't convert percent to decimal percent value: " + json.dumps(j__) + "!"],
                "success": False}

    if "m" not in supported_patterns.__dict__:
        supported_patterns.m = {
            #"include-from-db": fromDb
            "include-from-file": from_file,
            "ref": ref,
            "humus_st2corg": humus_to_corg,
            "humus-class->corg": humus_to_corg,
            "ld_eff2trd": ld_to_trd,
            "bulk-density-class->raw-density": ld_to_trd,
            "KA5TextureClass2clay": ka5_to_clay,
            "KA5-texture-class->clay": ka5_to_clay,
            "KA5TextureClass2sand": ka5_to_sand,
            "KA5-texture-class->sand": ka5_to_sand,
            "sandAndClay2lambda": sand_clay_to_lambda,
            "sand-and-clay->lambda": sand_clay_to_lambda,
            "%": percent
        }

    return supported_patterns.m


def print_possible_errors(errs, include_warnings=False):
    if not errs["success"]:
        for err in errs["errors"]:
            print err

    if include_warnings and "warnings" in errs:
        for war in errs["warnings"]:
            print war

    return errs["success"]


def create_env_json_from_json_config(crop_site_sim):
    "create the json version of the env from the json config files"
    for k, j in crop_site_sim.iteritems():
        if j is None:
            return None

    path_to_parameters = crop_site_sim["sim"]["include-file-base-path"]

    def add_base_path(j, base_path):
        "add include file base path if not there"
        if not "include-file-base-path" in j:
            j["include-file-base-path"] = base_path

    crop_site_sim2 = {}
    #collect all errors in all files and don't stop as early as possible
    errors = set()
    for k, j in crop_site_sim.iteritems():
        if k == "climate":
            continue

        add_base_path(j, path_to_parameters)
        res = find_and_replace_references(j, j)
        if res["success"]:
            crop_site_sim2[k] = res["result"]
        else:
            errors.update(res["errors"])

    if len(errors) > 0:
        for err in errors:
            print err
        return None

    cropj = crop_site_sim2["crop"]
    sitej = crop_site_sim2["site"]
    simj = crop_site_sim2["sim"]

    env = {}
    env["type"] = "Env"

    #store debug mode in env, take from sim.json, but prefer params map
    env["debugMode"] = simj["debug?"]

    cpp = {
        "type": "CentralParameterProvider",
        "userCropParameters": cropj["CropParameters"],
        "userEnvironmentParameters": sitej["EnvironmentParameters"],
        "userSoilMoistureParameters": sitej["SoilMoistureParameters"],
        "userSoilTemperatureParameters": sitej["SoilTemperatureParameters"],
        "userSoilTransportParameters": sitej["SoilTransportParameters"],
        "userSoilOrganicParameters": sitej["SoilOrganicParameters"],
        "simulationParameters": simj,
        "siteParameters": sitej["SiteParameters"]
    }

    env["params"] = cpp
    env["cropRotation"] = cropj["cropRotation"]
    env["events"] = simj["output"]["events"]

    env["pathToClimateCSV"] = simj["climate.csv"]
    env["csvViaHeaderOptions"] = simj["climate.csv-options"]

    climate_csv_string = crop_site_sim["climate"] if "climate" in crop_site_sim else ""
    if climate_csv_string:
        add_climate_data_to_env(env, simj, climate_csv_string)

    return env


def add_climate_data_to_env(env, simj, climate_csv_string=""):
    "add climate data separately to env"

    if not climate_csv_string:
        with open(simj["climate.csv"]) as _:
            climate_csv_string = _.read()

    if climate_csv_string:
        env["climateData"] = json.loads(monica_python.readClimateDataFromCSVStringViaHeadersToJsonString(climate_csv_string, json.dumps(simj["climate.csv-options"])))

    return env
