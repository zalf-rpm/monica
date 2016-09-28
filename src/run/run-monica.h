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

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "../core/monica.h"
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
	
	struct DLL_API Env : public Tools::Json11Serializable
  {
    Env() {}
		
    Env(CentralParameterProvider cpp);

		Env(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

    //Interface method for python wrapping. Simply returns number
    //of possible simulation steps according to avaible climate data.
    std::size_t numberOfPossibleSteps() const { return da.noOfStepsPossible(); }

    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

    //! object holding the climate data
    Climate::DataAccessor da;
		std::string pathToClimateCSV;
		json11::Json csvViaHeaderOptions;

    //! vector of elements holding the data of the single crops in the rotation
    std::vector<CultivationMethod> cropRotation;

		std::vector<OId> dailyOutputIds;
		std::vector<OId> monthlyOutputIds;
		std::vector<OId> yearlyOutputIds;
		std::vector<OId> runOutputIds;
		std::vector<OId> cropOutputIds;
		std::map<Tools::Date, std::vector<OId>> atOutputIds;

    int customId{-1};

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

	
	//! structure holding all results of one monica run
	class DLL_API Result
	{
	public:
		Result() {}
		~Result() {}

		//! just to keep track of the grid point the calculation is being made for
		Tools::GridPoint gp;
		//is as gp before used to track results when multiple parallel
		//unordered invocations of monica will happen
		int customId;

		//! vector of the result of one crop per year
    std::vector<CMResult> pvrs;

		//! results not regarding a particular crop in a rotation
		typedef std::map<ResultId, std::vector<double>> RId2Vector;
		RId2Vector generalResults;

		std::vector<double> getResultsById(int id);

		int sizeGeneralResults() { return int(generalResults.size()); }

		std::vector<std::string> dates;

		Output out;

		std::string toString();
	};

	//----------------------------------------------------------------------------

  //std::pair<Tools::Date, std::map<Climate::ACD, double>>
  //climateDataForStep(const Climate::DataAccessor& da, std::size_t stepNo);

  //! main function for running monica under a given Env(ironment)
	//! @param env the environment completely defining what the model needs and gets
	//! @return a structure with all the Monica results
  DLL_API Result runMonica(Env env);
}

#endif
