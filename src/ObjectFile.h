#ifndef _object_file_h_
#define _object_file_h_

#include "Relocation.h"
#include "Symbol.h"
#include "Error.h"
#include "Lexer.h"

#include <string>
#include <list>
#include <deque>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <regex>

using namespace std;

typedef string entry_section;

class ObjectFile
{
public:
	friend class Linker;
	friend class Loader;

	ObjectFile(const string& input_file_name);
	~ObjectFile();

	void extract();

	deque<string> section_names;

	list<Symbol*> symbols;
	list<Symbol*> sections;

	unordered_map<entry_section, string> machine_code;
	unordered_map<entry_section, list<Relocation*>> relocations;
private:
	string input_file_name;
};

#endif