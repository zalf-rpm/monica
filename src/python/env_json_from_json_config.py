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
			, "ld_eff2trd": ld2trd
			, "KA5TextureClass2clay": KA52clay
			, "KA5TextureClass2sand": KA52sand
			, "sandAndClay2lambda": sandClay2lambda
			, "%": percent
			}

	return supportedPatterns.m;


def printPossibleErrors(es, includeWarnings = False):
	if not es["success"]:
		for e in es["errors"]:
			print e

	if includeWarnings and "warnings" in es:
		for w in es["warnings"]:
			print w

	return es["success"]


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
		, "userSoilMoistureParameters": sitej["SoilOrganicParameters"]
		, "userSoilTemperatureParameters": sitej["SoilTemperatureParameters"]
		, "userSoilTransportParameters": sitej["SoilTransportParameters"]
		, "userSoilOrganicParameters": sitej["SoilMoistureParameters"]
		, "simulationParameters": simj
		, "siteParameters": sitej["SiteParameters"]
	}

	def parseOutputIds(oids):
		j = json.dumps(oids)
		rs = monica_python.parseOutputIdsToJsonString(j)
		p = json.loads(rs, "latin-1")
		return p

	env["params"] = cpp
	env["cropRotation"] = cropj["cropRotation"]
	env["dailyOutputIds"] = parseOutputIds(simj["output"]["daily"])
	env["monthlyOutputIds"] = parseOutputIds(simj["output"]["monthly"])
	env["yearlyOutputIds"] = parseOutputIds(simj["output"]["yearly"])
	env["cropOutputIds"] = parseOutputIds(simj["output"]["crop"])
	if type(simj["output"]["at"]) == types.DictType:
		aoids = {}
		for k, v in simj["output"]["at"].iteritems():
			aoids[k] = parseOutputIds(v)
		env["atOutputIds"] = aoids
	env["runOutputIds"] = parseOutputIds(simj["output"]["run"])

	#get no of climate file header lines from sim.json, but prefer from params map
	climateDataSettings = simj["climate.csv-options"]

	options = {}
	options["separator"] = defaultValue(climateDataSettings, "csv-separator", ",")
	options["noOfHeaderLines"] = defaultValue(climateDataSettings, "no-of-climate-file-header-lines", 2)
	options["headerName2ACDName"] = climateDataSettings["header-to-acd-names"]
	options["startDate"] = simj["start-date"]
	options["endDate"] = simj["end-date"]
	options["useLeapYears"] = simj["use-leap-years"]
	print "startDate:", options["startDate"], "endDate:", options["endDate"]

	with open(simj["climate.csv"]) as f:
		climateCSVString = f.read()

	env["da"] = json.loads(monica_python.readClimateDataFromCSVStringViaHeadersToJsonString(climateCSVString, json.dumps(options)))

	return env
