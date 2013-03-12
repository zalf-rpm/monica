/**
Authors: 
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the util library used by models created at the Institute of 
Landscape Systems Analysis at the ZALF.
<one line to give the program's name and a brief idea of what it does.>
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

#ifndef MONICAEOM_H
#define MONICAEOM_H

#ifdef TEST_LANDCARE_DSS

#include "typedefs.h"
#include "eom/src/typedefs.h"

namespace Monica
{
  enum TillageType { plough = 1, conserving = 2, noTillage = 3 };

  struct EomPVPInfo
  {
    EomPVPInfo()
      : pvpId(-1), cropId(-1), tillageType(plough),
      crossCropAdaptionFactor(0.0) {}
    Eom::PVPId pvpId;
    CropId cropId;
    TillageType tillageType;
    double crossCropAdaptionFactor;
  };

  EomPVPInfo eomPVPId2cropId(Eom::PVPId pvpId);

  int eomOrganicFertilizerId2monicaOrganicFertilizerId(int eomId);
}

#endif /*#ifdef TEST_LANDCARE_DSS*/

#endif // MONICAEOM_H
