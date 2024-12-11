client:
	g++ -o ./build/client/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
server:
	g++ -o ./build/server/server ./src/server.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
install-deps:
	sudo apt-get update
	sudo apt-get install gcc build-essential -y
	sudo apt-get install libgtkmm-3.0-dev -y
server-run:
	./build/server/server 5000 -s
run:
	./build/client/client
run-a-pair:
	./build/client/client &
	./build/client/client
client-build-and-run-a-pair:
	g++ -o ./build/client/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
	./build/client/client &
	./build/client/client
clean:
	rm -f ./build/*
run-10-clients:
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client &
	./build/client/client
run-demo-server:
	./build/demo_server 5053 -a