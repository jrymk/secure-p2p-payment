install-deps:
	sudo apt-get install libgtkmm-3.0-dev
server-run:
	./build/demo_server 5051 -a
client:
	g++ -o ./build/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
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