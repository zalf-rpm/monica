/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef RUN_MONICA_H_
#define RUN_MONICA_H_

#include <ostream>
#include <vector>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "../core/monica-model.h"
#include "cultivation-method.h"
#include "climate/climate-common.h"
#include "../io/output.h"

namespace Monica
{
	struct CropRotation : public Tools::Json11Serializable
	{
		CropRotation() {}

		CropRotation(Tools::Date start, Tools::Date end, std::vector<CultivationMethod> cropRotation)
			: start(start), end(end), cropRotation(cropRotation)
		{}

		CropRotation(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		Tools::Date start, end;
		std::vector<CultivationMethod> cropRotation;
	};

	struct DLL_API Env : public Tools::Json11Serializable
  {
    Env() {}
		
    Env(CentralParameterProvider&& cpp);

		Env(json11::Json object);
		// construct env from json object

		virtual Tools::Errors merge(json11::Json j);
		// merge a json file into Env

		virtual json11::Json to_json() const;
		// serialize to json

		bool returnObjOutputs() const { return outputs["obj-outputs?"].bool_value(); }
		// is the output as a list (e.g. days) of an object (holding all the requested data)

    //! object holding the climate data
    Climate::DataAccessor climateData;
		// 1. priority, object holding the climate data

		std::string climateCSV;
		// 2nd priority, if climateData not valid, try to read climate data from csv string

		std::vector<std::string> pathsToClimateCSV;
		// 3. priority, if climateCSV is empty, try to load climate data from vector of file paths
				
		json11::Json csvViaHeaderOptions;
		// the csv options for reading/parsing csv data

    std::vector<CultivationMethod> cropRotation;
		// vector of elements holding the data of the single crops in the rotation

		std::vector<CropRotation> cropRotations;
		// optionally 

		json11::Json events;
		// the events section defining the requested outputs

		json11::Json outputs;
		// the whole outputs section
		
	  json11::Json customId;
		// id identifiying this particular run

		std::string sharedId;
		// shared id between runs belonging together

    CentralParameterProvider params;

    std::string toString() const;

    std::string berestRequestAddress;

    //zeromq reply socket for datastream controlled MONICA run
    //std::string inputDatastreamAddress;
    //std::string inputDatastreamProtocol;
//    std::string inputDatastreamHost;
    //std::string inputDatastreamPort;

    //zeromq publish socket for outputs of MONICA
    //std::string outputDatastreamAddress;
    //std::string outputDatastreamProtocol;
//    std::string outputDatastreamHost;
    //std::string outputDatastreamPort;

		bool debugMode{false};
  };

  //------------------------------------------------------------------------------------------

	struct Spec : public Tools::Json11Serializable
	{
		Spec() {}

		Spec(json11::Json j) { merge(j); }

		virtual Tools::Errors merge(json11::Json j);

		std::function<bool(const MonicaModel&)> createExpressionFunc(json11::Json j);

		virtual json11::Json to_json() const { return origSpec; }

		json11::Json origSpec;

		std::function<bool(const MonicaModel&)> startf;
		std::function<bool(const MonicaModel&)> endf;
		std::function<bool(const MonicaModel&)> fromf;
		std::function<bool(const MonicaModel&)> tof;
		std::function<bool(const MonicaModel&)> atf;
		std::function<bool(const MonicaModel&)> whilef;
	};

	struct StoreData
	{
		void aggregateResults();
		void aggregateResultsObj();
		void storeResultsIfSpecApplies(const MonicaModel& monica, bool storeObjOutputs = false);

		Tools::Maybe<bool> withinEventStartEndRange;
		Tools::Maybe<bool> withinEventFromToRange;
		Spec spec;
		std::vector<OId> outputIds;
		std::vector<Tools::J11Array> intermediateResults;
		std::vector<Tools::J11Array> results;
		std::vector<Tools::J11Object> resultsObj;
	};

	//----------------------------------------------------------------------------

	//! can be called initially to set alternative path for the MONICA dll/so to db-connections.ini
	DLL_API void initPathToDB(const std::string& initialPathToIniFile = "db-connections.ini");

  //std::pair<Tools::Date, std::map<Climate::ACD, double>>
  //climateDataForStep(const Climate::DataAccessor& da, std::size_t stepNo);

  //! main function for running monica under a given Env(ironment)
	//! @param env the environment completely defining what the model needs and gets
	//! @return a structure with all the Monica results
  DLL_API Output runMonica(Env env);
}

#endif
