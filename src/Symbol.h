#ifndef _symbol_h_
#define _symbol_h_

#include "Data.h"

#include <string>
#include <iostream>
#include <deque>

using namespace std;

class Symbol
{
public:
	Symbol(const string& name, const string& section, const uint16_t& value); // symbol constructor
	Symbol(const string& name); // section constructor

	static void create_symbol(const string& name, const uint16_t& value = 0, const uint8_t& type = 0); // value == 0 for undefined symbol
	static void create_section(const string& name);

	static void define_symbol(const string& symbol_name, const uint16_t& symbol_value = 0); // don't forger to call!

	static bool exists_in_symbol_table(const string& symbol_name);

	static bool defined_in_symbol_table(const string& symbol_name);
	static bool declared_in_symbol_table(const string& symbol_name);

	static void make_global(const string& symbol_name);

	static void delete_symbol_table();

	string name; 
	string section;
	string scope; 

	string relative_relocatable_symbol;

	uint16_t value;
	uint16_t size; // size == 0 for symbol

	uint16_t equ_offset;

	int16_t number;

	bool defined;

	bool is_symbol; 
	bool is_section;

	bool absolute; // if (!true) -> relocatable; if (true) -> absolute
	bool equ_relocatable;

	bool is_extern;
};

#endif