![Build Status](https://github.com/zalf-rpm/monica/actions/workflows/docker-image.yml/badge.svg)

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

In order to build MONICA one needs to have at least to clone the repositories zalf-rpm/mas-infrastructure and 
zalf-rpm/monica-parameters (parameters repository for MONICA) in addition to monica itself.

After cloning the repositories you should have this file structure:

    monica-master
		      |_ monica
		      |_ monica-parameters
		      |_ mas-infrastructure
   

Also install Python to run the minimal examples. 

## Windows
 see https://github.com/zalf-rpm/monica/wiki/How-to-compile-MONICA-(Windows) for installation and build instructions

monica-run will execute a local MONICA with the example Hohenfinow2 in the installer directory and output its result into 
installer\Hohenfinow2\out.csv.

    cd monica/_cmake_win64
    ./monica-run -o out.csv installer/Hohenfinow2/sim-min.json
    
## Linux

see https://github.com/zalf-rpm/monica/wiki/How-to-compile-MONICA-(Linux) for installation and build instructions.

Create a folder for the output files:
Shell:
    mkdir output_csv

Assign the path to the monica-parameters directory:
Shell:
    export MONICA_PARAMETERS=`pwd`/monica-parameters
Run the standard example and write the results into out.csv
Shell:
    cd monica/_cmake_release
    ./monica-run -o ../../output_csv/out.csv ../installer/Hohenfinow2/sim-min.json

# Usage

To run MONICA locally with the standard Hohenfinow2 example under Windows using the standard installer, 
go to the c:\users\USER_PROFILE\MONICA directory and execute in a Commandshell

    monica-run -o out.csv Examples/Hohenfinow2/sim-min.json

There are the following tools/versions of MONICA available.

* libmonica ... the libmonica.dll/.so with the MONICA core functionality
* monica-run ... the standalone MONICA commandline model (using libmonica)
* monica-zmq-proxy ... a tool to run serverside and forward/distribute jobs (ZMQ job messages) to MONICA workers (servers)
* monica-zmq-server ... a MONICA server accepting ZeroMQ messages and process them, usually to be used in connection with ZMQ proxy/proxies and a cloud of worker monica-zmq-servers


# Installation

On Windows an NSIS installer can be created. The installer will by default install all programm code into c:\program files and
the parameters, databases and examples into c:\users\USER_PROFILE\MONICA. The MONICA install directory will be added to the 
PATH environment variable as well as add it to the PYTHONPATH environment variable. So after a normal installation MONICA should be useable from the commandline.


## Contributing 

Contributions are welcome.

## Credits: Include a section for credits in order to highlight and link to the authors of your project.

MONICA is model of the [Research Platform "Data Analysis & Simulation"](https://www.zalf.de/en/struktur/fds/Pages/default.aspx) at the [Leibniz Centre for Agricultural Landscape Research (ZALF)](http://www.zalf.de/en).

Authors
* Claas Nendel (claas (dot) nendel [at] zalf (dot) de)
* Xenia Specka (xenia (dot) specka [at] zalf (dot) de)
* Michael Berg-Mohnicke (michael (dot) berg [at] zalf (dot) de)

Maintainers:
Currently maintained by the authors.

## License

Monica is distributed under the [Mozilla Public License, v. 2.0](http://mozilla.org/MPL/2.0/)

