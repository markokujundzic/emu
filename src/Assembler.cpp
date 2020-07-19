#include "Assembler.h"

list<UnknownSymbol*> Assembler::table_of_unknown_symbols;

list<Symbol*> Assembler::symbol_table;

list <Data*> Assembler::data;

vector<vector<string>> Assembler::tokens;

deque<string> Assembler::section_names;

Symbol *Assembler::current_section;

uint16_t Assembler::location_counter = 0;

string Assembler::input = "";

int8_t Assembler::num_of_sections = 0;

constexpr uint8_t Assembler::_default;

constexpr uint8_t Assembler::_extern;

constexpr uint8_t Assembler::_global;

constexpr uint8_t Assembler::_equ;

void Assembler::resolve_unknown_symbols()
{
	regex symbol_regex("[_a-zA-Z]\\w*");
	regex number_regex("\\d+");
	regex operation_regex("[\\+-]");

	size_t to_delete = Assembler::table_of_unknown_symbols.size();

	while (true)
	{
		bool change = false;

		for (list<UnknownSymbol*>::iterator it = Assembler::table_of_unknown_symbols.begin(); it != Assembler::table_of_unknown_symbols.end(); it++)
		{
			if ((*it)->deleted)
			{
				continue;
			}

			deque<string> c_operands = (*it)->operands;
			deque<string> c_operations = (*it)->operations;

			bool undefined_symbol_found = false;

			for (const auto& itr : c_operands)
			{
				if (regex_match(itr, symbol_regex))
				{
					for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
					{
						if ((*it)->name == itr)
						{
							if (!(*it)->defined)
							{
								undefined_symbol_found = true;
							}

							break;
						}
					}

					if (undefined_symbol_found)
					{
						break;
					}
				}
				else if (regex_match(itr, operation_regex) || regex_match(itr, number_regex))
				{
					continue;
				}
			}

			if (undefined_symbol_found)
			{
				continue;
			}

			// all operands are either defined symbols or literals -> symbol value can be calculated

			// classification index

			deque<string> symbols;

			c_operands = (*it)->operands;
			c_operations = (*it)->operations;

			unordered_map<string, int8_t> classification_index;

			for (size_t i = 0; i < Assembler::section_names.size(); i++)
			{
				classification_index.insert(make_pair(Assembler::section_names[i], 0));
			}

			string current_operation;
			string current_operand;

			while (c_operands.size() || c_operations.size())
			{
				current_operand = c_operands.front();
				c_operands.pop_front();

				current_operation = c_operations.front();
				c_operations.pop_front();

				if (!regex_match(current_operand, symbol_regex))
				{
					continue;
				}

				for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
				{
					if ((*ite)->name == current_operand)
					{
						if ((*ite)->is_extern)
						{
							classification_index[(*ite)->section] += 1;
						}
						else if (!(*ite)->absolute)
						{
							if (current_operation == "+")
							{
								classification_index[(*ite)->section] += 1;
							}
							else
							{
								classification_index[(*ite)->section] -= 1;
							}
						}

						symbols.push_back(current_operand);

						break;
					}
				}
			}

			int8_t found_entry = 0;

			string section_to_define;

			for (const pair<string, uint8_t>& ite : classification_index)
			{
				if (ite.second < 0 || ite.second > 1)
				{
					string directive = "";

					while ((*it)->operands.size() || (*it)->operations.size())
					{
						directive += (*it)->operations.front();
						directive += " ";

						(*it)->operations.pop_front();

						directive += (*it)->operands.front();
						directive += " ";

						(*it)->operands.pop_front();
					}

					if (directive.front() == '+')
					{
						directive.erase(0, 2);
					}

					string error = "Error: classification index is incorect for the following directive: .equ " + (*it)->name + ", " + directive;

					throw Error(error);
				}
				else if (ite.second == 1)
				{
					section_to_define = ite.first;
					found_entry++;
				}
				else if (ite.second == 0)
				{
					continue;
				}

				if (found_entry > 1)
				{
					string directive = "";

					while ((*it)->operands.size() || (*it)->operations.size())
					{
						directive += (*it)->operations.front();
						directive += " ";

						(*it)->operations.pop_front();

						directive += (*it)->operands.front();
						directive += " ";

						(*it)->operands.pop_front();
					}

					if (directive.front() == '+')
					{
						directive.erase(0, 2);
					}

					string error = "Error: classification index is incorect for the following directive: .equ " + (*it)->name + ", " + directive;

					throw Error(error);
				}
			}

			if (found_entry == 1)
			{
				// symbol is relocatable

				c_operands = (*it)->operands;
				c_operations = (*it)->operations;

				int16_t result = 0;
				int16_t to_add = 0;

				string relative_relocatable_symbol;
				string scope;

				bool entry = false;
				bool is_extern = false;

				while (c_operands.size() || c_operations.size())
				{
					current_operation = c_operations.front();
					c_operations.pop_front();

					current_operand = c_operands.front();
					c_operands.pop_front();

					if (regex_search(current_operand, symbol_regex))
					{
						for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
						{
							if ((*iter)->name == current_operand)
							{
								if (!entry && (*iter)->section == section_to_define && !(*iter)->absolute && current_operation == "+")
								{
									if ((*iter)->is_extern)
									{
										is_extern = true;
									}

									to_add = (*iter)->value;

									scope = (*iter)->scope;

									relative_relocatable_symbol = current_operand;

									entry = true;

									break;
								}
								else
								{
									if (current_operation == "+")
									{
										result += (int16_t) ((*iter)->value & 0xFFFF);
									}
									else if (current_operation == "-")
									{
										result -= (int16_t) ((*iter)->value & 0xFFFF);
									}

									break;
								}
							}
						}
					}
					else if (regex_search(current_operand, number_regex))
					{
						if (current_operation == "+")
						{
							result += (int16_t) (stoi(current_operand) & 0xFFFF);
						}
						else if (current_operation == "-")
						{
							result -= (int16_t) (stoi(current_operand) & 0xFFFF);
						}
					}
				}

				for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
				{
					if ((*iter)->name == (*it)->name)
					{
						(*iter)->is_extern = is_extern;
						(*iter)->equ_relocatable = true;
						(*iter)->defined = true;
						(*iter)->value = result;
						(*iter)->section = section_to_define;

						if ((*iter)->is_extern)
						{
							(*iter)->relative_relocatable_symbol = relative_relocatable_symbol;
							(*iter)->scope = scope;
						}
						else
						{
							if ((*iter)->scope == "GLOBAL")
							{
								(*iter)->relative_relocatable_symbol = relative_relocatable_symbol;
								(*iter)->equ_offset = result;
							}

							(*iter)->value += to_add;
						}

						break;
					}
				}

				(*it)->deleted = true;

				to_delete--;

				change = true;
			}
			else if (found_entry == 0)
			{
				// symbol is absolute

				c_operands = (*it)->operands;
				c_operations = (*it)->operations;

				int16_t result = 0;

				string op;

				while (c_operands.size() || c_operations.size())
				{
					current_operation = c_operations.front();
					c_operations.pop_front();

					current_operand = c_operands.front();
					c_operands.pop_front();

					if (regex_match(current_operand, symbol_regex))
					{
						for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
						{
							if ((*iter)->name == current_operand)
							{
								if (current_operation == "+")
								{
									result += (int16_t) ((*iter)->value & 0xFFFF);
								}
								else if (current_operation == "-")
								{
									result -= (int16_t) ((*iter)->value & 0xFFFF);
								}

								break;
							}
						}
					}
					else if (regex_match(current_operand, number_regex))
					{
						if (current_operation == "+")
						{
							result += (int16_t) (stoi(current_operand) & 0xFFFF);
						}
						else if (current_operation == "-")
						{
							result -= (int16_t) (stoi(current_operand) & 0xFFFF);
						}
					}
				}

				for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
				{
					if ((*iter)->name == (*it)->name)
					{
						(*iter)->absolute = true;
						(*iter)->defined = true;
						(*iter)->value = result;
						(*iter)->section = ".abs";
						(*iter)->equ_relocatable = true;

						break;
					}
				}

				(*it)->deleted = true;

				to_delete--;

				change = true;
			}

			// classification index
		}

		if (!change && to_delete)
		{
			string error = "Error: undefined symbol(s) or circular dependency detected -> .EQU directive.";

			throw Error(error);
		}

		if (!to_delete)
		{
			break;
		}
	}

	Assembler::table_of_unknown_symbols.clear();
}

void Assembler::check_up()
{
	int16_t num = 0;

	for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
	{
		(*it)->number = num++;
	}

	// -> ready == true  -> instruction contains section content for writing + instruction contains no symbols, hence no relocations
	// -> ready == false -> contains symbols -> PENDING CHECK FOR UNDEFINED SYMBOLS -> if (ok)  -> PENDING WRITING SECTION CONTENT + PENDING RELOCATION 
	//																			    -> if (!ok) -> ERROR

	for (list<Data*>::iterator it = Assembler::data.begin(); it != Assembler::data.end(); it++)
	{
		if ((*it)->directive == Data::__directive)
		{
			if (!(*it)->ready)
			{
				// check for undefined symbols

				regex symbol_regex("[_a-zA-Z]\\w*");
				regex number_regex("-?\\d+");

				string concat = "";
				string help = "";

				string current;

				queue<string> copy = (*it)->tokens;
				queue<string> out = copy;

				copy.pop();

				bool flag = false;

				while (copy.size())
				{
					current = copy.front();
					copy.pop();

					for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
					{
						if (regex_match(current, number_regex))
						{
							continue;
						}

						if (current == (*ite)->name)
						{
							if (!(*ite)->defined)
							{
								flag = true;

								help += current;

								if (copy.size())
								{
									help += ", ";
								}
								else
								{
									help += " ";
								}
							}
						}
					}
				}

				if (flag)
				{
					help.pop_back();

					concat = out.front();
					out.pop();

					concat += " ";

					while (out.size())
					{
						concat += out.front();
						out.pop();

						if (out.size())
						{
							concat += ", ";
						}
						else
						{
							concat += " ";
						}
					}

					concat.pop_back();

					string error = "Error: undefined symbol(s): " + help + " -> " + concat + " directive.";

					throw Error(error);
				}

				queue<string> tokens = (*it)->tokens;

				string machine_code = "";

				string current_token = tokens.front();

				string directive = current_token;

				tokens.pop();

				uint16_t lc_increase = 0;

				while (tokens.size())
				{
					current_token = tokens.front();
					tokens.pop();

					if (regex_match(current_token, number_regex))
					{
						if (directive == ".word")
						{
							uint16_t val = (uint16_t) (stoi(current_token) & 0xFFFF);

							machine_code += Lexer::to_hex(val);
							machine_code += " ";

							lc_increase += 2;
						}
						else if (directive == ".byte")
						{
							uint8_t val = (uint8_t) (stoi(current_token) & 0xFF);

							machine_code += Lexer::to_hex(val);
							machine_code += " ";

							lc_increase++;
						}
					}
					else if (regex_match(current_token, symbol_regex))
					{
						for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
						{
							if ((*ite)->name == current_token)
							{
								if ((*ite)->absolute)
								{
									if (directive == ".word")
									{
										uint16_t val = (*ite)->value;

										machine_code += Lexer::to_hex(val);
										machine_code += " ";

										lc_increase += 2;
									}
									else if (directive == ".byte")
									{
										uint8_t val = (uint8_t) ((*ite)->value & 0xFF);

										machine_code += Lexer::to_hex(val);
										machine_code += " ";

										lc_increase++;
									}
								}
								else
								{
									if ((*ite)->equ_relocatable)
									{
										// relocatable symbol

										string type;

										if (directive == ".word")
										{
											uint16_t val = 0;

											if ((*ite)->scope == "GLOBAL")
											{
												if ((*ite)->is_extern)
												{
													val = (*ite)->value;

													machine_code += Lexer::to_hex(val);
													machine_code += " ";
												}
												else
												{
													val = (*ite)->equ_offset;

													machine_code += Lexer::to_hex(val);
													machine_code += " ";
												}
											}
											else if ((*ite)->scope == "LOCAL")
											{
												val = (*ite)->value;

												machine_code += Lexer::to_hex(val);
												machine_code += " ";
											}

											type = "R_386_16";
										}
										else if (directive == ".byte")
										{
											uint8_t val = 0;

											if ((*ite)->scope == "GLOBAL")
											{
												if ((*ite)->is_extern)
												{
													val = (uint8_t) ((*ite)->value & 0xFF);

													machine_code += Lexer::to_hex(val);
													machine_code += " ";
												}
												else
												{
													val = (uint8_t) ((*ite)->equ_offset & 0xFF);

													machine_code += Lexer::to_hex(val);
													machine_code += " ";
												}
											}
											else if ((*ite)->scope == "LOCAL")
											{
												val = (uint8_t) ((*ite)->value & 0xFF);

												machine_code += Lexer::to_hex(val);
												machine_code += " ";
											}

											type = "R_386_8";
										}

										uint16_t value = 0;

										if ((*ite)->scope == "GLOBAL")
										{
											string symbol = (*ite)->relative_relocatable_symbol;

											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if (symbol == (*iter)->name)
												{
													value = (*iter)->number;

													break;
												}
											}

										}
										else if ((*ite)->scope == "LOCAL")
										{
											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if ((*ite)->section == (*iter)->name)
												{
													value = (*iter)->number;

													break;
												}
											}
										}

										Relocation *_relocation = new Relocation((*it)->location_counter + lc_increase, value, type, (*it)->section_name);
										(*it)->relocations.push_back(_relocation);

										if (directive == ".word")
										{
											lc_increase += 2;
										}
										else if (directive == ".byte")
										{
											lc_increase++;
										}

										// relocatable symbol
									}
									else
									{
										string type;

										if (directive == ".word")
										{
											uint16_t val = (*ite)->value;

											if ((*ite)->scope == "GLOBAL")
											{
												val = 0;
											}

											machine_code += Lexer::to_hex(val);
											machine_code += " ";

											type = "R_386_16";
										}
										else if (directive == ".byte")
										{
											uint8_t val = (uint8_t) ((*ite)->value & 0xFF);

											if ((*ite)->scope == "GLOBAL")
											{
												val = 0;
											}

											machine_code += Lexer::to_hex(val);
											machine_code += " ";

											type = "R_386_8";
										}

										uint16_t value = 0;

										if ((*ite)->scope == "GLOBAL")
										{
											value = (*ite)->number;
										}
										else if ((*ite)->scope == "LOCAL")
										{
											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if ((*ite)->section == (*iter)->name)
												{
													value = (*iter)->number;
												}
											}
										}

										Relocation *_relocation = new Relocation((*it)->location_counter + lc_increase, value, type, (*it)->section_name);
										(*it)->relocations.push_back(_relocation);

										if (directive == ".word")
										{
											lc_increase += 2;
										}
										else if (directive == ".byte")
										{
											lc_increase++;
										}
									}
								}

								break;
							}
						}
					}
				}

				machine_code.pop_back();

				(*it)->machine_code = machine_code;
			}
		}
		else if ((*it)->directive == Data::__instruction)
		{
			if (!(*it)->ready)
			{
				// check for undefined symbols

				regex symbol_regex("[_a-zA-Z]\\w*");
				regex number_regex("-?\\d+");

				string error;

				_instruction temp = (*it)->relocatable_symbols;

				if (temp.is_jmp)
				{
					if (!Symbol::defined_in_symbol_table(temp.symbol_1))
					{
						error = "Error: undefined symbol " + temp.symbol_1 + " -> " + (*it)->tokens.front() + " " + (*it)->tokens.back() + " instruction.";

						throw Error(error);
					}
				}
				else
				{
					if (regex_match(temp.symbol_1, symbol_regex) && !Symbol::defined_in_symbol_table(temp.symbol_1))
					{
						queue<string> help = (*it)->tokens;

						help.pop();

						error = "Error: undefined symbol " + temp.symbol_1 + " -> " + (*it)->tokens.front() + " " + help.front() + ", " + (*it)->tokens.back() + " instruction.";

						throw Error(error);
					}
					else if (regex_match(temp.symbol_2, symbol_regex) && !Symbol::defined_in_symbol_table(temp.symbol_2))
					{
						queue<string> help = (*it)->tokens;

						help.pop();

						error = "Error: undefined symbol " + temp.symbol_2 + " -> " + (*it)->tokens.front() + " " + help.front() + ", " + (*it)->tokens.back() + " instruction.";

						throw Error(error);
					}
				}

				if (temp.is_jmp)
				{
					bool relocation = true;

					string out;

					uint8_t byte = 0;

					uint8_t op_code = 0;

					for (const pair<string, instruction_data>& itr : Parser::instructions)
					{
						if (regex_match((*it)->tokens.front(), itr.second.pattern))
						{
							byte = itr.second.op_code;
							break;
						}
					}

					op_code += (byte << 3);

					if (temp.word)
					{
						op_code += (0x1 << 2);
					}

					out = Lexer::to_hex(op_code);
					out += " ";

					op_code = byte = 0;

					if (temp.push_or_pop)
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_not_jmp_symbol)
						{
							if (temp.addressing_1 == i.first)
							{
								byte = i.second.op_code;

								break;
							}
						}
					}
					else
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_jmp_symbol)
						{
							if (temp.addressing_1 == i.first)
							{
								byte = i.second.op_code;

								break;
							}
						}
					}

					op_code += (byte << 5);

					uint8_t reg_num = 0;

					reg_num += temp.reg_num_1;

					op_code += (reg_num << 1);

					out += Lexer::to_hex(op_code);
					out += " ";

					for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
					{
						if ((*ite)->name == temp.symbol_1)
						{
							if ((*ite)->absolute)
							{
								if (!temp.word && temp.addressing_1 == "IMMEDIATE")
								{
									out += Lexer::to_hex((uint8_t) ((*ite)->value & 0xFF));
								}
								else
								{
									out += Lexer::to_hex((*ite)->value);
								}

								(*it)->machine_code = out;

								break;
							}
							else
							{
								if ((*ite)->equ_relocatable)
								{
									// relocatable symbol

									if ((*ite)->scope == "GLOBAL")
									{
										if (temp.addressing_1 == "REG_IND_DISPL_PC")
										{
											if ((*it)->section_name == (*ite)->section)
											{
												relocation = false;

												int16_t val = (*ite)->value - ((*it)->location_counter + 4);

												out += Lexer::to_hex(val);
											}
											else if ((*ite)->is_extern)
											{
												int16_t val = (*ite)->value - 2;

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = -2;

												out += Lexer::to_hex(val);
											}
										}
										else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
										{
											int8_t val = 0;

											if ((*ite)->is_extern)
											{
												val = (int8_t) ((*ite)->value & 0xFF);

												out += Lexer::to_hex(val);
											}
											else
											{
												val = 0;

												out += Lexer::to_hex(val);
											}
										}
										else
										{
											if ((*ite)->is_extern)
											{
												int16_t val = (*ite)->value;

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = 0;

												out += Lexer::to_hex(val);
											}
										}
									}
									else if ((*ite)->scope == "LOCAL")
									{
										if (temp.addressing_1 == "REG_IND_DISPL_PC")
										{
											if ((*it)->section_name == (*ite)->section)
											{
												relocation = false;

												int16_t val = (*ite)->value - ((*it)->location_counter + 4);

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = (*ite)->value - 2;

												out += Lexer::to_hex(val);
											}
										}
										else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
										{
											int8_t val = (int8_t) ((*ite)->value & 0xFF);

											out += Lexer::to_hex(val);
										}
										else
										{
											int16_t val = (int16_t) ((*ite)->value);

											out += Lexer::to_hex(val);
										}
									}

									(*it)->machine_code = out;

									// relocation

									if (relocation)
									{
										if ((*ite)->scope == "GLOBAL")
										{
											string type;

											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												type = "R_386_PC16";
											}
											else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
											{
												type = "R_386_8";
											}
											else
											{
												type = "R_386_16";
											}

											uint16_t value = 0;

											string equ_rel;

											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if (temp.symbol_1 == (*iter)->name)
												{
													if ((*iter)->is_extern)
													{
														equ_rel = (*iter)->relative_relocatable_symbol;

														for (list<Symbol*>::iterator itera = Assembler::symbol_table.begin(); itera != Assembler::symbol_table.end(); itera++)
														{
															if ((*itera)->name == equ_rel)
															{
																value = (*itera)->number;

																break;
															}
														}
													}
													else
													{
														value = (*iter)->number;
													}

													break;
												}
											}

											Relocation *_relocation = new Relocation((*it)->location_counter + 2, value, type, (*it)->section_name);

											(*it)->relocations.push_back(_relocation);
										}
										else if ((*ite)->scope == "LOCAL")
										{
											string type;

											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												type = "R_386_PC16";
											}
											else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
											{
												type = "R_386_8";
											}
											else
											{
												type = "R_386_16";
											}

											uint16_t _value;

											string section = (*ite)->section;

											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if (section == (*iter)->name)
												{
													_value = (*iter)->number;

													break;
												}
											}

											Relocation *_relocation = new Relocation((*it)->location_counter + 2, _value, type, (*it)->section_name);

											(*it)->relocations.push_back(_relocation);
										}
									}

									// relocatable symbol
								}
								else
								{
									if ((*ite)->scope == "GLOBAL")
									{
										if (temp.addressing_1 == "REG_IND_DISPL_PC")
										{
											if ((*it)->section_name == (*ite)->section)
											{
												relocation = false;

												int16_t val = (*ite)->value - ((*it)->location_counter + 4);

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = -2;

												out += Lexer::to_hex(val);
											}
										}
										else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
										{
											int8_t val = 0;

											out += Lexer::to_hex(val);
										}
										else
										{
											int16_t val = 0;

											out += Lexer::to_hex(val);
										}
									}
									else if ((*ite)->scope == "LOCAL")
									{
										if (temp.addressing_1 == "REG_IND_DISPL_PC")
										{
											if ((*it)->section_name == (*ite)->section)
											{
												relocation = false;

												int16_t val = (*ite)->value - ((*it)->location_counter + 4);

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = (*ite)->value - 2;

												out += Lexer::to_hex(val);
											}
										}
										else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
										{
											int8_t val = (int8_t) ((*ite)->value & 0xFF);

											out += Lexer::to_hex(val);
										}
										else
										{
											int16_t val = (int16_t) ((*ite)->value);

											out += Lexer::to_hex(val);
										}
									}

									(*it)->machine_code = out;

									// relocation

									if (relocation)
									{
										if ((*ite)->scope == "GLOBAL")
										{
											string type;

											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												type = "R_386_PC16";
											}
											else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
											{
												type = "R_386_8";
											}
											else
											{
												type = "R_386_16";
											}

											Relocation *_relocation = new Relocation((*it)->location_counter + 2, (*ite)->number, type, (*it)->section_name);

											(*it)->relocations.push_back(_relocation);
										}
										else if ((*ite)->scope == "LOCAL")
										{
											string type;

											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												type = "R_386_PC16";
											}
											else if (!temp.word && temp.addressing_1 == "IMMEDIATE")
											{
												type = "R_386_8";
											}
											else
											{
												type = "R_386_16";
											}

											uint16_t _value;

											string section = (*ite)->section;

											for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
											{
												if (section == (*iter)->name)
												{
													_value = (*iter)->number;

													break;
												}
											}

											Relocation *_relocation = new Relocation((*it)->location_counter + 2, _value, type, (*it)->section_name);

											(*it)->relocations.push_back(_relocation);
										}
									}
								}

								break;
							}
						}
					}
				}
				else if (!temp.is_jmp)
				{
					regex symbol("\\$?[_a-zA-Z]\\w*(\\(%(r[1-7]|pc|sp|psw)[hl]?\\))?");
					regex literal("\\$?-?\\d+(\\(%(r[1-7]|pc)[hl]?\\))?");

					bool word = temp.word;

					bool relocation = true;

					bool op1_symbol = false;
					bool op2_symbol = false;

					if (regex_match(temp.symbol_1, symbol))
					{
						op1_symbol = true;
					}

					if (regex_match(temp.symbol_2, symbol))
					{
						op2_symbol = true;
					}

					// first byte

					string out;

					uint8_t byte = 0;

					uint8_t op_code = 0;

					for (const pair<string, instruction_data>& itr : Parser::instructions)
					{
						if (regex_match((*it)->tokens.front(), itr.second.pattern))
						{
							byte = itr.second.op_code;
							break;
						}
					}

					op_code += (byte << 3);

					if (word)
					{
						op_code += (0x1 << 2);
					}

					out = Lexer::to_hex(op_code);
					out += " ";

					op_code = byte = 0;

					// first byte

					// second byte

					if (op1_symbol)
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_not_jmp_symbol)
						{
							if (temp.addressing_1 == i.first)
							{
								byte = i.second.op_code;
								break;
							}
						}
					}
					else
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_not_jmp_literal)
						{
							if (temp.addressing_1 == i.first)
							{
								byte = i.second.op_code;

								break;
							}
						}
					}

					op_code += (byte << 5);

					uint8_t reg_num = 0;

					reg_num += temp.reg_num_1;

					op_code += (reg_num << 1);

					if (!word)
					{
						if (temp.addressing_1 == "REG_DIR")
						{
							if (temp.reg_1_h_l == 'h')
							{
								op_code += 0x1;
							}
						}
					}

					out += Lexer::to_hex(op_code);
					out += " ";

					// second byte

					uint16_t offset = 2;

					// third and fourth byte

					if (op1_symbol)
					{
						for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
						{
							if ((*ite)->name == temp.symbol_1)
							{
								if ((*ite)->absolute)
								{
									if ((*it)->tokens.front() == "div" || (*it)->tokens.front() == "divw" || (*it)->tokens.front() == "divb")
									{
										if ((*ite)->value == 0 && temp.addressing_1 == "IMMEDIATE")
										{
											throw Error("Error: dividing by zero.");
										}
									}

									if (!word && temp.addressing_1 == "IMMEDIATE")
									{
										int8_t val = (int8_t) ((*ite)->value & 0xFF);

										out += Lexer::to_hex(val);
										out += " ";

										offset++;
									}
									else
									{
										int16_t val = (int16_t) ((*ite)->value);

										out += Lexer::to_hex(val);
										out += " ";

										offset += 2;
									}

									break;
								}
								else
								{
									if ((*ite)->equ_relocatable)
									{
										// relocatable symbol

										if ((*ite)->scope == "GLOBAL")
										{
											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op2_bytes)
													{
														lc_offset += 2;
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else if ((*ite)->is_extern)
												{
													int16_t val = 0;

													if (temp.op2_bytes)
													{
														val = (*ite)->value - 5;
													}
													else
													{
														val = (*ite)->value - 3;
													}

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = 0;

													if (temp.op2_bytes)
													{
														val = -5;
													}
													else
													{
														val = -3;
													}

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset += 2;
											}
											else if (!word && temp.addressing_1 == "IMMEDIATE")
											{
												int8_t val = 0;

												if ((*ite)->is_extern)
												{
													val = (int8_t) ((*ite)->value & 0xFF);

													out += Lexer::to_hex(val);
												}
												else
												{
													val = 0;

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset++;
											}
											else
											{
												int16_t val = 0;

												if ((*ite)->is_extern)
												{
													val = (*ite)->value;

													out += Lexer::to_hex(val);
												}
												else
												{
													val = 0;

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset += 2;
											}
										}
										else if ((*ite)->scope == "LOCAL")
										{
											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op2_bytes)
													{
														lc_offset += 2;
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = 0;

													if (temp.op2_bytes)
													{
														val = (*ite)->value - 5;
													}
													else
													{
														val = (*ite)->value - 3;
													}

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset += 2;
											}
											else if (!word && temp.addressing_1 == "IMMEDIATE")
											{
												int8_t val = (int8_t) ((*ite)->value & 0xFF);

												out += Lexer::to_hex(val);
												out += " ";

												offset++;
											}
											else
											{
												int16_t val = (int16_t) ((*ite)->value);

												out += Lexer::to_hex(val);
												out += " ";

												offset += 2;
											}
										}

										// relocation

										if (relocation)
										{
											if ((*ite)->scope == "GLOBAL")
											{
												string type;

												if (temp.addressing_1 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_1 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t value = 0;

												string symbol = (*ite)->relative_relocatable_symbol;

												string equ_rel;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (temp.symbol_1 == (*iter)->name)
													{
														if ((*iter)->is_extern)
														{
															equ_rel = (*iter)->relative_relocatable_symbol;

															for (list<Symbol*>::iterator itera = Assembler::symbol_table.begin(); itera != Assembler::symbol_table.end(); itera++)
															{
																if ((*itera)->name == equ_rel)
																{
																	value = (*itera)->number;

																	break;
																}
															}
														}
														else
														{
															value = (*iter)->number;
														}

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + 2, value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
											else if ((*ite)->scope == "LOCAL")
											{
												string type;

												if (temp.addressing_1 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_1 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t _value;

												string section = (*ite)->section;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (section == (*iter)->name)
													{
														_value = (*iter)->number;

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + 2, _value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
										}

										// relocatable symbol
									}
									else
									{
										if ((*ite)->scope == "GLOBAL")
										{
											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op2_bytes)
													{
														lc_offset += 2;
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = 0;

													if (temp.op2_bytes)
													{
														val = -5;
													}
													else
													{
														val = -3;
													}

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset += 2;
											}
											else if (!word && temp.addressing_1 == "IMMEDIATE")
											{
												int8_t val = 0;

												out += Lexer::to_hex(val);
												out += " ";

												offset++;
											}
											else
											{
												int16_t val = 0;

												out += Lexer::to_hex(val);
												out += " ";

												offset += 2;
											}
										}
										else if ((*ite)->scope == "LOCAL")
										{
											if (temp.addressing_1 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op2_bytes)
													{
														lc_offset += 2;
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = 0;

													if (temp.op2_bytes)
													{
														val = (*ite)->value - 5;
													}
													else
													{
														val = (*ite)->value - 3;
													}

													out += Lexer::to_hex(val);
												}

												out += " ";

												offset += 2;
											}
											else if (!word && temp.addressing_1 == "IMMEDIATE")
											{
												int8_t val = (int8_t) ((*ite)->value & 0xFF);

												out += Lexer::to_hex(val);
												out += " ";

												offset++;
											}
											else
											{
												int16_t val = (int16_t) ((*ite)->value);

												out += Lexer::to_hex(val);
												out += " ";

												offset += 2;
											}
										}

										// relocation

										if (relocation)
										{
											if ((*ite)->scope == "GLOBAL")
											{
												string type;

												if (temp.addressing_1 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_1 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + 2, (*ite)->number, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
											else if ((*ite)->scope == "LOCAL")
											{
												string type;

												if (temp.addressing_1 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_1 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t _value;

												string section = (*ite)->section;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (section == (*iter)->name)
													{
														_value = (*iter)->number;

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + 2, _value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
										}
									}

									break;
								}
							}
						}
					}
					else
					{
						if (temp.op1_bytes)
						{
							if (!word && temp.addressing_1 == "IMMEDIATE")
							{
								int8_t val = (int8_t) (stoi(temp.symbol_1) & 0xFF);

								out += Lexer::to_hex(val);
								out += " ";

								offset++;
							}
							else
							{
								int16_t val = (int16_t) (stoi(temp.symbol_1) & 0xFFFF);

								out += Lexer::to_hex(val);
								out += " ";

								offset += 2;
							}
						}
					}

					// third and fourth byte

					offset++;

					// fifth byte

					if (op2_symbol)
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_not_jmp_symbol)
						{
							if (temp.addressing_2 == i.first)
							{
								byte = i.second.op_code;
								break;
							}
						}
					}
					else
					{
						for (const pair<string, addressing_data>& i : Parser::addressing_types_not_jmp_literal)
						{
							if (temp.addressing_2 == i.first)
							{
								byte = i.second.op_code;

								break;
							}
						}
					}

					op_code = (byte << 5);

					reg_num = 0;

					reg_num += temp.reg_num_2;

					op_code += (reg_num << 1);

					if (!word)
					{
						if (temp.addressing_2 == "REG_DIR")
						{
							if (temp.reg_2_h_l == 'h')
							{
								op_code += 0x1;
							}
						}
					}

					out += Lexer::to_hex(op_code);
					out += " ";

					// fifth byte

					//sixth and seventh byte

					if (op2_symbol)
					{
						for (list<Symbol*>::iterator ite = Assembler::symbol_table.begin(); ite != Assembler::symbol_table.end(); ite++)
						{
							if ((*ite)->name == temp.symbol_2)
							{
								if ((*ite)->absolute)
								{
									if (!word && temp.addressing_2 == "IMMEDIATE")
									{
										int8_t val = (int8_t) ((*ite)->value & 0xFF);

										out += Lexer::to_hex(val);
									}
									else
									{
										int16_t val = (int16_t) ((*ite)->value);

										out += Lexer::to_hex(val);
									}

									break;
								}
								else
								{
									if ((*ite)->equ_relocatable)
									{
										// relocatable symbol

										if ((*ite)->scope == "GLOBAL")
										{
											if (temp.addressing_2 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op1_bytes)
													{
														if (!temp.word && temp.addressing_1 == "IMMEDIATE")
														{
															lc_offset++;
														}
														else
														{
															lc_offset += 2;
														}
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else if ((*ite)->is_extern)
												{
													int16_t val = (*ite)->value - 2;

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = -2;

													out += Lexer::to_hex(val);
												}
											}
											else
											{
												int16_t val = 0;

												if ((*ite)->is_extern)
												{
													val = (*ite)->value;

													out += Lexer::to_hex(val);
												}
												else
												{
													val = 0;

													out += Lexer::to_hex(val);
												}
											}
										}
										else if ((*ite)->scope == "LOCAL")
										{
											if (temp.addressing_2 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op1_bytes)
													{
														if (!temp.word && temp.addressing_1 == "IMMEDIATE")
														{
															lc_offset++;
														}
														else
														{
															lc_offset += 2;
														}
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = (*ite)->value - 2;

													out += Lexer::to_hex(val);
												}
											}
											else
											{
												int16_t val = (int16_t) ((*ite)->value);

												out += Lexer::to_hex(val);
											}
										}

										// relocation

										if (relocation)
										{
											if ((*ite)->scope == "GLOBAL")
											{
												string type;

												if (temp.addressing_2 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t value = 0;

												string symbol = (*ite)->relative_relocatable_symbol;

												string equ_rel;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (temp.symbol_2 == (*iter)->name)
													{
														if ((*iter)->is_extern)
														{
															equ_rel = (*iter)->relative_relocatable_symbol;

															for (list<Symbol*>::iterator itera = Assembler::symbol_table.begin(); itera != Assembler::symbol_table.end(); itera++)
															{
																if ((*itera)->name == equ_rel)
																{
																	value = (*itera)->number;

																	break;
																}
															}
														}
														else
														{
															value = (*iter)->number;
														}

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + offset, value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
											else if ((*ite)->scope == "LOCAL")
											{
												string type;

												if (temp.addressing_2 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t _value;

												string section = (*ite)->section;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (section == (*iter)->name)
													{
														_value = (*iter)->number;

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + offset, _value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
										}

										// relocatable symbol
									}
									else
									{
										if ((*ite)->scope == "GLOBAL")
										{
											if (temp.addressing_2 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op1_bytes)
													{
														if (!temp.word && temp.addressing_1 == "IMMEDIATE")
														{
															lc_offset++;
														}
														else
														{
															lc_offset += 2;
														}
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = -2;

													out += Lexer::to_hex(val);
												}
											}
											else if (!word && temp.addressing_2 == "IMMEDIATE")
											{
												int8_t val = 0;

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = 0;

												out += Lexer::to_hex(val);
											}
										}
										else if ((*ite)->scope == "LOCAL")
										{
											if (temp.addressing_2 == "REG_IND_DISPL_PC")
											{
												if ((*it)->section_name == (*ite)->section)
												{
													uint16_t lc_offset = 5;

													if (temp.op1_bytes)
													{
														if (!temp.word && temp.addressing_1 == "IMMEDIATE")
														{
															lc_offset++;
														}
														else
														{
															lc_offset += 2;
														}
													}

													relocation = false;

													int16_t val = (*ite)->value - ((*it)->location_counter + lc_offset);

													out += Lexer::to_hex(val);
												}
												else
												{
													int16_t val = (*ite)->value - 2;

													out += Lexer::to_hex(val);
												}
											}
											else if (!word && temp.addressing_2 == "IMMEDIATE")
											{
												int8_t val = (int8_t) ((*ite)->value & 0xFF);

												out += Lexer::to_hex(val);
											}
											else
											{
												int16_t val = (int16_t) ((*ite)->value);

												out += Lexer::to_hex(val);
											}
										}

										// relocation

										if (relocation)
										{
											if ((*ite)->scope == "GLOBAL")
											{
												string type;

												if (temp.addressing_2 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_2 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + offset, (*ite)->number, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
											else if ((*ite)->scope == "LOCAL")
											{
												string type;

												if (temp.addressing_2 == "REG_IND_DISPL_PC")
												{
													type = "R_386_PC16";
												}
												else if (!word && temp.addressing_2 == "IMMEDIATE")
												{
													type = "R_386_8";
												}
												else
												{
													type = "R_386_16";
												}

												uint16_t _value;

												string section = (*ite)->section;

												for (list<Symbol*>::iterator iter = Assembler::symbol_table.begin(); iter != Assembler::symbol_table.end(); iter++)
												{
													if (section == (*iter)->name)
													{
														_value = (*iter)->number;

														break;
													}
												}

												Relocation *_relocation = new Relocation((*it)->location_counter + offset, _value, type, (*it)->section_name);

												(*it)->relocations.push_back(_relocation);
											}
										}
									}

									break;
								}
							}
						}
					}
					else
					{
						if (temp.op2_bytes)
						{
							if (!word && temp.addressing_2 == "IMMEDIATE")
							{
								int8_t val = (int8_t) (stoi(temp.symbol_2) & 0xFF);

								out += Lexer::to_hex(val);
							}
							else
							{
								int16_t val = (int16_t) (stoi(temp.symbol_2) & 0xFFFF);

								out += Lexer::to_hex(val);
							}
						}
					}

					// sixth and seventh byte

					if (out.back() == ' ')
					{
						out.pop_back();
					}

					(*it)->machine_code = out;
				}
			}
		}
	}
}

void Assembler::single_pass()
{
	if (!tokens.size())
	{
		throw Error("ERROR: Input file contains no assembly code.");
	}

	Symbol::create_section(".und");
	Symbol::create_section(".abs");

	for (size_t i = 0; i < tokens.size(); i++)
	{
		queue<string> assembler_tokens;

		if (!tokens[i].size())
		{
			continue;
		}

		for (size_t j = 0; j < tokens[i].size(); j++)
		{
			Lexer::to_lower(tokens[i][j]);
			assembler_tokens.push(tokens[i][j]);
		}

		string current = assembler_tokens.front();
		assembler_tokens.pop();

		if (Parser::resolve_type(current) == "LABEL") // label
		{
			if (current_section->name == ".und" || current_section->name == ".abs")
			{
				throw Error("Error: symbol cannot be defined inside .und section or .abs section.");
			}

			// remove the ':' sign at the end of the label

			size_t colon = current.find(Lexer::sequences[Lexer::_colon]);

			if (colon != string::npos)
			{
				current.erase(colon);
			}

			if (Symbol::defined_in_symbol_table(current))
			{
				string error = "Error: symbol " + current + " is already defined.";

				throw Error(error);
			}
			else if (Symbol::declared_in_symbol_table(current))
			{
				Symbol::define_symbol(current, Assembler::location_counter);
			}
			else if (!Symbol::exists_in_symbol_table(current))
			{
				Symbol::create_symbol(current, Assembler::location_counter);
				Symbol::define_symbol(current);
			}

			if (!assembler_tokens.size())
			{
				continue;
			}

			current = assembler_tokens.front();
			assembler_tokens.pop();
		}

		// continue the execution

		if (Parser::resolve_type(current) == "LABEL") // label
		{
			throw Error("Error: syntax not allowed -> label after label.");
		}
		else if (Parser::resolve_type(current) == "INSTRUCTION") // instruction 
		{
			if (current_section->name == ".und")
			{
				throw Error("Error: instruction cannot be located inside .und section.");
			}

			string mnemonic = current;

			for (const pair<string, instruction_data>& it : Parser::instructions)
			{
				if (regex_match(current, it.second.pattern))
				{
					mnemonic = it.first;
					break;
				}
			}

			if (assembler_tokens.size() != Parser::instructions[mnemonic].num_of_operands)
			{
				string error = "Error: wrong number of operands -> " + mnemonic + " instruction.";

				throw Error(error);
			}

			// continue

			queue<string> instruction_tokens;

			instruction_tokens.push(current);

			if (Parser::resolve_instruction_type(current) == "NOT_JMP") // not jump instructions
			{
				regex num_of_operands_zero("halt|iret|ret");
				regex num_of_operands_two("xchg[bw]?|mov[bw]?|add[bw]?|sub[bw]?|mul[bw]?|div[bw]?|cmp[bw]?|not[bw]?|and[bw]?|or[bw]?|xor[bw]?|test[bw]?|shl[bw]?|shr[bw]?|mod[bw]?");

				if (regex_match(current, num_of_operands_zero))
				{
					string machine_code;

					uint8_t byte = 0;

					uint8_t op_code = 0;

					for (const pair<string, instruction_data>& it : Parser::instructions)
					{
						if (regex_match(current, it.second.pattern))
						{
							byte = it.second.op_code;
							break;
						}
					}

					op_code += (byte << 3);

					Data *_data = new Data(instruction_tokens, Lexer::to_hex(op_code), true, false);
					Assembler::data.push_back(_data);

					Assembler::location_counter += 1;

					Assembler::current_section->size += 1;
				}
				else if (regex_match(current, num_of_operands_two))
				{
					regex symbol("\\$?[_a-zA-Z]\\w*(\\(%(r[1-7]|pc|sp|psw)[hl]?\\))?");
					regex literal("\\$?-?((0x[a-f0-9]+)?|\\d+)(\\(%(r[1-7]|pc|sp|psw)[hl]?\\))?");

					uint8_t operand_size = 2;

					bool word = true;

					if (current.back() == 'b' && current != "sub")
					{
						operand_size = 1;

						word = false;
					}

					bool op1_symbol = false;
					bool op2_symbol = false;

					if (regex_match(assembler_tokens.front(), symbol))
					{
						op1_symbol = true;
					}
					else
					{
						if (Parser::resolve_addressing_not_jmp_literal(assembler_tokens.front()) == "NOT_RECOGNIZED")
						{
							string error = "Error: token type not recognized -> " + current + " instruction.";

							throw Error(error);
						}
					}

					if (regex_match(assembler_tokens.back(), symbol))
					{
						op2_symbol = true;
					}
					else
					{
						if (Parser::resolve_addressing_not_jmp_literal(assembler_tokens.back()) == "NOT_RECOGNIZED")
						{
							string error = "Error: token type not recognized -> " + current + " instruction.";

							throw Error(error);
						}
					}

					_instruction to_send;

					to_send.is_jmp = false;
					to_send.word = word;

					if (op1_symbol || op2_symbol)
					{
						uint16_t lc_increase = 3;

						bool op1_bytes = false;
						bool op2_bytes = false;

						instruction_tokens.push(assembler_tokens.front());
						instruction_tokens.push(assembler_tokens.back());

						if (op1_symbol)
						{
							string addressing;

							for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_symbol)
							{
								if (regex_match(assembler_tokens.front(), it.second.pattern))
								{
									op1_bytes = it.second.has_operand;
									addressing = it.first;

									to_send.addressing_1 = addressing;
									to_send.op1_bytes = op1_bytes;

									string op_copy = assembler_tokens.front();

									if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										string copy = assembler_tokens.front();

										copy.pop_back();

										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: r<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}

									uint8_t reg_num = 0;

									if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										size_t help = op_copy.find_first_of('(');

										string lit = op_copy;
										string lit1 = op_copy;

										lit = lit.substr(0, help);

										string temp = lit1.substr(help + 3, string::npos);

										temp.pop_back();

										if (temp == "p")
										{
											temp = "6";
										}
										else if (temp == "c")
										{
											temp = "7";
										}

										if (temp == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(temp) & 0xFF);
										}

										to_send.reg_num_1 = reg_num;

										to_send.symbol_1 = lit;

										if (!Symbol::exists_in_symbol_table(lit))
										{
											Symbol::create_symbol(lit);
										}
									}
									else if (addressing == "IMMEDIATE")
									{
										if (current == "xchg" || current == "xchgb" || current == "xchgw")
										{
											throw Error("Error: operand(s) of XCHG instruction cannot be immediately addressed.");
										}

										op_copy.erase(0, 1);

										to_send.symbol_1 = op_copy;

										if (!Symbol::exists_in_symbol_table(to_send.symbol_1))
										{
											Symbol::create_symbol(to_send.symbol_1);
										}
									}
									else if (addressing == "MEMORY")
									{
										to_send.symbol_1 = op_copy;

										if (!Symbol::exists_in_symbol_table(to_send.symbol_1))
										{
											Symbol::create_symbol(to_send.symbol_1);
										}
									}

									break;
								}
							}
						}
						else
						{
							string addressing;

							for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
							{
								if (regex_match(assembler_tokens.front(), it.second.pattern))
								{
									op1_bytes = it.second.has_operand;
									addressing = it.first;

									to_send.addressing_1 = addressing;
									to_send.op1_bytes = op1_bytes;

									string op_copy = assembler_tokens.front();

									if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										string copy = assembler_tokens.front();

										copy.pop_back();

										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}

									if (word) // error check
									{
										string copy = assembler_tokens.front();

										if (addressing == "REG_DIR")
										{
											if (copy.back() == 'h' || copy.back() == 'l')
											{
												throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
											}
										}
									}

									uint8_t reg_num = 0;

									if (addressing == "REG_DIR")
									{
										op_copy.erase(0, 2);

										if (op_copy[0] == 'p')
										{
											op_copy[0] = '6';
										}
										else if (op_copy[0] == 'c')
										{
											op_copy[0] = '7';
										}

										if (op_copy == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
										}

										to_send.reg_num_1 = reg_num;

										if (!word)
										{
											if (op_copy.back() == 'h' || op_copy.back() == 'l')
											{
												to_send.reg_1_h_l = op_copy.back();
											}
											else
											{
												string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

												throw Error(error);
											}
										}
									}
									else if (addressing == "REG_IND")
									{
										op_copy.erase(0, 3);

										op_copy.pop_back();

										if (op_copy == "p")
										{
											op_copy = "6";
										}
										else if (op_copy == "c")
										{
											op_copy = "7";
										}

										if (op_copy == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
										}

										to_send.reg_num_1 = reg_num;
									}
									else if (addressing == "REG_IND_DISPL")
									{
										size_t help = op_copy.find_first_of('(');

										string lit = op_copy;
										string lit1 = op_copy;

										lit = lit.substr(0, help);

										if (lit[0] == '0' && lit[1] == 'x')
										{
											if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
											else
											{
												lit = to_string(strtol(lit.c_str(), nullptr, 0));
											}
										}
										else
										{
											if (stoi(lit) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
										}

										string temp = lit1.substr(help + 3, string::npos);

										temp.pop_back();

										if (temp == "p")
										{
											temp = "6";
										}
										else if (temp == "c")
										{
											temp = "7";
										}

										if (temp == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(temp) & 0xFF);
										}

										to_send.reg_num_1 = reg_num;

										to_send.symbol_1 = lit;
									}
									else if (addressing == "IMMEDIATE")
									{
										if (current == "xchg" || current == "xchgb" || current == "xchgw")
										{
											throw Error("Error: operand(s) of XCHG instruction cannot be immediately addressed.");
										}

										op_copy.erase(0, 1);

										if ((op_copy[0] == '0' && op_copy[1] == 'x') || op_copy[1] == '0' && op_copy[2] == 'x')
										{
											if (strtol(op_copy.c_str(), nullptr, 0) > 0xFFFF)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
											else
											{
												op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
											}
										}

										if (word && stol(op_copy) > 0xFFFF)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else if (!word && stol(op_copy) > 0xFF)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}

										to_send.symbol_1 = op_copy;

										if (current == "div" || current == "divb" || current == "divw")
										{
											if (op_copy == "0")
											{
												throw Error("Error: dividing by zero.");
											}
										}
									}
									else if (addressing == "MEMORY")
									{
										if (op_copy[0] == '0' && op_copy[1] == 'x')
										{
											if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
											else
											{
												op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
											}
										}
										else
										{
											if (stoi(op_copy) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
										}

										to_send.symbol_1 = op_copy;
									}

									break;
								}
							}
						}

						if (op2_symbol)
						{
							string addressing;

							for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_symbol)
							{
								if (regex_match(assembler_tokens.back(), it.second.pattern))
								{
									op2_bytes = it.second.has_operand;
									addressing = it.first;

									to_send.addressing_2 = addressing;
									to_send.op2_bytes = op2_bytes;

									string op_copy = assembler_tokens.back();

									if (word) // error check
									{
										string copy = assembler_tokens.back();

										if (addressing == "REG_DIR")
										{
											if (copy.back() == 'h' || copy.back() == 'l')
											{
												throw Error("Error: reg<num>(h/l) only allowed when operand size is one byte.");
											}
										}
										else if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
										{
											copy.pop_back();

											if (copy.back() == 'h' || copy.back() == 'l')
											{
												throw Error("Error: reg<num>(h/l) only allowed when operand size is one byte.");
											}
										}
									}

									uint8_t reg_num = 0;

									if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										size_t help = op_copy.find_first_of('(');

										string lit = op_copy;
										string lit1 = op_copy;

										lit = lit.substr(0, help);

										string temp = lit1.substr(help + 3, string::npos);

										temp.pop_back();

										if (temp == "p")
										{
											temp = "6";
										}
										else if (temp == "c")
										{
											temp = "7";
										}

										if (temp == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(temp) & 0xFF);
										}

										to_send.reg_num_2 = reg_num;

										to_send.symbol_2 = lit;

										if (!Symbol::exists_in_symbol_table(to_send.symbol_2))
										{
											Symbol::create_symbol(to_send.symbol_2);
										}
									}
									else if (addressing == "IMMEDIATE")
									{
										queue<string> copy = instruction_tokens;

										copy.pop();

										string error = "Error: destination operand cannot be immediately addressed -> " + current + " " + copy.front() + ", " + copy.back() + " instruction.";

										throw Error(error);
									}
									else if (addressing == "MEMORY")
									{
										to_send.symbol_2 = op_copy;

										if (!Symbol::exists_in_symbol_table(to_send.symbol_2))
										{
											Symbol::create_symbol(to_send.symbol_2);
										}
									}

									break;
								}
							}
						}
						else
						{
							string addressing;

							for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
							{
								if (regex_match(assembler_tokens.back(), it.second.pattern))
								{
									op2_bytes = it.second.has_operand;
									addressing = it.first;

									to_send.addressing_2 = addressing;
									to_send.op2_bytes = op2_bytes;

									string op_copy = assembler_tokens.back();

									if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										string copy = assembler_tokens.back();

										copy.pop_back();

										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}

									if (word) // error check
									{
										string copy = assembler_tokens.back();

										if (addressing == "REG_DIR")
										{
											if (copy.back() == 'h' || copy.back() == 'l')
											{
												throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
											}
										}
									}

									uint8_t reg_num = 0;

									if (addressing == "REG_DIR")
									{
										op_copy.erase(0, 2);

										if (op_copy[0] == 'p')
										{
											op_copy[0] = '6';
										}
										else if (op_copy[0] == 'c')
										{
											op_copy[0] = '7';
										}

										if (op_copy == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
										}

										to_send.reg_num_2 = reg_num;

										if (!word)
										{
											if (op_copy.back() == 'h' || op_copy.back() == 'l')
											{
												to_send.reg_2_h_l = op_copy.back();
											}
											else
											{
												string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

												throw Error(error);
											}
										}
									}
									else if (addressing == "REG_IND")
									{
										op_copy.erase(0, 3);

										op_copy.pop_back();

										if (op_copy == "p")
										{
											op_copy = "6";
										}
										else if (op_copy == "c")
										{
											op_copy = "7";
										}

										if (op_copy == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
										}

										to_send.reg_num_2 = reg_num;
									}
									else if (addressing == "REG_IND_DISPL")
									{
										size_t help = op_copy.find_first_of('(');

										string lit = op_copy;
										string lit1 = op_copy;

										lit = lit.substr(0, help);

										if (lit[0] == '0' && lit[1] == 'x')
										{
											if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
											else
											{
												lit = to_string(strtol(lit.c_str(), nullptr, 0));
											}
										}
										else
										{
											if (stoi(lit) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
										}

										string temp = lit1.substr(help + 3, string::npos);

										temp.pop_back();

										if (temp == "p")
										{
											temp = "6";
										}
										else if (temp == "c")
										{
											temp = "7";
										}

										if (temp == "sw")
										{
											reg_num = 0xF;
										}
										else
										{
											reg_num = (uint8_t) (stoi(temp) & 0xFF);
										}

										to_send.reg_num_2 = reg_num;

										to_send.symbol_2 = lit;
									}
									else if (addressing == "IMMEDIATE")
									{
										queue<string> q = instruction_tokens;
										q.pop();

										string error = "Error: destination operand cannot be immediately addressed -> " + instruction_tokens.front() + " " + q.front() + ", " + q.back() + " instruction.";

										throw Error(error);

									}
									else if (addressing == "MEMORY")
									{
										if (op_copy[0] == '0' && op_copy[1] == 'x')
										{
											if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
											else
											{
												op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
											}
										}
										else
										{
											if (stoi(op_copy) & 0xFFFF0000)
											{
												string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

												throw Error(error);
											}
										}

										to_send.symbol_2 = op_copy;
									}

									break;
								}
							}
						}

						if (op1_bytes)
						{
							if (!word && to_send.addressing_1 == "IMMEDIATE")
							{
								lc_increase += 1;
							}
							else
							{
								lc_increase += 2;
							}
						}

						if (op2_bytes)
						{
							lc_increase += 2;
						}

						Data *_data = new Data(instruction_tokens, to_send, " ", false, false);
						Assembler::data.push_back(_data);

						Assembler::location_counter += lc_increase;

						Assembler::current_section->size += lc_increase;

						assembler_tokens.pop();
						assembler_tokens.pop();
					}
					else
					{
						uint16_t lc_increase = 3;

						bool op1_bytes = false;
						bool op2_bytes = false;

						instruction_tokens.push(assembler_tokens.front());
						instruction_tokens.push(assembler_tokens.back());

						string machine_code;

						uint8_t byte = 0;

						uint8_t op_code = 0;

						for (const pair<string, instruction_data>& it : Parser::instructions)
						{
							if (regex_match(current, it.second.pattern))
							{
								byte = it.second.op_code;
								break;
							}
						}

						op_code += (byte << 3);

						if (word)
						{
							op_code += (0x1 << 2);
						}

						machine_code = Lexer::to_hex(op_code);
						machine_code += " ";

						// second byte - start

						uint8_t reg_num = 0;

						op_code = byte = 0;

						string addressing;

						for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
						{
							if (regex_match(assembler_tokens.front(), it.second.pattern))
							{
								byte = it.second.op_code; // addressing op_code
								op1_bytes = it.second.has_operand; // does it contain bytes
								addressing = it.first;

								string op_copy = assembler_tokens.front();

								if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									string copy = assembler_tokens.front();

									copy.pop_back();

									if (copy.back() == 'h' || copy.back() == 'l')
									{
										throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
									}
								}

								if (word) // error check
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								op_code += (byte << 5);

								int value = 0;

								if (addressing == "REG_DIR")
								{
									op_copy.erase(0, 2);

									if (op_copy[0] == 'p')
									{
										op_copy[0] = '6';
									}
									else if (op_copy[0] == 'c')
									{
										op_copy[0] = '7';
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}

									if (!word)
									{
										if (op_copy.back() != 'h' && op_copy.back() != 'l')
										{
											string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

											throw Error(error);
										}
									}
								}
								else if (addressing == "REG_IND")
								{
									op_copy.erase(0, 3);

									op_copy.pop_back();

									if (op_copy == "p")
									{
										op_copy = "6";
									}
									else if (op_copy == "c")
									{
										op_copy = "7";
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}
								}
								else if (addressing == "REG_IND_DISPL")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(0, help);

									if (lit[0] == '0' && lit[1] == 'x')
									{
										if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											lit = to_string(strtol(lit.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(lit) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									value = stoi(lit);
								}
								else if (addressing == "IMMEDIATE")
								{
									if (current == "xchg" || current == "xchgb" || current == "xchgw")
									{
										throw Error("Error: operand(s) of XCHG instruction cannot be immediately addressed.");
									}

									op_copy.erase(0, 1);

									if ((op_copy[0] == '0' && op_copy[1] == 'x') || op_copy[1] == '0' && op_copy[2] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) > 0xFFFF)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}

									if (word && stol(op_copy) > 0xFFFF)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}
									else if (!word && stol(op_copy) > 0xFF)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}

									value = stoi(op_copy);

									if (current == "div" || current == "divb" || current == "divw")
									{
										if (op_copy == "0")
										{
											throw Error("Error: dividing by zero.");
										}
									}
								}
								else if (addressing == "MEMORY")
								{
									if (op_copy[0] == '0' && op_copy[1] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(op_copy) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									value = stoi(op_copy);
								}

								op_code += (reg_num << 1);

								if (!word)
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h')
										{
											op_code += 0x1;
										}
									}
								}

								machine_code += Lexer::to_hex(op_code);
								machine_code += " ";

								if (op1_bytes)
								{
									if (!word && addressing == "IMMEDIATE")
									{
										int8_t val = (int8_t) (value & 0xFF);

										machine_code += Lexer::to_hex(val);
										machine_code += " ";

										lc_increase++;
									}
									else
									{
										int16_t val = (int16_t) (value & 0xFFFF);

										machine_code += Lexer::to_hex(val);
										machine_code += " ";

										lc_increase += 2;
									}
								}

								break;
							}
						}

						// second byte - end

						// third byte - start

						reg_num = 0;

						op_code = byte = 0;

						for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
						{
							if (regex_match(assembler_tokens.back(), it.second.pattern))
							{
								byte = it.second.op_code; // addressing op_code
								op2_bytes = it.second.has_operand; // does it contain bytes
								addressing = it.first;

								string op_copy = assembler_tokens.back();

								if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									string copy = assembler_tokens.back();

									copy.pop_back();

									if (copy.back() == 'h' || copy.back() == 'l')
									{
										throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
									}
								}

								if (word) // error check
								{
									string copy = assembler_tokens.back();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								op_code += (byte << 5);

								int value = 0;

								if (addressing == "REG_DIR")
								{
									op_copy.erase(0, 2);

									if (op_copy[0] == 'p')
									{
										op_copy[0] = '6';
									}
									else if (op_copy[0] == 'c')
									{
										op_copy[0] = '7';
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}

									if (!word)
									{
										if (op_copy.back() != 'h' && op_copy.back() != 'l')
										{
											string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

											throw Error(error);
										}
									}
								}
								else if (addressing == "REG_IND")
								{
									op_copy.erase(0, 3);

									op_copy.pop_back();

									if (op_copy == "p")
									{
										op_copy = "6";
									}
									else if (op_copy == "c")
									{
										op_copy = "7";
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}
								}
								else if (addressing == "REG_IND_DISPL")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(0, help);

									if (lit[0] == '0' && lit[1] == 'x')
									{
										if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											lit = to_string(strtol(lit.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(lit) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									value = stoi(lit);
								}
								else if (addressing == "IMMEDIATE")
								{
									queue<string> q = instruction_tokens;
									q.pop();

									string error = "Error: destination operand cannot be immediately addressed -> " + instruction_tokens.front() + " " + q.front() + ", " + q.back() + " instruction.";
									throw Error(error);

								}
								else if (addressing == "MEMORY")
								{
									if (op_copy[0] == '0' && op_copy[1] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(op_copy) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									value = stoi(op_copy);
								}

								op_code += (reg_num << 1);

								if (!word)
								{
									string copy = assembler_tokens.back();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h')
										{
											op_code += 0x1;
										}
									}
								}

								machine_code += Lexer::to_hex(op_code);
								machine_code += " ";

								if (op2_bytes)
								{
									int16_t val = (int16_t) (value & 0xFFFF);

									machine_code += Lexer::to_hex(val);
									machine_code += " ";

									lc_increase += 2;
								}

								break;
							}
						}

						// third byte - end

						machine_code.pop_back();

						Data *_data = new Data(instruction_tokens, machine_code, true, false);
						Assembler::data.push_back(_data);

						Assembler::location_counter += lc_increase;

						Assembler::current_section->size += lc_increase;

						assembler_tokens.pop();
						assembler_tokens.pop();
					}
				}
			}
			else if (Parser::resolve_instruction_type(current) == "JMP") // jump instructions or push/pop
			{
				regex symbol("\\*?\\$?[_a-zA-Z]\\w*(\\(%(r[1-7]|pc|sp|psw)[hl]?\\))?");

				bool word = true;

				if (current.back() == 'b')
				{
					word = false;
				}

				bool op1_symbol = false;

				if (regex_match(assembler_tokens.front(), symbol))
				{
					op1_symbol = true;
				}
				else
				{
					if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
					{
						if (Parser::resolve_addressing_not_jmp_literal(assembler_tokens.front()) == "NOT_RECOGNIZED")
						{
							string error = "Error: token type not recognized -> " + current + " instruction.";

							throw Error(error);
						}
					}
					else
					{
						if (Parser::resolve_addressing_jmp_literal(assembler_tokens.front()) == "NOT_RECOGNIZED")
						{
							string error = "Error: token type not recognized -> " + current + " instruction.";

							throw Error(error);
						}
					}
				}

				if (op1_symbol)
				{
					uint16_t lc_increase = 2;

					bool op1_bytes = false;

					instruction_tokens.push(assembler_tokens.front());

					_instruction to_send;

					to_send.is_jmp = true;

					to_send.word = word;

					if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
					{
						to_send.push_or_pop = true;
					}

					string addressing;

					if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
					{
						// push pop

						for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_symbol)
						{
							if (regex_match(assembler_tokens.front(), it.second.pattern))
							{
								op1_bytes = it.second.has_operand;
								addressing = it.first;

								to_send.addressing_1 = addressing;
								to_send.op1_bytes = op1_bytes;

								string op_copy = assembler_tokens.front();

								if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
								{
									if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										string copy = assembler_tokens.front();

										copy.pop_back();

										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: r<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								uint8_t reg_num = 0;

								if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(0, help);

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									to_send.reg_num_1 = reg_num;

									to_send.symbol_1 = lit;

									if (!Symbol::exists_in_symbol_table(lit))
									{
										Symbol::create_symbol(lit);
									}
								}
								else if (addressing == "IMMEDIATE")
								{
									if (Parser::resolve_patch(current) == "POP")
									{
										throw Error("Error: operand of POP instruction cannot be immediately addressed.");
									}

									string symbol;

									to_send.symbol_1 = op_copy.substr(1, string::npos);

									symbol = to_send.symbol_1;

									if (!Symbol::exists_in_symbol_table(symbol))
									{
										Symbol::create_symbol(symbol);
									}
								}
								else if (addressing == "MEMORY")
								{
									to_send.symbol_1 = op_copy;

									if (!Symbol::exists_in_symbol_table(to_send.symbol_1))
									{
										Symbol::create_symbol(to_send.symbol_1);
									}
								}

								break;
							}
						}

						// push pop
					}
					else
					{
						for (const pair<string, addressing_data>& it : Parser::addressing_types_jmp_symbol)
						{
							if (regex_match(assembler_tokens.front(), it.second.pattern))
							{
								op1_bytes = it.second.has_operand;
								addressing = it.first;

								to_send.addressing_1 = addressing;
								to_send.op1_bytes = op1_bytes;

								string op_copy = assembler_tokens.front();

								if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
								{
									if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
									{
										string copy = assembler_tokens.front();

										copy.pop_back();

										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: r<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								uint8_t reg_num = 0;

								if (addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(1, help - 1);

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									to_send.reg_num_1 = reg_num;

									to_send.symbol_1 = lit;

									if (!Symbol::exists_in_symbol_table(lit))
									{
										Symbol::create_symbol(lit);
									}
								}
								else if (addressing == "IMMEDIATE")
								{
									string symbol;

									to_send.symbol_1 = op_copy;

									symbol = to_send.symbol_1;


									if (!Symbol::exists_in_symbol_table(symbol))
									{
										Symbol::create_symbol(symbol);
									}
								}
								else if (addressing == "MEMORY")
								{
									to_send.symbol_1 = op_copy.substr(1, string::npos);

									if (!Symbol::exists_in_symbol_table(to_send.symbol_1))
									{
										Symbol::create_symbol(to_send.symbol_1);
									}
								}

								break;
							}
						}
					}

					if (op1_bytes)
					{
						if (!word && addressing == "IMMEDIATE")
						{
							lc_increase++;
						}
						else
						{
							lc_increase += 2;
						}
					}

					Data *_data = new Data(instruction_tokens, to_send, " ", false, false);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase;

					Assembler::current_section->size += lc_increase;

					assembler_tokens.pop();
				}
				else
				{
					uint16_t lc_increase = 2;

					bool op1_bytes = false;

					instruction_tokens.push(assembler_tokens.back());

					string machine_code;

					uint8_t byte = 0;

					uint8_t op_code = 0;

					for (const pair<string, instruction_data>& it : Parser::instructions)
					{
						if (regex_match(current, it.second.pattern))
						{
							byte = it.second.op_code;
							break;
						}
					}

					op_code += (byte << 3);

					if (word)
					{
						op_code += (0x1 << 2);
					}

					machine_code = Lexer::to_hex(op_code);
					machine_code += " ";

					// second byte - start

					uint8_t reg_num = 0;

					op_code = byte = 0;

					string addressing;

					if (Parser::resolve_patch(current) == "PUSH" || Parser::resolve_patch(current) == "POP")
					{
						for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
						{
							if (regex_match(assembler_tokens.front(), it.second.pattern))
							{
								byte = it.second.op_code; // addressing op_code
								op1_bytes = it.second.has_operand; // does it contain bytes
								addressing = it.first;

								string op_copy = assembler_tokens.front();

								if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									string copy = assembler_tokens.front();

									copy.pop_back();

									if (copy.back() == 'h' || copy.back() == 'l')
									{
										throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
									}
								}

								if (word) // error check
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								op_code += (byte << 5);

								int value = 0;

								if (addressing == "REG_DIR")
								{
									op_copy.erase(0, 2);

									if (op_copy[0] == 'p')
									{
										op_copy[0] = '6';
									}
									else if (op_copy[0] == 'c')
									{
										op_copy[0] = '7';
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}

									if (!word)
									{
										if (op_copy.back() != 'h' && op_copy.back() != 'l')
										{
											string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

											throw Error(error);
										}
									}
								}
								else if (addressing == "REG_IND")
								{
									op_copy.erase(0, 3);

									op_copy.pop_back();

									if (op_copy == "p")
									{
										op_copy = "6";
									}
									else if (op_copy == "c")
									{
										op_copy = "7";
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}
								}
								else if (addressing == "REG_IND_DISPL")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(0, help);

									if (lit[0] == '0' && lit[1] == 'x')
									{
										if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											lit = to_string(strtol(lit.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(lit) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									value = stoi(lit);
								}
								else if (addressing == "IMMEDIATE")
								{
									if (Parser::resolve_patch(current) == "POP")
									{
										throw Error("Error: operand of POP instruction cannot be immediately addressed.");
									}

									op_copy = op_copy.substr(1, string::npos);

									if ((op_copy[0] == '0' && op_copy[1] == 'x') || op_copy[1] == '0' && op_copy[2] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) > 0xFFFF)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}

									if (word && stol(op_copy) > 0xFFFF)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}
									else if (!word && stol(op_copy) > 0xFF)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}

									value = stoi(op_copy);
								}
								else if (addressing == "MEMORY")
								{
									if (op_copy[0] == '0' && op_copy[1] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(op_copy) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									value = stoi(op_copy);
								}

								op_code += (reg_num << 1);

								if (!word)
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h')
										{
											op_code += 0x1;
										}
									}
								}

								machine_code += Lexer::to_hex(op_code);
								machine_code += " ";

								if (op1_bytes)
								{
									if (addressing == "IMMEDIATE" && !word)
									{
										int8_t val = (int8_t) (value & 0xFF);

										machine_code += Lexer::to_hex(val);

										lc_increase++;
									}
									else
									{
										int16_t val = (int16_t) (value & 0xFFFF);

										machine_code += Lexer::to_hex(val);

										lc_increase += 2;
									}

									machine_code += " ";
								}

								break;
							}
						}
					}
					else
					{
						for (const pair<string, addressing_data>& it : Parser::addressing_types_jmp_literal)
						{
							if (regex_match(assembler_tokens.front(), it.second.pattern))
							{
								byte = it.second.op_code; // addressing op_code
								op1_bytes = it.second.has_operand; // does it contain bytes
								addressing = it.first;

								string op_copy = assembler_tokens.front();

								if (addressing == "REG_IND" || addressing == "REG_IND_DISPL" || addressing == "REG_IND_DISPL_PC")
								{
									string copy = assembler_tokens.front();

									copy.pop_back();

									if (copy.back() == 'h' || copy.back() == 'l')
									{
										throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
									}
								}

								if (word) // error check
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h' || copy.back() == 'l')
										{
											throw Error("Error: reg<num>(h/l) is only allowed when operand size is one byte and with REG_DIR addressing.");
										}
									}
								}

								op_code += (byte << 5);

								int value = 0;

								if (addressing == "REG_DIR")
								{
									op_copy.erase(0, 3);

									if (op_copy[0] == 'p')
									{
										op_copy[0] = '6';
									}
									else if (op_copy[0] == 'c')
									{
										op_copy[0] = '7';
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}

									if (!word)
									{
										if (op_copy.back() != 'h' && op_copy.back() != 'l')
										{
											string error = "Error: higher or lower byte must be specified if instruction operand is one byte and if REG_DIR addressing is used.";

											throw Error(error);
										}
									}
								}
								else if (addressing == "REG_IND")
								{
									op_copy.erase(0, 4);

									op_copy.pop_back();

									if (op_copy[0] == 'p')
									{
										op_copy[0] = '6';
									}
									else if (op_copy[0] == 'c')
									{
										op_copy[0] = '7';
									}

									if (op_copy == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(op_copy) & 0xFF);
									}
								}
								else if (addressing == "REG_IND_DISPL")
								{
									size_t help = op_copy.find_first_of('(');

									string lit = op_copy;
									string lit1 = op_copy;

									lit = lit.substr(1, help - 1);

									if (lit[0] == '0' && lit[1] == 'x')
									{
										if (strtol(lit.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											lit = to_string(strtol(lit.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(lit) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									string temp = lit1.substr(help + 3, string::npos);

									temp.pop_back();

									if (temp == "p")
									{
										temp = "6";
									}
									else if (temp == "c")
									{
										temp = "7";
									}

									if (temp == "sw")
									{
										reg_num = 0xF;
									}
									else
									{
										reg_num = (uint8_t) (stoi(temp) & 0xFF);
									}

									value = stoi(lit);
								}
								else if (addressing == "IMMEDIATE")
								{
									if (op_copy[0] == '-')
									{
										string error = "Error: JMP instruction cannot have a negative immediate value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}

									if (op_copy[0] == '0' && op_copy[1] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) > 0xFFFF)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}

									if (word && stoul(op_copy) & 0xFFFF0000)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}
									else if (!word && stoul(op_copy) & 0xFF00)
									{
										string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

										throw Error(error);
									}

									value = stoi(op_copy);
								}
								else if (addressing == "MEMORY")
								{
									op_copy = op_copy.substr(1, string::npos);

									if (op_copy[0] == '0' && op_copy[1] == 'x')
									{
										if (strtol(op_copy.c_str(), nullptr, 0) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
										else
										{
											op_copy = to_string(strtol(op_copy.c_str(), nullptr, 0));
										}
									}
									else
									{
										if (stoi(op_copy) & 0xFFFF0000)
										{
											string error = "Error: operand size is greater than MAX value -> " + instruction_tokens.front() + " instruction.";

											throw Error(error);
										}
									}

									value = stoi(op_copy);
								}

								op_code += (reg_num << 1);

								if (!word)
								{
									string copy = assembler_tokens.front();

									if (addressing == "REG_DIR")
									{
										if (copy.back() == 'h')
										{
											op_code += 0x1;
										}
									}
								}

								machine_code += Lexer::to_hex(op_code);
								machine_code += " ";

								if (op1_bytes)
								{
									if (addressing == "IMMEDIATE" && !word)
									{
										int8_t val = (int8_t) (value & 0xFF);

										machine_code += Lexer::to_hex(val);

										lc_increase++;
									}
									else
									{
										int16_t val = (int16_t) (value & 0xFFFF);

										machine_code += Lexer::to_hex(val);

										lc_increase += 2;
									}

									machine_code += " ";
								}

								break;
							}
						}
					}

					// second byte - end

					machine_code.pop_back();

					Data *_data = new Data(instruction_tokens, machine_code, true, false);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase;

					Assembler::current_section->size += lc_increase;

				}
			}
			else if (Parser::resolve_instruction_type(current) == "NOT_RECOGNIZED")
			{
				string error = "Error: Instruction token type not recognized -> token: " + current;

				throw Error(error);
			}
			else
			{
				throw Error("Error: Unrecognized error.");
			}
		}
		else if (Parser::resolve_type(current) == "DIRECTIVE") // directive
		{
			if (current_section->name == ".und" && (Parser::resolve_directive(current) == ".SKIP" || Parser::resolve_directive(current) == ".BYTE" || Parser::resolve_directive(current) == ".WORD"))
			{
				string error = "Error: directive cannot be located inside .und section -> " + Parser::resolve_directive(current) + " directive.";

				throw Error(error);
			}

			if (!assembler_tokens.size() && Parser::resolve_directive(current) != ".END")
			{
				throw Error("Error: insuficient number of operands.");
			}

			if (Parser::resolve_directive(current) == ".GLOBAL") // .global
			{
				while (assembler_tokens.size())
				{
					current = assembler_tokens.front();
					assembler_tokens.pop();

					if (Symbol::exists_in_symbol_table(current))
					{
						Symbol::make_global(current);
					}
					else
					{
						Symbol::create_symbol(current, Assembler::_default, Assembler::_global);
					}
				}
			}
			else if (Parser::resolve_directive(current) == ".EXTERN") // .extern
			{
				while (assembler_tokens.size())
				{
					current = assembler_tokens.front();
					assembler_tokens.pop();

					if (!Symbol::defined_in_symbol_table(current))
					{
						if (Symbol::declared_in_symbol_table(current))
						{
							for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
							{
								if (current == (*it)->name)
								{
									(*it)->is_extern = true;
									(*it)->section = ".und";
									(*it)->scope = "GLOBAL";
									(*it)->value = 0;
									(*it)->defined = true;

									break;
								}
							}
						}
						else
						{
							Symbol::create_symbol(current, Assembler::_default, Assembler::_extern);
						}
					}
					else
					{
						throw Error("Error: symbol already defined in symbol table -> .EXTERN directive.");
					}
				}
			}
			else if (Parser::resolve_directive(current) == ".SECTION") // .section
			{
				regex section_name("[\\._a-zA-Z][\\._a-zA-Z0-9]*:");

				if (!regex_match(assembler_tokens.front(), section_name))
				{
					string error = "Error: invalid token type : -> .section " + assembler_tokens.front();

					throw Error(error);
				}

				string current = assembler_tokens.front();
				assembler_tokens.pop();

				// remove the ':' sign at the end of the section name

				size_t colon = current.find(Lexer::sequences[Lexer::_colon]);

				if (colon != string::npos)
				{
					current.erase(colon);
				}

				if (Symbol::exists_in_symbol_table(current))
				{
					for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
					{
						if ((*it)->name == current)
						{
							(*it)->defined = true;
							(*it)->is_section = true;
							(*it)->is_symbol = false;
							(*it)->section = (*it)->name;

							bool found = false;

							for (auto& sec : Assembler::section_names)
							{
								if (sec == (*it)->name)
								{
									found = true;
									break;
								}
							}

							if (!found)
							{
								Assembler::section_names.push_back(current);
							}

							Assembler::current_section = *it;
							Assembler::location_counter = Assembler::current_section->size;
							break;
						}
					}
				}
				else
				{
					// insert the new section into symbol table, make it the current section

					Symbol::create_section(current);
				}
			}
			else if (Parser::resolve_directive(current) == ".END") // .end
			{
				Assembler::current_section->size = Assembler::location_counter;
			}
			else if (Parser::resolve_directive(current) == ".WORD") // .word
			{
				regex symbol_regex("[_a-zA-Z]\\w*");
				regex number_regex("(0x[a-f0-9]+)?|\\d+");

				queue<string> instruction_vector; // for Data object

				instruction_vector.push(current);

				bool symbols = false;

				queue<string> copy = assembler_tokens;

				while (copy.size())
				{
					current = copy.front();
					copy.pop();

					if (regex_match(current, number_regex))
					{
						if (current[0] == '0' && current[1] == 'x')
						{
							current = to_string(strtol(current.c_str(), nullptr, 0));
						}
					}
					else if (regex_match(current, symbol_regex))
					{
						if (!Symbol::exists_in_symbol_table(current))
						{
							Symbol::create_symbol(current);
						}

						symbols = true;
					}
					else
					{
						throw Error("Error: bad opperand type, operand must be either a number or a symbol -> .BYTE directive.");
					}

					instruction_vector.push(current);
				}

				uint16_t lc_increase = (uint16_t) (assembler_tokens.size() & 0xFFFF);

				if (symbols)
				{
					Data *_data = new Data(instruction_vector, " ", false, true);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase * 2;
					Assembler::current_section->size += lc_increase * 2;
				}
				else
				{
					string out = "";

					while (assembler_tokens.size())
					{
						current = assembler_tokens.front();
						assembler_tokens.pop();

						if (current[0] == '0' && current[1] == 'x')
						{
							if (strtol(current.c_str(), nullptr, 0) & 0xFFFF0000)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
							else
							{
								current = to_string(strtol(current.c_str(), nullptr, 0));
							}
						}
						else
						{
							if (stoi(current) & 0xFFFF0000)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
						}

						uint16_t _literal = (uint16_t) (stoi(current) & 0xFFFF);

						out += Lexer::to_hex(_literal);
						out += " ";
					}

					out.pop_back();

					Data *_data = new Data(instruction_vector, out, true, true);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase * 2;
					Assembler::current_section->size += lc_increase * 2;
				}
			}
			else if (Parser::resolve_directive(current) == ".BYTE") // .byte
			{
				regex symbol_regex("[_a-zA-Z]\\w*");
				regex number_regex("(0x[a-f0-9]+)?|\\d+");

				queue<string> instruction_vector; // for Data object

				instruction_vector.push(current);

				bool symbols = false;

				queue<string> copy = assembler_tokens;

				while (copy.size())
				{
					current = copy.front();
					copy.pop();

					if (regex_match(current, number_regex))
					{
						if (current[0] == '0' && current[1] == 'x')
						{
							current = to_string(strtol(current.c_str(), nullptr, 0));
						}
					}
					else if (regex_match(current, symbol_regex))
					{
						if (!Symbol::exists_in_symbol_table(current))
						{
							Symbol::create_symbol(current);
						}

						symbols = true;
					}
					else
					{
						throw Error("Error: bad opperand type, operand must be either a number or a symbol -> .BYTE directive.");
					}

					instruction_vector.push(current);
				}

				uint16_t lc_increase = (uint16_t) (assembler_tokens.size() & 0xFFFF);

				if (symbols)
				{
					Data *_data = new Data(instruction_vector, " ", false, true);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase;
					Assembler::current_section->size += lc_increase;
				}
				else
				{
					string out = "";

					while (assembler_tokens.size())
					{
						current = assembler_tokens.front();
						assembler_tokens.pop();

						if (current[0] == '0' && current[1] == 'x')
						{
							if (strtol(current.c_str(), nullptr, 0) & 0xFF00)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
							else
							{
								current = to_string(strtol(current.c_str(), nullptr, 0));
							}
						}
						else
						{
							if (stoi(current) & 0xFF00)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
						}

						uint8_t _literal = (uint8_t) (stoi(current) & 0xFF);

						out += Lexer::to_hex(_literal);
						out += " ";
					}

					out.pop_back();

					Data *_data = new Data(instruction_vector, out, true, true);
					Assembler::data.push_back(_data);

					Assembler::location_counter += lc_increase;
					Assembler::current_section->size += lc_increase;
				}
			}
			else if (Parser::resolve_directive(current) == ".SKIP") // .skip
			{
				if (assembler_tokens.size() != 1)
				{
					throw Error("Error: .SKIP directive only takes one literal.");
				}

				string directive_name = current;

				current = assembler_tokens.front();
				assembler_tokens.pop();

				regex regex("(0x[a-f0-9]+)?|\\d+");

				if (!regex_match(current, regex))
				{
					throw Error("Error: token must be a number -> .SKIP directive.");
				}

				if (current[0] == '0' && current[1] == 'x')
				{
					if (strtol(current.c_str(), nullptr, 0) & 0xFFFF0000)
					{
						throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
					}
					else
					{
						current = to_string(strtol(current.c_str(), nullptr, 0));
					}
				}
				else
				{
					if (stoi(current) & 0xFFFF0000)
					{
						throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
					}
				}

				uint16_t _literal = (uint16_t) (stoi(current) & 0xFFFF);

				queue<string> data_vector;

				data_vector.push(directive_name);
				data_vector.push(current);

				uint8_t __literal = 0;

				string code;

				for (int i = 0; i < _literal; i++)
				{
					code += Lexer::to_hex(__literal);
					code += " ";
				}

				code.pop_back();

				Data *_data = new Data(data_vector, code, true, Data::__directive);

				Assembler::data.push_back(_data);

				Assembler::location_counter += _literal;

				Assembler::current_section->size += _literal;
			}
			else if (Parser::resolve_directive(current) == ".EQU") // .equ
			{
				regex symbol_regex("[\\._a-zA-Z]\\w*");
				regex number_regex("(0x[a-f0-9]+)?|\\d+");
				regex operation_regex("[\\+-]");

				string equ_symbol = assembler_tokens.front();
				assembler_tokens.pop();

				queue<string> copy = assembler_tokens;

				deque<string> operands;
				deque<string> operations;

				bool contains_symbol = false;

				if (!regex_match(equ_symbol, symbol_regex))
				{
					throw Error("Error: first operand must be a symbol -> .EQU directive.");
				}

				if (Symbol::defined_in_symbol_table(equ_symbol))
				{
					string error = "Error: symbol " + equ_symbol + " is already defined -> .EQU directive.";

					throw Error(error);
				}
				else if (!Symbol::exists_in_symbol_table(equ_symbol))
				{
					Symbol::create_symbol(equ_symbol, 0, 3);
				}

				if (copy.empty())
				{
					throw Error("Error: directive must contain more than one operand -> .EQU directive.");
				}

				while (copy.size())
				{
					current = copy.front();
					copy.pop();

					if (regex_match(current, symbol_regex))
					{
						contains_symbol = true;

						operands.push_back(current);

						if (!Symbol::exists_in_symbol_table(current))
						{
							Symbol::create_symbol(current, 0, 3);
						}
					}
					else if (regex_match(current, number_regex))
					{
						if (current[0] == '0' && current[1] == 'x')
						{
							if (strtol(current.c_str(), nullptr, 0) & 0xFFFF0000)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
							else
							{
								current = to_string(strtol(current.c_str(), nullptr, 0));
							}
						}
						else
						{
							if (stoi(current) & 0xFFFF0000)
							{
								throw Error("Error: operand size is greater than MAX operand size -> .EQU directive.");
							}
						}

						operands.push_back(current);
					}
					else if (regex_match(current, operation_regex))
					{
						operations.push_back(current);
					}
					else
					{
						throw Error("Error: unrecognized token type, operands and operations must be separated by a whitespace character -> .EQU directive.");
					}
				}

				if (operands.size() < operations.size())
				{
					throw Error("Error: number of operands must be greater than number of operations -> .EQU directive.");
				}
				else if (operands.size() > operations.size())
				{
					operations.push_front("+");
				}

				if (contains_symbol)
				{
					UnknownSymbol *_unknown = new UnknownSymbol(equ_symbol, operands, operations);
					Assembler::table_of_unknown_symbols.push_back(_unknown);
				}
				else
				{
					uint16_t result = 0;

					string op;

					while (operands.size() || operations.size())
					{
						op = operations.front();
						operations.pop_front();

						uint16_t operand = (uint16_t) (stoi(operands.front()) & 0xFFFF);

						operands.pop_front();

						if (op == "+")
						{
							result += operand;
						}
						else if (op == "-")
						{
							result -= operand;
						}
					}

					for (list<Symbol*>::iterator it = Assembler::symbol_table.begin(); it != Assembler::symbol_table.end(); it++)
					{
						if ((*it)->name == equ_symbol)
						{
							(*it)->section = ".abs";
							(*it)->value = result;
							(*it)->absolute = true;
							(*it)->defined = true;
							(*it)->equ_relocatable = true;

							break;
						}
					}
				}
			}
		}
		else if (Parser::resolve_type(current) == "NOT_RECOGNIZED") // not recognized
		{
			string error = "Error: Token type not recognized -> token: " + current;

			throw Error(error);
		}
		else
		{
			throw Error("Error: Unrecognized error.");
		}
	}

	Assembler::resolve_unknown_symbols();

	Assembler::check_up();
}