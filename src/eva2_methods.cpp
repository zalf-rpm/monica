/**
Authors: 
Xenia Specka <xenia.specka@zalf.de>

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

#include <cassert>
#include <vector>
#include <sstream>
#include <ctime>

#include "eva2_methods.h"
#include "monica-parameters.h"
#include "debug.h"

#include "tools/algorithms.h"
#include "db/abstract-db-connections.h"

#define LOKI_OBJECT_LEVEL_THREADING
#include "loki/Threads.h"

using namespace Db;
using namespace std;
using namespace Monica;
using namespace Tools;
using namespace Climate;

/**
 * @brief Lockable object
 */
struct L: public Loki::ObjectLevelLockable<L> {
};

/**
 * @brief Reads soil parameters from eva2 database.
 * The tables Tab_BoProfil and Tab_BoChemie contain the values
 * for the soil parameter set. The soil in table is divided into
 * horizon that are converted into the according number of layers.
 * Each layer inside a horizon does have the same parameter set.
 *
 * @return Vector of soil parameters that contains a parameter set for each layer
 */
const SoilPMs*
Monica::readSoilParametersForEva2(const GeneralParameters &gps, int profil_nr, int standort_id, int variante) {
  debug() << "----------------------------------------------------------------\n";
  debug() << "Reading soilparameters for Profile: " << profil_nr << endl;
  debug() << "----------------------------------------------------------------\n";

  int layer_thickness_cm = gps.ps_LayerThickness.front() * 100; //cm
  int max_depth_cm = gps.ps_ProfileDepth * 100; //cm
  int number_of_layers = int(double(max_depth_cm) / double(layer_thickness_cm));

  static L lockable;
  static bool initialized = false;
  vector<SoilParameters>* sps = new vector<SoilParameters> ;

  if (!initialized) {
    L::Lock lock(lockable);
    if (!initialized) {

      // prepare request
      const char *request_base = "select p.Profil_Nr, p.Hor_Nr, p.HO, p.HU, p.S, p.T, p.TRD_g_jecm3, c.Corg, c.ph, p.BoArt, p.Skellet "
        "from eva2.3_31_Boden_Physik as p inner join eva2.3_32_Boden_Chemie as c "
        "on p.Profil_Nr = c.Profil_Nr AND p.Hor_Nr = c.Hor_Nr "
        "where p.Profil_Nr=%i and p.id_standort=%i order by Hor_Nr";


      char request[300];
      sprintf(request, request_base, profil_nr, standort_id);
      debug() << request << endl;

      // connect to database eva2
      DB *con = newConnection("eva2");

      DBRow row;
      con->select(request);

      int index = 0;
      int layer_count = 0;
      int row_count = con->getNumberOfRows();
      while (!(row = con->getRow()).empty()){

        // row counter
        index++;



        // string id = row[0];
        int horiz_oben = satof(row[2]);
        int horiz_unten = satof(row[3]);

        double sand = -1;
        if (! row[4].empty()) {
            sand = satof(row[4]);
        }

        double clay = -1;
        if (! row[5].empty()) {
            clay = satof(row[5]);
        }

        double raw_density;
        if (! row[6].empty()) {
          raw_density = satof(row[6]);

        } else {
            cout << "variante: " << variante << "\tSize: " << sps->size() << "\t" << standort_id << endl;
            // individuall raw density for simulations in muencheberg
          if (standort_id == 59) {
              if (variante == 1 || variante == 3) {
                  // mit Bodenbearbeitung
                  if (sps->size() < 3) {
                      raw_density = 1.45;
                  } else {
                      raw_density = 1.65;
                  }
              } else {
                raw_density = 1.65;
              }
          }
        }



        double corg = satof(row[7]);
        double ph = satof(row[8]);
        std::string soil_type = row[9];

        double stone = -1;
        if (! row[10].empty()) {
//            stone = satof(row[10]);
        }

                // calc number of layers that lay in the horizon
        double horiz_width = double(horiz_unten - horiz_oben);
        horiz_width = (round(horiz_width / 10.0)) * 10.0;
        int layer_in_horiz = int(horiz_width) / layer_thickness_cm;

        // read soil parameter
        SoilParameters soil_param;

        // test if there was a value for sand in input stream
        if (sand != -1) {
          soil_param.vs_SoilSandContent = (sand / 100.0);
        } else {
            soil_param.vs_SoilSandContent = KA52sand(soil_type);
        }

        // test if there was a value for sand in input stream
        if (clay!=-1) {
          soil_param.vs_SoilClayContent = (clay / 100.0);
        } else {
          soil_param.vs_SoilClayContent = KA52clay(soil_type);
        }


        if (stone!=-1) {
            soil_param.vs_SoilStoneContent = (stone/100.0);
        } else {
            soil_param.vs_SoilStoneContent = 0.0;
        }

        soil_param.vs_SoilpH = ph;
        soil_param.set_vs_SoilOrganicCarbon(corg / 100.0);

        soil_param.set_vs_SoilRawDensity(raw_density);

        soil_param.vs_Lambda = soil_param.texture2lambda(soil_param.vs_SoilSandContent, soil_param.vs_SoilClayContent);
        soil_param.vs_SoilTexture = soil_type;

        soilCharacteristicsKA5(soil_param);

        // test validity of soil parameters
        bool valid_soil_params = soil_param.isValid();
        if (!valid_soil_params) {
            cout << "Error in soil parameters. Aborting now simulation";
            exit(-1);
        }

//        debug << "Bodenart:\t" << soil_type.c_str() << endl;
//        debug << soil_param.toString() << endl;

        // for each layer in horizon the soil parameter set is equal
        // so for each layer the same soil parameters are added to SoilPMs vector
        layer_count += layer_in_horiz;
        for (int i = 0; i < layer_in_horiz; i++) {
          sps->push_back(soil_param);
        }

        // last horizon, so add same parameter set to all remaining layers
        if (index == row_count) {
          for (int i = layer_count; i < number_of_layers; i++) {
              if (standort_id == 59) {
                  if (variante == 1 || variante == 3) {
                      // mit Bodenbearbeitung
                      if (sps->size() < 3) {
                          soil_param.set_vs_SoilRawDensity(1.45);
                      } else {
                          soil_param.set_vs_SoilRawDensity(1.65);
                      }
                  } else {
                      soil_param.set_vs_SoilRawDensity(1.65);
                  }
              }
            sps->push_back(soil_param);
          }
        }
      }

      delete con;

      initialized = true;
    }
  }
  return sps;
}

/**
 * Returns the mercantiled rounded number
 * @param number Number to be rounded
 * @param decimal Number of decimals of the number
 * @return Rounded number
 */
//double Monica::round(double number, int decimal)
//{
//  number *= std::pow(10.0, decimal);
//  if (number >= 0) {
//    number = std::floor(number + 0.5);
//  } else {
//    number = std::ceil(number - 0.5);
//  }
//  number /= std::pow(10.0, decimal);
//  return int(number);
//}

/**
 * @brief Returns ID for WStation of selected profil_number, the parameter was measured for.
 *
 * @param profil_nr Number of profile
 * @param parameter Parameter id as it is specified in EVA2-table S_Parameter
 * @return
 */
double Monica::getEffictiveRootingDepth(int profile)
{
  debug() << "\n--> Get WE (effective rooting depth for profile  " << profile << endl;
  std::vector<WStation> wstation;

  static L lockable;
  L::Lock lock(lockable);
  ostringstream request;
  request << "SELECT WE FROM 3_30_Boden_Geografie B "
          << "WHERE profil_nr = " << profile;

  debug() << request.str() << endl;

  // connect to database eva2
  DB *con = newConnection("eva2");
  DBRow row;
  con->select(request.str().c_str());

  double we = 1.2;
  while(!(row = con->getRow()).empty()) {
      if (!row[0].empty()) {
        we = satof(row[0])/100.0;
      }
  }
  delete con;

  return we;
}


/**
 * @brief Returns ID for WStation of selected profil_number, the parameter was measured for.
 *
 * @param profil_nr Number of profile
 * @param parameter Parameter id as it is specified in EVA2-table S_Parameter
 * @return
 */
std::vector<WStation> Monica::getIDOfWStation(int location, int parameter)
{
  debug() << "\n--> Get WStation_ID for parameter " << parameter << endl;
  std::vector<WStation> wstation;

  static L lockable;
  L::Lock lock(lockable);
  ostringstream request;
  request << "SELECT id_w_station, Startdatum, Enddatum FROM S_W_Station_je_Messgroesse "
          << "WHERE id_messgroesse=" << parameter
      << " AND ID_Standort=" << location
      << " AND ((Alternative=\"Standard\") OR (Alternative is null))"
      << " ORDER BY startdatum";

  debug() << request.str() << endl;

  // connect to database eva2
  DB *con = newConnection("eva2");
  DBRow row;
  con->select(request.str().c_str());
  while(!(row = con->getRow()).empty()) {
      WStation station;
      station.id=satoi(row[0]);
      if (!row[1].empty()) {
        station.start = Tools::fromMysqlString(row[1]);
      } else {
          station.start = Tools::Date();
      }
      if (!row[2].empty()) {
        station.end = Tools::fromMysqlString(row[2]);
      } else {
        station.end = Tools::Date();
      }
      station.name = getNameOfWStation(station.id);

      debug() << station.name.c_str() << "\tid: "<<  station.id << "\t" << station.start.toString().c_str() << "\t" << station.end.toString().c_str() << endl;
      wstation.push_back(station);
  }
  delete con;

  //assert(wstation.size()>0);

  for (unsigned int i=0; i<wstation.size(); i++) {
	  debug() << "WStation = " << wstation.at(i).id << endl;
  }
  return wstation;
}

/**
 * @brief Returns name of weather station specified by parameter station.
 *
 * The name is fetched by a database request from table S_W_Station
 * @param station ID of weather station
 * @return Name of weather station
 */
std::string
Monica::getNameOfWStation(int station)
{
  debug() << "--> Get Name of WStation for station " << station << endl;
  std::string name="";

  static L lockable;
  L::Lock lock(lockable);
  ostringstream request;
  request << "SELECT W_Station_kurz FROM S_W_Station "
          << "WHERE id_w_station=" << station;

  debug() << request.str() << endl;

  // connect to database eva2
  DB *con = newConnection("eva2");
  DBRow row;
  con->select(request.str().c_str());
  while(!(row = con->getRow()).empty()) {
    name = row[0];
  }
  delete con;

  assert(!name.empty());


  debug() << "Name of WStation = \"" << name << "\"" << endl;
  return name;
}

/**
 *
 * @param id_parameter
 * @param wstation
 * @return
 */
double
Monica::checkUnit(int id_parameter, std::string wstation)
{
  debug() << "--> Check unit of " << id_parameter << " (" << wstation << ") for a conversion" << endl;

  static L lockable;
  L::Lock lock(lockable);

  ostringstream request;
  request << "SELECT E FROM 1_50_Wetter where id_messgroesse=" << id_parameter
            << " AND WStation=\"" << wstation << "\"";

  debug() << request.str() << endl;


  // request unit of parameter from weather table
  const char* unit;
  bool init = false;
  // connect to database eva2
  DB *con = newConnection("eva2");
  DBRow row1;
  con->select(request.str().c_str());
  while (!(row1 = con->getRow()).empty()){
    if (!row1[0].empty()) {
      if (!init) {
        unit = row1[0].c_str();
        init= true;
        break;
      }

    }
  }
  delete con;
  debug() << "Received unit " << unit << " from database" << endl << endl;


  // request conversion factor from database
  ostringstream request_conv;
  request_conv << "SELECT faktor FROM S_Umrechnung_Einheiten "
      << "WHERE original_einheit=\"" << unit << "\" AND ziel_einheit_monica=\"MJ/m2d\"";
  debug() << request_conv.str() << endl;

  double conversion_factor=1.0;
  // connect to database eva2
  DB *con2 = newConnection("eva2");
  DBRow row;
  con2->select(request_conv.str().c_str());
  while (!(row = con2->getRow()).empty()){
    if (! row[0].empty()) {
      conversion_factor = satof(row[0]);
    }
  }
  delete con2;

  // debug
  debug() << "Received conversion factor of " << conversion_factor << endl << endl;

  return conversion_factor;
}

/**
 * @brief Reading of weather data from eva2 table Wetter2.
 *
 * All weather values are stored in one column called Wert.
 * The different weather parameters can be distinguished
 * by the column id_parameter. Each id stands for a special parameter,
 * e.g. wind, tmin, tmax etc. climate data accessor wil be
 * filled with those vectors.
 *
 * @param location
 * @param profil_nr
 * @param start_date
 * @param end_date
 * @return
 */
Climate::DataAccessor
Monica::climateDataFromEva2DB(int location, int profil_nr, Tools::Date start_date, Tools::Date end_date, CentralParameterProvider& cpp, double latitude)
{
  // some debug messages
  debug() << "----------------------------------------------------------------\n";
  debug() << "--> Reading weather parameters for profile number: " << profil_nr << endl; //<< " (" << station << ")"<< endl;
  debug() << "Start date: " << start_date.toString() << endl;
  debug() << "End date: " << end_date.toString() << endl;
  debug() << "----------------------------------------------------------------\n";



  int id_parameter[WETTER_PARAMETER_COUNT] = {TAVG, TMIN, TMAX, GLOBRAD, RELHUMID, WIND, PRECIP};

  // gueterfelde uses another wind height, so change the wind parameter
  // before the data base query
  if (location == LOCATION_GUETERFELDE) {
    //id_parameter[5] = WIND_3m;
    id_parameter[5] = WIND_2m;
    id_parameter[3] = SUNHOURS;
    cpp.userEnvironmentParameters.p_WindSpeedHeight = 2;
  }

  if (location== LOCATION_TROSSIN) {
      id_parameter[5] = WIND_2m;
      cpp.userEnvironmentParameters.p_WindSpeedHeight = 2;

      // Klitzschen
//      id_parameter[5] = WIND_10m;
//      cpp.userEnvironmentParameters.p_WindSpeedHeight = 10;
  }

  if  (location == LOCATION_GUELZOW_GRENZSTANDORT) {
      id_parameter[5] = WIND_2m;
      cpp.userEnvironmentParameters.p_WindSpeedHeight = 2;
  }


  if  (location == LOCATION_BERNBURG) {
      //id_parameter[5] = WIND_10m;
      cpp.userEnvironmentParameters.p_WindSpeedHeight = 8;
      id_parameter[5] = WIND_8m;
      id_parameter[3] = SUNHOURS;

  }

  std::vector<WStation> id_wstation[WETTER_PARAMETER_COUNT];

  for (unsigned int i=0; i<WETTER_PARAMETER_COUNT; i++) {
    id_wstation[i] = getIDOfWStation(location, id_parameter[i]);
  }

  debug() << endl;

//  // get number of different weather stations used
//  // for this location and safe ids for wstation
//  std::vector<WStation> different_id_wstation;
//  for (unsigned int i=0; i<id_wstation[0].size(); i++) {
//    debug() << "diff wstation init: " << i << "\t" << id_wstation[0].at(i).id << endl;
//	  different_id_wstation.push_back(id_wstation[0].at(i));
//  }


//  for (unsigned int param=1; param<WETTER_PARAMETER_COUNT; param++) {
//
//    for (unsigned int j=0; j<id_wstation[param].size(); j++) {
//
//      for(unsigned int diff=0; diff<different_id_wstation.size(); diff++) {
//        cout << "Param: " << id_parameter[param] <<  "\t" << id_wstation[param].at(j).id << "\t" << different_id_wstation.at(diff).id << endl;
//        cout << id_wstation[param].at(j).start.toString().c_str()<< "\t" << different_id_wstation.at(diff).start.toString().c_str() << endl;
//        cout << id_wstation[param].at(j).end.toString().c_str()<< "\t" << different_id_wstation.at(diff).end.toString().c_str() << endl;
//
//    	  if ( (id_wstation[param].at(j).id == different_id_wstation.at(diff).id) &&
//    	       (id_wstation[param].at(j).start == different_id_wstation.at(diff).start) &&
//    	       (id_wstation[param].at(j).end == different_id_wstation.at(diff).end) )
//    	    {
//    	    // same station found so break the j-loop here
//    	      cout << "Same station found" << endl;
//    		  break;
//    		}
//		    if (diff==(different_id_wstation.size()-1)) {
//			    // alle bisherigen durchgesucht, aber diese Station nicht gefunden
//  			  // so dass eine neue hinzugefügt werden muss
//		      debug() << "Add diff wstation:\tparam: " << param << "\tj: " << j << "\tdiff: " << diff << "\t" << id_wstation[param].at(j).id << endl;
//		      if (id_wstation[param].at(j).start < different_id_wstation.at(diff).start) {
//		          std::vector<WStation>::iterator it = different_id_wstation.begin();
//		          it+=diff;
//	  		      different_id_wstation.insert(it,id_wstation[param].at(j));
//	  		      diff=0;
//		      }
//		    }
//      } // for diff
//    }//for j
//  } // for param


  // get name of WStations
//  debug() << "Number of W_Stations: " << different_id_wstation.size() << endl;
//  std::string wstation_name[different_id_wstation.size()];
//  for (unsigned int i=0; i<different_id_wstation.size(); i++) {
//    debug() << "\nWStation: " << i << "\t"   << different_id_wstation.at(i).id << endl;
//    wstation_name[i] = getNameOfWStation(different_id_wstation.at(i).id);
//  }


  // temporary vectors
  vector<double> _tmin;
  vector<double> _tmax;
  vector<double> _tavg;
  vector<double> _globrad;
  vector<double> _relhumid;
  vector<double> _wind;
  vector<double> _precip;
  vector<double> _sunhours;

  bool unit_globrad_initialized = false;
  double conversion_globrad = 1.0;

  static L lockable;
  static bool initialized = false;

  if (!initialized) {
      L::Lock lock(lockable);

      if (!initialized) {

          for (int param=0; param<WETTER_PARAMETER_COUNT; param++) {


              for (unsigned int station_index=0; station_index<id_wstation[param].size(); station_index++) {

                  unit_globrad_initialized = false;

                  WStation station=id_wstation[param].at(station_index);
                  std::string name(getNameOfWStation(station.id));

                  Date sdate = start_date;
                  Date edate = end_date;

                  Date currentDate = start_date;

                  if (sdate < station.start) {
                      sdate=station.start;
                  }

                  if ( (edate > station.end) && (station.end != Date(1,1,1951) && (station.end.isValid())) ) {
                      edate=station.end;
                  }
                  unit_globrad_initialized = false;
                  ostringstream request;
                  request << "SELECT id_messgroesse, datum, Wert, E FROM 1_50_Wetter WHERE (";
                  request << "id_messgroesse=" << id_parameter[param];
                  request << ") AND WStation='" << name
                      << "' AND Datum>=" << sdate.toMysqlString().c_str() << " "
                      << "AND Datum<=" << edate.toMysqlString().c_str() << " "
                      << "order by id_messgroesse ASC, datum ASC";


                  debug() << "\n" << request.str() << endl <<  endl;

                  // date needed for applying precipitation correction values
                  Date date = start_date;

                  // connect to database eva2
                  DB *con = newConnection("eva2");
                  DBRow row;
                  con->select(request.str().c_str());


                  while(!(row = con->getRow()).empty()) {

                      // split climate data according to their id
                      switch (satoi(row[0])) {

                      case Monica::TMIN:
                        //            cout << "tmin: " << tmin;
                        _tmin.push_back(satof(row[2]));
                        break;

                      case Monica::TMAX:
                        //            cout << "tmax: " << tmin;
                        _tmax.push_back(satof(row[2]));
                        break;

                      case Monica::TAVG:
                        _tavg.push_back(satof(row[2]));
                        break;

                      case Monica::RELHUMID:
                        _relhumid.push_back(satof(row[2]));
                        break;

                      case Monica::WIND:
                      case Monica::WIND_3m:
                      case Monica::WIND_2m:
                      case Monica::WIND_10m:
                      case Monica::WIND_8m:
                      case Monica::WIND_19m:

                        _wind.push_back(satof(row[2]));
                        break;

                      case Monica::PRECIP:
                        {
                          double precip = satof(row[2]);
                          precip*=cpp.getPrecipCorrectionValue(date.month()-1);
                          _precip.push_back(precip);
                          date++;
                        }
                        break;

                      case Monica::GLOBRAD:
                        if (!unit_globrad_initialized) {
                            conversion_globrad = checkUnit(GLOBRAD, name);
                            unit_globrad_initialized = true;
                        }
                        _globrad.push_back(conversion_globrad*satof(row[2]));


                        break;

                      case Monica::SUNHOURS:
                        _globrad.push_back(sunshine2globalRadiation(currentDate.dayOfYear(), satof(row[2]), latitude, true));

                        break;

                      default:
                        break;
                      } // switch
                      currentDate++;

                  } // while
                  delete con;

              } // for station_index
          } // for param
          initialized = true;
      } // if (!initialized)
  } // if (!initialized)

  // number of days between start date and end date
  unsigned int days = (unsigned int)start_date.numberOfDaysTo(end_date) + 1;

  debug() << "Days: " << days <<
      "\tWIND " << _wind.size() <<
      "\tTMIN " << _tmin.size() <<
      "\tTMAX " << _tmax.size() <<
      "\tTAVG " << _tavg.size() <<
      "\tRELHUMID " << _relhumid.size() <<
      "\tPRECIP " << _precip.size() <<
      "\tGLOBRAD" << _globrad.size() <<
      "\tSUNHOURS " << _sunhours.size() <<

      endl;

  // assert that enough values have been read from data base
  assert(_tmin.size()==days);
  assert(_tmax.size()==days);
  assert(_tavg.size()==days);
  assert(_relhumid.size()==days);
  assert(_wind.size()==days);
  assert(_precip.size()==days);

//  if (location == LOCATION_GUETERFELDE) {
//    assert(_sunhours.size()==days);
//  } else {
//    assert(_globrad.size()==days);
//  }

  DataAccessor da(start_date, end_date);

  // add climate data to data accessor
  da.addClimateData(tmin, _tmin);
  da.addClimateData(tmax, _tmax);
  da.addClimateData(tavg, _tavg);
  da.addClimateData(relhumid, _relhumid);
  da.addClimateData(wind, _wind);
  da.addClimateData(precip, _precip);


  da.addClimateData(globrad, _globrad);

  if (_sunhours.size()>0) da.addClimateData(globrad, _sunhours);





  // some debug messages
  debug() << "Have read " << _tmin.size() << " items for tmin" << endl;
  debug() << "Have read " << _tmax.size() << " items for tmax" << endl;
  debug() << "Have read " << _tavg.size() << " items for tavg" << endl;
  debug() << "Have read " << _relhumid.size() << " items for relhumid" << endl;
  debug() << "Have read " << _wind.size() << " items for wind" << endl;
  debug() << "Have read " << _precip.size() << " items for precip" << endl;
  debug() << "Have read " << _globrad.size() << " items for globrad" << endl;
  debug() << "Have read " << _sunhours.size() << " items for sunhours" << endl;

  return da;
}


SiteParameters
Monica::readSiteParametersForEva2(int location, int profil_nr)
{
  debug() << "----------------------------------------------------------------\n";
  debug() << "Reading SiteParameters for location: " << location << endl;
  debug() << "----------------------------------------------------------------\n";


  static L lockable;
  L::Lock lock(lockable);
  ostringstream request;
  request << "SELECT hangneigung_m_pro_m, atmosph_N_Deposition_kg_jeha_u_a FROM S_Standorte "
          << "WHERE id_standort=" << location;

  debug() << request.str() << endl;

  double latitude=0.0;
  double slope=0.0;
  double n_deposition = 30.0;

  // connect to database eva2
  DB *con = newConnection("eva2");
  DBRow row;
  con->select(request.str().c_str());
  while(!(row = con->getRow()).empty()) {


      if (! row[0].empty()) {
        slope = satof(row[0]);
      }

      if (! row[1].empty()) {
          n_deposition = satof(row[1]);
      }

  }

  ostringstream request2;
  request2 << "SELECT latitude FROM 3_30_Boden_Geografie "
            << "WHERE profil_nr=" << profil_nr;
  debug() << request2.str() << endl;

  DBRow row2;
  con->select(request2.str().c_str());
  while(!(row2 = con->getRow()).empty()) {

      if (! row2[0].empty()) {
          latitude = satof(row2[0]);
      }
    }

  delete con;

  assert(latitude!=0);


  SiteParameters site_parameters;
  site_parameters.vs_Latitude = latitude;
  site_parameters.vs_Slope = slope;
  site_parameters.vq_NDeposition = n_deposition;

  debug() << site_parameters.toString().c_str() << endl;
  return site_parameters;
}

/**
 * Returns a filename that contains station name and profil id.
 * @param station
 * @param profil_nr
 * @return
 */
std::string
Monica::getFilename(int station_id, int profil_nr)
{
  const char* station;

  // get station name
  switch (station_id) {
  case LOCATION_AHOLFING:
    station = "aholfing";
    break;
  case LOCATION_ASCHA:
    station = "ascha";
    break;
  case LOCATION_BANDOW:
    station = "bandow";
    break;
  case LOCATION_BERGE:
    station = "berge";
    break;
  case LOCATION_BRUCHHAUSEN:
    station = "bruchhausen";
    break;
  case LOCATION_BURKERSDORF:
    station = "burkersdorf";
    break;
  case LOCATION_DORNBURG:
    station = "dornburg";
    break;
  case LOCATION_ETTLINGEN:
    station = "ettlingen";
    break;
  case LOCATION_GUELZOW_GRENZSTANDORT:
    station = "gülzow_grenzstandort";
    break;
  case LOCATION_GUETERFELDE:
    station = "gueterfelde";
    break;
  case LOCATION_HAUS_DUESSE:
    station = "haus_duesse";
    break;
  case LOCATION_OBERWEISSBACH:
    station = "oberweissbach";
    break;
  case LOCATION_PAULINENAUE:
    station = "paulinenaue";
    break;
  case LOCATION_RAUISCHHOLZHAUSEN:
    station = "rauischholzhausen";
    break;
  case LOCATION_STRAUBING:
    station = "straubing";
    break;
  case LOCATION_TROSSIN:
    station = "trossin";
    break;
  case LOCATION_WEHNEN:
    station = "wehnen";
    break;
  case LOCATION_WERLTE:
    station = "werlte";
    break;
  case LOCATION_WITZENHAUSEN:
    station = "witzenhausen";
    break;
  case LOCATION_GROSS_KREUTZ:
    station = "gross_kreutz";
    break;
  case LOCATION_DOLGELIN:
    station = "dolgelin";
    break;
  case LOCATION_SOPHIENHOF:
    station = "sophienhof";
    break;
  case LOCATION_BRAMSTEDT:
    station = "bramstedt";
    break;
  case LOCATION_VRESCHEN_BOKEL:
    station = "vreschen_bokel";
    break;
  case LOCATION_HAUFELD:
    station = "haufeld";
    break;
  case LOCATION_GUELZOW_OEKO:
    station = "gülzow_oekofeld";
    break;
  default:
    cerr << "Unknown station id." << endl;
    assert(0);
    break;
  } // switch(station)

  // add current date and time to filename
  time_t rawtime;
  struct tm * timeinfo;
  char buffer [17];
  char filename[100];
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  strftime (buffer,80,"%Y-%m-%d_%H-%M",timeinfo);
  sprintf(filename, "eva2_data/%s_%d-%s.txt", station, profil_nr, buffer);

  return string(filename);
}

void
Monica::readPrecipitationCorrectionValues(CentralParameterProvider& cpp)
{
  // debug
  debug() << "Reading precipitation correction values according to Richter 1995" << endl;

  static L lockable;
  static bool initialized = false;

  if (!initialized) {
    L::Lock lock(lockable);

    if (!initialized) {
      DB *con = newConnection("eva2");
      DBRow row;
      con->select("select monat, werte_kersebaum from S_Niederschlagskorrekturwerte order by monat");

      while(!(row = con->getRow()).empty()) {
        int month = satoi(row[0]);
        double value = satof(row[1]);
        cpp.setPrecipCorrectionValue(month,value);
      }

      delete con;
      initialized = true;

    } // ! initialized
  } // ! initialized
}


void
Monica::readGroundwaterInfos(CentralParameterProvider& cpp, int location)
{
  // debug
  debug() << "Reading groundwater information from table S_Standorte" << endl;

  static L lockable;
  static bool initialized = false;

  if (!initialized) {
    L::Lock lock(lockable);

    if (!initialized) {
      DB *con = newConnection("eva2");
      DBRow row;
      ostringstream request;
      request << "SELECT id_standort, grundwassertiefe_min, grundwassertiefe_max "
              << "FROM S_Standorte "
              << "WHERE id_standort=" << location;
      con->select(request.str().c_str());
      debug() << request.str().c_str() << endl;

      while(!(row = con->getRow()).empty()) {
        if (! row[1].empty()) {
          debug() << "GROUNDWATER\tmin: " << row[1] << endl;
          cpp.userEnvironmentParameters.p_MinGroundwaterDepth = satof(row[1]);
        }
        if (! row[2].empty()) {
          debug() << "GROUNDWATER\tmax: " << row[2] << endl;
          cpp.userEnvironmentParameters.p_MaxGroundwaterDepth = satof(row[2]);
        }

      }

      delete con;
      initialized = true;

    } // ! initialized
  } // ! initialized
}



/**
 *
 * @param id_string
 * @return
 */
std::vector<ProductionProcess>
Monica::getCropManagementData(std::string id_string, std::string eva2_crop, int location)
{
  vector<ProductionProcess> ff;
  vector<string> id_pg_list;
  id_pg_list.push_back(id_string);

  // debug
  debug() << endl << "Reading sowing and harvesting date from eva2 database" << endl;

  static L lockable;

  L::Lock lock(lockable);

  // connect to eva2 database
  DB *con = newConnection("eva2");
  DB *con2 = newConnection("eva2");


  // Special implementation to catch perennial crops
  // especially for ff04 of the eva2 experiments

  bool perennial = false;

  // first check, if the current crop is perennial
  std::ostringstream request_multi_years;
  request_multi_years << "SELECT winsommehrj FROM S_Fruechte S where id_frucht=" << eva2_crop;
  con->select(request_multi_years.str().c_str());
  debug() << request_multi_years.str().c_str() << endl;
  // get date of sowing
  DBRow row_perennial;
  while (!(row_perennial = con->getRow()).empty()) {
    string text= row_perennial[0];
    if (text == "mehrjaehrig") {
        perennial = true;
        debug() << "Mehrjährig" << endl;
    }
  }

  // if the crop is perennial, there may be different fruchtfolgeglieder
  // that holds information for the same crop
  if (perennial) {
      // get all id_pgs in this crop rotation with the same fruchtfolgeglied
      std::string id_string_short = id_string.substr(0,6);
      std::ostringstream request_pruefglieder;

      request_pruefglieder << "SELECT id_fruchtfolgeglied, id_frucht, erntejahr  FROM 3_70_Pruefglieder P where id_pg like \"" << id_string_short << "%\" order by id_fruchtfolgeglied";
      con->select(request_pruefglieder.str().c_str());
      debug() << request_pruefglieder.str().c_str() << endl;

      string frucht_alt = "";
      DBRow row_pruefglied;
      while (!(row_pruefglied = con->getRow()).empty()) {

          string ff_glied = row_pruefglied[0];
          string ff_art = row_pruefglied[1];
          string year = row_pruefglied[2];

          if (ff_art=="20") ff_art = "020";
          if (ff_art=="41") ff_art = "041";
          if (ff_art=="25") ff_art = "025";

          debug() << "PG: " << atoi(ff_art.c_str()) << "\t" << atoi(eva2_crop.c_str()) << endl;
          if (atoi(ff_art.c_str()) == atoi(eva2_crop.c_str())) {
              string new_id_pg = id_string_short;
              if (frucht_alt != "" && atoi(frucht_alt.c_str()) == atoi(ff_art.c_str()) ) {
                  debug() << "Gleiche Frucht" << endl;
                  new_id_pg.append(ff_glied);
                  new_id_pg.append(ff_art);
                  new_id_pg.append("_");
                  new_id_pg.append(year);

                  id_pg_list.push_back(new_id_pg);
              } else {
                  frucht_alt = ff_art;
              }
          }
      }
  }

  // get crop pointer with correct crop od
  CropPtr crop = getEva2CropId2Crop(eva2_crop, location);
  ProductionProcess pp(eva2_crop, crop);

  debug() << "CropId:\t" << crop->id() << endl;

  Tools::Date sowing_date;
  Tools::Date harvest_date;

  // get sowing date -------------------------
  std::ostringstream request_sowing;
  request_sowing << "SELECT Datum, ID_pg FROM 2_60_Bew_Daten T where id_pg like \""
               << id_string << "%\" and id_arbeit like \"2%\" and datum is not null order by Datum ASC";
  con->select(request_sowing.str().c_str());
  debug() << request_sowing.str().c_str() << endl;


  // get date of sowing
  DBRow row_sowing;

  string id_pg;
  while (!(row_sowing = con->getRow()).empty()) {
    sowing_date = fromMysqlString(row_sowing[0]);
    id_pg=row_sowing[1];
  }

  pp.addApplication(Seed(sowing_date, crop));

  // check 10th item in id, because there can be found
  // the real type of usage
  const char gp_char = id_pg[10];
  int usage = satoi(&gp_char);

  // Ganzpflanze oder Korn geerntet?
  //  bool ganz_pflanze = false;
  if (id_pg[0] == '5' && id_pg[1] == '9') {
    // Überschreiben der Nutzungsart für Müncheberg und Winterroggen
    if (eva2_crop == "172") {
        usage = NUTZUNG_GANZPFLANZE;
    }
  }
  if (usage == NUTZUNG_GANZPFLANZE && eva2_crop!="160") {
    // Ausnahme für Sudangras, da in Ettlingen zweimal geschnitten und des mit
    // dem Schneiden Probleme gab
    debug() << "Ganzpflanze: " << eva2_crop.c_str() << "\t" << id_pg << endl;
    //ganz_pflanze = true;
  }

  // Test if Gruenduengung
  if (usage == NUTZUNG_GRUENDUENGUNG) {
    debug() << "Gründüngung: " << eva2_crop.c_str() << "\t" << id_pg << endl;
  }


  crop->setCropParameters(getCropParametersFromMonicaDB(crop->id()));
  debug() << "EVA2 - Found crop parameters for " << eva2_crop.c_str() << endl;

  crop->setEva2TypeUsage(usage);
  debug() << "Looking for residues for " << eva2_crop.c_str() << endl;

  crop->setResidueParameters(getResidueParametersFromMonicaDB(crop->id()));
  debug() << "Creating new production process for crop "  << eva2_crop.c_str() << endl;



  for (int i=0; i<id_pg_list.size(); i++) {

      id_string = id_pg_list.at(i);

      // get harvest date  ------------------------------------
      std::ostringstream request_harvest;
      request_harvest << "SELECT DatumErnte FROM 2_10_Ertraege T where id_pg like \""
                   << id_string << "%\" and id_termin=61 and datumernte is not null group by DatumErnte order by DatumErnte";

      con->select(request_harvest.str().c_str());
      debug() << request_harvest.str().c_str() << endl;

      // get date of harvest
      DBRow row_harvest;


      // table 1_Ertraege list all yield mounts with harvest yield;
      // need to read only first row, because harvest date would be
      // all the same for other rows
      row_harvest = con->getRow();
      if (! row_harvest.empty()) {
        harvest_date = fromMysqlString(row_harvest[0]);
      }


    // check if there are cutting dates provided
    std::ostringstream request_cutting;
    request_cutting << "SELECT DatumErnte FROM 2_10_Ertraege T where id_pg like \""
                     << id_string << "%\" and (id_termin>=62 and id_termin<=69) and datumernte is not null "
                     << " group by DatumErnte";
    con->select(request_cutting.str().c_str());
    debug() << request_cutting.str().c_str() << endl;

    DBRow row_cutting;
    Tools::Date cutting_date;
    while (!(row_cutting = con->getRow()).empty()) {
      cutting_date = fromMysqlString(row_cutting[0]);
      crop->addCuttingDate(cutting_date);
      pp.addApplication(Cutting(cutting_date, crop));
      debug() << "Cutting Date:\t" << cutting_date.toString() << endl;

      // overwrite harvest date with last cutting date
      harvest_date = cutting_date;
    }

    if (i == id_pg_list.size()-1) {

      debug() << "Sowing Date:\t" << sowing_date.toString() << endl;
      debug() << "Harvest Date:\t" << harvest_date.toString() << endl;
      pp.addApplication(Harvest(harvest_date, crop, pp.cropResultPtr()));
      crop->setSeedAndHarvestDate(sowing_date, harvest_date);    }


    // get tillages
    debug() << endl;
    std::ostringstream request_tillages;
    request_tillages << "SELECT id_Arbeit, Datum FROM 2_60_Bew_Daten T where id_pg like \""
                         << id_string << "%\" and id_arbeit like \"1%\"";
    con->select(request_tillages.str().c_str());
    debug() << request_tillages.str().c_str() << endl;

    // get date of tillage -------------------------------------
    DBRow row_tillages;

    while (!(row_tillages = con->getRow()).empty()) {
      int tillage = satoi(row_tillages[0]);
      Tools::Date tillages_date;

      // db table S_Arbeit --> 113, 114 Pflügen mit/ohne Packer
      if (tillage == 113 || tillage == 114) {
        tillages_date= fromMysqlString(row_tillages[1]);
        pp.addApplication(TillageApplication(tillages_date, 0.3));
        debug() << "Add tillage (0.3m)  at: " << tillages_date.toString() << endl;
      }
    }


    // get fertilisation
    debug() << endl;
    std::ostringstream request_fertiliser;
    request_fertiliser << "SELECT Id_bew_daten, datum FROM 2_60_Bew_Daten T where id_pg like \""
                          << id_string << "%\" and id_arbeit like \"3%\"";
    con->select(request_fertiliser.str().c_str());
    debug() << request_fertiliser.str().c_str() << endl;
    debug() << "Found " << con->getNumberOfRows() << " fertilisers" << endl << endl;

     // get date of fertilisation -------------------------------------
     DBRow row_fertiliser;

     while (!(row_fertiliser = con->getRow()).empty()){
       string bew_id = row_fertiliser[0];
       Tools::Date fertilizer_date = fromMysqlString(row_fertiliser[1].c_str());

       // get amount and id of fertiliser
       std::ostringstream request_fertiliser_id;
       request_fertiliser_id << "SELECT id_Duenger, Menge "
                             << "FROM 2_63_Betriebsmittel_Duenger T where id_bew_daten = \""
                             << bew_id << "\" and id_Duenger is not null";
       debug() << request_fertiliser_id.str().c_str() << endl;
       con2->select(request_fertiliser_id.str().c_str());

       DBRow row_fertiliser_id;
       while (!(row_fertiliser_id = con2->getRow()).empty()) {
         string fert_id = row_fertiliser_id[0];
         double fert_amount = satof(row_fertiliser_id[1]);
         debug() << "Fert_amount: " << fert_amount << endl;


          // check if parameter of such fertiliser is known to MONICA
          pair<FertiliserType, int> fertTypeAndId = eva2FertiliserId2monicaFertiliserId(fert_id);
          switch (fertTypeAndId.first) {

            // mineral fertiliser
            case mineral: {
                double fert_percentage = getNPercentageInFertilizer(fert_id);
                double conversion_factor = getOrganicFertiliserConversionFactor(fert_id);
                debug() << "N percentage in fertilizer: " << fert_percentage << endl;
                double n_amount = (fert_amount * conversion_factor)  * fert_percentage / 100.0;

                //create mineral fertiliser application
                const MineralFertiliserParameters& mfp = getMineralFertiliserParametersFromMonicaDB(fertTypeAndId.second);
                if (mfp.getCarbamid() == 0.0 && mfp.getNO3() == 0.0 && mfp.getNH4() == 0.0) {
                    std::cerr << "Cannot find fertiliser " << fertTypeAndId.second << " in MONICA database. Aborting now ..." << endl;
                    exit(-1);
                }
                pp.addApplication(MineralFertiliserApplication(fertilizer_date, mfp, n_amount));
                debug() << "Adding mineral fertiliser: " << fertilizer_date.toString() << "\t" << fert_id << "\t" << n_amount << "kg/ha" << endl << endl;
                cout << MineralFertiliserApplication(fertilizer_date, mfp, n_amount).toString() << endl;
                break;
            }

            // organic fertiliser
            case organic: {

              double fert_percentage = getNPercentageInFertilizer(fert_id);
              double conversion_factor = getOrganicFertiliserConversionFactor(fert_id);
              debug() << "N percentage in fertilizer: " << fert_percentage << endl;
              fert_amount *= conversion_factor;

              OrganicMatterParameters *omp = new OrganicMatterParameters(*(getOrganicFertiliserParametersFromMonicaDB(fertTypeAndId.second)));
              omp = getOrganicFertiliserDetails(omp, fert_id);
              omp->vo_NConcentration = fert_percentage / 100.0;

              //create organic fertiliser application
              pp.addApplication(OrganicFertiliserApplication(fertilizer_date, omp, fert_amount, true));
              debug() << "Adding organic fertiliser: " << fertilizer_date.toString() << "\t" << fert_id << "\t" << fert_amount << "kg/ha\tfert_prz: " << omp->vo_NConcentration <<  endl << endl;
              break;
            }

            // unknown fertiliser application
            case undefined: {
              // case for fertilisers that are unknown or not relevant to MONICA
              debug() << "Ignoring fertilizer " << fert_id << " because it contains no nitrogen." << endl;
            }
          } // switch

       } // while fertiliser 2
     }// while fertiliser

  } // for id_pg_list

  delete con;
  delete con2;

  debug() << "End of getCropManagementData" << endl;
  ff.push_back(pp);

  debug() << endl;
  return ff;
}


OrganicMatterParameters*
Monica::getOrganicFertiliserDetails(OrganicMatterParameters *omp, std::string fert_id)
{
  debug() << "Get organic fertiliser details for \"" << fert_id.c_str() << "\"" << endl;

  static L lockable;

  L::Lock lock(lockable);

  DB *con = newConnection("eva2");
  std::ostringstream request_fertiliser;
  request_fertiliser << " SELECT TM_PrzFM, NO3_N_PrzFM, NH4_N_PrzFM, Harnstoff_N_PrzFM FROM S_Duenger S where id_duenger=\"" << fert_id.c_str() << "\"";

  debug() << request_fertiliser.str().c_str() << endl;
  con->select(request_fertiliser.str().c_str());

  double value = 0.0;
  double factor = 0.0;
  DBRow row;
  while(!(row = con->getRow()).empty()) {
      omp->vo_AOM_DryMatterContent = satof(row[0]) / 100.0;    //! Dry matter content of added organic matter [kg DM kg FM-1]
      omp->vo_AOM_NH4Content = satof(row[2]) / omp->vo_AOM_DryMatterContent / 100.0;          //! Ammonium content in added organic matter [kg N kg DM-1]
      omp->vo_AOM_NO3Content = satof(row[1]) / omp->vo_AOM_DryMatterContent / 100.0;          //! Nitrate content in added organic matter [kg N kg DM-1]
      omp->vo_AOM_CarbamidContent = satof(row[3]) / omp->vo_AOM_DryMatterContent / 100.0;     //! Carbamide content in added organic matter [kg N kg DM-1]

      debug() << "vo_AOM_DryMatterContent" << omp->vo_AOM_DryMatterContent << endl;
      debug() << "vo_AOM_NH4Content" << omp->vo_AOM_NH4Content << endl;
      debug() << "vo_AOM_NO3Content" << omp->vo_AOM_NO3Content << endl;
      debug() << "vo_AOM_CarbamidContent" << omp->vo_AOM_CarbamidContent << endl;
  }
  delete con;

  // if fertiliser is liquid, then there is some conversion factor
  // for calculation from l to kg
  return omp;

}

double
Monica::getOrganicFertiliserConversionFactor(std::string fert_id)
{
  debug() << "Get organic fertiliser conversion details for \"" << fert_id.c_str() << "\"" << endl;

  static L lockable;

  L::Lock lock(lockable);

  DB *con = newConnection("eva2");
  std::ostringstream request_fertiliser;
  request_fertiliser << " SELECT Einheit, Faktor_Liter_in_kg FROM S_Duenger S where id_duenger=\"" << fert_id.c_str() << "\" and (Einheit=\"kg\" or Einheit=\"l\" or Einheit=\"m3\")";

  debug() << request_fertiliser.str().c_str() << endl;
  con->select(request_fertiliser.str().c_str());

  std::string einheit = "";
  double factor_je_liter = 0.0;
  DBRow row;
  while(!(row = con->getRow()).empty()) {
    einheit = row[0];
    factor_je_liter = satof(row[1]);
  }
  delete con;

  if (einheit == "m3") {
      factor_je_liter *= 1000.0;  // m3 --> l
  }

  // if fertiliser is liquid, then there is some conversion factor
  // for calculation from l to kg
  debug () << "Conversion factor: " << factor_je_liter << endl;
  return factor_je_liter;

}


/**
 * @brief Reads percentage of N in 1 kg fertilizer.
 * @param id_fert Eva2 ID of fertilizer
 * @return Percentage of N
 */
double
Monica::getNPercentageInFertilizer(std::string id_fert)
{
  debug() << "Reading N percentage of fertilizer \"" << id_fert.c_str() << "\"" << endl;

  static L lockable;

  L::Lock lock(lockable);

  DB *con = newConnection("eva2");
  std::ostringstream request_fertiliser;
  request_fertiliser << " SELECT Nges_PrzFM FROM S_Duenger S where id_duenger=\"" << id_fert.c_str() << "\" and (Einheit=\"kg\" or Einheit=\"l\" or Einheit=\"m3\")";

  debug() << request_fertiliser.str().c_str() << endl;
  con->select(request_fertiliser.str().c_str());

  double value = 0.0;
  DBRow row;
  while(!(row = con->getRow()).empty()) {
    value = satof(row[0]);
  }
  delete con;


  // if fertiliser is liquid, then there is some conversion factor
  // for calculation from l to kg
  return value;
}

/**
 * Maps a fertiliser id of Eva2 to MONICA's known  fertiliser list.
 *
 * @param name Id used in EVA2 database for fertiliser
 * @return Pair that consist of categorie (mineral, organic) and fertiliser id
 */
pair<FertiliserType, int>
Monica::eva2FertiliserId2monicaFertiliserId(const string& name)
{
  if (name == "D47") {
    return make_pair(mineral, 1); //1.00 0.00 0.00 01.00 M Kalkammonsalpeter (Einh : kg N / ha)
  } else if (name == "D42") {
    return make_pair(mineral,3);  // Ammonsulfatsalpeter
  } else if (name == "D57") {
    return make_pair(mineral,2);  // Diammonsulphat
  } else if (name == "D74" || name == "D141" || name == "D66") {
	  return make_pair(mineral,11);  // NPK-Dünger (verschiedene Sorten)
  } else if (name == "D45") {
    return make_pair(mineral,12);  // Harnstoff mit Nitrifikationshemmstoff
  } else if (name == "D44") {
    return make_pair(mineral,8);  // Harnstoff
  } else if (name == "D52") {
    return make_pair(mineral,14);  // Stickstoffdüngerlösung mit Schwefel, Piasan 24 S
  } else if (name == "D140") {
    return make_pair(mineral,4);  // Alzon 2
  } else if (name == "D145") {
      return make_pair(mineral,18);  // Kemistar
  } else if (name == "D149") {
      return make_pair(undefined,7);  // Yara Vita Raps
  } else if (name == "D200") {
    return make_pair(mineral,15);  // Ammoniumnitrat-Harnstoff-Lösung + Ammoniumthiosulfat, 27-3
  } else if (name == "D202") {
      return make_pair(mineral,15);  // Ammoniumnitrat-Harnstoff-Lösung + Ammoniumthiosulfat
  } else if (name == "D206") {
    return make_pair(mineral,16);  // Alzon flüssig S
  } else if (name == "D40") {
    return make_pair(mineral,17);  // Piamon
  }

  else if (name == "D611") {
      return make_pair(organic, 3);
  } else if (name == "D612") {
      return make_pair(organic, 3);
  } else if (name == "D613") {
      return make_pair(organic, 3);
  } else if (name == "D614") {
      return make_pair(organic, 3);
  } else if (name == "D615") {
      return make_pair(organic, 3);
  } else if (name == "D616") {
      return make_pair(organic, 3);
  } else if (name == "D617") {
      return make_pair(organic, 3);
  } else if (name == "D618") {
      return make_pair(organic, 3);
  } else if (name == "D619") {
      return make_pair(organic, 3);
  } else if (name == "D620") {
      return make_pair(organic, 3);
  } else if (name == "D621") {
      return make_pair(organic, 3);
  } else if (name == "D622") {
      return make_pair(organic, 3);
  } else if (name == "D623") {
      return make_pair(organic, 3);
  } else if (name == "D624") {
      return make_pair(organic, 3);
  } else if (name == "D625") {
      return make_pair(organic, 3);
  } else if (name == "D626") {
      return make_pair(organic, 3);
  } else if (name == "D627") {
      return make_pair(organic, 3);
  } else if (name == "D628") {
      return make_pair(organic, 3);
  } else if (name == "D629") {
      return make_pair(organic, 3);
  } else if (name == "D630") {
      return make_pair(organic, 3);
  } else if (name == "D631") {
      return make_pair(organic, 3);
  } else if (name == "D632") {
      return make_pair(organic, 3);
  } else if (name == "D633") {
      return make_pair(organic, 3);
  } else if (name == "D634") {
      return make_pair(organic, 3);
  } else if (name == "D635") {
      return make_pair(organic, 3);
  } else if (name == "D636") {
      return make_pair(organic, 3);
  } else if (name == "D637") {
      return make_pair(organic, 3);
  } else if (name == "D638") {
      return make_pair(organic, 3);
  } else if (name == "D639") {
      return make_pair(organic, 3);
  } else if (name == "D640") {
      return make_pair(organic, 3);
  } else if (name == "D641") {
      return make_pair(organic, 3);
  } else if (name == "D642") {
      return make_pair(organic, 3);
  } else if (name == "D643") {
      return make_pair(organic, 3);
  } else if (name == "D644") {
      return make_pair(organic, 3);
  } else if (name == "D645") {
      return make_pair(organic, 3);
  } else if (name == "D646") {
      return make_pair(organic, 3);
  } else if (name == "D647") {
      return make_pair(organic, 3);
  } else if (name == "D648") {
      return make_pair(organic, 3);
  } else if (name == "D649") {
      return make_pair(organic, 3);
  } else if (name == "D650") {
      return make_pair(organic, 3);
  } else if (name == "D651") {
      return make_pair(organic, 3);
  } else if (name == "D652") {
      return make_pair(organic, 3);
  } else if (name == "D653") {
      return make_pair(organic, 3);
  } else if (name == "D654") {
      return make_pair(organic, 3);
  } else if (name == "D655") {
      return make_pair(organic, 3);
  } else if (name == "D656") {
      return make_pair(organic, 3);
  } else if (name == "D657") {
      return make_pair(organic, 3);
  }



  // undefined mineral fertilisers
  else if (name == "D5") {
    return make_pair(undefined,0);  // Granukal 85
  } else if (name == "D122") {
    return make_pair(undefined,0);  // Triple-Superphosphat
  } else if (name == "D23") {
    return make_pair(undefined,0);  // 60er Kali
  } else if (name == "D129") {
      return make_pair(undefined,0);  // Bittersalz
  } else if (name == "D130") {
    return make_pair(undefined,0);  // Kieserit fein
  } else if (name == "D131") {
      return make_pair(undefined,0);  // Kieserit granuliert
  } else if (name == "D134") {
    return make_pair(undefined,0);  // Dolokorn 90
  } else if (name == "D21") {
    return make_pair(undefined,0);  // 40er Kali
  } else if (name == "D25") {
    return make_pair(undefined,0);  // Kaliumsulfat
  } else if (name == "D27") {
    return make_pair(undefined,0);  // Magnesia-Kainit
  } else if (name == "D28") {
    return make_pair(undefined,0);  // PK-Dünger mit S 12-24
  } else if (name == "D115") {
    return make_pair(undefined,0);  // Patentkali
  } else if (name == "D120") {
    return make_pair(undefined,0);  // P 40
  } else if (name == "D121") {
    return make_pair(undefined,0);  // Superphosphat
  } else if (name == "D124") {
    return make_pair(undefined,0);  //  PK 14-22
  } else if (name == "D116") {
    return make_pair(undefined,0);  // Thomaskali 6-16-6
  } else if (name == "D2") {
    return make_pair(undefined,0);  // Branntkalk
  } else if (name == "D6") {
    return make_pair(undefined,0);  // Kalkmergel Mg-Kalk 85
  } else if (name == "D152") {
      return make_pair(undefined,0);  // Kalkmergel Cao
  } else if (name == "D123") {
    return make_pair(undefined,0);  // PK-Dünger mit Mg 13-36-5
  } else if (name == "D135") {
    return make_pair(undefined,0);  // Calcium Carbonat
  } else if (name == "D147") {
    return make_pair(undefined,0);  // Mangan Chelat
  } else if (name == "D148") {
    return make_pair(undefined,0);  // Nutribor
  } else if (name == "D150") {
    return make_pair(undefined,0);  // Korn-Kali
  } else if (name == "D151") {
    return make_pair(undefined,0);  // Kalk, Optiflor 80/10
  } else if (name == "D153") {
    return make_pair(undefined,0);  // PK 9,9-25,8 (5+0)
  } else if (name == "D208") {
    return make_pair(undefined,0);  // Thomaskali
  } else if (name == "D205") {
    return make_pair(undefined,0);  // PK-Dünger mit Mg und S 11-22 (4+6)
  }else if (name == "0") {
    return make_pair(undefined,0);
  }

//    if (name == "KN")
//      return make_pair(mineral, 7); //0.00 1.00 0.00 01.00 M Kaliumnitrat (Einh : kg N / ha)
//    if (name == "AHL")
//      return make_pair(mineral, 10); //1.00 0.00 0.00 01.00   M Ammoniumharnstoffloesung
//    if (name == "AS")
//      return make_pair(mineral, 3); //1.00 0.00 0.00 01.00   M Ammoniumsulfat (Einh: kg N/ha)
//    if (name == "DAP")
//      return make_pair(mineral, 2); //1.00 0.00 0.00 01.00   M Diammoniumphosphat (Einh: kg N/ha)
//    if (name == "SG")
//      return make_pair(organic, 7); //0.67 0.00 1.00 06.70   O Schweineguelle (Einh: z. B. m3/ha)
//    if (name == "RG1")
//      return make_pair(organic, 3); //0.43 0.00 1.00 02.40   O Rinderguelle (Einh: z. B. m3/ha)
//    if (name == "RG2")
//      return make_pair(organic, 3); //0.43 0.00 1.00 01.80   O Rinderguelle (Einh: z. B. m3/ha)
//    if (name == "RG3")
//      return make_pair(organic, 3); //0.43 0.00 1.00 03.40   O Rinderguelle (Einh: z. B. m3/ha)
//    if (name == "RG4")
//      return make_pair(organic, 3); //0.43 0.00 1.00 03.70   O Rinderguelle (Einh: z. B. m3/ha)
//    if (name == "RG5")
//      return make_pair(organic, 3); //0.43 0.00 1.00 03.30   O Rinderguelle (Einh: z. B. m3/ha)
//    if (name == "SM")
//      return make_pair(organic, 1); //0.15 0.20 0.80 00.60   O Stallmist (Einh: z. B.  dt/ha)
//    if (name == "ST1")
//      return make_pair(organic, 1); //0.07 0.10 0.90 00.48   O Stallmist (Einh: z. B.  dt/ha)
//    if (name == "ST2")
//      return make_pair(organic, 1); //0.07 0.10 0.90 00.63   O Stallmist (Einh: z. B.  dt/ha)
//    if (name == "ST3")
//      return make_pair(organic, 1); //0.07 0.10 0.90 00.82   O Stallmist (Einh: z. B.  dt/ha)
//    if (name == "RM1")
//      return make_pair(organic, 1); //0.15 0.20 0.80 00.60   O Stallmist (Einh: z. B.  dt/ha)
//    if (name == "FM")
//      return make_pair(organic, 1); //0.65 0.80 0.20 01.00   O Stallmist (Einh: z. B.  kg N/ha)
//    if (name == "LM")
//      return make_pair(organic, 1); //0.85 0.80 0.20 01.00   O Jauche (Einh: z. B.  kg N/ha)
//    if (name == "H")
//      return make_pair(mineral, 8); //01.00 1.00 0.00 0.00 1.00 0.15 kg N/ha  M Harnstoff
//    if (name == "NPK")
//      return make_pair(mineral, 5); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha  M NPK Mineraldünger
//    if (name == "ALZ")
//      return make_pair(mineral, 8); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha  M Alzon
//    if (name == "AZU")
//      return make_pair(mineral, 1); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha  M Ansul
//    if (name == "NIT")
//      return make_pair(mineral, 5); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha  M Nitrophoska
//    if (name == "SSA")
//      return make_pair(mineral, 3); //01.00 1.00 0.00 0.00 1.00 0.10 kg N/ha  M schwefelsaures Ammoniak
//    if (name == "RG")
//      return make_pair(organic, 3); //04.70 0.43 0.00 1.00 1.00 0.40 m3 / ha  O Rindergülle
//    if (name == "RM")
//      return make_pair(organic, 1); //00.60 0.15 0.20 0.80 1.00 0.40 dt / ha  O Rindermist
//    if (name == "RSG")
//      return make_pair(organic, 3); //05.70 0.55 0.00 1.00 1.00 0.40 m3 / ha  O Rinder/Schweinegülle
//    if (name == "SSM")
//      return make_pair(organic, 5); //00.76 0.15 0.20 0.80 1.00 0.40 dt / ha  O Schweinemist
//    if (name == "HG")
//      return make_pair(organic, 12); //10.70 0.68 0.00 1.00 1.00 0.40 m3 / ha   O Hühnergülle
//    if (name == "HFM")
//      return make_pair(organic, 11); //02.30 0.15 0.20 0.80 1.00 0.40 dt / ha   O Hähnchentrockenmist
//    if (name == "HM")
//      return make_pair(organic, 11); //02.80 0.15 0.20 0.80 1.00 0.40 dt / ha   O Hühnermist
//    if (name == "CK")
//      return make_pair(mineral, 1); //00.30 0.00 1.00 0.00 0.00 0.00 dt / ha  M Calbokalk
//    if (name == "KSL")
//      return make_pair(organic, 16); //01.00 0.25 0.20 0.80 0.00 0.10 dt / ha   O Klärschlamm
//    if (name == "BAK")
//      return make_pair(organic, 15); //01.63 0.00 0.05 0.60 0.00 0.00 dt / ha   O Bioabfallkompst

    debug() << "Error - cannot find eva2 fertiliser \"" << name << "\" in known fertiliser list for MONICA!"
         << endl <<  "What should I do?" << endl;
    exit(-1);

    return make_pair(mineral, -1);
  }

/**
 * Returns a crop pointer with a crop id for eva2 crop ids.
 * Eva2 crop ids are defined in table S_Fruechte in database.
 * This method maps these ids to MONICA crop ids, that will be
 * needed for the crop parameters table.
 * @param eva2_crop
 * @return Pointer with crop
 */
CropPtr
Monica::getEva2CropId2Crop(std::string eva2_crop, int location)
{
  if(eva2_crop == EVA2_WINTER_WEIZEN) return CropPtr(new Crop(1, "Winterweizen"));
  if(eva2_crop == EVA2_SOMMER_WEIZEN) return CropPtr(new Crop(1, "Sommerweizen"));
  if(eva2_crop == EVA2_WINTER_GERSTE) return CropPtr(new Crop(2, "Wintergerste"));
  if(eva2_crop == EVA2_SOMMER_GERSTE) return CropPtr(new Crop(4, "Sommergerste"));
  if(eva2_crop == EVA2_WINTER_ROGGEN) return CropPtr(new Crop(3, "Winterroggen"));
  if(eva2_crop == EVA2_SOMMER_ROGGEN) return CropPtr(new Crop(20, "Sommerroggen"));
  if(eva2_crop == EVA2_WINTERTRITICALE) return CropPtr(new Crop(19, "Wintertriticale"));
  if(eva2_crop == EVA2_WINTER_ROGGEN_TRITICALE) return CropPtr(new Crop(19, "Winterroggen - Wintertriticale"));
  if(eva2_crop == EVA2_SOMMER_TRITICALE) return CropPtr(new Crop(23, "Sommertriticale"));
  if(eva2_crop == EVA2_SOMMER_ROGGEN_TRITICALE) return CropPtr(new Crop(23, "Sommerroggen - Sommertriticale"));


  if(eva2_crop == EVA2_ZUECKER_RUEBE) return CropPtr(new Crop(10, "Zuckerrübe"));
  if(eva2_crop == EVA2_SENF) return CropPtr(new Crop(11, "Senf"));

  if(eva2_crop == EVA2_WELSCHES_WEIDELGRAS) return CropPtr(new Crop(16, "Welsches Weidelgras"));
  if(eva2_crop == EVA2_EINJ_WEIDELGRAS) return CropPtr(new Crop(16, "Einjähriges Weidelgras"));
  if(eva2_crop == EVA2_BASTARD_WEIDELGRAS) return CropPtr(new Crop(16, "Bastard-Weidelgras"));
  if(eva2_crop == EVA_SUDANGRAS) return CropPtr(new Crop(18, "Sudangras"));
  if(eva2_crop == EVA2_OEL_RETTICH) return CropPtr(new Crop(17, "Ölrettich"));
  if(eva2_crop == EVA2_PHACELIA) return CropPtr(new Crop(12, "Phacelia"));
  if(eva2_crop == EVA2_FUTTERHIRSE) return CropPtr(new Crop(21, "Futterhirse"));

  if(eva2_crop == EVA2_KLEEGRAS) return CropPtr(new Crop(13, "Kleegras"));
  if(eva2_crop == EVA2_LUZERNEGRAS) return CropPtr(new Crop(14, "Luzerne"));
  if(eva2_crop == EVA2_LUZERNE_KLEEGRAS) return CropPtr(new Crop(15, "Luzerne - Kleegras"));

  if(eva2_crop == EVA2_HAFER_SORTENGEMISCH) return CropPtr(new Crop(22, "Hafer Sortengemisch"));
  if(eva2_crop == EVA2_HAFER) return CropPtr(new Crop(22, "Hafer"));
  if(eva2_crop == EVA2_KARTOFFEL) return CropPtr(new Crop(8, "Frühe Kartoffel"));
  //if(eva2_crop == EVA2_SONNENBLUME) return CropPtr(new Crop(9, "TODO: Parametersatz für Sonnenblume zuweisen"));
  if(eva2_crop == EVA2_ERBSE) return CropPtr(new Crop(25, "Erbse"));
  if(eva2_crop == EVA2_LANDSBERGER_GEMENGE) return CropPtr(new Crop(26, "Landsberger Gemenge"));


  




  // test if mais
  if(eva2_crop == EVA2_MAIS) {
    //if (location==27 || location == 11) {
        // special maize parameter set for werlte, ascha
      return CropPtr(new Crop(7, "Silage Maize"));  // Montoni, Fiacre
    //} else {
        // parameter set for other eva locations
    //  return CropPtr(new Crop(14, "High Yield Maize (Atletico, Coxximo"));  // Atletico , Coxximo
    //}
  }

  if(eva2_crop == EVA2_WINTER_RAPS) return CropPtr(new Crop(9, "Winterraps"));
  if(eva2_crop == EVA2_SOMMER_RAPS) return CropPtr(new Crop(50, "Sommerraps"));




//  if(eva2_crop == "GM") return CropPtr(new Crop(10, eva2_crop));
//  if(eva2_crop == "KS") return CropPtr(new Crop(8, eva2_crop));
//  if(eva2_crop == "KF") return CropPtr(new Crop(8, eva2_crop));
//  if(eva2_crop == "WC") return CropPtr(new Crop(9, eva2_crop));
//  if(eva2_crop == "SC") return CropPtr(new Crop(9, eva2_crop));
//  if(eva2_crop == "BR") return CropPtr(new Crop(eva2_crop));

  debug() << "Error - Cannot map an Eva2 crop "<< eva2_crop.c_str() << " to a parameterised crop." << endl;
  debug() << "Please add new parameter set to the crop table in monica database"
       << " and adapt method \"getEva2CropId2Crop()\" in source code!" << endl;
  exit(-1);
  return CropPtr();
}

