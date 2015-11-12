/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) 2007-2013, Leibniz Centre for Agricultural Landscape Research (ZALF)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

#include "climate/climate-common.h"
#include "tools/date.h"
#include "tools/json11-helper.h"
#include "soil/soil.h"
#include "../core/monica-typedefs.h"
#include "../core/monica-parameters.h"
#include "../core/crop.h"

namespace Monica
{
  class MonicaModel;

  class WorkStep : public Tools::Json11Serializable
	{
	public:
    WorkStep(){}

    WorkStep(const Tools::Date& d);

    WorkStep(json11::Json object);

    virtual WorkStep* clone() const = 0;

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

		Tools::Date date() const { return _date; }

    virtual void setDate(Tools::Date date) {_date = date; }

		//! do whatever the workstep has to do
		virtual void apply(MonicaModel* model) = 0;

	protected:
		Tools::Date _date;
	};

  typedef std::shared_ptr<WorkStep> WSPtr;

	//----------------------------------------------------------------------------

	class Seed : public WorkStep
	{
	public:
    Seed(){}

    Seed(const Tools::Date& at, CropPtr crop);

    Seed(json11::Json object);

    virtual Seed* clone() const {return new Seed(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json(bool includeFullCropParameters = true) const;

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

	class Harvest : public WorkStep
	{
	public:
    Harvest() {}

    Harvest(const Tools::Date& at,
            CropPtr crop,
            PVResultPtr cropResult,
            std::string method = "total");

    Harvest(json11::Json j);

    virtual Harvest* clone() const { return new Harvest(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json(bool includeFullCropParameters = true) const;

    virtual void apply(MonicaModel* model);

    void setDate(Tools::Date date)
    {
      this->_date = date;
      _crop->setSeedAndHarvestDate(_crop->seedDate(), date);
    }

    void setPercentage(double percentage) { _percentage = percentage; }

    void setExported(bool exported)	{ _exported = exported; }

    PVResultPtr cropResult() const { return _cropResult; }

  private:
    CropPtr _crop;
    PVResultPtr _cropResult;
    std::string _method;
    double _percentage{0};
    bool _exported{true};
	};

  //----------------------------------------------------------------------------

	class Cutting : public WorkStep
	{
	public:
    Cutting(const Tools::Date& at);

    Cutting(json11::Json object);

    virtual Cutting* clone() const {return new Cutting(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

		virtual void apply(MonicaModel* model);
  };

	//----------------------------------------------------------------------------

	class MineralFertiliserApplication : public WorkStep
	{
	public:
		MineralFertiliserApplication(const Tools::Date& at,
																 MineralFertiliserParameters partition,
                                 double amount);

    MineralFertiliserApplication(json11::Json object);

    virtual MineralFertiliserApplication* clone() const {return new MineralFertiliserApplication(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual void apply(MonicaModel* model);

    MineralFertiliserParameters partition() const { return _partition; }

		double amount() const { return _amount; }

	private:
		MineralFertiliserParameters _partition;
		double _amount;
	};

  //----------------------------------------------------------------------------

	class OrganicFertiliserApplication : public WorkStep
	{
	public:
		OrganicFertiliserApplication(const Tools::Date& at,
                                 const OrganicMatterParametersPtr params,
																 double amount,
                                 bool incorp = true);

    OrganicFertiliserApplication(json11::Json j);

    virtual OrganicFertiliserApplication* clone() const {return new OrganicFertiliserApplication(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

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

	class TillageApplication : public WorkStep
	{
	public:
    TillageApplication(const Tools::Date& at, double depth);

    TillageApplication(json11::Json object);

    virtual TillageApplication* clone() const {return new TillageApplication(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

    virtual void apply(MonicaModel* model);

		double depth() const { return _depth; }

	private:
    double _depth{0.0};
	};

	//----------------------------------------------------------------------------

	class IrrigationApplication : public WorkStep
	{
	public:
    IrrigationApplication(const Tools::Date& at, double amount,
                          IrrigationParameters params = IrrigationParameters());

    IrrigationApplication(json11::Json object);

    virtual IrrigationApplication* clone() const {return new IrrigationApplication(*this); }

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

		virtual void apply(MonicaModel* model);

		double amount() const { return _amount; }

		double nitrateConcentration() const { return _params.nitrateConcentration; }

		double sulfateConcentration() const { return _params.sulfateConcentration; }

	private:
    double _amount{0};
		IrrigationParameters _params;
	};

	//----------------------------------------------------------------------------

  WSPtr makeWorkstep(json11::Json object);

  class CultivationMethod
      : public Tools::Json11Serializable
      , public std::multimap<Tools::Date, WSPtr>
	{
	public:
    CultivationMethod(const std::string& name = std::string("Fallow"));

    //! is semantically the equivalent to creating an empty CM and adding Seed, Harvest and Cutting applications
    CultivationMethod(CropPtr crop, const std::string& name = std::string());

    CultivationMethod(json11::Json object);

    virtual void merge(json11::Json j);

    virtual json11::Json to_json() const;

		template<class Application>
		void addApplication(const Application& a)
		{
      insert(std::make_pair(a.date(), WSPtr(new Application(a))));
    }

    template<>
    void addApplication<Seed>(const Seed& s)
    {
      insert(std::make_pair(s.date(), WSPtr(new Seed(s))));
      _crop = s.crop();
    }

    void addApplication(WSPtr a){ insert(std::make_pair(a->date(), a)); }

		void apply(const Tools::Date& date, MonicaModel* model) const;

		Tools::Date nextDate(const Tools::Date & date) const;

		std::string name() const { return _name; }

		CropPtr crop() const { return _crop; }

		bool isFallow() const { return !_crop->isValid();  }

		//! when does the PV start
    Tools::Date startDate() const;

		//! when does the whole PV end
    Tools::Date endDate() const;

    const std::multimap<Tools::Date, WSPtr>& getWorksteps() const { return *this; }

    void clearWorksteps() { clear(); }

		std::string toString() const;

		PVResult cropResult() const { return *(_cropResult.get()); }
		PVResultPtr cropResultPtr() const { return _cropResult; }

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
		PVResultPtr _cropResult;
	};
}

#endif
