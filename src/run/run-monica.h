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
  inline std::string pathSeparator()
  {
    return
    #ifdef __unix__
        "/";
#else
        "\\";
#endif
  }
	
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
		
    Env(CentralParameterProvider cpp);

		Env(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		bool returnObjOutputs() const { return outputs["obj-outputs?"].bool_value(); }

    //Interface method for python wrapping. Simply returns number
    //of possible simulation steps according to avaible climate data.
    std::size_t numberOfPossibleSteps() const { return climateData.noOfStepsPossible(); }

    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

    //! object holding the climate data
    Climate::DataAccessor climateData;
		std::vector<std::string> pathsToClimateCSV;
		json11::Json csvViaHeaderOptions;

    //! vector of elements holding the data of the single crops in the rotation
    std::vector<CultivationMethod> cropRotation;
		std::vector<CropRotation> cropRotations;

		json11::Json events;
		json11::Json outputs;
		
		//! id identifiying this particular run
	  json11::Json customId;

		//! shared id between runs belonging together
		std::string sharedId;

    CentralParameterProvider params;

    std::string toString() const;

    std::string berestRequestAddress;

    //zeromq reply socket for datastream controlled MONICA run
    std::string inputDatastreamAddress;
    std::string inputDatastreamProtocol;
//    std::string inputDatastreamHost;
    std::string inputDatastreamPort;

    //zeromq publish socket for outputs of MONICA
    std::string outputDatastreamAddress;
    std::string outputDatastreamProtocol;
//    std::string outputDatastreamHost;
    std::string outputDatastreamPort;

		bool debugMode{false};
  };

  //------------------------------------------------------------------------------------------

	struct Spec : public Tools::Json11Serializable
	{
		struct DMY
		{
			Tools::Maybe<uint> day;
			Tools::Maybe<uint> month;
			Tools::Maybe<uint> year;
		};

		enum EventType { eDate, eCrop, eExpression, eUnset };

		Spec() {}

		Spec(json11::Json j) { merge(j); }

		virtual Tools::Errors merge(json11::Json j);

		void init(Tools::Maybe<DMY>& member, json11::Json j, std::string time);

		virtual json11::Json to_json() const;

		json11::Json origSpec;

		EventType eventType{eUnset};

		std::map<std::string, std::string> time2event;
		std::map<std::string, std::function<bool(const MonicaModel&)>> time2expression;

		Tools::Maybe<DMY> start;
		Tools::Maybe<DMY> end;

		Tools::Maybe<DMY> from;
		Tools::Maybe<DMY> to;

		Tools::Maybe<DMY> at;

		bool isYearRange() const { return from.value().year.isValue() && to.value().year.isValue(); }
		bool isMonthRange() const { return from.value().month.isValue() && to.value().month.isValue(); }
		bool isDayRange() const { return from.value().day.isValue() && to.value().day.isValue(); }

		bool isAt() const { return at.isValue() && (at.value().year.isValue() || at.value().month.isValue() || at.value().day.isValue()); }
		bool isRange() const { return !isAt(); }
	};

	struct StoreData
	{
		void aggregateResults();
		void aggregateResultsObj();
		void storeResultsIfSpecApplies(const MonicaModel& monica);
		void storeResultsIfSpecAppliesObj(const MonicaModel& monica);

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
