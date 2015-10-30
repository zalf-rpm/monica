/**
Authors: 
Jan Vaillant <jan.vaillant@zalf.de>

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
#include <iostream>
#include <mutex>

#include "tools/debug.h"
#include "tools/algorithms.h"
#include "soil/soil.h"
#include "../run/run-monica.h"
#include "../io/database-io.h"

#include "hermes-file-io.h"

using namespace Monica;
using namespace Tools;
using namespace std;

namespace
{
	//helper for parsing dates
	struct DMY
	{
		int d, m, y;
		Date toDate(bool useLeapYears = true) const
		{
			return Date(d, m, y, useLeapYears);
		}
	};

	// to read HERMES two digit date format in management files
	struct ParseDate
	{
		DMY operator()(const string & d)
		{
			DMY r;
			int length = d.size();

			if (length == 6) {
				// old HERMES format ddmmyy
				r.d = atoi(d.substr(0, 2).c_str());
				r.m = atoi(d.substr(2, 2).c_str());
				r.y = atoi(d.substr(4, 2).c_str());
				r.y = r.y <= 72 ? 2000 + r.y : 1900 + r.y;

			}
			else if (length == 8) {

				//ddmmyyyy
				r.d = atoi(d.substr(0, 2).c_str());
				r.m = atoi(d.substr(2, 2).c_str());
				r.y = atoi(d.substr(4, 4).c_str());

			}
			else {
				// other unexpected length
				cout << "ERROR - Cannot parse date \"" << d.c_str() << "\"" << endl;
				cout << "Should be of format DDMMYY or DDMMYYYY" << endl;
				cout << "Aborting simulation now!" << endl;
				exit(-1);
			}

			return r;
		}
	} parseDate;
}

//------------------------------------------------------------------------------------

Monica::CropPtr Monica::hermesCropId2Crop(const string& hermesCropId)
{
	if (hermesCropId == "WW")
		return CropPtr(new Crop(1, hermesCropId)); // Winter wheat
	if (hermesCropId == "SW")
		return CropPtr(new Crop(25, hermesCropId)); // Spring wheat
	if (hermesCropId == "WG")
		return CropPtr(new Crop(2, hermesCropId)); // Winter barley
	if (hermesCropId == "SG")
		return CropPtr(new Crop(4, hermesCropId)); // Spring barley
	if (hermesCropId == "WR")
		return CropPtr(new Crop(3, hermesCropId)); // Winter rye
	if (hermesCropId == "WR_GD")
		return CropPtr(new Crop(51, hermesCropId)); // Silage winter rye
	if (hermesCropId == "SR")
		return CropPtr(new Crop(20, hermesCropId)); // Spring rye
	if (hermesCropId == "OAT")
		return CropPtr(new Crop(22, hermesCropId)); // Oats
	if (hermesCropId == "ZR")
		return CropPtr(new Crop(10, hermesCropId)); // Sugar beet
	if (hermesCropId == "SM")
		return CropPtr(new Crop(7, hermesCropId)); // Silage maize
	if (hermesCropId == "GM")
		return CropPtr(new Crop(5, hermesCropId)); // Grain maize
	if (hermesCropId == "GMB")
		return CropPtr(new Crop(6, hermesCropId)); // Grain maize Brazil (Pioneer)
	if (hermesCropId == "MEP")
		return CropPtr(new Crop(8, hermesCropId)); // Early potato
	if (hermesCropId == "MLP")
		return CropPtr(new Crop(8, hermesCropId)); // Late potato
	if (hermesCropId == "WC")
		return CropPtr(new Crop(9, hermesCropId)); // Winter canola
	if (hermesCropId == "SC")
		return CropPtr(new Crop(9, hermesCropId)); // Spring canola
	if (hermesCropId == "MU")
		return CropPtr(new Crop(11, hermesCropId)); // Mustard
	if (hermesCropId == "PH")
		return CropPtr(new Crop(12, hermesCropId)); // Phacelia
	if (hermesCropId == "CLV")
		return CropPtr(new Crop(13, hermesCropId)); // Kleegras
	if (hermesCropId == "LZG")
		return CropPtr(new Crop(14, hermesCropId)); // Luzerne-Gras
	if (hermesCropId == "WDG")
		return CropPtr(new Crop(16, hermesCropId)); // Weidelgras
	if (hermesCropId == "FP")
		return CropPtr(new Crop(24, hermesCropId)); // Field pea
	if (hermesCropId == "OR")
		return CropPtr(new Crop(17, hermesCropId)); // Oil raddish
	if (hermesCropId == "SDG")
		return CropPtr(new Crop(18, hermesCropId)); // Sudan grass
	if (hermesCropId == "WTR")
		return CropPtr(new Crop(19, hermesCropId)); // Winter triticale
	if (hermesCropId == "STR")
		return CropPtr(new Crop(23, hermesCropId)); // Spring triticale
	if (hermesCropId == "SOR")
		return CropPtr(new Crop(21, hermesCropId)); // Sorghum
	if (hermesCropId == "SX0")
		return CropPtr(new Crop(28, hermesCropId)); // Soy bean maturity group 000
	if (hermesCropId == "S00")
		return CropPtr(new Crop(29, hermesCropId)); // Soy bean maturity group 00
	if (hermesCropId == "S0X")
		return CropPtr(new Crop(30, hermesCropId)); // Soy bean maturity group 0
	if (hermesCropId == "S01")
		return CropPtr(new Crop(31, hermesCropId)); // Soy bean maturity group I
	if (hermesCropId == "S02")
		return CropPtr(new Crop(32, hermesCropId)); // Soy bean maturity group II
	if (hermesCropId == "S03")
		return CropPtr(new Crop(33, hermesCropId)); // Soy bean maturity group III
	if (hermesCropId == "S04")
		return CropPtr(new Crop(34, hermesCropId)); // Soy bean maturity group IV
	if (hermesCropId == "S05")
		return CropPtr(new Crop(35, hermesCropId)); // Soy bean maturity group V
	if (hermesCropId == "S06")
		return CropPtr(new Crop(36, hermesCropId)); // Soy bean maturity group VI
	if (hermesCropId == "S07")
		return CropPtr(new Crop(37, hermesCropId)); // Soy bean maturity group VII
	if (hermesCropId == "S08")
		return CropPtr(new Crop(38, hermesCropId)); // Soy bean maturity group VIII
	if (hermesCropId == "S09")
		return CropPtr(new Crop(39, hermesCropId)); // Soy bean maturity group IX
	if (hermesCropId == "S10")
		return CropPtr(new Crop(40, hermesCropId)); // Soy bean maturity group X
	if (hermesCropId == "S11")
		return CropPtr(new Crop(41, hermesCropId)); // Soy bean maturity group XI
	if (hermesCropId == "S12")
		return CropPtr(new Crop(42, hermesCropId)); // Soy bean maturity group XII
	if (hermesCropId == "COS")
		return CropPtr(new Crop(43, hermesCropId)); // Cotton short
	if (hermesCropId == "COM")
		return CropPtr(new Crop(44, hermesCropId)); // Cotton medium
	if (hermesCropId == "COL")
		return CropPtr(new Crop(45, hermesCropId)); // Cotton long
	if (hermesCropId == "EMM")
		return CropPtr(new Crop(47, hermesCropId)); // Emmer 3000 b.c.
	if (hermesCropId == "EIN")
		return CropPtr(new Crop(48, hermesCropId)); // Einkorn 3000 b.c.
	if (hermesCropId == "COB")
		return CropPtr(new Crop(46, hermesCropId)); // Cotton medium Brazil
	if (hermesCropId == "SC")
		return CropPtr(new Crop(49, hermesCropId)); // Sugar cane transplant	
	if (hermesCropId == "SCT")
		return CropPtr(new Crop(50, hermesCropId)); // Sugar cane ratoon
	if (hermesCropId == "DUW")
		return CropPtr(new Crop(52, hermesCropId)); // Durum wheat
	if (hermesCropId == "FTO")
		return CropPtr(new Crop(53, hermesCropId)); // Field tomato
	if (hermesCropId == "BR")
		return CropPtr(new Crop(hermesCropId));


	return CropPtr();
}

//------------------------------------------------------------------------------------

pair<FertiliserType, string> Monica::hermesFertiliserName2monicaFertiliserId(const string& name)
{
	if (name == "KN")
		return make_pair(mineral, "PN"); //0.00 1.00 0.00 01.00 M Kaliumnitrat (Einh : kg N / ha)
	if (name == "KAS")
		return make_pair(mineral, "AN"); //1.00 0.00 0.00 01.00 M Kalkammonsalpeter (Einh : kg N / ha)
	if (name == "UR")
		return make_pair(mineral, "U"); //1.00 0.00 0.00 01.00   M Harnstoff
	if (name == "AHL")
		return make_pair(mineral, "UAS"); //1.00 0.00 0.00 01.00   M Ammoniumharnstoffloesung
	if (name == "UAN")
		return make_pair(mineral, "UAN"); //1.00 0.00 0.00 01.00   M Urea ammonium nitrate solution
	if (name == "AS")
		return make_pair(mineral, "AS"); //1.00 0.00 0.00 01.00   M Ammoniumsulfat (Einh: kg N/ha)
	if (name == "DAP")
		return make_pair(mineral, "AP"); //1.00 0.00 0.00 01.00   M Diammoniumphosphat (Einh: kg N/ha)
	if (name == "SG")
		return make_pair(organic, "PIS"); //0.67 0.00 1.00 06.70   O Schweineguelle (Einh: kg FM/ha)
	if (name == "SU")
		return make_pair(organic, "PIU"); //0.67 0.00 1.00 06.70   O Schweineurin (Einh: kg FM/ha)
	if (name == "RG1")
		return make_pair(organic, "CAS"); //0.43 0.00 1.00 02.40   O Rinderguelle (Einh: kg FM/ha)
	if (name == "RG2")
		return make_pair(organic, "CAS"); //0.43 0.00 1.00 01.80   O Rinderguelle (Einh: kg FM/ha)
	if (name == "RG3")
		return make_pair(organic, "CAS"); //0.43 0.00 1.00 03.40   O Rinderguelle (Einh: kg FM/ha)
	if (name == "RG4")
		return make_pair(organic, "CAS"); //0.43 0.00 1.00 03.70   O Rinderguelle (Einh: kg FM/ha)
	if (name == "RG5")
		return make_pair(organic, "CAS"); //0.43 0.00 1.00 03.30   O Rinderguelle (Einh: kg FM/ha)
	if (name == "SM")
		return make_pair(organic, "CADLM"); //0.15 0.20 0.80 00.60   O Stallmist (Einh: kg FM/ha)
	if (name == "ST1")
		return make_pair(organic, "CADLM"); //0.07 0.10 0.90 00.48   O Stallmist (Einh: kg FM/ha)
	if (name == "ST2")
		return make_pair(organic, "CADLM"); //0.07 0.10 0.90 00.63   O Stallmist (Einh: kg FM/ha)
	if (name == "ST3")
		return make_pair(organic, "CADLM"); //0.07 0.10 0.90 00.82   O Stallmist (Einh: kg FM/ha)
	if (name == "RM1")
		return make_pair(organic, "CAM"); //0.15 0.20 0.80 00.60   O Stallmist (Einh: kg FM/ha)
	if (name == "FM")
		return make_pair(organic, "CADLM"); //0.65 0.80 0.20 01.00   O Stallmist (Einh: kg FM/ha)
	if (name == "LM")
		return make_pair(organic, "CAS"); //0.85 0.80 0.20 01.00   O Jauche (Einh: kg FM/ha)
	if (name == "H")
		return make_pair(mineral, "U"); //01.00 1.00 0.00 0.00 1.00 0.15 kg N/ha 	M Harnstoff
	if (name == "NPK")
		return make_pair(mineral, "CP2"); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha 	M NPK Mineraldünger
	if (name == "ALZ")
		return make_pair(mineral, "U"); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha 	M Alzon
	if (name == "AZU")
		return make_pair(mineral, "AN"); //01.00 1.00 0.00 0.00 1.00 0.12 kg N/ha 	M Ansul
	if (name == "NIT")
		return make_pair(mineral, "CP2"); //01.00 1.00 0.00 0.00 0.00 0.10 kg N/ha 	M Nitrophoska
	if (name == "SSA")
		return make_pair(mineral, "AS"); //01.00 1.00 0.00 0.00 1.00 0.10 kg N/ha 	M schwefelsaures Ammoniak
	if (name == "RG")
		return make_pair(organic, "CAS"); //04.70 0.43 0.00 1.00 1.00 0.40 m3 / ha 	O Rindergülle (Einh: kg FM/ha)
	if (name == "RM")
		return make_pair(organic, "CADLM"); //00.60 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Rinderfestmist (Einh: kg FM/ha)
	if (name == "RM")
		return make_pair(organic, "CAM"); //00.60 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Rindermist (Einh: kg FM/ha)
	if (name == "RSG")
		return make_pair(organic, "CAS"); //05.70 0.55 0.00 1.00 1.00 0.40 m3 / ha 	O Rinder/Schweinegülle (Einh: kg FM/ha)
	if (name == "SM")
		return make_pair(organic, "PIM"); //00.76 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Schweinemist (Einh: kg FM/ha)
	if (name == "SSM")
		return make_pair(organic, "PIDLM"); //00.76 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Schweinefestmist (Einh: kg FM/ha)
	if (name == "HG")
		return make_pair(organic, "POM"); //10.70 0.68 0.00 1.00 1.00 0.40 m3 / ha 	O Hühnergülle (Einh: kg FM/ha)
	if (name == "HFM")
		return make_pair(organic, "PODLM"); //02.30 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Hähnchentrockenmist (Einh: kg FM/ha)
	if (name == "HM")
		return make_pair(organic, "PODLM"); //02.80 0.15 0.20 0.80 1.00 0.40 dt / ha 	O Hühnermist (Einh: kg FM/ha)
	if (name == "CK")
		return make_pair(mineral, "AN"); //00.30 0.00 1.00 0.00 0.00 0.00 dt / ha 	M Carbokalk
	if (name == "KSL")
		return make_pair(organic, "SS"); //01.00 0.25 0.20 0.80 0.00 0.10 dt / ha 	O Klärschlamm (Einh: kg FM/ha)
	if (name == "BAK")
		return make_pair(organic, "GWC"); //01.63 0.00 0.05 0.60 0.00 0.00 dt / ha 	O Bioabfallkompst (Einh: kg FM/ha)
	if (name == "MST")
		return make_pair(organic, "MS"); // Maize straw
	if (name == "WST")
		return make_pair(organic, "WS"); // Wheat straw
	if (name == "SST")
		return make_pair(organic, "SOY"); // Soybean straw
	if (name == "WEE")
		return make_pair(organic, "WEEDS"); // Weeds
	if (name == "YP3")
		return make_pair(mineral, "CF4"); //01.00 0.43 0.57 0.00 1.00 1.00 kg N/ha 	M Yara Pellon Y3
	if (name == "ASH")
		return make_pair(organic, "ASH"); // Ashes from burnt forest understorey

	cout << "Error: Cannot find fertiliser " << name << " in hermes fertiliser map. Aborting..." << endl;
	exit(-1);
	return make_pair(mineral, "");
}

//------------------------------------------------------------------------------------

vector<CultivationMethod>
Monica::cropRotationFromHermesFile(const string& pathToFile, bool useAutomaticHarvestTrigger, AutomaticHarvestParameters autoHarvestParams)
{
	vector<CultivationMethod> ff;

	ifstream ifs(pathToFile.c_str(), ios::binary);
	if (!ifs.good()) {
		cerr << "Could not open file " << pathToFile.c_str() << " . Aborting now!" << endl;
		exit(1);
	}

	string s;

	//skip first line
	getline(ifs, s);

	while (getline(ifs, s))
	{
		if (trim(s) == "end")
			break;

		istringstream ss(s);
		string crp;
		string sowingDate, harvestDate, tillageDate;
		double exp, tillage_depth;
		int t;
		ss >> t >> crp >> sowingDate >> harvestDate >> tillageDate >> exp >> tillage_depth;

		Date sd = parseDate(sowingDate).toDate(true);
		Date td = parseDate(tillageDate).toDate(true);
		Date hd;

		// tst if dates are valid
		if (!sd.isValid() || !td.isValid())
		{
			debug() << "Error - Invalid sowing or tillage date in \"" << pathToFile.c_str() << "\"" << endl;
			debug() << "Line: " << s.c_str() << endl;
			debug() << "Aborting simulation now!" << endl;
			exit(-1);
		}

		//create crop
		CropPtr crop = hermesCropId2Crop(crp);
		crop->setCropParameters(getCropParametersFromMonicaDB(crop->id()));
		crop->setResidueParameters(getResidueParametersFromMonicaDB(crop->id()));

		if (!useAutomaticHarvestTrigger) {
			// Do not use automatic harvest trigger
			// Adds harvest date from crop rotation file
			hd = parseDate(harvestDate).toDate(true);

			if (!hd.isValid()) {
				debug() << "Error - Invalid harvest date in \"" << pathToFile.c_str() << "\"" << endl;
				debug() << "Line: " << s.c_str() << endl;
				debug() << "Aborting simulation now!" << endl;
				exit(-1);
			}
		}
		else {

			debug() << "Activate automatic Harvest Trigger" << endl;

			// use harvest trigger
			// change latest harvest date to crop specific fallback harvest doy        
			autoHarvestParams.setLatestHarvestDOY(crop->cropParameters()->speciesParams.pc_LatestHarvestDoy);
			crop->activateAutomaticHarvestTrigger(autoHarvestParams);

			int harvest_year = sd.year();
			if (crp == "WW" || crp == "SW" || crp == "WG" || crp == "WR" || crp == "WR_GD" || crp == "SB" ||
				crp == "WC" || crp == "WTR")
			{
				// increment harvest year based on sowing year for winter crops
				// to determine harvest year for automatic harvest trigger
				harvest_year++;
			}

			debug() << "harvest_year:\t" << harvest_year << endl;

			// ###################################################################
			// # Important notes for the automatic harvest trigger (by XS)
			// ###################################################################        
			// @TODO: Change work flow of automatic harvest trigger
			// ###################################################################
			// Automatic harvest trigger works as follows:
			// If activated the latest harvest doy for the crop is automatically  used as
			// harvest date. A harvest application is added as usual when creating
			// the production process (constructor).
			// If the harvest trigger is activated during simulation (main loop in monica.cpp)
			// a new harvest application is created and directly applied without adding
			// it to the list of applications in the production process.
			// Because the fallback harvest application was already added as a harvest event
			// in the production process during creation of the PP, a crop may be harvested 
			// two times. But the apply method of the harvest application tests before
			// doing anything if there is a valid crop pointer. If not, nothing is done.
			// So if a harvest application is called a second time nothing happens ...
			// ###################################################################

			// set harvest date to latest crop specific fallback harvest date
			hd = Date::julianDate(crop->cropParameters()->speciesParams.pc_LatestHarvestDoy, harvest_year, true);

		}


		crop->setSeedAndHarvestDate(sd, hd);



		CultivationMethod pp(crop);
		pp.addApplication(TillageApplication(td, (tillage_depth / 100.0)));
		//cout << "production-process: " << pp.toString() << endl;

		ff.push_back(pp);
	}

	return ff;
}

//----------------------------------------------------------------------------

/**
* @todo Micha/Xenia: Überprüfen, ob YearIndex rauskommen kann.
*/
Climate::DataAccessor Monica::climateDataFromHermesFiles(const std::string& pathToFile,
	int fromYear, int toYear,
	const CentralParameterProvider& cpp,
	bool useLeapYears,
	double latitude)
{
	using namespace Climate;

	DataAccessor da(Date(1, 1, fromYear, useLeapYears), Date(31, 12, toYear, useLeapYears));

	vector<double> _tmin;
	vector<double> _tavg;
	vector<double> _tmax;
	vector<double> _globrad;
	vector<double> _relhumid;
	vector<double> _wind;
	vector<double> _precip;
	vector<double> _sunhours;

	Date date = Date(1, 1, fromYear, useLeapYears);

	for (int y = fromYear; y <= toYear; y++)
	{

		ostringstream yss;
		yss << y;
		string ys = yss.str();
		ostringstream oss;
		oss << pathToFile << ys.substr(1, 3);
		debug() << "File: " << oss.str().c_str() << endl;
		ifstream ifs(oss.str().c_str(), ios::binary);
		if (!ifs.good()) {
			cerr << "Could not open file " << oss.str().c_str() << " . Aborting now!" << endl;
			exit(1);
		}
		string s;

		//skip first line(s)
		getline(ifs, s);
		getline(ifs, s);
		getline(ifs, s);

		int daysCount = 0;
		int allowedDays = Date(31, 12, y, useLeapYears).dayOfYear();
		//    cout << "tavg\t" << "tmin\t" << "tmax\t" << "wind\t"
		debug() << "allowedDays: " << allowedDays << " " << y << "\t" << useLeapYears << "\tlatitude:\t" << latitude << endl;
		//<< "sunhours\t" << "globrad\t" << "precip\t" << "ti\t" << "relhumid\n";
		while (getline(ifs, s))
		{
			//if(trim(s) == "end") break;

			//Tp_av Tpmin Tpmax T_s10 T_s20 vappd wind sundu radia prec jday RF
			double td;
			int ti;
			double tmin, tmax, tavg, wind, sunhours, globrad, precip, relhumid;
			istringstream ss(s);

			ss >> tavg >> tmin >> tmax >> td >> td >> td >> wind
				>> sunhours >> globrad >> precip >> ti >> relhumid;

			// test if globrad or sunhours should be used
			if (globrad >= 0.0)
			{
				// use globrad
				// HERMES weather files deliver global radiation as [J cm-2]
				// Here, we push back [MJ m-2 d-1]
				double globradMJpm2pd = globrad * 100.0 * 100.0 / 1000000.0;
				_globrad.push_back(globradMJpm2pd);
			}
			else if (sunhours >= 0.0)
			{
				// invalid globrad use sunhours
				// convert sunhours into globrad
				// debug() << "Invalid globrad - use sunhours instead" << endl;
				_globrad.push_back(sunshine2globalRadiation(date.dayOfYear(), sunhours, latitude, true));
				_sunhours.push_back(sunhours);
			}
			else
			{
				// error case
				debug() << "Error: No global radiation or sunhours specified for day " << date.toString().c_str() << endl;
				debug() << "Aborting now ..." << endl;
				exit(-1);
			}

			if (relhumid >= 0.0) {
				_relhumid.push_back(relhumid);
			}

			// precipitation correction by Richter values
			precip *= cpp.getPrecipCorrectionValue(date.month() - 1);

			//      cout << tavg << "\t"<< tmin << "\t" << tmax << "\t" << wind
			//<< "\t" << sunhours <<"\t" << globrad <<"\t" << precip <<"\t" << ti <<"\t" << relhumid << endl;

			_tavg.push_back(tavg);
			_tmin.push_back(tmin);
			_tmax.push_back(tmax);
			_wind.push_back(wind);
			_precip.push_back(precip);


			daysCount++;
			date++;
		}

		if (daysCount != allowedDays)
		{
			debug() << "Wrong number of days in " << oss.str().c_str() << " ." << " Found " << daysCount << " days but should have been "
				<< allowedDays << " days. Aborting." << endl;
			exit(1);
		}
	}

	//int days = (toYearIndex - fromYearIndex + 1) * 365;
	//assert(int(_tmin.size()) == days);

	da.addClimateData(tmin, _tmin);
	da.addClimateData(tmax, _tmax);
	da.addClimateData(tavg, _tavg);
	da.addClimateData(globrad, _globrad);
	da.addClimateData(wind, _wind);
	da.addClimateData(precip, _precip);

	if (!_sunhours.empty())
		da.addClimateData(sunhours, _sunhours);

	if (!_relhumid.empty()) {
		da.addClimateData(relhumid, _relhumid);
	}

	return da;
}

//----------------------------------------------------------------------------

std::vector<CultivationMethod>
Monica::attachFertiliserSA(std::vector<CultivationMethod> cropRotation, const std::string pathToFertiliserFile)
{
	attachFertiliserApplicationsToCropRotation(cropRotation, pathToFertiliserFile);
	return cropRotation;
}

void
Monica::attachFertiliserApplicationsToCropRotation(std::vector<CultivationMethod>& cr,
	const std::string& pathToFile)
{
	ifstream ifs(pathToFile.c_str(), ios::binary);
	string s;

	std::vector<CultivationMethod>::iterator it = cr.begin();
	if (it == cr.end())
		return;

	//skip first line
	getline(ifs, s);

	Date currentEnd = it->endDate();
	while (getline(ifs, s))
	{
		if (trim(s) == "end")
			break;

		//Schlag_ID  N  FRT   Date
		double sid;
		double n;
		string frt;
		string sfdate;
		bool incorp;
		istringstream ss(s);
		ss >> sid >> n >> frt >> sfdate >> incorp;

		//get data parsed and to use leap years if the crop rotation uses them
		Date fdate = parseDate(sfdate).toDate(it->crop()->seedDate().useLeapYears());

		if (!fdate.isValid())
		{
			debug() << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
			debug() << "Line: " << s.c_str() << endl;
			debug() << "Aborting simulation now!" << endl;
			exit(-1);
		}

		//cout << "PP start: " << it->start().toString()
		//<< " PP end: " << it->end().toString() << endl;
		//cout << "fdate: " << fdate.toString()
		//<< " currentEnd: " << currentEnd.toString() << endl;

		//if the currently read fertiliser date is after the current end
		//of the crop, move as long through the crop rotation as
		//we find an end date that lies after the currently read fertiliser date
		while (fdate > currentEnd)
		{
			//move to next crop and possibly exit at the end
			it++;
			if (it == cr.end())
				break;

			currentEnd = it->endDate();

			//cout << "new PP start: " << it->start().toString()
			//<< " new PP end: " << it->end().toString() << endl;
			//cout << "new currentEnd: " << currentEnd.toString() << endl;

		}

		//which type and id is the current fertiliser
		auto fertTypeAndId = hermesFertiliserName2monicaFertiliserId(frt);
		switch (fertTypeAndId.first)
		{
		case mineral:
		{
			//create mineral fertiliser application
			const MineralFertiliserParameters& mfp = getMineralFertiliserParametersFromMonicaDB(fertTypeAndId.second);
			it->addApplication(MineralFertiliserApplication(fdate, mfp, n));
			break;
		}
		case organic:
		{
			//create organic fertiliser application
			auto omp = getOrganicFertiliserParametersFromMonicaDB(fertTypeAndId.second);
			//omp->vo_NConcentration = 100.0;
			it->addApplication(OrganicFertiliserApplication(fdate, omp, n, incorp));
			break;
		}
		case undefined:
		{
			break;
		}
		}

		//cout << "----------------------------------" << endl;
	}

}

//------------------------------------------------------------------------------

void
Monica::attachIrrigationApplicationsToCropRotation(std::vector<CultivationMethod>& cr,
	const std::string& pathToFile)
{
	ifstream ifs(pathToFile.c_str(), ios::in);
	if (!ifs.is_open()) {
		return;
	}
	string s;

	std::vector<CultivationMethod>::iterator it = cr.begin();
	if (it == cr.end())
		return;

	//skip first line
	getline(ifs, s);

	Date currentEnd = it->endDate();
	while (getline(ifs, s))
	{
		if (trim(s) == "end")
			break;

		//Field_ID  mm SCc IrrDat NCc
		double fid;
		int mm;
		double scc; //sulfate concentration [mg dm-3]
		string irrDate;
		double ncc; //nitrate concentration [mg dm-3]
		istringstream ss(s);
		ss >> fid >> mm >> scc >> irrDate >> ncc;

		//get data parsed and to use leap years if the crop rotation uses them
		Date idate = parseDate(irrDate).toDate(it->crop()->seedDate().useLeapYears());
		if (!idate.isValid())
		{
			debug() << "Error - Invalid date in \"" << pathToFile.c_str() << "\"" << endl;
			debug() << "Line: " << s.c_str() << endl;
			debug() << "Aborting simulation now!" << endl;
			exit(-1);
		}

		//cout << "PP start: " << it->start().toString()
		//<< " PP end: " << it->end().toString() << endl;
		//cout << "irrigationDate: " << idate.toString()
		//<< " currentEnd: " << currentEnd.toString() << endl;

		//if the currently read irrigation date is after the current end
		//of the crop, move as long through the crop rotation as
		//we find an end date that lies after the currently read irrigation date
		while (idate > currentEnd)
		{
			//move to next crop and possibly exit at the end
			it++;
			if (it == cr.end())
				break;

			currentEnd = it->endDate();

			//cout << "new PP start: " << it->start().toString()
			//<< " new PP end: " << it->end().toString() << endl;
			//cout << "new currentEnd: " << currentEnd.toString() << endl;
		}

		//finally add the application to the current crops list
		it->addApplication(IrrigationApplication(idate, mm, IrrigationParameters(ncc, scc)));

		//cout << "----------------------------------" << endl;
	}
}

//------------------------------------------------------------------------------
