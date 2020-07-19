#ifndef _unknown_symbol_h_
#define _unknown_symbol_h_

#include <deque>
#include <string>

using namespace std;

class UnknownSymbol
{
public:
	UnknownSymbol(const string& name, const deque<string>& operands, const deque<string>& operations);

	static void delete_unknown_symbols();

	deque<string> operands;
	deque<string> operations;

	string name;

	bool deleted;

	// .equ a, b + c - d

	// LOCAL OR GLOBAL -> SYMBOL TABLE VALUE (b + c - d)
	// EXTERN          -> SYMBOL TABLE VALUE (c - d)

	// LOCAL + PC_RELOCATION        -> b + c - d - NEXT INSTRUCTION OFFSET
	// LOCAL + ABSOLUTE RELOCATION  -> b + c - d

	// GLOBAL + PC_RELOCATION       -> c - d - NEXT INSTRUCTION OFFSET
	// GLOBAL + ABSOLUTE RELOCATION -> c - d
};

#endif