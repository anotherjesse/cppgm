// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <memory>
#include <cstring>
#include <cstdint>
#include <climits>
#include <map>
#include <set>
#include <limits>
#include <regex>

using namespace std;

// Reuse PA1 tokenizer implementation in this translation unit.
#define main pa1_pptoken_main
#include "pptoken.cpp"
#undef main

// See 3.9.1: Fundamental Types
enum EFundamentalType
{
	// 3.9.1.2
	FT_SIGNED_CHAR,
	FT_SHORT_INT,
	FT_INT,
	FT_LONG_INT,
	FT_LONG_LONG_INT,

	// 3.9.1.3
	FT_UNSIGNED_CHAR,
	FT_UNSIGNED_SHORT_INT,
	FT_UNSIGNED_INT,
	FT_UNSIGNED_LONG_INT,
	FT_UNSIGNED_LONG_LONG_INT,

	// 3.9.1.1 / 3.9.1.5
	FT_WCHAR_T,
	FT_CHAR,
	FT_CHAR16_T,
	FT_CHAR32_T,

	// 3.9.1.6
	FT_BOOL,

	// 3.9.1.8
	FT_FLOAT,
	FT_DOUBLE,
	FT_LONG_DOUBLE,

	// 3.9.1.9
	FT_VOID,

	// 3.9.1.10
	FT_NULLPTR_T
};

// FundamentalTypeOf: convert fundamental type T to EFundamentalType
// for example: `FundamentalTypeOf<long int>()` will return `FT_LONG_INT`
template<typename T> constexpr EFundamentalType FundamentalTypeOf();
template<> constexpr EFundamentalType FundamentalTypeOf<signed char>() { return FT_SIGNED_CHAR; }
template<> constexpr EFundamentalType FundamentalTypeOf<short int>() { return FT_SHORT_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<int>() { return FT_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<long int>() { return FT_LONG_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<long long int>() { return FT_LONG_LONG_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<unsigned char>() { return FT_UNSIGNED_CHAR; }
template<> constexpr EFundamentalType FundamentalTypeOf<unsigned short int>() { return FT_UNSIGNED_SHORT_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<unsigned int>() { return FT_UNSIGNED_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<unsigned long int>() { return FT_UNSIGNED_LONG_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<unsigned long long int>() { return FT_UNSIGNED_LONG_LONG_INT; }
template<> constexpr EFundamentalType FundamentalTypeOf<wchar_t>() { return FT_WCHAR_T; }
template<> constexpr EFundamentalType FundamentalTypeOf<char>() { return FT_CHAR; }
template<> constexpr EFundamentalType FundamentalTypeOf<char16_t>() { return FT_CHAR16_T; }
template<> constexpr EFundamentalType FundamentalTypeOf<char32_t>() { return FT_CHAR32_T; }
template<> constexpr EFundamentalType FundamentalTypeOf<bool>() { return FT_BOOL; }
template<> constexpr EFundamentalType FundamentalTypeOf<float>() { return FT_FLOAT; }
template<> constexpr EFundamentalType FundamentalTypeOf<double>() { return FT_DOUBLE; }
template<> constexpr EFundamentalType FundamentalTypeOf<long double>() { return FT_LONG_DOUBLE; }
template<> constexpr EFundamentalType FundamentalTypeOf<void>() { return FT_VOID; }
template<> constexpr EFundamentalType FundamentalTypeOf<nullptr_t>() { return FT_NULLPTR_T; }

// convert EFundamentalType to a source code
const map<EFundamentalType, string> FundamentalTypeToStringMap
{
	{FT_SIGNED_CHAR, "signed char"},
	{FT_SHORT_INT, "short int"},
	{FT_INT, "int"},
	{FT_LONG_INT, "long int"},
	{FT_LONG_LONG_INT, "long long int"},
	{FT_UNSIGNED_CHAR, "unsigned char"},
	{FT_UNSIGNED_SHORT_INT, "unsigned short int"},
	{FT_UNSIGNED_INT, "unsigned int"},
	{FT_UNSIGNED_LONG_INT, "unsigned long int"},
	{FT_UNSIGNED_LONG_LONG_INT, "unsigned long long int"},
	{FT_WCHAR_T, "wchar_t"},
	{FT_CHAR, "char"},
	{FT_CHAR16_T, "char16_t"},
	{FT_CHAR32_T, "char32_t"},
	{FT_BOOL, "bool"},
	{FT_FLOAT, "float"},
	{FT_DOUBLE, "double"},
	{FT_LONG_DOUBLE, "long double"},
	{FT_VOID, "void"},
	{FT_NULLPTR_T, "nullptr_t"}
};

// token type enum for `simples`
enum ETokenType
{
	// keywords
	KW_ALIGNAS,
	KW_ALIGNOF,
	KW_ASM,
	KW_AUTO,
	KW_BOOL,
	KW_BREAK,
	KW_CASE,
	KW_CATCH,
	KW_CHAR,
	KW_CHAR16_T,
	KW_CHAR32_T,
	KW_CLASS,
	KW_CONST,
	KW_CONSTEXPR,
	KW_CONST_CAST,
	KW_CONTINUE,
	KW_DECLTYPE,
	KW_DEFAULT,
	KW_DELETE,
	KW_DO,
	KW_DOUBLE,
	KW_DYNAMIC_CAST,
	KW_ELSE,
	KW_ENUM,
	KW_EXPLICIT,
	KW_EXPORT,
	KW_EXTERN,
	KW_FALSE,
	KW_FLOAT,
	KW_FOR,
	KW_FRIEND,
	KW_GOTO,
	KW_IF,
	KW_INLINE,
	KW_INT,
	KW_LONG,
	KW_MUTABLE,
	KW_NAMESPACE,
	KW_NEW,
	KW_NOEXCEPT,
	KW_NULLPTR,
	KW_OPERATOR,
	KW_PRIVATE,
	KW_PROTECTED,
	KW_PUBLIC,
	KW_REGISTER,
	KW_REINTERPET_CAST,
	KW_RETURN,
	KW_SHORT,
	KW_SIGNED,
	KW_SIZEOF,
	KW_STATIC,
	KW_STATIC_ASSERT,
	KW_STATIC_CAST,
	KW_STRUCT,
	KW_SWITCH,
	KW_TEMPLATE,
	KW_THIS,
	KW_THREAD_LOCAL,
	KW_THROW,
	KW_TRUE,
	KW_TRY,
	KW_TYPEDEF,
	KW_TYPEID,
	KW_TYPENAME,
	KW_UNION,
	KW_UNSIGNED,
	KW_USING,
	KW_VIRTUAL,
	KW_VOID,
	KW_VOLATILE,
	KW_WCHAR_T,
	KW_WHILE,

	// operators/punctuation
	OP_LBRACE,
	OP_RBRACE,
	OP_LSQUARE,
	OP_RSQUARE,
	OP_LPAREN,
	OP_RPAREN,
	OP_BOR,
	OP_XOR,
	OP_COMPL,
	OP_AMP,
	OP_LNOT,
	OP_SEMICOLON,
	OP_COLON,
	OP_DOTS,
	OP_QMARK,
	OP_COLON2,
	OP_DOT,
	OP_DOTSTAR,
	OP_PLUS,
	OP_MINUS,
	OP_STAR,
	OP_DIV,
	OP_MOD,
	OP_ASS,
	OP_LT,
	OP_GT,
	OP_PLUSASS,
	OP_MINUSASS,
	OP_STARASS,
	OP_DIVASS,
	OP_MODASS,
	OP_XORASS,
	OP_BANDASS,
	OP_BORASS,
	OP_LSHIFT,
	OP_RSHIFT,
	OP_RSHIFTASS,
	OP_LSHIFTASS,
	OP_EQ,
	OP_NE,
	OP_LE,
	OP_GE,
	OP_LAND,
	OP_LOR,
	OP_INC,
	OP_DEC,
	OP_COMMA,
	OP_ARROWSTAR,
	OP_ARROW,
};

// StringToETokenTypeMap map of `simple` `preprocessing-tokens` to ETokenType
const unordered_map<string, ETokenType> StringToTokenTypeMap =
{
	// keywords
	{"alignas", KW_ALIGNAS},
	{"alignof", KW_ALIGNOF},
	{"asm", KW_ASM},
	{"auto", KW_AUTO},
	{"bool", KW_BOOL},
	{"break", KW_BREAK},
	{"case", KW_CASE},
	{"catch", KW_CATCH},
	{"char", KW_CHAR},
	{"char16_t", KW_CHAR16_T},
	{"char32_t", KW_CHAR32_T},
	{"class", KW_CLASS},
	{"const", KW_CONST},
	{"constexpr", KW_CONSTEXPR},
	{"const_cast", KW_CONST_CAST},
	{"continue", KW_CONTINUE},
	{"decltype", KW_DECLTYPE},
	{"default", KW_DEFAULT},
	{"delete", KW_DELETE},
	{"do", KW_DO},
	{"double", KW_DOUBLE},
	{"dynamic_cast", KW_DYNAMIC_CAST},
	{"else", KW_ELSE},
	{"enum", KW_ENUM},
	{"explicit", KW_EXPLICIT},
	{"export", KW_EXPORT},
	{"extern", KW_EXTERN},
	{"false", KW_FALSE},
	{"float", KW_FLOAT},
	{"for", KW_FOR},
	{"friend", KW_FRIEND},
	{"goto", KW_GOTO},
	{"if", KW_IF},
	{"inline", KW_INLINE},
	{"int", KW_INT},
	{"long", KW_LONG},
	{"mutable", KW_MUTABLE},
	{"namespace", KW_NAMESPACE},
	{"new", KW_NEW},
	{"noexcept", KW_NOEXCEPT},
	{"nullptr", KW_NULLPTR},
	{"operator", KW_OPERATOR},
	{"private", KW_PRIVATE},
	{"protected", KW_PROTECTED},
	{"public", KW_PUBLIC},
	{"register", KW_REGISTER},
	{"reinterpret_cast", KW_REINTERPET_CAST},
	{"return", KW_RETURN},
	{"short", KW_SHORT},
	{"signed", KW_SIGNED},
	{"sizeof", KW_SIZEOF},
	{"static", KW_STATIC},
	{"static_assert", KW_STATIC_ASSERT},
	{"static_cast", KW_STATIC_CAST},
	{"struct", KW_STRUCT},
	{"switch", KW_SWITCH},
	{"template", KW_TEMPLATE},
	{"this", KW_THIS},
	{"thread_local", KW_THREAD_LOCAL},
	{"throw", KW_THROW},
	{"true", KW_TRUE},
	{"try", KW_TRY},
	{"typedef", KW_TYPEDEF},
	{"typeid", KW_TYPEID},
	{"typename", KW_TYPENAME},
	{"union", KW_UNION},
	{"unsigned", KW_UNSIGNED},
	{"using", KW_USING},
	{"virtual", KW_VIRTUAL},
	{"void", KW_VOID},
	{"volatile", KW_VOLATILE},
	{"wchar_t", KW_WCHAR_T},
	{"while", KW_WHILE},

	// operators/punctuation
	{"{", OP_LBRACE},
	{"<%", OP_LBRACE},
	{"}", OP_RBRACE},
	{"%>", OP_RBRACE},
	{"[", OP_LSQUARE},
	{"<:", OP_LSQUARE},
	{"]", OP_RSQUARE},
	{":>", OP_RSQUARE},
	{"(", OP_LPAREN},
	{")", OP_RPAREN},
	{"|", OP_BOR},
	{"bitor", OP_BOR},
	{"^", OP_XOR},
	{"xor", OP_XOR},
	{"~", OP_COMPL},
	{"compl", OP_COMPL},
	{"&", OP_AMP},
	{"bitand", OP_AMP},
	{"!", OP_LNOT},
	{"not", OP_LNOT},
	{";", OP_SEMICOLON},
	{":", OP_COLON},
	{"...", OP_DOTS},
	{"?", OP_QMARK},
	{"::", OP_COLON2},
	{".", OP_DOT},
	{".*", OP_DOTSTAR},
	{"+", OP_PLUS},
	{"-", OP_MINUS},
	{"*", OP_STAR},
	{"/", OP_DIV},
	{"%", OP_MOD},
	{"=", OP_ASS},
	{"<", OP_LT},
	{">", OP_GT},
	{"+=", OP_PLUSASS},
	{"-=", OP_MINUSASS},
	{"*=", OP_STARASS},
	{"/=", OP_DIVASS},
	{"%=", OP_MODASS},
	{"^=", OP_XORASS},
	{"xor_eq", OP_XORASS},
	{"&=", OP_BANDASS},
	{"and_eq", OP_BANDASS},
	{"|=", OP_BORASS},
	{"or_eq", OP_BORASS},
	{"<<", OP_LSHIFT},
	{">>", OP_RSHIFT},
	{">>=", OP_RSHIFTASS},
	{"<<=", OP_LSHIFTASS},
	{"==", OP_EQ},
	{"!=", OP_NE},
	{"not_eq", OP_NE},
	{"<=", OP_LE},
	{">=", OP_GE},
	{"&&", OP_LAND},
	{"and", OP_LAND},
	{"||", OP_LOR},
	{"or", OP_LOR},
	{"++", OP_INC},
	{"--", OP_DEC},
	{",", OP_COMMA},
	{"->*", OP_ARROWSTAR},
	{"->", OP_ARROW}
};

// map of enum to string
const map<ETokenType, string> TokenTypeToStringMap =
{
	{KW_ALIGNAS, "KW_ALIGNAS"},
	{KW_ALIGNOF, "KW_ALIGNOF"},
	{KW_ASM, "KW_ASM"},
	{KW_AUTO, "KW_AUTO"},
	{KW_BOOL, "KW_BOOL"},
	{KW_BREAK, "KW_BREAK"},
	{KW_CASE, "KW_CASE"},
	{KW_CATCH, "KW_CATCH"},
	{KW_CHAR, "KW_CHAR"},
	{KW_CHAR16_T, "KW_CHAR16_T"},
	{KW_CHAR32_T, "KW_CHAR32_T"},
	{KW_CLASS, "KW_CLASS"},
	{KW_CONST, "KW_CONST"},
	{KW_CONSTEXPR, "KW_CONSTEXPR"},
	{KW_CONST_CAST, "KW_CONST_CAST"},
	{KW_CONTINUE, "KW_CONTINUE"},
	{KW_DECLTYPE, "KW_DECLTYPE"},
	{KW_DEFAULT, "KW_DEFAULT"},
	{KW_DELETE, "KW_DELETE"},
	{KW_DO, "KW_DO"},
	{KW_DOUBLE, "KW_DOUBLE"},
	{KW_DYNAMIC_CAST, "KW_DYNAMIC_CAST"},
	{KW_ELSE, "KW_ELSE"},
	{KW_ENUM, "KW_ENUM"},
	{KW_EXPLICIT, "KW_EXPLICIT"},
	{KW_EXPORT, "KW_EXPORT"},
	{KW_EXTERN, "KW_EXTERN"},
	{KW_FALSE, "KW_FALSE"},
	{KW_FLOAT, "KW_FLOAT"},
	{KW_FOR, "KW_FOR"},
	{KW_FRIEND, "KW_FRIEND"},
	{KW_GOTO, "KW_GOTO"},
	{KW_IF, "KW_IF"},
	{KW_INLINE, "KW_INLINE"},
	{KW_INT, "KW_INT"},
	{KW_LONG, "KW_LONG"},
	{KW_MUTABLE, "KW_MUTABLE"},
	{KW_NAMESPACE, "KW_NAMESPACE"},
	{KW_NEW, "KW_NEW"},
	{KW_NOEXCEPT, "KW_NOEXCEPT"},
	{KW_NULLPTR, "KW_NULLPTR"},
	{KW_OPERATOR, "KW_OPERATOR"},
	{KW_PRIVATE, "KW_PRIVATE"},
	{KW_PROTECTED, "KW_PROTECTED"},
	{KW_PUBLIC, "KW_PUBLIC"},
	{KW_REGISTER, "KW_REGISTER"},
	{KW_REINTERPET_CAST, "KW_REINTERPET_CAST"},
	{KW_RETURN, "KW_RETURN"},
	{KW_SHORT, "KW_SHORT"},
	{KW_SIGNED, "KW_SIGNED"},
	{KW_SIZEOF, "KW_SIZEOF"},
	{KW_STATIC, "KW_STATIC"},
	{KW_STATIC_ASSERT, "KW_STATIC_ASSERT"},
	{KW_STATIC_CAST, "KW_STATIC_CAST"},
	{KW_STRUCT, "KW_STRUCT"},
	{KW_SWITCH, "KW_SWITCH"},
	{KW_TEMPLATE, "KW_TEMPLATE"},
	{KW_THIS, "KW_THIS"},
	{KW_THREAD_LOCAL, "KW_THREAD_LOCAL"},
	{KW_THROW, "KW_THROW"},
	{KW_TRUE, "KW_TRUE"},
	{KW_TRY, "KW_TRY"},
	{KW_TYPEDEF, "KW_TYPEDEF"},
	{KW_TYPEID, "KW_TYPEID"},
	{KW_TYPENAME, "KW_TYPENAME"},
	{KW_UNION, "KW_UNION"},
	{KW_UNSIGNED, "KW_UNSIGNED"},
	{KW_USING, "KW_USING"},
	{KW_VIRTUAL, "KW_VIRTUAL"},
	{KW_VOID, "KW_VOID"},
	{KW_VOLATILE, "KW_VOLATILE"},
	{KW_WCHAR_T, "KW_WCHAR_T"},
	{KW_WHILE, "KW_WHILE"},
	{OP_LBRACE, "OP_LBRACE"},
	{OP_RBRACE, "OP_RBRACE"},
	{OP_LSQUARE, "OP_LSQUARE"},
	{OP_RSQUARE, "OP_RSQUARE"},
	{OP_LPAREN, "OP_LPAREN"},
	{OP_RPAREN, "OP_RPAREN"},
	{OP_BOR, "OP_BOR"},
	{OP_XOR, "OP_XOR"},
	{OP_COMPL, "OP_COMPL"},
	{OP_AMP, "OP_AMP"},
	{OP_LNOT, "OP_LNOT"},
	{OP_SEMICOLON, "OP_SEMICOLON"},
	{OP_COLON, "OP_COLON"},
	{OP_DOTS, "OP_DOTS"},
	{OP_QMARK, "OP_QMARK"},
	{OP_COLON2, "OP_COLON2"},
	{OP_DOT, "OP_DOT"},
	{OP_DOTSTAR, "OP_DOTSTAR"},
	{OP_PLUS, "OP_PLUS"},
	{OP_MINUS, "OP_MINUS"},
	{OP_STAR, "OP_STAR"},
	{OP_DIV, "OP_DIV"},
	{OP_MOD, "OP_MOD"},
	{OP_ASS, "OP_ASS"},
	{OP_LT, "OP_LT"},
	{OP_GT, "OP_GT"},
	{OP_PLUSASS, "OP_PLUSASS"},
	{OP_MINUSASS, "OP_MINUSASS"},
	{OP_STARASS, "OP_STARASS"},
	{OP_DIVASS, "OP_DIVASS"},
	{OP_MODASS, "OP_MODASS"},
	{OP_XORASS, "OP_XORASS"},
	{OP_BANDASS, "OP_BANDASS"},
	{OP_BORASS, "OP_BORASS"},
	{OP_LSHIFT, "OP_LSHIFT"},
	{OP_RSHIFT, "OP_RSHIFT"},
	{OP_RSHIFTASS, "OP_RSHIFTASS"},
	{OP_LSHIFTASS, "OP_LSHIFTASS"},
	{OP_EQ, "OP_EQ"},
	{OP_NE, "OP_NE"},
	{OP_LE, "OP_LE"},
	{OP_GE, "OP_GE"},
	{OP_LAND, "OP_LAND"},
	{OP_LOR, "OP_LOR"},
	{OP_INC, "OP_INC"},
	{OP_DEC, "OP_DEC"},
	{OP_COMMA, "OP_COMMA"},
	{OP_ARROWSTAR, "OP_ARROWSTAR"},
	{OP_ARROW, "OP_ARROW"}
};

// convert integer [0,15] to hexadecimal digit
char ValueToHexChar(int c)
{
	switch (c)
	{
	case 0: return '0';
	case 1: return '1';
	case 2: return '2';
	case 3: return '3';
	case 4: return '4';
	case 5: return '5';
	case 6: return '6';
	case 7: return '7';
	case 8: return '8';
	case 9: return '9';
	case 10: return 'A';
	case 11: return 'B';
	case 12: return 'C';
	case 13: return 'D';
	case 14: return 'E';
	case 15: return 'F';
	default: throw logic_error("ValueToHexChar of nonhex value");
	}
}

// hex dump memory range
string HexDump(const void* pdata, size_t nbytes)
{
	unsigned char* p = (unsigned char*) pdata;

	string s(nbytes*2, '?');

	for (size_t i = 0; i < nbytes; i++)
	{
		s[2*i+0] = ValueToHexChar((p[i] & 0xF0) >> 4);
		s[2*i+1] = ValueToHexChar((p[i] & 0x0F) >> 0);
	}

	return s;
}

// DebugPostTokenOutputStream: helper class to produce PA2 output format
struct DebugPostTokenOutputStream
{
	// output: invalid <source>
	void emit_invalid(const string& source)
	{
		cout << "invalid " << source << endl;
	}

	// output: simple <source> <token_type>
	void emit_simple(const string& source, ETokenType token_type)
	{
		cout << "simple " << source << " " << TokenTypeToStringMap.at(token_type) << endl;
	}

	// output: identifier <source>
	void emit_identifier(const string& source)
	{
		cout << "identifier " << source << endl;
	}

	// output: literal <source> <type> <hexdump(data,nbytes)>
	void emit_literal(const string& source, EFundamentalType type, const void* data, size_t nbytes)
	{
		cout << "literal " << source << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: literal <source> array of <num_elements> <type> <hexdump(data,nbytes)>
	void emit_literal_array(const string& source, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes)
	{
		cout << "literal " << source << " array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> character <type> <hexdump(data,nbytes)>
	void emit_user_defined_literal_character(const string& source, const string& ud_suffix, EFundamentalType type, const void* data, size_t nbytes)
	{
		cout << "user-defined-literal " << source << " " << ud_suffix << " character " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> string array of <num_elements> <type> <hexdump(data, nbytes)>
	void emit_user_defined_literal_string_array(const string& source, const string& ud_suffix, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes)
	{
		cout << "user-defined-literal " << source << " " << ud_suffix << " string array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> <prefix>
	void emit_user_defined_literal_integer(const string& source, const string& ud_suffix, const string& prefix)
	{
		cout << "user-defined-literal " << source << " " << ud_suffix << " integer " << prefix << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> <prefix>
	void emit_user_defined_literal_floating(const string& source, const string& ud_suffix, const string& prefix)
	{
		cout << "user-defined-literal " << source << " " << ud_suffix << " floating " << prefix << endl;
	}

	// output : eof
	void emit_eof()
	{
		cout << "eof" << endl;
	}
};


// use these 3 functions to scan `floating-literals` (see PA2)
// for example PA2Decode_float("12.34") returns "12.34" as a `float` type
float PA2Decode_float(const string& s)
{
	istringstream iss(s);
	float x;
	iss >> x;
	return x;
}

double PA2Decode_double(const string& s)
{
	istringstream iss(s);
	double x;
	iss >> x;
	return x;
}

long double PA2Decode_long_double(const string& s)
{
	istringstream iss(s);
	long double x;
	iss >> x;
	return x;
}


struct PPToken
{
	string type;
	string data;
};

struct CollectPPTokenStream : IPPTokenStream
{
	vector<PPToken> tokens;

	void emit_whitespace_sequence() override { tokens.push_back({"whitespace-sequence", ""}); }
	void emit_new_line() override { tokens.push_back({"new-line", ""}); }
	void emit_header_name(const string& data) override { tokens.push_back({"header-name", data}); }
	void emit_identifier(const string& data) override { tokens.push_back({"identifier", data}); }
	void emit_pp_number(const string& data) override { tokens.push_back({"pp-number", data}); }
	void emit_character_literal(const string& data) override { tokens.push_back({"character-literal", data}); }
	void emit_user_defined_character_literal(const string& data) override { tokens.push_back({"user-defined-character-literal", data}); }
	void emit_string_literal(const string& data) override { tokens.push_back({"string-literal", data}); }
	void emit_user_defined_string_literal(const string& data) override { tokens.push_back({"user-defined-string-literal", data}); }
	void emit_preprocessing_op_or_punc(const string& data) override { tokens.push_back({"preprocessing-op-or-punc", data}); }
	void emit_non_whitespace_char(const string& data) override { tokens.push_back({"non-whitespace-character", data}); }
	void emit_eof() override { tokens.push_back({"eof", ""}); }
};

vector<PPToken> TokenizeToPPTokens(const string& input)
{
	vector<int> cps = DecodeUTF8(input);
	CollectPPTokenStream stream;
	PPTokenizer tokenizer(stream);
	for (int cp : cps)
	{
		tokenizer.process(cp);
	}
	tokenizer.process(EndOfFile);
	return stream.tokens;
}

bool IsIdStart(char c)
{
	return c == '_' || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool IsIdContinue(char c)
{
	return IsIdStart(c) || ('0' <= c && c <= '9');
}

bool IsHexChar(char c)
{
	return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

int HexVal(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return 10 + (c - 'a');
	return 10 + (c - 'A');
}

bool DecodeOneUTF8(const string& s, size_t& i, int& cp)
{
	if (i >= s.size()) return false;
	unsigned char b0 = static_cast<unsigned char>(s[i++]);
	if ((b0 & 0x80) == 0)
	{
		cp = b0;
		return true;
	}
	if ((b0 & 0xE0) == 0xC0)
	{
		if (i >= s.size()) return false;
		unsigned char b1 = static_cast<unsigned char>(s[i++]);
		if ((b1 & 0xC0) != 0x80) return false;
		cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
		return cp >= 0x80;
	}
	if ((b0 & 0xF0) == 0xE0)
	{
		if (i + 1 >= s.size()) return false;
		unsigned char b1 = static_cast<unsigned char>(s[i++]);
		unsigned char b2 = static_cast<unsigned char>(s[i++]);
		if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
		cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
		return cp >= 0x800;
	}
	if ((b0 & 0xF8) == 0xF0)
	{
		if (i + 2 >= s.size()) return false;
		unsigned char b1 = static_cast<unsigned char>(s[i++]);
		unsigned char b2 = static_cast<unsigned char>(s[i++]);
		unsigned char b3 = static_cast<unsigned char>(s[i++]);
		if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
		cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
		return 0x10000 <= cp && cp <= 0x10FFFF;
	}
	return false;
}

bool IsValidCodePoint(int cp)
{
	return 0 <= cp && cp <= 0x10FFFF && !(0xD800 <= cp && cp <= 0xDFFF);
}

bool DecodeEscape(const string& s, size_t& i, int& cp)
{
	if (i >= s.size()) return false;
	char c = s[i++];
	switch (c)
	{
	case '\'': cp = '\''; return true;
	case '"': cp = '"'; return true;
	case '?': cp = '?'; return true;
	case '\\': cp = '\\'; return true;
	case 'a': cp = '\a'; return true;
	case 'b': cp = '\b'; return true;
	case 'f': cp = '\f'; return true;
	case 'n': cp = '\n'; return true;
	case 'r': cp = '\r'; return true;
	case 't': cp = '\t'; return true;
	case 'v': cp = '\v'; return true;
	case 'x':
	{
		if (i >= s.size() || !IsHexChar(s[i])) return false;
		unsigned long long v = 0;
		while (i < s.size() && IsHexChar(s[i]))
		{
			v = (v << 4) | static_cast<unsigned long long>(HexVal(s[i]));
			if (v > 0x10FFFFULL) return false;
			i++;
		}
		cp = static_cast<int>(v);
		return IsValidCodePoint(cp);
	}
	case 'u':
	case 'U':
	{
		size_t digits = (c == 'u') ? 4 : 8;
		if (i + digits > s.size()) return false;
		unsigned long long v = 0;
		for (size_t k = 0; k < digits; k++)
		{
			if (!IsHexChar(s[i + k])) return false;
			v = (v << 4) | static_cast<unsigned long long>(HexVal(s[i + k]));
		}
		i += digits;
		cp = static_cast<int>(v);
		return IsValidCodePoint(cp);
	}
	default:
		if ('0' <= c && c <= '7')
		{
			unsigned v = c - '0';
			for (int k = 0; k < 2 && i < s.size() && ('0' <= s[i] && s[i] <= '7'); k++)
			{
				v = (v << 3) | static_cast<unsigned>(s[i] - '0');
				i++;
			}
			cp = static_cast<int>(v);
			return IsValidCodePoint(cp);
		}
		return false;
	}
}

bool ParseQuoted(const string& s, size_t quote_pos, char quote, vector<int>& cps, size_t& end_pos)
{
	size_t i = quote_pos + 1;
	while (i < s.size())
	{
		if (s[i] == quote)
		{
			end_pos = i + 1;
			return true;
		}
		if (s[i] == '\n') return false;
		if (s[i] == '\\')
		{
			i++;
			int cp = 0;
			if (!DecodeEscape(s, i, cp)) return false;
			cps.push_back(cp);
			continue;
		}
		int cp = 0;
		if (!DecodeOneUTF8(s, i, cp)) return false;
		cps.push_back(cp);
	}
	return false;
}

struct ParsedStringPiece
{
	bool ok = false;
	string prefix;
	string ud_suffix;
	vector<int> cps;
};

ParsedStringPiece ParseStringPiece(const PPToken& tok)
{
	ParsedStringPiece out;
	const string& s = tok.data;
	size_t i = 0;
	if (s.compare(0, 2, "u8") == 0) { out.prefix = "u8"; i = 2; }
	else if (!s.empty() && (s[0] == 'u' || s[0] == 'U' || s[0] == 'L')) { out.prefix = s.substr(0, 1); i = 1; }

	size_t literal_end = string::npos;
	if (i < s.size() && s[i] == 'R')
	{
		if (i + 1 >= s.size() || s[i + 1] != '"') return out;
		size_t j = i + 2;
		string delim;
		while (j < s.size() && s[j] != '(')
		{
			delim.push_back(s[j]);
			j++;
		}
		if (j >= s.size()) return out;
		size_t content_start = j + 1;
		string close = ")" + delim + "\"";
		size_t p = s.find(close, content_start);
		if (p == string::npos) return out;
		string raw = s.substr(content_start, p - content_start);
		out.cps = DecodeUTF8(raw);
		literal_end = p + close.size();
	}
	else
	{
		if (i >= s.size() || s[i] != '"') return out;
		size_t end_pos = 0;
		if (!ParseQuoted(s, i, '"', out.cps, end_pos)) return out;
		literal_end = end_pos;
	}

	if (tok.type == "user-defined-string-literal")
	{
		if (literal_end >= s.size() || s[literal_end] != '_') return out;
		size_t j = literal_end + 1;
		while (j < s.size() && IsIdContinue(s[j])) j++;
		if (j != s.size()) return out;
		out.ud_suffix = s.substr(literal_end);
	}
	else if (literal_end != s.size())
	{
		return out;
	}

	out.ok = true;
	return out;
}

struct ParsedChar
{
	bool ok = false;
	string ud_suffix;
	EFundamentalType type = FT_INT;
	vector<unsigned char> bytes;
};

ParsedChar ParseCharacterLiteral(const PPToken& tok)
{
	ParsedChar out;
	const string& s = tok.data;
	size_t i = 0;
	string prefix;
	if (!s.empty() && (s[0] == 'u' || s[0] == 'U' || s[0] == 'L')) { prefix = s.substr(0, 1); i = 1; }
	if (i >= s.size() || s[i] != '\'') return out;
	vector<int> cps;
	size_t end_pos = 0;
	if (!ParseQuoted(s, i, '\'', cps, end_pos)) return out;
	if (cps.size() != 1 || !IsValidCodePoint(cps[0])) return out;
	if (tok.type == "user-defined-character-literal")
	{
		if (end_pos >= s.size() || s[end_pos] != '_') return out;
		size_t j = end_pos + 1;
		while (j < s.size() && IsIdContinue(s[j])) j++;
		if (j != s.size()) return out;
		out.ud_suffix = s.substr(end_pos);
	}
	else if (end_pos != s.size())
	{
		return out;
	}

	int cp = cps[0];
	if (prefix.empty())
	{
		if (cp <= 127)
		{
			out.type = FT_CHAR;
			unsigned char v = static_cast<unsigned char>(cp);
			out.bytes.assign(&v, &v + 1);
		}
		else
		{
			out.type = FT_INT;
			int v = cp;
			unsigned char* p = reinterpret_cast<unsigned char*>(&v);
			out.bytes.assign(p, p + sizeof(v));
		}
	}
	else if (prefix == "u")
	{
		if (cp > 0xFFFF) return out;
		out.type = FT_CHAR16_T;
		char16_t v = static_cast<char16_t>(cp);
		unsigned char* p = reinterpret_cast<unsigned char*>(&v);
		out.bytes.assign(p, p + sizeof(v));
	}
	else if (prefix == "U")
	{
		out.type = FT_CHAR32_T;
		char32_t v = static_cast<char32_t>(cp);
		unsigned char* p = reinterpret_cast<unsigned char*>(&v);
		out.bytes.assign(p, p + sizeof(v));
	}
	else
	{
		out.type = FT_WCHAR_T;
		wchar_t v = static_cast<wchar_t>(cp);
		unsigned char* p = reinterpret_cast<unsigned char*>(&v);
		out.bytes.assign(p, p + sizeof(v));
	}
	out.ok = true;
	return out;
}

struct ParsedInteger
{
	bool ok = false;
	bool ud = false;
	string ud_suffix;
	string core;
	string suffix;
	int base = 10;
	unsigned long long value = 0;
	bool nondecimal = false;
};

bool ParseIntegerSuffix(const string& s)
{
	if (s.empty()) return true;
	if (!regex_match(s, regex("^(?:[uU](?:[lL]{1,2})?|(?:[lL]{1,2})[uU]?)$"))) return false;
	size_t p = s.find_first_of("lL");
	if (p != string::npos && p + 1 < s.size() && (s[p + 1] == 'l' || s[p + 1] == 'L') && s[p] != s[p + 1]) return false;
	return true;
}

ParsedInteger ParseIntegerLiteral(const string& s)
{
	ParsedInteger out;
	size_t ud_pos = s.find('_');
	string base_part = s;
	if (ud_pos != string::npos)
	{
		out.ud = true;
		out.ud_suffix = s.substr(ud_pos);
		base_part = s.substr(0, ud_pos);
		if (out.ud_suffix.size() < 2 || !IsIdStart(out.ud_suffix[1])) return out;
		for (size_t i = 2; i < out.ud_suffix.size(); i++) if (!IsIdContinue(out.ud_suffix[i])) return out;
	}

	size_t cut = base_part.size();
	while (cut > 0 && (base_part[cut - 1] == 'u' || base_part[cut - 1] == 'U' || base_part[cut - 1] == 'l' || base_part[cut - 1] == 'L'))
	{
		cut--;
	}
	out.core = base_part.substr(0, cut);
	out.suffix = base_part.substr(cut);
	if (out.core.empty()) return out;
	if (out.ud && !out.suffix.empty()) return out;
	if (!ParseIntegerSuffix(out.suffix)) return out;

	if (out.core.size() >= 2 && out.core[0] == '0' && (out.core[1] == 'x' || out.core[1] == 'X'))
	{
		out.base = 16;
		out.nondecimal = true;
		if (out.core.size() == 2) return out;
		for (size_t i = 2; i < out.core.size(); i++) if (!IsHexChar(out.core[i])) return out;
	}
	else if (out.core.size() > 1 && out.core[0] == '0')
	{
		out.base = 8;
		out.nondecimal = true;
		for (char c : out.core) if (c < '0' || c > '7') return out;
	}
	else
	{
		out.base = 10;
		for (char c : out.core) if (c < '0' || c > '9') return out;
	}

	if (!out.ud)
	{
		unsigned __int128 v = 0;
		size_t start = (out.base == 16) ? 2 : 0;
		for (size_t i = start; i < out.core.size(); i++)
		{
			int d = (out.base == 16) ? HexVal(out.core[i]) : (out.core[i] - '0');
			v = v * static_cast<unsigned>(out.base) + static_cast<unsigned>(d);
			if (v > numeric_limits<unsigned long long>::max()) return out;
		}
		out.value = static_cast<unsigned long long>(v);
	}
	out.ok = true;
	return out;
}

bool ParseFloatingLiteral(const string& s, bool& ud, string& ud_suffix, string& prefix, EFundamentalType& ty)
{
	ud = false;
	ud_suffix.clear();
	prefix = s;
	string n = s;
	size_t p = s.find('_');
	if (p != string::npos)
	{
		ud = true;
		ud_suffix = s.substr(p);
		if (ud_suffix.size() < 2 || !IsIdStart(ud_suffix[1])) return false;
		for (size_t i = 2; i < ud_suffix.size(); i++) if (!IsIdContinue(ud_suffix[i])) return false;
		n = s.substr(0, p);
		prefix = n;
	}

	char suf = 0;
	if (!n.empty() && (n.back() == 'f' || n.back() == 'F' || n.back() == 'l' || n.back() == 'L'))
	{
		suf = n.back();
		n.pop_back();
		if (ud) prefix = n;
	}

	static const regex float_re("^((([0-9]+\\.[0-9]*|\\.[0-9]+)([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+))$");
	if (!regex_match(n, float_re)) return false;

	if (suf == 'f' || suf == 'F') ty = FT_FLOAT;
	else if (suf == 'l' || suf == 'L') ty = FT_LONG_DOUBLE;
	else ty = FT_DOUBLE;
	return true;
}

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		string input = oss.str();

		vector<PPToken> pptoks = TokenizeToPPTokens(input);
		DebugPostTokenOutputStream output;

		for (size_t i = 0; i < pptoks.size(); i++)
		{
			const PPToken& t = pptoks[i];
			if (t.type == "eof") break;
			if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

			if (t.type == "header-name" || t.type == "non-whitespace-character")
			{
				output.emit_invalid(t.data);
				continue;
			}

			if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
			{
				if (t.data == "#" || t.data == "##" || t.data == "%:" || t.data == "%:%:")
				{
					output.emit_invalid(t.data);
					continue;
				}
				auto it = StringToTokenTypeMap.find(t.data);
				if (it != StringToTokenTypeMap.end())
				{
					output.emit_simple(t.data, it->second);
				}
				else if (t.type == "identifier")
				{
					output.emit_identifier(t.data);
				}
				else
				{
					output.emit_invalid(t.data);
				}
				continue;
			}

			if (t.type == "pp-number")
			{
				bool is_hex_int_style = t.data.size() >= 2 && t.data[0] == '0' && (t.data[1] == 'x' || t.data[1] == 'X');
				bool is_float_like = !is_hex_int_style && ((t.data.find('.') != string::npos) || (t.data.find('e') != string::npos) || (t.data.find('E') != string::npos));
				if (is_float_like)
				{
					bool ud = false;
					string ud_suffix;
					string prefix;
					EFundamentalType ty = FT_DOUBLE;
					if (!ParseFloatingLiteral(t.data, ud, ud_suffix, prefix, ty))
					{
						output.emit_invalid(t.data);
						continue;
					}
					if (ud)
					{
						output.emit_user_defined_literal_floating(t.data, ud_suffix, prefix);
						continue;
					}
					if (ty == FT_FLOAT)
					{
						float v = PA2Decode_float(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					else if (ty == FT_LONG_DOUBLE)
					{
						long double v = PA2Decode_long_double(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					else
					{
						double v = PA2Decode_double(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					continue;
				}

				ParsedInteger pi = ParseIntegerLiteral(t.data);
				if (!pi.ok)
				{
					output.emit_invalid(t.data);
					continue;
				}
				if (pi.ud)
				{
					output.emit_user_defined_literal_integer(t.data, pi.ud_suffix, pi.core);
					continue;
				}

				auto fits_signed = [&](long long mx) { return pi.value <= static_cast<unsigned long long>(mx); };
				auto fits_unsigned = [&](unsigned long long mx) { return pi.value <= mx; };
				vector<EFundamentalType> order;
				string suf = pi.suffix;
				bool has_u = (suf.find('u') != string::npos) || (suf.find('U') != string::npos);
				int lcount = 0;
				for (char c : suf) if (c == 'l' || c == 'L') lcount++;

				if (!pi.nondecimal)
				{
					if (!has_u && lcount == 0) order = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
					else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_LONG_LONG_INT};
					else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT};
					else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else order = {FT_UNSIGNED_LONG_LONG_INT};
				}
				else
				{
					if (!has_u && lcount == 0) order = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else order = {FT_UNSIGNED_LONG_LONG_INT};
				}

				bool emitted = false;
				for (EFundamentalType ty : order)
				{
					if (ty == FT_INT && fits_signed(numeric_limits<int>::max())) { int v = static_cast<int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_LONG_INT && fits_signed(numeric_limits<long long>::max())) { long int v = static_cast<long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_LONG_LONG_INT && fits_signed(numeric_limits<long long>::max())) { long long int v = static_cast<long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_INT && fits_unsigned(numeric_limits<unsigned int>::max())) { unsigned int v = static_cast<unsigned int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_LONG_INT) { unsigned long int v = static_cast<unsigned long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_LONG_LONG_INT) { unsigned long long int v = static_cast<unsigned long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				}
				if (!emitted) output.emit_invalid(t.data);
				continue;
			}

			if (t.type == "character-literal" || t.type == "user-defined-character-literal")
			{
				ParsedChar pc = ParseCharacterLiteral(t);
				if (!pc.ok)
				{
					output.emit_invalid(t.data);
				}
				else if (pc.ud_suffix.empty())
				{
					output.emit_literal(t.data, pc.type, pc.bytes.data(), pc.bytes.size());
				}
				else
				{
					output.emit_user_defined_literal_character(t.data, pc.ud_suffix, pc.type, pc.bytes.data(), pc.bytes.size());
				}
				continue;
			}

			if (t.type == "string-literal" || t.type == "user-defined-string-literal")
			{
				size_t j = i;
				vector<PPToken> seq;
				while (j < pptoks.size())
				{
					if (pptoks[j].type == "whitespace-sequence" || pptoks[j].type == "new-line")
					{
						j++;
						continue;
					}
					if (pptoks[j].type == "string-literal" || pptoks[j].type == "user-defined-string-literal")
					{
						seq.push_back(pptoks[j]);
						j++;
						continue;
					}
					break;
				}
				i = j - 1;

				string src;
				for (size_t k = 0; k < seq.size(); k++) { if (k) src += " "; src += seq[k].data; }

				vector<ParsedStringPiece> parts;
				bool ok = true;
				for (const PPToken& p : seq)
				{
					ParsedStringPiece ps = ParseStringPiece(p);
					if (!ps.ok) { ok = false; break; }
					parts.push_back(move(ps));
				}
				if (!ok) { output.emit_invalid(src); continue; }

				set<string> encs;
				set<string> uds;
				for (const auto& p : parts)
				{
					if (!p.prefix.empty()) encs.insert(p.prefix);
					if (!p.ud_suffix.empty()) uds.insert(p.ud_suffix);
				}
				if (encs.size() > 1 || uds.size() > 1) { output.emit_invalid(src); continue; }
				string enc = encs.empty() ? "" : *encs.begin();
				string udsuf = uds.empty() ? "" : *uds.begin();

				vector<int> cps;
				for (const auto& p : parts) cps.insert(cps.end(), p.cps.begin(), p.cps.end());
				cps.push_back(0);

				vector<unsigned char> bytes;
				size_t num_elements = 0;
				EFundamentalType ty = FT_CHAR;
				if (enc.empty() || enc == "u8")
				{
					ty = FT_CHAR;
					string s8;
					for (int cp : cps) s8 += EncodeUTF8One(cp);
					bytes.assign(s8.begin(), s8.end());
					num_elements = bytes.size();
				}
				else if (enc == "u")
				{
					ty = FT_CHAR16_T;
					for (int cp : cps)
					{
						if (!IsValidCodePoint(cp)) { ok = false; break; }
						if (cp <= 0xFFFF)
						{
							char16_t v = static_cast<char16_t>(cp);
							unsigned char* p = reinterpret_cast<unsigned char*>(&v);
							bytes.insert(bytes.end(), p, p + 2);
							num_elements++;
						}
						else
						{
							unsigned x = static_cast<unsigned>(cp - 0x10000);
							char16_t hi = static_cast<char16_t>(0xD800 | ((x >> 10) & 0x3FF));
							char16_t lo = static_cast<char16_t>(0xDC00 | (x & 0x3FF));
							unsigned char* p1 = reinterpret_cast<unsigned char*>(&hi);
							unsigned char* p2 = reinterpret_cast<unsigned char*>(&lo);
							bytes.insert(bytes.end(), p1, p1 + 2);
							bytes.insert(bytes.end(), p2, p2 + 2);
							num_elements += 2;
						}
					}
				}
				else if (enc == "U" || enc == "L")
				{
					ty = (enc == "U") ? FT_CHAR32_T : FT_WCHAR_T;
					for (int cp : cps)
					{
						if (!IsValidCodePoint(cp)) { ok = false; break; }
						uint32_t v = static_cast<uint32_t>(cp);
						unsigned char* p = reinterpret_cast<unsigned char*>(&v);
						bytes.insert(bytes.end(), p, p + 4);
						num_elements++;
					}
				}
				if (!ok) { output.emit_invalid(src); continue; }

				if (udsuf.empty())
				{
					output.emit_literal_array(src, num_elements, ty, bytes.data(), bytes.size());
				}
				else
				{
					output.emit_user_defined_literal_string_array(src, udsuf, num_elements, ty, bytes.data(), bytes.size());
				}
				continue;
			}

			output.emit_invalid(t.data);
		}

		output.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
