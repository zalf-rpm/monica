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

		Tools::Date date() const { return _date; }

    virtual void setDate(Tools::Date date) {_date = date; }

		//! do whatever the workstep has to do
		virtual void apply(MonicaModel* model) = 0;

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

    virtual void apply(MonicaModel* model);

    void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedAndHarvestDate(date, _crop->harvestDate());
    }

    CropPtr crop() const { return _crop; }

  private:
    CropPtr _crop;
	};

	//----------------------------------------------------------------------------

	class DLL_API Harvest : public WorkStep
	{
	public:
		Harvest();

    Harvest(const Tools::Date& at,
            CropPtr crop,
            CMResultPtr cropResult,
            std::string method = "total");

    Harvest(json11::Json j);

    virtual Harvest* clone() const { return new Harvest(*this); }

    virtual Tools::Errors merge(json11::Json j);

		virtual json11::Json to_json() const { return to_json(true); }

    json11::Json to_json(bool includeFullCropParameters) const;

    virtual std::string type() const { return "Harvest"; }

    virtual void apply(MonicaModel* model);

    void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedAndHarvestDate(_crop->seedDate(), date);
    }

    void setPercentage(double percentage) { _percentage = percentage; }

    void setExported(bool exported)	{ _exported = exported; }

    CMResultPtr cropResult() const { return _cropResult; }

  private:
    CropPtr _crop;
    CMResultPtr _cropResult;
    std::string _method;
    double _percentage{0};
    bool _exported{true};
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

		virtual void apply(MonicaModel* model);
  };

	//----------------------------------------------------------------------------

	class DLL_API MineralFertiliserApplication : public WorkStep
	{
	public:
		MineralFertiliserApplication(const Tools::Date& at,
																 MineralFertiliserParameters partition,
                                 double amount);

    MineralFertiliserApplication(json11::Json object);

    virtual MineralFertiliserApplication* clone() const {return new MineralFertiliserApplication(*this); }

    virtual Tools::Errors merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual std::string type() const { return "MineralFertiliserApplication"; }

    virtual void apply(MonicaModel* model);

    MineralFertiliserParameters partition() const { return _partition; }

		double amount() const { return _amount; }

	private:
		MineralFertiliserParameters _partition;
		double _amount;
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

    virtual void apply(MonicaModel* model);

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

    virtual void apply(MonicaModel* model);

		double depth() const { return _depth; }

	private:
    double _depth{0.3};
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

		virtual void apply(MonicaModel* model);

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
      , public std::multimap<Tools::Date, WSPtr>
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
      insert(std::make_pair(a.date(), std::make_shared<Application>(a)));
    }
    
//    void addApplication(WSPtr a){ insert(std::make_pair(a->date(), a)); }

    void apply(const Tools::Date& date, MonicaModel* model) const;

		Tools::Date nextDate(const Tools::Date & date) const;

		std::string name() const { return _name; }

		CropPtr crop() const { return _crop; }

    bool isFallow() const { return !_crop || !_crop->isValid();  }

		//! when does the PV start
    Tools::Date startDate() const;

		//! when does the whole PV end
    Tools::Date endDate() const;

    const std::multimap<Tools::Date, WSPtr>& getWorksteps() const { return *this; }

    void clearWorksteps() { clear(); }

		std::string toString() const;

    CMResult cropResult() const { return *(_cropResult.get()); }
    CMResultPtr cropResultPtr() const { return _cropResult; }

		//the custom id is used to keep a potentially usage defined
    //mapping to an entity from another domain,
    //e.g. a Carbiocial CropActivity which the CultivationMethod was based on
		void setCustomId(int cid) { _customId = cid; }
		int customId() const { return _customId; }

    void setIrrigateCrop(bool irr){ _irrigateCrop = irr; }
    bool irrigateCrop() const { return _irrigateCrop; }

	private:
    int _customId{0};
		std::string _name;
		CropPtr _crop;
    bool _irrigateCrop{false};

    //store results of the cultivation method
    CMResultPtr _cropResult;
	};
	
	template<>
	DLL_API inline void CultivationMethod::addApplication<Seed>(const Seed& s)
  {
    insert(std::make_pair(s.date(), std::make_shared<Seed>(s)));
    _crop = s.crop();
  }
	
  template<>
	DLL_API inline void CultivationMethod::addApplication<Harvest>(const Harvest& h)
  {
    insert(std::make_pair(h.date(), std::make_shared<Harvest>(h)));
    _cropResult = h.cropResult();
  }

}

#endif
