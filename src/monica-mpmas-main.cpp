/**
Authors: 
Dr. Claas Nendel <claas.nendel@zalf.de>
Xenia Specka <xenia.specka@zalf.de>
Michael Berg <michael.berg@zalf.de>

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
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include "db/db.h"
#include "db/abstract-db-connections.h"
#include "tools/date.h"
#include "tools/algorithms.h"
#include "tools/helper.h"
#include "tools/debug.h"
#include "soil/soil.h"

#include "monica.h"
#include "monica-parameters.h"
#include "simulation.h"
#include "carbiocial.h"

#include "MpmasMaster.h"

using namespace std;
using namespace Db;
using namespace Tools;
using namespace Monica;

/*
namespace mpmasScope
{	// just for now, so that this simple exit function can still be used
	mpmas* mpmasPointer;
	void exitfun();
}

//using namespace mpmasScope;

void mpmasScope::exitfun()
{
	cout << "EXITING ..." << endl;
	
	//deallocateAll();
	//delete mpmasPointer;

	cout << "-------------------------------------------------------------------------" << endl << "SimName:  "
			<< SIMNAME().c_str() << endl;
	cout << "INDIR():  " << INDIR().c_str() << " true: " << strcmp(INDIR().c_str(), ".") << endl;
	cout << "OUTDIR(): " << OUTDIR().c_str() << " true: " << strcmp(OUTDIR().c_str(), ".") << endl;
	cout << "XMLDIR(): " << XMLDIR().c_str() << endl;
	cout << "KEYDIR(): " << KEYDIR().c_str() << endl
			<< "-------------------------------------------------------------------------" << endl;
	//exit(1);
}
*/

/*

	Mpmas programm call:
	
	-IBRA009 -OBRA009 -NBSL_ -T19 -T82 -Y1

	-I path to input folders
	-O path to output folders
	-N name of simulation scenario

	Optional flags for debugging:
	
	-T19 writes exported land-use table to BRA009/out/BSL_CatchMap0cropAct_01.asc and 
	 imported yields to BRA009/input/dat/BSL_CropYields01.dat
	 
	-T82 writes headers into both files
	 
	-Y1 makes Mpmas stop after one period (input files contain data for more years)
	 
*/

int main(int argc, char** argv)
{
#if WIN32
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");

    //use the non-default db-conections-core.ini
		dbConnectionParameters("db-connections.ini");
#endif

		//-BBRA018/input/dat/tf__InactiveSectors0.dat

#ifdef MONICA
		/* in case that sectors should be disabled during initialization, call Mpmas with option -B[path to controlFile] 
		e.g.: [workingDir\InactiveSectors.dat]

		This file must contain the following information:

		Number of sectors to disable
		List of Sector IDs (here: GIS-ID)
		*/
#endif
		
		//create instance of Mpmas and set pointer for exit function
		mpmas mpmas(argc, argv);
		//mpmasScope::mpmasPointer = &mpmas;

#ifdef MONICA
		//just for debugging ...
		const int numCropActs = 125;

		//allocate memory
		mpmas.allocateMemoryForMonica(numCropActs);

#endif
					
		//get length of simulation horizon and number of spin-up rounds
		int numYears = mpmas.getNumberOfYearsToSimulate();
		int numSpinUp = mpmas.getNumberOfSpinUpRounds();

		//loop over simulation horizon, starting with end of period 0
		//in case of spin-up, start counting with negative value
		for (int year = -numSpinUp; year < numYears; year++)
		{	
#ifndef MONICA
			//run through one year completely
			int rtcode = mpmas.simulateOnePeriodComplete(year);

			//stop prematurely if needed
			if(0 < rtcode)
				break;
#else

			// create arrays for external crop growth model
			// just for debugging ...
			int cropActIDX[numCropActs];
			double cropAreaX[numCropActs];
			// ...

			//just for debugging
			int numDisabled = 1;
			int disabledCropActID[1];
			disabledCropActID[0] = 2;

			//if needed, exclude crop activities from being grown
			//mpmas.disableCropActivities(numDisabled, disabledCropActID);

			//-BBRA020/input/dat/tf__InactiveSectors0.dat

			//just for debugging
			//int catchmentID = 2;
			/*
			int numDisabledSectors = 1;
			int disabledSectorID2[408];
			int disabledSectorID4[96];
			for(int i = 0; i < 408; i++)
				disabledSectorID2[i] = Carbiocial::makeInactiveSectorId(2, i+1);
			for(int i = 0; i < 96; i++)
				disabledSectorID4[i] = Carbiocial::makeInactiveSectorId(4, i);
			
			//if needed, exclude specific sectors from simulation (their agents will be deleted)
			mpmas.disableAgentsInSectors(2, 408, disabledSectorID2);
			mpmas.disableAgentsInSectors(4, 96, disabledSectorID4);
			*/

			//export land-use maps
			int rtcode1 = mpmas.simulateOnePeriodExportingLandUse(year, numCropActs, cropActIDX, cropAreaX);

			//stop prematurely if needed
			if(0 < rtcode1)
				break;

			//here: call external crop growth model
			MatrixDouble cropActIDsFromFile;
			cropActIDsFromFile.readFromFileWithDims("BRA020/cropActIds.dat");
			MatrixDouble cropYieldsFromFile;
			//cropYieldsFromFile.readFromFileWithDims("C:/Users/michael/development/GitHub/monica/monica-mpmas/BRA020/cropYields.dat");
			cropYieldsFromFile.readFromFileWithDims("./BRA020/cropYields.dat");

			if(TestFun(19))//just to debug
			{	
				cropActIDsFromFile.writeToFile("ReadCropActIds.txt");
				cropYieldsFromFile.writeToFile("ReadCropYields.txt");
			}

			// some values, just for debugging ...
			int cropActIDM[numCropActs];
			double cropYieldM[numCropActs];
			double stoverYieldM[numCropActs];

			for(int i = 0; i < numCropActs; i++)
			{
				cropActIDM[i] = (int)cropActIDsFromFile.getValue(i);
				cropYieldM[i] = cropYieldsFromFile.getValue(i);
				stoverYieldM[i] = 0.0;
			}
			// ...


			//import yields maps
			int rtcode2 = mpmas.simulateOnePeriodImportingYields(year, numCropActs, cropActIDM, cropYieldM, stoverYieldM);

			// some fake values, just for debugging ...
			//double aggregateFarmIncome;
			//double aggregateGrossMargins[numCropActs];

			//aggregate economic indicators
			//(void)mpmas.simulateOnePeriodAgentIncomes(year, aggregateFarmIncome, numCropActs, cropActIDM, aggregateGrossMargins);

			// some static arrays, just for debugging ...
			int numAgents = 3;
			int agentID[3];
			agentID[0] = 4000701;
			agentID[1] = 4000702;
			agentID[2] = 4000703;
			double farmIncome[3];
			double grossMarginsAgent0[numCropActs];
			double grossMarginsAgent1[numCropActs];
			double grossMarginsAgent2[numCropActs];
			double* individualGrossMargins[3];
			individualGrossMargins[0] = grossMarginsAgent0;
			individualGrossMargins[1] = grossMarginsAgent1;
			individualGrossMargins[2] = grossMarginsAgent2;

			//get also economic indicators of selected agents
			(void)mpmas.getPerformanceDataForSelectedAgents(numAgents, agentID, farmIncome, numCropActs, cropActIDM, individualGrossMargins);

			//stop prematurely if needed
			if(0 < rtcode2)
				break;
#endif
		}

#ifdef MONICA
		//deallocate memory
		mpmas.deallocateMemoryForMonica();

#endif
		
		return 0;
}
