./bin/emulator : $(patsubst ./src/%.cpp, ./bin/%.o, $(wildcard ./src/*.cpp))
	g++ -pthread -static -g -I ./src -std=c++17 $^ -o $@ 

./bin/%.o : ./src/%.cpp
	g++ -pthread -static -g -I ./src -std=c++17 -c $< -o $@ 

clean:
	rm -f ./bin/*.o
	rm -f .bin/assembler

.PHONY: clean

run: 
	bin/emulator -place=.text@0x1400 -place=iv_table@0x0000 tests/input1.o 


