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
#include <algorithm>

#include <kj/debug.h>
#include <kj/common.h>
#define KJ_MVCAP(var) var = kj::mv(var)

#include <capnp/ez-rpc.h>
#include <capnp/message.h>
#include <capnp/rpc-twoparty.h>
#include <kj/thread.h>

#include "tools/debug.h"
//#include "db/abstract-db-connections.h"
#include "common/rpc-connection-manager.h"

#include "run-monica-capnp.h"

#include <capnp/persistent.capnp.h>
#include "model.capnp.h"
#include "common.capnp.h"
#include "registry.capnp.h"

#include "common/sole.hpp"

using namespace std;
using namespace monica;
using namespace Tools;
using namespace mas;
using namespace mas::infrastructure::common;

string appName = "monica-capnp-proxy";
string version = "1.0.0-beta";

typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText, mas::schema::common::StructuredText> MonicaEnvInstance;

class RunMonicaProxy;
class Unregister final : public mas::schema::common::Action::Server
{
public:
  Unregister(RunMonicaProxy& proxy, size_t monicaServerId) : _proxy(proxy), _monicaServerId(monicaServerId) {}

  ~Unregister();

  kj::Promise<void> do_(DoContext context) override; //do @0 ();

private:
  void unreg();

  RunMonicaProxy& _proxy;
  size_t _monicaServerId;
};

class RunMonicaProxy final : public mas::schema::model::EnvInstanceProxy< mas::schema::common::StructuredText, mas::schema::common::StructuredText>::Server
{
  friend class Unregister;
    
  struct X {
    MonicaEnvInstance::Client client{ nullptr };
    size_t id{ 0 };
    int jobs{ 0 };

    void unset() { jobs = -1; client = nullptr; }
    void reset(MonicaEnvInstance::Client&& client) { jobs = 0; this->client = client; }
  };

  std::vector<X> _xs;
  std::string _uuid;

public:
  RunMonicaProxy() : _uuid(sole::uuid4().str()) {}

  RunMonicaProxy(vector<MonicaEnvInstance::Client>& monicas) : _uuid(sole::uuid4().str()) {
    size_t id = 0;
    for (auto&& client : monicas) {
      _xs.push_back({ kj::mv(client), id++, 0 });
    }
  }

  kj::Promise<void> info(InfoContext context) //override
  {
    auto rs = context.getResults();

    rs.setId("monica-proxy_" + _uuid);
    rs.setName("Monica capnp proxy");
    rs.setDescription("");
    return kj::READY_NOW;
  }

  kj::Promise<void> run(RunContext context) override //run @0 (env :Env) -> (result :Common.StructuredText);
  {
    if (_xs.empty())
      return kj::READY_NOW;

    X* min = &_xs.front();
    if (min->jobs > 0 || min->jobs < 0) {
      for (size_t i = 0, size = _xs.size(); i < size; i++) {
        auto& x = _xs[i];
        if (x.jobs < 0) //skip empty storage places
          continue;
        if (x.jobs < min->jobs) {
          min = &x;
          if (min->jobs == 0) //stop searching if a worker has nothing to do
            break;
        }
      }
    }
    if (min->jobs < 0) //just empty storage places, no clients connected
      return kj::READY_NOW;

    auto req = min->client.runRequest();
    req.setEnv(context.getParams().getEnv());
    min->jobs++;
    auto id = min->id;
    cout << "added job to worker: " << id << " now " << min->jobs << " in worker queue" << endl;
    return req.send().then([context, id, this](auto&& res) mutable {
      if (id < this->_xs.size()) {
        X& x = _xs[id];
        x.jobs--;
        cout << "finished job of worker: " << id << " now " << x.jobs << " in worker queue" << endl;
        context.setResults(res);
      }
      }, [context, id, this](kj::Exception&& exception) {
        cout << "job for worker with id: " << id << " failed" << endl;
        cout << "Exception: " << exception.getDescription().cStr() << endl;
        //try to erase id from map, so it can't be used anymore
        if (id < this->_xs.size()) {
          _xs[id].unset();
        }
      });
  }

  kj::Promise<void> registerEnvInstance(RegisterEnvInstanceContext context) override  // registerEnvInstance @0 (instance :EnvInstance) -> (unregister :Common.Callback);
  {
    auto instance = context.getParams().getInstance();
    bool filledEmptySlot = false;
    size_t registeredAsId = 0;
    for (X& x : _xs) {
      if (x.jobs < 0) {
        //x = { kj::mv(instance), x.id, 0 };
        x.reset(kj::mv(instance));
        registeredAsId = x.id;
        filledEmptySlot = true;
        break;
      }
    }
    if (!filledEmptySlot) {
      _xs.push_back({ kj::mv(instance), _xs.size(), 0 });
      registeredAsId = _xs.back().id;
    }

    int count = 0;
    for (X& x : _xs) {
      if (x.jobs >= 0)
        count++;
    }
        
    cout << "added service to proxy: service-id: " << registeredAsId << " -> " << count << " services registered now" << endl;

    context.getResults().setUnregister(kj::heap<Unregister>(*this, registeredAsId));

    return kj::READY_NOW;
  }
};

Unregister::~Unregister() {
  unreg();
}

void Unregister::unreg() {
  cout << "unregistering id: " << _monicaServerId << endl;
  if (_monicaServerId < _proxy._xs.size()) {
    _proxy._xs[_monicaServerId].unset();
  }
}

kj::Promise<void> Unregister::do_(DoContext context) // do @0 ();
{
  unreg();
  return kj::READY_NOW;
}

//capnp::Capability::Client mainInterface(nullptr);

kj::AsyncIoProvider::PipeThread runServer(kj::AsyncIoProvider& ioProvider, bool startMonicaThreadsInDebugMode) {
  return ioProvider.newPipeThread([startMonicaThreadsInDebugMode](
    kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream, kj::WaitScope& waitScope) {
      capnp::TwoPartyVatNetwork network(stream, capnp::rpc::twoparty::Side::SERVER);
      auto server = makeRpcServer(network, kj::heap<RunMonica>(startMonicaThreadsInDebugMode, new mas::infrastructure::common::Restorer()));
      network.onDisconnect().wait(waitScope);
    });
}

//kj::Promise<kj::Own<kj::AsyncIoStream>> connectAttach(kj::Own<kj::NetworkAddress>&& addr) {
//	return addr->connect().attach(kj::mv(addr));
//}

//struct ErrorHandler : public kj::TaskSet::ErrorHandler {
//	void taskFailed(kj::Exception&& exception) override {
//		kj::throwFatalException(kj::mv(exception));
//	}
//};
//ErrorHandler eh;
//kj::TaskSet tasks(eh);

//struct ServerContext {
//	kj::Own<kj::AsyncIoStream> stream;
//	capnp::TwoPartyVatNetwork network;
//	capnp::RpcSystem<capnp::rpc::twoparty::VatId> rpcSystem;

//	ServerContext(
//		kj::Own<kj::AsyncIoStream>&& stream,
//		capnp::ReaderOptions readerOpts)
//		: stream(kj::mv(stream))
//		, network(*this->stream, capnp::rpc::twoparty::Side::SERVER, readerOpts)
//		, rpcSystem(makeRpcServer(network, mainInterface)) {}
//};

//void acceptLoop(kj::Own<kj::ConnectionReceiver>&& listener, capnp::ReaderOptions readerOpts) {
//	auto ptr = listener.get();
//	tasks.add(ptr->accept().then(kj::mvCapture(kj::mv(listener),
//		[readerOpts](kj::Own<kj::ConnectionReceiver>&& listener,
//			kj::Own<kj::AsyncIoStream>&& connection) {
//				acceptLoop(kj::mv(listener), readerOpts);

//				cout << "connection from client" << endl;
//				auto server = kj::heap<ServerContext>(kj::mv(connection), readerOpts);

        // Arrange to destroy the server context when all references are gone, or when the
        // EzRpcServer is destroyed (which will destroy the TaskSet).
//				tasks.add(server->network.onDisconnect().attach(kj::mv(server)));
//		})));
//}

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
  MonicaEnvInstance::Client client;
};

CMETRes
createMonicaEnvThread(kj::AsyncIoProvider& ioProvider, bool startMonicaThreadsInDebugMode) {
  auto serverThread = runServer(ioProvider, startMonicaThreadsInDebugMode);
  auto tc = kj::heap<ThreadContext>(kj::mv(serverThread));

  capnp::MallocMessageBuilder vatIdMessage(8);
  auto vatId = vatIdMessage.initRoot<capnp::rpc::twoparty::VatId>();
  vatId.setSide(capnp::rpc::twoparty::Side::CLIENT);
  auto client = tc->rpcSystem.bootstrap(vatId).castAs<MonicaEnvInstance>();

  auto prom = tc->network.onDisconnect().attach(kj::mv(tc));
  return CMETRes{ prom.fork(), kj::mv(client) };
}

int main(int argc, const char* argv[]) {

  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  string address = "*";
  int port = -1;
  unsigned int no_of_threads = 0;
  bool startMonicaThreadsInDebugMode = false;

  //init path to db-connections.ini
  //if (auto monicaHome = getenv("MONICA_HOME"))
  //{
  //	auto pathToFile = string(monicaHome) + Tools::pathSeparator() + "db-connections.ini";
  //	//init for monica-run
  //	Db::dbConnectionParameters(pathToFile);
  //}
    
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

    //auto paf = kj::newPromiseAndFulfiller<uint>();
    //kj::ForkedPromise<uint> portPromise = paf.promise.fork();

    //auto& network = ioContext.provider->getNetwork();
    //auto bindAddress = address + (port < 0 ? "" : string(":") + to_string(port));
    //uint defaultPort = 0;

    //auto&& portFulfiller = paf.fulfiller;
    //tasks.add(network.parseAddress(bindAddress, defaultPort)
    //  .then([KJ_MVCAP(portFulfiller)](kj::Own<kj::NetworkAddress>&& addr) mutable {
    //  auto listener = addr->listen();
    //  portFulfiller->fulfill(listener->getPort());
    //  acceptLoop(kj::mv(listener), capnp::ReaderOptions());
    //}));
    
    vector<MonicaEnvInstance::Client> clients;
    vector<kj::Promise<void>> proms;
    for (unsigned int i = 0; i < no_of_threads; i++) {
      auto promAndClient = createMonicaEnvThread(*ioContext.provider, startMonicaThreadsInDebugMode);
      proms.push_back(promAndClient.fp.addBranch());
      clients.push_back(kj::mv(promAndClient.client));
    }

    capnp::Capability::Client mainInterface = kj::heap<RunMonicaProxy>(clients);

    ConnectionManager conMan;
    auto portPromise = conMan.bind(ioContext, mainInterface, address, port < 0 ? 0U : kj::uint(port));
    auto port = portPromise.wait(ioContext.waitScope);
    
    //port = portPromise.addBranch().wait(ioContext.waitScope);
    
    /*
    auto cap = conman.connect(ioContext, "capnp://insecure@localhost:10001/abcd").wait(ioContext.waitScope);
    auto registratorCap = cap.castAs<rpc::registry::Registrator>();
    auto regReq = registratorCap.registerRequest();
    regReq.setCategoryId("abc");
    regReq.setRef(mainInterface.castAs<mas::schema::common::Identifiable>());
//{
      auto regResp = regReq.send().wait(ioContext.waitScope);
      auto unreg = regResp.getUnreg();
//		}
    //unreg.callRequest().send().wait(ioContext.waitScope);

    auto bsCap = conman.connect(ioContext, "capnp://insecure@localhost:10001").wait(ioContext.waitScope);
    auto registryCap = bsCap.castAs<rpc::registry::Registry>();
    auto entriesReq = registryCap.entriesRequest();
    entriesReq.setCategoryId("abc");
    auto entriesRes = entriesReq.send().wait(ioContext.waitScope);
    auto entries = entriesRes.getEntries();
    auto fstEntry = entries[0];
    //auto self = fstEntry.getRef().castAs<mas::schema::model::EnvInstanceProxy<capnp::Text, capnp::Text>>();
    auto catId = fstEntry.getCategoryId();
    cout << "catId: " << catId.cStr() << endl;
    auto ref = fstEntry.getRef();
    auto persistSelfCap = fstEntry.getRef().castAs<capnp::Persistent<capnp::Text, capnp::Text>>();

    auto saveReq = persistSelfCap.saveRequest();
    auto sturdyRefRes = saveReq.send().wait(ioContext.waitScope);
    auto sturdyRef = sturdyRefRes.getSturdyRef().cStr();
    cout << "sr: " << sturdyRef << endl;

    auto saveReq2 = persistSelfCap.saveRequest();
    auto sturdyRefRes2 = saveReq2.send().wait(ioContext.waitScope);
    auto sturdyRef2 = sturdyRefRes2.getSturdyRef().cStr();
    cout << "sr2: " << sturdyRef2 << endl;
    */

    //port = portPromise.addBranch().wait(ioContext.waitScope);
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

