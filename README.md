# MONICA - The Model for Nitrogen and Carbon in Agro-ecosystems

MONICA is a dynamic, process-based simulation model which describes transport and bio-chemical turn-over of carbon, nitrogen and water in agro-ecosystems. On daily time steps the most important processes in soil and plant are modelled mechanistically. They are linked in such way that feed-back relations of the single processes are reproduced as close to nature as possible. MONICA works one dimensional and represents a space of 1 m² surface area and 2 m depth.

The acronym MONICA is derived from „MOdel of Nitrogen and Carbon dynamics in Agro-ecosystems”.

# Content

* [Download](#download)
* [Building](#building)
  * [Windows](#windows)
  * [Linux](#linux)
* [Usage](#usage)
* [Installation](#installation)
* [Contributing](#contributing)
* [License](#license)


# Download

Builds can be downloaded from the repository's [releases page](https://github.com/zalf-rpm/monica/releases).


# Building

In order to build MONICA one needs to have at least to clone the repositories zalf-rpm/util (non core MONICA code), zalf-rpm/sys-libs (includes of some external libraries and pre-build binaries for Windows) and 
zalf-rpm/monica-parameters (parameters repository for MONICA) in addition to monica itself.
The easiest way to do this it to clone zalf-rpm/monica-master with its submodules.

After cloning the repositories you should have this file structure:

    monica-master
		      |_ monica
		      |_ sys-lib
		      |_ util
		      |_ monica-parameters
   

To be able to call MONICA from Python (build the minimal Python interface) [Boost.Python](http://www.boost.org/doc/libs/1_66_0/libs/python/doc/html/index.html) is required. 
Simply download and un-archive the whole BOOST distribution eg. [boost-1.66.7z](https://sourceforge.net/projects/boost/files/boost/1.66.0/boost_1_66_0.7z).
After unpacking boost ist will have following folder structure:

    |_ boost_1_66_0
       |_ boost
       |_ doc
       |_ lib
       |_ ...
Either copy the boost_1_66_0 folder into the monica-master folder or create a symlink 

  > mklink /D boost <your boost path>` (e.g. mklink /D boost C:\boost_1_66_0)
 
Boost has to be on the same level as monica.
Also install Python to have the Python include directory available. 

In order to run the examples, copy the file db-connections.ini.template and rename it to db-connections.ini. 
This file is used to configure access to SQLite and MySQL databases (access to the later is not being compiled into MONICA by default).

## Windows

Use Visual Studio 2017 (Community is enough) and cmake (3.2+) to create a solution file.
run monica/update_solution.cmd or run monica/update_solution_x64.cmd. 
These batch scripts will create folder (´_cmake_win32´ or ´_cmake_win64´) in which you can find a monica.sln.

You should be able to compile successfully all projects. 

The project monica-run will execute a local MONICA with the example Hohenfinow2 in the installer directory and output its result into 
installer\Hohenfinow2\out.csv.


## Linux

To build MONICA under Linux, run inside the monica-master repository directory

    >mkdir -p `_cmake_linux`
    >cd `_cmake_linux`
    >cmake ../monica
   
to create the makefile.

Then a `make` should you leave with a working version of MONICA. If you compiled the code in the repository directory running

    ./monica run installer/Hohenfinow2/sim.json > out.csv

from the shell should run the standard example and write the results into out.csv.


# Usage

MONICA consists right now of a number of tools/parts. Most of them can be called at the commandline like

    monica command1 [command2] parameter1 parameter2 ....

eg. to run MONICA locally with the standard Hohenfinow2 example under Windows using the standard installer, 
go to the c:\users\USER_PROFILE\MONICA directory and execute in a Commandshell

    monica run Examples/Hohenfinow2/sim.json > out.csv

Alternatively one can type always the full name of the tool, which are named monica-run, monica-zmq-run, monica-zmq-server etc, 
monica is just a proxy program calling the actual tools. There are the following tools/versions of MONICA available.

* libmonica ... the libmonica.dll/.so with the MONICA core functionality
* monica ... the proxy tool, to call the other tools
* monica-python ... the monica_python.dll/.so to be used as Python library, acting as simple bridge to call MONICA from Python
* monica-run ... the standalone MONICA commandline model (using libmonica)
* monica-zmq-control ... a tool to run on the server and accepting ZeroMQ messages to spawn MONICA servers/ZMQ Proxies etc
* monica-zmq-control-send ... a client side tool to send ZMQ messages to the server 
* monica-zmq-proxy ... a tool to run serverside and forward/distribute jobs (ZMQ job messages) to MONICA workers (servers)
* monica-zmq-run ... the ZMQ client to monica-zmq-server/the equivalent to monica-run, but sends work to a monica-zmq-server
* monica-zmq-server ... a MONICA server accepting ZeroMQ messages and process them, usually to be used in connection with ZMQ proxy/proxies and a cloud of worker monica-zmq-servers


# Installation

On Windows an NSIS installer can be created. The installer will by default install all programm code into c:\program files and
the parameters, databases and examples into c:\users\USER_PROFILE\MONICA. The MONICA install directory will be added to the 
PATH environment variable as well as add it to the PYTHONPATH environment variable. So after a normal installation MONICA should be useable
from the commandline as well as from Python (import monica_python).


## Contributing 

Contributions are welcome.

## Credits: Include a section for credits in order to highlight and link to the authors of your project.

MONICA is model of the [Institue of Landscape Systems Analysis](http://www.zalf.de/en/institute_einrichtungen/lsa/Pages/default.aspx) at the [Leibniz Centre for Agricultural Landscape Research (ZALF)](http://www.zalf.de/en).

Authors
* Claas Nendel (claas (dot) nendel [at] zalf (dot) de)
* Xenia Specka (xenia (dot) specka [at] zalf (dot) de)
* Michael Berg-Mohnicke (michael (dot) berg [at] zalf (dot) de)
* Tommaso Stella (tommaso (dot) stella [at] zalf (dot) de)

Maintainers:
Currently maintained by the authors.

## License

Monica is distributed under the [Mozilla Public License, v. 2.0](http://mozilla.org/MPL/2.0/)

