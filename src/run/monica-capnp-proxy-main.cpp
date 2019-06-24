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

#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include <list>
#include <algorithm>

#include <kj/debug.h>
#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "tools/debug.h"
#include "db/abstract-db-connections.h"

#include "run-monica-capnp.h"

#include "model.capnp.h"
#include "common.capnp.h"

using namespace std;
using namespace Monica;
using namespace Tools;
using namespace json11;
using namespace Climate;
using namespace zalf::capnp;

string appName = "monica-capnp-proxy";
string version = "1.0.0-beta";

class RunMonicaProxy;
class Unregister final : public rpc::Common::Unregister::Server
{
public:
	Unregister(RunMonicaProxy& proxy, int monicaServerId) : _proxy(proxy), _monicaServerId(monicaServerId) {}

	~Unregister();

	kj::Promise<void> unregister(UnregisterContext context) override; //unregister @1 ();

	private:
		void unreg();

		RunMonicaProxy& _proxy;
		int _monicaServerId;
};

class RunMonicaProxy final : public rpc::Model::EnvInstanceProxy::Server
{
	friend class Unregister;

	typedef rpc::Model::EnvInstance::Client MonicaClient;
	struct X {
		int id{ 0 };
		MonicaClient client{ nullptr };
		int jobs{ 0 };
	};

	std::list<X> _xs;
	int _id = 0;

public:
	RunMonicaProxy() {}

	RunMonicaProxy(vector<rpc::Model::EnvInstance::Client>& monicas)  {
		for (auto&& client : monicas) {
			_xs.push_back({ _id++, kj::mv(client), 0 });
		}
	}

	kj::Promise<void> run(RunContext context) override //run @0 (env :Env) -> (result :Common.StructuredText);
	{
		if(_xs.empty())
			return kj::READY_NOW;

		X& min = _xs.front();
		if (min.jobs > 0) {
			for (auto& x : _xs) {
				if (x.jobs < min.jobs) {
					min = x;
				}
			}
		}
		
		auto req = min.client.runRequest();
		req.setEnv(context.getParams().getEnv());
		min.jobs++;
		cout << "added job to worker: " << min.id << " now " << min.jobs << " in worker queue" << endl;
		int id = min.id;
		return req.send().then([context, id, this](auto&& res) mutable {
			for(X& x : this->_xs) {
				if(x.id == id) {
					x.jobs--;
					cout << "finished job of worker: " << id << " now " << x.jobs << " in worker queue" << endl;
					context.setResults(res);
				}
			}
			}, [context, id, this](kj::Exception&& exception) {
				cout << "job for worker with id: " << id << " failed" << endl;
				cout << "Exception: " << exception.getDescription().cStr() << endl;
				//try to erase id from map, so it can't be used anymore
				_xs.remove_if([=](auto& x){ return x.id == id; });
			});
	}

	kj::Promise<void> registerService(RegisterServiceContext context) override  //registerService @0 [Service] (service :Service) -> (unregister :Unregister);
	{
		auto service = context.getParams().getService().getAs<rpc::Model::EnvInstance>();
		_xs.push_back({ _id++, kj::mv(service), 0 });
		cout << "added service to proxy: " << _id << " services registered now" << endl;

		context.getResults().setUnregister(kj::heap<Unregister>(*this, _id - 1));

		return kj::READY_NOW;
	}

	kj::Promise<void> registerService2(RegisterService2Context context) override  //registerService2 @0 (service :EnvInstance);
	{
		auto service = context.getParams().getService();
		_xs.push_back({ _id++, kj::mv(service), 0 });
		cout << "added service to proxy: " << _id << " services registered now" << endl;
		return kj::READY_NOW;
	}

};

Unregister::~Unregister() {
	unreg();
}

void Unregister::unreg() {
	cout << "unregistering id: " << _monicaServerId << endl;
	_proxy._xs.remove_if([=](auto& x){ return x.id == _monicaServerId; });
}

kj::Promise<void> Unregister::unregister(UnregisterContext context) //unregister @1 ();
{
	unreg();
	return kj::READY_NOW;
}

/*
int main_working_no_disconnect_support(int argc, const char* argv[]){

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string address = "*";
	int port = -1;

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
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl;
	};

	if (argc >= 1)
	{
		for (auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if (arg == "-p" || arg == "--port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					port = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA proxy" << endl;

		// Set up a server.
		capnp::EzRpcServer server(kj::heap<RunMonicaProxy>(), address + (port < 0 ? "" : string(":") + to_string(port)));

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

		debug() << "stopped Cap'n Proto MONICA proxy" << endl;
	}

	return 0;
}
*/

capnp::Capability::Client mainInterface(nullptr);

kj::AsyncIoProvider::PipeThread runServer(kj::AsyncIoProvider& ioProvider, bool startMonicaThreadsInDebugMode) {
	return ioProvider.newPipeThread([startMonicaThreadsInDebugMode](
		kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
			capnp::TwoPartyVatNetwork network(stream, capnp::rpc::twoparty::Side::SERVER);
			auto server = makeRpcServer(network, kj::heap<RunMonicaImpl>(startMonicaThreadsInDebugMode));
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

struct CMETRes {
	kj::ForkedPromise<void> fp;
	zalf::capnp::rpc::Model::EnvInstance::Client client;
};

CMETRes
createMonicaEnvThread(kj::AsyncIoProvider& ioProvider, bool startMonicaThreadsInDebugMode) {
	auto serverThread = runServer(ioProvider, startMonicaThreadsInDebugMode);
	auto tc = kj::heap<ThreadContext>(kj::mv(serverThread));

	capnp::MallocMessageBuilder vatIdMessage(8);
	auto vatId = vatIdMessage.initRoot<capnp::rpc::twoparty::VatId>();
	vatId.setSide(capnp::rpc::twoparty::Side::CLIENT);
	auto client = tc->rpcSystem.bootstrap(vatId).castAs<rpc::Model::EnvInstance>();

	auto prom = tc->network.onDisconnect().attach(kj::mv(tc));
	return CMETRes{prom.fork(), kj::mv(client)};
}

int main(int argc, const char* argv[]) {

	setlocale(LC_ALL, "");
	setlocale(LC_NUMERIC, "C");

	string address = "*";
	int port = -1;
	uint no_of_threads = 0;
	bool startMonicaThreadsInDebugMode = false;

	//init path to db-connections.ini
	if (auto monicaHome = getenv("MONICA_HOME"))
	{
		auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
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
			<< " -d | --debug "
			"... show debug outputs" << endl
			<< " -p | --port ... PORT (default: none)] "
			"... runs the server bound to the port, PORT may be ommited to choose port automatically." << endl
			<< " -t | --monica-threads ... NUMBER (default: " << no_of_threads << ")] "
			"... starts additionally to the proxy NUMBER of MONICA threads which can be served via the proxy." << endl;
	};

	if (argc >= 1)
	{
		for (auto i = 1; i < argc; i++)
		{
			string arg = argv[i];
			if (arg == "-d" || arg == "--debug") {
				startMonicaThreadsInDebugMode = true;
			}
			else if (arg == "-p" || arg == "--port")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					port = stoi(argv[++i]);
			}
			else if (arg == "-t" || arg == "--monica-threads")
			{
				if (i + 1 < argc && argv[i + 1][0] != '-')
					no_of_threads = stoi(argv[++i]);
			}
			else if (arg == "-h" || arg == "--help")
				printHelp(), exit(0);
			else if (arg == "-v" || arg == "--version")
				cout << appName << " version " << version << endl, exit(0);
		}

		debug() << "starting Cap'n Proto MONICA proxy" << endl;

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
		for (uint i = 0; i < no_of_threads; i++) {
			auto promAndClient = createMonicaEnvThread(*ioContext.provider, startMonicaThreadsInDebugMode);
			proms.push_back(promAndClient.fp.addBranch());
			clients.push_back(kj::mv(promAndClient.client));
		}
		//init the proxy, which will be 
		mainInterface = kj::heap<RunMonicaProxy>(clients);

		port = portPromise.addBranch().wait(ioContext.waitScope);
		if (port == 0) {
			// The address format "unix:/path/to/socket" opens a unix domain socket,
			// in which case the port will be zero.
			std::cout << "Listening on Unix socket..." << std::endl;
		}
		else {
			std::cout << "Listening on port " << port << "..." << std::endl;
		}
		
		// Run forever, accepting connections and handling requests.
		kj::NEVER_DONE.wait(ioContext.waitScope);

		debug() << "stopped Cap'n Proto MONICA proxy" << endl;
	}

	return 0;
}

