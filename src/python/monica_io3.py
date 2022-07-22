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

import json
import os
from pathlib import Path
import sys
import time

PATH_TO_SOIL_IO3 = Path(os.path.realpath(__file__)).parent.parent.parent.parent / "util/soil"

if str(PATH_TO_SOIL_IO3) not in sys.path:
    sys.path.insert(1, str(PATH_TO_SOIL_IO3))
import soil_io3

#------------------------------------------------------------------------------

CACHE_REFS = False

OP_AVG = 0
OP_MEDIAN = 1
OP_SUM = 2
OP_MIN = 3
OP_MAX = 4
OP_FIRST = 5
OP_LAST = 6
OP_NONE = 7
OP_UNDEFINED_OP_ = 8


ORGAN_ROOT = 0
ORGAN_LEAF = 1
ORGAN_SHOOT = 2
ORGAN_FRUIT = 3
ORGAN_STRUCT = 4
ORGAN_SUGAR = 5
ORGAN_UNDEFINED_ORGAN_ = 6

#------------------------------------------------------------------------------

def oid_is_organ(oid):
    return oid["organ"] != ORGAN_UNDEFINED_ORGAN_

#------------------------------------------------------------------------------

def oid_is_range(oid):
    return oid["fromLayer"] >= 0 \
        and oid["toLayer"] >= 0 #\
        #and oid["fromLayer"] < oid["toLayer"]

#------------------------------------------------------------------------------

def op_to_string(op):
    return {
        OP_AVG: "AVG",
        OP_MEDIAN: "MEDIAN",
        OP_SUM: "SUM",
        OP_MIN: "MIN",
        OP_MAX: "MAX",
        OP_FIRST: "FIRST",
        OP_LAST: "LAST",
        OP_NONE: "NONE",
        OP_UNDEFINED_OP_: "undef"
    }.get(op, "undef")

#------------------------------------------------------------------------------

def organ_to_string(organ):
    return {
        ORGAN_ROOT: "Root",
        ORGAN_LEAF: "Leaf",
        ORGAN_SHOOT: "Shoot",
        ORGAN_FRUIT: "Fruit",
        ORGAN_STRUCT: "Struct",
        ORGAN_SUGAR: "Sugar",
        ORGAN_UNDEFINED_ORGAN_: "undef"
    }.get(organ, "undef")

#------------------------------------------------------------------------------

def oid_to_string(oid, include_time_agg):
    oss = ""
    oss += "["
    oss += oid["name"]
    if oid_is_organ(oid):
        oss += ", " + organ_to_string(oid["organ"])
    elif oid_is_range(oid):
        oss += ", [" + str(oid["fromLayer"] + 1) + ", " + str(oid["toLayer"] + 1) \
        + (", " + op_to_string(oid["layerAggOp"]) if oid["layerAggOp"] != OP_NONE else "") \
        + "]"
    elif oid["fromLayer"] >= 0:
        oss += ", " + str(oid["fromLayer"] + 1)
    if include_time_agg:
        oss += ", " + op_to_string(oid["timeAggOp"])
    oss += "]"

    return oss

#------------------------------------------------------------------------------

def write_output_header_rows(output_ids,
                             include_header_row=True,
                             include_units_row=True,
                             include_time_agg=False):
    "write header rows"
    row1 = []
    row2 = []
    row3 = []
    row4 = []
    for oid in output_ids:
        from_layer = oid["fromLayer"]
        to_layer = oid["toLayer"]
        is_organ = oid_is_organ(oid)
        is_range = oid_is_range(oid) and oid["layerAggOp"] == OP_NONE
        if is_organ:
            # organ is being represented just by the value of fromLayer currently
            to_layer = from_layer = oid["organ"]
        elif is_range:
            from_layer += 1
            to_layer += 1     # display 1-indexed layer numbers to users
        else:
            to_layer = from_layer # for aggregated ranges, which aren't being displayed as range

        for i in range(from_layer, to_layer+1):
            str1 = ""
            if is_organ:
                str1 += (oid["name"] + "/" + organ_to_string(oid["organ"])) if len(oid["displayName"]) == 0 else oid["displayName"]
            elif is_range:
                str1 += (oid["name"] + "_" + str(i)) if len(oid["displayName"]) == 0 else oid["displayName"]
            else:
                str1 += oid["name"] if len(oid["displayName"]) == 0 else oid["displayName"]
            row1.append(str1)
            row4.append("j:" + oid["jsonInput"].replace("\"", ""))
            row3.append("m:" + oid_to_string(oid, include_time_agg))
            row2.append("[" + oid["unit"] + "]")

    out = []
    if include_header_row:
        out.append(row1)
    if include_units_row:
        out.append(row4)
    if include_time_agg:
        out.append(row3)
        out.append(row2)

    return out

#------------------------------------------------------------------------------

def write_output(output_ids, values, round_ids={}):
    "write actual output lines"
    out = []
    if len(values) > 0:
        for k in range(0, len(values[0])):
            i = 0
            row = []
            for oid in output_ids:
                oid_name = oid["displayName"] if len(oid["displayName"]) > 0 else oid["name"]
                j__ = values[i][k]
                if isinstance(j__, list):
                    for jv_ in j__:
                        row.append(round(jv_, round_ids[oid_name]) if oid_name in round_ids else jv_)
                else:
                    row.append(round(j__, round_ids[oid_name]) if oid_name in round_ids else j__)
                i += 1
            out.append(row)
    return out

#------------------------------------------------------------------------------

def write_output_obj(output_ids, values, round_ids={}):
    "write actual output lines"
    out = []
    for obj in values:
        row = []
        for oid in output_ids:
            oid_name = oid["displayName"] if len(oid["displayName"]) > 0 else oid["name"]
            j__ = obj.get(oid_name, "")
            if isinstance(j__, list):
                for jv_ in j__:
                    row.append(round(jv_, round_ids[oid_name]) if oid_name in round_ids else jv_)
            else:
                row.append(round(j__, round_ids[oid_name]) if oid_name in round_ids else j__)
        out.append(row)
    return out

#------------------------------------------------------------------------------

def is_absolute_path(p):
    "is absolute path"
    return p.startswith("/") \
        or (len(p) == 2 and p[1] == ":") \
        or (len(p) > 2 and p[1] == ":" \
            and (p[2] == "\\" \
                or p[2] == "/"))

#------------------------------------------------------------------------------

def fix_system_separator(path):
    "fix system separator"
    path = path.replace("\\", "/")
    new_path = path
    while True:
        new_path = path.replace("//", "/")
        if new_path == path:
            break
        path = new_path
    return new_path

#------------------------------------------------------------------------------

def replace_env_vars(path):
    "replace ${ENV_VAR} in path"
    start_token = "${"
    end_token = "}"
    start_pos = path.find(start_token)
    while start_pos > -1:
        end_pos = path.find(end_token, start_pos + 1)
        if end_pos > -1:
            name_start = start_pos + 2
            env_var_name = path[name_start : end_pos]
            env_var_content = os.environ.get(env_var_name, None)
            if env_var_content:
                path = path.replace(path[start_pos : end_pos + 1], env_var_content)
                start_pos = path.find(start_token)
            else:
                start_pos = path.find(start_token, end_pos + 1)
        else:
            break

    return path

#------------------------------------------------------------------------------

def default_value(dic, key, default):
    "return default value if key not there"
    return dic[key] if key in dic else default

#------------------------------------------------------------------------------

def read_and_parse_json_file(path):
    with open(path) as f:
        return {"result": json.load(f), "errors": [], "success": True}
    return {"result": {},
            "errors": ["Error opening file with path : '" + path + "'!"],
            "success": False}

#------------------------------------------------------------------------------

def parse_json_string(jsonString):
    return {"result": json.loads(jsonString), "errors": [], "success": True}

#------------------------------------------------------------------------------

def is_string_type(j):
    return isinstance(j, str)

#------------------------------------------------------------------------------

def find_and_replace_references(root, j):
    sp = supported_patterns()

    success = True
    errors = []

    if isinstance(j, list) and len(j) > 0:

        arr = []
        array_is_reference_function = False

        if is_string_type(j[0]):
            if j[0] in sp:
                f = sp[j[0]]
                array_is_reference_function = True

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

        return {"result": arr, "errors": errors, "success": len(errors) == 0}

    elif isinstance(j, dict):
        obj = {}

        for k, v in j.items():
            r = find_and_replace_references(root, v)
            success = success and r["success"]
            if not r["success"]:
                for e in r["errors"]:
                    errors.append(e)
            obj[k] = r["result"]

        return {"result": obj, "errors": errors, "success": len(errors) == 0}

    return {"result": j, "errors": errors, "success": len(errors) == 0}

#------------------------------------------------------------------------------

def supported_patterns():

    def ref(root, j):
        if CACHE_REFS and "cache" not in ref.__dict__:
            ref.cache = {}

        if len(j) == 3 \
         and is_string_type(j[1]) \
         and is_string_type(j[2]):

            key1 = j[1]
            key2 = j[2]

            if CACHE_REFS and (key1, key2) in ref.cache:
                return ref.cache[(key1, key2)]

            res = find_and_replace_references(root, root[key1][key2])
            
            if CACHE_REFS:
                ref.cache[(key1, key2)] = res
            return res

        return {"result": j,
                "errors": ["Couldn't resolve reference: " + json.dumps(j) + "!"],
                "success" : False}

    '''
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
                vector<Json> spjs;
                int profileId = isParamMap ? j[1]["id"].int_value() : j[2].int_value();
                auto sps = Soil::soilParameters(db, profileId);
                for(auto sp : *sps)
                    spjs.push_back(sp.to_json());

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
    '''

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
            if jo_["success"] and not isinstance(jo_["result"], type(None)):
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
            and isinstance(j__[1], int):
            return {"result": soil_io3.humus_class_to_corg(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't convert humus level to corg: " + json.dumps(j__) + "!"],
                "success": False}

    def ld_to_trd(_, j__):
        if len(j__) == 3 \
            and isinstance(j__[1], int) \
            and isinstance(j__[2], float):
            return {"result": soil_io3.bulk_density_class_to_raw_density(j__[1], j__[2]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't convert bulk density class to raw density using function: " + json.dumps(j__) + "!"],
                "success": False}

    def ka5_to_clay(_, j__):
        if len(j__) == 2 and is_string_type(j__[1]):
            return {"result": soil_io3.ka5_texture_to_clay(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get soil clay content from KA5 soil class: " + json.dumps(j__) + "!"],
                "success": False}

    def ka5_to_sand(_, j__):
        if len(j__) == 2 and is_string_type(j__[1]):
            return {"result": soil_io3.ka5_texture_to_sand(j__[1]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get soil sand content from KA5 soil class: " + json.dumps(j__) + "!"],
                "success": False}

    def sand_clay_to_lambda(_, j__):
        if len(j__) == 3 \
            and isinstance(j__[1], float) \
            and isinstance(j__[2], float):
            return {"result": soil_io3.sand_and_clay_to_lambda(j__[1], j__[2]), "errors": [], "success": True}
        return {"result": j__,
                "errors": ["Couldn't get lambda value from soil sand and clay content: " + json.dumps(j__) + "!"],
                "success": False}

    def percent(_, j__):
        if len(j__) == 2 and isinstance(j__[1], float):
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

#------------------------------------------------------------------------------

def print_possible_errors(errs, include_warnings=False):
    if not errs["success"]:
        for err in errs["errors"]:
            print(err)

    if include_warnings and "warnings" in errs:
        for war in errs["warnings"]:
            print(war)

    return errs["success"]

#------------------------------------------------------------------------------

def create_env_json_from_json_config(crop_site_sim):
    "create the json version of the env from the json config files"
    for k, j in crop_site_sim.items():
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
    for k, j in crop_site_sim.items():
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
            print(err)
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
    env["cropRotation"] = cropj.get("cropRotation", None)
    env["cropRotations"] = cropj.get("cropRotations", None)
    env["events"] = simj["output"]["events"]

    env["pathToClimateCSV"] = simj["climate.csv"]
    env["csvViaHeaderOptions"] = simj["climate.csv-options"]
    env["csvViaHeaderOptions"]["latitude"] = sitej["SiteParameters"]["Latitude"]

    env["climateCSV"] = crop_site_sim["climate"] if "climate" in crop_site_sim else ""

    return env
    