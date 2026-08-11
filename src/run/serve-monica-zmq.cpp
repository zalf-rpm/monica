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

#include "serve-monica-zmq.h"

#include <iostream>
#include <map>
#include <tuple>

#include "climate/climate-file-io.h"
#include "run-monica.h"
#include "tools/debug.h"
#include "zeromq/zmq-helper.h"

#ifdef INCLUDE_SR_SUPPORT
#include "capnp-helper.h"
#include "common/rpc-connection-manager.h"
#endif

using namespace std;
using namespace monica;
using namespace Tools;
using namespace json11;
using namespace Soil;
using namespace Climate;

void monica::serveZmqMonicaFull(zmq::context_t *zmqContext,
                                map<SocketRole, SocketConfig> socketAddresses) {
#ifdef INCLUDE_SR_SUPPORT
  auto ioContext = kj::setupAsyncIo();
  mas::infrastructure::common::ConnectionManager conMan(ioContext);
#endif

  bool startedServerInDebugMode = activateDebug;

  if (socketAddresses.empty()) {
    cerr << "No supplied address for a receiving zmq socket! Exiting." << endl;
    return;
  }

  SocketConfig rconfig;
  auto rci = socketAddresses.find(ReceiveJob);
  if (rci != socketAddresses.end())
    rconfig = rci->second;
  vector<string> rAddresses = rconfig.addresses;
  int receiveSocketType = ZMQ_REP;
  if (rconfig.type == Pull)
    receiveSocketType = ZMQ_PULL;
  zmq::socket_t socket(*zmqContext, receiveSocketType);

  try {
    debug() << "MONICA: " << (rconfig.op == bind ? "binding" : "connecting")
            << " monica zeromq receiving socket to address: ";
    for (auto i : kj::indices(rAddresses))
      debug() << (i > 0 ? "," : "") << rAddresses[i];
    debug() << endl;
    for (const auto &address : rAddresses)
      rconfig.op == bind ? socket.bind(address) : socket.connect(address);
    debug() << "MONICA: " << (rconfig.op == bind ? "bound" : "connected")
            << " monica zeromq receiving socket to address: ";
    for (auto i : kj::indices(rAddresses))
      debug() << (i > 0 ? "," : "") << rAddresses[i];
    debug() << endl;

    vector<string> sAddresses = rconfig.addresses;
    SocketConfig sconfig;
    auto sci = socketAddresses.find(SendResult);
    if (sci != socketAddresses.end())
      sconfig = sci->second, sAddresses = sconfig.addresses;
    int sendSocketType = sconfig.type == Router ? ZMQ_ROUTER : ZMQ_PUSH;
    zmq::socket_t sendSocket(*zmqContext, sendSocketType);
    bool distinctSendSocket = sAddresses != rAddresses;

    vector<string> cAddresses = rconfig.addresses;
    SocketConfig cconfig;
    auto cci = socketAddresses.find(Control);
    if (cci != socketAddresses.end())
      cconfig = cci->second, cAddresses = cconfig.addresses;
    int controlSocketType = ZMQ_SUB;
    zmq::socket_t controlSocket(*zmqContext, controlSocketType);
    bool distinctControlSocket = cAddresses != rAddresses;

    zmq::pollitem_t items[] = {{(void *)socket, 0, ZMQ_POLLIN, 0},
                               {(void *)controlSocket, 0, ZMQ_POLLIN, 0}};

    try {
      if (distinctSendSocket) {
        for (const auto &address : sAddresses)
          sconfig.op == bind ? sendSocket.bind(address)
                             : sendSocket.connect(address);
      }

      try {
        int topicCharCount = 0;
        if (distinctControlSocket) {
          auto topic = "finish";
          topicCharCount = strlen(topic);
          for (const auto &address : cAddresses)
            cconfig.op == bind ? controlSocket.bind(address)
                               : controlSocket.connect(address);
          controlSocket.setsockopt(ZMQ_SUBSCRIBE, topic, topicCharCount);
        }

        while (true) {
          try {
            Msg msg;
            zmq::poll(&items[0], distinctControlSocket ? 2 : 1, -1);

            if (items[0].revents & ZMQ_POLLIN)
              msg = receiveMsg(socket);
            if (distinctControlSocket && items[1].revents & ZMQ_POLLIN) {
              msg = receiveMsg(controlSocket, topicCharCount);
            }

            // auto msg = receiveMsg(socket);

            string msgType = msg.type();
            if (msgType == "finish") {
              // only send reply when not in pipeline configuration
              if (rconfig.type != Pull) {
                J11Object resultMsg;
                resultMsg["type"] = "ack";
                try {
                  s_send(distinctSendSocket ? sendSocket : socket,
                         Json(resultMsg).dump());
                } catch (const zmq::error_t &e) {
                  cerr << "Exception on trying to reply to 'finish' request "
                          "with 'ack' message on zmq socket with address(es): ";
                  int i = 0;
                  for (const auto &address : sAddresses)
                    cerr << (i > 0 ? "," : "") << address, ++i;
                  cerr << "! Still will finish MONICA process! Error: ["
                       << e.what() << "]" << endl;
                }
              }
              sendSocket.setsockopt(ZMQ_LINGER, 0);
              sendSocket.close();

              controlSocket.setsockopt(ZMQ_LINGER, 0);
              controlSocket.close();

              socket.setsockopt(ZMQ_LINGER, 0);
              socket.close();

              break;
            } else if (msgType == "Env") {
              auto sharedId = msg.json["sharedId"].is_null()
                                  ? ""
                                  : msg.json["sharedId"].string_value();
              monica::Output out, out2;
              auto customId = msg.json["customId"];
              out.customId = customId;
              out2.customId = customId;
              bool isNoDataPassThrough =
                  customId.is_object() && customId["nodata"].bool_value();
              bool isIC = msg.json["params"]["userCropParameters"]
                                  ["intercropping"]["is_intercropping"]
                                      .bool_value();
              if (isNoDataPassThrough) {
                debug() << "nodata pass through -> customId: "
                        << customId.dump() << endl;
              } else {
                Env env;
                auto pathToSoilDir = fixSystemSeparator(
                    replaceEnvVars("${MONICA_PARAMETERS}/soil/"));
                env.params.siteParameters
                    .calculateAndSetPwpFcSatFunctions["Wessolek2009"] = Soil::
                    getInitializedUpdateUnsetPwpFcSatfromKA5textureClassFunction(
                        pathToSoilDir);
                env.params.siteParameters
                    .calculateAndSetPwpFcSatFunctions["VanGenuchten"] =
                    Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
                env.params.siteParameters
                    .calculateAndSetPwpFcSatFunctions["VanGenuchtenVereecken"] =
                    Soil::updateUnsetPwpFcSatFromVanGenuchtenVereecken;
                env.params.siteParameters
                    .calculateAndSetPwpFcSatFunctions["VanGenuchtenToth"] =
                    Soil::updateUnsetPwpFcSatFromVanGenuchtenToth;
                env.params.siteParameters
                    .calculateAndSetPwpFcSatFunctions["Toth"] =
                    Soil::updateUnsetPwpFcSatFromToth;

                auto errors = env_merge(&env, msg.json);
                if (errors.success()) {
                  EResult<DataAccessor> eda;
                  try {
                    if (!env.climateData.isValid()) {
                      if (!env.climateCSV.empty()) {
                        eda = readClimateDataFromCSVStringViaHeaders(
                            env.climateCSV, env.csvViaHeaderOptions);
                      } else if (!env.pathsToClimateCSV.empty()) {
                        eda = readClimateDataFromCSVFilesViaHeaders(
                            env.pathsToClimateCSV, env.csvViaHeaderOptions);

#ifdef INCLUDE_SR_SUPPORT
                        Climate::DataAccessor finalDA = kj::mv(eda.result);
                        for (const auto &sr : env.pathsToClimateCSV) {
                          if (sr.find("capnp://") == 0) {
                            auto ts =
                                conMan.tryConnectB(sr)
                                    .castAs<mas::schema::climate::TimeSeries>();
                            auto da = dataAccessorFromTimeSeries(ts).wait(
                                ioContext.waitScope);
                            if (!finalDA.isValid()) {
                              finalDA = kj::mv(da);
                            } else {
                              finalDA.mergeClimateData(kj::mv(da), true);
                            }
                          }
                        }
                        eda.result = kj::mv(finalDA);
#endif
                      }
                    }

#ifdef INCLUDE_SR_SUPPORT
                    // no soil data have been loaded, but there might be a capnp
                    // sturdy ref
                    string soilSR;
                    if (msg.json["params"]["siteParameters"]
                                ["SoilProfileParameters"]
                                    .is_string()) {
                      soilSR = msg.json["params"]["siteParameters"]
                                       ["SoilProfileParameters"]
                                           .string_value();
                    }
                    if (!soilSR.empty()) {
                      auto sp = conMan.tryConnectB(soilSR)
                                    .castAs<mas::schema::soil::Profile>();
                      auto soilpsj =
                          fromCapnpSoilProfile(sp).wait(ioContext.waitScope);
                      auto soilps = Soil::createSoilPMs(soilpsj);
                      if (soilps.second.failure())
                        printPossibleErrors(soilps.second, activateDebug);
                      else
                        env.params.siteParameters.vs_SoilParameters =
                            soilps.first;
                    }
#endif

                    if (eda.success()) {
                      if (!env.climateData.isValid())
                        env.climateData = kj::mv(eda.result);

                      env.debugMode = startedServerInDebugMode && env.debugMode;

                      env.params.userSoilMoistureParameters
                          .getCapillaryRiseRate = [](const string &soilTexture,
                                                     size_t distance) {
                        return Soil::readCapillaryRiseRates().getRate(
                            soilTexture, distance);
                      };

                      // isIC = env.params.userCropParameters.isIntercropping;
                      debug() << "running             -> customId: "
                              << env.customId.dump() << endl;
                      auto str = msg.json.dump();
                      std::tie(out, out2) = runMonicaIC(kj::mv(env), isIC);
                      // cout << "out: " << out.to_json().dump() << endl;
                    }
                  } catch (std::exception &e) {
                    eda.appendError(
                        kj::str("Error while running MONICA: ", e.what())
                            .cStr());
                  }
                  out.errors = eda.errors;
                  out.warnings = eda.warnings;
                } else {
                  out.errors = errors.errors;
                  out.warnings = errors.warnings;
                }
              }

              try {
                if (!sharedId.empty())
                  s_sendmore(distinctSendSocket ? sendSocket : socket,
                             sharedId);

                if (isIC) {
                  auto outs =
                      json11::Json(J11Object({{"1", output::to_json(&out)},
                                              {"2", output::to_json(&out2)}}));
                  s_send(distinctSendSocket ? sendSocket : socket, outs.dump());
                } else {
                  s_send(distinctSendSocket ? sendSocket : socket,
                         output::to_json(&out).dump());
                }
              } catch (const zmq::error_t &e) {
                cerr << "Exception on trying to reply with result message on "
                        "zmq socket with address: ";
                for (auto i : kj::indices(sAddresses))
                  cerr << (i > 0 ? "," : "") << sAddresses[i];
                cerr << "! Will continue to receive requests! Error: ["
                     << e.what() << "]" << endl;
              }
            } else {
              J11Object resultMsg;
              resultMsg["type"] = "error";
              debug() << "Error, original message was: " << msg.msg << endl;

              try {
                s_send(distinctSendSocket ? sendSocket : socket,
                       Json(resultMsg).dump());
              } catch (const zmq::error_t &e) {
                cerr << "Exception on trying to reply to '" << msgType
                     << "' request with 'error' message on zmq socket with "
                        "address: ";
                for (auto i : kj::indices(sAddresses))
                  cerr << (i > 0 ? "," : "") << sAddresses[i];
                cerr << "! Still will finish MONICA process! Error: ["
                     << e.what() << "]" << endl;
              }
            }
          } catch (const zmq::error_t &e) {
            cerr << "Exception on trying to receive request message on zmq "
                    "socket with address: ";
            for (auto i : kj::indices(rAddresses))
              cerr << (i > 0 ? "," : "") << rAddresses[i];
            cerr << "! Will continue to receive requests! Error: [" << e.what()
                 << "]" << endl;
          }
        }
      } catch (const zmq::error_t &e) {
        cerr << "Couldn't " << (rconfig.op == bind ? "bind" : "connect")
             << " zmq subscribe socket to address: ";
        for (auto i : kj::indices(cAddresses))
          cerr << (i > 0 ? "," : "") << cAddresses[i];
        cerr << "! Error: " << e.what() << endl;
      }
    } catch (const zmq::error_t &e) {
      cerr << "Couldn't " << (rconfig.op == bind ? "bind" : "connect")
           << " zmq push socket to address: ";
      for (auto i : kj::indices(sAddresses))
        cerr << (i > 0 ? "," : "") << sAddresses[i];
      cerr << "! Error: " << e.what() << endl;
    }
  } catch (const zmq::error_t &e) {
    cerr << "Couldn't " << (rconfig.op == bind ? "bind" : "connect")
         << " zmq socket to address: ";
    for (auto i : kj::indices(rAddresses))
      cerr << (i > 0 ? "," : "") << rAddresses[i];
    cerr << "! Error: " << e.what() << endl;
  }

  debug() << "exiting serveZmqMonicaFull" << endl;
}
