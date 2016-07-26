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

	struct DLL_API OId : public Tools::Json11Serializable
	{
		enum OP { AVG, MEDIAN, SUM, MIN, MAX, FIRST, LAST, NONE, _UNDEFINED_OP_ };
		
		enum ORGAN { ROOT = 0, LEAF, SHOOT, FRUIT, STRUCT, SUGAR, _UNDEFINED_ORGAN_ };

		OId() {}

		//! just name 
		OId(int id) 
			: id(id) {}
		
		//! id and organ
		OId(int id, ORGAN organ) 
			: id(id), organ(organ) {}

		//! id and layer aggregation
		OId(int id, OP layerAgg) 
			: id(id), layerAggOp(layerAgg), fromLayer(0), toLayer(20) {}

		//! id, layer aggregation and time aggregation, shortcut for aggregating all layers in non daily setting
		OId(int id, OP layerAgg, OP timeAgg) 
			: id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(0), toLayer(20) {}
		
		//! id, layer aggregation of from to (incl) to layers
		OId(int id, int from, int to, OP layerAgg)
			: id(id), layerAggOp(layerAgg), fromLayer(from), toLayer(to) {}

		//! aggregate layers from to (incl) to in a non daily setting
		OId(int id, int from, int to, OP layerAgg, OP timeAgg)
			: id(id), layerAggOp(layerAgg), timeAggOp(timeAgg), fromLayer(from), toLayer(to) {}

		OId(json11::Json object);

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;
		
		bool isRange() const { return fromLayer >= 0 && toLayer >= 0 && fromLayer < toLayer; }

		bool isOrgan() const { return organ != _UNDEFINED_ORGAN_; }

		virtual std::string toString(bool includeTimeAgg = false) const;

		std::string toString(OId::OP op) const;
		std::string toString(OId::ORGAN organ) const;

		int id{-1};
		std::string name;
		std::string unit;
		std::string jsonInput;
		OP layerAggOp{NONE}; //! aggregate values on potentially daily basis (e.g. soil layers)
		OP timeAggOp{AVG}; //! aggregate values in a second time range (e.g. monthly)
		ORGAN organ{_UNDEFINED_ORGAN_};
		int fromLayer{-1}, toLayer{-1};
	};

	//---------------------------------------------------------------------------

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

	struct DLL_API MonicaRefs
	{
		MonicaRefs refresh(int timestep, Tools::Date currentDate)
		{
			MonicaRefs copy(*this);
			copy.timestep = timestep;
			copy.currentDate = currentDate;
			copy.mcg = copy.monica.cropGrowth();
			copy.cropPlanted = copy.monica.isCropPlanted();
			return copy;
		}

		const MonicaModel& monica;
		const SoilTemperature& temp;
		const SoilMoisture& moist;
		const SoilOrganic& org;
		const SoilColumn& soilc;
		const SoilTransport& trans;
		const CropGrowth* mcg;
		bool cropPlanted;
		Climate::DataAccessor da;
		int timestep;
		Tools::Date currentDate;
	};

	struct DLL_API BOTRes
	{
		typedef std::vector<json11::Json> ResultVector;
		std::map<int, std::function<void(MonicaRefs&, ResultVector&, OId)>> ofs;
		std::map<std::string, Result2> name2result;
	};
	DLL_API BOTRes& buildOutputTable();

	struct DLL_API Output
	{
		typedef int Id;
		typedef int Month;
		typedef int Year;
		//typedef std::pair<std::string, std::string> SpeciesAndCultivarId;
		typedef std::string SpeciesAndCultivarId;

		std::vector<Tools::J11Array> daily;
		std::map<Month, std::vector<Tools::J11Array>> monthly;
		std::vector<Tools::J11Array> yearly;
		std::map<Tools::Date, std::vector<Tools::J11Array>> at;
		std::map<SpeciesAndCultivarId, std::vector<Tools::J11Array>> crop;
		std::vector<json11::Json> run;
	};
	
	DLL_API void addOutputToResultMessage(Output& out, Tools::J11Object& msg);
	DLL_API void addResultMessageToOutput(const Tools::J11Object& msg, Output& out);

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
