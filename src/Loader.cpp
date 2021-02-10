#include "Loader.h"

constexpr uint32_t Loader::MEMORY_SIZE;

uint8_t Loader::memory[MEMORY_SIZE] = { 0x00 };

uint16_t Loader::next_address_to_write = 0x0810;

unordered_map<string, uint16_t> Loader::loaded_sections;

unordered_map<string, uint16_t> Loader::section_values;

/*
0xFFFF	---------------------------------------------
	|          MEMORY MAPPED REGISTERS          |
0xFF00	|-------------------------------------------|
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
	|                                           |
0x0810	|                                           | -> FIRST UNOCCUPIED ADDRESS : 0x0810
0x080F	|-------------------------------------------|
	|                                           |
	|                   STACK                   | -> STACK SIZE : 0x080F - 0x0010 + 0x1 = 0x0800 -> 2048 B
0x0010	|                                           |
0x000F	|-------------------------------------------|
	|                    IVT                    |
0x0000	---------------------------------------------
*/

void Loader::relocate()
{
	Symbol *und = new Symbol(".und");

	for (list<ObjectFile*>::iterator it = Linker::object_files.begin(); it != Linker::object_files.end(); it++)
	{
		list<Symbol*> helper;

		helper.push_back(und);

		for (const auto& itr : (*it)->sections)
		{
			helper.push_back(itr);
		}

		for (const auto& itr : (*it)->symbols)
		{
			helper.push_back(itr);
		}

		for (auto& itr : (*it)->relocations)
		{
			if (itr.second.empty())
			{
				continue;
			}

			for (auto& rel : itr.second)
			{
				bool escape = false;

				for (const auto& x : Linker::section_offset)
				{
					auto temp = x.second;

					for (const auto& help : temp)
					{
						if (help.first == (*it)->input_file_name)
						{
							rel->offset += help.second;
							escape = true;
							break;
						}
					}

					if (escape)
					{
						break;
					}
				}

				uint16_t value = rel->value;

				string symbol;

				for (const auto& sym : helper)
				{
					if (value == sym->number)
					{
						symbol = sym->name;
						break;
					}
				}

				for (const auto& sym : Linker::symbol_table)
				{
					if (symbol == sym->name)
					{
						rel->value = sym->number;
						break;
					}
				}

				Relocation *_rel = new Relocation(rel->offset, rel->value, rel->type, rel->section, (*it)->input_file_name);

				if (Linker::relocations.find(itr.first) == Linker::relocations.end())
				{
					list<Relocation*> temp = {};

					Linker::relocations.insert(make_pair(itr.first, temp));
				}

				Linker::relocations[itr.first].push_back(_rel);
			}
		}
	}

	delete und;

	for (unordered_map<string, list<Relocation*>>::iterator it = Linker::relocations.begin(); it != Linker::relocations.end(); it++)
	{
		list<Relocation*> temp = (*it).second;

		while (!temp.empty())
		{
			Relocation *current_relocation = temp.front();
			temp.pop_front();

			uint16_t old = current_relocation->offset;

			uint16_t val = current_relocation->value;

			uint16_t symbol_value = 0;

			string section_name;
			bool is_section = false;

			uint16_t section_offset = 0;
			bool to_write = false;

			string section_to_add = (*it).first;

			for (const auto& it : Linker::symbol_table)
			{
				if (it->number == val)
				{
					section_name = it->section;
					symbol_value = it->value;
					is_section = it->is_section ? true : false;
					break;
				}
			}

			for (const auto& it : Linker::symbol_table)
			{
				if (section_to_add == it->name)
				{
					current_relocation->value += it->value;
					break;
				}
			}

			if (is_section)
			{
				bool escape = false;

				for (const auto& x : Linker::section_offset)
				{
					auto t = x.second;

					for (const auto& help : t)
					{
						if (help.first == current_relocation->object_file)
						{
							section_offset += help.second;
							escape = true;
							to_write = true;
							break;
						}
					}

					if (escape)
					{
						break;
					}
				}
			}

			if (current_relocation->type == "R_386_PC16")
			{
				// PC-relative relocation

				if (to_write)
				{
					Lexer::change_section_content(old, Linker::machine_code[it->first], section_offset);
				}

				Lexer::change_section_content(old, Linker::machine_code[it->first], symbol_value);
				Lexer::change_section_content(old, Linker::machine_code[it->first], (int16_t) (-current_relocation->offset));
			}
			else if (current_relocation->type == "R_386_8" || current_relocation->type == "R_386_16")
			{
				// absolute relocation

				if (current_relocation->type == "R_386_8")
				{
					if (to_write)
					{
						Lexer::change_section_content(old, Linker::machine_code[it->first], (uint8_t) (section_offset & 0xFF));
					}

					Lexer::change_section_content(old, Linker::machine_code[it->first], (uint8_t) (symbol_value & 0xFF));
				}
				else if (current_relocation->type == "R_386_16")
				{
					if (to_write)
					{
						Lexer::change_section_content(old, Linker::machine_code[it->first], section_offset);
					}

					Lexer::change_section_content(old, Linker::machine_code[it->first], symbol_value);
				}
			}
			else
			{
				string error = "Loading Error: unrecognized relocation type -> " + current_relocation->type + ".";

				throw Error(error);
			}
		}
	}
}

void Loader::load_all_object_files()
{
	for (deque<pair<string, uint16_t>>::iterator it = Linker::section_start_addresses.begin(); it != Linker::section_start_addresses.end(); it++)
	{
		uint16_t size = 0;

		if (it->first == "iv_table" && it->second == 0x0000)
		{
			Loader::loaded_sections.insert(make_pair(it->first, it->second));

			continue;
		}

		for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
		{
			if ((*iter)->name == it->first)
			{
				(*iter)->value = it->second;

				size = (*iter)->size;

				break;
			}
		}

		if (!size)
		{
			continue;
		}

		if (it->second + size > Linker::_memory_mapped_registers)
		{
			string error = "Loading Error: section " + it->first + " overlaps with memory space reserved for memory mapped register.";

			throw Error(error);
		}
		else if (it->second < Loader::next_address_to_write)
		{
			string overlap;

			for (const auto& it : Linker::symbol_table)
			{
				if (it->value + it->size == Loader::next_address_to_write)
				{
					overlap = it->name;
					break;
				}
			}

			string error = "Loading Error: section " + it->first + " overlaps with section " + overlap + ".";

			throw Error(error);
		}
		else
		{
			Loader::next_address_to_write = it->second + size;

			Loader::loaded_sections.insert(make_pair(it->first, it->second));
		}
	}

	list<Symbol*> remaining_sections;

	for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
	{
		if ((*iter)->is_section)
		{
			if (!(*iter)->size)
			{
				continue;
			}

			if (Loader::loaded_sections.find((*iter)->name) == Loader::loaded_sections.end())
			{
				remaining_sections.push_back(*iter);
			}
			else
			{
				continue;
			}
		}
	}

	for (list<Symbol*>::iterator iter = remaining_sections.begin(); iter != remaining_sections.end(); iter++)
	{
		if (Loader::next_address_to_write + (*iter)->size > Linker::_memory_mapped_registers)
		{
			string error = "Loading Error: section " + (*iter)->name + " overlaps with memory space reserved for memory mapped register.";

			throw Error(error);
		}

		(*iter)->value = Loader::next_address_to_write;

		Loader::next_address_to_write += (*iter)->size;

		Loader::loaded_sections.insert(make_pair((*iter)->name, (*iter)->value));
	}

	for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
	{
		if ((*iter)->is_section)
		{
			section_values.insert(make_pair((*iter)->name, (*iter)->value));
		}
	}

	uint8_t symbol_count = 0;

	for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end(); iter++)
	{
		if ((*iter)->is_symbol)
		{
			symbol_count++;
		}
	}

	for (list<Symbol*>::iterator iter = Linker::symbol_table.begin(); iter != Linker::symbol_table.end() && symbol_count; iter++)
	{
		if ((*iter)->is_symbol)
		{
			(*iter)->value += section_values[(*iter)->section];

			bool escape = false;

			for (list<ObjectFile*>::iterator it = Linker::object_files.begin(); it != Linker::object_files.end(); it++)
			{
				for (list<Symbol*>::iterator ite = (*it)->symbols.begin(); ite != (*it)->symbols.end(); ite++)
				{
					if ((*ite)->name == (*iter)->name && (*ite)->defined && !(*ite)->absolute)
					{
						bool end = false;

						for (const auto& x : Linker::section_offset)
						{
							auto temp = x.second;

							for (const auto& help : temp)
							{
								if (help.first == (*it)->input_file_name)
								{
									(*iter)->value += help.second;
									end = true;
									escape = true;
									break;
								}
							}

							if (end)
							{
								break;
							}
						}

						break;
					}
				}

				if (escape)
				{
					break;
				}
			}

			symbol_count--;
		}
	}

	if (symbol_count)
	{
		throw Error("Runtime Error: segmentation fault (core dumped).");
	}

	Loader::relocate();

	Loader::insert_program();
}

void Loader::insert_program()
{
	for (auto& sec : Linker::machine_code)
	{
		if (sec.second.empty())
		{
			continue;
		}

		cout << ".section " << sec.first << ":" << endl;

		size_t x = 0;

		static constexpr size_t _num = 104;

		for (size_t i = 0; i < sec.second.size(); i++)
		{
			if ((i - x) % _num == 0 && sec.second[i] == ' ')
			{
				sec.second[i] = '\n';
				x++;
			}
		}

		cout << sec.second << endl << endl;

		uint16_t loading_address = 0;

		for (const auto& sym : Linker::symbol_table)
		{
			if (sym->name == sec.first)
			{
				loading_address = sym->value;
				break;
			}
		}

		string code = sec.second;

		code.push_back('z'); // sentinel character to detect the end of the string

		while (true)
		{
			string temp = code.substr(0, 2);

			uint8_t store_value = (uint8_t) (strtol(temp.c_str(), nullptr, 16) & 0xFF);

			Loader::memory[loading_address++] = store_value;

			code.erase(0, 2);

			if (code[0] == 'z')
			{
				break;
			}
			else
			{
				code.erase(0, 1);
			}
		}
	}
}
