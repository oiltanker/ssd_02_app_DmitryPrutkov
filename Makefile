all: ssd_02_app

ssd_02_app: main.cpp client.cpp service.cpp
	g++ $^ -std=c++17 -lcpprest -lpthread -lssl -lcrypto -lboost_system -O3 -o $@