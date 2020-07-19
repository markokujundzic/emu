#ifndef _parser_h_
#define _parser_h_

#include "Structs.h"

#include <unordered_map>
#include <string>
#include <regex>

using namespace std;

class Parser
{
public:
	static unordered_map<string, regex> type;
	static unordered_map<string, regex> directives;
	static unordered_map<string, regex> patching;
	static unordered_map<string, regex> instruction_type;
	static unordered_map<string, addressing_data> addressing_types_not_jmp_symbol;
	static unordered_map<string, addressing_data> addressing_types_not_jmp_literal;
	static unordered_map<string, addressing_data> addressing_types_jmp_symbol;
	static unordered_map<string, addressing_data> addressing_types_jmp_literal;
	static unordered_map<string, instruction_data> instructions;

	static string resolve_type(const string& data_type);
	static string resolve_directive(const string& directive_name);
	static string resolve_patch(const string& name);
	static string resolve_addressing_not_jmp_literal(const string& name);
	static string resolve_addressing_not_jmp_symbol(const string& name);
	static string resolve_addressing_jmp_literal(const string& name);
	static string resolve_addressing_jmp_symbol(const string& name);
	static string resolve_instruction(const string& mnemonic);
	static string resolve_instruction_type(const string& mnemonic);
};

#endif

