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

#ifndef EVA_METHODS_H_
#define EVA_METHODS_H_

/**
* @file eva_methods.h
*/

#include <iostream>
#include <string>
#include <fstream>

#include "climate/climate-common.h"
#include "tools/date.h"
#include "monica-parameters.h"
#include "soilcolumn.h"
#include "soiltemperature.h"
#include "soilmoisture.h"
#include "soilorganic.h"
#include "soiltransport.h"
#include "crop.h"
#include "monica.h"
#include "soil/soil.h"

namespace Monica 
{
	/**
	* Profil numbers for eva2 locations with different profiles. The
	* number of profiles can be found in eva2 database in table
	* Tab_BoGeografie
	* @brief Profil number needed for soil parameters
	*/
	enum Eva2_Profil {
		PROFILE_DORNBURG_1=1,   //!< DORNBURG - Profil 1
		PROFILE_DORNBURG_2=5,
		PROFILE_DORNBURG_3=6,
		PROFILE_WERLTE_81=81,
		PROFILE_WERLTE_82=82,
		PROFILE_WERLTE_83=83,
		PROFILE_WERLTE_84=84,
		PROFILE_WERLTE_85=85
	};

	enum Eva2_Standort {
		LOCATION_AHOLFING=10,
		LOCATION_ASCHA=11,
		LOCATION_BANDOW=12,
		LOCATION_BERGE = 13,
		LOCATION_BRUCHHAUSEN = 14,
		LOCATION_BURKERSDORF = 15,
		LOCATION_DORNBURG=16,
		LOCATION_ETTLINGEN = 17,
		LOCATION_GUELZOW_GRENZSTANDORT = 18,
		LOCATION_GUETERFELDE = 19,
		LOCATION_HAUS_DUESSE = 20,
		LOCATION_OBERWEISSBACH = 21,
		LOCATION_PAULINENAUE = 22,
		LOCATION_RAUISCHHOLZHAUSEN = 23,
		LOCATION_STRAUBING = 24,
		LOCATION_TROSSIN=25,
		LOCATION_WEHNEN=26,
		LOCATION_WERLTE=27,
		LOCATION_WITZENHAUSEN=28,
		LOCATION_GROSS_KREUTZ=29,
		LOCATION_DOLGELIN=30,
		LOCATION_SOPHIENHOF=31,
		LOCATION_BRAMSTEDT=32,
		LOCATION_VRESCHEN_BOKEL=33,
		LOCATION_HAUFELD=34,
		LOCATION_GUELZOW_OEKO=35,
		LOCATION_BERNBURG=44
	};

	enum Eva2_Klassifikation {
		GRUNDVERSUCH = 1
	};


	enum Eva2_Variante {
		ANLAGE1 = 1
	};

	// eva2 fruchtart

#define EVA2_MAIS "141"
#define EVA2_SOMMER_GERSTE "145"
#define EVA2_SOMMER_ROGGEN "147"
#define EVA2_SOMMER_ROGGEN_TRITICALE "148"
#define EVA2_SOMMER_TRITICALE "150"
#define EVA2_SOMMER_WEIZEN "151"
#define EVA2_SOMMER_RAPS "211"

#define EVA2_SONNENBLUME "157"
#define EVA_SUDANGRAS "160"
#define EVA2_WINTER_GERSTE "164"
#define EVA2_WINTER_RAPS "170"
#define EVA2_WINTER_ROGGEN "172"
#define EVA2_WINTERTRITICALE "175"
#define EVA2_WINTER_WEIZEN "176"
#define EVA2_WINTER_ROGGEN_TRITICALE "222"

#define EVA2_FUTTERHIRSE "180"
#define EVA2_ZUECKER_RUEBE "181"
#define EVA2_EINJ_WEIDELGRAS "182"
#define EVA2_BASTARD_WEIDELGRAS "183"
#define EVA2_WELSCHES_WEIDELGRAS "179"

#define EVA2_OEL_RETTICH "041"
#define EVA2_SENF "043"
#define EVA2_PHACELIA "025"
#define EVA2_LANDSBERGER_GEMENGE "020"
#define EVA2_ERBSE "111"
#define EVA2_KLEEGRAS "128"
#define EVA2_LUZERNEGRAS "139"
#define EVA2_LUZERNE_KLEEGRAS "140"
#define EVA2_HAFER_SORTENGEMISCH "124"
#define EVA2_HAFER "120"
#define EVA2_KARTOFFEL "127"

	/**
	* @brief ID parameter used in EVA2 weather database; can be looked up in table S_Parameter.
	*/
	enum Eva2_WetterIdParameter {
		TAVG = 12000,      //!< TAVG
		TMIN = 12003,      //!< TMIN
		TMAX = 12006,      //!< TMAX
		GLOBRAD = 42009,   //!< GLOBRAD = global radation
		RELHUMID = 62000,
		WIND = 92500,
		WIND_3m = 93000,
		WIND_2m = 92000,
		WIND_2_50m = 92500,
		WIND_10m = 91000,
		WIND_8m = 98000,
		WIND_19m = 91900,
		PRECIP = 31009,
		SUNHOURS = 52009,
		WETTER_PARAMETER_COUNT=8
	};


	/**
	*
	*/
	class WStation
	{
	public:

		WStation() { id=0; name=""; }
		~WStation(){}

		std::string name;
		int id;
		Tools::Date start;
		Tools::Date end;

	};

	const Soil::SoilPMs* readSoilParametersForEva2(const GeneralParameters& gps, int profil_nr, int standort_id, int variante = -1);
	Climate::DataAccessor climateDataFromEva2DB(int location, int profil_nr, Tools::Date start_date, Tools::Date end_date, CentralParameterProvider& cpp, double latitude = 0.0);
	SiteParameters readSiteParametersForEva2(int location, int profil_nr);
	double getEffictiveRootingDepth(int profile);
	std::vector<WStation> getIDOfWStation(int profil_nr, int parameter);
	std::string getNameOfWStation(int station);
	std::string getFilename(int station, int profil_nr);
	double checkUnit(int, std::string);
	void readPrecipitationCorrectionValues(CentralParameterProvider& cpp);
	void readGroundwaterInfos(CentralParameterProvider& cpp, int location);


	std::vector<ProductionProcess> getCropManagementData(std::string id_string, std::string eva2_crop, int location=0);
	CropPtr getEva2CropId2Crop(std::string eva2_crop, int location=0);
	std::pair<FertiliserType, int> eva2FertiliserId2monicaFertiliserId(const std::string& name);
	OrganicMatterParameters* getOrganicFertiliserDetails(OrganicMatterParameters *omp, std::string fert_id);
	double getNPercentageInFertilizer(std::string);
	double getOrganicFertiliserConversionFactor(std::string fert_id);

	//double round(double number, int decimal=0);

} // namespace Monica

#endif

