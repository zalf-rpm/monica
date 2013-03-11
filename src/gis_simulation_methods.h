/**
 * @file gis_simulation_methods.h
 */

#ifndef GIS_SIMULATION_METHODS_H_
#define GIS_SIMULATION_METHODS_H_

#if defined RUN_GIS

#include "monica-parameters.h"
#include "tools/date.h"
#include "climate/climate.h"
#include "tools/coord-trans.h"
#include "monica.h"
#include "interpolation/interpol.h"
#include "grid/grid.h"
#include "monica.h"


namespace Monica
{

Climate::DataAccessor getClimateDateOfThuringiaStation(char * station, std::string start_date_s, std::string end_date_s, CentralParameterProvider& cpp);


Monica::Result runSinglePointSimulation(char* station_id, int soiltype, double slope, double height_nn, double gw, std::string start_date_s, std::string end_date_s, double julian_sowing_date, std::string path);
Monica::Result createGISSimulation(int x, int y, std::string start_date_s, std::string end_date_s, double julian_sowing_date, char* hdf_filename, char* hdf_voronoi, std::string path, int ext_buek_id);

Monica::Result createGISSimulationSingleStation(int row, int col, std::string start_date_s, std::string end_date_s, double julian_sowing_date, char* station_id,  char* hdf_filename, std::string path, int soiltype = -1);
Monica::Result runSinglePointSimulation(char* station_id, int soiltype, double slope, double height_nn, double gw, std::string start_date_s, std::string end_date_s, double julian_sowing_date, std::string path);


std::vector<double> getClimateInformation(int x, int y, int date_index, char* hdf_filename, char* hdf_voronoi);

std::string getThurStationName(int stat_id);


} // Monica

#endif

#endif /*GIS_SIMULATION_METHODS_H_*/

