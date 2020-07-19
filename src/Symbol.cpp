#include "Symbol.h"
#include "Assembler.h"

Symbol::Symbol(const string& name, const string& section, const uint16_t& value) : name(name), section(section), defined(false), scope("LOCAL"), value(value), is_symbol(true), is_section(false), size(0), number(0), absolute(false), equ_relocatable(false), is_extern(false), equ_offset(0), relative_relocatable_symbol("N/A")
{

}

Symbol::Symbol(const string& name) : name(name), section(name), defined(true), scope("LOCAL"), value(0), is_symbol(false), is_section(true), size(0), number(0), absolute(false), equ_relocatable(false), is_extern(false), equ_offset(0), relative_relocatable_symbol("N/A")
{

}

void Symbol::define_symbol(const string& symbol_name, const uint16_t& symbol_value)
{
	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		if (symbol_name == (*it)->name)
		{
			if (symbol_value)
			{
				(*it)->value = symbol_value;
			}

			(*it)->section = Assembler::current_section->name;
			(*it)->defined = true;

			break;
		}
	}
}

void Symbol::delete_symbol_table()
{
	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		delete *it;
	}

	Assembler::symbol_table.clear();

	Assembler::num_of_sections = 0;

	Assembler::current_section = nullptr;
}

bool Symbol::exists_in_symbol_table(const string& symbol_name)
{
	bool ret = false;

	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		if (symbol_name == (*it)->name)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

bool Symbol::defined_in_symbol_table(const string& symbol_name)
{
	bool ret = false;

	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		if (symbol_name == (*it)->name && (*it)->defined)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

bool Symbol::declared_in_symbol_table(const string& symbol_name)
{
	bool ret = false;

	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		if (symbol_name == (*it)->name && !(*it)->defined)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

void Symbol::make_global(const string& symbol_name)
{
	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		if (symbol_name == (*it)->name)
		{
			(*it)->scope = "GLOBAL";
			break;
		}
	}
}

void Symbol::create_symbol(const string& name, const uint16_t& value, const uint8_t& type)
{
	Symbol *symbol;

	// type == 0 -> default, type == 1 -> .extern, type == 2 -> .global

	if (type == Assembler::_equ)
	{
		symbol = new Symbol(name, ".und", value);
	}
	else if (type == Assembler::_extern)
	{
		symbol = new Symbol(name, ".und", value);
		symbol->scope = "GLOBAL";
		symbol->defined = true;
		symbol->is_extern = true;
	}
	else
	{
		symbol = new Symbol(name, Assembler::current_section->section, value);

		if (type == Assembler::_global)
		{
			symbol->scope = "GLOBAL";
		}
	}

	Assembler::symbol_table.push_back(symbol);
}

void Symbol::create_section(const string& name)
{
	Assembler::section_names.push_back(name);

	Symbol *section = new Symbol(name);

	if (name != ".und")
	{
		Assembler::current_section->size = Assembler::location_counter;
	}
	else
	{
		Assembler::current_section = section;
	}

	if (name != ".und")
	{
		for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
		{
			if (*it == Assembler::current_section)
			{
				Assembler::symbol_table.insert(++it, section);
				break;
			}
		}
	}
	else
	{
		Assembler::symbol_table.push_back(section);
	}

	Assembler::num_of_sections++;
	Assembler::location_counter = 0;

	if (name != ".und")
	{
		Assembler::current_section = section;
	}
}