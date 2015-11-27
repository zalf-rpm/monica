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

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <string>

#include "db/abstract-db-connections.h"

#include "tools/debug.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-typedefs.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "../core/simulation.h"
#include "soil/conversion.h"

using namespace std;
using namespace Monica;
using namespace json11;
using namespace Tools;
using namespace Climate;

Json readAndParseFile(string path)
{
	Json j;
	path = fixSystemSeparator(path);

	ifstream ifs;
	ifs.open(path);
	if(ifs.good())
	{
		string sj;
		for(string line; getline(ifs, line);)
			sj += line;

		string err;
		j = Json::parse(sj, err);
	}
	ifs.close();

	return j;
}

const map<string, function<pair<Json, bool>(const Json&, const Json&)>>& supportedPatterns();

Json findAndReplaceReferences(const Json& root, const Json& j)
{
	auto sp = supportedPatterns();
	
	auto jstr = j.dump();

	if(j.is_array())
	{
		Json::array arr;

		bool arrayIsReferenceFunction = false;
		if(j[0].is_string())
		{
			auto p = sp.find(j[0].string_value());
			if(p != sp.end())
			{
				arrayIsReferenceFunction = true;

				//check for nested function invocations in the arguments
				Json::array funcArr;
				for(auto i : j.array_items())
					funcArr.push_back(findAndReplaceReferences(root, i));

				//invoke function
				auto jAndSuccess = (p->second)(root, funcArr);

				//if successful try to recurse into result for functions in result
				if(jAndSuccess.second)
					return findAndReplaceReferences(root, jAndSuccess.first);
			}
		}

		if(!arrayIsReferenceFunction)
			for(auto jv : j.array_items())
				arr.push_back(findAndReplaceReferences(root, jv));

		return arr;
	}
	else if(j.is_object())
	{
		Json::object obj;

		for(auto p : j.object_items())
			obj[p.first] = findAndReplaceReferences(root, p.second);

		return obj;
	}

	return j;
}

const map<string, function<pair<Json, bool>(const Json&, const Json&)>>& supportedPatterns()
{
	auto ref = [](const Json& root, const Json& j) -> pair<Json, bool>
	{ 
		if(j.array_items().size() == 3
			 && j[1].is_string()
			 && j[2].is_string())
			return make_pair(root[j[1].string_value()][j[2].string_value()], true);
		return make_pair(j, false);
	};

	auto fromDb = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() >= 3 
			 && j[1].is_string()
			 && j[2].is_string())
		{
			auto type = j[1].string_value();
			if(type == "mineral_fertiliser")
				return make_pair(getMineralFertiliserParametersFromMonicaDB(j[2].string_value()).to_json(), 
												 true);
			else if(type == "organic_fertiliser")
				return make_pair(getOrganicFertiliserParametersFromMonicaDB(j[2].string_value())->to_json(), 
												 true);
			else if(type == "crop_residue"
							&& j.array_items().size() == 4
							&& j[3].is_string())
				return make_pair(getResidueParametersFromMonicaDB(j[2].string_value(), 
																													j[3].string_value())->to_json(), 
												 true);
			else if(type == "species")
				return make_pair(getSpeciesParametersFromMonicaDB(j[2].string_value())->to_json(), 
												 true);
			else if(type == "cultivar"
							&& j.array_items().size() == 4
							&& j[3].is_string())
				return make_pair(getCultivarParametersFromMonicaDB(j[2].string_value(), 
																													 j[3].string_value())->to_json(), 
												 true);
			else if(type == "crop"
							&& j.array_items().size() == 4
							&& j[3].is_string())
				return make_pair(getCropParametersFromMonicaDB(j[2].string_value(), 
																											 j[3].string_value())->to_json(), 
												 true);
		}

		return make_pair(j, false);
	};

	auto fromFile = [](const Json&, const Json& j) -> pair<Json, bool>
	{ 
		if(j.array_items().size() == 2 
			 && j[1].is_string())
			return make_pair(readAndParseFile(j[1].string_value()), true);
		return make_pair(j, false); 
	};
		
	auto humus2corg = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
			 && j[1].is_number())
			return make_pair(Soil::humus_st2corg(j[1].int_value()), true);
		return make_pair(j, false);
	};

	auto ld2trd = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 3
			 && j[1].is_number()
			 && j[2].is_number())
			return make_pair(Soil::ld_eff2trd(j[1].int_value(), j[2].number_value()),
											 true);
		return make_pair(j, false);
	};

	auto KA52clay = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
			 && j[1].is_string())
			return make_pair(Soil::KA5texture2clay(j[1].string_value()), true);
		return make_pair(j, false);
	};

	auto KA52sand = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
			 && j[1].is_string())
			return make_pair(Soil::KA5texture2sand(j[1].string_value()), true);
		return make_pair(j, false);
	};

	auto sandClay2lambda = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 3
			 && j[1].is_number()
			 && j[2].is_number())
			return make_pair(Soil::sandAndClay2lambda(j[1].number_value(),
																								j[2].number_value()),
											 true);
		return make_pair(j, false);
	};

	auto percent = [](const Json&, const Json& j) -> pair<Json, bool>
	{
		if(j.array_items().size() == 2
			 && j[1].is_number())
			return make_pair(j[1].number_value() / 100.0, true);
		return make_pair(j, false);
	};

	static map<string, function<pair<Json, bool>(const Json&,const Json&)>> m{
	{"include-from-db", fromDb},
	{"include-from-file", fromFile},
	{"ref", ref},
	{"humus_st2corg", humus2corg},
	{"ld_eff2trd", ld2trd},
	{"KA5TextureClass2clay", KA52clay},
	{"KA5TextureClass2sand", KA52sand},
	{"sandAndClay2lambda", sandClay2lambda},
	{"%", percent}};
	return m;
}

struct PARMParams
{
	string pathToProjectInputFiles;
	string projectName;
	Date startDate, endDate;
};
void parseAndRunMonica(PARMParams ps)
{
	if(!ps.projectName.empty())
		ps.projectName += ".";

	vector<Json> cropSiteSim;
	for(auto p : {"crop.json", "site.json", "sim.json"})
		cropSiteSim.push_back(readAndParseFile(ps.pathToProjectInputFiles + "/"
																					 + ps.projectName + p));

	vector<Json> cropSiteSim2;
	for(auto& j : cropSiteSim)
		cropSiteSim2.push_back(findAndReplaceReferences(j, j));

	for(auto j : cropSiteSim2)
	{
		auto str = j.dump(); 
		str;
	}
	
	auto cropj = cropSiteSim2.at(0);
	auto sitej = cropSiteSim2.at(1);
	auto simj = cropSiteSim2.at(2);

	if(!ps.startDate.isValid())
		set_iso_date_value(ps.startDate, simj, "startDate");
	if(!ps.endDate.isValid())
		set_iso_date_value(ps.endDate, simj, "endDate");

	Env env;
	env.params = readUserParameterFromDatabase(MODE_HERMES);

	env.params.userEnvironmentParameters.merge(sitej["EnvironmentParameters"]);
	env.params.siteParameters.merge(sitej["SiteParameters"]);
	env.params.simulationParameters.merge(simj);

	for(Json cmj : cropj["cropRotation"].array_items())
		env.cropRotation.push_back(cmj);
	
	env.da = readClimateDataFromCSVFileViaHeaders(ps.pathToProjectInputFiles
																								+ "/" + ps.projectName + "climate.csv",
																								",",
																								ps.startDate, 
																								ps.endDate);

	if(!env.da.isValid())
		return;
	
	env.params.writeOutputFiles = true;
	env.params.pathToOutputDir = ps.pathToProjectInputFiles;

	Result r = runMonica(env);
	r;
}

#include "soil/soil.h"
void test()
{
	auto res = Soil::fcSatPwpFromKA5textureClass("fS",
																							 0,
																							 1.5*1000.0,
																							 0.8/100.0);

	//Json j = readAndParseFile("installer/Hohenfinow2/json/test.crop.json");
	//auto j2 = findAndReplaceReferences(j, j);
	//auto str = j2.dump();
}

void writeDbParams()
{
	writeCropParameters("parameters/crops");
	writeMineralFertilisers("parameters/mineral-fertilisers");
	writeOrganicFertilisers("parameters/organic-fertilisers");
	writeCropResidues("parameters/crop-residues");
	writeUserParameters(MODE_HERMES, "parameters/user-parameters");
	writeUserParameters(MODE_EVA2, "parameters/user-parameters");
	writeUserParameters(MODE_MACSUR_SCALING, "parameters/user-parameters");
}

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	if(argc > 1)
	{
		map<string, string> params;
		if((argc - 1) % 2 == 0)
			for(int i = 1; i < argc; i += 2)
				params[toLower(argv[i])] = argv[i + 1];
		else
			return 1;

		activateDebug = stob(params["debug?:"], false);

		if(params["mode:"] == "hermes")
		{
			cout << "starting MONICA with old HERMES input files" << endl;
			Monica::runWithHermesData(fixSystemSeparator(params["path:"]));
			cout << "finished MONICA" << endl;
		}
		else
		{
			PARMParams ps;
			ps.pathToProjectInputFiles = params["path:"];
			ps.projectName = params["project:"];
			ps.startDate = params["start-date:"];
			ps.endDate = params["end-date:"];

			cout << "starting MONICA with JSON input files" << endl;
			parseAndRunMonica(ps);
			cout << "finished MONICA" << endl;
		}
	}
	
	//test();
	
	//writeDbParams();

	return 0;
}
