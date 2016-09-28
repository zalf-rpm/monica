/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors:
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include <thread>
#include <tuple>

#include "zmq.hpp"
#include "zhelpers.hpp"

#include "json11/json11.hpp"

#include "serve-monica-zmq.h"
#include "cultivation-method.h"
#include "tools/debug.h"
#include "../io/database-io.h"
#include "run-monica.h"
#include "tools/zmq-helper.h"
#include "../io/output.h"
#include "climate/climate-file-io.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Soil;
using namespace Climate;

/*
json11::Json createHarvestingMessage(CMResult result, const MonicaModel& monica)
{
  result.results[sumFertiliser] = monica.sumFertiliser();
  result.results[daysWithCrop] = monica.daysWithCrop();
  result.results[NStress] = monica.getAccumulatedNStress();
  result.results[WaterStress] = monica.getAccumulatedWaterStress();
  result.results[HeatStress] = monica.getAccumulatedHeatStress();
  result.results[OxygenStress] = monica.getAccumulatedOxygenStress();

	//    cout << "created harvesting msg: " << result.to_json().dump() << endl;
	return result.to_json();
}

json11::Json createSoilResultsMessage(const MonicaModel& monica)
{
	//    const SoilTemperature& mst = monica.soilTemperature();
	const SoilMoisture& msm = monica.soilMoisture();
  auto mcg = ((MonicaModel&)monica).cropGrowth();
	//    const SoilOrganic& mso = monica.soilOrganic();
	//    const SoilColumn& msc = monica.soilColumn();
	//    const SoilTransport& msq = monica.soilTransport();

	json11::Json::array sms;
	int outLayers = 20;
	for (int i_Layer = 0; i_Layer < outLayers; i_Layer++)
		sms.push_back(msm.get_SoilMoisture(i_Layer)*100.0);

	auto soilMsg = json11::Json::object{
	{"soilmoistures", sms},
	{to_string(avg30_60cmSoilMoisture), monica.avgSoilMoisture(3,6)},
  {to_string(leachingNAtBoundary), monica.nLeaching()},
  {"rootingDepth", mcg ? mcg->get_RootingDepth() : -1}};

	//    cout << "created soilmoisture msg: " << Json(soilMsg).dump() << endl;
	return soilMsg;
}

json11::Json createMarch31stResultsMessage(const MonicaModel& monica)
{
	auto jsonMsg = json11::Json::object{
	{to_string(sum90cmYearlyNatDay), monica.sumNmin(0.9)},
	{to_string(sum30cmSoilTemperature), monica.sumSoilTemperature(3)},
	{to_string(sum90cmYearlyNO3AtDay), monica.sumNO3AtDay(0.9)},
	{to_string(avg30cmSoilTemperature), monica.avg30cmSoilTemperature()},
	{to_string(avg0_30cmSoilMoisture), monica.avgSoilMoisture(0,3)},
	{to_string(avg30_60cmSoilMoisture), monica.avgSoilMoisture(3,6)},
	{to_string(avg60_90cmSoilMoisture), monica.avgSoilMoisture(6,9)},
	{to_string(avg0_90cmSoilMoisture), monica.avgSoilMoisture(0,9)},
	{to_string(waterFluxAtLowerBoundary), monica.groundWaterRecharge()},
	{to_string(avg0_30cmCapillaryRise), monica.avgCapillaryRise(0,3)},
	{to_string(avg30_60cmCapillaryRise), monica.avgCapillaryRise(3,6)},
	{to_string(avg60_90cmCapillaryRise), monica.avgCapillaryRise(6,9)},
	{to_string(avg0_30cmPercolationRate), monica.avgPercolationRate(0,3)},
	{to_string(avg30_60cmPercolationRate), monica.avgPercolationRate(3,6)},
	{to_string(avg60_90cmPercolationRate), monica.avgPercolationRate(6,9)},
	{to_string(evapotranspiration), monica.getEvapotranspiration()},
	{to_string(transpiration), monica.getTranspiration()},
	{to_string(evaporation), monica.getEvaporation()},
	{to_string(sum30cmSMB_CO2EvolutionRate), monica.get_sum30cmSMB_CO2EvolutionRate()},
	{to_string(NH3Volatilised), monica.getNH3Volatilised()},
	{to_string(sum30cmActDenitrificationRate), monica.getsum30cmActDenitrificationRate()},
	{to_string(leachingNAtBoundary), monica.nLeaching()} };

	return jsonMsg;
}

void aggregateValues(map<ResultId, double>& avs, 
										 map<Climate::ACD, double>& climateData, 
										 const MonicaModel& monica)
{
	avs[avg10cmMonthlyAvgCorg] += monica.avgCorg(0.1);
	avs[avg30cmMonthlyAvgCorg] += monica.avgCorg(0.3);
	avs[mean90cmMonthlyAvgWaterContent] += monica.mean90cmWaterContent();
	avs[monthlySumGroundWaterRecharge] += monica.groundWaterRecharge();
	avs[monthlySumNLeaching] += monica.nLeaching();
	avs[monthlySurfaceRunoff] += monica.surfaceRunoff();
	avs[monthlyPrecip] += climateData[Climate::precip];
	avs[monthlyETa] += monica.getETa();

	avs[yearlySumGroundWaterRecharge] += monica.groundWaterRecharge();
	avs[yearlySumNLeaching] += monica.nLeaching();
}

json11::Json createMonthlyResultsMessage(Date date, 
																				 map<ResultId, double>& avs, 
																				 const MonicaModel& monica)
{
	auto jsonMsg = json11::Json::object{
	{to_string(avg10cmMonthlyAvgCorg), avs[avg10cmMonthlyAvgCorg] / double(date.daysInMonth())},
	{to_string(avg30cmMonthlyAvgCorg), avs[avg30cmMonthlyAvgCorg] / double(date.daysInMonth())},
	{to_string(mean90cmMonthlyAvgWaterContent), monica.mean90cmWaterContent()},
	{to_string(monthlySumGroundWaterRecharge), avs[monthlySumGroundWaterRecharge]},
	{to_string(monthlySumNLeaching), avs[monthlySumNLeaching]},
	{to_string(maxSnowDepth), monica.maxSnowDepth()},
	{to_string(sumSnowDepth), monica.getAccumulatedSnowDepth()},
	{to_string(sumFrostDepth), monica.getAccumulatedFrostDepth()},
	{to_string(sumSurfaceRunOff), monica.sumSurfaceRunOff()},
	{to_string(sumNH3Volatilised), monica.getSumNH3Volatilised()},
	{to_string(monthlySurfaceRunoff), avs[monthlySurfaceRunoff]},
	{to_string(monthlyPrecip), avs[monthlyPrecip]},
	{to_string(monthlyETa), avs[monthlyETa]},
	{to_string(monthlySoilMoistureL0), monica.avgSoilMoisture(0,1) * 100.0},
	{to_string(monthlySoilMoistureL1), monica.avgSoilMoisture(1,2) * 100.0},
	{to_string(monthlySoilMoistureL2), monica.avgSoilMoisture(2,3) * 100.0},
	{to_string(monthlySoilMoistureL3), monica.avgSoilMoisture(3,4) * 100.0},
	{to_string(monthlySoilMoistureL4), monica.avgSoilMoisture(4,5) * 100.0},
	{to_string(monthlySoilMoistureL5), monica.avgSoilMoisture(5,6) * 100.0},
	{to_string(monthlySoilMoistureL6), monica.avgSoilMoisture(6,7) * 100.0},
	{to_string(monthlySoilMoistureL7), monica.avgSoilMoisture(7,8) * 100.0},
	{to_string(monthlySoilMoistureL8), monica.avgSoilMoisture(8,9) * 100.0},
	{to_string(monthlySoilMoistureL9), monica.avgSoilMoisture(9,10) * 100.0},
	{to_string(monthlySoilMoistureL10), monica.avgSoilMoisture(10,11) * 100.0},
	{to_string(monthlySoilMoistureL11), monica.avgSoilMoisture(11,12) * 100.0},
	{to_string(monthlySoilMoistureL12), monica.avgSoilMoisture(12,13) * 100.0},
	{to_string(monthlySoilMoistureL13), monica.avgSoilMoisture(13,14) * 100.0},
	{to_string(monthlySoilMoistureL14), monica.avgSoilMoisture(14,15) * 100.0},
	{to_string(monthlySoilMoistureL15), monica.avgSoilMoisture(15,16) * 100.0},
	{to_string(monthlySoilMoistureL16), monica.avgSoilMoisture(16,17) * 100.0},
	{to_string(monthlySoilMoistureL17), monica.avgSoilMoisture(17,18) * 100.0},
	{to_string(monthlySoilMoistureL18), monica.avgSoilMoisture(18,19) * 100.0} };

	avs[avg10cmMonthlyAvgCorg] = 0;
	avs[avg30cmMonthlyAvgCorg] = 0;
	avs[mean90cmMonthlyAvgWaterContent] = 0;
	avs[monthlySumGroundWaterRecharge] = 0;
	avs[monthlySumNLeaching] = 0;
	avs[monthlySurfaceRunoff] = 0;
	avs[monthlyPrecip] = 0;
	avs[monthlyETa] = 0;

	return jsonMsg;
}

json11::Json createYearlyResultsMessage(map<ResultId, double>& avs)
{
	auto jsonMsg = json11::Json::object{
	{to_string(yearlySumGroundWaterRecharge), avs[yearlySumGroundWaterRecharge]},
	{to_string(yearlySumNLeaching), avs[yearlySumNLeaching]} };

	avs[yearlySumGroundWaterRecharge] = 0;
	avs[yearlySumNLeaching] = 0;

	return jsonMsg;
}
*/

void Monica::startZeroMQMonica(zmq::context_t* zmqContext, 
															 string inputSocketAddress, 
															 string outputSocketAddress, 
															 bool isInProcess)
{
	/*
  zmq::socket_t input(*zmqContext, isInProcess ? ZMQ_PAIR : ZMQ_PULL);
	debug() << "MONICA: connecting monica zeromq input socket to address: " << inputSocketAddress << endl;
	try
	{
		input.connect(inputSocketAddress.c_str());
		debug() << "MONICA: connected monica zeromq input socket to address: " << inputSocketAddress << endl;

		zmq::socket_t output_(*zmqContext, ZMQ_PUSH);
		zmq::socket_t& output = isInProcess ? input : output_;
		if(isInProcess)
			debug() << "MONICA: using monica zeromq pair input socket also as output socket at address: " << outputSocketAddress << endl;
		else
		{
			debug() << "MONICA: binding monica zeromq output socket to address: " << outputSocketAddress << endl;
			output.bind(outputSocketAddress.c_str());
			debug() << "MONICA: bound monica zeromq output socket to address: " << outputSocketAddress << endl;
		}

		unique_ptr<MonicaModel> monicaUPtr;
		map<ResultId, double> aggregatedValues;

		//the possibly active crop
		CropPtr crop;
		int customId = -1;
		int prevDevStage = 0;
		while(true)
		{
			try
			{
				auto msg = receiveMsg(input);
				//    cout << "Received message " << msg.toString() << endl;
		//    if(!msg.valid)
		//    {
		//      this_thread::sleep_for(chrono::milliseconds(100));
		//      continue;
		//    }

				string msgType = msg.type();
				if(msgType == "finish")
					break;
				else
				{
					if(msgType == "initMonica")
					{
						if(monicaUPtr)
							monicaUPtr.reset();

						Json& initMsg = msg.json;

						customId = initMsg["customId"].int_value();
						SiteParameters site(initMsg["site"]);
						CentralParameterProvider cpp = readUserParameterFromDatabase(initMsg["centralParameterType"].int_value());
						cpp.siteParameters = site;
						monicaUPtr = unique_ptr<MonicaModel>(new MonicaModel(cpp));//make_unique<MonicaModel>(cpp);

						aggregatedValues.clear();
					}
					else if(msgType == "dailyStep")
					{
						if(!monicaUPtr)
						{
							cout << "Error: No initMonica message has been received yet, dropping message " << msg.toString() << endl;
							continue;
						}

						MonicaModel& monica = *monicaUPtr.get();

						monica.resetDailyCounter();

						// test if monica's crop has been dying in previous step
						// if yes, it will be incorporated into soil
						if(monica.cropGrowth() && monica.cropGrowth()->isDying())
							monica.incorporateCurrentCrop();

						auto dsm = msg.json["climateData"].object_items();

						Date date = Date::fromIsoDateString(msg.json["date"].string_value(), false);
						map<Climate::ACD, double> climateData = {{Climate::tmin, dsm["tmin"].number_value()},
																										 {Climate::tavg, dsm["tavg"].number_value()},
																										 {Climate::tmax, dsm["tmax"].number_value()},
																										 {Climate::precip, dsm["precip"].number_value()},
																										 {Climate::wind, dsm["wind"].number_value()},
																										 {Climate::globrad, dsm["globrad"].number_value()},
																										 {Climate::relhumid, dsm["relhumid"].number_value()}};

						debug() << "currentDate: " << date.toString() << endl;

						auto dailyStepResultMsg = Json::object{{"date", date.toIsoDateString()}};

						//apply worksteps
						string err;
						if(msg.json.has_shape({{"worksteps", Json::ARRAY}}, err))
						{
							for(auto ws : msg.json["worksteps"].array_items())
							{
								auto wsType = ws["type"].string_value();
								if(wsType == "Seed")
								{
									Seed seed(ws);
									seed.apply(&monica);
									crop = seed.crop();
									prevDevStage = 0;
								}
								else if(wsType == "Harvest")
								{
									Harvest h(ws);
									auto cropResult = h.cropResult();
									cropResult->date = date;

									h.apply(&monica);
									dailyStepResultMsg["harvesting"] =
										createHarvestingMessage(*cropResult.get(), *monicaUPtr.get());

									//to count the applied fertiliser for the next production process
									monica.resetFertiliserCounter();
									crop.reset();
									prevDevStage = 0;
								}
								else if(wsType == "Cutting")
									Cutting(ws).apply(&monica);
								else if(wsType == "MineralFertiliserApplication")
									MineralFertiliserApplication(ws).apply(&monica);
								else if(wsType == "OrganicFertiliserApplication")
									OrganicFertiliserApplication(ws).apply(&monica);
								else if(wsType == "TillageApplication")
									TillageApplication(ws).apply(&monica);
								else if(wsType == "IrrigationApplication")
									IrrigationApplication(ws).apply(&monica);
							}
						}

						if(monica.isCropPlanted())
							monica.cropStep(date, climateData);

						monica.generalStep(date, climateData);

						aggregateValues(aggregatedValues, climateData, monica);

						dailyStepResultMsg["soil"] = createSoilResultsMessage(monica);

						if(date.day() == 31 && date.month() == 3)
							dailyStepResultMsg["march31st"] = createMarch31stResultsMessage(monica);

						if(date.day() == date.daysInMonth())
							dailyStepResultMsg["monthly"] = createMonthlyResultsMessage(date, aggregatedValues, monica);

						if(date == Date(31, 12, date.year()))
							dailyStepResultMsg["yearly"] = createYearlyResultsMessage(aggregatedValues);

						int devStage = monica.cropGrowth() ? monica.cropGrowth()->get_DevelopmentalStage() + 1 : 0;
						if(prevDevStage < devStage)
						{
							prevDevStage = devStage;
							dailyStepResultMsg["newDevStage"] = devStage;
						}

						try
						{
							s_send(output, Json(dailyStepResultMsg).dump());
						}
						catch(zmq::error_t e)
						{
							cerr
								<< "Exception on trying to send message on zmq push socket with address: "
								<< outputSocketAddress << "! Error: [" << e.what() << "]" << endl;
						}
					}
				}
			}
			catch(zmq::error_t e)
			{
				cerr
					<< "Exception on trying to receive message on zmq pull socket with address: "
					<< inputSocketAddress << "! Will continue to receive push messages! Error: [" << e.what() << "]" << endl;
			}
		}
	}
	catch(zmq::error_t e)
	{
		cerr 
			<< "Couldn't connect socket to address: " << inputSocketAddress << " or "
			<< outputSocketAddress << "! Error: [" << e.what() << "]" << endl;
	}

	debug() << "exiting startZeroMQMonica" << endl;
	*/
}

//-----------------------------------------------------------------------------

void Monica::serveZmqMonicaFull(zmq::context_t* zmqContext, 
																map<ZmqSocketRole, pair<ZmqSocketType, vector<string>>> socketAddresses)
{
	if(socketAddresses.empty())
	{
		cerr << "No supplied address for a receiving zmq socket! Exiting." << endl;
		return;
	}

	ZmqSocketType rType; vector<string> rAddresses;
	auto rci = socketAddresses.find(ReceiveJob);
	if(rci != socketAddresses.end())
		tie(rType, rAddresses) = rci->second;
	int receiveSocketType = ZMQ_REP;
	if(rType == Pull)
		receiveSocketType = ZMQ_PULL;
	zmq::socket_t socket(*zmqContext, receiveSocketType);

	try
	{
		//normal reply socket has to bind to address
		if(rType == Reply)
		{
			debug() << "MONICA: binding monica zeromq reply socket to address: ";
			int i = 0;
			for(auto address : rAddresses)
				debug() << (i > 0 ? "," : "") << address, ++i;
			debug() << endl;
			for(auto address : rAddresses)
				socket.bind(address);
			debug() << "MONICA: bound monica zeromq reply socket to address: ";
			i = 0;
			for(auto address : rAddresses)
				debug() << (i > 0 ? "," : "") << address, ++i;
			debug() << endl;
		}
		//proxy reply socket or pull socket have to connect to either proxy dealer socket or push socket
		else
		{
			debug() << "MONICA: connecting monica zeromq reply socket to address: ";
			int i = 0;
			for(auto address : rAddresses)
				debug() << (i > 0 ? "," : "") << address, ++i;
			debug() << endl;
			for(auto address : rAddresses)
				socket.connect(address);
			debug() << "MONICA: connected monica zeromq reply socket to address: ";
			i = 0;
			for(auto address : rAddresses)
				debug() << (i > 0 ? "," : "") << address, ++i;
			debug() << endl;
		}

		ZmqSocketType sType; vector<string> sAddresses = rAddresses;
		auto sci = socketAddresses.find(SendResult);
		if(sci != socketAddresses.end())
			tie(sType, sAddresses) = sci->second;
		int sendSocketType = ZMQ_PUSH;
		zmq::socket_t sendSocket(*zmqContext, sendSocketType);
		bool distinctSendSocket = sAddresses != rAddresses;

		ZmqSocketType cType; vector<string> cAddresses = rAddresses;
		auto cci = socketAddresses.find(Control);
		if(cci != socketAddresses.end())
			tie(cType, cAddresses) = cci->second;
		int controlSocketType = ZMQ_SUB;
		zmq::socket_t controlSocket(*zmqContext, controlSocketType);
		bool distinctControlSocket = cAddresses != rAddresses;

		zmq::pollitem_t items[] =
		{{(void*)socket, 0, ZMQ_POLLIN, 0}
		,{(void*)controlSocket, 0, ZMQ_POLLIN, 0}
		};

		try
		{
			if(distinctSendSocket)
				for(auto address : sAddresses)
					sendSocket.connect(address);

			try
			{
				int topicCharCount = 0;
				if(distinctControlSocket)
				{
					topicCharCount = 6;
					for(auto address : cAddresses)
						controlSocket.connect(address);
					controlSocket.setsockopt(ZMQ_SUBSCRIBE, "finish", topicCharCount);
				}

				while(true)
				{
					try
					{
						Msg msg;
						zmq::poll(&items[0], distinctControlSocket ? 2 : 1, -1);

						if(items[0].revents & ZMQ_POLLIN)
							msg = receiveMsg(socket);
						if(distinctControlSocket
							 && items[1].revents & ZMQ_POLLIN)
							msg = receiveMsg(controlSocket, topicCharCount);

						//auto msg = receiveMsg(socket);

						string msgType = msg.type();
						if(msgType == "finish")
						{
							J11Object resultMsg;
							resultMsg["type"] = "ack";
							try
							{
								s_send(distinctSendSocket ? sendSocket : socket, Json(resultMsg).dump());
							}
							catch(zmq::error_t e)
							{
								cerr << "Exception on trying to reply to 'finish' request with 'ack' message on zmq socket with address(es): ";
								int i = 0;
								for(auto address : sAddresses)
									cerr << (i > 0 ? "," : "") << address, ++i;
								cerr << "! Still will finish MONICA process! Error: [" << e.what() << "]" << endl;
							}
							sendSocket.setsockopt(ZMQ_LINGER, 0);
							sendSocket.close();

							controlSocket.setsockopt(ZMQ_LINGER, 0);
							controlSocket.close();

							socket.setsockopt(ZMQ_LINGER, 0);
							socket.close();

							break;
						}
						else if(msgType == "Env")
						{
							Json& fullMsg = msg.json;

							Env env(msg.json);
							if(!env.da.isValid() && !env.pathToClimateCSV.empty())
								env.da = readClimateDataFromCSVFileViaHeaders(env.pathToClimateCSV, env.csvViaHeaderOptions);

							activateDebug = activateDebug && env.debugMode;

							env.params.userSoilMoistureParameters.getCapillaryRiseRate =
								[](string soilTexture, int distance)
							{
								return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
							};

							auto res = runMonica(env);

							J11Object resultMsg;
							resultMsg["type"] = "result";
							addOutputToResultMessage(res.out, resultMsg);

							try
							{
								s_send(distinctSendSocket ? sendSocket : socket, Json(resultMsg).dump());
							}
							catch(zmq::error_t e)
							{
								cerr << "Exception on trying to reply with result message on zmq socket with address: ";
								int i = 0;
								for(auto address : sAddresses)
									cerr << (i > 0 ? "," : "") << address, ++i;
								cerr << "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
							}
						}
						else
						{
							J11Object resultMsg;
							resultMsg["type"] = "error";
							try
							{
								s_send(distinctSendSocket ? sendSocket : socket, Json(resultMsg).dump());
							}
							catch(zmq::error_t e)
							{
								cerr 
									<< "Exception on trying to reply to '" << msgType 
									<< "' request with 'error' message on zmq socket with address: ";
								int i = 0;
								for(auto address : sAddresses)
									cerr << (i > 0 ? "," : "") << address, ++i;
								cerr << "! Still will finish MONICA process! Error: [" << e.what() << "]" << endl;
							}
						}
					}
					catch(zmq::error_t e)
					{
						cerr << "Exception on trying to receive request message on zmq socket with address: ";
						int i = 0;
						for(auto address : rAddresses)
							cerr << (i > 0 ? "," : "") << address, ++i;
						cerr << "! Will continue to receive requests! Error: [" << e.what() << "]" << endl;
					}
				}
			}
			catch(zmq::error_t e)
			{
				cerr << "Couldn't connect zmq subscribe socket to address: ";
				int i = 0;
				for(auto address : cAddresses)
					cerr << (i > 0 ? "," : "") << address, ++i; 
				cerr << "! Error: " << e.what() << endl;
			}
		}
		catch(zmq::error_t e)
		{
			cerr << "Couldn't bind zmq push socket to address: ";
			int i = 0;
			for(auto address : sAddresses)
				cerr << (i > 0 ? "," : "") << address, ++i; 
			cerr << "! Error: " << e.what() << endl;
		}
	}
	catch(zmq::error_t e)
	{
		cerr
			<< "Couldn't " << (rType == Reply ? "bind" : "connect")
			<< " zmq socket to address: ";
		int i = 0;
		for(auto address : rAddresses)
			cerr << (i > 0 ? "," : "") << address, ++i;
		cerr << "! Error: " << e.what() << endl;
	}

	debug() << "exiting serveZmqMonicaFull" << endl;
}

//-----------------------------------------------------------------------------


