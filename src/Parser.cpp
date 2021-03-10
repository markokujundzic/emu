#include "Parser.h"

unordered_map<string, addressing_data> Parser::addressing_types_jmp_symbol =
{ 
	{ "IMMEDIATE",         { regex("[_a-zA-Z]\\w*"),                              0x0, true  } },
	{ "REG_DIR",          { regex("\\*%(r[0-7]|pc|sp|psw)[hl]?"),                 0x1, false } },
	{ "REG_IND",          { regex("\\(%r([0-7]|pc|sp|psw)[hl]?\\)"),              0x2, false } },
	{ "REG_IND_DISPL",    { regex("\\*[_a-zA-Z]\\w*\\(%(r[0-6]|sp|psw)[hl]?\\)"), 0x3, true  } },
	{ "REG_IND_DISPL_PC", { regex("\\*[_a-zA-Z]\\w*\\(%(r7|pc)[hl]?\\)"),         0x3, true  } },
	{ "MEMORY",           { regex("\\*[_a-zA-Z]\\w*"),                            0x4, true  } }
};

unordered_map<string, addressing_data> Parser::addressing_types_jmp_literal =
{
	{ "IMMEDIATE",     { regex("-?((0x[a-f0-9]+)?|\\d+)"),                              0x0, true  } },
	{ "REG_DIR",       { regex("\\*%(r[0-7]|pc|sp|psw)[hl]?"),                          0x1, false } },
	{ "REG_IND",       { regex("\\*\\(%(r[0-7]|pc|sp|psw)[hl]?\\)"),                    0x2, false } },
	{ "REG_IND_DISPL", { regex("\\*(0x)?([0-9]|[a-f])+\\(%(r[0-7]|pc|sp|psw)[hl]?\\)"), 0x3, true  } },
	{ "MEMORY",        { regex("\\*((0x[a-f0-9]+)?|\\d+)"),                             0x4, true  } }
};

unordered_map<string, addressing_data> Parser::addressing_types_not_jmp_symbol =
{ 
	{ "IMMEDIATE",        { regex("\\$[_a-zA-Z]\\w*"),                         0x0, true  } },
	{ "REG_DIR",          { regex("%(r[0-7]|pc|sp|psw)[hl]?"),                 0x1, false } },
	{ "REG_IND",          { regex("\\(%(r[0-7]|pc|sp|psw)[hl]?\\)"),           0x2, false } },
	{ "REG_IND_DISPL",    { regex("[_a-zA-Z]\\w*\\(%(r[0-6]|sp|psw)[hl]?\\)"), 0x3, true  } },
	{ "REG_IND_DISPL_PC", { regex("[_a-zA-Z]\\w*\\(%(r7|pc)[hl]?\\)"),         0x3, true  } },
	{ "MEMORY",           { regex("[_a-zA-Z]\\w*"),                            0x4, true  } }
};

unordered_map<string, addressing_data> Parser::addressing_types_not_jmp_literal =
{
	{ "IMMEDIATE",     { regex("\\$-?((0x[a-f0-9]+)?|\\d+)"),                        0x0, true  } },
	{ "REG_DIR",       { regex("%(r[0-7]|pc|sp|psw)[hl]?"),                          0x1, false } },
	{ "REG_IND",       { regex("\\(%(r[0-7]|pc|sp|psw)[hl]?\\)"),                    0x2, false } },
	{ "REG_IND_DISPL", { regex("(0x)?([0-9]|[a-f])+\\(%(r[0-7]|pc|sp|psw)[hl]?\\)"), 0x3, true  } },
	{ "MEMORY",        { regex("((0x[a-f0-9]+)?|\\d+)"),                             0x4, true  } }
};


unordered_map<string, regex> Parser::type = 
{
	{ "INSTRUCTION", regex("[a-z]{2,5}") },
	{ "LABEL",	   	 regex("[_a-zA-Z]\\w*:") },
	{ "DIRECTIVE",	 regex("\\.\\w+[^:]") }
};

unordered_map<string, regex> Parser::instruction_type =
{
	{ "JMP",     regex("int[bw]?|call[bw]?|jmp[bw]?|jeq[bw]?|jne[bw]?|jgt[bw]?|push[bw]?|pop[bw]?")                                                                             },
	{ "NOT_JMP", regex("halt|iret|ret|xchg[bw]?|mov[bw]?|add[bw]?|sub[bw]?|mul[bw]?|div[bw]?|cmp[bw]?|not[bw]?|and[bw]?|or[bw]?|xor[bw]?|test[bw]?|shl[bw]?|shr[bw]?|mod[bw]?") }
};

unordered_map<string, regex> Parser::patching =
{
	{ ".BYTE", regex("\\.byte")   },
	{ ".WORD", regex("\\.word")   },
	{ "HALT",  regex("halt")      },
	{ "IRET",  regex("iret")      },
	{ "RET",   regex("ret")       },
	{ "INT",   regex("int[bw]?")  },
	{ "CALL",  regex("call[bw]?") },
	{ "JMP",   regex("jmp[bw]?")  },
	{ "JEQ",   regex("jeq[bw]?")  },
	{ "JNE",   regex("jne[bw]?")  },
	{ "JGT",   regex("jgt[bw]?")  },
	{ "PUSH",  regex("push[bw]?") },
	{ "POP",   regex("pop[bw]?")  },
	{ "XCHG",  regex("xchg[bw]?") },
	{ "MOV",   regex("mov[bw]?")  },
	{ "ADD",   regex("add[bw]?")  },
	{ "SUB",   regex("sub[bw]?")  },
	{ "MUL",   regex("mul[bw]?")  },
	{ "DIV",   regex("div[bw]?")  },
	{ "CMP",   regex("cmp[bw]?")  },
	{ "NOT",   regex("not[bw]?")  },
	{ "AND",   regex("and[bw]?")  },
	{ "OR",    regex("or[bw]?")   },
	{ "XOR",   regex("xor[bw]?")  },
	{ "TEST",  regex("test[bw]?") },
	{ "SHL",   regex("shl[bw]?")  },
	{ "SHR",   regex("shr[bw]?")  },
	{ "MOD",   regex("mod[bw]?")  }
};

unordered_map<string, instruction_data> Parser::instructions =
{
	{ "HALT", { regex("halt"),	0x00, 0 } },
	{ "IRET", { regex("iret"),	0x01, 0 } },
	{ "RET",  { regex("ret"),	0x02, 0 } },
	{ "INT",  { regex("int[bw]?"),	0x03, 1 } },
	{ "CALL", { regex("call[bw]?"),	0x04, 1 } },
	{ "JMP",  { regex("jmp[bw]?"),	0x05, 1 } },
	{ "JEQ",  { regex("jeq[bw]?"),	0x06, 1 } },
	{ "JNE",  { regex("jne[bw]?"),	0x07, 1 } },
	{ "JGT",  { regex("jgt[bw]?"),	0x08, 1 } },
	{ "PUSH", { regex("push[bw]?"),	0x09, 1 } },
	{ "POP",  { regex("pop[bw]?"),	0x0A, 1 } },
	{ "XCHG", { regex("xchg[bw]?"), 0x0B, 2 } },
	{ "MOV",  { regex("mov[bw]?"),  0x0C, 2 } },
	{ "ADD",  { regex("add[bw]?"),  0x0D, 2 } },
	{ "SUB",  { regex("sub[bw]?"),  0x0E, 2 } },
	{ "MUL",  { regex("mul[bw]?"),  0x0F, 2 } },
	{ "DIV",  { regex("div[bw]?"),  0x10, 2 } },
	{ "CMP",  { regex("cmp[bw]?"),  0x11, 2 } },
	{ "NOT",  { regex("not[bw]?"),  0x12, 2 } },
	{ "AND",  { regex("and[bw]?"),  0x13, 2 } },
	{ "OR",   { regex("or[bw]?"),   0x14, 2 } },
	{ "XOR",  { regex("xor[bw]?"),  0x15, 2 } },
	{ "TEST", { regex("test[bw]?"), 0x16, 2 } },
	{ "SHL",  { regex("shl[bw]?"),  0x17, 2 } },
	{ "SHR",  { regex("shr[bw]?"),  0x18, 2 } },
	{ "MOD",  { regex("mod[bw]?"),  0x19, 2 } }
};

unordered_map<string, regex> Parser::directives =
{
	{ ".GLOBAL",  regex("\\.global")  },
	{ ".EXTERN",  regex("\\.extern")  },
	{ ".SECTION", regex("\\.section") },
	{ ".END",     regex("\\.end")     },
	{ ".BYTE",    regex("\\.byte")    },
	{ ".WORD",    regex("\\.word")    },
	{ ".SKIP",    regex("\\.skip")    },
	{ ".EQU",     regex("\\.equ")     }
};

string Parser::resolve_addressing_not_jmp_literal(const string& name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_literal)
	{
		if (regex_match(name, it.second.pattern))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_addressing_not_jmp_symbol(const string& name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, addressing_data>& it : Parser::addressing_types_not_jmp_symbol)
	{
		if (regex_match(name, it.second.pattern))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_addressing_jmp_literal(const string& name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, addressing_data>& it : Parser::addressing_types_jmp_literal)
	{
		if (regex_match(name, it.second.pattern))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_addressing_jmp_symbol(const string& name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, addressing_data>& it : Parser::addressing_types_jmp_symbol)
	{
		if (regex_match(name, it.second.pattern))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_patch(const string& name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, regex>& it : Parser::patching)
	{
		if (regex_match(name, it.second))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_type(const string& data_type)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, regex>& it : Parser::type)
	{
		if (regex_match(data_type, it.second))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_instruction_type(const string& data_type)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, regex>& it : Parser::instruction_type)
	{
		if (regex_match(data_type, it.second))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_instruction(const string& mnemonic)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, instruction_data>& it : Parser::instructions)
	{
		if (regex_match(mnemonic, it.second.pattern))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}

string Parser::resolve_directive(const string& directive_name)
{
	string ret = "NOT_RECOGNIZED";

	for (const pair<string, regex>& it : Parser::directives)
	{
		if (regex_match(directive_name, it.second))
		{
			ret = it.first;
			break;
		}
	}

	return ret;
}
