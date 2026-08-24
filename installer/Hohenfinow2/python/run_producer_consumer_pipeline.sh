# Start "monica server" start_monica_server.cmd
export MONICA_PARAMETERS=../../../../monica-parameters
../../../build/monica-zmq-server -bi -i tcp://localhost:6666 -bo -o tcp://localhost:7777
#../../../../monica/_cmake_debug/monica-zmq-server -bi -i tcp://localhost:6666 -bo -o tcp://localhost:7777
pid=$!

# Start "monica producer" start_producer.cmd
python run-producer.py env=test_env.json

# Start /wait "monica consumer" start_consumer.cmd
python run-consumer.py

# monica-zmq-server blocks forever, so it has to be killed explicitly once the consumer is done
#taskkill /F /IM monica-zmq-server.exe >nul 2>&1
kill $pid
