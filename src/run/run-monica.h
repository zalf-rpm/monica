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

  struct Env
  {
    Env() {}

    Env(CentralParameterProvider cpp);

    //Interface method for python wrapping. Simply returns number
    //of possible simulation steps according to avaible climate data.
    std::size_t numberOfPossibleSteps() const { return da.noOfStepsPossible(); }

    void addOrReplaceClimateData(std::string, const std::vector<double>& data);

    //! object holding the climate data
    Climate::DataAccessor da;

    //! vector of elements holding the data of the single crops in the rotation
    std::vector<CultivationMethod> cropRotation;

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

  private:
    //! Variable for differentiate betweend execution modes of MONICA
    int mode{MODE_LC_DSS};
  };

  //------------------------------------------------------------------------------------------

	/*!
	* @brief structure holding all results of one monica run
	*/
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

		std::string toString();
	};

	//----------------------------------------------------------------------------

  std::pair<Tools::Date, std::map<Climate::ACD, double>>
  climateDataForStep(const Climate::DataAccessor& da, std::size_t stepNo);

  /*!
   * main function for running monica under a given Env(ironment)
   * @param env the environment completely defining what the model needs and gets
   * @return a structure with all the Monica results
   */
#ifndef	MONICA_GUI
  Result runMonica(Env env);
#else
  Result runMonica(Env env, Configuration* cfg = NULL);
#endif

	void initializeFoutHeader(std::ofstream&);
	void initializeGoutHeader(std::ofstream&);
	void writeCropResults(const CropGrowth*, std::ofstream&, std::ofstream&, bool);
	void writeGeneralResults(std::ofstream &fout, std::ofstream &gout, Env &env,
		MonicaModel &monica, int d);
	void dumpMonicaParametersIntoFile(std::string, CentralParameterProvider &cpp);

}

#endif
