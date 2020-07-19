#ifndef _lexer_h_
#define _lexer_h_

#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>
#include <sstream>
#include <regex>
#include <deque>

using namespace std;

class Lexer
{
public:
	static constexpr int8_t _end         = 0;
	static constexpr int8_t _escape      = 1;
	static constexpr int8_t _comment     = 2;
	static constexpr int8_t _colon       = 3;
	static constexpr int8_t _object_file = 4;

	static void tokenize_input_file(const string& filename);
	static void write_to_output(const string& output_filename);

	static uint16_t determine_section_size(const string& section);

	static string to_hex(int16_t word);
	static string to_hex(int8_t byte);

	static string to_hex(uint16_t word);
	static string to_hex(uint8_t byte);

	static void change_section_content(uint16_t relocation_offset, string& section_content, const int16_t& word);
	static void change_section_content(uint16_t relocation_offset, string& section_content, const int8_t& word);

	static void change_section_content(uint16_t relocation_offset, string& section_content, const uint16_t& word);
	static void change_section_content(uint16_t relocation_offset, string& section_content, const uint8_t& word);

	static void to_lower(string& convert);
	static void to_upper(string& convert);

	static const string sequences[5];
};

#endif