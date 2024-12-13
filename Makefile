client:
	mkdir -p ./build/client1
	g++ -o ./build/client1/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11 -lssl -lcrypto
	cd ./build/client1 && ./client
client2:
	mkdir -p ./build/client1
	mkdir -p ./build/client2
	g++ -o ./build/client1/client ./src/client.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11 -lssl -lcrypto
	cp ./build/client1/client ./build/client2
	cd ./build/client1 && ./client &
	cd ./build/client2 && ./client
run:
	cd ./build/client && ./client
server:
	mkdir -p ./build/server
	g++ -o ./build/server/server ./src/server.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11 -lssl -lcrypto
	cd ./build/server && ./server 5001 -a
server-run:
	cd ./build/server && ./server 5001 -a
install-deps:
	sudo apt-get update
	sudo apt-get install gcc build-essential -y
	sudo apt-get install libgtkmm-3.0-dev -y
	sudo apt-get install libssl-dev -y
clean:
	rm -f ./build/*