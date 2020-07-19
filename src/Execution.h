#ifndef _execution_h_
#define _execution_h_

#include "Loader.h"
#include "Parser.h"
#include "Interrupt.h"

#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <deque>
#include <mutex>
#include <memory>

using namespace std;

typedef uint16_t memory_size;

class Execution
{
public:
	static constexpr uint8_t NUM_OF_REGS = 6;

	static constexpr uint16_t Z_MASK  = (0x1 << 0x0);
	static constexpr uint16_t O_MASK  = (0x1 << 0x1);
	static constexpr uint16_t C_MASK  = (0x1 << 0x2);
	static constexpr uint16_t N_MASK  = (0x1 << 0x3);
	static constexpr uint16_t TR_MASK = (0x1 << 0xD);
	static constexpr uint16_t TL_MASK = (0x1 << 0xE);
	static constexpr uint16_t I_MASK  = (0x1 << 0xF);

	static constexpr uint8_t IVT_ENTRY_0 = 0x0;
	static constexpr uint8_t IVT_ENTRY_1 = 0x2;
	static constexpr uint8_t IVT_ENTRY_2 = 0x4;
	static constexpr uint8_t IVT_ENTRY_3 = 0x6;
	static constexpr uint8_t IVT_ENTRY_4 = 0x8;
	static constexpr uint8_t IVT_ENTRY_5 = 0xA;
	static constexpr uint8_t IVT_ENTRY_6 = 0xC;
	static constexpr uint8_t IVT_ENTRY_7 = 0xE;

	static constexpr uint16_t timer_cfg = 0xFF10;

	static constexpr uint16_t data_out  = 0xFF00;
	static constexpr uint16_t data_in   = 0xFF02;

	static constexpr bool execution = true;

	static mutex condition_mutex;
	static mutex memory_mutex;
	static mutex interrupt_mutex;

	static int16_t registers[NUM_OF_REGS];

	static uint16_t psw;
	static uint16_t pc;
	static uint16_t sp;

	static vector<unique_ptr<thread>> threads;

	static deque<unique_ptr<Interrupt>> interrupts;

	static uint8_t (&mem)[Loader::MEMORY_SIZE];

	static atomic<bool> instruction_error;

	static atomic<bool> halt;

	static atomic<bool> data_out_write;

	static uint16_t first_operand_value_word;
	static uint16_t second_operand_value_word;

	static int8_t first_operand_value_byte;
	static int8_t second_operand_value_byte;

	static int8_t first_operand_register_number;
	static int8_t second_operand_register_number;

	static bool first_operand_register_high;
	static bool second_operand_register_high;

	static string current_instruction;

	static string first_operand_addressing;
	static string second_operand_addressing;

	static uint8_t num_of_operands;

	static uint8_t operand_size;

	static string interrupt_type;

	static uint16_t read_word_from_memory(const memory_size& address);

	static void push_byte(const int8_t& byte);
	static void push_word(const int16_t& word);

	static int8_t  pop_byte();
	static int16_t pop_word();

	static void disable_interrupts();

	static bool interrupts_enabled();

	static bool terminal_interrupts_enabled();

	static bool timer_interrupts_enabled();

	static void decode_instruction();

	static void initialize();

	static void execute_instruction();

	static void resolve_interrupts();

	static void set_in_motion();

	static void reset();
};

#endif