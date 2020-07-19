#include "ObjectFile.h"

ObjectFile::ObjectFile(const string& input_file_name) : input_file_name(input_file_name)
{

}

ObjectFile::~ObjectFile()
{
	for (list<Symbol*>::iterator it = symbols.begin(); it != symbols.end(); it++)
	{
		delete *it;
	}

	symbols.clear();

	for (list<Symbol*>::iterator it = sections.begin(); it != sections.end(); it++)
	{
		delete *it;
	}

	sections.clear();

	unordered_map<entry_section, list<Relocation*>>::iterator it = relocations.begin();

	while (it != relocations.end())
	{
		for (list<Relocation*>::iterator itr = it->second.begin(); itr != it->second.end(); itr++)
		{
			delete *itr;
		}

		it++;
	}

	relocations.clear();
}

void ObjectFile::extract()
{
	ifstream input_file;

	input_file.open(input_file_name);

	if (input_file.is_open())
	{
		string current_line;

		getline(input_file, current_line); // remove / *** Symbol Table *** /
		getline(input_file, current_line); // remove headers
		getline(input_file, current_line); // remove .und section
		getline(input_file, current_line); // remove .abs section
		getline(input_file, current_line);

		if (current_line.empty())
		{
			string error = "Error: object file contains an empty symbol table -> file: " + input_file_name;

			throw Error(error);
		}

		while (!current_line.empty())
		{
			// name

			size_t first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], 0);
			size_t last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string name = current_line.substr(first_element, last_element - first_element);

			// section

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string section = current_line.substr(first_element, last_element - first_element);

			// scope

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string scope = current_line.substr(first_element, last_element - first_element);

			// section

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string temp = current_line.substr(first_element, last_element - first_element);

			bool is_section;

			if (temp == "true")
			{
				is_section = true;

				section_names.push_back(name);
			}
			else
			{
				is_section = false;
			}

			// check

			if (scope == "LOCAL" && !is_section)
			{
				getline(input_file, current_line);

				continue;
			}

			// extern

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			temp = current_line.substr(first_element, last_element - first_element);

			bool is_extern;

			if (temp == "true")
			{
				is_extern = true;
			}
			else
			{
				is_extern = false;
			}

			// absolute

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			temp = current_line.substr(first_element, last_element - first_element);

			bool absolute;

			if (temp == "true")
			{
				absolute = true;
			}
			else
			{
				absolute = false;
			}

			// equ

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			temp = current_line.substr(first_element, last_element - first_element);

			bool equ;

			if (temp == "true")
			{
				equ = true;
			}
			else
			{
				equ = false;
			}

			// depend.

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string depend = current_line.substr(first_element, last_element - first_element);

			// value

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string val = current_line.substr(first_element, last_element - first_element);

			uint16_t value = (uint16_t) (strtol(val.c_str(), nullptr, 0) & 0xFFFF);

			// size

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string siz = current_line.substr(first_element, last_element - first_element);

			uint16_t size = (uint16_t) (strtol(siz.c_str(), nullptr, 0) & 0xFFFF);

			// entry

			first_element = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], last_element);
			last_element = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], first_element);

			string num = current_line.substr(first_element, last_element - first_element);

			int16_t number = (int16_t) (strtol(num.c_str(), nullptr, 0) & 0xFFFF);

			Symbol *_symbol = new Symbol(name, section, value);

			_symbol->scope = scope;
			_symbol->is_section = is_section;
			_symbol->is_extern = is_extern;
			_symbol->absolute = absolute;
			_symbol->equ_relocatable = equ;
			_symbol->relative_relocatable_symbol = depend;
			_symbol->size = size;
			_symbol->number = number;

			if (is_section)
			{
				sections.push_back(_symbol);
			}
			else
			{
				symbols.push_back(_symbol);
			}

			getline(input_file, current_line);
		}

		for (const auto& it : section_names)
		{
			machine_code.insert(make_pair(it, ""));
		}

		while (section_names.size())
		{
			string current_section_name = section_names.front();
			section_names.pop_front();

			getline(input_file, current_line); // remove #<section_name>
			getline(input_file, current_line);

			while (!current_line.empty())
			{
				machine_code[current_section_name] += current_line;
				machine_code[current_section_name] += " ";

				getline(input_file, current_line);
			}

			while (machine_code[current_section_name].back() == ' ')
			{
				machine_code[current_section_name].pop_back();
			}

			while (machine_code[current_section_name].size() && machine_code[current_section_name].back() == ' ')
			{
				machine_code[current_section_name].pop_back();
			}

			getline(input_file, current_line); // remove #.ret <section_name>
			getline(input_file, current_line); // remove headers
			getline(input_file, current_line);

			while (!current_line.empty())
			{
				// offset

				size_t start = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], 0);
				size_t end = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], start);

				string temp = current_line.substr(start, end - start);

				uint16_t offset = (uint16_t) (strtol(temp.c_str(), nullptr, 0) & 0xFFFF);

				// type

				start = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], end);
				end = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], start);

				string type = current_line.substr(start, end - start);

				// value

				start = current_line.find_first_not_of(Lexer::sequences[Lexer::_object_file], end);
				end = current_line.find_first_of(Lexer::sequences[Lexer::_object_file], start);

				temp = current_line.substr(start, end - start);

				uint16_t value = (uint16_t) (strtol(temp.c_str(), nullptr, 0) & 0xFFFF);

				Relocation *_relocation = new Relocation(offset, value, type, current_section_name);

				if (relocations.find(current_section_name) == relocations.end())
				{
					list<Relocation*> temp = { };

					relocations.insert(make_pair(current_section_name, temp));
				}

				relocations[current_section_name].push_back(_relocation);

				getline(input_file, current_line);
			}
		}

		input_file.close();
	}
	else
	{
		throw Error("Error: cannot open input file.");
	}
}