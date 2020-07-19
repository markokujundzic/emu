#include "Execution.h"

constexpr uint8_t Execution::NUM_OF_REGS;

constexpr uint16_t Execution::Z_MASK;

constexpr uint16_t Execution::O_MASK;

constexpr uint16_t Execution::C_MASK;

constexpr uint16_t Execution::N_MASK;

constexpr uint16_t Execution::TR_MASK;

constexpr uint16_t Execution::TL_MASK;

constexpr uint16_t Execution::I_MASK;

constexpr uint8_t Execution::IVT_ENTRY_0;

constexpr uint8_t Execution::IVT_ENTRY_1;

constexpr uint8_t Execution::IVT_ENTRY_2;

constexpr uint8_t Execution::IVT_ENTRY_3;

constexpr uint8_t Execution::IVT_ENTRY_4;

constexpr uint8_t Execution::IVT_ENTRY_5;

constexpr uint8_t Execution::IVT_ENTRY_6;

constexpr uint8_t Execution::IVT_ENTRY_7;

constexpr uint16_t Execution::timer_cfg;

constexpr uint16_t Execution::data_out;

constexpr uint16_t Execution::data_in;

constexpr bool Execution::execution;

int16_t Execution::registers[Execution::NUM_OF_REGS];

uint16_t Execution::psw;

uint16_t Execution::pc;

uint16_t Execution::sp;

uint8_t Execution::operand_size;

uint16_t Execution::first_operand_value_word;

uint16_t Execution::second_operand_value_word;

int8_t Execution::first_operand_value_byte;

int8_t Execution::second_operand_value_byte;

int8_t Execution::first_operand_register_number;

int8_t Execution::second_operand_register_number;

bool Execution::first_operand_register_high;

bool Execution::second_operand_register_high;

string Execution::first_operand_addressing;

string Execution::second_operand_addressing;

string Execution::current_instruction;

string Execution::interrupt_type = "";

vector<unique_ptr<thread>> Execution::threads;

deque<unique_ptr<Interrupt>> Execution::interrupts;

atomic<bool> Execution::halt;

atomic<bool> Execution::instruction_error;

atomic<bool> Execution::data_out_write;

mutex Execution::memory_mutex;

mutex Execution::interrupt_mutex;

mutex Execution::condition_mutex;

uint8_t Execution::num_of_operands;

uint8_t(&Execution::mem)[Loader::MEMORY_SIZE] = Loader::memory;

inline bool Execution::interrupts_enabled()
{
	return ((Execution::psw >> 0xF) == 0x0);
}

inline bool Execution::timer_interrupts_enabled()
{
	return ((Execution::psw >> 0xD) == 0x0);
}

inline bool Execution::terminal_interrupts_enabled()
{
	return ((Execution::psw >> 0xE) == 0x0);
}

inline void Execution::disable_interrupts()
{
	Execution::psw |= Execution::I_MASK;
}

void Execution::resolve_interrupts()
{
	if (Execution::data_out_write)
	{
		Execution::memory_mutex.lock();

		char key_pressed = Execution::mem[Execution::data_out];

		cout << key_pressed << flush;

		Execution::memory_mutex.unlock();

		Execution::data_out_write = false;
	}

	if (Execution::interrupts_enabled())
	{
		Execution::interrupt_mutex.lock();

		if (!Execution::interrupts.empty())
		{
			Execution::interrupt_type = Execution::interrupts.front()->type;
			Execution::interrupts.pop_front();
		}
		else
		{
			Execution::interrupt_mutex.unlock();
			return;
		}

		Execution::interrupt_mutex.unlock();

		if (Execution::interrupt_type == Interrupt::TIMER_INTERRUPT)
		{
			if (Execution::timer_interrupts_enabled())
			{
				Execution::push_word(Execution::pc);
				Execution::push_word(Execution::psw);

				Execution::memory_mutex.lock();

				Execution::pc = Execution::read_word_from_memory(Execution::IVT_ENTRY_2);

				Execution::memory_mutex.unlock();
			}
			else
			{
				return;
			}
		}
		else if (Execution::interrupt_type == Interrupt::TERMINAL_INTERRUPT)
		{
			if (Execution::terminal_interrupts_enabled())
			{
				Interrupt::terminal_interrupt_to_handle = false;

				Execution::push_word(Execution::pc);
				Execution::push_word(Execution::psw);

				Execution::memory_mutex.lock();

				Execution::pc = Execution::read_word_from_memory(Execution::IVT_ENTRY_3);

				Execution::memory_mutex.unlock();
			}
			else
			{
				return;
			}
		}

		Execution::disable_interrupts();

		return;
	}
	else
	{
		return;
	}
}

uint16_t Execution::read_word_from_memory(const memory_size& address)
{
	uint8_t higher_byte = Execution::mem[address + 1];
	uint8_t lower_byte = Execution::mem[address + 0];

	return (higher_byte << 8) | lower_byte;
}

void Execution::push_byte(const int8_t& byte)
{
	Execution::memory_mutex.lock();

	if (--Execution::sp < Linker::_stack_end_address)
	{
		Execution::memory_mutex.unlock();

		throw Error("Error: Stack pointer value has exceeded the maximum value -> Stack Overflow.");
	}

	uint8_t _byte = byte;

	Execution::mem[Execution::sp] = _byte;

	Execution::memory_mutex.unlock();
}

void Execution::push_word(const int16_t& word)
{
	Execution::memory_mutex.lock();

	if (Execution::sp - 2 < Linker::_stack_end_address)
	{
		Execution::memory_mutex.unlock();

		throw Error("Error: Stack pointer value has exceeded the maximum value -> Stack Overflow.");
	}

	uint16_t copy = word;

	uint8_t lower_byte = (uint8_t) (copy & 0xFF);
	uint8_t higher_byte = (uint8_t) ((copy >> 8) & 0xFF);

	Execution::mem[--Execution::sp] = higher_byte;
	Execution::mem[--Execution::sp] = lower_byte;

	Execution::memory_mutex.unlock();
}

int8_t Execution::pop_byte()
{
	if (Execution::sp++ > Linker::_stack_start_address)
	{
		Execution::memory_mutex.unlock();

		throw Error("Error: Stack pointer value has exceeded the minimum value -> Stack Underflow.");
	}

	return (int8_t) Execution::mem[Execution::sp];
}

int16_t Execution::pop_word()
{
	if (Execution::sp + 2 > Linker::_stack_start_address)
	{
		Execution::memory_mutex.unlock();

		throw Error("Error: Stack pointer value has exceeded the minimum value -> Stack Underflow.");
	}

	uint8_t lower_byte = (uint8_t) (Execution::mem[Execution::sp++] & 0xFF);
	uint8_t higher_byte = (uint8_t) (Execution::mem[Execution::sp++] & 0xFF);

	return (int16_t) (higher_byte << 8 | lower_byte);
}

inline void Execution::reset()
{
	Execution::operand_size = Execution::num_of_operands = 0;

	Execution::first_operand_value_byte = Execution::second_operand_value_byte = 0;

	Execution::first_operand_value_word = Execution::second_operand_value_word = 0;

	Execution::first_operand_register_number = Execution::second_operand_register_number = 0;

	Execution::first_operand_register_high = Execution::second_operand_register_high = false;

	Execution::current_instruction = Execution::first_operand_addressing = Execution::second_operand_addressing = "";
}

void Execution::initialize()
{
	for (auto& it : Execution::registers)
	{
		it = 0;
	}

	Execution::sp = Linker::_stack_start_address;

	Execution::psw = 0x0000;

	Execution::halt = Interrupt::terminal_end = Interrupt::timer_end = Interrupt::terminal_interrupt_to_handle = Execution::instruction_error = Execution::data_out_write = false;

	Execution::mem[Execution::timer_cfg + 0] = 0x00;
	Execution::mem[Execution::timer_cfg + 1] = 0x00;

	Execution::mem[Execution::data_in + 0] = 0x00;
	Execution::mem[Execution::data_in + 1] = 0x00;

	Execution::mem[Execution::data_out + 0] = 0x00;
	Execution::mem[Execution::data_out + 1] = 0x00;

	Execution::threads.push_back(make_unique<thread>(thread(Interrupt::timer_routine)));
	Execution::threads.push_back(make_unique<thread>(thread(Interrupt::terminal_routine)));

	Execution::memory_mutex.lock();

	Execution::pc = Execution::read_word_from_memory(Execution::IVT_ENTRY_0);

	Execution::memory_mutex.unlock();
}

void Execution::decode_instruction()
{
	uint8_t first_byte = Execution::mem[Execution::pc++];

	first_byte = first_byte >> 2;

	if (first_byte & 0x1)
	{
		Execution::operand_size = 2;
	}
	else
	{
		Execution::operand_size = 1;
	}

	first_byte = first_byte >> 1;

	bool matched = false;

	for (const auto& it : Parser::instructions)
	{
		if (it.second.op_code == first_byte)
		{
			Execution::current_instruction = it.first;
			Execution::num_of_operands = it.second.num_of_operands;
			matched = true;
			break;
		}
	}

	if (!matched)
	{
		Execution::instruction_error = true;

		Execution::memory_mutex.lock();

		Execution::pc = Execution::read_word_from_memory(Execution::IVT_ENTRY_1);

		Execution::memory_mutex.unlock();

		return;
	}

	if (Execution::num_of_operands)
	{
		uint8_t second_byte = Execution::mem[Execution::pc++];

		if (second_byte & 0x1)
		{
			Execution::first_operand_register_high = true;
		}

		second_byte = second_byte >> 1;

		Execution::first_operand_register_number = (second_byte & 0xF);

		second_byte = second_byte >> 4;

		switch (second_byte & 0x7)
		{
			case 0x0:
				Execution::first_operand_addressing = "IMMEDIATE";
				break;
			case 0x1:
				Execution::first_operand_addressing = "REG_DIR";
				break;
			case 0x2:
				Execution::first_operand_addressing = "REG_IND";
				break;
			case 0x3:
				Execution::first_operand_addressing = "REG_IND_DISPL";
				break;
			case 0x4:
				Execution::first_operand_addressing = "MEMORY";
				break;
		}

		if (Execution::operand_size == 1)
		{
			if (Execution::first_operand_addressing == "IMMEDIATE")
			{
				Execution::first_operand_value_byte = Execution::mem[Execution::pc++];
			}
			else if (Execution::first_operand_addressing == "REG_IND_DISPL" || Execution::first_operand_addressing == "MEMORY")
			{
				uint8_t third_byte = Execution::mem[Execution::pc++];
				uint8_t fourth_byte = Execution::mem[Execution::pc++];

				Execution::first_operand_value_word = (uint16_t) (fourth_byte << 8 | third_byte);
			}
		}
		else
		{
			if (Execution::first_operand_addressing == "REG_IND_DISPL" || Execution::first_operand_addressing == "MEMORY" || Execution::first_operand_addressing == "IMMEDIATE")
			{
				uint8_t third_byte = Execution::mem[Execution::pc++];
				uint8_t fourth_byte = Execution::mem[Execution::pc++];

				Execution::first_operand_value_word = (int16_t) (fourth_byte << 8 | third_byte);
			}
		}
	}

	if (Execution::num_of_operands == 2)
	{
		uint8_t fifth_byte = Execution::mem[Execution::pc++];

		if (fifth_byte & 0x1)
		{
			Execution::second_operand_register_high = true;
		}

		fifth_byte = fifth_byte >> 1;

		Execution::second_operand_register_number = (fifth_byte & 0xF);

		fifth_byte = fifth_byte >> 4;

		switch (fifth_byte & 0x7)
		{
			case 0x0:
				Execution::second_operand_addressing = "IMMEDIATE";
				break;
			case 0x1:
				Execution::second_operand_addressing = "REG_DIR";
				break;
			case 0x2:
				Execution::second_operand_addressing = "REG_IND";
				break;
			case 0x3:
				Execution::second_operand_addressing = "REG_IND_DISPL";
				break;
			case 0x4:
				Execution::second_operand_addressing = "MEMORY";
				break;
		}

		if (Execution::second_operand_addressing == "REG_IND_DISPL" || Execution::second_operand_addressing == "MEMORY" || Execution::second_operand_addressing == "IMMEDIATE")
		{
			uint8_t sixth_byte = Execution::mem[Execution::pc++];
			uint8_t seventh_byte = Execution::mem[Execution::pc++];

			Execution::second_operand_value_word = (uint16_t) (seventh_byte << 8 | sixth_byte);
		}
	}

	if (first_operand_addressing == "IMMEDIATE" && current_instruction == "DIV" && ((first_operand_value_word == 0 && operand_size == 2) || (first_operand_value_byte == 0 && operand_size == 1)))
	{
		Execution::instruction_error = true;

		Execution::memory_mutex.lock();

		Execution::pc = Execution::read_word_from_memory(Execution::IVT_ENTRY_1);

		Execution::memory_mutex.unlock();

		return;
	}
}

void Execution::execute_instruction()
{
	int16_t pc_rel_address = -1;

	if (Execution::current_instruction == "HALT")
	{
		Execution::halt = true;

		return;
	}
	else if (Execution::current_instruction == "IRET")
	{
		Execution::memory_mutex.lock();

		Execution::psw = Execution::pop_word();
		Execution::pc = Execution::pop_word();

		Execution::memory_mutex.unlock();

		return;
	}
	else if (Execution::current_instruction == "RET")
	{
		Execution::memory_mutex.lock();

		Execution::pc = Execution::pop_word();

		Execution::memory_mutex.unlock();

		return;
	}

	if (Execution::num_of_operands == 1)
	{
		int16_t destination_operand_word = 0;
		int8_t  destination_operand_byte = 0;

		if (Execution::first_operand_addressing == "IMMEDIATE")
		{
			if (Execution::operand_size == 1)
			{
				destination_operand_byte = Execution::first_operand_value_byte;
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::first_operand_value_word;
			}
		}
		else if (Execution::first_operand_addressing == "REG_DIR")
		{
			if (Execution::operand_size == 1)
			{
				if (Execution::first_operand_register_high)
				{
					if (Execution::first_operand_register_number == 6)
					{
						destination_operand_byte = (int8_t) ((Execution::sp >> 8) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 7)
					{
						destination_operand_byte = (int8_t) ((Execution::pc >> 8) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 15)
					{
						destination_operand_byte = (int8_t) ((Execution::psw >> 8) & 0xFF);
					}
					else
					{
						destination_operand_byte = (int8_t) ((Execution::registers[Execution::first_operand_register_number] >> 8) & 0xFF);
					}
				}
				else
				{
					if (Execution::first_operand_register_number == 6)
					{
						destination_operand_byte = (int8_t) ((Execution::sp) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 7)
					{
						destination_operand_byte = (int8_t) ((Execution::pc) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 15)
					{
						destination_operand_byte = (int8_t) ((Execution::psw) & 0xFF);
					}
					else
					{
						destination_operand_byte = (int8_t) ((Execution::registers[Execution::first_operand_register_number]) & 0xFF);
					}
				}
			}
			else if (Execution::operand_size == 2)
			{
				if (Execution::first_operand_register_number == 6)
				{
					destination_operand_word = Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					destination_operand_word = Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					destination_operand_word = Execution::psw;
				}
				else
				{
					destination_operand_word = Execution::registers[Execution::first_operand_register_number];
				}
			}
		}
		else if (Execution::first_operand_addressing == "REG_IND")
		{
			uint16_t address = 0;

			if (Execution::first_operand_register_number == 6)
			{
				address = Execution::sp;
			}
			else if (Execution::first_operand_register_number == 7)
			{
				address = Execution::pc;
			}
			else if (Execution::first_operand_register_number == 15)
			{
				address = Execution::psw;
			}
			else
			{
				address = Execution::registers[Execution::first_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[address + 1] << 8 | Execution::mem[address + 0];
			}
		}
		else if (Execution::first_operand_addressing == "REG_IND_DISPL")
		{
			uint16_t address = Execution::first_operand_value_word;

			if (Execution::first_operand_register_number == 6)
			{
				address += Execution::sp;
			}
			else if (Execution::first_operand_register_number == 7)
			{
				address += Execution::pc;
				pc_rel_address = address;
			}
			else if (Execution::first_operand_register_number == 15)
			{
				address += Execution::psw;
			}
			else
			{
				address += Execution::registers[Execution::first_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[address + 1] << 8 | Execution::mem[address + 0];
			}
		}
		else if (Execution::first_operand_addressing == "MEMORY")
		{
			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[Execution::first_operand_value_word] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[first_operand_value_word + 1] << 8 | Execution::mem[first_operand_value_word + 0];
			}
		}

		if (Execution::current_instruction == "PUSH")
		{
			if (Execution::operand_size == 1)
			{
				Execution::push_byte(destination_operand_byte);
				return;
			}
			else
			{
				Execution::push_word(destination_operand_word);
				return;
			}
		}
		else if (Execution::current_instruction == "POP")
		{
			Execution::memory_mutex.lock();

			if (Execution::first_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::first_operand_register_high)
					{
						if (Execution::first_operand_register_number == 6)
						{
							Execution::sp = (int16_t) (Execution::pop_byte() << 8 | 0x00);
						}
						else if (Execution::first_operand_register_number == 7)
						{
							Execution::pc = (int16_t) (Execution::pop_byte() << 8 | 0x00);
						}
						else if (Execution::first_operand_register_number == 15)
						{
							Execution::psw = (int16_t) (Execution::pop_byte() << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::first_operand_register_number] = (int16_t) (Execution::pop_byte() << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::first_operand_register_number == 6)
						{
							Execution::sp = (int16_t) (Execution::pop_byte());
						}
						else if (Execution::first_operand_register_number == 7)
						{
							Execution::pc = (int16_t) (Execution::pop_byte());
						}
						else if (Execution::first_operand_register_number == 15)
						{
							Execution::psw = (int16_t) (Execution::pop_byte());
						}
						else
						{
							Execution::registers[Execution::first_operand_register_number] = (int16_t) (Execution::pop_byte());
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::first_operand_register_number == 6)
					{
						Execution::sp = (int16_t) (Execution::pop_word());
					}
					else if (Execution::first_operand_register_number == 7)
					{
						Execution::pc = (int16_t) (Execution::pop_word());
					}
					else if (Execution::first_operand_register_number == 15)
					{
						Execution::psw = (int16_t) (Execution::pop_word());
					}
					else
					{
						Execution::registers[Execution::first_operand_register_number] = (int16_t) (Execution::pop_word());
					}
				}
			}
			else if (Execution::first_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::first_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::first_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = Execution::pop_byte();
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = Execution::pop_byte();
					Execution::mem[address + 1] = Execution::pop_byte();
				}
			}
			else if (Execution::first_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = first_operand_value_word;

				if (Execution::first_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::first_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = (int16_t) (Execution::pop_byte());
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = Execution::pop_byte();
					Execution::mem[address + 1] = Execution::pop_byte();
				}
			}
			else if (Execution::first_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::first_operand_value_word] = Execution::pop_byte();
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[Execution::first_operand_value_word + 0] = Execution::pop_byte();
					Execution::mem[Execution::first_operand_value_word + 1] = Execution::pop_byte();

				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "INT")
		{
			Execution::push_word(Execution::pc);
			Execution::push_word(Execution::psw);

			Execution::memory_mutex.lock();

			uint16_t destination = 0;

			if (Execution::operand_size == 1)
			{
				destination = (uint16_t) (destination_operand_byte);
			}
			else
			{
				destination = (uint16_t) destination_operand_word;
			}

			Execution::pc = (Execution::mem[(destination % 8) * 2 + 1] << 8) | Execution::mem[(destination % 8) * 2 + 0];

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "CALL")
		{
			Execution::push_word(Execution::pc);

			Execution::memory_mutex.lock();

			if (Execution::operand_size == 1)
			{
				Execution::pc = (uint16_t) (destination_operand_byte);
			}
			else
			{
				Execution::pc = (uint16_t) destination_operand_word;
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "JMP")
		{
			Execution::memory_mutex.lock();

			if (Execution::operand_size == 1)
			{
				if (pc_rel_address != -1)
				{
					Execution::pc = (uint16_t) pc_rel_address;
					pc_rel_address = -1;
				}
				else
				{
					Execution::pc = (uint16_t) destination_operand_byte;
				}
			}
			else
			{
				if (pc_rel_address != -1)
				{
					Execution::pc = (uint16_t) pc_rel_address;
					pc_rel_address = -1;
				}
				else
				{
					Execution::pc = (uint16_t) destination_operand_word;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "JEQ")
		{
			if ((Execution::psw & Execution::Z_MASK) == Execution::Z_MASK)
			{
				Execution::memory_mutex.lock();

				if (Execution::operand_size == 1)
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_byte;
					}
				}
				else
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_word;
					}
				}

				Execution::memory_mutex.unlock();
			}

			return;
		}
		else if (Execution::current_instruction == "JNE")
		{
			if ((Execution::psw & Execution::Z_MASK) != Execution::Z_MASK)
			{
				Execution::memory_mutex.lock();

				if (Execution::operand_size == 1)
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_byte;
					}
				}
				else
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_word;
					}
				}

				Execution::memory_mutex.unlock();
			}

			return;
		}
		else if (Execution::current_instruction == "JGT")
		{
			if (((Execution::psw & Execution::N_MASK) >> 3) == ((Execution::psw & Execution::O_MASK) >> 1))
			{
				Execution::memory_mutex.lock();

				if (Execution::operand_size == 1)
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_byte;
					}
				}
				else
				{
					if (pc_rel_address != -1)
					{
						Execution::pc = (uint16_t) pc_rel_address;
						pc_rel_address = -1;
					}
					else
					{
						Execution::pc = (uint16_t) destination_operand_word;
					}
				}

				Execution::memory_mutex.unlock();
			}

			return;
		}
	}
	else if (Execution::num_of_operands == 2)
	{
		// source

		int16_t source_operand_word = 0;
		int8_t  source_operand_byte = 0;

		if (Execution::first_operand_addressing == "IMMEDIATE")
		{
			if (Execution::operand_size == 1)
			{
				source_operand_byte = Execution::first_operand_value_byte;
			}
			else if (Execution::operand_size == 2)
			{
				source_operand_word = Execution::first_operand_value_word;
			}
		}
		else if (Execution::first_operand_addressing == "REG_DIR")
		{
			if (Execution::operand_size == 1)
			{
				if (Execution::first_operand_register_high)
				{
					if (Execution::first_operand_register_number == 6)
					{
						source_operand_byte = (int8_t) ((Execution::sp >> 8) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 7)
					{
						source_operand_byte = (int8_t) ((Execution::pc >> 8) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 15)
					{
						source_operand_byte = (int8_t) ((Execution::psw >> 8) & 0xFF);
					}
					else
					{
						source_operand_byte = (int8_t) ((Execution::registers[Execution::first_operand_register_number] >> 8) & 0xFF);
					}
				}
				else
				{
					if (Execution::first_operand_register_number == 6)
					{
						source_operand_byte = (int8_t) ((Execution::sp) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 7)
					{
						source_operand_byte = (int8_t) ((Execution::pc) & 0xFF);
					}
					else if (Execution::first_operand_register_number == 15)
					{
						source_operand_byte = (int8_t) ((Execution::psw) & 0xFF);
					}
					else
					{
						source_operand_byte = (int8_t) ((Execution::registers[Execution::first_operand_register_number]) & 0xFF);
					}
				}
			}
			else if (Execution::operand_size == 2)
			{
				if (Execution::first_operand_register_number == 6)
				{
					source_operand_word = Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					source_operand_word = Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					source_operand_word = Execution::psw;
				}
				else
				{
					source_operand_word = Execution::registers[Execution::first_operand_register_number];
				}
			}
		}
		else if (Execution::first_operand_addressing == "REG_IND")
		{
			uint16_t address = 0;

			if (Execution::first_operand_register_number == 6)
			{
				address = Execution::sp;
			}
			else if (Execution::first_operand_register_number == 7)
			{
				address = Execution::pc;
			}
			else if (Execution::first_operand_register_number == 15)
			{
				address = Execution::psw;
			}
			else
			{
				address = Execution::registers[Execution::first_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				source_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				source_operand_word = Execution::mem[address + 1] << 8 | Execution::mem[address + 0];
			}
		}
		else if (Execution::first_operand_addressing == "REG_IND_DISPL")
		{
			uint16_t address = Execution::first_operand_value_word;

			if (Execution::first_operand_register_number == 6)
			{
				address += Execution::sp;
			}
			else if (Execution::first_operand_register_number == 7)
			{
				address += Execution::pc;
			}
			else if (Execution::first_operand_register_number == 15)
			{
				address += Execution::psw;
			}
			else
			{
				address += Execution::registers[Execution::first_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				source_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				source_operand_word = Execution::mem[address + 1] << 8 | Execution::mem[address + 0];
			}
		}
		else if (Execution::first_operand_addressing == "MEMORY")
		{
			if (Execution::operand_size == 1)
			{
				source_operand_byte = (int8_t) (Execution::mem[Execution::first_operand_value_word] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				source_operand_word = Execution::mem[Execution::first_operand_value_word + 1] << 8 | Execution::mem[Execution::first_operand_value_word + 0];
			}
		}

		// destination

		int16_t destination_operand_word = 0;
		int8_t  destination_operand_byte = 0;

		if (Execution::second_operand_addressing == "IMMEDIATE")
		{
			if (Execution::operand_size == 1)
			{
				destination_operand_byte = Execution::second_operand_value_byte;
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::second_operand_value_word;
			}
		}
		else if (Execution::second_operand_addressing == "REG_DIR")
		{
			if (Execution::operand_size == 1)
			{
				if (Execution::second_operand_register_high)
				{
					if (Execution::second_operand_register_number == 6)
					{
						destination_operand_byte = (int8_t) ((Execution::sp >> 8) & 0xFF);
					}
					else if (Execution::second_operand_register_number == 7)
					{
						destination_operand_byte = (int8_t) ((Execution::pc >> 8) & 0xFF);
					}
					else if (Execution::second_operand_register_number == 15)
					{
						destination_operand_byte = (int8_t) ((Execution::psw >> 8) & 0xFF);
					}
					else
					{
						destination_operand_byte = (int8_t) ((Execution::registers[Execution::second_operand_register_number] >> 8) & 0xFF);
					}
				}
				else
				{
					if (Execution::second_operand_register_number == 6)
					{
						destination_operand_byte = (int8_t) ((Execution::sp) & 0xFF);
					}
					else if (Execution::second_operand_register_number == 7)
					{
						destination_operand_byte = (int8_t) ((Execution::pc) & 0xFF);
					}
					else if (Execution::second_operand_register_number == 15)
					{
						destination_operand_byte = (int8_t) ((Execution::psw) & 0xFF);
					}
					else
					{
						destination_operand_byte = (int8_t) ((Execution::registers[Execution::second_operand_register_number]) & 0xFF);
					}
				}
			}
			else if (Execution::operand_size == 2)
			{
				if (Execution::second_operand_register_number == 6)
				{
					destination_operand_word = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					destination_operand_word = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					destination_operand_word = Execution::psw;
				}
				else
				{
					destination_operand_word = Execution::registers[Execution::second_operand_register_number];
				}
			}
		}
		else if (Execution::second_operand_addressing == "REG_IND")
		{
			uint16_t address = 0;

			if (Execution::second_operand_register_number == 6)
			{
				address = Execution::sp;
			}
			else if (Execution::second_operand_register_number == 7)
			{
				address = Execution::pc;
			}
			else if (Execution::second_operand_register_number == 15)
			{
				address = Execution::psw;
			}
			else
			{
				address = Execution::registers[Execution::second_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[address + 1] << 8 | Execution::mem[address + 0];
			}
		}
		else if (Execution::first_operand_addressing == "REG_IND_DISPL")
		{
			uint16_t address = Execution::second_operand_value_word;

			if (Execution::second_operand_register_number == 6)
			{
				address += Execution::sp;
			}
			else if (Execution::second_operand_register_number == 7)
			{
				address += Execution::pc;
			}
			else if (Execution::second_operand_register_number == 15)
			{
				address += Execution::psw;
			}
			else
			{
				address += Execution::registers[Execution::second_operand_register_number];
			}

			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[address] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[address];
			}
		}
		else if (Execution::second_operand_addressing == "MEMORY")
		{
			if (Execution::operand_size == 1)
			{
				destination_operand_byte = (int8_t) (Execution::mem[Execution::second_operand_value_word] & 0xFF);
			}
			else if (Execution::operand_size == 2)
			{
				destination_operand_word = Execution::mem[second_operand_value_word + 1] << 8 | Execution::mem[second_operand_value_word + 0];
			}
		}

		// instructions

		int8_t temp_byte = 0;
		int16_t temp_word = 0;

		if (Execution::current_instruction == "XCHG")
		{
			// temp = dst

			if (Execution::operand_size == 1)
			{
				temp_byte = destination_operand_byte;
			}
			else
			{
				temp_word = destination_operand_word;
			}

			// dst = src

			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp = source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc = source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw = source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] = source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] = source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[Execution::second_operand_value_word + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[Execution::second_operand_value_word + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}

			// src = temp

			if (Execution::first_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::first_operand_register_high)
					{
						if (Execution::first_operand_register_number == 6)
						{
							Execution::sp = (int16_t) (temp_byte << 8 | 0x00);
						}
						else if (Execution::first_operand_register_number == 7)
						{
							Execution::pc = (int16_t) (temp_byte << 8 | 0x00);
						}
						else if (Execution::first_operand_register_number == 15)
						{
							Execution::psw = (int16_t) (temp_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::first_operand_register_number] = (int16_t) (temp_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::first_operand_register_number == 6)
						{
							Execution::sp = (int16_t) temp_byte;
						}
						else if (Execution::first_operand_register_number == 7)
						{
							Execution::pc = (int16_t) temp_byte;
						}
						else if (Execution::first_operand_register_number == 15)
						{
							Execution::psw = (int16_t) temp_byte;
						}
						else
						{
							Execution::registers[Execution::first_operand_register_number] = (int16_t) temp_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::first_operand_register_number == 6)
					{
						Execution::sp = temp_word;
					}
					else if (Execution::first_operand_register_number == 7)
					{
						Execution::pc = temp_word;
					}
					else if (Execution::first_operand_register_number == 15)
					{
						Execution::psw = temp_word;
					}
					else
					{
						Execution::registers[Execution::first_operand_register_number] = temp_word;
					}
				}
			}
			else if (Execution::first_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::first_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::first_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = (int16_t) temp_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (temp_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((temp_word >> 8) & 0xFF);
				}
			}
			else if (Execution::first_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::first_operand_value_word;

				if (Execution::first_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::first_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::first_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::first_operand_register_number];
				}


				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = temp_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (temp_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((temp_word >> 8) & 0xFF);
				}
			}
			else if (Execution::first_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::first_operand_value_word] = temp_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[Execution::first_operand_value_word + 0] = (int8_t) (temp_word & 0xFF);
					Execution::mem[Execution::first_operand_value_word + 1] = (int8_t) ((temp_word >> 8) & 0xFF);
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "MOV")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp = source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc = source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw = source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] = source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (address == Execution::data_in)
				{
					Execution::data_out_write = true;
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (address == Execution::data_out)
				{
					Execution::data_out_write = true;
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::second_operand_value_word == Execution::data_out)
				{
					Execution::data_out_write = true;
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] = source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[Execution::second_operand_value_word + 0] = (int8_t) (source_operand_word & 0xFF);
					Execution::mem[Execution::second_operand_value_word + 1] = (int8_t) ((source_operand_word >> 8) & 0xFF);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "ADD")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp += (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc += (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw += (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] += (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp += (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc += (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw += (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] += (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp += source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc += source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw += source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] += source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] += (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word + destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word + destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}


				if (Execution::operand_size == 1)
				{
					Execution::mem[address] += (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word + destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word + destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] += (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word + destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word + destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte + destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_byte >= 0 && destination_operand_byte >= 0 && temp_byte < 0) || (source_operand_byte < 0 && destination_operand_byte < 0 && temp_byte >= 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if ((destination_operand_byte < 0 && temp_byte >= 0) || (source_operand_byte < 0 && temp_byte >= 0))
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word + destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_word >= 0 && destination_operand_word >= 0 && temp_word < 0) || (source_operand_word < 0 && destination_operand_word < 0 && temp_word >= 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if ((destination_operand_word < 0 && temp_word >= 0) || (source_operand_word < 0 && temp_word >= 0))
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "SUB")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp -= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc -= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw -= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] -= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp -= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc -= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw -= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] -= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp -= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc -= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw -= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] -= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] -= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word - source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word - source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] -= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word - source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word - source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] -= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((destination_operand_word - source_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((destination_operand_word - source_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = destination_operand_byte - source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_byte < 0 && destination_operand_byte >= 0 && temp_byte >= 0) || (source_operand_byte >= 0 && destination_operand_byte < 0 && temp_byte < 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if (destination_operand_byte >= 0 && temp_byte < 0)
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}
			else
			{
				temp_word = destination_operand_word - source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_word < 0 && destination_operand_word >= 0 && temp_word >= 0) || (source_operand_word >= 0 && destination_operand_word < 0 && temp_word < 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if (destination_operand_word >= 0 && temp_word < 0)
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "MUL")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp *= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc *= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw *= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] *= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp *= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc *= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw *= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] *= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp *= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc *= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw *= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] *= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] *= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word * source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word * source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] *= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word * source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word * source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] *= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((destination_operand_word * source_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((destination_operand_word * source_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte * destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word * destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "DIV")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp /= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc /= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw /= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] /= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp /= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc /= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw /= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] /= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp /= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc /= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw /= source_operand_word;
					}
					else
					{
						// Execution::registers[5] = destination_operand_word % source_operand_word;
						Execution::registers[Execution::second_operand_register_number] /= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] /= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word / source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word / source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] /= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word / source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word / source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] /= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((destination_operand_word / source_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((destination_operand_word / source_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte / destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word / destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "MOD")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp %= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc %= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw %= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] %= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp %= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc %= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw %= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] %= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp %= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc %= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw %= source_operand_word;
					}
					else
					{
						// Execution::registers[5] = destination_operand_word % source_operand_word;
						Execution::registers[Execution::second_operand_register_number] %= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] %= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word % source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word % source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] %= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((destination_operand_word % source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((destination_operand_word % source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] %= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((destination_operand_word % source_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((destination_operand_word % source_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte % destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word % destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "CMP")
		{
			Execution::memory_mutex.lock();

			if (Execution::operand_size == 1)
			{
				temp_byte = destination_operand_byte - source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_byte < 0 && destination_operand_byte >= 0 && temp_byte >= 0) || (source_operand_byte >= 0 && destination_operand_byte < 0 && temp_byte < 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if (destination_operand_byte >= 0 && temp_byte < 0)
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}
			else
			{
				temp_word = destination_operand_word - source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				if ((source_operand_word < 0 && destination_operand_word >= 0 && temp_word >= 0) || (source_operand_word >= 0 && destination_operand_word < 0 && temp_word < 0))
				{
					Execution::psw |= Execution::O_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::O_MASK;
				}

				if (destination_operand_word >= 0 && temp_word < 0)
				{
					Execution::psw |= Execution::C_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::C_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "NOT")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = ~(int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = ~(int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = ~(int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = ~(int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp = ~(int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc = ~(int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw = ~(int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] = ~(int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp = ~source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc = ~source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw = ~source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] = ~source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = ~(int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((~source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((~source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] = ~(int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((~source_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((~source_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] = ~(int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((~source_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((~source_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = ~source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = ~source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "AND")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp &= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc &= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw &= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] &= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp &= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc &= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw &= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] &= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp &= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc &= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw &= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] &= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] &= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word & destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word & destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] &= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word & destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word & destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] &= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word & destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word & destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte & destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word & destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "OR")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp |= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc |= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw |= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] |= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp |= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc |= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw |= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] |= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp |= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc |= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw |= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] |= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] |= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word | destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word | destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] |= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word | destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word | destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] |= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word | destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word | destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte | destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word | destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "XOR")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp ^= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc ^= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw ^= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] ^= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp ^= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc ^= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw ^= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] ^= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp ^= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc ^= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw ^= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] ^= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] ^= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word ^ destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word ^ destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] ^= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word ^ destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word ^ destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] ^= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word ^ destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word ^ destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte ^ destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word ^ destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "TEST")
		{
			Execution::memory_mutex.lock();

			if (Execution::operand_size == 1)
			{
				temp_byte = source_operand_byte & destination_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}
			else
			{
				temp_word = source_operand_word & destination_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "SHL")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp <<= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc <<= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw <<= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] <<= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp <<= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc <<= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw <<= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] <<= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp <<= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc <<= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw <<= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] <<= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] <<= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word << destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word << destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] <<= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word << destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word << destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] <<= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word << destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word << destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = destination_operand_byte << source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				for (int8_t i = 1; i <= source_operand_byte; ++i)
				{
					if (temp_byte < 0)
					{
						Execution::psw |= Execution::C_MASK;
					}
					else
					{
						Execution::psw &= ~Execution::C_MASK;
					}

					temp_byte <<= 1;
				}
			}
			else
			{
				temp_word = destination_operand_word << source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				for (int16_t i = 1; i <= source_operand_word; ++i)
				{
					if (temp_word < 0)
					{
						Execution::psw |= Execution::C_MASK;
					}
					else
					{
						Execution::psw &= ~Execution::C_MASK;
					}

					temp_word <<= 1;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
		else if (Execution::current_instruction == "SHR")
		{
			Execution::memory_mutex.lock();

			if (Execution::second_operand_addressing == "REG_DIR")
			{
				if (Execution::operand_size == 1)
				{
					if (Execution::second_operand_register_high)
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp >>= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc >>= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw >>= (int16_t) (source_operand_byte << 8 | 0x00);
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] >>= (int16_t) (source_operand_byte << 8 | 0x00);
						}
					}
					else
					{
						if (Execution::second_operand_register_number == 6)
						{
							Execution::sp >>= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 7)
						{
							Execution::pc >>= (int16_t) source_operand_byte;
						}
						else if (Execution::second_operand_register_number == 15)
						{
							Execution::psw >>= (int16_t) source_operand_byte;
						}
						else
						{
							Execution::registers[Execution::second_operand_register_number] >>= (int16_t) source_operand_byte;
						}
					}
				}
				else if (Execution::operand_size == 2)
				{
					if (Execution::second_operand_register_number == 6)
					{
						Execution::sp >>= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 7)
					{
						Execution::pc >>= source_operand_word;
					}
					else if (Execution::second_operand_register_number == 15)
					{
						Execution::psw >>= source_operand_word;
					}
					else
					{
						Execution::registers[Execution::second_operand_register_number] >>= source_operand_word;
					}
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND")
			{
				uint16_t address = 0;

				if (Execution::second_operand_register_number == 6)
				{
					address = Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address = Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address = Execution::psw;
				}
				else
				{
					address = Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] >>= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word >> destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "REG_IND_DISPL")
			{
				uint16_t address = Execution::second_operand_value_word;

				if (Execution::second_operand_register_number == 6)
				{
					address += Execution::sp;
				}
				else if (Execution::second_operand_register_number == 7)
				{
					address += Execution::pc;
				}
				else if (Execution::second_operand_register_number == 15)
				{
					address += Execution::psw;
				}
				else
				{
					address += Execution::registers[Execution::second_operand_register_number];
				}

				if (Execution::operand_size == 1)
				{
					Execution::mem[address] >>= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[address + 0] = (int8_t) ((source_operand_word >> destination_operand_word) & 0xFF);
					Execution::mem[address + 1] = (int8_t) ((source_operand_word >> destination_operand_word) >> 8);
				}
			}
			else if (Execution::second_operand_addressing == "MEMORY")
			{
				if (Execution::operand_size == 1)
				{
					Execution::mem[Execution::second_operand_value_word] >>= (int16_t) source_operand_byte;
				}
				else if (Execution::operand_size == 2)
				{
					Execution::mem[second_operand_value_word + 0] = (int8_t) ((source_operand_word >> destination_operand_word) & 0xFF);
					Execution::mem[second_operand_value_word + 1] = (int8_t) ((source_operand_word >> destination_operand_word) >> 8);
				}
			}

			if (Execution::operand_size == 1)
			{
				temp_byte = destination_operand_byte >> source_operand_byte;

				if (temp_byte < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_byte == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				for (int8_t i = 1; i <= source_operand_byte; ++i)
				{
					if (temp_byte < 0)
					{
						Execution::psw |= Execution::C_MASK;
					}
					else
					{
						Execution::psw &= ~Execution::C_MASK;
					}

					temp_byte >>= 1;
				}
			}
			else
			{
				temp_word = destination_operand_word >> source_operand_word;

				if (temp_word < 0)
				{
					Execution::psw |= Execution::N_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::N_MASK;
				}

				if (temp_word == 0)
				{
					Execution::psw |= Execution::Z_MASK;
				}
				else
				{
					Execution::psw &= ~Execution::Z_MASK;
				}

				for (int16_t i = 1; i <= source_operand_word; ++i)
				{
					if (temp_word < 0)
					{
						Execution::psw |= Execution::C_MASK;
					}
					else
					{
						Execution::psw &= ~Execution::C_MASK;
					}

					temp_word >>= 1;
				}
			}

			Execution::memory_mutex.unlock();

			return;
		}
	}
}

void Execution::set_in_motion()
{
	Execution::initialize();

	Execution::reset();

	Execution::decode_instruction();

	while (Execution::execution)
	{
		if (Execution::instruction_error)
		{
			Execution::decode_instruction();
		}

		Execution::execute_instruction();

		if (Execution::halt)
		{
			Execution::condition_mutex.lock();

			Interrupt::terminal_end = true;
			Interrupt::timer_end    = true;

			Execution::condition_mutex.unlock();

			break;
		}

		Execution::resolve_interrupts();

		Execution::reset();

		Execution::decode_instruction();
	}

	for (auto& thread : Execution::threads)
	{
		thread->join();
	}
}