#ifndef _structs_h_
#define _structs_h_

#include <string>
#include <regex>

using namespace std;

typedef struct
{
	string symbol_1;
	string symbol_2;

	string addressing_1;
	string addressing_2;

	uint8_t reg_num_1 = 0;
	uint8_t reg_num_2 = 0;

	char reg_1_h_l;
	char reg_2_h_l;

	bool op1_bytes;
	bool op2_bytes;

	bool is_jmp;

	bool word; // word == true, byte == false

	bool push_or_pop = false;

} _instruction;

typedef struct
{
	regex pattern;

	uint8_t op_code;

	uint8_t num_of_operands;

} instruction_data;

typedef struct
{
	regex pattern;

	uint8_t op_code;

	bool has_operand;

} addressing_data;

#endif