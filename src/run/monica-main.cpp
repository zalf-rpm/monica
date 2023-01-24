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
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <set>

using namespace std;

string appName = "monica";
string version = "2.0.0-beta";

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  auto printHelp = [=]()
  {
    cout
      << appName << " commands/options" << endl
      << endl
      << "commands/options:" << endl
      << endl
      << " -h | --help ... this help output" << endl
      << " -v | --version ... outputs " << appName << " version" << endl
      << endl
      << "   run PARAMETERS ... start monica-run with PARAMETERS" << endl
      << " | zmq   " << endl //run PARAMETERS ... run MONICA ZeroMQ client 'monica-zmq-run' with PARAMETERS" << endl
      << "     | server PARAMETERS ... run MONICA ZeroMQ server 'monica-zmq-server' with PARAMETERS" << endl
      << "     | proxy PARAMETERS ... run MONICA ZeroMQ proxy 'monica-zmq-proxy' with PARAMETERS" << endl;
      //<< "     | control PARAMETERS ... run MONICA ZeroMQ control node 'monica-zmq-control' with PARAMETERS" << endl
      //<< "     | control send PARAMETERS ... send command to MONICA ZeroMQ control node via calling 'monica-zmq-control-send' with PARMETERS" << endl;
  };

  auto printZmqHelp = [=]()
  {
    cout
      << appName << " zmq commands/options" << endl
      << endl
      << "commands/options:" << endl
      << endl
      << " -h | --help ... this help output" << endl
      << endl
      //<< "   run PARAMETERS ... run MONICA ZeroMQ client 'monica-zmq-run' with PARAMETERS" << endl
      << " | server PARAMETERS ... run MONICA ZeroMQ server 'monica-zmq-server' with PARAMETERS" << endl
      << " | proxy PARAMETERS ... run MONICA ZeroMQ proxy 'monica-zmq-proxy' with PARAMETERS" << endl
      //<< " | control PARAMETERS ... run MONICA ZeroMQ control node 'monica-zmq-control' with PARAMETERS" << endl
      //<< " | control send PARAMETERS ... send command to MONICA ZeroMQ control node via calling 'monica-zmq-control-send' with PARMETERS" << endl;
  };

  if(argc > 1)
  {
    vector<string> args;

    for(auto i = 1; i < argc; i++)
    {
      string arg = argv[i];
      if(arg == "-h" || arg == "--help")
      {
        if(args.empty())
          printHelp(), exit(0);
        else if(args.back() == "zmq")
          printZmqHelp(), exit(0);
        else
          args.push_back(arg);
      }
      else if(args.empty() 
              && (arg == "-v" || arg == "--version"))
        cout << appName << " version " << version << endl, exit(0);
      else
        args.push_back(arg);
    }

    set<string> s = { "run", "zmq", "proxy", "server" };// , "control", "send"	};

    ostringstream oss;
    oss << "monica";
    int i = 0;
    for(auto a : args)
      oss << (s.find(a) != s.end() ? "-" : " ") << a;

    if(oss.str() == "monica-zmq")
      printZmqHelp();
    else if(oss.str().substr(0, 7) != "monica-")
      printHelp();
    else
      system(oss.str().c_str());
  }
  else
    printHelp();

  return 0;
}
