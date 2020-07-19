#include "Interrupt.h"
#include "Execution.h"

atomic<bool> Interrupt::timer_end;

atomic<bool> Interrupt::terminal_end;

atomic<bool> Interrupt::terminal_interrupt_to_handle;

constexpr bool Interrupt::barrier;

uint16_t Interrupt::sleeping_time = 0;

char Interrupt::buffer = 0;

struct termios Interrupt::_termios;

const string Interrupt::TIMER_INTERRUPT    = "TIMER_INTERRUPT";
const string Interrupt::TERMINAL_INTERRUPT = "TERMINAL_INTERRUPT";

unordered_map<uint16_t, uint16_t> Interrupt::sleeping_period =
{
	{ 0x0, 500   },
	{ 0x1, 1000  },
	{ 0x2, 1500  },
	{ 0x3, 2000  },
	{ 0x4, 5000  },
	{ 0x5, 10000 },
	{ 0x6, 30000 },
	{ 0x7, 60000 }
};

Interrupt::Interrupt(const interrupt_type& type) : type(type)
{

}

void Interrupt::turn_on_buffered_input()
{
	tcgetattr(STDIN_FILENO, &Interrupt::_termios);
	Interrupt::_termios.c_lflag |= ICANON;
	tcsetattr(STDIN_FILENO, TCSANOW, &Interrupt::_termios);
}

void Interrupt::turn_off_buffered_input()
{
	tcgetattr(STDIN_FILENO, &Interrupt::_termios);
	Interrupt::_termios.c_lflag &= ~ICANON;
	tcsetattr(STDIN_FILENO, TCSANOW, &Interrupt::_termios);
}

void Interrupt::timer_routine()
{
	while (!Interrupt::timer_end)
	{
		Execution::memory_mutex.lock();

		bool found = false;

		for (const auto& sleep : Interrupt::sleeping_period)
		{
			if (sleep.first == (Execution::mem[Execution::timer_cfg + 1] << 8 | Execution::mem[Execution::timer_cfg + 0]))
			{
				Interrupt::sleeping_time = sleep.second;
				found = true;
				break;
			}
		}

		if (!found)
		{
			Interrupt::sleeping_time = 500;
		}

		Execution::memory_mutex.unlock();

		auto time = chrono::high_resolution_clock::now();

		while (chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - time).count() <= Interrupt::sleeping_time);

		Execution::interrupt_mutex.lock();

		Execution::interrupts.push_back(make_unique<Interrupt>(Interrupt::TIMER_INTERRUPT));

		Execution::interrupt_mutex.unlock();
	}
}

void Interrupt::terminal_routine()
{
	while (!Interrupt::terminal_end)
	{
		while (Interrupt::terminal_interrupt_to_handle);

		Interrupt::buffer = getchar();

		Interrupt::terminal_interrupt_to_handle = true;

		while (Interrupt::barrier)
		{
			Execution::memory_mutex.lock();

			if (!Execution::mem[Execution::data_in])
			{
				Execution::memory_mutex.unlock();
				break;
			}

			Execution::memory_mutex.unlock();
		}

		Execution::memory_mutex.lock();

		Execution::mem[Execution::data_in] = Interrupt::buffer;

		Execution::memory_mutex.unlock();

		Execution::interrupt_mutex.lock();

		Execution::interrupts.push_back(make_unique<Interrupt>(Interrupt::TERMINAL_INTERRUPT));

		Execution::interrupt_mutex.unlock();
	}
}
