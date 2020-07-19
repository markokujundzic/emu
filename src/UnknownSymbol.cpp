#include "UnknownSymbol.h"
#include "Assembler.h"

UnknownSymbol::UnknownSymbol(const string& name, const deque<string>& operands, const deque<string>& operations) : name(name), operands(operands), operations(operations), deleted(false)
{

}

void UnknownSymbol::delete_unknown_symbols()
{
	for (list<UnknownSymbol*>::iterator it = Assembler::table_of_unknown_symbols.begin(); it != Assembler::table_of_unknown_symbols.end(); it++)
	{
		delete *it;
	}

	Assembler::table_of_unknown_symbols.clear();
}