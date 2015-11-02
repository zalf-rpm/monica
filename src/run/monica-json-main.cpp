/**
Authors: 
Jan Vaillant <jan.vaillant@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <string>

#include "db/abstract-db-connections.h"

#include "tools/debug.h"
#include "../io/configuration.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-typedefs.h"
#include "../core/monica.h"
#include "tools/json11-helper.h"

using namespace std;
using namespace Monica;
using namespace json11;
using namespace Tools;


Json readAndParseFile(string path)
{
	Json j;

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

const map<string, function<Json(const Json&, Json)>>& supportedPatterns()
{
	auto ref = [](const Json& root, Json j)
	{ 
		if(j.array_items().size() == 3 
			 && j[1].is_string() 
			 && j[2].is_string())
			return root[j[1].string_value()][j[2].string_value()];
		return j; 
	};

	auto fromDb = [](const Json&, Json j)
	{
		if(j.array_items().size() >= 3 
			 && j[1].is_string()
			 && j[2].is_string())
		{
			auto type = j[1].string_value();
			if(type == "mineral_fertiliser")
				getMineralFertiliserParametersFromMonicaDB(j[2].string_value());
			else if(type == "organic_fertiliser")
				getOrganicFertiliserParametersFromMonicaDB(j[2].string_value());
			else if(type == "crop_residue"
							&& j.array_items().size() == 4
							&& j[3].is_string())
				getResidueParametersFromMonicaDB(j[2].string_value(), j[3].string_value());
			else if(type == "species")
				getCropParametersFromMonicaDB(0);
		}

		return j; 
	};

	auto fromFile = [](const Json&, Json j)
	{ 
		if(j.array_items().size() == 2 
			 && j[1].is_string())
			return readAndParseFile(j[1].string_value());
		return j; 
	};
		
	static map<string, function<Json(const Json&,Json)>> m{
	{"include-from-db", fromDb},
	{"include-from-file", fromFile},
	{"ref", ref}};
	return m;
}


void findAndReplaceReferences(const Json& root, Json& j)
{
	auto sp = supportedPatterns();
	if(j.is_array())
	{
		if(j[0].is_string())
		{
			auto p = sp.find(j[0].string_value());
			if(p != sp.end())
				j = (p->second)(root, j);
		}
		else
			for(auto jv : j.array_items())
				findAndReplaceReferences(root, jv);
	}
	else if(j.is_object())
		for(auto p : j.object_items())
			findAndReplaceReferences(root, p.second);
}

void parseAndRunMonica(const string& pathToInputFiles)
{
	CentralParameterProvider cpp = readUserParameterFromDatabase(MODE_HERMES);

	auto parse = [](string path)
	{
		Json j;

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
	};

	vector<Json> cropSiteSim;
	for(auto p : {"crop.json", "site.json", "sim.json"})
		cropSiteSim.push_back(parse(pathToInputFiles + "/" + p));






	Env env;






	Result r = runMonica(env);

}


#include "soil/soil.h"
int main(int argc, char** argv)
{
	auto res = Soil::fcSatPwpFromKA5textureClass("fS",
																							 0,
																							 1.5*1000.0,
																							 0.8/100.0);




	//writeCropParameters("crop-parameters");
	//writeMineralFertilisers("mineral-fertilisers");
	//writeOrganicFertilisers("organic-fertilisers");
	writeCropResidues("residues");
	//writeUserParameters(MODE_HERMES, "user-parameters");
	//writeUserParameters(MODE_EVA2, "user-parameters");
	//writeUserParameters(MODE_MACSUR_SCALING, "user-parameters");

	return 0;
}









/* embedded meta.xxx.json.o */
// extern char _binary_meta_sim_json_start;
// extern char _binary_meta_site_json_start;
// extern char _binary_meta_crop_json_start;

static int freeMetaCsonAndReturn(int x)
{
  if (Monica::Configuration::metaSim)
    cson_value_free(const_cast<cson_value*>(Monica::Configuration::metaSim));
  if (Monica::Configuration::metaSite)
    cson_value_free(const_cast<cson_value*>(Monica::Configuration::metaSite));
  if (Monica::Configuration::metaCrop)
    cson_value_free(const_cast<cson_value*>(Monica::Configuration::metaCrop));

  return x;
}

static int initMetaCson()
{

  std::cerr << "initMetaCson" << std::endl;

  /* open and parse meta.xxx files */
  int rc = 0;
	FILE* metaSimFile = fopen((string("meta.json") + pathSeparator() + "meta.sim.json").c_str(), "r" );
  if (!metaSimFile) {
    std::cerr << "Error opening meta.sim.json file [meta.json/meta.sim.json]!" << std::endl;
    return 3;
  }
  else {
    rc = Monica::Configuration::readJSON(metaSimFile, const_cast<cson_value**>(&Monica::Configuration::metaSim));
    fclose(metaSimFile);
    if (rc != 0) {
      std::cerr << "Error parsing meta.sim.json file [meta.json/meta.sim.json]!" << std::endl;
      return freeMetaCsonAndReturn(3);
    }    
  }

	FILE* metaSiteFile = fopen((string("meta.json") + pathSeparator() + "meta.site.json").c_str(), "r");
  if (!metaSiteFile) {
    std::cerr << "Error opening meta.site.json file [meta.json/meta.site.json]!" << std::endl;
    return 3;
  }
  else {
    rc = Monica::Configuration::readJSON(metaSiteFile, const_cast<cson_value**>(&Monica::Configuration::metaSite));
    fclose(metaSiteFile);
    if (rc != 0) {
      std::cerr << "Error parsing meta.site.json file [meta.json/meta.site.json]!" << std::endl;
      return freeMetaCsonAndReturn(3);
    }    
  }

	FILE* metaCropFile = fopen((string("meta.json") + pathSeparator() + "meta.crop.json").c_str(), "r");
  if (!metaCropFile) {
    std::cerr << "Error opening meta.crop.json file [meta.json/meta.crop.json]!" << std::endl;
    return 3;
  }
  else {
    rc = Monica::Configuration::readJSON(metaCropFile, const_cast<cson_value**>(&Monica::Configuration::metaCrop));
    fclose(metaCropFile);
    if (rc != 0) {
      std::cerr << "Error parsing meta.crop.json file [meta.json/meta.crop.json]!" << std::endl;
      return freeMetaCsonAndReturn(3);
    }    
  }

  return rc;
}

#ifndef MONICA_GUI
static void showHelp()
{
  printf("Usage:\n\t./%s [-?|--help] [options] [-p project_name] [-d json_dir] [-i db_ini_file] [-w weather_dir] [-m prefix_weather] [-o out_dir]\n", 
          "monica");
  putchar('\n');
  puts("\t-p\tprefix of required files:"); 
  putchar('\n');
  puts("\t\tproject_name.sim.json  (simulation settings)"); 
  puts("\t\tproject_name.site.json (site specific parameters)"); 
  puts("\t\tproject_name.crop.json (crops & rotation)"); 
  putchar('\n');
  puts("\t-d\tpath where json files reside");
  putchar('\n');
  puts("\t-i\tname of db ini file");
  putchar('\n');
  puts("\t-w\tpath where weather files reside");
  putchar('\n');
  puts("\t-m\tprefix of weather files");
  putchar('\n');
  puts("\t-o\toutput path");
  putchar('\n');
  puts("\toptions:");
  putchar('\n');
  puts("\tdebug\tshow extra debug output");
  putchar('\n');
}
#endif

#if false
/**
 * Main routine of stand alone model.
 * @param argc Number of program's arguments
 * @param argv Pointer of program's arguments
 */
int main(int argc, char** argv)
{
  /* embedded meta.xxx.json.o */
  // std::string metaSim = &_binary_meta_sim_json_start;
  /* test meta data */
  // int rc = 0;
  // cson_parse_info info = cson_parse_info_empty;
  // cson_value* metaSimVal;
  // rc = cson_parse_string(&metaSimVal, metaSim.c_str(), strlen(metaSim.c_str()), NULL, &info);
  // if (rc != 0) {
  //   printf("JSON parse error in meta.sim.json, code=%d (%s), at line %u, column %u.\n",
  //           info.errorCode, cson_rc_string(rc), info.line, info.col );
  //   return 1;
  // }
  // cson_value_free(metaSimVal);
  // cson_value_free(metaSimVal);
  Tools::activateDebug = false;

#ifdef MONICA_GUI
  /* MONICA_GUI part */
#else
  FILE* simFile = NULL;
  FILE* siteFile = NULL;
  FILE* cropFile = NULL;
  char const* projectName = NULL;
  char const* dirNameJSON = NULL;
  char const* dbIniName = NULL;
  char const* dirNameMet = NULL;
  char const* preMetFiles = NULL;
  char const* outPath = NULL;
  int i = 1;
  setlocale( LC_ALL, "C" ) /* supposedly important for underlying JSON parser. */;
  for( ; i < argc; ++i ) {
    char const * arg = argv[i];
    // std::cout << "arg " << arg << std::endl;
    if( 0 == strcmp("-p",arg) ) {
      ++i;
      if( i < argc ) {
        projectName = argv[i];
        std::cout << "projectName: " << projectName << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("-d",arg) ) {
      ++i;
      if( i < argc ) {
        dirNameJSON = argv[i];
        std::cout << "dirNameJSON: " << dirNameJSON << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("-i",arg) ) {
      ++i;
      if( i < argc ) {
        dbIniName = argv[i];
        std::cout << "dbIniName: " << dbIniName << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("-w",arg) ) {
      ++i;
      if( i < argc ) {
        dirNameMet = argv[i];
        std::cout << "dirNameMet: " << dirNameMet << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("-m",arg) ) {
      ++i;
      if( i < argc ) {
        preMetFiles = argv[i];
        std::cout << "preMetFiles: " << preMetFiles << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("-o",arg) ) {
      ++i;
      if( i < argc ) {
        outPath = argv[i];
        std::cout << "outPath: " << outPath << std::endl;
        continue;
      }
      else {
        showHelp();
        return 1;
      }
    }
    else if( 0 == strcmp("debug",arg) ) {
      Tools::activateDebug = true;
      std::cout << "Monica::activateDebug: " << Tools::activateDebug << std::endl;
      continue;
    }
    else if( (0 == strcmp("-?",arg)) || (0 == strcmp("--help",arg) ) ) {
      showHelp();
      return 0;
    }
    else {
      showHelp();
      return 0;
    }
  }
		
  if( dirNameJSON && (0!=strcmp("-",dirNameJSON)) && projectName && (0!=strcmp("-",projectName))) {
		std::string simFilePath = std::string(dirNameJSON) + pathSeparator() + std::string(projectName) + ".sim.json";
    std::cout << "simFilePath: " << simFilePath << std::endl;
    simFile = fopen( simFilePath.c_str(), "r" );
    if (!simFile) {
      std::cerr << "Error opening sim file [" << simFilePath << "]!" << std::endl;
      return 3;
    }

    std::string siteFilePath = std::string(dirNameJSON) + pathSeparator() + std::string(projectName) + ".site.json";
    std::cout << "siteFilePath: " << siteFilePath << std::endl;
    siteFile = fopen( siteFilePath.c_str(), "r" );
    if (!siteFile) {
      std::cerr << "Error opening site file [" << siteFilePath << "]!" << std::endl;
      return 3;
    }
    
		std::string cropFilePath = std::string(dirNameJSON) + pathSeparator() + std::string(projectName) + ".crop.json";
    std::cout << "cropFilePath: " << cropFilePath << std::endl;
    cropFile = fopen( cropFilePath.c_str(), "r" );
    if (!cropFile) {
      std::cerr << "Error opening crop file [" << cropFilePath << "]!" << std::endl;
      return 3;
    }
  }
  else {
    showHelp();
    return 0;    
  }

  if (dbIniName && (0!=strcmp("-",dbIniName))) {
    //if (!boost::filesystem::exists(dbIniName)) {
    //  std::cerr << "Error finding db ini [" << dbIniName << "]!" << std::endl;
    //  return 4;
    //}   
  }
  else {
    showHelp();
    return 0; 
  }

  if( outPath && (0!=strcmp("-",outPath))) {
    //if (!boost::filesystem::exists(outPath)) {
    //  std::cerr << "Error finding output [" << outPath << "]!" << std::endl;
    //  return 4;
    //}   
  }
  else {
    showHelp();
    return 0; 
  }

  int ret = initMetaCson(); 
  if (ret != 0)
    return ret; 

  /* setup config & run monica */
  Monica::Result res;
  Monica::Configuration* cfg = new Monica::Configuration(std::string(outPath), std::string(dirNameMet), std::string(preMetFiles), std::string(dbIniName));
  Tools::debug() << outPath << "\t" << dirNameMet << "\t" << preMetFiles << "\t" << dbIniName << endl;
  std::cout << "Monica::Configuration" << std::endl;
  bool ok = cfg->setJSON(simFile, siteFile, cropFile);  
  std::cout << "Monica::Configuration::setJSON" << std::endl;
  fclose(simFile);
  fclose(siteFile);
  fclose(cropFile);
  std::cout << ok << std::endl;
  if (ok) 
    res = cfg->run();
  
  delete cfg;

  Tools::debug() << "sizeGeneralResults: " << res.sizeGeneralResults() << std::endl;

  if (res.sizeGeneralResults() == 0)
    return freeMetaCsonAndReturn(1);
  else
    return freeMetaCsonAndReturn(0);

#endif
#ifdef MONICA_GUI
  int ret = initMetaCson(); 
  if (ret != 0)
    return ret; 

  QApplication a(argc, argv);
  a.setStyle("Fusion");
  MainWindow w;
  w.show();
  ret = a.exec();
  return freeMetaCsonAndReturn(ret);
#endif
}
#endif
