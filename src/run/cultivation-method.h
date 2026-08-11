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

#include <string>
#include <vector>

#include "json11/json11.hpp"

#include "common/dll-exports.h"
#include "tools/date.h"
#include "workstep.h"

namespace monica {
class MonicaModel;

struct DLL_API CultivationMethod {
  std::vector<WSPtr> allWorksteps;
  std::vector<WSPtr> allAbsWorksteps;
  std::vector<WSPtr> unfinishedDynamicWorksteps;
  int customId{0};
  std::string name;
  bool canBeSkipped{false}; //! can this crop be skipped, eg. is a catch or cover crop
  bool isCoverCrop{false}; //! is like canBeSkipped (and implies it), but different rule for when
                           //! cultivation methods will be skipped
  bool repeat{true}; //! if false the cultivation method won't participate in wrapping at the end of
                     //! the crop rotation
};

namespace cultivationmethod {

DLL_API Tools::Errors merge(CultivationMethod *cm, json11::Json j);
DLL_API json11::Json to_json(const CultivationMethod *cm);
DLL_API void apply(const CultivationMethod *cm, const Tools::Date &date, MonicaModel *model);
DLL_API void absApply(const CultivationMethod *cm, const Tools::Date &date, MonicaModel *model);
DLL_API void apply(CultivationMethod *cm, MonicaModel *model, bool runOnlyAtStartOfDayWorksteps);
DLL_API Tools::Date nextDate(const CultivationMethod *cm, const Tools::Date &date);
DLL_API Tools::Date nextAbsDate(const CultivationMethod *cm, const Tools::Date &date);
DLL_API std::vector<WSPtr> workstepsAt(const CultivationMethod *cm, const Tools::Date &date);
DLL_API std::vector<WSPtr> absWorkstepsAt(const CultivationMethod *cm, const Tools::Date &date);
DLL_API bool areOnlyAbsoluteWorksteps(const CultivationMethod *cm);
DLL_API std::vector<WSPtr> staticWorksteps(const CultivationMethod *cm);
DLL_API std::vector<WSPtr> allDynamicWorksteps(const CultivationMethod *cm);
DLL_API bool allDynamicWorkstepsFinished(const CultivationMethod *cm);
DLL_API Tools::Date startDate(const CultivationMethod *cm);
DLL_API Tools::Date absStartDate(const CultivationMethod *cm, bool includeDynamicWorksteps = true);
DLL_API Tools::Date absLatestSowingDate(const CultivationMethod *cm);
DLL_API Tools::Date endDate(const CultivationMethod *cm);
DLL_API Tools::Date absEndDate(const CultivationMethod *cm);
DLL_API std::string toString(const CultivationMethod *cm);
DLL_API bool reinit(CultivationMethod *cm, Tools::Date date, bool forceInitYear = false);

} // namespace cultivationmethod

DLL_API CultivationMethod makeCultivationMethod(json11::Json object);

} // namespace monica
