#pragma once

#include <string>
#include <vector>

using namespace std;

struct IPPTokenStream;

enum EPPTokenType
{
	PPT_WHITESPACE_SEQUENCE,
	PPT_NEW_LINE,
	PPT_HEADER_NAME,
	PPT_IDENTIFIER,
	PPT_PP_NUMBER,
	PPT_CHARACTER_LITERAL,
	PPT_USER_DEFINED_CHARACTER_LITERAL,
	PPT_STRING_LITERAL,
	PPT_USER_DEFINED_STRING_LITERAL,
	PPT_PREPROCESSING_OP_OR_PUNC,
	PPT_NON_WHITESPACE_CHAR,
	PPT_EOF
};

struct PPToken
{
	EPPTokenType type;
	string data;

	PPToken(EPPTokenType type = PPT_EOF, string data = "")
		: type(type), data(move(data))
	{}
};

vector<PPToken> LexPPTokens(string input);
void EmitPPTokens(const vector<PPToken>& tokens, IPPTokenStream& output);
