#ifndef _data_h_
#define _data_h_

#include "Relocation.h"
#include "Structs.h"

#include <queue>
#include <string>
#include <list>

using namespace std;

class Data
{
public:
	Data(const queue<string>& queue, const _instruction& relocatable_symbols, const string& machine_code, const bool& ready = false, const bool& directive = false);
	Data(const queue<string>& queue, const string& machine_code, const bool& ready = false, const bool& directive = false);

	static void delete_data();

	static constexpr bool __directive = true;
	static constexpr bool __instruction = false;

	queue<string> tokens; // all instruction tokens

	list<Relocation*> relocations;

	string section_name; // name of the section where the instruction is stored
	string machine_code; // string representation in hex

	uint16_t location_counter;

	_instruction relocatable_symbols;

	bool ready;
	bool directive; // is it a directive or an instruction
};

#endif