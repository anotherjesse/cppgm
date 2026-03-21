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
#include <limits>
#include <map>

using namespace std;

#include "PPLexer.h"

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
	explicit DebugPostTokenOutputStream(ostream& out_)
		: out(out_)
	{
	}

	// output: invalid <source>
	void emit_invalid(const string& source)
	{
		out << "invalid " << source << endl;
	}

	// output: simple <source> <token_type>
	void emit_simple(const string& source, ETokenType token_type)
	{
		out << "simple " << source << " " << TokenTypeToStringMap.at(token_type) << endl;
	}

	// output: identifier <source>
	void emit_identifier(const string& source)
	{
		out << "identifier " << source << endl;
	}

	// output: literal <source> <type> <hexdump(data,nbytes)>
	void emit_literal(const string& source, EFundamentalType type, const void* data, size_t nbytes)
	{
		out << "literal " << source << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: literal <source> array of <num_elements> <type> <hexdump(data,nbytes)>
	void emit_literal_array(const string& source, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes)
	{
		out << "literal " << source << " array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> character <type> <hexdump(data,nbytes)>
	void emit_user_defined_literal_character(const string& source, const string& ud_suffix, EFundamentalType type, const void* data, size_t nbytes)
	{
		out << "user-defined-literal " << source << " " << ud_suffix << " character " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> string array of <num_elements> <type> <hexdump(data, nbytes)>
	void emit_user_defined_literal_string_array(const string& source, const string& ud_suffix, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes)
	{
		out << "user-defined-literal " << source << " " << ud_suffix << " string array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> <prefix>
	void emit_user_defined_literal_integer(const string& source, const string& ud_suffix, const string& prefix)
	{
		out << "user-defined-literal " << source << " " << ud_suffix << " integer " << prefix << endl;
	}

	// output: user-defined-literal <source> <ud_suffix> <prefix>
	void emit_user_defined_literal_floating(const string& source, const string& ud_suffix, const string& prefix)
	{
		out << "user-defined-literal " << source << " " << ud_suffix << " floating " << prefix << endl;
	}

	// output : eof
	void emit_eof()
	{
		out << "eof" << endl;
	}

	ostream& out;
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

bool IsAsciiIdentifierStartChar(char c)
{
	return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool IsAsciiIdentifierContinueChar(char c)
{
	return IsAsciiIdentifierStartChar(c) || (c >= '0' && c <= '9');
}

bool IsValidUdSuffix(const string& suffix);

bool IsHexDigitChar(char c)
{
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

int HexCharValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	return 10 + c - 'A';
}

bool DecodeUtf8One(const string& s, size_t& i, int& cp)
{
	if (i >= s.size())
		return false;

	unsigned char lead = static_cast<unsigned char>(s[i]);
	if (lead <= 0x7F)
	{
		cp = lead;
		++i;
		return true;
	}

	int length = 0;
	int value = 0;
	if ((lead & 0xE0) == 0xC0)
	{
		length = 2;
		value = lead & 0x1F;
	}
	else if ((lead & 0xF0) == 0xE0)
	{
		length = 3;
		value = lead & 0x0F;
	}
	else if ((lead & 0xF8) == 0xF0)
	{
		length = 4;
		value = lead & 0x07;
	}
	else
	{
		return false;
	}

	if (i + static_cast<size_t>(length) > s.size())
		return false;

	for (int j = 1; j < length; ++j)
	{
		unsigned char cont = static_cast<unsigned char>(s[i + j]);
		if ((cont & 0xC0) != 0x80)
			return false;
		value = (value << 6) | (cont & 0x3F);
	}

	if ((length == 2 && value < 0x80) ||
		(length == 3 && value < 0x800) ||
		(length == 4 && value < 0x10000) ||
		value > 0x10FFFF ||
		(value >= 0xD800 && value <= 0xDFFF))
	{
		return false;
	}

	cp = value;
	i += static_cast<size_t>(length);
	return true;
}

const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted =
{
	{0xA8,0xA8},
	{0xAA,0xAA},
	{0xAD,0xAD},
	{0xAF,0xAF},
	{0xB2,0xB5},
	{0xB7,0xBA},
	{0xBC,0xBE},
	{0xC0,0xD6},
	{0xD8,0xF6},
	{0xF8,0xFF},
	{0x100,0x167F},
	{0x1681,0x180D},
	{0x180F,0x1FFF},
	{0x200B,0x200D},
	{0x202A,0x202E},
	{0x203F,0x2040},
	{0x2054,0x2054},
	{0x2060,0x206F},
	{0x2070,0x218F},
	{0x2460,0x24FF},
	{0x2776,0x2793},
	{0x2C00,0x2DFF},
	{0x2E80,0x2FFF},
	{0x3004,0x3007},
	{0x3021,0x302F},
	{0x3031,0x303F},
	{0x3040,0xD7FF},
	{0xF900,0xFD3D},
	{0xFD40,0xFDCF},
	{0xFDF0,0xFE44},
	{0xFE47,0xFFFD},
	{0x10000,0x1FFFD},
	{0x20000,0x2FFFD},
	{0x30000,0x3FFFD},
	{0x40000,0x4FFFD},
	{0x50000,0x5FFFD},
	{0x60000,0x6FFFD},
	{0x70000,0x7FFFD},
	{0x80000,0x8FFFD},
	{0x90000,0x9FFFD},
	{0xA0000,0xAFFFD},
	{0xB0000,0xBFFFD},
	{0xC0000,0xCFFFD},
	{0xD0000,0xDFFFD},
	{0xE0000,0xEFFFD}
};

const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted =
{
	{0x300,0x36F},
	{0x1DC0,0x1DFF},
	{0x20D0,0x20FF},
	{0xFE20,0xFE2F}
};

bool InRanges(const vector<pair<int, int>>& ranges, int cp)
{
	size_t low = 0;
	size_t high = ranges.size();
	while (low < high)
	{
		size_t mid = (low + high) / 2;
		if (cp < ranges[mid].first)
			high = mid;
		else if (cp > ranges[mid].second)
			low = mid + 1;
		else
			return true;
	}
	return false;
}

bool IsIdentifierContinueCodePoint(int cp)
{
	if (cp == '_' || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9'))
		return true;
	return InRanges(AnnexE1_Allowed_RangesSorted, cp) ||
		InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
}

bool IsValidUdSuffix(const string& suffix)
{
	if (suffix.empty() || suffix[0] != '_' || suffix.size() == 1)
		return false;

	size_t i = 1;
	while (i < suffix.size())
	{
		int cp = 0;
		if (!DecodeUtf8One(suffix, i, cp))
			return false;
		if (!IsIdentifierContinueCodePoint(cp))
			return false;
	}
	return true;
}

bool DecodeUtf8String(const string& s, vector<int>& out)
{
	size_t i = 0;
	while (i < s.size())
	{
		int cp = 0;
		if (!DecodeUtf8One(s, i, cp))
			return false;
		out.push_back(cp);
	}
	return true;
}

void AppendUtf8(vector<unsigned char>& out, int cp)
{
	string s;
	if (cp <= 0x7F)
	{
		s.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FF)
	{
		s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
		s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
		s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else
	{
		s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
		s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}

	out.insert(out.end(), s.begin(), s.end());
}

void AppendLittleEndian16(vector<unsigned char>& out, uint16_t v)
{
	out.push_back(static_cast<unsigned char>(v & 0xFF));
	out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
}

void AppendLittleEndian32(vector<unsigned char>& out, uint32_t v)
{
	out.push_back(static_cast<unsigned char>(v & 0xFF));
	out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
	out.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
	out.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
}

bool IsValidUnicodeCodePoint(int cp)
{
	return cp >= 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

bool DecodeEscapeSequence(const string& s, size_t& i, int& cp)
{
	if (i >= s.size() || s[i] != '\\')
		return false;
	++i;
	if (i >= s.size())
		return false;

	char c = s[i++];
	switch (c)
	{
	case '\'':
	case '"':
	case '?':
	case '\\':
		cp = c;
		return true;
	case 'a':
		cp = '\a';
		return true;
	case 'b':
		cp = '\b';
		return true;
	case 'f':
		cp = '\f';
		return true;
	case 'n':
		cp = '\n';
		return true;
	case 'r':
		cp = '\r';
		return true;
	case 't':
		cp = '\t';
		return true;
	case 'v':
		cp = '\v';
		return true;
	case 'x':
	{
		if (i >= s.size() || !IsHexDigitChar(s[i]))
			return false;
		unsigned int value = 0;
		while (i < s.size() && IsHexDigitChar(s[i]))
		{
			if (value > 0x10FFFFu)
				return false;
			value = value * 16u + static_cast<unsigned int>(HexCharValue(s[i]));
			++i;
		}
		cp = static_cast<int>(value);
		return IsValidUnicodeCodePoint(cp);
	}
	default:
		if (c >= '0' && c <= '7')
		{
			unsigned int value = static_cast<unsigned int>(c - '0');
			int count = 1;
			while (count < 3 && i < s.size() && s[i] >= '0' && s[i] <= '7')
			{
				value = value * 8u + static_cast<unsigned int>(s[i] - '0');
				++i;
				++count;
			}
			cp = static_cast<int>(value);
			return IsValidUnicodeCodePoint(cp);
		}
		return false;
	}
}

enum EStringEncoding
{
	SE_CHAR,
	SE_CHAR16,
	SE_CHAR32,
	SE_WCHAR
};

enum EStringPrefixKind
{
	SPK_ORDINARY,
	SPK_UTF8,
	SPK_CHAR16,
	SPK_CHAR32,
	SPK_WCHAR
};

struct IntegerLiteralInfo
{
	bool ok = false;
	unsigned long long value = 0;
	bool decimal = false;
	bool unsigned_suffix = false;
	int length_suffix = 0; // 0 none, 1 long, 2 long long
};

bool ParseUnsignedLongLong(const string& digits, int base, unsigned long long& out)
{
	out = 0;
	for (char c : digits)
	{
		int digit = 0;
		if (c >= '0' && c <= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = 10 + c - 'a';
		else
			digit = 10 + c - 'A';

		if (digit >= base)
			return false;
		if (out > (numeric_limits<unsigned long long>::max() - static_cast<unsigned long long>(digit)) / static_cast<unsigned long long>(base))
			return false;
		out = out * static_cast<unsigned long long>(base) + static_cast<unsigned long long>(digit);
	}
	return true;
}

bool ParseIntegerLiteralInfo(const string& source, IntegerLiteralInfo& out)
{
	int base = 10;
	size_t digit_start = 0;
	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
	{
		base = 16;
		digit_start = 2;
		if (digit_start >= source.size() || !IsHexDigitChar(source[digit_start]))
			return false;
	}
	else if (!source.empty() && source[0] == '0')
	{
		base = 8;
		digit_start = 0;
	}

	size_t i = digit_start;
	if (base == 16)
	{
		while (i < source.size() && IsHexDigitChar(source[i]))
			++i;
	}
	else if (base == 8)
	{
		while (i < source.size() && source[i] >= '0' && source[i] <= '7')
			++i;
		if (i == digit_start)
			++i;
	}
	else
	{
		if (source[0] < '1' || source[0] > '9')
			return false;
		while (i < source.size() && source[i] >= '0' && source[i] <= '9')
			++i;
	}

	string suffix = source.substr(i);
	bool unsigned_suffix = false;
	int length_suffix = 0;

	if (!suffix.empty())
	{
		if (suffix == "u" || suffix == "U")
		{
			unsigned_suffix = true;
		}
		else if (suffix == "l" || suffix == "L")
		{
			length_suffix = 1;
		}
		else if (suffix == "ll" || suffix == "LL")
		{
			length_suffix = 2;
		}
		else if (suffix == "ul" || suffix == "uL" || suffix == "Ul" || suffix == "UL" ||
			suffix == "lu" || suffix == "lU" || suffix == "Lu" || suffix == "LU")
		{
			unsigned_suffix = true;
			length_suffix = 1;
		}
		else if (suffix == "ull" || suffix == "uLL" || suffix == "Ull" || suffix == "ULL" ||
			suffix == "llu" || suffix == "llU" || suffix == "LLu" || suffix == "LLU")
		{
			unsigned_suffix = true;
			length_suffix = 2;
		}
		else
		{
			return false;
		}
	}

	string digits = source.substr(digit_start, i - digit_start);
	unsigned long long value = 0;
	if (!ParseUnsignedLongLong(digits, base, value))
		return false;

	out.ok = true;
	out.value = value;
	out.decimal = base == 10;
	out.unsigned_suffix = unsigned_suffix;
	out.length_suffix = length_suffix;
	return true;
}

bool IsValidIntegerLiteralCore(const string& source)
{
	if (source.empty())
		return false;

	int base = 10;
	size_t digit_start = 0;
	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
	{
		base = 16;
		digit_start = 2;
		if (digit_start >= source.size() || !IsHexDigitChar(source[digit_start]))
			return false;
	}
	else if (source[0] == '0')
	{
		base = 8;
	}

	size_t i = digit_start;
	if (base == 16)
	{
		while (i < source.size() && IsHexDigitChar(source[i]))
			++i;
	}
	else if (base == 8)
	{
		while (i < source.size() && source[i] >= '0' && source[i] <= '7')
			++i;
	}
	else
	{
		if (source[0] < '1' || source[0] > '9')
			return false;
		while (i < source.size() && source[i] >= '0' && source[i] <= '9')
			++i;
	}

	return i == source.size();
}

template<typename T>
bool FitsUnsignedValue(unsigned long long value)
{
	return value <= static_cast<unsigned long long>(numeric_limits<T>::max());
}

EFundamentalType ChooseIntegerType(const IntegerLiteralInfo& info)
{
	vector<EFundamentalType> candidates;
	if (info.decimal)
	{
		if (!info.unsigned_suffix && info.length_suffix == 0)
			candidates = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
		else if (!info.unsigned_suffix && info.length_suffix == 1)
			candidates = {FT_LONG_INT, FT_LONG_LONG_INT};
		else if (!info.unsigned_suffix && info.length_suffix == 2)
			candidates = {FT_LONG_LONG_INT};
		else if (info.unsigned_suffix && info.length_suffix == 0)
			candidates = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (info.unsigned_suffix && info.length_suffix == 1)
			candidates = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else
			candidates = {FT_UNSIGNED_LONG_LONG_INT};
	}
	else
	{
		if (!info.unsigned_suffix && info.length_suffix == 0)
			candidates = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!info.unsigned_suffix && info.length_suffix == 1)
			candidates = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!info.unsigned_suffix && info.length_suffix == 2)
			candidates = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (info.unsigned_suffix && info.length_suffix == 0)
			candidates = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (info.unsigned_suffix && info.length_suffix == 1)
			candidates = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else
			candidates = {FT_UNSIGNED_LONG_LONG_INT};
	}

	for (EFundamentalType type : candidates)
	{
		switch (type)
		{
		case FT_INT:
			if (FitsUnsignedValue<int>(info.value))
				return type;
			break;
		case FT_UNSIGNED_INT:
			if (FitsUnsignedValue<unsigned int>(info.value))
				return type;
			break;
		case FT_LONG_INT:
			if (FitsUnsignedValue<long int>(info.value))
				return type;
			break;
		case FT_UNSIGNED_LONG_INT:
			if (FitsUnsignedValue<unsigned long int>(info.value))
				return type;
			break;
		case FT_LONG_LONG_INT:
			if (FitsUnsignedValue<long long int>(info.value))
				return type;
			break;
		case FT_UNSIGNED_LONG_LONG_INT:
			if (FitsUnsignedValue<unsigned long long int>(info.value))
				return type;
			break;
		default:
			break;
		}
	}

	return FT_VOID;
}

void EmitIntegerLiteral(DebugPostTokenOutputStream& output, const string& source, EFundamentalType type, unsigned long long value)
{
	switch (type)
	{
	case FT_INT:
	{
		int x = static_cast<int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	case FT_UNSIGNED_INT:
	{
		unsigned int x = static_cast<unsigned int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	case FT_LONG_INT:
	{
		long int x = static_cast<long int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	case FT_UNSIGNED_LONG_INT:
	{
		unsigned long int x = static_cast<unsigned long int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	case FT_LONG_LONG_INT:
	{
		long long int x = static_cast<long long int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	case FT_UNSIGNED_LONG_LONG_INT:
	{
		unsigned long long int x = static_cast<unsigned long long int>(value);
		output.emit_literal(source, type, &x, sizeof(x));
		break;
	}
	default:
		throw logic_error("bad integer literal type");
	}
}

bool ParseFloatingCore(const string& s)
{
	size_t i = 0;
	size_t digits_before = 0;
	while (i < s.size() && s[i] >= '0' && s[i] <= '9')
	{
		++i;
		++digits_before;
	}

	bool has_dot = false;
	size_t digits_after = 0;
	if (i < s.size() && s[i] == '.')
	{
		has_dot = true;
		++i;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9')
		{
			++i;
			++digits_after;
		}
		if (digits_before == 0 && digits_after == 0)
			return false;
	}
	else if (digits_before == 0)
	{
		return false;
	}

	bool has_exp = false;
	if (i < s.size() && (s[i] == 'e' || s[i] == 'E'))
	{
		has_exp = true;
		++i;
		if (i < s.size() && (s[i] == '+' || s[i] == '-'))
			++i;
		size_t exp_digits = 0;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9')
		{
			++i;
			++exp_digits;
		}
		if (exp_digits == 0)
			return false;
	}

	return i == s.size() && (has_dot || has_exp);
}

struct ParsedNumberLiteral
{
	bool ok = false;
	bool is_integer = false;
	bool is_user_defined = false;
	string ud_suffix;
	string prefix;
	IntegerLiteralInfo int_info;
	EFundamentalType float_type = FT_DOUBLE;
};

ParsedNumberLiteral ParsePPNumberLiteral(const string& source)
{
	ParsedNumberLiteral result;

	size_t underscore = source.find('_');
	if (underscore != string::npos)
	{
		string prefix = source.substr(0, underscore);
		string ud_suffix = source.substr(underscore);
		if (!IsValidUdSuffix(ud_suffix))
			return result;

		if (IsValidIntegerLiteralCore(prefix))
		{
			result.ok = true;
			result.is_integer = true;
			result.is_user_defined = true;
			result.ud_suffix = ud_suffix;
			result.prefix = prefix;
			return result;
		}

		if (ParseFloatingCore(prefix))
		{
			result.ok = true;
			result.is_integer = false;
			result.is_user_defined = true;
			result.ud_suffix = ud_suffix;
			result.prefix = prefix;
			return result;
		}

		return result;
	}

	IntegerLiteralInfo int_info;
	if (ParseIntegerLiteralInfo(source, int_info))
	{
		result.ok = true;
		result.is_integer = true;
		result.int_info = int_info;
		return result;
	}

	string core = source;
	EFundamentalType float_type = FT_DOUBLE;
	if (!core.empty())
	{
		char c = core.back();
		if (c == 'f' || c == 'F')
		{
			float_type = FT_FLOAT;
			core.pop_back();
		}
		else if (c == 'l' || c == 'L')
		{
			float_type = FT_LONG_DOUBLE;
			core.pop_back();
		}
	}

	if (ParseFloatingCore(core))
	{
		result.ok = true;
		result.is_integer = false;
		result.float_type = float_type;
		return result;
	}

	return result;
}

struct ParsedCharLiteral
{
	bool ok = false;
	string source;
	bool user_defined = false;
	string ud_suffix;
	EFundamentalType type = FT_VOID;
	uint32_t value = 0;
};

bool DecodeQuotedContent(const string& content, bool raw, vector<int>& cps)
{
	if (raw)
		return DecodeUtf8String(content, cps);

	size_t i = 0;
	while (i < content.size())
	{
		int cp = 0;
		if (content[i] == '\\')
		{
			if (!DecodeEscapeSequence(content, i, cp))
				return false;
		}
		else if (!DecodeUtf8One(content, i, cp))
		{
			return false;
		}

		if (!IsValidUnicodeCodePoint(cp))
			return false;
		cps.push_back(cp);
	}
	return true;
}

ParsedCharLiteral ParseCharacterLiteralToken(const PPToken& token)
{
	ParsedCharLiteral out;
	out.source = token.data;
	out.user_defined = token.type == PPT_USER_DEFINED_CHARACTER_LITERAL;

	size_t pos = 0;
	string prefix;
	if (token.data.compare(0, 1, "u") == 0)
	{
		prefix = "u";
		pos = 1;
	}
	else if (token.data.compare(0, 1, "U") == 0)
	{
		prefix = "U";
		pos = 1;
	}
	else if (token.data.compare(0, 1, "L") == 0)
	{
		prefix = "L";
		pos = 1;
	}

	if (pos >= token.data.size() || token.data[pos] != '\'')
		return out;
	++pos;

	vector<int> cps;
	while (pos < token.data.size())
	{
		if (token.data[pos] == '\'')
		{
			++pos;
			break;
		}
		int cp = 0;
		if (token.data[pos] == '\\')
		{
			if (!DecodeEscapeSequence(token.data, pos, cp))
				return out;
		}
		else if (!DecodeUtf8One(token.data, pos, cp))
		{
			return out;
		}
		cps.push_back(cp);
	}

	if (cps.size() != 1)
		return out;
	if (pos > token.data.size())
		return out;

	string suffix = token.data.substr(pos);
	if (out.user_defined)
	{
		if (!IsValidUdSuffix(suffix))
			return out;
		out.ud_suffix = suffix;
	}
	else if (!suffix.empty())
	{
		return out;
	}

	int cp = cps[0];
	if (!IsValidUnicodeCodePoint(cp))
		return out;

	if (prefix.empty())
	{
		out.type = cp <= 127 ? FT_CHAR : FT_INT;
	}
	else if (prefix == "u")
	{
		if (cp > 0xFFFF)
			return out;
		out.type = FT_CHAR16_T;
	}
	else if (prefix == "U")
	{
		out.type = FT_CHAR32_T;
	}
	else
	{
		out.type = FT_WCHAR_T;
	}

	out.value = static_cast<uint32_t>(cp);
	out.ok = true;
	return out;
}

struct ParsedStringLiteralToken
{
	bool ok = false;
	string source;
	EStringEncoding encoding = SE_CHAR;
	EStringPrefixKind prefix_kind = SPK_ORDINARY;
	bool user_defined = false;
	string ud_suffix;
	vector<int> codepoints;
};

bool MatchPrefix(const string& s, const string& prefix)
{
	return s.compare(0, prefix.size(), prefix) == 0;
}

ParsedStringLiteralToken ParseStringLiteralToken(const PPToken& token)
{
	ParsedStringLiteralToken out;
	out.source = token.data;
	out.user_defined = token.type == PPT_USER_DEFINED_STRING_LITERAL;

	string prefix_text;
	bool raw = false;
	size_t pos = 0;

	if (MatchPrefix(token.data, "u8R"))
	{
		prefix_text = "u8";
		raw = true;
		pos = 3;
	}
	else if (MatchPrefix(token.data, "uR"))
	{
		prefix_text = "u";
		raw = true;
		pos = 2;
	}
	else if (MatchPrefix(token.data, "UR"))
	{
		prefix_text = "U";
		raw = true;
		pos = 2;
	}
	else if (MatchPrefix(token.data, "LR"))
	{
		prefix_text = "L";
		raw = true;
		pos = 2;
	}
	else if (MatchPrefix(token.data, "R"))
	{
		prefix_text = "";
		raw = true;
		pos = 1;
	}
	else if (MatchPrefix(token.data, "u8"))
	{
		prefix_text = "u8";
		pos = 2;
	}
	else if (MatchPrefix(token.data, "u"))
	{
		prefix_text = "u";
		pos = 1;
	}
	else if (MatchPrefix(token.data, "U"))
	{
		prefix_text = "U";
		pos = 1;
	}
	else if (MatchPrefix(token.data, "L"))
	{
		prefix_text = "L";
		pos = 1;
	}

	if (prefix_text.empty() || prefix_text == "u8")
		out.encoding = SE_CHAR;
	else if (prefix_text == "u")
		out.encoding = SE_CHAR16;
	else if (prefix_text == "U")
		out.encoding = SE_CHAR32;
	else
		out.encoding = SE_WCHAR;

	if (prefix_text.empty())
		out.prefix_kind = SPK_ORDINARY;
	else if (prefix_text == "u8")
		out.prefix_kind = SPK_UTF8;
	else if (prefix_text == "u")
		out.prefix_kind = SPK_CHAR16;
	else if (prefix_text == "U")
		out.prefix_kind = SPK_CHAR32;
	else
		out.prefix_kind = SPK_WCHAR;

	if (raw)
	{
		if (pos >= token.data.size() || token.data[pos] != '"')
			return out;
		size_t delim_start = pos + 1;
		size_t paren = token.data.find('(', delim_start);
		if (paren == string::npos)
			return out;
		string delim = token.data.substr(delim_start, paren - delim_start);
		string closing = ")" + delim + "\"";
		size_t close = token.data.rfind(closing);
		if (close == string::npos || close < paren)
			return out;
		string content = token.data.substr(paren + 1, close - (paren + 1));
		string suffix = token.data.substr(close + closing.size());
		if (out.user_defined)
		{
			if (!IsValidUdSuffix(suffix))
				return out;
			out.ud_suffix = suffix;
		}
		else if (!suffix.empty())
		{
			return out;
		}
		if (!DecodeQuotedContent(content, true, out.codepoints))
			return out;
		out.ok = true;
		return out;
	}

	if (pos >= token.data.size() || token.data[pos] != '"')
		return out;
	size_t start = pos + 1;
	size_t i = start;
	vector<int> cps;
	while (i < token.data.size())
	{
		if (token.data[i] == '"')
			break;
		int cp = 0;
		if (token.data[i] == '\\')
		{
			if (!DecodeEscapeSequence(token.data, i, cp))
				return out;
		}
		else if (!DecodeUtf8One(token.data, i, cp))
		{
			return out;
		}
		cps.push_back(cp);
	}

	if (i >= token.data.size() || token.data[i] != '"')
		return out;
	string suffix = token.data.substr(i + 1);
	if (out.user_defined)
	{
		if (!IsValidUdSuffix(suffix))
			return out;
		out.ud_suffix = suffix;
	}
	else if (!suffix.empty())
	{
		return out;
	}

	out.codepoints = move(cps);
	out.ok = true;
	return out;
}

struct EncodedStringData
{
	EFundamentalType type = FT_CHAR;
	size_t num_elements = 0;
	vector<unsigned char> bytes;
};

bool EncodeStringData(const vector<int>& cps, EStringEncoding encoding, EncodedStringData& out)
{
	out.bytes.clear();

	if (encoding == SE_CHAR)
	{
		out.type = FT_CHAR;
		for (int cp : cps)
		{
			if (!IsValidUnicodeCodePoint(cp))
				return false;
			AppendUtf8(out.bytes, cp);
		}
		out.bytes.push_back(0);
		out.num_elements = out.bytes.size();
		return true;
	}

	if (encoding == SE_CHAR16)
	{
		out.type = FT_CHAR16_T;
		size_t units = 0;
		for (int cp : cps)
		{
			if (!IsValidUnicodeCodePoint(cp))
				return false;
			if (cp <= 0xFFFF)
			{
				AppendLittleEndian16(out.bytes, static_cast<uint16_t>(cp));
				++units;
			}
			else
			{
				int x = cp - 0x10000;
				uint16_t hi = static_cast<uint16_t>(0xD800 + (x >> 10));
				uint16_t lo = static_cast<uint16_t>(0xDC00 + (x & 0x3FF));
				AppendLittleEndian16(out.bytes, hi);
				AppendLittleEndian16(out.bytes, lo);
				units += 2;
			}
		}
		AppendLittleEndian16(out.bytes, 0);
		out.num_elements = units + 1;
		return true;
	}

	out.type = encoding == SE_CHAR32 ? FT_CHAR32_T : FT_WCHAR_T;
	for (int cp : cps)
	{
		if (!IsValidUnicodeCodePoint(cp))
			return false;
		AppendLittleEndian32(out.bytes, static_cast<uint32_t>(cp));
	}
	AppendLittleEndian32(out.bytes, 0);
	out.num_elements = cps.size() + 1;
	return true;
}

bool IsStringTokenType(EPPTokenType type)
{
	return type == PPT_STRING_LITERAL || type == PPT_USER_DEFINED_STRING_LITERAL;
}

bool EmitStringSequence(const vector<PPToken>& filtered, size_t& i, DebugPostTokenOutputStream& output)
{
	size_t run_end = i;
	string joined_source;
	while (run_end < filtered.size() && IsStringTokenType(filtered[run_end].type))
	{
		if (!joined_source.empty())
			joined_source += " ";
		joined_source += filtered[run_end].data;
		++run_end;
	}

	size_t j = i;
	vector<ParsedStringLiteralToken> parts;
	bool saw_user_defined = false;
	string ud_suffix;
	EStringPrefixKind chosen_prefix = SPK_ORDINARY;

	while (j < run_end)
	{
		ParsedStringLiteralToken parsed = ParseStringLiteralToken(filtered[j]);
		if (!parsed.ok)
		{
			output.emit_invalid(joined_source);
			i = run_end;
			return true;
		}

		if (parsed.prefix_kind != SPK_ORDINARY)
		{
			if (chosen_prefix == SPK_ORDINARY)
				chosen_prefix = parsed.prefix_kind;
			else if (chosen_prefix != parsed.prefix_kind)
			{
				output.emit_invalid(joined_source);
				i = run_end;
				return true;
			}
		}

		if (parsed.user_defined)
		{
			if (!saw_user_defined)
			{
				ud_suffix = parsed.ud_suffix;
				saw_user_defined = true;
			}
			else if (ud_suffix != parsed.ud_suffix)
			{
				output.emit_invalid(joined_source);
				i = run_end;
				return true;
			}
		}

		parts.push_back(move(parsed));
		++j;
	}

	vector<int> all_codepoints;
	for (const ParsedStringLiteralToken& part : parts)
		all_codepoints.insert(all_codepoints.end(), part.codepoints.begin(), part.codepoints.end());

	EStringEncoding encoding = SE_CHAR;
	if (chosen_prefix == SPK_CHAR16)
		encoding = SE_CHAR16;
	else if (chosen_prefix == SPK_CHAR32)
		encoding = SE_CHAR32;
	else if (chosen_prefix == SPK_WCHAR)
		encoding = SE_WCHAR;

	EncodedStringData encoded;
	if (!EncodeStringData(all_codepoints, encoding, encoded))
	{
		output.emit_invalid(joined_source);
		i = run_end;
		return true;
	}

	if (saw_user_defined)
		output.emit_user_defined_literal_string_array(joined_source, ud_suffix, encoded.num_elements, encoded.type, encoded.bytes.data(), encoded.bytes.size());
	else
		output.emit_literal_array(joined_source, encoded.num_elements, encoded.type, encoded.bytes.data(), encoded.bytes.size());

	i = run_end;
	return true;
}

void EmitPostTokenStream(const vector<PPToken>& filtered, DebugPostTokenOutputStream& output, bool emit_eof)
{
	size_t i = 0;
	while (i < filtered.size())
	{
		const PPToken& token = filtered[i];
		switch (token.type)
		{
		case PPT_EOF:
			if (emit_eof)
				output.emit_eof();
			++i;
			break;

		case PPT_IDENTIFIER:
		{
			auto it = StringToTokenTypeMap.find(token.data);
			if (it != StringToTokenTypeMap.end())
				output.emit_simple(token.data, it->second);
			else
				output.emit_identifier(token.data);
			++i;
			break;
		}

		case PPT_PREPROCESSING_OP_OR_PUNC:
		{
			if (token.data == "#" || token.data == "##" || token.data == "%:" || token.data == "%:%:")
			{
				output.emit_invalid(token.data);
				++i;
				break;
			}

			auto it = StringToTokenTypeMap.find(token.data);
			if (it != StringToTokenTypeMap.end())
				output.emit_simple(token.data, it->second);
			else
				output.emit_invalid(token.data);
			++i;
			break;
		}

		case PPT_PP_NUMBER:
		{
			ParsedNumberLiteral parsed = ParsePPNumberLiteral(token.data);
			if (!parsed.ok)
			{
				output.emit_invalid(token.data);
			}
			else if (parsed.is_user_defined)
			{
				if (parsed.is_integer)
					output.emit_user_defined_literal_integer(token.data, parsed.ud_suffix, parsed.prefix);
				else
					output.emit_user_defined_literal_floating(token.data, parsed.ud_suffix, parsed.prefix);
			}
			else if (parsed.is_integer)
			{
				EFundamentalType type = ChooseIntegerType(parsed.int_info);
				if (type == FT_VOID)
					output.emit_invalid(token.data);
				else
					EmitIntegerLiteral(output, token.data, type, parsed.int_info.value);
			}
			else
			{
				if (parsed.float_type == FT_FLOAT)
				{
					float x = PA2Decode_float(token.data);
					output.emit_literal(token.data, FT_FLOAT, &x, sizeof(x));
				}
				else if (parsed.float_type == FT_LONG_DOUBLE)
				{
					long double x = PA2Decode_long_double(token.data);
					output.emit_literal(token.data, FT_LONG_DOUBLE, &x, sizeof(x));
				}
				else
				{
					double x = PA2Decode_double(token.data);
					output.emit_literal(token.data, FT_DOUBLE, &x, sizeof(x));
				}
			}
			++i;
			break;
		}

		case PPT_CHARACTER_LITERAL:
		case PPT_USER_DEFINED_CHARACTER_LITERAL:
		{
			ParsedCharLiteral parsed = ParseCharacterLiteralToken(token);
			if (!parsed.ok)
			{
				output.emit_invalid(token.data);
			}
			else if (parsed.user_defined)
			{
				switch (parsed.type)
				{
				case FT_CHAR:
				{
					char x = static_cast<char>(parsed.value);
					output.emit_user_defined_literal_character(parsed.source, parsed.ud_suffix, FT_CHAR, &x, sizeof(x));
					break;
				}
				case FT_INT:
				{
					int x = static_cast<int>(parsed.value);
					output.emit_user_defined_literal_character(parsed.source, parsed.ud_suffix, FT_INT, &x, sizeof(x));
					break;
				}
				case FT_CHAR16_T:
				{
					char16_t x = static_cast<char16_t>(parsed.value);
					output.emit_user_defined_literal_character(parsed.source, parsed.ud_suffix, FT_CHAR16_T, &x, sizeof(x));
					break;
				}
				case FT_CHAR32_T:
				{
					char32_t x = static_cast<char32_t>(parsed.value);
					output.emit_user_defined_literal_character(parsed.source, parsed.ud_suffix, FT_CHAR32_T, &x, sizeof(x));
					break;
				}
				case FT_WCHAR_T:
				{
					wchar_t x = static_cast<wchar_t>(parsed.value);
					output.emit_user_defined_literal_character(parsed.source, parsed.ud_suffix, FT_WCHAR_T, &x, sizeof(x));
					break;
				}
				default:
					output.emit_invalid(token.data);
					break;
				}
			}
			else
			{
				switch (parsed.type)
				{
				case FT_CHAR:
				{
					char x = static_cast<char>(parsed.value);
					output.emit_literal(parsed.source, FT_CHAR, &x, sizeof(x));
					break;
				}
				case FT_INT:
				{
					int x = static_cast<int>(parsed.value);
					output.emit_literal(parsed.source, FT_INT, &x, sizeof(x));
					break;
				}
				case FT_CHAR16_T:
				{
					char16_t x = static_cast<char16_t>(parsed.value);
					output.emit_literal(parsed.source, FT_CHAR16_T, &x, sizeof(x));
					break;
				}
				case FT_CHAR32_T:
				{
					char32_t x = static_cast<char32_t>(parsed.value);
					output.emit_literal(parsed.source, FT_CHAR32_T, &x, sizeof(x));
					break;
				}
				case FT_WCHAR_T:
				{
					wchar_t x = static_cast<wchar_t>(parsed.value);
					output.emit_literal(parsed.source, FT_WCHAR_T, &x, sizeof(x));
					break;
				}
				default:
					output.emit_invalid(token.data);
					break;
				}
			}
			++i;
			break;
		}

		case PPT_STRING_LITERAL:
		case PPT_USER_DEFINED_STRING_LITERAL:
			EmitStringSequence(filtered, i, output);
			break;

		case PPT_HEADER_NAME:
		case PPT_NON_WHITESPACE_CHAR:
			output.emit_invalid(token.data);
			++i;
			break;

		case PPT_WHITESPACE_SEQUENCE:
		case PPT_NEW_LINE:
			++i;
			break;
		}
	}
}

void RunPostTokenOnRawInput(const string& input, ostream& out, bool emit_eof)
{
	vector<PPToken> tokens = LexPPTokens(input);
	vector<PPToken> filtered;
	for (const PPToken& token : tokens)
	{
		if (token.type != PPT_WHITESPACE_SEQUENCE && token.type != PPT_NEW_LINE)
			filtered.push_back(token);
	}

	DebugPostTokenOutputStream output(out);
	EmitPostTokenStream(filtered, output, emit_eof);
}

#ifndef CPPGM_POSTTOKEN_LIBRARY
int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		RunPostTokenOnRawInput(oss.str(), cout, true);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
#endif
