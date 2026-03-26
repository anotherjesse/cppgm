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
#include <set>

using namespace std;

// Reuse the PA1 tokenizer implementation in-process for preprocessing-tokenization.
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

namespace pa2
{
enum class PPTokenType
{
	WhitespaceSequence,
	NewLine,
	HeaderName,
	Identifier,
	PPNumber,
	CharacterLiteral,
	UserDefinedCharacterLiteral,
	StringLiteral,
	UserDefinedStringLiteral,
	PreprocessingOpOrPunc,
	NonWhitespaceCharacter
};

struct PPToken
{
	PPTokenType type;
	string source;
};

struct PPTokenCollector : IPPTokenStream
{
	vector<PPToken> tokens;

	void emit_whitespace_sequence() { tokens.push_back({PPTokenType::WhitespaceSequence, ""}); }
	void emit_new_line() { tokens.push_back({PPTokenType::NewLine, ""}); }
	void emit_header_name(const string& data) { tokens.push_back({PPTokenType::HeaderName, data}); }
	void emit_identifier(const string& data) { tokens.push_back({PPTokenType::Identifier, data}); }
	void emit_pp_number(const string& data) { tokens.push_back({PPTokenType::PPNumber, data}); }
	void emit_character_literal(const string& data) { tokens.push_back({PPTokenType::CharacterLiteral, data}); }
	void emit_user_defined_character_literal(const string& data) { tokens.push_back({PPTokenType::UserDefinedCharacterLiteral, data}); }
	void emit_string_literal(const string& data) { tokens.push_back({PPTokenType::StringLiteral, data}); }
	void emit_user_defined_string_literal(const string& data) { tokens.push_back({PPTokenType::UserDefinedStringLiteral, data}); }
	void emit_preprocessing_op_or_punc(const string& data) { tokens.push_back({PPTokenType::PreprocessingOpOrPunc, data}); }
	void emit_non_whitespace_char(const string& data) { tokens.push_back({PPTokenType::NonWhitespaceCharacter, data}); }
	void emit_eof() {}
};

bool IsIdentifierStartAscii(char c)
{
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsIdentifierContinueAscii(char c)
{
	return IsIdentifierStartAscii(c) || (c >= '0' && c <= '9');
}

bool IsValidIdentifierAscii(const string& s)
{
	if (s.empty() || !IsIdentifierStartAscii(s[0]))
		return false;
	for (size_t i = 1; i < s.size(); ++i)
		if (!IsIdentifierContinueAscii(s[i]))
			return false;
	return true;
}

int HexValue(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	return -1;
}

bool DecodeUtf8(const string& s, vector<uint32_t>& out)
{
	out.clear();
	size_t i = 0;
	while (i < s.size())
	{
		unsigned char b0 = static_cast<unsigned char>(s[i]);
		if (b0 <= 0x7F)
		{
			out.push_back(b0);
			++i;
			continue;
		}

		auto cont = [&](size_t idx, unsigned char& b) -> bool
		{
			if (idx >= s.size())
				return false;
			b = static_cast<unsigned char>(s[idx]);
			return (b & 0xC0) == 0x80;
		};

		if (b0 >= 0xC2 && b0 <= 0xDF)
		{
			unsigned char b1;
			if (!cont(i + 1, b1))
				return false;
			out.push_back(((b0 & 0x1F) << 6) | (b1 & 0x3F));
			i += 2;
			continue;
		}

		if (b0 == 0xE0)
		{
			unsigned char b1, b2;
			if (!cont(i + 1, b1) || !cont(i + 2, b2) || b1 < 0xA0 || b1 > 0xBF)
				return false;
			out.push_back(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
			i += 3;
			continue;
		}

		if ((b0 >= 0xE1 && b0 <= 0xEC) || (b0 >= 0xEE && b0 <= 0xEF))
		{
			unsigned char b1, b2;
			if (!cont(i + 1, b1) || !cont(i + 2, b2))
				return false;
			out.push_back(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
			i += 3;
			continue;
		}

		if (b0 == 0xED)
		{
			unsigned char b1, b2;
			if (!cont(i + 1, b1) || !cont(i + 2, b2) || b1 < 0x80 || b1 > 0x9F)
				return false;
			out.push_back(((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F));
			i += 3;
			continue;
		}

		if (b0 == 0xF0)
		{
			unsigned char b1, b2, b3;
			if (!cont(i + 1, b1) || !cont(i + 2, b2) || !cont(i + 3, b3) || b1 < 0x90 || b1 > 0xBF)
				return false;
			out.push_back(((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
			i += 4;
			continue;
		}

		if (b0 >= 0xF1 && b0 <= 0xF3)
		{
			unsigned char b1, b2, b3;
			if (!cont(i + 1, b1) || !cont(i + 2, b2) || !cont(i + 3, b3))
				return false;
			out.push_back(((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
			i += 4;
			continue;
		}

		if (b0 == 0xF4)
		{
			unsigned char b1, b2, b3;
			if (!cont(i + 1, b1) || !cont(i + 2, b2) || !cont(i + 3, b3) || b1 < 0x80 || b1 > 0x8F)
				return false;
			out.push_back(((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F));
			i += 4;
			continue;
		}

		return false;
	}

	return true;
}

bool EncodeUtf8CodePoint(uint32_t cp, string& out)
{
	if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF)
		return false;

	if (cp <= 0x7F)
	{
		out.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FF)
	{
		out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else
	{
		out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}

	return true;
}

bool DecodeEscape(const vector<uint32_t>& cps, size_t& i, uint32_t& cp_out)
{
	if (i + 1 >= cps.size())
		return false;

	uint32_t c = cps[i + 1];
	switch (c)
	{
	case '\'': cp_out = '\''; i += 2; return true;
	case '"': cp_out = '"'; i += 2; return true;
	case '?': cp_out = '?'; i += 2; return true;
	case '\\': cp_out = '\\'; i += 2; return true;
	case 'a': cp_out = '\a'; i += 2; return true;
	case 'b': cp_out = '\b'; i += 2; return true;
	case 'f': cp_out = '\f'; i += 2; return true;
	case 'n': cp_out = '\n'; i += 2; return true;
	case 'r': cp_out = '\r'; i += 2; return true;
	case 't': cp_out = '\t'; i += 2; return true;
	case 'v': cp_out = '\v'; i += 2; return true;
	default: break;
	}

	if (c == 'x')
	{
		size_t j = i + 2;
		if (j >= cps.size())
			return false;

		uint64_t value = 0;
		bool any = false;
		while (j < cps.size())
		{
			int hv = HexValue(static_cast<int>(cps[j]));
			if (hv < 0)
				break;
			any = true;
			value = (value << 4) | static_cast<uint64_t>(hv);
			if (value > 0x10FFFFull)
				return false;
			++j;
		}
		if (!any)
			return false;

		cp_out = static_cast<uint32_t>(value);
		i = j;
		return true;
	}

	if (c >= '0' && c <= '7')
	{
		size_t j = i + 1;
		uint32_t value = 0;
		int count = 0;
		while (j < cps.size() && count < 3 && cps[j] >= '0' && cps[j] <= '7')
		{
			value = value * 8 + static_cast<uint32_t>(cps[j] - '0');
			++j;
			++count;
		}
		cp_out = value;
		i = j;
		return true;
	}

	return false;
}

bool IsValidCodePoint(uint32_t cp)
{
	return cp < 0xD800 || (cp >= 0xE000 && cp <= 0x10FFFF);
}

enum class StringPrefix
{
	None,
	U8,
	U16,
	U32,
	Wide
};

struct ParsedCharacterLiteral
{
	bool ok;
	EFundamentalType type;
	uint32_t value;
	string ud_suffix;
};

struct ParsedStringLiteral
{
	bool ok;
	StringPrefix prefix;
	vector<uint32_t> cps;
	string ud_suffix;
};

bool ParseIdentifierSuffix(const vector<uint32_t>& cps, size_t& i, string& out)
{
	out.clear();
	if (i >= cps.size())
		return true;

	if (cps[i] > 0x7F)
		return false;

	char c0 = static_cast<char>(cps[i]);
	if (!IsIdentifierStartAscii(c0))
		return false;

	out.push_back(c0);
	++i;

	while (i < cps.size())
	{
		if (cps[i] > 0x7F)
			return false;
		char c = static_cast<char>(cps[i]);
		if (!IsIdentifierContinueAscii(c))
			break;
		out.push_back(c);
		++i;
	}

	return true;
}

ParsedCharacterLiteral ParseCharacterLiteral(const string& source, bool expect_ud)
{
	ParsedCharacterLiteral r;
	r.ok = false;
	r.type = FT_INT;
	r.value = 0;

	vector<uint32_t> cps;
	if (!DecodeUtf8(source, cps))
		return r;

	size_t i = 0;
	int prefix = 0;
	if (i + 1 < cps.size() && (cps[i] == 'u' || cps[i] == 'U' || cps[i] == 'L') && cps[i + 1] == '\'')
	{
		prefix = cps[i];
		++i;
	}

	if (i >= cps.size() || cps[i] != '\'')
		return r;
	++i;

	vector<uint32_t> chars;
	while (i < cps.size() && cps[i] != '\'')
	{
		if (cps[i] == '\\')
		{
			uint32_t cp = 0;
			if (!DecodeEscape(cps, i, cp))
				return r;
			chars.push_back(cp);
		}
		else
		{
			if (cps[i] == '\n')
				return r;
			chars.push_back(cps[i]);
			++i;
		}
	}

	if (i >= cps.size() || cps[i] != '\'')
		return r;
	++i;

	string suffix;
	if (!ParseIdentifierSuffix(cps, i, suffix))
		return r;
	if (i != cps.size())
		return r;

	if (expect_ud)
	{
		if (suffix.empty() || suffix[0] != '_')
			return r;
	}
	else if (!suffix.empty())
	{
		return r;
	}

	if (chars.size() != 1)
		return r;

	uint32_t cp = chars[0];
	if (!IsValidCodePoint(cp))
		return r;

	r.ud_suffix = suffix;
	r.value = cp;

	if (prefix == 0)
	{
		r.type = cp <= 127 ? FT_CHAR : FT_INT;
	}
	else if (prefix == 'u')
	{
		if (cp > 0xFFFF)
			return r;
		r.type = FT_CHAR16_T;
	}
	else if (prefix == 'U')
	{
		r.type = FT_CHAR32_T;
	}
	else
	{
		r.type = FT_WCHAR_T;
	}

	r.ok = true;
	return r;
}

bool StartsWithAscii(const vector<uint32_t>& cps, size_t i, const string& s)
{
	if (i + s.size() > cps.size())
		return false;
	for (size_t k = 0; k < s.size(); ++k)
		if (cps[i + k] != static_cast<unsigned char>(s[k]))
			return false;
	return true;
}

bool IsValidRawDelimChar(uint32_t cp)
{
	if (cp == ' ' || cp == '(' || cp == ')' || cp == '\\')
		return false;
	if (cp == '\t' || cp == '\v' || cp == '\f' || cp == '\n' || cp == '\r')
		return false;
	return cp >= 0x20 && cp <= 0x7E;
}

ParsedStringLiteral ParseStringLiteral(const string& source, bool expect_ud)
{
	ParsedStringLiteral r;
	r.ok = false;
	r.prefix = StringPrefix::None;

	vector<uint32_t> cps;
	if (!DecodeUtf8(source, cps))
		return r;

	size_t i = 0;
	bool raw = false;

	if (StartsWithAscii(cps, i, "u8R\""))
	{
		r.prefix = StringPrefix::U8;
		raw = true;
		i += 3;
	}
	else if (StartsWithAscii(cps, i, "uR\""))
	{
		r.prefix = StringPrefix::U16;
		raw = true;
		i += 2;
	}
	else if (StartsWithAscii(cps, i, "UR\""))
	{
		r.prefix = StringPrefix::U32;
		raw = true;
		i += 2;
	}
	else if (StartsWithAscii(cps, i, "LR\""))
	{
		r.prefix = StringPrefix::Wide;
		raw = true;
		i += 2;
	}
	else if (StartsWithAscii(cps, i, "R\""))
	{
		r.prefix = StringPrefix::None;
		raw = true;
		i += 1;
	}
	else if (StartsWithAscii(cps, i, "u8\""))
	{
		r.prefix = StringPrefix::U8;
		i += 2;
	}
	else if (StartsWithAscii(cps, i, "u\""))
	{
		r.prefix = StringPrefix::U16;
		i += 1;
	}
	else if (StartsWithAscii(cps, i, "U\""))
	{
		r.prefix = StringPrefix::U32;
		i += 1;
	}
	else if (StartsWithAscii(cps, i, "L\""))
	{
		r.prefix = StringPrefix::Wide;
		i += 1;
	}
	else if (StartsWithAscii(cps, i, "\""))
	{
		r.prefix = StringPrefix::None;
	}
	else
	{
		return r;
	}

	if (!raw)
	{
		if (i >= cps.size() || cps[i] != '"')
			return r;
		++i;
		while (i < cps.size() && cps[i] != '"')
		{
			if (cps[i] == '\\')
			{
				uint32_t cp = 0;
				if (!DecodeEscape(cps, i, cp) || !IsValidCodePoint(cp))
					return r;
				r.cps.push_back(cp);
			}
			else
			{
				if (cps[i] == '\n')
					return r;
				r.cps.push_back(cps[i]);
				++i;
			}
		}
		if (i >= cps.size() || cps[i] != '"')
			return r;
		++i;
	}
	else
	{
		if (i >= cps.size() || cps[i] != '"')
			return r;
		++i;

		size_t delim_begin = i;
		while (i < cps.size() && cps[i] != '(')
		{
			if (!IsValidRawDelimChar(cps[i]))
				return r;
			++i;
			if (i - delim_begin > 16)
				return r;
		}
		if (i >= cps.size() || cps[i] != '(')
			return r;
		vector<uint32_t> delim(cps.begin() + delim_begin, cps.begin() + i);
		++i;

		for (;;)
		{
			if (i >= cps.size())
				return r;
			if (cps[i] != ')')
			{
				r.cps.push_back(cps[i]);
				++i;
				continue;
			}

			bool match = true;
			if (i + 1 + delim.size() >= cps.size())
				match = false;
			else
			{
				for (size_t d = 0; d < delim.size(); ++d)
					if (cps[i + 1 + d] != delim[d])
						match = false;
				if (match && cps[i + 1 + delim.size()] != '"')
					match = false;
			}

			if (!match)
			{
				r.cps.push_back(cps[i]);
				++i;
				continue;
			}

			i += 1 + delim.size() + 1;
			break;
		}
	}

	string suffix;
	if (!ParseIdentifierSuffix(cps, i, suffix))
		return r;
	if (i != cps.size())
		return r;

	if (expect_ud)
	{
		if (suffix.empty() || suffix[0] != '_')
			return r;
	}
	else if (!suffix.empty())
	{
		return r;
	}

	r.ud_suffix = suffix;
	r.ok = true;
	return r;
}

bool EncodeStringForPrefix(const vector<uint32_t>& cps, StringPrefix prefix, EFundamentalType& out_type, vector<unsigned char>& out_bytes, size_t& out_elements)
{
	out_bytes.clear();
	out_elements = 0;

	if (prefix == StringPrefix::None || prefix == StringPrefix::U8)
	{
		out_type = FT_CHAR;
		string s;
		for (size_t i = 0; i < cps.size(); ++i)
			if (!EncodeUtf8CodePoint(cps[i], s))
				return false;
		s.push_back('\0');
		out_bytes.assign(s.begin(), s.end());
		out_elements = out_bytes.size();
		return true;
	}

	if (prefix == StringPrefix::U16)
	{
		out_type = FT_CHAR16_T;
		vector<char16_t> v;
		for (size_t i = 0; i < cps.size(); ++i)
		{
			uint32_t cp = cps[i];
			if (!IsValidCodePoint(cp))
				return false;
			if (cp <= 0xFFFF)
			{
				v.push_back(static_cast<char16_t>(cp));
			}
			else
			{
				cp -= 0x10000;
				v.push_back(static_cast<char16_t>(0xD800 + ((cp >> 10) & 0x3FF)));
				v.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
			}
		}
		v.push_back(0);
		out_elements = v.size();
		const unsigned char* p = reinterpret_cast<const unsigned char*>(v.data());
		out_bytes.assign(p, p + v.size() * sizeof(char16_t));
		return true;
	}

	vector<uint32_t> v;
	for (size_t i = 0; i < cps.size(); ++i)
	{
		if (!IsValidCodePoint(cps[i]))
			return false;
		v.push_back(cps[i]);
	}
	v.push_back(0);
	out_elements = v.size();
	const unsigned char* p = reinterpret_cast<const unsigned char*>(v.data());
	out_bytes.assign(p, p + v.size() * sizeof(uint32_t));
	out_type = prefix == StringPrefix::U32 ? FT_CHAR32_T : FT_WCHAR_T;
	return true;
}

string JoinSources(const vector<PPToken>& toks, size_t b, size_t e)
{
	string out;
	for (size_t i = b; i < e; ++i)
	{
		if (i != b)
			out += " ";
		out += toks[i].source;
	}
	return out;
}

bool ParseIntegerSuffix(const string& s, bool& has_u, int& len_kind)
{
	has_u = false;
	len_kind = 0;

	int ucount = 0;
	string rem;
	for (size_t i = 0; i < s.size(); ++i)
	{
		char c = s[i];
		if (c == 'u' || c == 'U')
		{
			++ucount;
			has_u = true;
		}
		else
		{
			rem.push_back(c);
		}
	}
	if (ucount > 1)
		return false;

	if (rem.empty())
		return true;
	if (rem == "l" || rem == "L")
	{
		len_kind = 1;
		return true;
	}
	if (rem == "ll" || rem == "LL")
	{
		len_kind = 2;
		return true;
	}

	return false;
}

bool ParseIntegerPrefixGrammar(const string& s)
{
	if (s.empty())
		return false;

	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		if (s.size() == 2)
			return false;
		for (size_t i = 2; i < s.size(); ++i)
			if (HexValue(s[i]) < 0)
				return false;
		return true;
	}

	if (s[0] == '0')
	{
		for (size_t i = 0; i < s.size(); ++i)
			if (s[i] < '0' || s[i] > '7')
				return false;
		return true;
	}

	if (s[0] < '1' || s[0] > '9')
		return false;
	for (size_t i = 1; i < s.size(); ++i)
		if (s[i] < '0' || s[i] > '9')
			return false;
	return true;
}

bool ParseDigitsToU64(const string& digits, int base, uint64_t& out, bool& too_big)
{
	out = 0;
	too_big = false;

	for (size_t i = 0; i < digits.size(); ++i)
	{
		int d = HexValue(digits[i]);
		if (d < 0 || d >= base)
			return false;
		if (too_big)
			continue;
		if (out > (numeric_limits<uint64_t>::max() - static_cast<uint64_t>(d)) / static_cast<uint64_t>(base))
		{
			too_big = true;
			continue;
		}
		out = out * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
	}

	return true;
}

vector<EFundamentalType> IntegerCandidateTypes(bool decimal, bool has_u, int len_kind)
{
	vector<EFundamentalType> out;
	if (decimal)
	{
		if (!has_u && len_kind == 0) out = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
		else if (has_u && len_kind == 0) out = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && len_kind == 1) out = {FT_LONG_INT, FT_LONG_LONG_INT};
		else if (has_u && len_kind == 1) out = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && len_kind == 2) out = {FT_LONG_LONG_INT};
		else if (has_u && len_kind == 2) out = {FT_UNSIGNED_LONG_LONG_INT};
	}
	else
	{
		if (!has_u && len_kind == 0) out = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && len_kind == 0) out = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && len_kind == 1) out = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && len_kind == 1) out = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && len_kind == 2) out = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && len_kind == 2) out = {FT_UNSIGNED_LONG_LONG_INT};
	}
	return out;
}

bool FitsType(EFundamentalType type, uint64_t value, bool too_big)
{
	if (too_big)
		return false;

	switch (type)
	{
	case FT_INT: return value <= static_cast<uint64_t>(numeric_limits<int>::max());
	case FT_LONG_INT: return value <= static_cast<uint64_t>(numeric_limits<long int>::max());
	case FT_LONG_LONG_INT: return value <= static_cast<uint64_t>(numeric_limits<long long int>::max());
	case FT_UNSIGNED_INT: return value <= static_cast<uint64_t>(numeric_limits<unsigned int>::max());
	case FT_UNSIGNED_LONG_INT: return value <= static_cast<uint64_t>(numeric_limits<unsigned long int>::max());
	case FT_UNSIGNED_LONG_LONG_INT: return value <= static_cast<uint64_t>(numeric_limits<unsigned long long int>::max());
	default: return false;
	}
}

struct ParsedBuiltinInteger
{
	bool ok;
	EFundamentalType type;
	uint64_t value;
};

ParsedBuiltinInteger ParseBuiltinInteger(const string& s)
{
	ParsedBuiltinInteger r;
	r.ok = false;
	r.type = FT_INT;
	r.value = 0;

	if (s.empty())
		return r;

	int base = 10;
	bool decimal = true;
	size_t i = 0;

	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		base = 16;
		decimal = false;
		i = 2;
	}
	else if (s[0] == '0')
	{
		base = 8;
		decimal = false;
		i = 0;
	}

	size_t digit_begin = i;
	while (i < s.size())
	{
		int d = HexValue(s[i]);
		if (d < 0 || d >= base)
			break;
		++i;
	}
	if (i == digit_begin)
		return r;

	string digits = s.substr(digit_begin, i - digit_begin);
	string suffix = s.substr(i);

	bool has_u = false;
	int len_kind = 0;
	if (!ParseIntegerSuffix(suffix, has_u, len_kind))
		return r;

	uint64_t value = 0;
	bool too_big = false;
	if (!ParseDigitsToU64(digits, base, value, too_big))
		return r;

	vector<EFundamentalType> candidates = IntegerCandidateTypes(decimal, has_u, len_kind);
	for (size_t k = 0; k < candidates.size(); ++k)
	{
		if (FitsType(candidates[k], value, too_big))
		{
			r.ok = true;
			r.type = candidates[k];
			r.value = value;
			return r;
		}
	}

	return r;
}

bool ParseFloatingCore(const string& s)
{
	if (s.empty())
		return false;
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i] == '_')
			return false;

	size_t i = 0;
	size_t db = 0;
	while (i < s.size() && s[i] >= '0' && s[i] <= '9')
	{
		++i;
		++db;
	}

	bool has_dot = false;
	size_t da = 0;
	if (i < s.size() && s[i] == '.')
	{
		has_dot = true;
		++i;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9')
		{
			++i;
			++da;
		}
	}

	bool has_exp = false;
	if (i < s.size() && (s[i] == 'e' || s[i] == 'E'))
	{
		has_exp = true;
		++i;
		if (i < s.size() && (s[i] == '+' || s[i] == '-'))
			++i;
		size_t de = 0;
		while (i < s.size() && s[i] >= '0' && s[i] <= '9')
		{
			++i;
			++de;
		}
		if (de == 0)
			return false;
	}

	if (!has_dot && !has_exp)
		return false;
	if (has_dot && db == 0 && da == 0)
		return false;

	return i == s.size();
}

struct ParsedBuiltinFloating
{
	bool ok;
	EFundamentalType type;
	string scan_source;
};

ParsedBuiltinFloating ParseBuiltinFloating(const string& s)
{
	ParsedBuiltinFloating r;
	r.ok = false;
	r.type = FT_DOUBLE;
	r.scan_source = s;

	if (s.empty())
		return r;

	char last = s[s.size() - 1];
	string core = s;
	if (last == 'f' || last == 'F' || last == 'l' || last == 'L')
	{
		core = s.substr(0, s.size() - 1);
		if (core.empty())
			return r;
		if (last == 'f' || last == 'F') r.type = FT_FLOAT;
		else r.type = FT_LONG_DOUBLE;
	}
	else
	{
		r.type = FT_DOUBLE;
	}

	if (!ParseFloatingCore(core))
		return r;

	r.ok = true;
	r.scan_source = core;
	return r;
}

bool TrySplitUdSuffix(const string& s, string& prefix, string& ud_suffix)
{
	size_t pos = s.find('_');
	if (pos == string::npos)
		return false;
	prefix = s.substr(0, pos);
	ud_suffix = s.substr(pos);
	if (prefix.empty())
		return false;
	if (!IsValidIdentifierAscii(ud_suffix))
		return false;
	if (ud_suffix[0] != '_')
		return false;
	return true;
}

void EmitBuiltinInteger(DebugPostTokenOutputStream& out, const string& source, const ParsedBuiltinInteger& lit)
{
	switch (lit.type)
	{
	case FT_INT: { int v = static_cast<int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_LONG_INT: { long int v = static_cast<long int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_LONG_LONG_INT: { long long int v = static_cast<long long int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_UNSIGNED_INT: { unsigned int v = static_cast<unsigned int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_UNSIGNED_LONG_INT: { unsigned long int v = static_cast<unsigned long int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_UNSIGNED_LONG_LONG_INT: { unsigned long long int v = static_cast<unsigned long long int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	default: out.emit_invalid(source); break;
	}
}

void EmitBuiltinFloating(DebugPostTokenOutputStream& out, const string& source, const ParsedBuiltinFloating& lit)
{
	if (lit.type == FT_FLOAT)
	{
		float v = PA2Decode_float(lit.scan_source);
		out.emit_literal(source, lit.type, &v, sizeof(v));
	}
	else if (lit.type == FT_DOUBLE)
	{
		double v = PA2Decode_double(lit.scan_source);
		out.emit_literal(source, lit.type, &v, sizeof(v));
	}
	else
	{
		long double v = PA2Decode_long_double(lit.scan_source);
		out.emit_literal(source, lit.type, &v, sizeof(v));
	}
}

void ProcessPPNumber(DebugPostTokenOutputStream& out, const string& source)
{
	string prefix;
	string ud_suffix;
	if (TrySplitUdSuffix(source, prefix, ud_suffix))
	{
		if (ParseIntegerPrefixGrammar(prefix))
		{
			out.emit_user_defined_literal_integer(source, ud_suffix, prefix);
			return;
		}

		if (ParseFloatingCore(prefix))
		{
			out.emit_user_defined_literal_floating(source, ud_suffix, prefix);
			return;
		}

		out.emit_invalid(source);
		return;
	}

	ParsedBuiltinInteger bi = ParseBuiltinInteger(source);
	if (bi.ok)
	{
		EmitBuiltinInteger(out, source, bi);
		return;
	}

	ParsedBuiltinFloating bf = ParseBuiltinFloating(source);
	if (bf.ok)
	{
		EmitBuiltinFloating(out, source, bf);
		return;
	}

	out.emit_invalid(source);
}

void ProcessCharacterLiteral(DebugPostTokenOutputStream& out, const string& source, bool expect_ud)
{
	ParsedCharacterLiteral lit = ParseCharacterLiteral(source, expect_ud);
	if (!lit.ok)
	{
		out.emit_invalid(source);
		return;
	}

	if (expect_ud)
	{
		switch (lit.type)
		{
		case FT_CHAR: { char v = static_cast<char>(lit.value); out.emit_user_defined_literal_character(source, lit.ud_suffix, lit.type, &v, sizeof(v)); return; }
		case FT_INT: { int v = static_cast<int>(lit.value); out.emit_user_defined_literal_character(source, lit.ud_suffix, lit.type, &v, sizeof(v)); return; }
		case FT_CHAR16_T: { char16_t v = static_cast<char16_t>(lit.value); out.emit_user_defined_literal_character(source, lit.ud_suffix, lit.type, &v, sizeof(v)); return; }
		case FT_CHAR32_T: { char32_t v = static_cast<char32_t>(lit.value); out.emit_user_defined_literal_character(source, lit.ud_suffix, lit.type, &v, sizeof(v)); return; }
		case FT_WCHAR_T: { wchar_t v = static_cast<wchar_t>(lit.value); out.emit_user_defined_literal_character(source, lit.ud_suffix, lit.type, &v, sizeof(v)); return; }
		default: out.emit_invalid(source); return;
		}
	}

	switch (lit.type)
	{
	case FT_CHAR: { char v = static_cast<char>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_INT: { int v = static_cast<int>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_CHAR16_T: { char16_t v = static_cast<char16_t>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_CHAR32_T: { char32_t v = static_cast<char32_t>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	case FT_WCHAR_T: { wchar_t v = static_cast<wchar_t>(lit.value); out.emit_literal(source, lit.type, &v, sizeof(v)); break; }
	default: out.emit_invalid(source); break;
	}
}

void ProcessStringRun(DebugPostTokenOutputStream& out, const vector<PPToken>& toks, size_t b, size_t e)
{
	string run_source = JoinSources(toks, b, e);

	vector<ParsedStringLiteral> parsed;
	for (size_t i = b; i < e; ++i)
	{
		bool expect_ud = toks[i].type == PPTokenType::UserDefinedStringLiteral;
		ParsedStringLiteral lit = ParseStringLiteral(toks[i].source, expect_ud);
		if (!lit.ok)
		{
			out.emit_invalid(run_source);
			return;
		}
		parsed.push_back(lit);
	}

	set<StringPrefix> prefix_set;
	set<string> ud_set;
	vector<uint32_t> all_cps;

	for (size_t i = 0; i < parsed.size(); ++i)
	{
		if (parsed[i].prefix != StringPrefix::None)
			prefix_set.insert(parsed[i].prefix);
		if (!parsed[i].ud_suffix.empty())
			ud_set.insert(parsed[i].ud_suffix);

		all_cps.insert(all_cps.end(), parsed[i].cps.begin(), parsed[i].cps.end());
	}

	if (prefix_set.size() > 1 || ud_set.size() > 1)
	{
		out.emit_invalid(run_source);
		return;
	}

	StringPrefix prefix = prefix_set.empty() ? StringPrefix::None : *prefix_set.begin();
	string ud_suffix = ud_set.empty() ? "" : *ud_set.begin();

	EFundamentalType elem_type = FT_CHAR;
	vector<unsigned char> bytes;
	size_t num_elements = 0;
	if (!EncodeStringForPrefix(all_cps, prefix, elem_type, bytes, num_elements))
	{
		out.emit_invalid(run_source);
		return;
	}

	if (ud_suffix.empty())
		out.emit_literal_array(run_source, num_elements, elem_type, bytes.data(), bytes.size());
	else
		out.emit_user_defined_literal_string_array(run_source, ud_suffix, num_elements, elem_type, bytes.data(), bytes.size());
}

void RunPostToken(const string& input, DebugPostTokenOutputStream& output)
{
	PPTokenCollector collector;
	PPTokenizer tokenizer(collector);
	for (size_t i = 0; i < input.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(input[i]);
		tokenizer.process(c);
	}
	tokenizer.process(EndOfFile);

	vector<PPToken> tokens;
	for (size_t i = 0; i < collector.tokens.size(); ++i)
	{
		if (collector.tokens[i].type == PPTokenType::WhitespaceSequence || collector.tokens[i].type == PPTokenType::NewLine)
			continue;
		tokens.push_back(collector.tokens[i]);
	}

	size_t i = 0;
	while (i < tokens.size())
	{
		PPTokenType t = tokens[i].type;
		const string& source = tokens[i].source;

		if (t == PPTokenType::StringLiteral || t == PPTokenType::UserDefinedStringLiteral)
		{
			size_t j = i;
			while (j < tokens.size() && (tokens[j].type == PPTokenType::StringLiteral || tokens[j].type == PPTokenType::UserDefinedStringLiteral))
				++j;
			ProcessStringRun(output, tokens, i, j);
			i = j;
			continue;
		}

		if (t == PPTokenType::HeaderName || t == PPTokenType::NonWhitespaceCharacter)
		{
			output.emit_invalid(source);
			++i;
			continue;
		}

		if (t == PPTokenType::Identifier)
		{
			auto it = StringToTokenTypeMap.find(source);
			if (it != StringToTokenTypeMap.end())
				output.emit_simple(source, it->second);
			else
				output.emit_identifier(source);
			++i;
			continue;
		}

		if (t == PPTokenType::PreprocessingOpOrPunc)
		{
			if (source == "#" || source == "##" || source == "%:" || source == "%:%:")
			{
				output.emit_invalid(source);
			}
			else
			{
				auto it = StringToTokenTypeMap.find(source);
				if (it != StringToTokenTypeMap.end())
					output.emit_simple(source, it->second);
				else
					output.emit_invalid(source);
			}
			++i;
			continue;
		}

		if (t == PPTokenType::PPNumber)
		{
			ProcessPPNumber(output, source);
			++i;
			continue;
		}

		if (t == PPTokenType::CharacterLiteral)
		{
			ProcessCharacterLiteral(output, source, false);
			++i;
			continue;
		}

		if (t == PPTokenType::UserDefinedCharacterLiteral)
		{
			ProcessCharacterLiteral(output, source, true);
			++i;
			continue;
		}

		output.emit_invalid(source);
		++i;
	}
}
} // namespace pa2

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		string input = oss.str();

		DebugPostTokenOutputStream output;
		pa2::RunPostToken(input, output);
		output.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
