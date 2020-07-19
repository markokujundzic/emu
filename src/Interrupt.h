#ifndef _interrupt_h_
#define _interrupt_h_

#include <atomic>
#include <string>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <memory>
#include <termios.h>
#include <unistd.h>

using namespace std;

typedef string interrupt_type;

class Interrupt
{
public:
	friend class Execution;

	Interrupt(const interrupt_type& type);

	static constexpr bool barrier = true;

	static const string TIMER_INTERRUPT;
	static const string TERMINAL_INTERRUPT;

	static unordered_map<uint16_t, uint16_t> sleeping_period;

	static atomic<bool> timer_end;
	static atomic<bool> terminal_end;

	static atomic<bool> terminal_interrupt_to_handle;

	static void terminal_routine();
	static void timer_routine();

	static uint16_t sleeping_time;

	static struct termios _termios;

	static void turn_off_buffered_input();

	static void turn_on_buffered_input();

	static char buffer;
private:
	interrupt_type type;
};

#endif