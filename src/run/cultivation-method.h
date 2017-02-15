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
#include "tools/json11-helper.h"
#include "soil/soil.h"
#include "../core/monica-parameters.h"
#include "../core/crop.h"
#include "../io/output.h"

namespace Monica
{
  class MonicaModel;

  class DLL_API WorkStep : public Tools::Json11Serializable
	{
	public:
    WorkStep(){}

    WorkStep(const Tools::Date& d);

    WorkStep(json11::Json object);

    virtual WorkStep* clone() const = 0;

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "WorkStep"; }

		virtual Tools::Date date() const { return _date; }

		virtual Tools::Date earliestDate() const { return Tools::Date(); }

		virtual Tools::Date latestDate() const { return Tools::Date(); }

    virtual void setDate(Tools::Date date) {_date = date; }

		//! do whatever the workstep has to do
		//! returns true if workstep is finished (dynamic worksteps might need to be applied again)
		virtual bool apply(MonicaModel* model);

		//! tell if this workstep is active and can be used 
		//! a workstep might temporarily be deactivated, eg a dynamic sowing workstep
		//! which has to be checked for sowing every day, but not anymore after sowing
		virtual bool isActive() const { return true; }

		//! reset potential state of workstep
		virtual void reset() {}

	protected:
		Tools::Date _date;
	};

  typedef std::shared_ptr<WorkStep> WSPtr;

	//----------------------------------------------------------------------------

	class DLL_API Seed : public WorkStep
	{
	public:
    Seed(){}

    Seed(const Tools::Date& at, CropPtr crop);

    Seed(json11::Json object);

    virtual Seed* clone() const {return new Seed(*this); }

    virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    virtual std::string type() const { return "Seed"; }

    virtual bool apply(MonicaModel* model);

    virtual void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedAndHarvestDate(date, _crop->harvestDate());
    }

    CropPtr crop() const { return _crop; }

  private:
    CropPtr _crop;
	};

	//---------------------------------------------------------------------------

	class DLL_API AutomaticSowing : public WorkStep
	{
	public:
		AutomaticSowing() {}

		AutomaticSowing(const Tools::Date& at, CropPtr crop);

		AutomaticSowing(json11::Json object);

		virtual AutomaticSowing* clone() const { return new AutomaticSowing(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

		json11::Json to_json(bool includeFullCropParameters) const;

		virtual std::string type() const { return "AutomaticSowing"; }

		virtual bool apply(MonicaModel* model);

		virtual void setDate(Tools::Date date)
		{
			this->_date = date;
			_crop->setSeedAndHarvestDate(date, _crop->harvestDate());
		}

		CropPtr crop() const { return _crop; }

		virtual bool isActive() const { return !_cropSeeded; }

		virtual void reset() 
		{ 
			_cropSeeded = _inSowingRange = false; 
			setDate(Tools::Date());
		}

		virtual Tools::Date earliestDate() const { return _earliestDate; }

		virtual Tools::Date latestDate() const { return _latestDate; }

	private:
		Tools::Date _earliestDate;
		Tools::Date _latestDate;
		double _minTempThreshold{0};
		int _daysInTempWindow{0};
		double _minPercentASW{0};
		double _maxPercentASW{100};
		double _max3dayPrecipSum{0};
		double _maxCurrentDayPrecipSum{0};
		double _tempSumAboveBaseTemp{0};
		double _baseTemp{0};
		CropPtr _crop;
		bool _inSowingRange{false};
		bool _cropSeeded{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API Harvest : public WorkStep
	{
	public:
		Harvest();

    Harvest(const Tools::Date& at,
            CropPtr crop,
            std::string method = "total");

    Harvest(json11::Json j);

    virtual Harvest* clone() const { return new Harvest(*this); }

    virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    virtual std::string type() const { return "Harvest"; }

    virtual bool apply(MonicaModel* model);

    virtual void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedAndHarvestDate(_crop->seedDate(), date);
    }

    void setPercentage(double percentage) { _percentage = percentage; }

    void setExported(bool exported)	{ _exported = exported; }

		CropPtr crop() const { return _crop; }
		void setCrop(CropPtr c) { _crop = c; }

  private:
    CropPtr _crop;
    std::string _method;
    double _percentage{0};
    bool _exported{true};
	};

  //----------------------------------------------------------------------------

	class DLL_API AutomaticHarvesting : public Harvest
	{
	public:
		AutomaticHarvesting();

		AutomaticHarvesting(CropPtr crop,
												std::string harvestTime,
												Tools::Date latestHarvest,
												std::string method = "total");

		AutomaticHarvesting(json11::Json object);

		virtual AutomaticHarvesting* clone() const { return new AutomaticHarvesting(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

		json11::Json to_json(bool includeFullCropParameters) const;

		virtual std::string type() const { return "AutomaticHarvesting"; }

		virtual bool apply(MonicaModel* model);

		virtual bool isActive() const { return !_cropHarvested; }

		virtual void reset() 
		{ 
			_cropHarvested = false; 
			setDate(Tools::Date());
		}

		virtual Tools::Date latestDate() const { return _latestDate; }

	private:
		std::string _harvestTime; //!< Harvest time parameter
		Tools::Date _latestDate;
		double _minPercentASW{0};
		double _maxPercentASW{100};
		double _max3dayPrecipSum{0};
		double _maxCurrentDayPrecipSum{0};
		bool _cropHarvested{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API Cutting : public WorkStep
	{
	public:
    Cutting(const Tools::Date& at);

    Cutting(json11::Json object);

    virtual Cutting* clone() const {return new Cutting(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "Cutting"; }

		virtual bool apply(MonicaModel* model);
  };

	//----------------------------------------------------------------------------

	class DLL_API MineralFertiliserApplication : public WorkStep
	{
	public:
		MineralFertiliserApplication() {}

		MineralFertiliserApplication(const Tools::Date& at,
																 MineralFertiliserParameters partition,
                                 double amount);

    MineralFertiliserApplication(json11::Json object);

    virtual MineralFertiliserApplication* clone() const {return new MineralFertiliserApplication(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "MineralFertiliserApplication"; }

    virtual bool apply(MonicaModel* model);

    MineralFertiliserParameters partition() const { return _partition; }

		double amount() const { return _amount; }

	private:
		MineralFertiliserParameters _partition;
		double _amount;
	};

  //----------------------------------------------------------------------------

	class DLL_API NDemandApplication : public WorkStep
	{
	public:
		NDemandApplication() {}

		NDemandApplication(int stage,
											 double depth,
											 MineralFertiliserParameters partition,
											 double Ndemand);

		NDemandApplication(Tools::Date date,
											 double depth,
											 MineralFertiliserParameters partition,
											 double Ndemand);

		NDemandApplication(json11::Json object);

		virtual NDemandApplication* clone() const { return new NDemandApplication(*this); }

		virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const;

		virtual std::string type() const { return "NDemandApplication"; }

		virtual bool apply(MonicaModel* model);

		MineralFertiliserParameters partition() const { return _partition; }

		virtual bool isActive() const { return !_appliedFertilizer; }

		virtual void reset() 
		{ 
			_appliedFertilizer = false; 
			setDate(Tools::Date());
		}

	private:
		MineralFertiliserParameters _partition;
		double _Ndemand{0};
		double _depth{0.0};
		int _stage{0};
		bool _appliedFertilizer{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API OrganicFertiliserApplication : public WorkStep
	{
	public:
		OrganicFertiliserApplication(const Tools::Date& at,
                                 const OrganicMatterParametersPtr params,
																 double amount,
                                 bool incorp = true);

    OrganicFertiliserApplication(json11::Json j);

    virtual OrganicFertiliserApplication* clone() const {return new OrganicFertiliserApplication(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "OrganicFertiliserApplication"; }

    virtual bool apply(MonicaModel* model);

    //! Returns parameter for organic fertilizer
    OrganicMatterParametersPtr parameters() const { return _params; }

		//! Returns fertilization amount
		double amount() const { return _amount; }

		//! Returns TRUE, if fertilizer is applied with incorporation
		bool incorporation() const { return _incorporation; }

  private:
    OrganicMatterParametersPtr _params;
    double _amount{0.0};
    bool _incorporation{false};
	};

	//----------------------------------------------------------------------------

	class DLL_API TillageApplication : public WorkStep
	{
	public:
    TillageApplication(const Tools::Date& at, double depth);

    TillageApplication(json11::Json object);

    virtual TillageApplication* clone() const {return new TillageApplication(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "TillageApplication"; }

    virtual bool apply(MonicaModel* model);

		double depth() const { return _depth; }

	private:
    double _depth{0.3};
	};

	//----------------------------------------------------------------------------

	class DLL_API SetValue : public WorkStep
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

	class DLL_API IrrigationApplication : public WorkStep
	{
	public:
    IrrigationApplication(const Tools::Date& at, double amount,
                          IrrigationParameters params = IrrigationParameters());

    IrrigationApplication(json11::Json object);

    virtual IrrigationApplication* clone() const {return new IrrigationApplication(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "IrrigationApplication"; }

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

    //! is semantically the equivalent to creating an empty CM and adding Seed, Harvest and Cutting applications
    CultivationMethod(CropPtr crop, const std::string& name = std::string());

    CultivationMethod(json11::Json object);

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

		template<class Application>
		void addApplication(const Application& a)
		{
      _worksteps.insert(std::make_pair(a.date(), std::make_shared<Application>(a)));
			updateDynamicWorkstepCount();
    }

    bool apply(const Tools::Date& date, MonicaModel* model) const;

		bool apply(MonicaModel* model);

		Tools::Date nextDate(const Tools::Date & date) const;

		std::vector<WSPtr> workstepsAt(const Tools::Date& date) const;

		std::vector<WSPtr> staticWorksteps() const;

		std::vector<WSPtr> dynamicWorksteps(bool justUnfinishedWorksteps = false) const;

		std::vector<WSPtr> unfinishedDynamicWorksteps() const { return dynamicWorksteps(true); }

		bool allDynamicWorkstepsFinished() const { return _unfinishedDynamicWorkstepsCount == 0; }

		std::string name() const { return _name; }

		CropPtr crop() const { return _crop; }

    bool isFallow() const { return !_crop || !_crop->isValid();  }

		//! when does the PV start
    Tools::Date startDate() const;

		//! when does the whole PV end
    Tools::Date endDate() const;

    const std::multimap<Tools::Date, WSPtr>& getWorksteps() const { return _worksteps; }

    void clearWorksteps() { _worksteps.clear(); }

		std::string toString() const;

		//the custom id is used to keep a potentially usage defined
    //mapping to an entity from another domain,
    //e.g. a Carbiocial CropActivity which the CultivationMethod was based on
		void setCustomId(int cid) { _customId = cid; }
		int customId() const { return _customId; }

    void setIrrigateCrop(bool irr){ _irrigateCrop = irr; }
    bool irrigateCrop() const { return _irrigateCrop; }

		//! reset cultivation method to initial state, if it will be reused (eg in a crop rotation)
		void reset();

		void updateDynamicWorkstepCount() { _unfinishedDynamicWorkstepsCount = unfinishedDynamicWorksteps().size(); }

	private:
		std::multimap<Tools::Date, WSPtr> _worksteps;
    int _customId{0};
		std::string _name;
		CropPtr _crop;
    bool _irrigateCrop{false};
		size_t _unfinishedDynamicWorkstepsCount{0};
	};
	
	template<>
	DLL_API inline void CultivationMethod::addApplication<Seed>(const Seed& s)
  {
    _worksteps.insert(std::make_pair(s.date(), std::make_shared<Seed>(s)));
    _crop = s.crop();
		updateDynamicWorkstepCount();
  }

	template<>
	DLL_API inline void CultivationMethod::addApplication<AutomaticSowing>(const AutomaticSowing& s)
	{
		_worksteps.insert(std::make_pair(s.date(), std::make_shared<AutomaticSowing>(s)));
		_crop = s.crop();
		updateDynamicWorkstepCount();
	}
	
  //template<>
	//DLL_API inline void CultivationMethod::addApplication<Harvest>(const Harvest& h)
  //{
  //  _worksteps.insert(std::make_pair(h.date(), std::make_shared<Harvest>(h)));
  //}
	

}

#endif
