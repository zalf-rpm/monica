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

#ifndef CULTIVATION_METHOD_H_
#define CULTIVATION_METHOD_H_

#include <map>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <memory>
#include <functional>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "climate/climate-common.h"
#include "tools/date.h"
#include "json11/json11-helper.h"
#include "soil/soil.h"
#include "../core/monica-parameters.h"
#include "../core/crop.h"
#include "../io/output.h"

namespace Monica
{
  class MonicaModel;

  class DLL_API Workstep : public Tools::Json11Serializable
	{
	public:
    Workstep(){}

    Workstep(const Tools::Date& d);

	Workstep(int noOfDaysAfterEvent, const std::string& afterEvent);

    Workstep(json11::Json object);

    virtual Workstep* clone() const = 0;

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "Workstep"; }

		virtual Tools::Date date() const { return _date; }

		virtual Tools::Date absDate() const { return _date.isAbsoluteDate() ? _date : _absDate; }

		virtual Tools::Date earliestDate() const { return date(); }

		virtual Tools::Date absEarliestDate() const { return absDate(); }

		virtual Tools::Date latestDate() const { return date(); }

		virtual Tools::Date absLatestDate() const { return absDate(); }

    virtual void setDate(Tools::Date date) {_date = date; }

		virtual int noOfDaysAfterEvent() const { return _applyNoOfDaysAfterEvent; }

		virtual std::string afterEvent() const { return _afterEvent; }

		//! do whatever the workstep has to do
		//! returns true if workstep is finished (dynamic worksteps might need to be applied again)
		virtual bool apply(MonicaModel* model);

		//! apply only if condition() is met (is used for dynamicWorksteps)
		virtual bool applyWithPossibleCondition(MonicaModel* model);

		virtual bool condition(MonicaModel* model);

		virtual bool isDynamicWorkstep() const { return !_date.isValid(); }

		//! tell if this workstep is active and can be used 
		//! a workstep might temporarily be deactivated, eg a dynamic sowing workstep
		//! which has to be checked for sowing every day, but not anymore after sowing
		virtual bool isActive() const { return _isActive; }

		//! reinit potential state of workstep
		virtual bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false);

		virtual std::function<double(MonicaModel*)> registerDailyFunction(std::function<std::vector<double>&()> getDailyValues) { 
			return std::function<double(MonicaModel*)>(); 
		};

		bool runAtStartOfDay() const { return _runAtStartOfDay; }

	protected:
    Tools::Date _date;
    Tools::Date _absDate;
    int _applyNoOfDaysAfterEvent{ 0 };
    std::string _afterEvent;
    int _daysAfterEventCount{ 0 };
    bool _isActive{ true };
		bool _runAtStartOfDay{ true };
	};

  typedef std::shared_ptr<Workstep> WSPtr;

	//----------------------------------------------------------------------------

	class DLL_API Sowing : public Workstep
	{
	public:
    Sowing(){}

    //Sowing(const Tools::Date& at, Crop crop);

    Sowing(json11::Json object);

		Sowing(const Sowing& other) 
			: _cropToPlant(other._cropToPlant ? kj::heap<Crop>(*other._cropToPlant.get()) : kj::Own<Crop>())
			, _crop(_cropToPlant.get())
			, _plantDensity(other._plantDensity) {}

    virtual Sowing* clone() const { return new Sowing(*this); }

    virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    virtual std::string type() const { return "Sowing"; }

    virtual bool apply(MonicaModel* model);

    virtual void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedDate(date);
    }

    const Crop* crop() const { return _crop; }

		Crop* crop() { return _crop; }

		void setCropForReplanting(kj::Own<Crop> c) { _cropToPlant = kj::mv(c); }

  private:
    kj::Own<Crop> _cropToPlant;
		Crop* _crop{ nullptr };
		int _plantDensity{-1}; //[plants m-2]
	};

	//---------------------------------------------------------------------------

	class DLL_API AutomaticSowing : public Sowing
	{
	public:
		AutomaticSowing() {}

		AutomaticSowing(json11::Json object);

		virtual AutomaticSowing* clone() const { return new AutomaticSowing(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

		json11::Json to_json(bool includeFullCropParameters) const;

		virtual std::string type() const { return "AutomaticSowing"; }

		virtual bool apply(MonicaModel* model);

		virtual bool condition(MonicaModel* model);

		virtual bool isActive() const { return !_cropSeeded; }

		virtual bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false);

		virtual Tools::Date earliestDate() const { return _earliestDate; }

		virtual Tools::Date absEarliestDate() const { return _absEarliestDate; }

		virtual Tools::Date latestDate() const { return _latestDate; }

		virtual Tools::Date absLatestDate() const { return _absLatestDate; }

		virtual std::function<double(MonicaModel*)> registerDailyFunction(std::function<std::vector<double>&()> getDailyValues);
		
	private:
		Tools::Date _absEarliestDate;
		Tools::Date _earliestDate;
		Tools::Date _latestDate;
		Tools::Date _absLatestDate;
		double _minTempThreshold{0};
		int _daysInTempWindow{0};
		double _minPercentASW{0};
		double _maxPercentASW{100};
		double _max3dayPrecipSum{0};
		double _maxCurrentDayPrecipSum{0};
		double _tempSumAboveBaseTemp{0};
		double _baseTemp{0};

		bool _checkForSoilTemperature{ false };
		double _soilDepthForAveraging{ 0.30 }; //= 30 cm
		int _daysInSoilTempWindow{ 0 }; 
		double _sowingIfAboveAvgSoilTemp{ 0 };
		std::function<std::vector<double>&()> _getAvgSoilTemps;
		
		bool _inSowingRange{false};
		bool _cropSeeded{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API Harvest : public Workstep
	{
	public:
		enum CropUsage { greenManure = 0, biomassProduction };

		struct OptCarbonManagementData
		{
			bool optCarbonConservation{false};
			double cropImpactOnHumusBalance{0};
			double maxResidueRecoverFraction{ 1 };
			CropUsage cropUsage{ biomassProduction };
			double residueHeq{0};
			double organicFertilizerHeq{0};
		};
		
	public:
		Harvest() {}

    //Harvest(const Tools::Date& at,
    //        Crop* crop,
    //        std::string method = "total");

		Harvest(json11::Json j) { merge(j); }

    virtual Harvest* clone() const { return new Harvest(*this); }

    virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    virtual std::string type() const { return "Harvest"; }

    virtual bool apply(MonicaModel* model);

    virtual void setDate(Tools::Date date)
    {
      this->_date = date;
      _sowing->crop()->setHarvestDate(date);
    }

    void setPercentage(double percentage) { _percentage = percentage; }

    void setExported(bool exported)	{ _exported = exported; }

		const Crop& crop() const { return *_sowing->crop(); }

		Sowing* sowing() { return _sowing; }

		void setSowing(Sowing* s) { _sowing = s; }

		//void setIsCoverCrop(bool isCoverCrop) { _optCarbMgmtData.isCoverCrop = isCoverCrop; }
	protected:
		Sowing* _sowing{ nullptr };

  private:
    double _percentage{0};
    bool _exported{true};
		OptCarbonManagementData _optCarbMgmtData;
	};

  //----------------------------------------------------------------------------

	class DLL_API AutomaticHarvest : public Harvest
	{
	public:
		AutomaticHarvest();

		//AutomaticHarvest(Crop* crop,
		//										std::string harvestTime,
		//										Tools::Date latestHarvest,
		//										std::string method = "total");

		AutomaticHarvest(json11::Json object);

		virtual AutomaticHarvest* clone() const { return new AutomaticHarvest(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

		json11::Json to_json(bool includeFullCropParameters) const;

		virtual std::string type() const { return "AutomaticHarvest"; }

		virtual bool apply(MonicaModel* model);

		virtual bool condition(MonicaModel* model);

		virtual bool isActive() const { return !_cropHarvested; }

		virtual bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false);

		virtual Tools::Date latestDate() const { return _latestDate; }

		virtual Tools::Date absLatestDate() const { return _absLatestDate; }

	private:
		std::string _harvestTime; //!< Harvest time parameter
		Tools::Date _latestDate;
		Tools::Date _absLatestDate;
		double _minPercentASW{0};
		double _maxPercentASW{100};
		double _max3dayPrecipSum{0};
		double _maxCurrentDayPrecipSum{0};
		bool _cropHarvested{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API Cutting : public Workstep
	{
	public:
    Cutting(const Tools::Date& at);

    Cutting(json11::Json object);

    virtual Cutting* clone() const {return new Cutting(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "Cutting"; }

		virtual bool apply(MonicaModel* model);


	enum CL { cut, left, none };
	enum Unit { percentage, biomass, LAI };
	struct Value {
		double value{ 0.0 };
		Unit unit{ percentage };
		CL cut_or_left{ cut };
	};

	private:
		std::map<int, Value> _organId2cuttingSpec;
		std::map<int, double> _organId2biomAfterCutting;
		std::map<int, double> _organId2exportFraction;
		double _cutMaxAssimilationRateFraction{1.0};
  };

	//----------------------------------------------------------------------------

	class DLL_API MineralFertilization : public Workstep
	{
	public:
		MineralFertilization() {}

		MineralFertilization(const Tools::Date& at,
																 MineralFertilizerParameters partition,
                                 double amount);

    MineralFertilization(json11::Json object);

    virtual MineralFertilization* clone() const {return new MineralFertilization(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "MineralFertilization"; }

    virtual bool apply(MonicaModel* model);

    MineralFertilizerParameters partition() const { return _partition; }

		double amount() const { return _amount; }

	private:
		MineralFertilizerParameters _partition;
		double _amount{ 0.0 };
	};

  //----------------------------------------------------------------------------

	class DLL_API NDemandFertilization : public Workstep
	{
	public:
		NDemandFertilization() {}

		NDemandFertilization(int stage,
												 double depth,
												 MineralFertilizerParameters partition,
												 double Ndemand);

		NDemandFertilization(Tools::Date date,
												 double depth,
												 MineralFertilizerParameters partition,
												 double Ndemand);

		NDemandFertilization(json11::Json object);

		virtual NDemandFertilization* clone() const { return new NDemandFertilization(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		virtual std::string type() const { return "NDemandFertilization"; }

		virtual bool apply(MonicaModel* model);

		virtual bool condition(MonicaModel* model);

		MineralFertilizerParameters partition() const { return _partition; }

		virtual bool isActive() const { return !_appliedFertilizer; }

		virtual bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false);

	private:
		Tools::Date _initialDate;
		MineralFertilizerParameters _partition;
		double _Ndemand{0};
		double _depth{0.0};
		int _stage{1};
		bool _appliedFertilizer{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API OrganicFertilization : public Workstep
	{
	public:
		OrganicFertilization(const Tools::Date& at,
                        const OrganicMatterParameters& params,
												double amount,
                        bool incorp = true);

    OrganicFertilization(json11::Json j);

    virtual OrganicFertilization* clone() const {return new OrganicFertilization(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "OrganicFertilization"; }

    virtual bool apply(MonicaModel* model);

    //! Returns parameter for organic fertilizer
    const OrganicMatterParameters& parameters() const { return _params; }

		//! Returns fertilization amount
		double amount() const { return _amount; }

		//! Returns TRUE, if fertilizer is applied with incorporation
		bool incorporation() const { return _incorporation; }

  private:
    OrganicMatterParameters _params;
    double _amount{0.0};
    bool _incorporation{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API Tillage : public Workstep
	{
	public:
    Tillage(const Tools::Date& at, double depth);

    Tillage(json11::Json object);

    virtual Tillage* clone() const {return new Tillage(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "Tillage"; }

    virtual bool apply(MonicaModel* model);

		double depth() const { return _depth; }

	private:
    double _depth{0.3};
	};

	//----------------------------------------------------------------------------

	class DLL_API SetValue : public Workstep
	{
	public:
		SetValue(const Tools::Date& at, OId oid, json11::Json value);

		SetValue(json11::Json object);

		virtual SetValue* clone() const { return new SetValue(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		virtual std::string type() const { return "SetValue"; }

		virtual bool apply(MonicaModel* model);

		json11::Json value() const { return _value; }

	private:
		OId _oid;
		json11::Json _value;
		std::function<json11::Json(const Monica::MonicaModel*)> _getValue;
	};

	//----------------------------------------------------------------------------

	class DLL_API SaveMonicaState : public Workstep {
	public:
		SaveMonicaState(const Tools::Date& at, std::string pathToSerializedStateFile);

		SaveMonicaState(json11::Json object);

		virtual SaveMonicaState* clone() const { return new SaveMonicaState(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		virtual std::string type() const { return "SaveMonicaState"; }

		virtual bool apply(MonicaModel* model);

		std::string pathToSerializedStateFile() const { return _pathToSerializedStateFile; }

	private:
		std::string _pathToSerializedStateFile;
	};

	//----------------------------------------------------------------------------

	class DLL_API Irrigation : public Workstep
	{
	public:
    Irrigation(const Tools::Date& at, double amount,
                          IrrigationParameters params = IrrigationParameters());

    Irrigation(json11::Json object);

    virtual Irrigation* clone() const {return new Irrigation(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "Irrigation"; }

		virtual bool apply(MonicaModel* model);

		double amount() const { return _amount; }

		double nitrateConcentration() const { return _params.nitrateConcentration; }

		double sulfateConcentration() const { return _params.sulfateConcentration; }

	private:
    double _amount{0};
		IrrigationParameters _params;
	};

	//----------------------------------------------------------------------------

	DLL_API WSPtr makeWorkstep(json11::Json object);

  class DLL_API CultivationMethod
      : public Tools::Json11Serializable
      //, public std::multimap<Tools::Date, WSPtr>
	{
	public:
    CultivationMethod(const std::string& name = std::string("Fallow"));

    //! is semantically the equivalent to creating an empty CM and adding Sowing, Harvest and Cutting applications
    //CultivationMethod(CropPtr crop, const std::string& name = std::string());

    CultivationMethod(json11::Json object);

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

		template<class Application>
		void addApplication(const Application& a)
		{
      //_allWorksteps.insert(std::make_pair(a.date(), std::make_shared<Application>(a)));
			_allWorksteps.push_back(std::make_shared<Application>(a));
    }

    void apply(const Tools::Date& date, MonicaModel* model) const;

		void absApply(const Tools::Date& date, MonicaModel* model) const;

		void apply(MonicaModel* model, bool runOnlyAtStartOfDayWorksteps);

		Tools::Date nextDate(const Tools::Date & date) const;

		Tools::Date nextAbsDate(const Tools::Date & date) const;

		std::vector<WSPtr> workstepsAt(const Tools::Date& date) const;

		std::vector<WSPtr> absWorkstepsAt(const Tools::Date& date) const;

		bool areOnlyAbsoluteWorksteps() const;

		std::vector<WSPtr> staticWorksteps() const;

		std::vector<WSPtr> allDynamicWorksteps() const;

		std::vector<WSPtr> unfinishedDynamicWorksteps() const { return _unfinishedDynamicWorksteps; }

		bool allDynamicWorkstepsFinished() const { return _unfinishedDynamicWorksteps.empty(); }

		std::string name() const { return _name; }

		const Crop& crop() const { return *_crop; }

    bool isFallow() const { return !_crop || !_crop->isValid();  }

		//! when does the PV start
    Tools::Date startDate() const;

		Tools::Date absStartDate(bool includeDynamicWorksteps = true) const;

		Tools::Date absLatestSowingDate() const;

		//! when does the whole PV end
    Tools::Date endDate() const;

		Tools::Date absEndDate() const;

    //const std::multimap<Tools::Date, WSPtr>& getWorksteps() const { return _allWorksteps; }
		const std::vector<WSPtr>& getWorksteps() const { return _allWorksteps; }

    void clearWorksteps() { _allWorksteps.clear(); }

		std::string toString() const;

		//the custom id is used to keep a potentially usage defined
    //mapping to an entity from another domain,
    //e.g. a Carbiocial CropActivity which the CultivationMethod was based on
		void setCustomId(int cid) { _customId = cid; }
		int customId() const { return _customId; }

    void setIrrigateCrop(bool irr){ _irrigateCrop = irr; }
    bool irrigateCrop() const { return _irrigateCrop; }

		//! reinit cultivation method to initial state, if it will be reused (eg in a crop rotation)
		//! returns if it was necessary to add a year to shift relative dates after date
		bool reinit(Tools::Date date, bool forceInitYear = false);

		bool canBeSkipped() const { return _canBeSkipped; }

		bool isCoverCrop() const { return _isCoverCrop; }

		bool repeat() const { return _repeat; }

	private:
		std::vector<WSPtr> _allWorksteps;
		std::vector<WSPtr> _allAbsWorksteps;
		std::vector<WSPtr> _unfinishedDynamicWorksteps;
    int _customId{0};
		std::string _name;
		const Crop* _crop{ nullptr };
    bool _irrigateCrop{false};
		bool _canBeSkipped{false}; //! can this crop be skipped, eg. is a catch or cover crop
		bool _isCoverCrop{false}; //! is like canBeSkipped (and implies it), but different rule for when cultivation methods will be skipped
		bool _repeat{true}; //! if false the cultivation method won't participate in wrapping at the end of the crop rotation
	};
	
	template<>
	DLL_API inline void CultivationMethod::addApplication<Sowing>(const Sowing& s)
  {
    //_allWorksteps.insert(std::make_pair(s.date(), std::make_shared<Sowing>(s)));
		_allWorksteps.push_back(std::make_shared<Sowing>(s));
    _crop = s.crop();
  }

	template<>
	DLL_API inline void CultivationMethod::addApplication<AutomaticSowing>(const AutomaticSowing& s)
	{
		//_allWorksteps.insert(std::make_pair(s.date(), std::make_shared<AutomaticSowing>(s)));
		_allWorksteps.push_back(std::make_shared<AutomaticSowing>(s));
		_crop = s.crop();
	}
	
  //template<>
	//DLL_API inline void CultivationMethod::addApplication<Harvest>(const Harvest& h)
  //{
  //  _allWorksteps.insert(std::make_pair(h.date(), std::make_shared<Harvest>(h)));
  //}
	

}

#endif
