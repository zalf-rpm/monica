# pragma pylint: disable=mixed-indentation

import time
import json
import types
import sys
#sys.path.append('')	 # path to monica_python.pyd or monica_python.so
#sys.path.append('') # path to soil_conversion.py
from soil_conversion import *
import monica_python

#print "sys.path: ", sys.path
#print "sys.version: ", sys.version


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


def oidIsOrgan(oid):
	return oid["organ"] != ORGAN_UNDEFINED_ORGAN_


def oidIsRange(oid):
	return oid["fromLayer"] >= 0 \
		and oid["toLayer"] >= 0 \
		and oid["fromLayer"] < oid["toLayer"] 


def opToString(op):
	return { 
		  OP_AVG: "AVG"
		, OP_MEDIAN: "MEDIAN"
		, OP_SUM: "SUM"
		, OP_MIN: "MIN"
		, OP_MAX: "MAX"
		, OP_FIRST: "FIRST"
		, OP_LAST: "LAST"
		, OP_NONE: "NONE"
		, OP_UNDEFINED_OP_: "undef"
		}.get(op, "undef")


def organToString(organ):
	return {
		  ORGAN_ROOT: "Root"
		, ORGAN_LEAF: "Leaf"
		, ORGAN_SHOOT: "Shoot"
		, ORGAN_FRUIT: "Fruit"
		, ORGAN_STRUCT: "Struct"
		, ORGAN_SUGAR: "Sugar"
		, ORGAN_UNDEFINED_ORGAN_: "undef"
		}.get(organ, "undef")


def oidToString(oid, includeTimeAgg):
	oss = ""
	oss += "["
	oss += oid["name"]
	if oidIsOrgan(oid):
		oss += ", " + organToString(oid["organ"])
	elif oidIsRange(oid):
		oss += ", [" + str(oid["fromLayer"] + 1) + ", " + str(oid["toLayer"] + 1) \
		+ (", " + opToString(oid["layerAggOp"]) if oid["layerAggOp"] != OP_NONE else "") \
		+ "]"
	elif oid["fromLayer"] >= 0:
		oss += ", " + str(oid["fromLayer"] + 1)
	if includeTimeAgg:
		oss += ", " + opToString(oid["timeAggOp"])
	oss += "]"

	return oss


def writeOutputHeaderRows(outputIds, includeHeaderRow = True, includeUnitsRow = True, includeTimeAgg = False):
	row1 = []
	row2 = []
	row3 = []
	row4 = []
	for oid in outputIds:
		fromLayer = oid["fromLayer"]
		toLayer = oid["toLayer"]
		isOrgan = oidIsOrgan(oid)
		isRange = oidIsRange(oid) and oid["layerAggOp"] == OP_NONE
		if isOrgan:
			toLayer = fromLayer = oid["organ"] # organ is being represented just by the value of fromLayer currently
		elif isRange:
			fromLayer += 1
			toLayer += 1     # display 1-indexed layer numbers to users
		else:
			toLayer = fromLayer # for aggregated ranges, which aren't being displayed as range

		for i in range(fromLayer, toLayer+1):
			str1 = ""
			if isOrgan:
				str1 += oid["name"] + "/" + organToString(oid["organ"])
			elif isRange:
				str1 += oid["name"] + "_" + str(i)
			else:
				str1 += oid["name"]
			row1.append(str1)
			row4.append("j:" + oid["jsonInput"].replace("\"", ""))
			row3.append("m:" + oidToString(oid, includeTimeAgg))
			row2.append("[" + oid["unit"] + "]")

	out = []
	if includeHeaderRow:
		out.append(row1)
	if includeUnitsRow:
		out.append(row4)
	if includeTimeAgg:
		out.append(row3)
		out.append(row2)
	
	return out


def writeOutput(outputIds, values):
	out = []
	if len(values) > 0:
		for k in range(0, len(values[0])):
			i = 0
			row = []
			for oid in outputIds:
				j = values[i][k]
				if type(j) == types.ListType:
					for jv in j:
						row.append(jv)
				else:
					row.append(j)
				i += 1
			out.append(row)
	return out


def isAbsolutePath(p):
	return p.startswith("/") or p.startswith("\\")


def fixSystemSeparator(p):
	return p


def defaultValue(d, key, default):
	return d[key] if key in d else default


def readAndParseJsonFile(path):
	with open(path) as f:
		return {"result": json.load(f), "errors": [], "success": True}
	return {"result": {}, "errors": ["Error opening file with path : '" + path + "'!"], "success": False}


def parseJsonString(jsonString):
	return {"result": json.loads(jsonString), "errors": [], "success": True}


def isStringType(j):
	return type(j) == types.StringType or type(j) == types.UnicodeType


def findAndReplaceReferences(root, j):
	sp = supportedPatterns()

	success = True
	errors = []

	if type(j) == types.ListType and len(j) > 0:
	
		arr = []
		arrayIsReferenceFunction = False

		if isStringType(j[0]):
			if j[0] in sp:
				f = sp[j[0]]
				arrayIsReferenceFunction = True

				#check for nested function invocations in the arguments
				funcArr = []
				for i in j: 
					r = findAndReplaceReferences(root, i)
					success = success and r["success"]
					if not r["success"]:
						for e in r["errors"]:
							errors.append(e)
					funcArr.append(r["result"])
				
				#invoke function
				jaes = f(root, funcArr)

				success = success and jaes["success"]
				if not jaes["success"]:
					for e in jaes["errors"]:
						errors.append(e)

				#if successful try to recurse into result for functions in result
				if jaes["success"]:
					r = findAndReplaceReferences(root, jaes["result"])
					success = success and r["success"]
					if not r["success"]:
						for e in r["errors"]:
							errors.append(e)
					return {"result": r["result"], "errors": errors, "success": len(errors) == 0}
				else:
					return {"result": {}, "errors": errors, "success": len(errors) == 0}

		if not arrayIsReferenceFunction:
			for jv in j:
				r = findAndReplaceReferences(root, jv)
				success = success and r["success"]
				if not r["success"]:
					for e in r["errors"]:
						errors.append(e)
				arr.append(r["result"])

		return {"result": arr, "errors": errors, "success": len(errors) == 0}

	elif type(j) == types.DictType:
		obj = {}

		for k, v in j.iteritems():
			r = findAndReplaceReferences(root, v)
			success = success and r["success"]
			if not r["success"]:
				for e in r["errors"]:
					errors.append(e)
			obj[k] = r["result"]

		return {"result": obj, "errors": errors, "success": len(errors) == 0}

	return {"result": j, "errors": errors, "success": len(errors) == 0}


def supportedPatterns():

	def ref(root, j):
		if "cache" not in ref.__dict__: 
			ref.cache = {} 

		if len(j) == 3 \
		 and isStringType(j[1]) \
		 and isStringType(j[2]):

			key1 = j[1]
			key2 = j[2]

			if (key1, key2) in ref.cache:
				return ref.cache[(key1, key2)]
			
			res = findAndReplaceReferences(root, root[key1][key2])
			ref.cache[(key1, key2)] = res
			return res
		
		return {"result": j, "errors": ["Couldn't resolve reference: " + json.dumps(j) + "!"], "success" : False}
	
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

	def fromFile(root, j):
		error = ""

		if len(j) == 2 and isStringType(j[1]):

			basePath = defaultValue(root, "include-file-base-path", ".")
			pathToFile = j[1]
			pathToFile = fixSystemSeparator(pathToFile 
																			if isAbsolutePath(pathToFile) 
																			else basePath + "/" + pathToFile)
			jo = readAndParseJsonFile(pathToFile)
			if jo["success"] and not type(jo["result"]) == None:
				return {"result": jo["result"], "errors": [], "success": True}
			
			return {"result": j, "errors": ["Couldn't include file with path: '" + pathToFile + "'!"], "success": False}

		return {"result": j, "errors": ["Couldn't include file with function: " + json.dumps(j) + "!"], "success": False}

	def humus2corg(r, j):
		if len(j) == 2 \
			and type(j[1]) == types.IntType:
			return {"result": humus_st2corg(j[1]), "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't convert humus level to corg: " + json.dumps(j) + "!"], "success": False}

	def ld2trd(r, j):
		if len(j) == 3 \
			and type(j[1]) == types.IntType \
			and type(j[2]) == types.FloatType:
			return {"result": ld_eff2trd(j[1], j[2]), "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't convert bulk density class to raw density using function: " + json.dumps(j) + "!"], "success": False}

	def KA52clay(r, j):
		if len(j) == 2 and isStringType(j[1]):
			return {"result": KA5texture2clay(j[1]), "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't get soil clay content from KA5 soil class: " + json.dumps(j) + "!"], "success": False}

	def KA52sand(r, j):
		if len(j) == 2 and isStringType(j[1]):
			return {"result": KA5texture2sand(j[1]), "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't get soil sand content from KA5 soil class: " + json.dumps(j) + "!"], "success": False}

	def sandClay2lambda(r, j):
		if len(j) == 3 \
			and type(j[1]) == types.FloatType \
			and type(j[2]) == types.FloatType:
			return {"result": sandAndClay2lambda(j[1], j[2]), "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't get lambda value from soil sand and clay content: " + json.dumps(j) + "!"], "success": False}

	def percent(r, j):
		if len(j) == 2 and type(j[1]) == types.FloatType:
			return {"result": j[1] / 100.0, "errors": [], "success": True}
		return {"result": j, "errors": ["Couldn't convert percent to decimal percent value: " + json.dumps(j) + "!"], "success": False}

	if "m" not in supportedPatterns.__dict__: 
		supportedPatterns.m = \
			{
			#"include-from-db": fromDb
			  "include-from-file": fromFile
			, "ref": ref
			, "humus_st2corg": humus2corg
			, "humus-class->corg": humus2corg
			, "ld_eff2trd": ld2trd
			, "bulk-density-class->raw-density": ld2trd
			, "KA5TextureClass2clay": KA52clay
			, "KA5-texture-class->clay": KA52clay
			, "KA5TextureClass2sand": KA52sand
			, "KA5-texture-class->sand": KA52sand
			, "sandAndClay2lambda": sandClay2lambda
			, "sand-and-clay->lambda": sandClay2lambda
			, "%": percent
			}

	return supportedPatterns.m


def printPossibleErrors(es, includeWarnings = False):
	if not es["success"]:
		for e in es["errors"]:
			print e

	if includeWarnings and "warnings" in es:
		for w in es["warnings"]:
			print w

	return es["success"]


def parseOutputIds(oids):
	j = json.dumps(oids)
	rs = monica_python.parseOutputIdsToJsonString(j)
	p = json.loads(rs, "latin-1")
	return p


def createEnvJsonFromJsonConfig(cropSiteSim):
	for k, j in cropSiteSim.iteritems():
		if j == None:
			return None

	pathToParameters = cropSiteSim["sim"]["include-file-base-path"]

	def addBasePath(j, basePath):
		if not "include-file-base-path" in j:
			j["include-file-base-path"] = pathToParameters

	cropSiteSim2 = {}
	#collect all errors in all files and don't stop as early as possible
	errors = set()
	for k, j in cropSiteSim.iteritems():
		if k == "climate":
			continue

		addBasePath(j, pathToParameters)
		r = findAndReplaceReferences(j, j)
		if r["success"]:
			cropSiteSim2[k] = r["result"]
		else:
			errors.update(r["errors"])

	if len(errors) > 0:
		for e in errors:
			print e
		return None

	cropj = cropSiteSim2["crop"]
	sitej = cropSiteSim2["site"]
	simj = cropSiteSim2["sim"]

	env = {}
	env["type"] = "Env"

	#store debug mode in env, take from sim.json, but prefer params map
	env["debugMode"] = simj["debug?"]

	cpp = {
		  "type": "CentralParameterProvider" 
		, "userCropParameters": cropj["CropParameters"]
		, "userEnvironmentParameters": sitej["EnvironmentParameters"]
		, "userSoilMoistureParameters": sitej["SoilMoistureParameters"]
		, "userSoilTemperatureParameters": sitej["SoilTemperatureParameters"]
		, "userSoilTransportParameters": sitej["SoilTransportParameters"]
		, "userSoilOrganicParameters": sitej["SoilOrganicParameters"]
		, "simulationParameters": simj
		, "siteParameters": sitej["SiteParameters"]
	}

	env["params"] = cpp
	env["cropRotation"] = cropj["cropRotation"]
	env["dailyOutputIds"] = parseOutputIds(simj["output"].get("daily", []))
	env["monthlyOutputIds"] = parseOutputIds(simj["output"].get("monthly", []))
	env["yearlyOutputIds"] = parseOutputIds(simj["output"].get("yearly", []))
	env["cropOutputIds"] = parseOutputIds(simj["output"].get("crop", []))
	if type(simj["output"].get("at", [])) == types.DictType:
		aoids = {}
		for k, v in simj["output"].get("at", []).iteritems():
			aoids[k] = parseOutputIds(v)
		env["atOutputIds"] = aoids
	env["runOutputIds"] = parseOutputIds(simj["output"].get("run", []))


	#get no of climate file header lines from sim.json, but prefer from params map
	climateDataSettings = simj["climate.csv-options"]

	options = {}
	options["separator"] = defaultValue(climateDataSettings, "csv-separator", ",")
	options["noOfHeaderLines"] = defaultValue(climateDataSettings, "no-of-climate-file-header-lines", 2)
	options["headerName2ACDName"] = climateDataSettings["header-to-acd-names"]
	options["startDate"] = simj["start-date"]
	options["endDate"] = simj["end-date"]
	options["useLeapYears"] = simj["use-leap-years"]
	#print "startDate:", options["startDate"], "endDate:", options["endDate"]

	if "climate" in cropSiteSim:
		climateCSVString = cropSiteSim["climate"]
	else:
		with open(simj["climate.csv"]) as f:
			climateCSVString = f.read()

	if climateCSVString:
		env["da"] = json.loads(monica_python.readClimateDataFromCSVStringViaHeadersToJsonString(climateCSVString, json.dumps(options)))

	return env

def addClimateDataToEnv(env, simj, climateCSVString = ""):
	#get no of climate file header lines from sim.json, but prefer from params map
	climateDataSettings = simj["climate.csv-options"]

	options = {}
	options["separator"] = defaultValue(climateDataSettings, "csv-separator", ",")
	options["noOfHeaderLines"] = defaultValue(climateDataSettings, "no-of-climate-file-header-lines", 2)
	options["headerName2ACDName"] = climateDataSettings["header-to-acd-names"]
	options["startDate"] = simj["start-date"]
	options["endDate"] = simj["end-date"]
	options["useLeapYears"] = simj["use-leap-years"]
	#print "startDate:", options["startDate"], "endDate:", options["endDate"]

	if not climateCSVString:
		with open(simj["climate.csv"]) as f:
			climateCSVString = f.read()

	if climateCSVString:
		env["da"] = json.loads(monica_python.readClimateDataFromCSVStringViaHeadersToJsonString(climateCSVString, json.dumps(options)))

	return env
