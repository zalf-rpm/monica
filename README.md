# MONICA - The Model for Nitrogen and Carbon in Agro-ecosystems

MONICA is a dynamic, process-based simulation model which describes transport and bio-chemical turn-over of carbon, nitrogen and water in agro-ecosystems. On daily time steps the most important processes in soil and plant are modelled mechanistically. They are linked in such way that feed-back relations of the single processes are reproduced as close to nature as possible. MONICA works one dimensional and represents a space of 1 m² surface area and 2 m depth.

The acronym MONICA is derived from „MOdel of Nitrogen and Carbon dynamics in Agro-ecosystems”.

# Content

# Building

In order to build MONICA one needs to have at least to clone the repositories zalf-lsa/util (non core MONICA code), 
zalf-lsa/sys-libs (source of some external libraries and pre-build binaries for Windows) and 
zalf-lsa/monica-parameters (parameters repository for MONICA). 
If the repositories are cloned alongside the MONICA repository no pathes have to be adjusted in the project files.

To be able to call MONICA from Python (build the minimal Python interface) [Boost.Python](http://www.boost.org/doc/libs/1_63_0/libs/python/doc/html/index.html) is required. 
Simply download and un-archive the whole BOOST distribution eg. [boost-1.63.7z](https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.7z).
Also install Python to have the Python include directory available. Both pathes have to be adjusted in the monica-python project properties page 
in the "C/C++|General|Additional Include Directories" section.

In order to run the examples, copy the file db-connections.ini.template and rename it to db-connections.ini. 
This file is used to configure access to SQLite and MySQL databases (access to the later is not being compiled into MONICA by default).

## Windows (Visual Studio)

Use Visual Studio 2015 (Community is enough) and open monica.sln in project-files. Check that all pathes are correct in the projects.
You should be able to compile successfully all projects. 

The project monica-run will execute a local MONICA with the example Hohenfinow2 in the installer directory and output its result into 
installer\Hohenfinow2\out.csv.


## Linux





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

Maintainers:
Currently maintained by the authors.

## License

Monica is distributed under the [Mozilla Public License, v. 2.0](http://mozilla.org/MPL/2.0/)

