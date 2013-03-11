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
