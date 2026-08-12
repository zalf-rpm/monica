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

This file is part of the util library used by models created at the Institute of
Landscape Systems Analysis at the ZALF.
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#pragma once

namespace Soil
{
  //! @author Claas Nendel
  struct OrganicConstants
  {
    static const double po_UreaMolecularWeight; //!< 0.06006; //!< [kg mol-1]
    static const double po_Urea_to_N; //!< 0.46667; ; //!< Converts 1 kg urea to 1 kg N
    static const double po_NH3MolecularWeight; //!< 0.01401; //!< [kg mol-1]
    static const double po_NH4MolecularWeight; //!< 0.01401; //!< [kg mol-1]
    static const double po_H2OIonConcentration; //!< 1.0;
    static const double po_pKaHNO2; //!< 3.29; //!< [] pKa value for nitrous acid
    static const double po_pKaNH3; //!< 6.5; //!< [] pKa value for ammonium
    static const double po_SOM_to_C; //!< 0.57; //!< 0.58; [] converts soil organic matter to carbon
    static const double po_AOM_to_C; //!< 0.45; //!< [] converts added organic matter to carbon
  };
}
