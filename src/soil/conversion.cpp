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

#include "conversion.h"

#include  <iostream>

using namespace Soil;
using namespace std;
using namespace Tools;

EResult<double> Soil::humusClass2corg(int humusClass) {
  switch (humusClass) {
    case 0:
      return 0.0;
    case 1:
      return 0.5 / 1.72;
    case 2:
      return 1.5 / 1.72;
    case 3:
      return 3.0 / 1.72;
    case 4:
      return 6.0 / 1.72;
    case 5:
      return 11.5 / 2.0;
    case 6:
      return 17.5 / 2.0;
    case 7:
      return 30.0 / 2.0;
  }
  return {0.0, string("Soil::humusClass2corg: Unknown humus class: " + to_string(humusClass) + "!")};
}

EResult<double> Soil::bulkDensityClass2rawDensity(int bulkDensityClass, double clay) {
  EResult<double> res;
  double x = 0.0;

  switch (bulkDensityClass) {
    case 1:
      x = 1.3;
      break;
    case 2:
      x = 1.5;
      break;
    case 3:
      x = 1.7;
      break;
    case 4:
      x = 1.9;
      break;
    case 5:
      x = 2.1;
      break;
    default:
      res.errors.push_back(string(
          "Soil::bulkDensityClass2rawDensity: Unknown bulk density class: " + to_string(bulkDensityClass) + "!"));
  }

  res.result = (x - (0.9 * clay)) * 1000.0; //* 1000 = conversion from g cm-3 -> kg m-3
  return res;
}

double Soil::sandAndClay2lambda(double sand, double clay) {
  double lambda = (2.0 * (sand * sand * 0.575)) + (clay * 0.1) + ((1.0 - sand - clay) * 0.35);
  // lambda = 1.0; //! @todo <b>Claas:</b> Temporary override until we have solved the problem with low water percolation loam soils
  return lambda;
}

std::string percentSandAndClayToKA5Texture(uint8_t sand, uint8_t clay) {
  std::string soil_texture;
  uint8_t silt = 100 - sand - clay;

  if (silt >= 0 && silt < 10 && clay >= 0.0 && clay < 5) {
    soil_texture = "SS"; // SS silt 0-10% clay 0-5%
  } else if (silt >= 0 && silt < 10 && clay >= 5 && clay < 17) {
    soil_texture = "ST2"; // ST2 silt 0-10% clay 5-17%
  } else if (silt >= 0 && silt < 15 && clay >= 17 && clay < 25) {
    soil_texture = "ST3"; // ST3 silt 0-15% clay 17-25%
  } else if (silt >= 10 && silt < 25 && clay >= 0 && clay < 5) {
    soil_texture = "SU2"; // SU2 silt 10-25% clay 0-5%
  } else if (silt >= 25 && silt < 40 && clay >= 0 && clay < 8) {
    soil_texture = "SU3"; // SU3 silt 25-40% clay 0-8%
  } else if (silt >= 40 && silt < 50 && clay >= 0 && clay < 8) {
    soil_texture = "SU4"; // SU4 silt 40-50% clay 0-8%
  } else if (silt >= 10 && silt < 25 && clay >= 5 && clay < 8) {
    soil_texture = "SL2"; // SL2 silt 10-25% clay 5-8%
  } else if (silt >= 10 && silt < 40 && clay >= 8 && clay < 12) {
    soil_texture = "SL3"; // SL3 silt 10-40% clay 8-12%
  } else if (silt >= 10 && silt < 40 && clay >= 12 && clay < 17) {
    soil_texture = "SL4"; // SL4 silt 10-40% clay 12-17%
  } else if (silt >= 40 && silt < 50 && clay >= 8 && clay < 17) {
    soil_texture = "SLU"; // SLU silt 40-50% clay 8-17%
  } else if (silt >= 40 && silt < 50 && clay >= 17 && clay < 25) {
    soil_texture = "LS2"; // LS2 silt 40-50% clay 17-25%
  } else if (silt >= 30 && silt < 40 && clay >= 17 && clay < 25) {
    soil_texture = "LS3"; // LS3 silt 30-40% clay 17-25%
  } else if (silt >= 15 && silt < 30 && clay >= 17 && clay < 25) {
    soil_texture = "LS4"; // LS4 silt 15-30% clay 17-25%
  } else if (silt >= 30 && silt < 50 && clay >= 25 && clay < 35) {
    soil_texture = "LT2"; // LT2 silt 30-50% clay 25-35%
  } else if (silt >= 30 && silt < 50 && clay >= 35 && clay < 45) {
    soil_texture = "LT3"; // LT3 silt 30-50% clay 35-45%
  } else if (silt >= 15 && silt < 30 && clay >= 25 && clay < 45) {
    soil_texture = "LTS"; // LTS silt 15-30% clay 25-45%
  } else if (silt >= 50 && silt < 65 && clay >= 17 && clay < 30) {
    soil_texture = "LU"; // LU silt 50-65% clay 17-30%
  } else if (silt >= 50 && silt < 65 && clay >= 8 && clay < 17) {
    soil_texture = "ULS"; // ULS silt 50-65% clay 8-17%
  } else if (silt >= 50 && silt < 80 && clay >= 0 && clay < 8) {
    soil_texture = "US"; // US silt 50-80% clay 0-8%
  } else if (silt >= 80 && clay >= 0 && clay < 8) {
    soil_texture = "UU"; // UU silt >80% clay 0-8%
  } else if (silt >= 65 && clay >= 8 && clay < 12) {
    soil_texture = "UT2"; // UT2 silt >65% clay 8-12%
  } else if (silt >= 65 && clay >= 12 && clay < 17) {
    soil_texture = "UT3"; // UT3 silt >65% clay 12-17%
  } else if (silt >= 65 && clay >= 17 && clay < 25) {
    soil_texture = "UT4"; // UT4 silt >65% clay 17-25%
  } else if (silt >= 0 && silt < 15 && clay >= 45 && clay < 65) {
    soil_texture = "TS2"; // TS2 silt 0-15% clay 45-65%
  } else if (silt >= 0 && silt < 15 && clay >= 35 && clay < 45) {
    soil_texture = "TS3"; // TS3 silt 0-15% clay 35-45%
  } else if (silt >= 0 && silt < 15 && clay >= 25 && clay < 35) {
    soil_texture = "TS4"; // TS4 silt 0-15% clay 25-35%
  } else if (silt >= 15 && silt < 30 && clay >= 45 && clay < 65) {
    soil_texture = "TL"; // TL silt 15-30% clay 45-65%
  } else if (silt >= 50 && silt < 65 && clay >= 30 && clay < 45) {
    soil_texture = "TU3"; // TU3 silt 50-65% clay 30-45%
  } else if (silt >= 30 && clay >= 45 && clay < 65) {
    soil_texture = "TU2"; // TU2 silt > 30% clay 45-65%
  } else if (silt >= 65 && clay >= 25) {
    soil_texture = "TU4"; // TU4 silt > 65% clay >25%
  } else if (clay >= 65) soil_texture = "TT"; // TT clay > 65
  return soil_texture;
}

string Soil::sandAndClay2KA5texture(double sand, double clay) {
  return percentSandAndClayToKA5Texture(int(sand * 100.0), int(clay * 100.0));
}

EResult<double> Soil::KA5texture2sand(string soilType) {
  EResult<double> res;
  double &x = res.result;
  soilType = Tools::toUpper(soilType);

  if (soilType == "FS") { x = 0.84; }
  else if (soilType == "FSMS") { x = 0.86; }
  else if (soilType == "FSGS") { x = 0.88; }
  else if (soilType == "GS") { x = 0.93; }
  else if (soilType == "MSGS") { x = 0.96; }
  else if (soilType == "MSFS") { x = 0.93; }
  else if (soilType == "MS") { x = 0.96; }
  else if (soilType == "SS") { x = 0.93; }
  else if (soilType == "SL2") { x = 0.76; }
  else if (soilType == "SL3") { x = 0.65; }
  else if (soilType == "SL4") { x = 0.60; }
  else if (soilType == "SLU") { x = 0.43; }
  else if (soilType == "ST2") { x = 0.84; }
  else if (soilType == "ST3") { x = 0.71; }
  else if (soilType == "SU2") { x = 0.80; }
  else if (soilType == "SU3") { x = 0.63; }
  else if (soilType == "SU4") { x = 0.56; }
  else if (soilType == "LS2") { x = 0.34; }
  else if (soilType == "LS3") { x = 0.44; }
  else if (soilType == "LS4") { x = 0.56; }
  else if (soilType == "LT2") { x = 0.30; }
  else if (soilType == "LT3") { x = 0.20; }
  else if (soilType == "LTS") { x = 0.42; }
  else if (soilType == "LU") { x = 0.19; }
  else if (soilType == "UU") { x = 0.10; }
  else if (soilType == "ULS") { x = 0.30; }
  else if (soilType == "US") { x = 0.31; }
  else if (soilType == "UT2") { x = 0.13; }
  else if (soilType == "UT3") { x = 0.11; }
  else if (soilType == "UT4") { x = 0.09; }
  else if (soilType == "UTL") { x = 0.19; }
  else if (soilType == "TT") { x = 0.17; }
  else if (soilType == "TL") { x = 0.17; }
  else if (soilType == "TU2") { x = 0.12; }
  else if (soilType == "TU3") { x = 0.10; }
  else if (soilType == "TS3") { x = 0.52; }
  else if (soilType == "TS2") { x = 0.37; }
  else if (soilType == "TS4") { x = 0.62; }
  else if (soilType == "TU4") { x = 0.05; }
  else if (soilType == "L") { x = 0.35; }
  else if (soilType == "S") { x = 0.93; }
  else if (soilType == "U") { x = 0.10; }
  else if (soilType == "T") { x = 0.17; }
  else if (soilType == "HZ1") { x = 0.30; }
  else if (soilType == "HZ2") { x = 0.30; }
  else if (soilType == "HZ3") { x = 0.30; }
  else if (soilType == "HH") { x = 0.15; }
  else if (soilType == "HN") { x = 0.15; }
  else {
    x = 0.66;
    res.errors.push_back(string("Soil::KA5texture2sand Unknown soil type: " + soilType + "!"));
  }

  return res;
}


EResult<double> Soil::KA5texture2clay(string soilType) {
  EResult<double> res;
  double &x = res.result;
  soilType = Tools::toUpper(soilType);

  if (soilType == "FS") { x = 0.02; }
  else if (soilType == "FSMS") { x = 0.02; }
  else if (soilType == "FSGS") { x = 0.02; }
  else if (soilType == "GS") { x = 0.02; }
  else if (soilType == "MSGS") { x = 0.02; }
  else if (soilType == "MSFS") { x = 0.02; }
  else if (soilType == "MS") { x = 0.02; }
  else if (soilType == "SS") { x = 0.02; }
  else if (soilType == "SL2") { x = 0.06; }
  else if (soilType == "SL3") { x = 0.10; }
  else if (soilType == "SL4") { x = 0.14; }
  else if (soilType == "SLU") { x = 0.12; }
  else if (soilType == "ST2") { x = 0.11; }
  else if (soilType == "ST3") { x = 0.21; }
  else if (soilType == "SU2") { x = 0.02; }
  else if (soilType == "SU3") { x = 0.04; }
  else if (soilType == "SU4") { x = 0.04; }
  else if (soilType == "LS2") { x = 0.21; }
  else if (soilType == "LS3") { x = 0.21; }
  else if (soilType == "LS4") { x = 0.21; }
  else if (soilType == "LT2") { x = 0.30; }
  else if (soilType == "LT3") { x = 0.40; }
  else if (soilType == "LTS") { x = 0.35; }
  else if (soilType == "LU") { x = 0.23; }
  else if (soilType == "UU") { x = 0.04; }
  else if (soilType == "ULS") { x = 0.12; }
  else if (soilType == "US") { x = 0.04; }
  else if (soilType == "UT2") { x = 0.10; }
  else if (soilType == "UT3") { x = 0.14; }
  else if (soilType == "UT4") { x = 0.21; }
  else if (soilType == "UTL") { x = 0.23; }
  else if (soilType == "TT") { x = 0.82; }
  else if (soilType == "TL") { x = 0.55; }
  else if (soilType == "TU2") { x = 0.55; }
  else if (soilType == "TU3") { x = 0.37; }
  else if (soilType == "TS3") { x = 0.40; }
  else if (soilType == "TS2") { x = 0.55; }
  else if (soilType == "TS4") { x = 0.30; }
  else if (soilType == "TU4") { x = 0.30; }
  else if (soilType == "L") { x = 0.31; }
  else if (soilType == "S") { x = 0.02; }
  else if (soilType == "U") { x = 0.04; }
  else if (soilType == "T") { x = 0.82; }
  else if (soilType == "HZ1") { x = 0.15; }
  else if (soilType == "HZ2") { x = 0.15; }
  else if (soilType == "HZ3") { x = 0.15; }
  else if (soilType == "HH") { x = 0.1; }
  else if (soilType == "HN") { x = 0.1; }
  else {
    x = 0.0;
    res.errors.push_back(string("Soil::KA5texture2clay: Unknown soil type: " + soilType + "!"));
  }

  return res;
}
