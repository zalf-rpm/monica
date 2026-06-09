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

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/main.h>

#include <capnp/any.h>

#include "common/common.h"
#include "common/rpc-connection-manager.h"
#include "json11/json11-helper.h"
#include "resource/version.h"
#include "tools/debug.h"

#include "PortConnector.h"
#include "run-monica-capnp.h"

#include "common.capnp.h"
#include "fbp.capnp.h"
#include "model.capnp.h"

#define KJ_MVCAP(var) var = kj::mv(var)

using namespace std;
using namespace monica;
using namespace Tools;

const std::string DEFAULT_CONFIG = R"delim(
{
  "category": {
    "id": "models/monica",
    "name": "Models/MONICA"
  },
  "info": {
    "id": "66f86dbb-efb1-4b16-8837-3a2aa1eca30e",
    "name": "MONICA",
    "description": "MONICA FBP component"
  },
  "type": "standard",
  "inPorts": [
    {
      "name": "config",
      "contentType": "common.capnp:StructuredText[JSON | TOML]"
    },
    {
      "name": "env",
      "contentType": "model.capnp:Env[common.capnp:StructuredText[JSON]]"
    }
  ],
  "outPorts": [
    {
      "name": "result",
      "contentType": "common.capnp:StructuredText[JSON]"
    }
  ],
  "defaultConfig": {
    "from_attr": {
      "value": null,
      "type": "string",
      "desc": "Get Env from this attribute instead of content of env IP."
    },
    "to_attr": {
      "value": null,
      "type": "string",
      "desc": "Send content instead in 'to_attr'"
    },
    "monica_sr": {
      "value": null,
      "type": "string",
      "desc": "Use this sturdy ref to connect to an external MONICA, else a MONICA instance will be created."
    }
  }
}
)delim";

class FBPMain {
public:
  enum PORTS { CONFIG, ENV, RESULT };

  std::map<int, kj::StringPtr> inPortNames = {
    {CONFIG, "config"},
    {ENV, "env"},
  };
  std::map<int, kj::StringPtr> outPortNames = {
    {RESULT, "result"},
  };

  explicit FBPMain(kj::ProcessContext& context)
  : ioContext(kj::setupAsyncIo())
  , conMan(ioContext)
  , ports(conMan, inPortNames, outPortNames)
  , context(context) {}

  kj::MainBuilder::Validity setOutputJsonDefaultConfig() {
    outputJsonDefaultConfig = true;
    return true;
  }

  kj::MainBuilder::Validity setName(kj::StringPtr name) {
    this->name = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity setEnvInSr(kj::StringPtr envInSr) {
    this->envInSr = kj::str(envInSr);
    return true;
  }

  kj::MainBuilder::Validity setConfigInSr(kj::StringPtr configInSr) {
    this->configInSr = kj::str(configInSr);
    return true;
  }

  kj::MainBuilder::Validity setResultOutSr(kj::StringPtr resultOutSr) {
    this->resultOutSr = kj::str(resultOutSr);
    return true;
  }

  kj::MainBuilder::Validity setLoglevel(kj::StringPtr loglevel) {
    this->loglevel = kj::str(loglevel);
    return true;
  }

  kj::MainBuilder::Validity setPortInfosReaderSr(kj::StringPtr name) {
    portInfosReaderSr = kj::str(name);
    return true;
  }

  kj::MainBuilder::Validity startComponent() {
    if (outputJsonDefaultConfig) {
      std::cout << DEFAULT_CONFIG << std::endl;
      return true;
    }

    bool startedServerInDebugMode = false;
    json11::Json config;

    debug() << "MONICA: starting MONICA Cap'n Proto FBP component" << endl;
    typedef mas::schema::fbp::IP IP;
    typedef mas::schema::fbp::Channel<IP> Channel;
    typedef mas::schema::model::EnvInstance<mas::schema::common::StructuredText,
                                            mas::schema::common::StructuredText>
      MonicaEnvInstance;
    typedef mas::schema::model::Env<mas::schema::common::StructuredText> Env;

    if (portInfosReaderSr.size() > 0) {
      KJ_LOG(INFO, portInfosReaderSr);
      ports.connectFromPortInfos(portInfosReaderSr);
      KJ_LOG(INFO, "connected to portInfosReaderSr");
    } else if (envInSr.size() > 0 && resultOutSr.size() > 0) {
      ports.connectToSrStr(inPortNames[ENV], envInSr, mas::infrastructure::common::PortConnector::IN);
      if (configInSr.size() > 0) {
        ports.connectToSrStr(inPortNames[CONFIG], configInSr, mas::infrastructure::common::PortConnector::IN);
      }
      ports.connectToSrStr(outPortNames[RESULT], resultOutSr, mas::infrastructure::common::PortConnector::OUT);
    } else {
      KJ_LOG(ERROR, "At least env_in_sr and result_out_sr has to be supplied.");
      return false;
    }


    try {
      if (ports.isInConnected(CONFIG)) {
        KJ_LOG(INFO, "CONFIG port is connected");
        auto configMsg =
          ports.in(CONFIG).readRequest().send().wait(ioContext.waitScope);
        KJ_LOG(INFO, "received msg from CONFIG port");
        // check for end of data from in port
        if (!configMsg.isDone()) {
          auto configIp = configMsg.getValue();
          auto configStr = configIp.getContent().getAs<capnp::Text>();
          auto configJson = parseJsonString(configStr.cStr());
          if (configJson.success()) config = configJson.result;
          else {
            auto defaultConfigJson = parseJsonString(DEFAULT_CONFIG);
            if (defaultConfigJson.success()) config = defaultConfigJson.result;
            else {
              KJ_LOG(ERROR,
                     "Couldn't parse config JSON string from CONFIG port or "
                     "DEFAULT_CONFIG: ",
                     configStr);
              return false;
            }
          }
        }
      }

      KJ_LOG(INFO, config.dump());
      if (config["fromAttr"].is_string()) fromAttr = kj::str(config["fromAttr"].string_value());
      if (config["toAttr"].is_string()) toAttr = kj::str(config["toAttr"].string_value());
      if (config["monica_sr"].is_string()) monicaSr = kj::str(config["monica_sr"].string_value());

      MonicaEnvInstance::Client runMonicaClient(nullptr);
      if (monicaSr.size() > 0) {
        runMonicaClient =
          conMan.tryConnectB(monicaSr.cStr()).castAs<MonicaEnvInstance>();
      } else {
        runMonicaClient = kj::heap<RunMonica>(startedServerInDebugMode);
      }

      while (ports.isInConnected(ENV) && ports.isOutConnected(RESULT)) {
        KJ_LOG(INFO, "trying to read from IN port");
        auto msg = ports.in(ENV).readRequest().send().wait(ioContext.waitScope);
        KJ_LOG(INFO, "received msg from IN port");
        // check for end of data from in port
        if (msg.isDone()) {
          KJ_LOG(INFO, "received done -> exiting main loop");
          break;
        } else {
          auto inIp = msg.getValue();
          auto attr = mas::infrastructure::common::getIPAttr(inIp, fromAttr);
          auto env = attr.orDefault(inIp.getContent()).getAs<Env>();
          KJ_LOG(INFO, "received env -> running MONICA");
          auto rreq = runMonicaClient.runRequest();
          rreq.setEnv(env);
          auto res = rreq.send().wait(ioContext.waitScope);
          KJ_LOG(INFO, "received MONICA result");
          if (res.hasResult() && res.getResult().hasValue()) {
            KJ_LOG(INFO, "result is not empty");
            auto resJsonStr = res.getResult().getValue();
            auto wreq = ports.out(RESULT).writeRequest();
            auto outIp = wreq.initValue();

            // set content if not to be set as attribute
            if (kj::size(toAttr) == 0) {
              auto st = outIp.initContent()
                             .initAs<mas::schema::common::StructuredText>();
              st.setValue(resJsonStr);
              st.setType(mas::schema::common::StructuredText::Type::JSON);
            }
            // copy attributes, if any and set result as attribute, if requested
            auto toAttrBuilder = mas::infrastructure::common::copyAndSetIPAttrs(
                                                                                inIp, outIp, toAttr);
            //, capnp::toAny(resJsonStr));
            KJ_IF_MAYBE(builder, toAttrBuilder) {
              auto st = builder->initAs<mas::schema::common::StructuredText>();
              st.setValue(resJsonStr);
              st.setType(mas::schema::common::StructuredText::Type::JSON);
            }
            KJ_LOG(INFO, "trying to send result on OUT port");
            wreq.send().wait(ioContext.waitScope);
            KJ_LOG(INFO, "sent result on OUT port");
          }
        }
      }
    } catch (const kj::Exception& e) {
      KJ_LOG(INFO, "Exception: ", e.getDescription());
      std::cerr << "Exception: " << e.getDescription().cStr() << endl;
    }

    ports.closeOutPorts();

    return true;
  }

  kj::MainFunc getMain() {
    return kj::MainBuilder(
                           context, kj::str("MONICA FBP Component v", VER_FILE_VERSION_STR),
                           "Offers a MONICA service.")
           .expectOptionalArg("port_infos_reader_SR",
                              KJ_BIND_METHOD(*this, setPortInfosReaderSr))
           .addOption({'O', "output_json_default_config"},
                      KJ_BIND_METHOD(*this, setOutputJsonDefaultConfig),
                      "Output JSON configuration file with default settings at "
                      "commandline. To be used with IIP at 'conf' port.")
           .addOptionWithArg({"env_in_sr"}, KJ_BIND_METHOD(*this, setEnvInSr),
                             "<sturdy_ref>", "Sturdy ref to input channel.")
           .addOptionWithArg({"config_in_sr"}, KJ_BIND_METHOD(*this, setConfigInSr),
                             "<sturdy_ref>", "Sturdy ref to config channel.")
           .addOptionWithArg({"result_out_sr"}, KJ_BIND_METHOD(*this, setResultOutSr),
                             "<sturdy_ref>", "Sturdy ref to output channel.")
           .addOptionWithArg({'n', "name"}, KJ_BIND_METHOD(*this, setName),
                             "<name>", "Name of process to be started.")
           .addOptionWithArg({'l', "log_level"}, KJ_BIND_METHOD(*this, setLoglevel),
                             "<loglevel> ", "Set logging level.")
           .callAfterParsing(KJ_BIND_METHOD(*this, startComponent))
           .build();
  }

private:
  kj::AsyncIoContext ioContext;
  mas::infrastructure::common::ConnectionManager conMan;
  mas::infrastructure::common::PortConnector ports;
  kj::ProcessContext& context;
  kj::String portInfosReaderSr;
  kj::String monicaSr;
  kj::String fromAttr;
  kj::String toAttr;
  kj::String envInSr;
  kj::String configInSr;
  kj::String resultOutSr;
  bool outputJsonDefaultConfig = false;
  kj::String name;
  kj::String loglevel;
};

KJ_MAIN(FBPMain)
