#ifndef _assembler_h_
#define _assembler_h_

#include "Error.h"
#include "Symbol.h"
#include "Lexer.h"
#include "Parser.h"
#include "Data.h"
#include "Relocation.h"
#include "UnknownSymbol.h"
#include "Structs.h"

#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <list>
#include <vector>
#include <iomanip>
#include <sstream>
#include <stdlib.h>

using namespace std;

class Assembler
{
public:
	static constexpr uint8_t _default = 0;
	static constexpr uint8_t _extern  = 1;
	static constexpr uint8_t _global  = 2;
	static constexpr uint8_t _equ     = 3;

	static void single_pass();

	static void resolve_unknown_symbols();

	static void check_up();

	static vector<vector<string>> tokens;

	static list<Symbol*> symbol_table;

	static list<Data*> data; // list of all instructions and directives that generate section content

	static list<UnknownSymbol*> table_of_unknown_symbols;

	static deque<string> section_names;

	static Symbol *current_section;

	static uint16_t location_counter;

	static int8_t num_of_sections;

	static string input;
};

#endif