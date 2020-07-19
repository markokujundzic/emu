#include "Linker.h"

list<ObjectFile*> Linker::object_files;

list<Symbol*> Linker::symbol_table;

deque<pair<string, uint16_t>> Linker::section_start_addresses;

unordered_map<string, string> Linker::machine_code;

unordered_map<string, list<Relocation*>> Linker::relocations;

unordered_map<string, deque<pair<string, uint16_t>>> Linker::section_offset;

constexpr uint16_t Linker::_memory_mapped_registers;

constexpr uint16_t Linker::_stack_start_address;

constexpr uint16_t Linker::_ivt_start_address;

constexpr uint16_t Linker::_stack_end_address;

constexpr uint16_t Linker::_ivt_end_address;

void Linker::sort()
{
	bool contains = false;

	for (const auto& x : Linker::section_start_addresses)
	{
		if (x.first == "iv_table")
		{
			contains = true;
			break;
		}
	}

	if (!contains)
	{
		Linker::section_start_addresses.push_front(make_pair("iv_table", 0x0000));
	}

	for (deque<pair<string, uint16_t>>::iterator i = Linker::section_start_addresses.begin(); i != Linker::section_start_addresses.end(); i++)
	{
		for (deque<pair<string, uint16_t>>::iterator j = i + 1; j != Linker::section_start_addresses.end(); j++)
		{
			if (j->second < i->second)
			{
				string t_string = i->first;
				uint16_t t_val = i->second;

				i->first = j->first;
				i->second = j->second;

				j->first = t_string;
				j->second = t_val;
			}
		}
	}
}

void Linker::resolve_start_address(string expression)
{
	expression = expression.erase(0, 7);

	size_t at_sign = expression.find_first_of('@', 0);

	string section = expression.substr(0, at_sign);

	expression = expression.erase(0, at_sign + 1);

	uint16_t value = (uint16_t) (strtol(expression.c_str(), nullptr, 0) & 0xFFFF);

	if (value > Linker::_memory_mapped_registers)
	{
		throw Error("Loading Error: memory address space from 0xFF00 to 0xFFFF is reserved for memory mapped registers, hence it cannot be used for storing section data.");
	}
	else if (value >= 0x0000 && value < 0x0010 && section != "iv_table")
	{
		throw Error("Loading Error: memory address space from 0x0000 to 0x000F is reserved for interrupt vector table, hence it cannot be used for storing section data.");
	}
	else if (value >= 0x0010 && value < 0x0810 && section != "iv_table")
	{
		throw Error("Loading Error: memory address space from 0x0010 to 0x080F is reserved for system stack, hence it cannot be used for storing section data.");
	}

	if (section == "iv_table" && value != 0x0000)
	{
		throw Error("Loading Error: iv_table must be loaded to memory address 0x0000.");
	}

	Linker::section_start_addresses.push_back(make_pair(section, value));
}

void Linker::link_all_object_files()
{
	Symbol *und = new Symbol(".und");

	Linker::symbol_table.push_back(und);

	int16_t entry = 1;

	bool iv_table_section = false;

	for (list<ObjectFile*>::iterator it = Linker::object_files.begin(); it != Linker::object_files.end(); it++)
	{
		for (list<Symbol*>::iterator ite = (*it)->sections.begin(); ite != (*it)->sections.end(); ite++)
		{
			if ((*ite)->name == "iv_table")
			{
				iv_table_section = true;
			}

			bool exists = false;

			uint16_t size = 0;

			Symbol *current_section = nullptr;

			for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
			{
				if ((*iter)->name == (*ite)->name)
				{
					exists = true;
					size = (*iter)->size;
					current_section = *iter;
					break;
				}
			}

			if (exists)
			{
				Linker::section_offset[(*ite)->name].push_back(make_pair((*it)->input_file_name, size));

				uint16_t size_increment = Lexer::determine_section_size((*it)->machine_code[(*ite)->name]);

				current_section->size += size_increment;

				Linker::machine_code[current_section->name] += (*it)->machine_code[current_section->name];
				Linker::machine_code[current_section->name] += " ";
			}
			else
			{
				Symbol *_new_section = new Symbol((*ite)->name);

				_new_section->size = Lexer::determine_section_size((*it)->machine_code[(*ite)->name]);
				_new_section->number = entry++;

				Linker::symbol_table.push_back(_new_section);

				deque<pair<string, uint16_t>> temp = { };

				Linker::section_offset.insert(make_pair((*ite)->name, temp));

				Linker::section_offset[(*ite)->name].push_back(make_pair((*it)->input_file_name, 0));

				Linker::machine_code.insert(make_pair((*ite)->name, (*it)->machine_code[(*ite)->name]));

				if (!Linker::machine_code[(*ite)->name].empty())
				{
					Linker::machine_code[(*ite)->name] += " ";
				}
			}
		}
	}

	if (!iv_table_section)
	{
		throw Error("Linking Error: iv_table section not found in any of the object files.");
	}

	for (auto& x : Linker::machine_code)
	{
		if (!x.second.empty())
		{
			x.second.pop_back();
		}
	}

	bool _start = false;

	for (list<ObjectFile*>::iterator it = Linker::object_files.begin(); it != Linker::object_files.end(); it++)
	{
		for (list<Symbol*>::iterator ite = (*it)->symbols.begin(); ite != (*it)->symbols.end(); ite++)
		{
			if ((*ite)->is_symbol)
			{
				if ((*ite)->name == "_start")
				{
					_start = true;
				}
			}

			bool exists = false;
			bool defined = false;
			bool ext = false;

			Symbol *current = nullptr;

			for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
			{
				if ((*iter)->name == (*ite)->name)
				{
					current = *iter;
					defined = (*iter)->defined;
					ext = (*ite)->is_extern;
					exists = true;
					break;
				}
			}

			if (exists && ext)
			{
				continue;
			}

			if (exists)
			{
				if (defined)
				{
					string error = "Linking Error: symbol " + (*ite)->name + " has already been defined.";

					throw Error(error);
				}
				else
				{
					current->value = (*ite)->value;
					current->section = (*ite)->section;
					current->defined = true;
				}
			}
			else
			{
				Symbol *_sym = new Symbol((*ite)->name, (*ite)->section, (*ite)->value);

				_sym->absolute = (*ite)->absolute;
				_sym->scope = (*ite)->scope;
				_sym->is_extern = (*ite)->is_extern;
				_sym->equ_relocatable = (*ite)->equ_relocatable;
				_sym->relative_relocatable_symbol = (*ite)->relative_relocatable_symbol;
				_sym->number = entry++;

				if (!_sym->is_extern)
				{
					_sym->defined = true;
				}

				Linker::symbol_table.push_back(_sym);
			}
		}
	}

	if (!_start)
	{
		throw Error("Linking Error: file entry point (_start symbol) is missing.");
	}

	for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
	{
		if ((*iter)->is_symbol)
		{
			if ((*iter)->equ_relocatable && !(*iter)->defined)
			{
				for (list<Symbol*>::iterator itera = Linker::symbol_table.begin(); itera != Linker::symbol_table.end(); itera++)
				{
					if ((*itera)->name == (*iter)->relative_relocatable_symbol)
					{
						if (!(*itera)->defined)
						{
							string error = "Linking Error: symbol " + (*itera)->name + " is undefined.";

							throw Error(error);
						}
						else
						{
							(*iter)->defined = true;
							(*iter)->section = (*itera)->section;
						}

						break;
					}
				}
			}
			else if ((*iter)->defined)
			{
				continue;
			}
			else if (!(*iter)->defined)
			{
				string error = "Linking Error: symbol " + (*iter)->name + " is undefined.";

				throw Error(error);
			}
		}
	}
}

void Linker::make_executable(deque<string>& input_object_files)
{
	while (!input_object_files.empty())
	{
		string current_object_file = input_object_files.front();
		input_object_files.pop_front();

		ObjectFile *_obj = new ObjectFile(current_object_file);

		_obj->extract();

		Linker::object_files.push_back(_obj);
	}

	Linker::link_all_object_files();

	Linker::sort();

	Linker::check_input();
}

void Linker::check_input()
{
	for (const auto& section : Linker::section_start_addresses)
	{
		bool exists = false;

		for (const auto& it : Linker::symbol_table)
		{
			if (section.first == it->name)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			string error = "Loading Error: supplied section does not exist in symbol table -> " + section.first + " section.";

			throw Error(error);
		}
	}
}

void Linker::delete_object_files()
{
	for (list<ObjectFile*>::iterator it = Linker::object_files.begin(); it != Linker::object_files.end(); it++)
	{
		delete *it;
	}

	Linker::object_files.clear();
}

void Linker::delete_symbol_table()
{
	for (list<Symbol*>::iterator it = Linker::symbol_table.begin(); it != Linker::symbol_table.end(); it++)
	{
		delete *it;
	}

	Linker::symbol_table.clear();
}

void Linker::delete_relocations()
{
	unordered_map<string, list<Relocation*>>::iterator it = Linker::relocations.begin();

	while (it != relocations.end())
	{
		for (list<Relocation*>::iterator itr = it->second.begin(); itr != it->second.end(); itr++)
		{
			delete *itr;
		}

		it++;
	}

	Linker::relocations.clear();
}