install-deps:
	sudo apt-get install libgtkmm-3.0-dev
server-run:
	./build/demo_server 5050 -a
client:
	g++ -o ./build/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
client-run:
	./build/client
client-build-and-run:
	g++ -o ./build/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
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
	./build/client &