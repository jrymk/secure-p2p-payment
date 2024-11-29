client:
	g++ -o ./build/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
server:
	g++ -o ./build/server ./src/server.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
	./build/server 5052 -s
install-deps:
	sudo apt-get update
	sudo apt-get install gcc build-essential -y
	sudo apt-get install libgtkmm-3.0-dev -y
server-run:
	./build/server 5053 -a
run:
	./build/client
run-a-pair:
	./build/client &
	./build/client
client-build-and-run-a-pair:
	g++ -o ./build/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
	./build/client &
	./build/client
clean:
	rm -f ./build/*
run-10-clients:
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client &
	./build/client
run-demo-server:
	./build/demo_server 5053 -a