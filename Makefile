all: tsamserver tsamclient

tsamserver: tsamserver.cpp
	g++ tsamserver.cpp -o tsamserver

tsamclient: tsamclient.cpp
	g++ tsamclient.cpp -o tsamclient

clean:
	rm -f tsamserver tsamclient