clean:
	rm -f client
build:
	g++ -o client main.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
run:
	./client
build-and-run:
	g++ -o client main.cpp `pkg-config --cflags --libs gtkmm-3.0` -std=c++11
	./client
run-server:
	./server 5050 -a
run-unattended:
	./client &
	disown