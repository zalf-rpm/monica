Producer/Consumer pipeline for Hohenfinow2 data

This example shows how to use the producer/consumer pattern to process data in a pipeline. The example uses the Hohenfinow2 data set.

A producer generates data and sends it to a monica server. 
The monica server runs a simulation and sends the results to a consumer.

The producer and consumer are implemented as python scripts. The producer script is called `run-producer.py` and the consumer script is called `run-consumer.py`. The scripts are located in the `python` folder.

The required data files are:
climate-min.csv
crop-min.csv
site-min.csv
sim-min.csv

These files are depending on base files located in monica-parameters folder.
The monica-parameters folder should be available to the monica server and project files as environment variable `MONICA_PARAMETERS`.

To start the whole pipeline, navigate to the `python` folder and run the following command:
start_producer_consumer_pipeline.cmd

To start single components, run the following commands:
start_monica_server.cmd
start_consumer.cmd
start_producer.cmd

The monica server will keep running until it is stopped by pressing `Ctrl+C` in the console window. The consumer and producer will stop automatically after they have finished their work.

