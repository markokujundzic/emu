#ifndef _error_h_
#define _error_h_

#include <exception>
#include <string>

using namespace std;

class Error : public exception
{
public:
	explicit Error(const string& message) : msg(message) 
	{

	}

	virtual ~Error() noexcept
	{ 

	}

	virtual const char* what() const throw ()
	{
		return msg.c_str();
	}

private:
	string msg;
};

#endif