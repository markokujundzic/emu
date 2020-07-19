#include "Data.h"
#include "Assembler.h"

constexpr bool Data::__directive;
constexpr bool Data::__instruction;

Data::Data(const queue<string>& queue, const string& machine_code, const bool& ready, const bool& directive) : tokens(queue), machine_code(machine_code), ready(ready), section_name(Assembler::current_section->name), location_counter(Assembler::location_counter), directive(directive)
{

}

Data::Data(const queue<string>& queue, const _instruction& relocatable_symbols, const string& machine_code, const bool& ready, const bool& directive) : tokens(queue), machine_code(machine_code), ready(ready), section_name(Assembler::current_section->name), location_counter(Assembler::location_counter), directive(directive), relocatable_symbols(relocatable_symbols)
{

}

void Data::delete_data()
{
	for (list<Data*>::iterator it = Assembler::data.begin(); it != Assembler::data.end(); it++)
	{
		delete *it;
	}

	Assembler::data.clear();
}