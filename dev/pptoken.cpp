#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

#include "DebugPPTokenStream.h"
#include "PPLexer.h"

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		DebugPPTokenStream output;
		EmitPPTokens(LexPPTokens(oss.str()), output);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
