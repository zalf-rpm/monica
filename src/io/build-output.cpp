/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg-Mohnicke <michael.berg@zalf.de>
Tommaso Stella <tommaso.stella@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "build-output.h"

#include <fstream>
#include <algorithm>
#include <mutex>
#include <numeric>
#include <iterator>
#include <climits>

#include "json11/json11-helper.h"
#include "tools/debug.h"
#include "tools/helper.h"
#include "tools/algorithms.h"
#include "../core/monica-model.h"
#include "../core/crop-module.h"
#include "../core/soilmoisture.h"
#include "../core/soiltemperature.h"
#include "../core/soiltransport.h"
#include "../core/soilorganic.h"

using namespace monica;
using namespace Tools;
using namespace std;
using namespace json11;

double monica::applyOIdOP(OId::OP op, const vector<double>& vs)
{
  double v = 0.0;
  if (!vs.empty())
  {
    v = vs.back();

    switch (op)
    {
    case OId::AVG:
      v = accumulate(vs.begin(), vs.end(), 0.0) / vs.size();
      break;
    case OId::MEDIAN:
      v = median(vs);
      break;
    case OId::SUM:
      v = accumulate(vs.begin(), vs.end(), 0.0);
      break;
    case OId::MIN:
      v = minMax(vs).first;
      break;
    case OId::MAX:
      v = minMax(vs).second;
      break;
    case OId::FIRST:
      v = vs.front();
      break;
    case OId::LAST:
    case OId::NONE:
    default:;
    }
  }

  return v;
}

Json monica::applyOIdOP(OId::OP op, const vector<Json>& js)
{
  Json res;

  if (!js.empty() && js.front().is_array())
  {
    vector<vector<double>> dss(js.front().array_items().size());
    for (auto& j : js)
    {
      int i = 0;
      for (auto& j2 : j.array_items())
      {
        dss[i].push_back(j2.number_value());
        ++i;
      }
    }

    J11Array r;
    for (auto& ds : dss)
      r.push_back(applyOIdOP(op, ds));

    res = r;
  }
  else
  {
    vector<double> ds;
    for (auto j : js)
      ds.push_back(j.number_value());
    res = applyOIdOP(op, ds);
  }

  return res;
}

vector<OId> monica::parseOutputIds(const J11Array& oidArray)
{
  vector<OId> outputIds;

  auto getAggregationOp = [](J11Array arr, uint8_t index, OId::OP def = OId::_UNDEFINED_OP_) -> OId::OP
  {
    if (arr.size() > index && arr[index].is_string())
    {
      string ops = arr[index].string_value();
      if (toUpper(ops) == "SUM")
        return OId::SUM;
      else if (toUpper(ops) == "AVG")
        return OId::AVG;
      else if (toUpper(ops) == "MEDIAN")
        return OId::MEDIAN;
      else if (toUpper(ops) == "MIN")
        return OId::MIN;
      else if (toUpper(ops) == "MAX")
        return OId::MAX;
      else if (toUpper(ops) == "FIRST")
        return OId::FIRST;
      else if (toUpper(ops) == "LAST")
        return OId::LAST;
      else if (toUpper(ops) == "NONE")
        return OId::NONE;
    }
    return def;
  };


  auto getOrgan = [](J11Array arr, uint8_t index, OId::ORGAN def = OId::_UNDEFINED_ORGAN_) -> OId::ORGAN
  {
    if (arr.size() > index && arr[index].is_string())
    {
      string ops = arr[index].string_value();
      if (toUpper(ops) == "ROOT")
        return OId::ROOT;
      else if (toUpper(ops) == "LEAF")
        return OId::LEAF;
      else if (toUpper(ops) == "SHOOT")
        return OId::SHOOT;
      else if (toUpper(ops) == "FRUIT")
        return OId::FRUIT;
      else if (toUpper(ops) == "STRUCT")
        return OId::STRUCT;
      else if (toUpper(ops) == "SUGAR")
        return OId::SUGAR;
    }
    return def;
  };

  const auto& name2metadata = buildOutputTable().name2metadata;
  for (Json idj : oidArray)
  {
    if (idj.is_string())
    {
      string name = idj.string_value();
      auto names = splitString(name, "|");
      names.resize(2);
      auto it = name2metadata.find(names[0]);
      if (it != name2metadata.end())
      {
        auto data = it->second;
        OId oid(data.id);
        oid.name = data.name;
        oid.displayName = names[1];
        oid.unit = data.unit;
        oid.jsonInput = name;
        outputIds.push_back(oid);
      }
    }
    else if (idj.is_array())
    {
      auto arr = idj.array_items();
      if (arr.size() >= 1)
      {
        OId oid;

        string name = arr[0].string_value();
        auto names = splitString(name, "|");
        names.resize(2);
        auto it = name2metadata.find(names[0]);
        if (it != name2metadata.end())
        {
          auto data = it->second;
          oid.id = data.id;
          oid.name = data.name;
          oid.displayName = names[1];
          oid.unit = data.unit;
          oid.jsonInput = Json(arr).dump();

          if (arr.size() >= 2)
          {
            auto val1 = arr[1];
            if (val1.is_number())
            {
              oid.fromLayer = val1.int_value() - 1;
              oid.toLayer = oid.fromLayer;
            }
            else if (val1.is_string())
            {
              auto op = getAggregationOp(arr, 1);
              if (op != OId::_UNDEFINED_OP_)
                oid.timeAggOp = op;
              else
                oid.organ = getOrgan(arr, 1, OId::_UNDEFINED_ORGAN_);
            }
            else if (val1.is_array())
            {
              auto arr2 = arr[1].array_items();

              if (arr2.size() >= 1)
              {
                auto val1_0 = arr2[0];
                if (val1_0.is_number())
                  oid.fromLayer = val1_0.int_value() - 1;
                else if (val1_0.is_string())
                  oid.organ = getOrgan(arr2, 0, OId::_UNDEFINED_ORGAN_);
              }
              if (arr2.size() >= 2)
              {
                auto val1_1 = arr2[1];
                if (val1_1.is_number())
                  oid.toLayer = val1_1.int_value() - 1;
                else if (val1_1.is_string())
                {
                  oid.toLayer = oid.fromLayer;
                  oid.layerAggOp = getAggregationOp(arr2, 1, OId::AVG);
                }
              }
              if (arr2.size() >= 3)
                oid.layerAggOp = getAggregationOp(arr2, 2, OId::AVG);
            }
          }
          if (arr.size() >= 3)
            oid.timeAggOp = getAggregationOp(arr, 2, OId::AVG);

          outputIds.push_back(oid);
        }
      }
    }
  }

  return outputIds;
}

template<typename T, typename Vector>
void store(OId oid, Vector& into, function<T(int)> getValue, int roundToDigits = 0)
{
  Vector multipleValues;
  vector<double> vs;
  if (oid.isOrgan())
    oid.toLayer = oid.fromLayer = int(oid.organ);

  for (int i = oid.fromLayer; i <= oid.toLayer; i++)
  {
    T v = 0;
    if (i < 0)
      debug() << "Error: " << oid.toString(true) << " has no or negative layer defined! Returning 0." << endl;
    else
      v = getValue(i);
    if (oid.layerAggOp == OId::NONE)
      multipleValues.push_back(Tools::round(v, roundToDigits));
    else
      vs.push_back(v);
  }

  if (oid.layerAggOp == OId::NONE)
    into.push_back(multipleValues);
  else
    into.push_back(applyOIdOP(oid.layerAggOp, vs));
}

template<typename T>
Json getComplexValues(OId oid, function<T(int)> getValue, int roundToDigits = 0)
{
  J11Array multipleValues;
  vector<double> vs;
  if (oid.isOrgan())
    oid.toLayer = oid.fromLayer = int(oid.organ);

  for (int i = oid.fromLayer; i <= oid.toLayer; i++)
  {
    T v = 0;
    if (i < 0)
      debug() << "Error: " << oid.toString(true) << " has no or negative layer defined! Returning 0." << endl;
    else
      v = getValue(i);
    if (oid.layerAggOp == OId::NONE)
      multipleValues.push_back(Tools::round(v, roundToDigits));
    else
      vs.push_back(v);
  }

  return oid.layerAggOp == OId::NONE ? Json(multipleValues) : Json(applyOIdOP(oid.layerAggOp, vs));
}

void setComplexValues(OId oid, function<void(int, json11::Json)> setValue, Json value)
{
  if (oid.isOrgan())
    oid.toLayer = oid.fromLayer = int(oid.organ);

  J11Array values;
  if (value.is_object() || value.is_null())
    return;
  else if (value.is_array())
    values = value.array_items();
  else
    values = J11Array(oid.toLayer - oid.fromLayer + 1, value);
  assert(values.size() <= INT_MAX);
  for (int i = oid.fromLayer, k = 0, vsize = (int)values.size(); i <= oid.toLayer, k < vsize; i++, k++)
  {
    if (i < 0)
      debug() << "Error: " << oid.toString(true) << " has no or negative layer defined! Can't set value." << endl;
    else
      setValue(i, values[k]);
  }
}

BOTRes& monica::buildOutputTable()
{
  static mutex lockable;

  //map of output ids to outputfunction
  static BOTRes m;
  static bool tableBuilt = false;

  typedef decltype(m.setfs)::mapped_type SETF_T;
  auto build = [&](OutputMetadata r,
    decltype(m.ofs)::mapped_type of,
    decltype(m.setfs)::mapped_type setf = SETF_T())
  {
    m.ofs[r.id] = of;
    if (setf)
      m.setfs[r.id] = setf;
    m.name2metadata[r.name] = r;
    return r;
  };

  // only initialize once
  if (!tableBuilt)
  {
    lock_guard<mutex> lock(lockable);

    //test if after waiting for the lock the other thread
    //already initialized the whole thing
    if (!tableBuilt)
    {
      int id = 0;

      build({ id++, "Count", "", "output 1 for counting things" },
        [](const MonicaModel& monica, OId oid)
      {
        return 1;
      });

      build({ id++, "CM-count", "", "output the order number of the current cultivation method" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cultivationMethodCount();
      });

      build({ id++, "Date", "", "output current date" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.currentStepDate().toIsoDateString();
      });

      build({ id++, "days-since-start", "", "output number of days since simulation start" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.currentStepDate() - monica.simulationParameters().startDate;
      });

      build({ id++, "DOY", "", "output current day of year" },
        [](const MonicaModel& monica, OId oid)
      {
        return int(monica.currentStepDate().dayOfYear());
      });

      build({ id++, "Month", "", "output current Month" },
        [](const MonicaModel& monica, OId oid)
      {
        return int(monica.currentStepDate().month());
      });

      build({ id++, "Year", "", "output current Year" },
        [](const MonicaModel& monica, OId oid)
      {
        return int(monica.currentStepDate().year());
      });

      build({ id++, "Crop", "", "crop name" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? monica.cropGrowth()->get_CropName() : "";
      });

      build({ id++, "TraDef", "0;1", "TranspirationDeficit" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_TranspirationDeficit(), 2) : 0.0;
      });

      build({ id++, "Tra", "mm", "ActualTranspiration" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.getTranspiration(), 2);
      });

      build({ id++, "NDef", "0;1", "CropNRedux" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_CropNRedux(), 2) : 0.0;
      });

      build({ id++, "HeatRed", "0;1", " HeatStressRedux" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_HeatStressRedux(), 2) : 0.0;
      });

      build({ id++, "FrostRed", "0;1", "FrostStressRedux" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_FrostStressRedux(), 2) : 0.0;
      });

      build({ id++, "OxRed", "0;1", "OxygenDeficit" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_OxygenDeficit(), 2) : 0.0;
      });

      build({ id++, "Stage", "1-6/7", "DevelopmentalStage" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? int(monica.cropGrowth()->get_DevelopmentalStage()) + 1 : 0;
      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        if (value.is_number() && monica.cropGrowth())
          monica.cropGrowth()->setStage(max(0, int(value.number_value()) - 1));
      });

      build({ id++, "TempSum", "�Cd", "CurrentTemperatureSum" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_CurrentTemperatureSum(), 1) : 0.0;
      });

      build({ id++, "VernF", "0;1", "VernalisationFactor" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_VernalisationFactor(), 2) : 0.0;
      });

      build({ id++, "DaylF", "0;1", "DaylengthFactor" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_DaylengthFactor(), 2) : 0.0;
      });

      build({ id++, "IncRoot", "kg ha-1", "OrganGrowthIncrement root" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_OrganGrowthIncrement(0), 2) : 0.0;
      });

      build({ id++, "IncLeaf", "kg ha-1", "OrganGrowthIncrement leaf" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_OrganGrowthIncrement(1), 2) : 0.0;
      });

      build({ id++, "IncShoot", "kg ha-1", "OrganGrowthIncrement shoot" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_OrganGrowthIncrement(2), 2) : 0.0;
      });

      build({ id++, "IncFruit", "kg ha-1", "OrganGrowthIncrement fruit" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_OrganGrowthIncrement(3), 2) : 0.0;
      });

      build({ id++, "RelDev", "0;1", "RelativeTotalDevelopment" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_RelativeTotalDevelopment(), 2) : 0.0;
      });

      build({ id++, "LT50", "°C", "LT50" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_LT50(), 1) : 0.0;
      });

      build({ id++, "AbBiom", "kgDM ha-1", "AbovegroundBiomass" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_AbovegroundBiomass(), 1) : 0.0;
      });

      build({ id++, "OrgBiom", "kgDM ha-1", "get_OrganBiomass(i)" },
        [](const MonicaModel& monica, OId oid)
      {
        if (oid.isOrgan()
          && monica.cropGrowth()
          && monica.cropGrowth()->get_NumberOfOrgans() > oid.organ)
          return round(monica.cropGrowth()->get_OrganBiomass(oid.organ), 1);
        else
          return 0.0;
      });

      build({ id++, "OrgGreenBiom", "kgDM ha-1", "get_OrganGreenBiomass(i)" },
        [](const MonicaModel& monica, OId oid)
      {
        if (oid.isOrgan()
          && monica.cropGrowth()
          && monica.cropGrowth()->get_NumberOfOrgans() > oid.organ)
          return round(monica.cropGrowth()->get_OrganGreenBiomass(oid.organ), 1);
        else
          return 0.0;
      });

      build({ id++, "Yield", "kgDM ha-1", "get_PrimaryCropYield" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_PrimaryCropYield(), 1) : 0.0;
      });

      build({id++, "SecondaryYield", "kgDM ha-1", "get_SecondaryCropYield"},
            [](const MonicaModel &monica, OId oid) {
              return monica.cropGrowth() ? round(monica.cropGrowth()->get_SecondaryCropYield(), 3) : 0.0;
            });

      build({ id++, "sumExportedCutBiomass", "kgDM ha-1", "return sum (across cuts) of exported cut biomass for current crop" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->sumExportedCutBiomass(), 1) : 0.0;
      });

      build({ id++, "exportedCutBiomass", "kgDM ha-1", "return exported cut biomass for current crop and cut" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->exportedCutBiomass(), 1) : 0.0;
      });

      build({ id++, "sumResidueCutBiomass", "kgDM ha-1", "return sum (across cuts) of residue cut biomass for current crop" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->sumResidueCutBiomass(), 1) : 0.0;
      });

      build({ id++, "residueCutBiomass", "kgDM ha-1", "return residue cut biomass for current crop and cut" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->residueCutBiomass(), 1) : 0.0;
      });

      build({ id++, "optCarbonExportedResidues", "kgDM ha-1", "return exported part of the residues according to optimal carbon balance" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.optCarbonExportedResidues(), 1);
      });

      build({ id++, "optCarbonReturnedResidues", "kgDM ha-1", "return returned to soil part of the residues according to optimal carbon balance" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.optCarbonReturnedResidues(), 1);
      });

      build({ id++, "humusBalanceCarryOver", "Heq-NRW ha-1", "return humus balance carry over according to optimal carbon balance" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.humusBalanceCarryOver(), 1);
      });


      build({ id++, "GroPhot", "kgCH2O ha-1", "GrossPhotosynthesisHaRate" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_GrossPhotosynthesisHaRate(), 4) : 0.0;
      });

      build({ id++, "NetPhot", "kgCH2O ha-1", "NetPhotosynthesis" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_NetPhotosynthesis(), 2) : 0.0;
      });

      build({ id++, "MaintR", "kgCH2O ha-1", "MaintenanceRespirationAS" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_MaintenanceRespirationAS(), 4) : 0.0;
      });

      build({ id++, "GrowthR", "kgCH2O ha-1", "GrowthRespirationAS" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_GrowthRespirationAS(), 4) : 0.0;
      });

      build({ id++, "StomRes", "s m-1", "StomataResistance" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_StomataResistance(), 2) : 0.0;
      });

      build({ id++, "Height", "m", "CropHeight" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_CropHeight(), 2) : 0.0;
      });

      build({ id++, "LAI", "m2 m-2", "LeafAreaIndex" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_LeafAreaIndex(), 4) : 0.0;
      });

      build({ id++, "RootDep", "layer#", "RootingDepth" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? int(monica.cropGrowth()->get_RootingDepth()) : 0;
      });

      build({ id++, "EffRootDep", "m", "Effective RootingDepth" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->getEffectiveRootingDepth(), 2) : 0.0;
      });

      build({ id++, "TotBiomN", "kgN ha-1", "TotalBiomassNContent" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_TotalBiomassNContent(), 1) : 0.0;
      });

      build({ id++, "AbBiomN", "kgN ha-1", "AbovegroundBiomassNContent" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_AbovegroundBiomassNContent(), 1) : 0.0;
      });

      build({ id++, "SumNUp", "kgN ha-1", "SumTotalNUptake" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_SumTotalNUptake(), 2) : 0.0;
      });

      build({ id++, "ActNup", "kgN ha-1", "ActNUptake" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_ActNUptake(), 2) : 0.0;
      });

      build({ id++, "RootWaUptak", "KgN ha-1", "RootWatUptakefromLayer" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.cropGrowth() ? monica.cropGrowth()->get_Transpiration(i) : 0.0; }, 4);
      });

      build({id++, "PotNup", "kgN ha-1", "PotNUptake"},
            [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_PotNUptake(), 2) : 0.0;
      });

      build({ id++, "NFixed", "kgN ha-1", "NFixed" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_BiologicalNFixation(), 2) : 0.0;
      });

      build({ id++, "Target", "kgN ha-1", "TargetNConcentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_TargetNConcentration(), 3) : 0.0;
      });

      build({ id++, "CritN", "kgN ha-1", "CriticalNConcentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_CriticalNConcentration(), 3) : 0.0;
      });

      build({ id++, "AbBiomNc", "kgN ha-1", "AbovegroundBiomassNConcentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_AbovegroundBiomassNConcentration(), 3) : 0.0;
      });

      build({ id++, "Nstress", "-", "NitrogenStressIndex" }

        , [](const MonicaModel& monica, OId oid)
      {
        double Nstress = 0;
        double AbBiomNc = monica.cropGrowth() ? round(monica.cropGrowth()->get_AbovegroundBiomassNConcentration(), 3) : 0.0;
        double CritN = monica.cropGrowth() ? round(monica.cropGrowth()->get_CriticalNConcentration(), 3) : 0.0;

        if (monica.cropGrowth())
        {
          Nstress = AbBiomNc < CritN ? round((AbBiomNc / CritN), 3) : 1;
        }
        return Nstress;
      });

      build({id++, "YieldNc", "kgN ha-1", "PrimaryYieldNConcentration"},
            [](const MonicaModel &monica, OId oid) {
              return monica.cropGrowth() ? round(monica.cropGrowth()->get_PrimaryYieldNConcentration(), 3) : 0.0;
            });


      build({ id++, "YieldN", "kgN ha-1", "PrimaryYieldNContent" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_PrimaryYieldNContent(), 3) : 0.0;
      });

      build({id++, "Protein", "kg kg-1", "RawProteinConcentration"},
            [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_RawProteinConcentration(), 3) : 0.0;
      });

      build({ id++, "NPP", "kgC ha-1", "NPP" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_NetPrimaryProduction(), 5) : 0.0;
      });

      build({ id++, "NPP-Organs", "kgC ha-1", "organ specific NPP" },
        [](const MonicaModel& monica, OId oid)
      {
        if (oid.isOrgan()
          && monica.cropGrowth()
          && monica.cropGrowth()->get_NumberOfOrgans() > oid.organ)
          return round(monica.cropGrowth()->get_OrganSpecificNPP(oid.organ), 4);
        else
          return 0.0;
      });

      build({ id++, "GPP", "kgC ha-1", "GPP" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_GrossPrimaryProduction(), 5) : 0.0;
      });

      build({ id++, "LightInterception1", "", "LightInterception of single crop or top layer of taller crop" },
        [](const MonicaModel& monica, OId oid)
        {
          return monica.cropGrowth() ? round(monica.cropGrowth()->getFractionOfInterceptedRadiation1(), 5) : 0.0;
        });

      build({ id++, "LightInterception2", "", "LightInterception of lower layer of taller crop" },
        [](const MonicaModel& monica, OId oid)
        {
          return monica.cropGrowth() ? round(monica.cropGrowth()->getFractionOfInterceptedRadiation2(), 5) : 0.0;
        });

      build({ id++, "Ra", "kgC ha-1", "autotrophic respiration" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_AutotrophicRespiration(), 5) : 0.0;
      });

      build({ id++, "Ra-Organs", "kgC ha-1", "organ specific autotrophic respiration" },
        [](const MonicaModel& monica, OId oid)
      {
        if (oid.isOrgan()
          && monica.cropGrowth()
          && monica.cropGrowth()->get_NumberOfOrgans() > oid.organ)
          return round(monica.cropGrowth()->get_OrganSpecificTotalRespired(oid.organ), 4);
        else
          return 0.0;
      });

      build({ id++, "Mois", "m3 m-3", "Soil moisture content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilMoisture().get_SoilMoisture(i); }, 3);
      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        setComplexValues(oid, [&](int i, Json j)
        {
          if (j.is_number())
            monica.soilColumnNC()[i].set_Vs_SoilMoisture_m3(j.number_value());
        }, value);
      });

      build({ id++, "ActNupLayer", "KgN ha-1", "ActNUptakefromLayer" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.cropGrowth() ? monica.cropGrowth()->get_NUptakeFromLayer(i) * 10000.0 : 0.0; }, 4);
      });


      build({id++, "Irrig", "mm", "Irrigation"},
            [](const MonicaModel& monica, OId oid)
      {
        return round(monica.dailySumIrrigationWater(), 3);
      });

      build({ id++, "Infilt", "mm", "Infiltration" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_Infiltration(), 1);
      });

      build({ id++, "Surface", "mm", "Surface water storage" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_SurfaceWaterStorage(), 1);
      });

      build({ id++, "RunOff", "mm", "Surface water runoff" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_SurfaceRunOff(), 1);
      });

      build({ id++, "SnowD", "mm", "Snow depth" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().getSnowDepth(), 1);
      });

      build({ id++, "FrostD", "m", "Frost front depth in soil" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_FrostDepth(), 1);
      });

      build({ id++, "ThawD", "m", "Thaw front depth in soil" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_ThawDepth(), 1);
      });

      build({ id++, "PASW", "m3 m-3", "PASW" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i)
        {
          return monica.soilMoisture().get_SoilMoisture(i) - monica.soilColumn().at(i).vs_PermanentWiltingPoint();
        }, 3);
      });

      build({ id++, "SurfTemp", "°C", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilTemperature().getSoilSurfaceTemperature(), 6);
      });

      build({ id++, "STemp", "°C", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilTemperature().getSoilTemperature(i); }, 6);
      });

      build({ id++, "Act_Ev", "mm", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_ActualEvaporation(), 1);
      });

      build({ id++, "Pot_ET", "mm", "ET0 * Kc" }

        , [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_PotentialEvapotranspiration(), 1);
      });

      build({ id++, "Act_ET", "mm", "vm_ActualTranspiration + vm_ActualEvaporation + vc_EvaporatedFromIntercept + vm_EvaporatedFromSurface" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_ActualEvapotranspiration(), 1);
      });

      //build({ id++, "Act_ET2", "mm", "" },
      //	[](const MonicaModel& monica, OId oid)
      //{
      //	return round((monica.soilMoisture().get_ActualEvaporation() + monica.getTranspiration()), 2);
      //});

      build({ id++, "ET0", "mm", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_ET0(), 1);
      });

      build({ id++, "Kc", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().getKcFactor(), 1);
      });

      build({ id++, "AtmCO2", "ppm", "Atmospheric CO2 concentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.get_AtmosphericCO2Concentration(), 0);
      });

      build({ id++, "AtmO3", "ppb", "Atmospheric O3 concentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.get_AtmosphericO3Concentration(), 0);
      });

      build({ id++, "Groundw", "m", "rounded according to interna usage" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.get_GroundwaterDepth(), 2);
      });

      build({ id++, "Recharge", "mm", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_GroundwaterRecharge(), 3);
      });

      build({ id++, "NLeach", "kgN ha-1", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilTransport().get_NLeaching(), 3);
      });

      build({ id++, "NO3", "kgN m-3", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i){ return monica.soilColumn().at(i).get_SoilNO3(); }, 6);
      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        setComplexValues(oid, [&](int i, Json j)
        {
          if (j.is_number())
            monica.soilColumnNC()[i].vs_SoilNO3 = j.number_value();
        }, value);
      });

      build({ id++, "Carb", "kgN m-3", "Soil Carbamid" },
        [](const MonicaModel& monica, OId oid)
      {
        //return round(monica.soilColumn().at(0).get_SoilCarbamid(), 4);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).get_SoilCarbamid(); }, 4);

      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        setComplexValues(oid, [&](int i, Json j)
        {
          if (j.is_number())
            monica.soilColumnNC()[i].vs_SoilCarbamid = j.number_value();
        }, value);
      });

      build({ id++, "NH4", "kgN m-3", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i){ return monica.soilColumn().at(i).get_SoilNH4(); }, 6);
      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        setComplexValues(oid, [&](int i, Json j)
        {
          if (j.is_number())
            monica.soilColumnNC()[i].vs_SoilNH4 = j.number_value();
        }, value);
      });

      build({ id++, "NO2", "kgN m-3", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i){ return monica.soilColumn().at(i).get_SoilNO2(); }, 6);
      },
        [](MonicaModel& monica, OId oid, Json value)
      {
        setComplexValues(oid, [&](int i, Json j)
        {
          if (j.is_number())
            monica.soilColumnNC()[i].vs_SoilNO2 = j.number_value();
        }, value);
      });

      build({ id++, "SOC", "kgC kg-1", "get_SoilOrganicC" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilOrganicCarbon(); }, 4);
      });

      build({ id++, "SOC-X-Y", "gC m-2", "SOC-X-Y" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i)
        {
          return monica.soilColumn().at(i).vs_SoilOrganicCarbon()
            * monica.soilColumn().at(i).vs_SoilBulkDensity()
            * monica.soilColumn().at(i).vs_LayerThickness
            * 1000;
        }, 4);
      });

      build({ id++, "OrgN", "kg N m-3", "get_Organic_N" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_Organic_N(i); }, 4);
      });

      build({ id++, "AOMf", "kgC m-3", "get_AOM_FastSum" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_AOM_FastSum(i); }, 4);
      });

      build({ id++, "AOMs", "kgC m-3", "get_AOM_SlowSum" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_AOM_SlowSum(i); }, 4);
      });

      build({ id++, "SMBf", "kgC m-3", "get_SMB_Fast" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SMB_Fast(i); }, 4);
      });

      build({ id++, "SMBs", "kgC m-3", "get_SMB_Slow" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SMB_Slow(i); }, 4);
      });

      build({ id++, "SOMf", "kgC m-3", "get_SOM_Fast" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SOM_Fast(i); }, 4);
      });

      build({ id++, "SOMs", "kgC m-3", "get_SOM_Slow" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SOM_Slow(i); }, 4);
      });

      build({ id++, "CBal", "kgC m-3", "get_CBalance" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_CBalance(i); }, 4);
      });

      build({ id++, "Nmin", "kgN ha-1", "NetNMineralisationRate" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_NetNMineralisationRate(i); }, 6);
      });

      build({ id++, "NetNmin", "kgN ha-1", "NetNmin" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_NetNMineralisation(), 5);
      });

      build({ id++, "Denit", "kgN ha-1", "Denit" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_Denitrification(), 5);
      });

    build({ id++, "N2O", "kgN ha-1", "N2O" },
      [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_N2O_Produced(), 5);
      });
    build({ id++, "N2Onit", "kgN ha-1", "N2O from nitrification" },
      [](const MonicaModel& monica, OId oid) {
        return round(monica.soilOrganic().get_N2O_Produced_Nit(), 5);
      });
    build({ id++, "N2Odenit", "kgN ha-1", "N2O from denitrification" },
      [](const MonicaModel& monica, OId oid) {
        return round(monica.soilOrganic().get_N2O_Produced_Denit(), 5);
      });

      build({ id++, "SoilpH", "", "SoilpH" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilColumn().at(0).get_SoilpH(), 1);
      });

      build({ id++, "NEP", "kgC ha-1", "NEP" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_NetEcosystemProduction(), 5);
      });

      build({ id++, "NEE", "kgC ha-", "NEE" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_NetEcosystemExchange(), 5);
      });

      build({ id++, "Rh", "kgC ha-", "Rh" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_DecomposerRespiration(), 5);
      });

      build({ id++, "Tmin", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::tmin);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Tavg", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::tavg);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Tmax", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::tmax);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Tmax>=40", "0|1", "if Tmax >= 40�C then 1 else 0" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::tmax);
        return ci == cd.end() ? 0 : (ci->second >= 40 ? 1 : 0);
      });

      build({ id++, "Precip", "mm", "Precipitation" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::precip);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Wind", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::wind);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Globrad", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::globrad);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Relhumid", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::relhumid);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "Sunhours", "", "" },
        [](const MonicaModel& monica, OId oid)
      {
        const auto& cd = monica.currentStepClimateData();
        auto ci = cd.find(Climate::sunhours);
        return ci == cd.end() ? 0.0 : round(ci->second, 4);
      });

      build({ id++, "BedGrad", "0;1", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilMoisture().get_PercentageSoilCoverage(), 3);
      });

      build({ id++, "N", "kgN m-3", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).get_SoilNmin(); }, 3);
      });

      build({ id++, "Co", "kgC m-3", "" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SoilOrganicC(i); }, 2);
      });

      build({ id++, "NH3", "kgN ha-1", "NH3_Volatilised" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.soilOrganic().get_NH3_Volatilised(), 3);
      });

      build({ id++, "NFert", "kgN ha-1", "dailySumFertiliser" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.dailySumFertiliser(), 1);
      });

      build({ id++, "SumNFert", "kgN ha-1", "sum of N fertilizer applied during cropping period" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.sumFertiliser(), 1);
      });

      build({ id++, "NOrgFert", "kgN ha-1", "dailySumOrgFertiliser" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.dailySumOrgFertiliser(), 1);
      });

      build({ id++, "SumNOrgFert", "kgN ha-1", "sum of N of organic fertilizer applied during cropping period" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.sumOrgFertiliser(), 1);
      });


      build({id++, "WaterContent", "%nFC", "soil water content in % of available soil water"},
            [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i)
        {
          double smm3 = monica.soilMoisture().get_SoilMoisture(i);
          double fc = monica.soilColumn().at(i).vs_FieldCapacity();
          double pwp = monica.soilColumn().at(i).vs_PermanentWiltingPoint();
          return (smm3 - pwp) / (fc - pwp); //[%nFK]
        }, 4);
      });

      build({ id++, "AWC", "m3 m-3", "available water capacity" },
        [](const MonicaModel& monica, OId oid)
        {
          return getComplexValues<double>(oid, [&](int i)
            {
              double fc = monica.soilColumn().at(i).vs_FieldCapacity();
              double pwp = monica.soilColumn().at(i).vs_PermanentWiltingPoint();
              return fc - pwp; 
            }, 4);
        });

      build({id++, "CapillaryRise", "mm", "capillary rise"},
            [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilMoisture().get_CapillaryRise(i); }, 3);
      });

      build({ id++, "PercolationRate", "mm", "percolation rate" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilMoisture().get_PercolationRate(i); }, 3);
      });

      build({ id++, "SMB-CO2-ER", "", "soilOrganic.get_SMB_CO2EvolutionRate" },
        [](const MonicaModel& monica, OId oid)
      {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().get_SMB_CO2EvolutionRate(i); }, 1);
      });

      build({ id++, "Evapotranspiration", "mm", "Remaining evapotranspiration" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.getEvapotranspiration(), 1);
      });

      build({ id++, "Evaporation", "mm", "evaporation from intercepted water" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.getEvaporation(), 1);
      });

      build({ id++, "ETa/ETc", "", "Act_ET / Pot_ET (actual evapotranspiration / potential evapotranspiration)" },
        [](const MonicaModel& monica, OId oid)
      {
        auto potET = monica.soilMoisture().get_PotentialEvapotranspiration();
        return potET > 0 ? round(monica.getETa() / potET, 2) : 1.0;
      });

      build({ id++, "Transpiration", "mm", "" },
        [](const MonicaModel& monica, OId oid)
      {
        return round(monica.getTranspiration(), 1);
      });

      build({ id++, "GrainN", "kg ha-1", "get_FruitBiomassNContent" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_FruitBiomassNContent(), 5) : 0.0;
      });




      build({ id++, "Fc", "m3 m-3", "field capacity" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_FieldCapacity(); }, 4);
      });

      build({ id++, "Pwp", "m3 m-3", "permanent wilting point" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_PermanentWiltingPoint(); }, 4);
      });

      build({ id++, "Sat", "m3 m-3", "saturation" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_Saturation(); }, 4);
      });

      build({ id++, "guenther-isoprene-emission", "umol m-2Ground d-1", "daily isoprene-emission of all species from Guenther model" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->guentherEmissions().isoprene_emission, 5) : 0.0;
      });

      build({ id++, "guenther-monoterpene-emission", "umol m-2Ground d-1", "daily monoterpene emission of all species from Guenther model" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->guentherEmissions().monoterpene_emission, 5) : 0.0;
      });

      build({ id++, "jjv-isoprene-emission", "umol m-2Ground d-1", "daily isoprene-emission of all species from JJV model" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->jjvEmissions().isoprene_emission, 5) : 0.0;
      });

      build({ id++, "jjv-monoterpene-emission", "umol m-2Ground d-1", "daily monoterpene emission of all species from JJV model" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->jjvEmissions().monoterpene_emission, 5) : 0.0;
      });

      build({ id++, "Nresid", "kg N ha-1", "Nitrogen content in crop residues" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_ResiduesNContent(), 1) : 0.0;
      });

      build({ id++, "Sand", "kg kg-1", "Soil sand content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilSandContent(); }, 2);
      });

      build({ id++, "Clay", "kg kg-1", "Soil clay content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilClayContent(); }, 2);
      });

      build({ id++, "Silt", "kg kg-1", "Soil silt content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilSiltContent(); }, 2);
      });

      build({ id++, "Stone", "kg kg-1", "Soil stone content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilStoneContent(); }, 2);
      });

      build({ id++, "pH", "kg kg-1", "Soil pH content" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilColumn().at(i).vs_SoilpH(); }, 2);
      });

      build({ id++, "O3-short-damage", "unitless", "short term ozone induced reduction of Ac" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_O3_shortTermDamage(), 2) : 0.0;
      });

      build({ id++, "O3-long-damage", "unitless", "long term ozone induced senescence" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_O3_longTermDamage(), 2) : 0.0;
      });

      build({ id++, "O3-WS-gs-reduction", "unitless", "water stress impact on stomatal conductance" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_O3_WStomatalClosure(), 2) : 0.0;
      });

      build({ id++, "O3-total-uptake", "�mol m-2", "total O3 uptake" }, //TODO units are not correct
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->get_O3_sumUptake(), 2) : 0.0;
      });

      build({ id++, "NO3conv", "", "get_vq_Convection" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilTransport().get_vq_Convection(i); }, 8);
      });

      build({ id++, "NO3disp", "", "get_vq_Dispersion" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) { return monica.soilTransport().get_vq_Dispersion(i); }, 8);
      });

      build({ id++, "noOfAOMPools", "", "number of AOM pools in existence currently" },
        [](const MonicaModel& monica, OId oid)
      {
        return int(monica.soilColumn().at(0).vo_AOM_Pool.size());
      });

      build({ id++, "CN_Ratio_AOM_Fast", "", "CN_Ratio_AOM_Fast" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) {
          const auto& layer = monica.soilColumn().at(i);
          return layer.vo_AOM_Pool.empty() ? 0.0 : layer.vo_AOM_Pool.at(0).vo_CN_Ratio_AOM_Fast;
        }, 5);
      });

      build({ id++, "AOM_Fast", "", "AOM_Fast" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) {
          const auto& layer = monica.soilColumn().at(i);
          return layer.vo_AOM_Pool.empty() ? 0.0 : layer.vo_AOM_Pool.at(0).vo_AOM_Fast;
        }, 5);
      });

      build({ id++, "AOM_Slow", "", "AOM_Slow" },
        [](const MonicaModel& monica, OId oid)
      {
        return getComplexValues<double>(oid, [&](int i) {
          const auto& layer = monica.soilColumn().at(i);
          return layer.vo_AOM_Pool.empty() ? 0.0 : layer.vo_AOM_Pool.at(0).vo_AOM_Slow;
        }, 5);
      });

      build({ id++, "rootNConcentration", "", "rootNConcentration" },
        [](const MonicaModel& monica, OId oid)
      {
        return monica.cropGrowth() ? round(monica.cropGrowth()->rootNConcentration(), 4) : 0.0;
      });
    build({ id++, "actammoxrate", "kgN/m3/d", "" },
      [](const MonicaModel& monica, OId oid) {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().actAmmoniaOxidationRate(i); }, 6);
      });

    build({ id++, "actnitrate", "kgN/m3/d", "" },
      [](const MonicaModel& monica, OId oid) {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().actNitrificationRate(i); }, 6);
      });

    build({ id++, "actdenitrate", "kgN/m3/d", "" },
      [](const MonicaModel& monica, OId oid) {
        auto nools = int(monica.soilColumn().vs_NumberOfOrganicLayers());
        oid.fromLayer = min(oid.fromLayer, nools - 1);
        oid.toLayer = min(oid.toLayer, nools - 1);
        return getComplexValues<double>(oid, [&](int i) { return monica.soilOrganic().actDenitrificationRate(i); }, 6);
      });
      build({ id++, "rootDensity", "", "cropGrowth->vc_RootDensity" },
    [](const MonicaModel& monica, OId oid) 
        {
      return getComplexValues<double>(oid, [&](int i) {
      return monica.cropGrowth() ? monica.cropGrowth()->getRootDensity(i) : 0.0; }, 4);
    });
    build({ id++, "rootingZone", "", "cropGrowth->vc_RootingZone" },
    [](const MonicaModel& monica, OId oid) {
      return monica.cropGrowth() ? monica.cropGrowth()->rootingZone() : 0.0;
    });
      build({ id++, "WaterFlux", "mm/d", "waterflux in layer" },
        [](const MonicaModel& monica, OId oid)
        {
          return getComplexValues<double>(oid, [&](int i)
            {
              return monica.soilMoisture().waterFlux(i);
            }, 1);
        });

#if MONICA_SOILTEMP
      build({ id++, "AMEI_Monica_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return round(const_cast<MonicaModel&>(monica)._instance_Monica_SoilTemp->soilTempState.getsoilSurfaceTemperature(), 6);
            });
      build({ id++, "AMEI_Monica_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_Monica_SoilTemp->soilTempState.getsoilTemperature().at(i); }, 6);
            });
#endif

#if DSSAT_ST_STANDALONE
      build({ id++, "AMEI_DSSAT_ST_standalone_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return round(const_cast<MonicaModel&>(monica)._instance_DSSAT_ST_standalone->_soilTempState.getSRFTEMP(), 6);
            });
      build({ id++, "AMEI_DSSAT_ST_standalone_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_DSSAT_ST_standalone->_soilTempState.getST().at(i); }, 6);
            });
#endif

#if DSSAT_EPICST_STANDALONE
      build({ id++, "AMEI_DSSAT_EPICST_standalone_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return round(const_cast<MonicaModel&>(monica)._instance_DSSAT_EPICST_standalone->_soilTempState.getSRFTEMP(), 6);
            });
      build({ id++, "AMEI_DSSAT_EPICST_standalone_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_DSSAT_EPICST_standalone->_soilTempState.getST().at(i); }, 6);
            });
#endif

#if SIMPLACE_SOIL_TEMPERATURE
      build({ id++, "AMEI_Simplace_Soil_Temperature_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return round(const_cast<MonicaModel&>(monica)._instance_Simplace_Soil_Temperature->_soilTempState.getSoilSurfaceTemperature(), 6);
            });
      build({ id++, "AMEI_Simplace_Soil_Temperature_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
                return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_Simplace_Soil_Temperature->_soilTempState.getSoilTempArray().at(i); }, 6);
            });
#endif

#if STICS_SOIL_TEMPERATURE
      build({ id++, "AMEI_Stics_soil_temperature_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_Stics_soil_temperature->soilTempState.getcanopy_temp_avg(), 6);
            });
      build({ id++, "AMEI_Stics_soil_temperature_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_Stics_soil_temperature->soilTempState.getlayer_temp().at(i); }, 6);
            });
#endif

#if SQ_SOIL_TEMPERATURE
      build({ id++, "AMEI_SQ_Soil_Temperature_SoilTemp_deep", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_SQ_Soil_Temperature->_soilTempState.getdeepLayerT(), 6);
            });
      build({ id++, "AMEI_SQ_Soil_Temperature_SoilTemp_min", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_SQ_Soil_Temperature->_soilTempState.getminTSoil(), 6);
            });
      build({ id++, "AMEI_SQ_Soil_Temperature_SoilTemp_max", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_SQ_Soil_Temperature->_soilTempState.getmaxTSoil(), 6);
            });
#endif

#if BIOMASURFACEPARTONSOILSWATC
      build({ id++, "AMEI_BiomaSurfacePartonSoilSWATC_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_BiomaSurfacePartonSoilSWATC->_soilTempAux.getSurfaceSoilTemperature(), 6);
            });
      build({ id++, "AMEI_BiomaSurfacePartonSoilSWATC_SurfTemp_min", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_BiomaSurfacePartonSoilSWATC->_soilTempAux.getSurfaceTemperatureMinimum(), 6);
            });
      build({ id++, "AMEI_BiomaSurfacePartonSoilSWATC_SurfTemp_max", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_BiomaSurfacePartonSoilSWATC->_soilTempAux.getSurfaceTemperatureMaximum(), 6);
            });
      build({ id++, "AMEI_BiomaSurfacePartonSoilSWATC_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_BiomaSurfacePartonSoilSWATC->_soilTempState.getSoilTemperatureByLayers().at(i); }, 6);
            });
#endif

#if BIOMASURFACESWATSOILSWATC
      build({ id++, "AMEI_BiomaSurfaceSWATSoilSWATC_SurfTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return round(const_cast<MonicaModel&>(monica)._instance_BiomaSurfaceSWATSoilSWATC->_soilTempAux.getSurfaceSoilTemperature(), 6);
            });

      build({ id++, "AMEI_BiomaSurfaceSWATSoilSWATC_SoilTemp", "°C", "" },
            [](const MonicaModel& monica, OId oid){
              return getComplexValues<double>(oid, [&](int i) { return const_cast<MonicaModel&>(monica)._instance_BiomaSurfaceSWATSoilSWATC->_soilTempState.getSoilTemperatureByLayers().at(i); }, 6);
            });
#endif

      tableBuilt = true;
    }
  }

  return m;
}

std::function<bool(double, double)> monica::getCompareOp(std::string ops)
{
  function<bool(double, double)> op = [](double, double) { return false; };

  if (ops == "<")
    op = [](double l, double r) { return l < r; };
  else if (ops == "<=")
    op = [](double l, double r) { return l <= r; };
  else if (ops == "=")
    op = [](double l, double r) { return l == r; };
  else if (ops == "!=")
    op = [](double l, double r) { return l != r; };
  else if (ops == ">")
    op = [](double l, double r) { return l > r; };
  else if (ops == ">=")
    op = [](double l, double r) { return l >= r; };

  return op;
}

bool monica::applyCompareOp(std::function<bool(double, double)> op, Json lj, Json rj)
{
  if (lj.is_number() && rj.is_number())
    return op(lj.number_value(), rj.number_value());
  else if (lj.is_array() && rj.is_number())
  {
    double rn = rj.number_value();
    auto lja = lj.array_items();
    return accumulate(lja.begin(), lja.end(), true, [=](bool acc, Json j)
    {
      return acc && (j.is_number() ? op(j.number_value(), rn) : false);
    });
  }
  else if (lj.is_number() && rj.is_array())
  {
    double ln = lj.number_value();
    auto rja = rj.array_items();
    return accumulate(rja.begin(), rja.end(), true, [=](bool acc, Json j)
    {
      return acc && (j.is_number() ? op(j.number_value(), ln) : false);
    });
  }
  else if (lj.is_array() && rj.is_array())
  {
    auto lja = lj.array_items();
    auto rja = rj.array_items();
    vector<bool> res;
    //compare values point wise (dot product)
    transform(lja.begin(), lja.end(), rja.begin(), back_inserter(res), [=](Json left, Json right)
    {
      return left.is_number() && right.is_number() ? op(left.number_value(), right.number_value()) : false;
    });
    return accumulate(res.begin(), res.end(), true, [](bool acc, bool v) { return acc && v; });
  }
  return false;
}

std::function<double(double, double)> monica::getPrimitiveCalcOp(std::string ops)
{
  function<double(double, double)> op = [](double, double) { return 0.0; };

  if (ops == "+")
    op = [](double l, double r) { return l + r; };
  else if (ops == "-")
    op = [](double l, double r) { return l - r; };
  else if (ops == "*")
    op = [](double l, double r) { return l * r; };
  else if (ops == "/")
    op = [](double l, double r) { return l / r; };

  return op;
}

json11::Json monica::applyPrimitiveCalcOp(std::function<double(double, double)> op, json11::Json lj, json11::Json rj)
{
  if (lj.is_number() && rj.is_number())
    return op(lj.number_value(), rj.number_value());
  else if (lj.is_array() && rj.is_number())
  {
    double rn = rj.number_value();
    auto lja = lj.array_items();
    vector<double> res;
    for (auto left : lja)
      res.push_back(left.is_number() ? op(left.number_value(), rn) : 0.0);
    return toPrimJsonArray(res);
  }
  else if (lj.is_number() && rj.is_array())
  {
    double ln = lj.number_value();
    auto rja = rj.array_items();
    vector<double> res;
    for (auto right : rja)
      res.push_back(right.is_number() ? op(ln, right.number_value()) : 0.0);
    return toPrimJsonArray(res);
  }
  else if (lj.is_array() && rj.is_array())
  {
    auto lja = lj.array_items();
    auto rja = rj.array_items();
    vector<bool> res;
    //compare values point wise (dot product)
    transform(lja.begin(), lja.end(), rja.begin(), back_inserter(res), [=](Json left, Json right)
    {
      return left.is_number() && right.is_number() ? op(left.number_value(), right.number_value()) : 0.0;
    });
    return toPrimJsonArray(res);
  }
  return 0.0;
}