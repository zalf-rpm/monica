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

#include "../core/monica.h"
#include "cultivation-method.h"

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

	struct OId : public Tools::Json11Serializable
	{
		enum OP { AVG, SUM, NONE };
		
		OId() {}

		OId(int id) : id(id) {}
		
		OId(int id, OP op) : id(id), op(op), from(0), to(20) {}
		
		OId(int id, OP op, int from, int to) : id(id), op(op), from(from), to(to) {}

		OId(json11::Json object);

		virtual void merge(json11::Json j);

		virtual json11::Json to_json() const;
		
		int id{-1};
		OP op{NONE};
		int from{-2}, to{-1};
	};
		
	//---------------------------------------------------------------------------

  struct Env : public Tools::Json11Serializable
  {
    Env() {}

    Env(CentralParameterProvider cpp);

		Env(json11::Json object);

		virtual void merge(json11::Json j);

		virtual json11::Json to_json() const;

    //Interface method for python wrapping. Simply returns number
    //of possible simulation steps according to avaible climate data.
    std::size_t numberOfPossibleSteps() const { return da.noOfStepsPossible(); }

    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

    //! object holding the climate data
    Climate::DataAccessor da;

    //! vector of elements holding the data of the single crops in the rotation
    std::vector<CultivationMethod> cropRotation;

		std::vector<OId> outputIds;

    int customId{-1};

    CentralParameterProvider params;

    std::string toString() const;

    //Set execution mode of Monica.
    //Disables debug outputs for some modes.
    void setMode(int m){ mode = m; }

    int getMode() const { return mode; }

    std::string berestRequestAddress;
//    std::string berestRequestProtocol;
    //    std::string berestRequestHost;
//    std::string berestRequestPort;

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

		std::shared_ptr<std::ostream> fout;
		std::shared_ptr<std::ostream> gout;

  private:
    //! Variable for differentiate betweend execution modes of MONICA
    int mode{MODE_LC_DSS};
  };

  //------------------------------------------------------------------------------------------

	struct Output
	{
		std::map<int, std::vector<std::string>> strings;
		std::map<int, std::vector<double>> doubles;
		std::map<int, std::vector<int>> ints;
		std::map<int, std::pair<int, int>> ranges;
	};

	//! structure holding all results of one monica run
	class Result
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

  std::pair<Tools::Date, std::map<Climate::ACD, double>>
  climateDataForStep(const Climate::DataAccessor& da, std::size_t stepNo);

  //! main function for running monica under a given Env(ironment)
	//! @param env the environment completely defining what the model needs and gets
	//! @return a structure with all the Monica results
  Result runMonica(Env env);

	Result runMonica_(Env env);

	/*
	struct OV
	{
		enum { INT, DOUBLE, STRING, DOUBLE_RANGE } tag;
		union
		{
			int i;
			double d;
			std::string s;
			std::tuple<double, int, int> dr;
		};
	};

	typedef std::map<int, std::vector<OV>> Output;
	//*/

	
	void initializeFoutHeader(std::ostream&);
	void initializeGoutHeader(std::ostream&);
	void writeCropResults(const CropGrowth*, 
												std::ostream&, 
												std::ostream&, 
												bool);
	void storeCropResults(const std::vector<OId>& outputIds,
												Output& res,
												const CropGrowth* mcg,
												bool cropPlanted);

	void writeGeneralResults(std::ostream& fout, 
													 std::ostream& gout, 
													 Env& env,
													 MonicaModel& monica, 
													 int d);
	void storeGeneralResults(const std::vector<OId>& outputIds,
													 Output& res,
													 Env& env,
													 MonicaModel& monica,
													 int d);
	void dumpMonicaParametersIntoFile(std::string, CentralParameterProvider& cpp);
}

#endif
