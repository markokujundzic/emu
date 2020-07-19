#ifndef _linker_h_
#define _linker_h_

#include "ObjectFile.h"

#include <string>
#include <deque>
#include <list>
#include <unordered_map>

using namespace std;

class Linker
{
public:
	static constexpr uint16_t _memory_mapped_registers = 0xFF00;
	static constexpr uint16_t _stack_start_address     = 0x0810;
	static constexpr uint16_t _ivt_start_address       = 0x0000;
	static constexpr uint16_t _stack_end_address       = 0x0010;
	static constexpr uint16_t _ivt_end_address         = 0x000F;

	static list<ObjectFile*> object_files;

	static list<Symbol*> symbol_table;

	static deque<pair<string, uint16_t>> section_start_addresses;

	static unordered_map<string, deque<pair<string, uint16_t>>> section_offset;

	static unordered_map<string, string> machine_code;

	static unordered_map<string, list<Relocation*>> relocations;

	static void make_executable(deque<string>& input_object_files);

	static void resolve_start_address(string expression);

	static void link_all_object_files();

	static void check_input();

	static void sort();

	static void delete_object_files();

	static void delete_symbol_table();

	static void delete_relocations();
};

#endif