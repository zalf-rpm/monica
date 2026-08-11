/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>
Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>

Maintainers:
Currently maintained by the authors.

This file is part of the MONICA model.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include "cultivation-method.h"

#include <algorithm>
#include <sstream>

#include "../core/monica-model.h"
#include "json11/json11-helper.h"
#include "tools/debug.h"

using namespace std;
using namespace monica;
using namespace Tools;

CultivationMethod monica::makeCultivationMethod(json11::Json j) {
  CultivationMethod cm;
  // NOTE: like the original CultivationMethod(json11::Json) constructor, the
  // merge() result (Errors) is not stored anywhere - discarded, not a mistake,
  // matches the original exactly.
  cultivationmethod::merge(&cm, j);
  return cm;
}

Errors cultivationmethod::merge(CultivationMethod *cm, json11::Json j) {
  Errors res;

  set_int_value(cm->customId, j, "customId");
  set_string_value(cm->name, j, "name");
  set_bool_value(cm->canBeSkipped, j, "can-be-skipped");
  set_bool_value(cm->isCoverCrop, j, "is-cover-crop");
  set_bool_value(cm->repeat, j, "repeat");

  // keep reference to sowing workstep for use with harvest workstep
  SowingData *sowingWS = nullptr;

  for (auto wsj : j["worksteps"].array_items()) {
    auto ws = makeWorkstep(wsj);
    if (!ws)
      continue;
    res.append(ws->errors);
    cm->allWorksteps.push_back(ws);
    switch (workstep::type(ws.get())) {
    case WorkstepType::SOWING:
      sowingWS = &std::get<SowingData>(ws->data);
      break;
    case WorkstepType::AUTOMATIC_SOWING:
      sowingWS = &std::get<AutomaticSowingData>(ws->data);
      break;
    case WorkstepType::HARVEST:
      if (sowingWS)
        std::get<HarvestData>(ws->data).sowing = sowingWS;
      break;
    case WorkstepType::AUTOMATIC_HARVEST:
      if (sowingWS)
        std::get<AutomaticHarvestData>(ws->data).sowing = sowingWS;
      break;
    default:
      break;
    }
  }

  return res;
}

json11::Json cultivationmethod::to_json(const CultivationMethod *cm) {
  auto wss = J11Array();
  for (auto ws : cm->allWorksteps)
    wss.push_back(workstep::to_json(ws.get()));

  return J11Object{{"type", "CultivationMethod"},
                   {"customId", cm->customId},
                   {"name", cm->name},
                   {"can-be-skipped", cm->canBeSkipped},
                   {"is-cover-crop", cm->isCoverCrop},
                   {"repeat", cm->repeat},
                   {"worksteps", wss}};
}

void cultivationmethod::apply(const CultivationMethod *cm, const Date &date,
                              MonicaModel *model) {
  for (auto ws : workstepsAt(cm, date))
    workstep::apply(ws.get(), model);
}

void cultivationmethod::absApply(const CultivationMethod *cm, const Date &date,
                                 MonicaModel *model) {
  for (auto ws : absWorkstepsAt(cm, date))
    workstep::apply(ws.get(), model);
}

void cultivationmethod::apply(CultivationMethod *cm, MonicaModel *model,
                              bool runOnlyAtStartOfDayWorksteps) {
  auto &udws = cm->unfinishedDynamicWorksteps;
  udws.erase(
      remove_if(udws.begin(), udws.end(),
                [model, runOnlyAtStartOfDayWorksteps](WSPtr wsp) {
                  return runOnlyAtStartOfDayWorksteps == wsp->runAtStartOfDay &&
                         workstep::applyWithPossibleCondition(wsp.get(), model);
                }),
      udws.end());
}

Date cultivationmethod::nextDate(const CultivationMethod *cm,
                                 const Date &date) {
  for (auto ws : cm->allWorksteps) {
    auto d = ws->date;
    if (d.isValid() && d > date)
      return d;
  }
  return Date();
}

Date cultivationmethod::nextAbsDate(const CultivationMethod *cm,
                                    const Date &date) {
  for (auto ws : cm->allAbsWorksteps) {
    auto ad = workstep::absDate(ws.get());
    if (ad.isValid() && ad > date)
      return ad;
  }
  return Date();
}

vector<WSPtr> cultivationmethod::workstepsAt(const CultivationMethod *cm,
                                             const Date &date) {
  vector<WSPtr> apps;
  for (auto ws : cm->allWorksteps)
    if (ws->date.isValid() && ws->date == date)
      apps.push_back(ws);

  return apps;
}

vector<WSPtr> cultivationmethod::absWorkstepsAt(const CultivationMethod *cm,
                                                const Date &date) {
  vector<WSPtr> apps;
  for (auto ws : cm->allAbsWorksteps)
    if (workstep::absDate(ws.get()).isValid() &&
        workstep::absDate(ws.get()) == date)
      apps.push_back(ws);

  return apps;
}

bool cultivationmethod::areOnlyAbsoluteWorksteps(const CultivationMethod *cm) {
  return all_of(cm->allWorksteps.begin(), cm->allWorksteps.end(),
                [](const WSPtr &ws) {
                  return ws->date.isValid() && ws->date.isAbsoluteDate();
                });
}

vector<WSPtr> cultivationmethod::staticWorksteps(const CultivationMethod *cm) {
  vector<WSPtr> wss;
  for (auto ws : cm->allWorksteps)
    if (ws->date.isValid())
      wss.push_back(ws);
  return wss;
}

vector<WSPtr>
cultivationmethod::allDynamicWorksteps(const CultivationMethod *cm) {
  return workstepsAt(cm, Date());
}

bool cultivationmethod::allDynamicWorkstepsFinished(
    const CultivationMethod *cm) {
  if (cm->unfinishedDynamicWorksteps.empty())
    return true;
  else {
    return all_of(cm->unfinishedDynamicWorksteps.begin(),
                  cm->unfinishedDynamicWorksteps.end(), [](const WSPtr &wsp) {
                    return workstep::type(wsp.get()) ==
                           WorkstepType::N_DEMAND_FERTILIZATION;
                  });
  }
}

Date cultivationmethod::startDate(const CultivationMethod *cm) {
  if (cm->allWorksteps.empty())
    return Date();

  auto dynEarliestStart = Date();
  for (auto ws : workstepsAt(cm, Date())) {
    auto ed = workstep::earliestDate(ws.get());
    if ((ed.isValid() && dynEarliestStart.isValid() && ed < dynEarliestStart) ||
        (ed.isValid() && !dynEarliestStart.isValid()))
      dynEarliestStart = ed;
  }

  Date startDate = dynEarliestStart;
  for (auto ws : cm->allWorksteps) {
    auto d = ws->date;
    if (d.isValid() && (d < startDate || !startDate.isValid()))
      startDate = d;
  }

  return startDate;
}

Date cultivationmethod::absStartDate(const CultivationMethod *cm,
                                     bool includeDynamicWorksteps) {
  if (cm->allAbsWorksteps.empty())
    return Date();

  auto dynEarliestStart = Date();
  if (includeDynamicWorksteps) {
    for (auto ws : absWorkstepsAt(cm, Date())) {
      auto ed = workstep::absEarliestDate(ws.get());
      if ((ed.isValid() && dynEarliestStart.isValid() &&
           ed < dynEarliestStart) ||
          (ed.isValid() && !dynEarliestStart.isValid()))
        dynEarliestStart = ed;
    }
  }

  Date startDate = dynEarliestStart;
  for (auto ws : cm->allAbsWorksteps) {
    auto ad = workstep::absDate(ws.get());
    if (ad.isValid() && (ad < startDate || !startDate.isValid()))
      startDate = ad;
  }

  return startDate;
}

Date cultivationmethod::absLatestSowingDate(const CultivationMethod *cm) {
  auto dynLatestSowingDate = Date();
  for (auto ws : cm->allAbsWorksteps) {
    auto t = workstep::type(ws.get());
    if (t == WorkstepType::SOWING || t == WorkstepType::AUTOMATIC_SOWING) {
      auto lsd = workstep::absLatestDate(ws.get());
      if (lsd.isValid() && dynLatestSowingDate < lsd)
        dynLatestSowingDate = lsd;
    }
  }

  return dynLatestSowingDate;
}

Date cultivationmethod::endDate(const CultivationMethod *cm) {
  if (cm->allWorksteps.empty())
    return Date();

  auto dynLatestEnd = Date();
  for (auto ws : workstepsAt(cm, Date())) {
    auto ed = workstep::latestDate(ws.get());
    if ((ed.isValid() && dynLatestEnd.isValid() && ed > dynLatestEnd) ||
        (ed.isValid() && !dynLatestEnd.isValid()))
      dynLatestEnd = ed;
  }

  Date endDate = dynLatestEnd;
  for (auto ws : cm->allWorksteps) {
    auto d = ws->date;
    if (d.isValid() && (d > endDate || !endDate.isValid()))
      endDate = d;
  }

  return endDate;
}

Date cultivationmethod::absEndDate(const CultivationMethod *cm) {
  if (cm->allAbsWorksteps.empty())
    return Date();

  auto dynLatestEnd = Date();
  for (auto ws : absWorkstepsAt(cm, Date())) {
    auto ed = workstep::absLatestDate(ws.get());
    if ((ed.isValid() && dynLatestEnd.isValid() && ed > dynLatestEnd) ||
        (ed.isValid() && !dynLatestEnd.isValid()))
      dynLatestEnd = ed;
  }

  Date endDate = dynLatestEnd;
  for (auto ws : cm->allAbsWorksteps) {
    auto ad = workstep::absDate(ws.get());
    if (ad.isValid() && (ad > endDate || !endDate.isValid()))
      endDate = ad;
  }

  return endDate;
}

std::string cultivationmethod::toString(const CultivationMethod *cm) {
  ostringstream s;
  s << "name: " << cm->name << " start: " << startDate(cm).toString()
    << " end: " << endDate(cm).toString() << endl;
  s << "worksteps:" << endl;
  for (auto p : cm->allWorksteps)
    // p->toString() in the original always fell back to the Json11Serializable
    // default (to_json().dump()), since Workstep never overrode toString()
    // itself - see workstep::to_json.
    s << "at: " << p->date.toString()
      << " what: " << workstep::to_json(p.get()).dump() << endl;
  return s.str();
}

bool cultivationmethod::reinit(CultivationMethod *cm, Tools::Date date,
                               bool forceInitYear) {
  cm->allAbsWorksteps.clear();
  cm->unfinishedDynamicWorksteps.clear();
  bool addedYear = false;
  for (auto ws : cm->allWorksteps) {
    addedYear =
        workstep::reinit(ws.get(), date, addedYear, forceInitYear) || addedYear;
    cm->allAbsWorksteps.push_back(ws);
    if (!workstep::absDate(ws.get()).isValid())
      cm->unfinishedDynamicWorksteps.push_back(ws);
  }

  return addedYear;
}
