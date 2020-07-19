#include "Assembler.h"
#include "Error.h"
#include "Lexer.h"
#include "Linker.h"
#include "Loader.h"
#include "Execution.h"

#include <iostream>
#include <string>
#include <deque>
#include <regex>
#include <memory>

using namespace std;

int main(int argc, const char *argv[])
{
	static constexpr int _min_num_of_args      = 2;
	static constexpr int _correct_execution    = 1;
	static constexpr int _incorrect_execution = -1;

	static const regex object_file_regex(".*\\.o$");
	static const regex start_address_regex("-place=[_a-zA-Z\\.]\\w*@0x[a-zA-Z0-9]{1,4}");

	try
	{
		if (argc < _min_num_of_args)
		{
			throw Error("Error: argument count is lower than 2.");
		}

		deque<string> input_object_files;

		for (int i = 1; i < argc; i++)
		{
			if (regex_match(argv[i], object_file_regex))
			{
				input_object_files.push_back(argv[i]);
			}
			else if (regex_match(argv[i], start_address_regex))
			{
				Linker::resolve_start_address(argv[i]);
			}
			else
			{
				throw Error("Error: input type must be either an object(.o) file or the following sequence: -place=<section_name>@<start_address>, where the starting address is 16-bit long.");
			}
		}

		if (input_object_files.empty())
		{
			throw Error("Error: at least one object(.o) file must be supplied as input.");
		}

		Interrupt::turn_off_buffered_input();

		Linker::make_executable(input_object_files);

		Loader::load_all_object_files();

		Execution::set_in_motion();

		Interrupt::turn_on_buffered_input();

		Linker::delete_object_files();

		Linker::delete_symbol_table();

		Linker::delete_relocations();
	}
	catch (const Error& error)
	{
		cerr << error.what() << endl;

		Execution::condition_mutex.lock();

		Interrupt::terminal_end = true;
		Interrupt::timer_end    = true;

		Execution::condition_mutex.unlock();

		for (auto& thread : Execution::threads)
		{
			thread->join();
		}

		Interrupt::turn_on_buffered_input();

		Linker::delete_object_files();

		Linker::delete_symbol_table();

		Linker::delete_relocations();

		return _incorrect_execution;
	}

	return _correct_execution;
}