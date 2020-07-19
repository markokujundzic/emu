#include "Lexer.h"
#include "Error.h"
#include "Assembler.h"

const string Lexer::sequences[5] =
{
	".end",
	"\n\t ,",
	"#",
	":",
	"\n\t "
};

void Lexer::change_section_content(uint16_t relocation_offset, string& section_content, const int8_t& word)
{
	relocation_offset *= 3;

	string temp = section_content.substr(relocation_offset, 2);

	int8_t res = (int8_t) (strtol(temp.c_str(), nullptr, 16) & 0xFF);
	res += word;

	temp = Lexer::to_hex(res);

	section_content[relocation_offset + 0] = temp[0];
	section_content[relocation_offset + 1] = temp[1];
}

void Lexer::change_section_content(uint16_t relocation_offset, string& section_content, const int16_t& word)
{
	relocation_offset *= 3;

	string temp_1 = section_content.substr(relocation_offset, 2);
	string temp_2 = section_content.substr(relocation_offset + 3, 2);

	int8_t higher = (int8_t) (strtol(temp_2.c_str(), nullptr, 16) & 0xFF);
	int8_t lower  = (int8_t) (strtol(temp_1.c_str(), nullptr, 16) & 0xFF);

	int16_t res = (higher << 8) | lower;
	res += word;

	string temp = Lexer::to_hex(res);

	section_content[relocation_offset + 0] = temp[0];
	section_content[relocation_offset + 1] = temp[1];
	section_content[relocation_offset + 3] = temp[3];
	section_content[relocation_offset + 4] = temp[4];
}

void Lexer::change_section_content(uint16_t relocation_offset, string& section_content, const uint8_t& word)
{
	relocation_offset *= 3;

	string temp = section_content.substr(relocation_offset, 2);

	int8_t res = (int8_t) (strtol(temp.c_str(), nullptr, 16) & 0xFF);
	res += word;

	temp = Lexer::to_hex(res);

	section_content[relocation_offset + 0] = temp[0];
	section_content[relocation_offset + 1] = temp[1];
}

void Lexer::change_section_content(uint16_t relocation_offset, string& section_content, const uint16_t& word)
{
	relocation_offset *= 3;

	string temp_1 = section_content.substr(relocation_offset, 2);
	string temp_2 = section_content.substr(relocation_offset + 3, 2);

	int8_t higher = (int8_t) (strtol(temp_2.c_str(), nullptr, 16) & 0xFF);
	int8_t lower  = (int8_t) (strtol(temp_1.c_str(), nullptr, 16) & 0xFF);

	int16_t res = (higher << 8) | lower;
	res += word;

	string temp = Lexer::to_hex(res);

	section_content[relocation_offset + 0] = temp[0];
	section_content[relocation_offset + 1] = temp[1];
	section_content[relocation_offset + 3] = temp[3];
	section_content[relocation_offset + 4] = temp[4];
}

uint16_t Lexer::determine_section_size(const string& section)
{
	uint16_t size = 0;

	for (const auto& it : section)
	{
		if (it != ' ' && it != '\n')
		{
			size++;
		}
	}

	return size / 2;
}

string Lexer::to_hex(int16_t word)
{
	stringstream _stream;

	int8_t higher_byte = (int8_t) ((word >> 8) & 0xFF);
	int8_t lower_byte  = (int8_t) (word & 0xFF);

	_stream << hex << ((lower_byte >> 4) & 0xF);
	_stream << hex << (lower_byte & 0xF);

	_stream << " ";

	_stream << hex << ((higher_byte >> 4) & 0xF);
	_stream << hex << (higher_byte & 0xF);

	string upper_stream = _stream.str();

	Lexer::to_upper(upper_stream);

	return upper_stream;
}

string Lexer::to_hex(int8_t byte)
{
	stringstream _stream;

	_stream << hex << ((byte >> 4) & 0xF);
	_stream << hex << (byte & 0xF);

	string upper_stream = _stream.str();

	Lexer::to_upper(upper_stream);

	return upper_stream;
}

string Lexer::to_hex(uint16_t word)
{
	stringstream _stream;

	uint8_t higher_byte = (uint8_t) ((word >> 8) & 0xFF);
	uint8_t lower_byte  = (uint8_t) (word & 0xFF);

	_stream << hex << ((lower_byte >> 4) & 0xF);
	_stream << hex << (lower_byte & 0xF);

	_stream << " ";

	_stream << hex << ((higher_byte >> 4) & 0xF);
	_stream << hex << (higher_byte & 0xF);

	string upper_stream = _stream.str();

	Lexer::to_upper(upper_stream);

	return upper_stream;
}

string Lexer::to_hex(uint8_t byte)
{
	stringstream _stream;

	_stream << hex << ((byte >> 4) & 0xF);
	_stream << hex << (byte & 0xF);

	string upper_stream = _stream.str();

	Lexer::to_upper(upper_stream);

	return upper_stream;
}

void Lexer::write_to_output(const string& output_filename)
{
	ofstream fout;

	fout.open(output_filename);

	if (fout.is_open())
	{
		const string empty_space = "                                    ";

		cout << empty_space << "/ *** Symbol Table *** /" << endl;
		fout << empty_space << "/ *** Symbol Table *** /" << endl;

		cout << left;
		cout << setw(10) << "NAME";
		cout << setw(10) << "SECTION";
		cout << setw(10) << "SCOPE";
		cout << setw(10) << "SECTION";
		cout << setw(10) << "EXTERN";
		cout << setw(10) << "ABSOLUTE";
		cout << setw(10) << "EQU";
		cout << setw(10) << "DEPEND.";
		cout << setw(10) << "VALUE";
		cout << setw(10) << "SIZE";
		cout << setw(10) << "ENTRY";
		cout << endl;

		fout << left;
		fout << setw(10) << "NAME";
		fout << setw(10) << "SECTION";
		fout << setw(10) << "SCOPE";
		fout << setw(10) << "SECTION";
		fout << setw(10) << "EXTERN";
		fout << setw(10) << "ABSOLUTE";
		fout << setw(10) << "EQU";
		fout << setw(10) << "DEPEND.";
		fout << setw(10) << "VALUE";
		fout << setw(10) << "SIZE";
		fout << setw(10) << "ENTRY";
		fout << endl;

		for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
		{
			cout << left;
			cout << setw(10) << (*it)->name;
			cout << setw(10) << (*it)->section;
			cout << setw(10) << (*it)->scope;
			cout << setw(10) << boolalpha << (*it)->is_section;
			cout << setw(10) << boolalpha << (*it)->is_extern;
			cout << setw(10) << boolalpha << (*it)->absolute;
			cout << setw(10) << boolalpha << (*it)->equ_relocatable;
			cout << setw(10) << boolalpha << (*it)->relative_relocatable_symbol;
			cout << setw(10) << uppercase << showbase << hex << (*it)->value;
			cout << setw(10) << uppercase << showbase << hex << (*it)->size;
			cout << setw(10) << noshowbase << dec << (*it)->number;
			cout << endl;

			fout << left;
			fout << setw(10) << (*it)->name;
			fout << setw(10) << (*it)->section;
			fout << setw(10) << (*it)->scope;
			fout << setw(10) << boolalpha << (*it)->is_section;
			fout << setw(10) << boolalpha << (*it)->is_extern;
			fout << setw(10) << boolalpha << (*it)->absolute;
			fout << setw(10) << boolalpha << (*it)->equ_relocatable;
			fout << setw(10) << boolalpha << (*it)->relative_relocatable_symbol;
			fout << setw(10) << uppercase << showbase << hex << (*it)->value;
			fout << setw(10) << uppercase << showbase << hex << (*it)->size;
			fout << setw(10) << noshowbase << dec << (*it)->number;
			fout << endl;
		}

		cout << endl;
		fout << endl;

		Assembler::section_names.pop_front(); // pop .und section
		Assembler::section_names.pop_front(); // pop .abs section

		while (Assembler::section_names.size())
		{
			string current = Assembler::section_names.front();
			Assembler::section_names.pop_front();

			cout << "#" << current << endl;
			fout << "#" << current << endl;

			string section = "";

			for (list<Data*>::iterator it = Assembler::data.begin(); it != Assembler::data.end(); it++)
			{
				if ((*it)->section_name == current)
				{
					section += (*it)->machine_code;
					section += " ";
				}
			}

			size_t x = 0;

			static constexpr size_t _num = 104;

			for (size_t i = 0; i < section.size(); i++)
			{
				if ((i - x) % _num == 0 && section[i] == ' ')
				{
					section[i] = '\n';
					x++;
				}
			}

			if (section.size() % (_num + 1) != 0)
			{
				section += '\n';
			}

			cout << section << endl;
			fout << section << endl;

			cout << "#.ret " << current << endl;
			fout << "#.ret " << current << endl;

			cout << left;
			cout << setw(15) << "OFFSET";
			cout << setw(15) << "TYPE";
			cout << setw(15) << "VALUE";
			cout << endl;

			fout << left;
			fout << setw(15) << "OFFSET";
			fout << setw(15) << "TYPE";
			fout << setw(15) << "VALUE";
			fout << endl;

			for (list<Data*>::iterator it = Assembler::data.begin(); it != Assembler::data.end(); it++)
			{
				if ((*it)->section_name == current)
				{
					for (list<Relocation*>::iterator ite = (*it)->relocations.begin(); ite != (*it)->relocations.end(); ite++)
					{
						cout << left;
						cout << setw(15) << uppercase << showbase << hex << (*ite)->offset;
						cout << setw(15) << (*ite)->type;
						cout << setw(15) << noshowbase << dec << (*ite)->value;
						cout << endl;

						fout << left;
						fout << setw(15) << uppercase << showbase << hex << (*ite)->offset;
						fout << setw(15) << (*ite)->type;
						fout << setw(15) << noshowbase << dec << (*ite)->value;
						fout << endl;
					}
				}
			}

			cout << endl;
			fout << endl;
		}

		fout.close();
	}
	else
	{
		throw Error("Error: cannot open output file.");
	}
}

void Lexer::tokenize_input_file(const string& filename)
{
	ifstream input_file;

	input_file.open(filename);

	bool end_flag = false;

	Assembler::input = filename;
	Assembler::input += '\n';

	if (input_file.is_open())
	{
		string line;

		vector<vector<string>> return_vector;

		while (input_file.good())
		{
			getline(input_file, line);

			Assembler::input += line;
			Assembler::input += '\n';

			vector<string> helper;

			deque<string> intermediate;

			size_t startOfComment = line.find(sequences[Lexer::_comment]);

			if (startOfComment != string::npos)
			{
				line.erase(startOfComment);
			}

			size_t firstToken = line.find_first_not_of(sequences[Lexer::_escape], 0);

			while (firstToken != string::npos)
			{
				size_t lastToken = line.find_first_of(sequences[Lexer::_escape], firstToken);

				intermediate.push_back(line.substr(firstToken, lastToken - firstToken));

				firstToken = line.find_first_not_of(sequences[Lexer::_escape], lastToken);
			}

			if (intermediate.size())
			{
				if (!intermediate.front().compare(sequences[Lexer::_end]))
				{
					end_flag = true;
					break;
				}

				for (size_t i = 0; i < intermediate.size(); i++)
				{
					helper.push_back(intermediate[i]);
				}

				return_vector.push_back(helper);
			}
		}

		if (!end_flag)
		{
			vector<string> end_vector;

			end_vector.push_back(Lexer::sequences[Lexer::_end]);

			return_vector.push_back(end_vector);
		}

		input_file.close();

		Assembler::tokens = return_vector;
	}
	else
	{
		throw Error("ERROR: Cannot open input file.");
	}
}

void Lexer::to_lower(string& convert)
{
	for_each(convert.begin(), convert.end(), [](char& c)
		{
			c = tolower(c);
		}
	);
}

void Lexer::to_upper(string& convert)
{
	for_each(convert.begin(), convert.end(), [](char& c)
		{
			c = toupper(c);
		}
	);
}