/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Jan Vaillant <jan.vaillant@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <string>

#include "tools/debug.h"
#include "cson/cson_amalgamation_core.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "soil/soil.h"

namespace Monica
{
class Result;
class CultivationMethod;
class CentralParameterProvider;

/**

  parse input, create Env, run monica and return result

*/
class Configuration
{
  public:
    Configuration(const std::string& outPath, const std::string& dirNameMet, const std::string& preMetFiles, const std::string& dbIniName = "db.ini");
    ~Configuration();

    /* set and validate configuration. If it fails all cson_values will be freed */
    bool setJSON(const std::string& sim, const std::string& site, const std::string& crop);
    bool setJSON(FILE* sim, FILE* site, FILE* crop);
    bool setJSON(cson_value* sim, cson_value* site, cson_value* crop);

    /* build environment and start simulation */
    const Result run();

    /* pointer to meta.xxx json */
    static const cson_value* metaSim;
    static const cson_value* metaSite;
    static const cson_value* metaCrop;

    /* progress during monica loop */
    virtual void setProgress(double progress);

    /* 
      static functions
      use e.g. in GUI without Config. instance  
    */

    /* init cson_value from string or file */
    static int readJSON(const std::string &str, cson_value** val);
    static int readJSON(FILE* file, cson_value** val);

    /* write to file */
    static int writeJSON(FILE* file, const cson_value* val);

    /* write to std:cout */
    static int printJSON(const cson_value* val, std::string *str = NULL);

    /* compare meta and project json files */
    static bool isValid(const cson_value* val, const cson_value* meta, const std::string& path = "root");

    /* simplify cson calls: return value if value != JSON null otherwise return def (default) */
    static bool getBool(const cson_object* obj, const std::string& path, bool def = false);
    static int getInt(const cson_object* obj, const std::string& path, int def = 0);
    static double getDbl(const cson_object* obj, const std::string& path, double def = 0.0);
    static std::string getStr(const cson_object* obj, const std::string& path, std::string def = std::string());
		static Tools::Date getIsoDate(const cson_object* obj, const std::string& path, Tools::Date def = Tools::Date(true));
    static bool isNull(const cson_object* obj, const std::string& path);

    /* print formated cson error to std:cerr */
    static void printCsonError(int rc, const cson_parse_info &info);

  private:
    /* check if all JSON is conform with meta description */
    bool isValidated();

    /* create layers from horizons */
    bool createLayers(std::vector<Soil::SoilParameters> &layers, cson_array* horizonsArr, double layerThicknessCm, int maxNoOfLayers);

    /* create production processes */
    bool createProcesses(std::vector<CultivationMethod> &pps, cson_array* cropsArr);
		bool addHarvestOps(CultivationMethod &pp, cson_array* harvArr);
		bool addTillageOps(CultivationMethod &pp, cson_array* tillArr);
    bool addFertilizers(CultivationMethod &pp, cson_array* fertArr, bool isOrganic = false);
    bool addIrrigations(CultivationMethod &pp, cson_array* irriArr);

    /* create climate data */
    bool createClimate(Climate::DataAccessor &da, CentralParameterProvider &cpp, double latitude, bool useLeapYears = true);

    /* free cson root objects*/
    void freeCson();

    cson_value* _sim;
    cson_value* _site;
    cson_value* _crop;

    std::string _outPath;
    std::string _dirNameMet;
    std::string _preMetFiles;
};

}  /* namespace Monica */

#endif /* CONFIGURATION_H_ */
