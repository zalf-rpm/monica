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

#pragma once

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

namespace monica {

class MonicaModel;

class DLL_API Workstep : public Tools::Json11Serializable {
public:
  Workstep() = default;

  explicit Workstep(const Tools::Date &d);

  Workstep(int noOfDaysAfterEvent, const std::string &afterEvent);

  explicit Workstep(json11::Json object);

  virtual Workstep *clone() const = 0;

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  virtual std::string type() const { return "Workstep"; }

  virtual Tools::Date date() const { return _date; }

  virtual Tools::Date absDate() const { return _date.isAbsoluteDate() ? _date : _absDate; }

  virtual Tools::Date earliestDate() const { return date(); }

  virtual Tools::Date absEarliestDate() const { return absDate(); }

  virtual Tools::Date latestDate() const { return date(); }

  virtual Tools::Date absLatestDate() const { return absDate(); }

  virtual void setDate(Tools::Date date) { _date = date; }

  virtual int noOfDaysAfterEvent() const { return _applyNoOfDaysAfterEvent; }

  virtual std::string afterEvent() const { return _afterEvent; }

  //! do whatever the workstep has to do
  //! returns true if workstep is finished (dynamic worksteps might need to be applied again)
  virtual bool apply(MonicaModel *model);

  //! apply only if condition() is met (is used for dynamicWorksteps)
  virtual bool applyWithPossibleCondition(MonicaModel *model);

  virtual bool condition(MonicaModel *model);

  virtual bool isDynamicWorkstep() const { return !_date.isValid(); }

  //! tell if this workstep is active and can be used 
  //! a workstep might temporarily be deactivated, eg a dynamic sowing workstep
  //! which has to be checked for sowing every day, but not anymore after sowing
  virtual bool isActive() const { return _isActive; }

  //! reinit potential state of workstep
  virtual bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false);

  virtual std::function<double(MonicaModel *)>
  registerDailyFunction(std::function<std::vector<double> &()> getDailyValues) {
    return std::function<double(MonicaModel *)>();
  };

  bool runAtStartOfDay() const { return _runAtStartOfDay; }

  Tools::Errors& errors() { return _errors; }

protected:
  Tools::Date _date;
  Tools::Date _absDate;
  int _applyNoOfDaysAfterEvent{0};
  std::string _afterEvent;
  int _daysAfterEventCount{0};
  bool _isActive{true};
  bool _runAtStartOfDay{true};
  Tools::Errors _errors;
};

typedef std::shared_ptr<Workstep> WSPtr;

class DLL_API Sowing : public Workstep {
public:
  Sowing() = default;

  Sowing(json11::Json object);

  Sowing(const Sowing &other)
      : _cropToPlant(other._cropToPlant ? kj::heap<Crop>(*other._cropToPlant.get()) : kj::Own<Crop>()),
        _crop(_cropToPlant.get()), _plantDensity(other._plantDensity) {}

  virtual Sowing *clone() const { return new Sowing(*this); }

  // explicit Sowing(mas::schema::model::monica::Params::Sowing::Reader reader) { deserialize(reader); }
  // void deserialize(mas::schema::model::monica::Params::Sowing::Reader reader);
  // void serialize(mas::schema::model::monica::Params::Sowing::Builder builder) const;

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override { return to_json(true); }

  json11::Json to_json(bool includeFullCropParameters) const;

  virtual std::string type() const { return "Sowing"; }

  bool apply(MonicaModel *model) override;

  void setDate(Tools::Date date) override {
    this->_date = date;
    _crop->setSeedDate(date);
  }

  const Crop *crop() const { return _crop; }

  Crop *crop() { return _crop; }

  void setCropForReplanting(kj::Own<Crop> c) { _cropToPlant = kj::mv(c); }

private:
  kj::Own<Crop> _cropToPlant;
  Crop *_crop{nullptr};
  int _plantDensity{-1}; //[plants m-2]
};

class DLL_API AutomaticSowing : public Sowing {
public:
  AutomaticSowing() = default;

  explicit AutomaticSowing(json11::Json object);

  AutomaticSowing *clone() const override { return new AutomaticSowing(*this); }

  virtual Tools::Errors merge(json11::Json j);

  json11::Json to_json() const override { return to_json(true); }

  json11::Json to_json(bool includeFullCropParameters) const;

  std::string type() const override { return "AutomaticSowing"; }

  bool apply(MonicaModel *model) override;

  bool condition(MonicaModel *model) override;

  bool isActive() const override { return !_cropSeeded; }

  bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false) override;

  Tools::Date earliestDate() const override { return _earliestDate; }

  Tools::Date absEarliestDate() const override { return _absEarliestDate; }

  Tools::Date latestDate() const override { return _latestDate; }

  Tools::Date absLatestDate() const override { return _absLatestDate; }

  std::function<double(MonicaModel *)>
  registerDailyFunction(std::function<std::vector<double> &()> getDailyValues) override;

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

  bool _checkForSoilTemperature{false};
  double _soilDepthForAveraging{0.30}; //= 30 cm
  int _daysInSoilTempWindow{0};
  double _sowingIfAboveAvgSoilTemp{0};
  std::function<std::vector<double> &()> _getAvgSoilTemps;

  bool _inSowingRange{false};
  bool _cropSeeded{false};
};

class DLL_API Harvest : public Workstep {
public:
  enum CropUsage {
    greenManure = 0, biomassProduction
  };

  struct OptCarbonManagementData {
    bool optCarbonConservation{false};
    double cropImpactOnHumusBalance{0};
    double maxResidueRecoverFraction{1};
    CropUsage cropUsage{biomassProduction};
    double residueHeq{0};
    double organicFertilizerHeq{0};
  };

  struct Spec {
    struct Value {
      double exportPercentage{100.0};
      bool incorporate{true};
    };

    std::map<int, Value> organ2specVal;
  };

public:
  Harvest() = default;

//Harvest(const Tools::Date& at,
//    Crop* crop,
//    std::string method = "total");

  explicit Harvest(json11::Json j);

  Harvest *clone() const override { return new Harvest(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override { return to_json(true); }

  json11::Json to_json(bool includeFullCropParameters) const;

  std::string type() const override { return "Harvest"; }

  bool apply(MonicaModel *model) override;

  void setDate(Tools::Date date) override {
    this->_date = date;
    _sowing->crop()->setHarvestDate(date);
  }

  //void setPercentage(double percentage) { _percentage = percentage; }

  void setExported(bool exported) { _exported = exported; }

  const Crop &crop() const { return *_sowing->crop(); }

  Sowing *sowing() { return _sowing; }

  void setSowing(Sowing *s) { _sowing = s; }

  //void setIsCoverCrop(bool isCoverCrop) { _optCarbMgmtData.isCoverCrop = isCoverCrop; }
protected:
  Sowing *_sowing{nullptr};

private:
  // double _percentage{0};
  bool _exported{true};
  Spec _spec;
  OptCarbonManagementData _optCarbMgmtData;
  int _incorporateIntoLayerNo{1};
};

class DLL_API AutomaticHarvest : public Harvest {
public:
  AutomaticHarvest();

  //AutomaticHarvest(Crop* crop,
  //										std::string harvestTime,
  //										Tools::Date latestHarvest,
  //										std::string method = "total");

  explicit AutomaticHarvest(json11::Json object);

  AutomaticHarvest *clone() const override { return new AutomaticHarvest(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override { return to_json(true); }

  json11::Json to_json(bool includeFullCropParameters) const;

  std::string type() const override { return "AutomaticHarvest"; }

  bool apply(MonicaModel *model) override;

  bool condition(MonicaModel *model) override;

  bool isActive() const override { return !_cropHarvested; }

  bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false) override;

  Tools::Date latestDate() const override { return _latestDate; }

  Tools::Date absLatestDate() const override { return _absLatestDate; }

private:
  std::string _harvestTime; //!< Harvest time parameter
  Tools::Date _latestDate;
  Tools::Date _absLatestDate;
  double _minPercentASW{0};
  double _maxPercentASW{999};
  double _max3dayPrecipSum{9999};
  double _maxCurrentDayPrecipSum{9999};
  bool _cropHarvested{false};
};

class DLL_API Cutting : public Workstep {
public:
  explicit Cutting(const Tools::Date &at);

  explicit Cutting(json11::Json object);

  Cutting *clone() const override { return new Cutting(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "Cutting"; }

  bool apply(MonicaModel *model) override;


  enum CL {
    cut, left, none
  };
  enum Unit {
    percentage, biomass, LAI
  };
  struct Value {
    double value{0.0};
    Unit unit{percentage};
    CL cut_or_left{cut};
  };

private:
  std::map<int, Value> _organId2cuttingSpec;
  std::map<int, double> _organId2biomAfterCutting;
  std::map<int, double> _organId2exportFraction;
  double _cutMaxAssimilationRateFraction{1.0};
};

class DLL_API MineralFertilization : public Workstep {
public:
  MineralFertilization() = default;

  MineralFertilization(const Tools::Date &at,
                       MineralFertilizerParameters partition,
                       double amount);

  explicit MineralFertilization(json11::Json object);

  MineralFertilization *clone() const override { return new MineralFertilization(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "MineralFertilization"; }

  bool apply(MonicaModel *model) override;

  MineralFertilizerParameters partition() const { return _partition; }

  double amount() const { return _amount; }

private:
  MineralFertilizerParameters _partition;
  double _amount{0.0};
};

class DLL_API NDemandFertilization : public Workstep {
public:
  NDemandFertilization() = default;

  NDemandFertilization(int stage,
                       double depth,
                       MineralFertilizerParameters partition,
                       double Ndemand);

  NDemandFertilization(Tools::Date date,
                       double depth,
                       MineralFertilizerParameters partition,
                       double Ndemand);

  explicit NDemandFertilization(json11::Json object);

  NDemandFertilization *clone() const override { return new NDemandFertilization(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "NDemandFertilization"; }

  bool apply(MonicaModel *model) override;

  bool condition(MonicaModel *model) override;

  MineralFertilizerParameters partition() const { return _partition; }

  bool isActive() const override { return !_appliedFertilizer; }

  bool reinit(Tools::Date date, bool addYear = false, bool forceInitYear = false) override;

private:
  Tools::Date _initialDate;
  MineralFertilizerParameters _partition;
  double _Ndemand{0};
  double _depth{0.0};
  int _stage{1};
  bool _appliedFertilizer{false};
};

class DLL_API OrganicFertilization : public Workstep {
public:
  OrganicFertilization(const Tools::Date &at,
                       const OrganicMatterParameters &params,
                       double amount,
                       bool incorp = true);

  explicit OrganicFertilization(json11::Json j);

  OrganicFertilization *clone() const override { return new OrganicFertilization(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "OrganicFertilization"; }

  bool apply(MonicaModel *model) override;

  //! Returns parameter for organic fertilizer
  const OrganicMatterParameters &parameters() const { return _params; }

  //! Returns fertilization amount
  double amount() const { return _amount; }

  //! Returns TRUE, if fertilizer is applied with incorporation
  bool incorporation() const { return _incorporation; }

private:
  OrganicMatterParameters _params;
  double _amount{0.0};
  bool _incorporation{false};
  int _incorporateIntoLayerNo{1};
};

class DLL_API Tillage : public Workstep {
public:
  Tillage(const Tools::Date &at, double depth);

  explicit Tillage(json11::Json object);

  Tillage *clone() const override { return new Tillage(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "Tillage"; }

  bool apply(MonicaModel *model) override;

  double depth() const { return _depth; }

private:
  double _depth{0.3};
};

class DLL_API SetValue : public Workstep {
public:
  SetValue(const Tools::Date &at, OId oid, json11::Json value);

  explicit SetValue(json11::Json object);

  SetValue *clone() const override { return new SetValue(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "SetValue"; }

  bool apply(MonicaModel *model) override;

  json11::Json value() const { return _value; }

private:
  OId _oid;
  json11::Json _value;
  std::function<json11::Json(const monica::MonicaModel *)> _getValue;
};

class DLL_API SaveMonicaState : public Workstep {
public:
  SaveMonicaState(const Tools::Date &at, std::string pathToSerializedStateFile, bool serializeAsJson = false,
    int noOfPreviousDaysSerializedClimateData = -1);

  explicit SaveMonicaState(json11::Json object);

  SaveMonicaState *clone() const override { return new SaveMonicaState(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "SaveMonicaState"; }

  bool apply(MonicaModel *model) override;

  std::string pathToSerializedStateFile() const { return _pathToFile; }

private:
  std::string _pathToFile;
  bool _toJson{false};
  int _noOfPreviousDaysSerializedClimateData{-1};
};

class DLL_API Irrigation : public Workstep {
public:
  Irrigation(const Tools::Date &at, double amount,
             IrrigationParameters params = IrrigationParameters());

  explicit Irrigation(json11::Json object);

  Irrigation *clone() const override { return new Irrigation(*this); }

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  std::string type() const override { return "Irrigation"; }

  bool apply(MonicaModel *model) override;

  double amount() const { return _amount; }

  double nitrateConcentration() const { return _params.nitrateConcentration; }

  double sulfateConcentration() const { return _params.sulfateConcentration; }

private:
  double _amount{0};
  IrrigationParameters _params;
};

DLL_API WSPtr makeWorkstep(json11::Json object);

class DLL_API CultivationMethod
    : public Tools::Json11Serializable
  //, public std::multimap<Tools::Date, WSPtr>
{
public:
  explicit CultivationMethod(const std::string &name = std::string("Fallow"));

  //! is semantically the equivalent to creating an empty CM and adding Sowing, Harvest and Cutting applications
  //CultivationMethod(CropPtr crop, const std::string& name = std::string());

  explicit CultivationMethod(json11::Json object);

  Tools::Errors merge(json11::Json j) override;

  json11::Json to_json() const override;

  template<class Application>
  void addApplication(const Application &a) {
    //_allWorksteps.insert(std::make_pair(a.date(), std::make_shared<Application>(a)));
    _allWorksteps.push_back(std::make_shared<Application>(a));
  }

  void apply(const Tools::Date &date, MonicaModel *model) const;

  void absApply(const Tools::Date &date, MonicaModel *model) const;

  void apply(MonicaModel *model, bool runOnlyAtStartOfDayWorksteps);

  Tools::Date nextDate(const Tools::Date &date) const;

  Tools::Date nextAbsDate(const Tools::Date &date) const;

  std::vector<WSPtr> workstepsAt(const Tools::Date &date) const;

  std::vector<WSPtr> absWorkstepsAt(const Tools::Date &date) const;

  bool areOnlyAbsoluteWorksteps() const;

  std::vector<WSPtr> staticWorksteps() const;

  std::vector<WSPtr> allDynamicWorksteps() const;

  std::vector<WSPtr> unfinishedDynamicWorksteps() const { return _unfinishedDynamicWorksteps; }

  bool allDynamicWorkstepsFinished() const;

  std::string name() const { return _name; }

  const Crop &crop() const { return *_crop; }

  bool isFallow() const { return !_crop || !_crop->isValid(); }

  //! when does the PV start
  Tools::Date startDate() const;

  Tools::Date absStartDate(bool includeDynamicWorksteps = true) const;

  Tools::Date absLatestSowingDate() const;

  //! when does the whole PV end
  Tools::Date endDate() const;

  Tools::Date absEndDate() const;

//const std::multimap<Tools::Date, WSPtr>& getWorksteps() const { return _allWorksteps; }
  const std::vector<WSPtr> &getWorksteps() const { return _allWorksteps; }

  void clearWorksteps() { _allWorksteps.clear(); }

  std::string toString() const;

  //the custom id is used to keep a potentially usage defined
  //mapping to an entity from another domain,
  //e.g. a Carbiocial CropActivity which the CultivationMethod was based on
  void setCustomId(int cid) { _customId = cid; }

  int customId() const { return _customId; }

  void setIrrigateCrop(bool irr) { _irrigateCrop = irr; }

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
  const Crop *_crop{nullptr};
  bool _irrigateCrop{false};
  bool _canBeSkipped{false}; //! can this crop be skipped, eg. is a catch or cover crop
  bool _isCoverCrop{
      false}; //! is like canBeSkipped (and implies it), but different rule for when cultivation methods will be skipped
  bool _repeat{true}; //! if false the cultivation method won't participate in wrapping at the end of the crop rotation
};

template<>
DLL_API inline void CultivationMethod::addApplication<Sowing>(const Sowing &s) {
//_allWorksteps.insert(std::make_pair(s.date(), std::make_shared<Sowing>(s)));
  _allWorksteps.push_back(std::make_shared<Sowing>(s));
  _crop = s.crop();
}

template<>
DLL_API inline void CultivationMethod::addApplication<AutomaticSowing>(const AutomaticSowing &s) {
  //_allWorksteps.insert(std::make_pair(s.date(), std::make_shared<AutomaticSowing>(s)));
  _allWorksteps.push_back(std::make_shared<AutomaticSowing>(s));
  _crop = s.crop();
}

//template<>
//DLL_API inline void CultivationMethod::addApplication<Harvest>(const Harvest& h)
//{
//  _allWorksteps.insert(std::make_pair(h.date(), std::make_shared<Harvest>(h)));
//}

} // namespace monica
