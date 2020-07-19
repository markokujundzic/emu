#ifndef _loader_h_
#define _loader_h_

#include "Linker.h"

#include <unordered_map>

using namespace std;

class Loader
{
public:
	static constexpr uint32_t MEMORY_SIZE = 0x10000;

	static uint8_t memory[MEMORY_SIZE];

	static uint16_t next_address_to_write;

	static unordered_map<string, uint16_t> loaded_sections;

	static unordered_map<string, uint16_t> section_values;

	static void load_all_object_files();

	static void relocate();

	static void insert_program();
};

#endif