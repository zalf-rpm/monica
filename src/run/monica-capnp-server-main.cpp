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

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>

#include <kj/debug.h>

#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>

#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>



#include "json11/json11.hpp"

#include "tools/helper.h"

#include "db/abstract-db-connections.h"
#include "tools/debug.h"
#include "run-monica.h"
#include "../io/database-io.h"
#include "../core/monica-model.h"
#include "tools/json11-helper.h"
#include "climate/climate-file-io.h"
#include "soil/conversion.h"
#include "env-from-json-config.h"
#include "tools/algorithms.h"
#include "../io/csv-format.h"
#include "climate/climate-common.h"

#include "model.capnp.h"
#include "common.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace zalf::capnp;

string appName = "monica-capnp-server";
string version = "1.0.0-beta";

std::map<std::string, DataAccessor> daCache;
bool startedServerInDebugMode = false;

DataAccessor fromCapnpData(
	const Date& startDate, 
	const Date& endDate, 
	capnp::List<rpc::Climate::Element>::Reader header, 
	capnp::List<capnp::List<float>>::Reader data)
{
	typedef rpc::Climate::Element E;

	if (data.size() == 0)
		return DataAccessor();

	DataAccessor da(startDate, endDate);
	//vector<double> d(data[0].size());
	for(int i = 0; i < header.size(); i++)
	{
		auto vs = data[i];
		vector<double> d(data[0].size());
		//transform(vs.begin(), vs.end(), d.begin(), [](float f) { return f; });
		for (int k = 0; k < vs.size(); k++)
			d[k] = vs[k];
		switch (header[i]) {
		case E::TMIN: da.addClimateData(ACD::tmin, std::move(d)); break;
		case E::TAVG: da.addClimateData(ACD::tavg, std::move(d)); break;
		case E::TMAX: da.addClimateData(ACD::tmax, std::move(d)); break;
		case E::PRECIP: da.addClimateData(ACD::precip, std::move(d)); break;
		case E::RELHUMID: da.addClimateData(ACD::relhumid, std::move(d)); break;
		case E::WIND: da.addClimateData(ACD::wind, std::move(d)); break;
		case E::GLOBRAD: da.addClimateData(ACD::globrad, std::move(d)); break;
		default:;
		}
	}

	return da;
}

class RunMonicaImpl final : public rpc::Model::EnvInstance::Server
{
	// Implementation of the Model::Instance Cap'n Proto interface

public:
	kj::Promise<void> run(RunContext context) override
	{
		debug() << ".";

		auto envR = context.getParams().getEnv(); 

		auto runMonica = [context, envR](DataAccessor da = DataAccessor()) mutable {
			string err;
			auto rest = envR.getRest();
			if (!rest.getStructure().isJson())
				return Monica::Output();

			const Json& envJson = Json::parse(rest.getValue().cStr(), err);
			//cout << "runMonica: " << envJson["customId"].dump() << endl;

			Env env(envJson);
			if (da.isValid())
				env.climateData = da;
			else if (!env.climateData.isValid() && !env.pathsToClimateCSV.empty())
				env.climateData = readClimateDataFromCSVFilesViaHeaders(env.pathsToClimateCSV, env.csvViaHeaderOptions);

			env.debugMode = startedServerInDebugMode && env.debugMode;

			env.params.userSoilMoistureParameters.getCapillaryRiseRate =
				[](string soilTexture, int distance) {
				return Soil::readCapillaryRiseRates().getRate(soilTexture, distance);
			};

			return Monica::runMonica(env);
		};

		if (envR.hasTimeSeries()) {
			auto ts = envR.getTimeSeries();
			auto rangeProm = ts.rangeRequest().send();
			auto headerProm = ts.headerRequest().send();
			auto dataTProm = ts.dataTRequest().send();
			
			return rangeProm
				.then([KJ_MVCAP(headerProm), KJ_MVCAP(dataTProm)](auto&& rangeResponse) mutable {
				return headerProm
					.then([KJ_MVCAP(rangeResponse), KJ_MVCAP(dataTProm)](auto&& headerResponse) mutable {
					return dataTProm
						.then([KJ_MVCAP(rangeResponse), KJ_MVCAP(headerResponse)](auto&& dataTResponse) mutable {
						auto sd = rangeResponse.getStartDate();
						auto ed = rangeResponse.getEndDate();
						return fromCapnpData(
							Date(sd.getDay(), sd.getMonth(), sd.getYear()),
							Date(ed.getDay(), ed.getMonth(), ed.getYear()),
							headerResponse.getHeader(), dataTResponse.getData());
					});
				});
			}).then([context, runMonica](DataAccessor da) mutable {
				auto out = runMonica(da);
				auto rs = context.getResults();
				rs.initResult();
				rs.getResult().setValue(out.toString());
			});
		}
		else {
			runMonica();
			return kj::READY_NOW;
		}
	}
};


class RunMonicaProxy final : public rpc::Model::EnvInstance::Server
{
	typedef rpc::Model::EnvInstance::Client MonicaClient;
	struct X {
		MonicaClient client{ nullptr };
		int jobs{ 0 };
	};

	map<int, X> id2x;

public:
	RunMonicaProxy(vector<rpc::Model::EnvInstance::Client> monicas)  {
		int i = 0;
		for (auto&& client : monicas) {
			id2x[i++] = { kj::mv(client), 0 };
		}
	}

	kj::Promise<void> run(RunContext context) override
	{
		int id = id2x.begin()->first;
		X min = id2x.begin()->second;
		if (min.jobs > 0)
		{
			for (auto& p : id2x) {
				if (p.second.jobs < min.jobs) {
					id = p.first; 
					min.client = p.second.client;
					min.jobs = p.second.jobs;
				}
			}
		}
		
		auto req = min.client.runRequest();
		req.setEnv(context.getParams().getEnv());
		id2x[id].jobs++;
		cout << "added job to worker: " << id << " now " << id2x[id].jobs << " in worker queue" << endl;
		return req.send().then([context, id, this](auto&& res) mutable {
			this->id2x[id].jobs--;
			cout << "finished job of worker: " << id << " now " << id2x[id].jobs << " in worker queue" << endl;
			context.setResults(res);
			});
	}
};


int main_(int argc, const char* argv[]){

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string address = "*";
	int port = 6666;

	//init path to db-connections.ini
	if (auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
		//init for dll/so
		initPathToDB(pathToFile);
		//init for monica-run
		Db::dbConnectionParameters(pathToFile);
	}

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	auto printHelp = [=]()
	{
		cout
			<< appName << "[options]" << endl
			<< endl
			<< "options:" << endl
			<< endl
			<< " -h | --help ... this help output" << endl
			<< " -v | --version ... outputs " << appName << " version and ZeroMQ version being used" << endl
			<< endl
			<< " -d | --debug ... show debug outputs" << endl
			<< " -a | --address ... ADDRESS (default: " << address << ")] "
			"... runs server bound to given address, may be '*' to bind to all local addresses" << endl
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl;
	};

	if (argc >= 1)
	{
		for (auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if (arg == "-d" || arg == "--debug") {
				activateDebug = true;
				startedServerInDebugMode = true;
			}
			else if (arg == "-a" || arg == "--address")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					address = argv[++i];
			}
			else if (arg == "-p" || arg == "--port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					port = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA server" << endl;

		// Set up a server.
		capnp::EzRpcServer server(kj::heap<RunMonicaImpl>(), address + (port < 0 ? "" : string(":") + to_string(port)));

		// Write the port number to stdout, in case it was chosen automatically.
		auto& waitScope = server.getWaitScope();
		port = server.getPort().wait(waitScope);
		if (port == 0) {
			// The address format "unix:/path/to/socket" opens a unix domain socket,
			// in which case the port will be zero.
			std::cout << "Listening on Unix socket..." << std::endl;
		}
		else {
			std::cout << "Listening on port " << port << "..." << std::endl;
		}

		// Run forever, accepting connections and handling requests.
		kj::NEVER_DONE.wait(waitScope);

		debug() << "stopped Cap'n Proto MONICA server" << endl;
	}

	return 0;
}

capnp::Capability::Client mainInterface(nullptr);

kj::AsyncIoProvider::PipeThread runServer(kj::AsyncIoProvider& ioProvider) {
	return ioProvider.newPipeThread([](
		kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
			capnp::TwoPartyVatNetwork network(stream, capnp::rpc::twoparty::Side::SERVER);
			auto server = makeRpcServer(network, kj::heap<RunMonicaImpl>());
			network.onDisconnect().wait(waitScope);
		});
}

kj::Promise<kj::Own<kj::AsyncIoStream>> connectAttach(kj::Own<kj::NetworkAddress>&& addr) {
	return addr->connect().attach(kj::mv(addr));
}

struct ErrorHandler : public kj::TaskSet::ErrorHandler {
	void taskFailed(kj::Exception&& exception) override {
		kj::throwFatalException(kj::mv(exception));
	}
};
ErrorHandler eh;
kj::TaskSet tasks(eh);

struct ServerContext {
	kj::Own<kj::AsyncIoStream> stream;
	capnp::TwoPartyVatNetwork network;
	capnp::RpcSystem<capnp::rpc::twoparty::VatId> rpcSystem;

	ServerContext(
		kj::Own<kj::AsyncIoStream>&& stream, 
		capnp::ReaderOptions readerOpts)
		: stream(kj::mv(stream))
		, network(*this->stream, capnp::rpc::twoparty::Side::SERVER, readerOpts)
		, rpcSystem(makeRpcServer(network, mainInterface)) {}
};

void acceptLoop(kj::Own<kj::ConnectionReceiver>&& listener, capnp::ReaderOptions readerOpts) {
	auto ptr = listener.get();
	tasks.add(ptr->accept().then(kj::mvCapture(kj::mv(listener),
		[readerOpts](kj::Own<kj::ConnectionReceiver>&& listener,
			kj::Own<kj::AsyncIoStream>&& connection) {
				acceptLoop(kj::mv(listener), readerOpts);

				cout << "connection from client" << endl;
				auto server = kj::heap<ServerContext>(kj::mv(connection), readerOpts);

				// Arrange to destroy the server context when all references are gone, or when the
				// EzRpcServer is destroyed (which will destroy the TaskSet).
				tasks.add(server->network.onDisconnect().attach(kj::mv(server)));
		})));
}

struct ThreadContext {
	kj::AsyncIoProvider::PipeThread serverThread;
	capnp::TwoPartyVatNetwork network;
	capnp::RpcSystem<capnp::rpc::twoparty::VatId> rpcSystem;

	ThreadContext(kj::AsyncIoProvider::PipeThread&& serverThread)
		: serverThread(kj::mv(serverThread))
		, network(*this->serverThread.pipe, capnp::rpc::twoparty::Side::SERVER)
		, rpcSystem(makeRpcClient(network)) {
	}
};

pair<kj::ForkedPromise<void>, zalf::capnp::rpc::Model::EnvInstance::Client> 
createMonicaEnvThread(kj::AsyncIoProvider& ioProvider) {
	auto serverThread = runServer(ioProvider);
	auto tc = kj::heap<ThreadContext>(kj::mv(serverThread));

	capnp::MallocMessageBuilder vatIdMessage(8);
	auto vatId = vatIdMessage.initRoot<capnp::rpc::twoparty::VatId>();
	vatId.setSide(capnp::rpc::twoparty::Side::CLIENT);
	auto client = tc->rpcSystem.bootstrap(vatId).castAs<rpc::Model::EnvInstance>();

	auto prom = tc->network.onDisconnect().attach(kj::mv(tc));
	return make_pair(prom.fork(), kj::mv(client));
}

int main(int argc, const char* argv[]) {

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string address = "*";
	int port = 6666;

	//init path to db-connections.ini
	if (auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
		//init for dll/so
		initPathToDB(pathToFile);
		//init for monica-run
		Db::dbConnectionParameters(pathToFile);
	}

	//use a possibly non-default db-connections.ini
	//Db::dbConnectionParameters("db-connections.ini");

	auto printHelp = [=]()
	{
		cout
			<< appName << "[options]" << endl
			<< endl
			<< "options:" << endl
			<< endl
			<< " -h | --help ... this help output" << endl
			<< " -v | --version ... outputs " << appName << " version and ZeroMQ version being used" << endl
			<< endl
			<< " -d | --debug ... show debug outputs" << endl
			<< " -a | --address ... ADDRESS (default: " << address << ")] "
			"... runs server bound to given address, may be '*' to bind to all local addresses" << endl
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl;
	};

	if (argc >= 1)
	{
		for (auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if (arg == "-d" || arg == "--debug") {
				activateDebug = true;
				startedServerInDebugMode = true;
			}
			else if (arg == "-a" || arg == "--address")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					address = argv[++i];
			}
			else if (arg == "-p" || arg == "--port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					port = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA server" << endl;
				
		auto ioContext = kj::setupAsyncIo();
		int callCount = 0;
		int handleCount = 0;
		
		auto paf = kj::newPromiseAndFulfiller<uint>();
		kj::ForkedPromise<uint> portPromise = paf.promise.fork();
		
		auto& network = ioContext.provider->getNetwork();
		auto bindAddress = address + (port < 0 ? "" : string(":") + to_string(port));
		uint defaultPort = 0;

		tasks.add(network.parseAddress(bindAddress, defaultPort)
			.then(kj::mvCapture(paf.fulfiller,
				[](kj::Own<kj::PromiseFulfiller<uint>>&& portFulfiller,
					kj::Own<kj::NetworkAddress>&& addr) {
						auto listener = addr->listen();
						portFulfiller->fulfill(listener->getPort());
						acceptLoop(kj::mv(listener), capnp::ReaderOptions());
				})));
		
		vector<rpc::Model::EnvInstance::Client> clients;
		vector<kj::Promise<void>> proms;
		for (int i = 0; i < 4; i++) {
			auto promAndClient = createMonicaEnvThread(*ioContext.provider);
			proms.push_back(promAndClient.first.addBranch());
			clients.push_back(kj::mv(promAndClient.second));
		}
		//init the proxy, which will be 
		mainInterface = kj::heap<RunMonicaProxy>(clients);
			 	 
		// Run forever, accepting connections and handling requests.
		kj::NEVER_DONE.wait(ioContext.waitScope);

		debug() << "stopped Cap'n Proto MONICA server" << endl;
	}

	return 0;
}
