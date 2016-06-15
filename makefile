shell.out: main.cpp noncanmode.cpp
	g++ -std=c++0x -Wall -g main.cpp noncanmode.cpp -o shell.out
