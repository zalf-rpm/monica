Monica single setup

This example shows how to use the monica to run a single simulation. The example uses the Hohenfinow2 data set.

The required data files are:
climate.csv - climate data
sim.json - simulation settings
site.json - site settings(soil, slope, etc.)
crop.json - crop settings (crop, cultivar, rotation etc.)


These files are depending on base files located in monica-parameters folder.
The monica-parameters folder should be available to the monica server and project files as environment variable `MONICA_PARAMETERS`.

To start a run, execute the following command:
run_monica.cmd

The command will set the environment variable `MONICA_PARAMETERS` and start monica-run.exe with the following arguments:
monica-run.exe -o .\sim-out.csv sim.json

The output file(sim-out.csv) will be written to the current directory.
