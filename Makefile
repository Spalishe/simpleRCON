server:
	g++ -Iinclude -ljsoncpp -std=c++20 server.cpp -o simpleRCON

client:
	g++ -Iinclude -ljsoncpp -std=c++20 client.cpp -o simpleRCON
