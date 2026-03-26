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

using namespace std;

#define main pptoken_main
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

enum class CollectedPPTokenKind
{
	Whitespace,
	NewLine,
	HeaderName,
	Identifier,
	PpNumber,
	CharacterLiteral,
	UserDefinedCharacterLiteral,
	StringLiteral,
	UserDefinedStringLiteral,
	PreprocessingOpOrPunc,
	NonWhitespaceCharacter
};

struct CollectedPPToken
{
	CollectedPPTokenKind kind;
	string source;
};

struct CollectingPPTokenStream : IPPTokenStream
{
	vector<CollectedPPToken> tokens;

	void emit_whitespace_sequence()
	{
		tokens.push_back({CollectedPPTokenKind::Whitespace, ""});
	}

	void emit_new_line()
	{
		tokens.push_back({CollectedPPTokenKind::NewLine, ""});
	}

	void emit_header_name(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::HeaderName, data});
	}

	void emit_identifier(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::Identifier, data});
	}

	void emit_pp_number(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::PpNumber, data});
	}

	void emit_character_literal(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::CharacterLiteral, data});
	}

	void emit_user_defined_character_literal(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::UserDefinedCharacterLiteral, data});
	}

	void emit_string_literal(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::StringLiteral, data});
	}

	void emit_user_defined_string_literal(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::UserDefinedStringLiteral, data});
	}

	void emit_preprocessing_op_or_punc(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::PreprocessingOpOrPunc, data});
	}

	void emit_non_whitespace_char(const string& data)
	{
		tokens.push_back({CollectedPPTokenKind::NonWhitespaceCharacter, data});
	}

	void emit_eof()
	{
	}
};

enum class EncodingPrefix
{
	Ordinary,
	U8,
	U16,
	U32,
	WChar
};

enum class NumericKind
{
	Invalid,
	Integer,
	Floating,
	UserDefinedInteger,
	UserDefinedFloating
};

enum class IntegerSuffixKind
{
	None,
	U,
	L,
	LL,
	UL,
	ULL
};

enum class FloatSuffixKind
{
	None,
	F,
	L
};

struct ParsedStringLiteral
{
	bool valid;
	EncodingPrefix encoding;
	bool user_defined;
	string ud_suffix;
	vector<int> code_points;

	ParsedStringLiteral()
		: valid(false), encoding(EncodingPrefix::Ordinary), user_defined(false)
	{}
};

struct ParsedCharacterLiteral
{
	bool valid;
	EncodingPrefix encoding;
	bool user_defined;
	string ud_suffix;
	int code_point;

	ParsedCharacterLiteral()
		: valid(false), encoding(EncodingPrefix::Ordinary), user_defined(false),
		  code_point(0)
	{}
};

struct ParsedNumberLiteral
{
	NumericKind kind;
	EncodingPrefix encoding;
	string prefix;
	string ud_suffix;
	EFundamentalType int_type;
	unsigned __int128 int_value;
	EFundamentalType float_type;

	ParsedNumberLiteral()
		: kind(NumericKind::Invalid), encoding(EncodingPrefix::Ordinary),
		  int_type(FT_INT), int_value(0), float_type(FT_DOUBLE)
	{}
};

static bool IsAsciiAlpha(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool IsAsciiDigit(unsigned char c)
{
	return c >= '0' && c <= '9';
}

static bool IsAsciiOctDigit(unsigned char c)
{
	return c >= '0' && c <= '7';
}

static bool IsAsciiHexDigit(unsigned char c)
{
	return IsAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool IsIdentifierContinueByte(unsigned char c)
{
	return c == '_' || IsAsciiAlpha(c) || IsAsciiDigit(c) || c >= 0x80;
}

static bool IsUDSuffix(const string& s)
{
	if (s.empty() || s[0] != '_')
		return false;

	for (size_t i = 1; i < s.size(); ++i)
	{
		if (!IsIdentifierContinueByte((unsigned char) s[i]))
			return false;
	}

	return true;
}

static bool IsUnicodeScalarValue(int cp)
{
	return (cp >= 0 && cp < 0xD800) || (cp >= 0xE000 && cp <= 0x10FFFF);
}

static void AppendUTF8CodePoint(string& s, int cp)
{
	if (!IsUnicodeScalarValue(cp))
		throw logic_error("invalid code point");

	if (cp <= 0x7F)
	{
		s.push_back((char) cp);
	}
	else if (cp <= 0x7FF)
	{
		s.push_back((char) (0xC0 | ((cp >> 6) & 0x1F)));
		s.push_back((char) (0x80 | ((cp >> 0) & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		s.push_back((char) (0xE0 | ((cp >> 12) & 0x0F)));
		s.push_back((char) (0x80 | ((cp >> 6) & 0x3F)));
		s.push_back((char) (0x80 | ((cp >> 0) & 0x3F)));
	}
	else
	{
		s.push_back((char) (0xF0 | ((cp >> 18) & 0x07)));
		s.push_back((char) (0x80 | ((cp >> 12) & 0x3F)));
		s.push_back((char) (0x80 | ((cp >> 6) & 0x3F)));
		s.push_back((char) (0x80 | ((cp >> 0) & 0x3F)));
	}
}

static void AppendUTF16CodePoint(u16string& s, int cp)
{
	if (!IsUnicodeScalarValue(cp))
		throw logic_error("invalid code point");

	if (cp <= 0xFFFF)
	{
		s.push_back((char16_t) cp);
	}
	else
	{
		cp -= 0x10000;
		s.push_back((char16_t) (0xD800 | ((cp >> 10) & 0x3FF)));
		s.push_back((char16_t) (0xDC00 | ((cp >> 0) & 0x3FF)));
	}
}

static void AppendUTF32CodePoint(u32string& s, int cp)
{
	if (!IsUnicodeScalarValue(cp))
		throw logic_error("invalid code point");

	s.push_back((char32_t) cp);
}

static void AppendWCharCodePoint(wstring& s, int cp)
{
	if (!IsUnicodeScalarValue(cp))
		throw logic_error("invalid code point");

	s.push_back((wchar_t) cp);
}

static size_t DecodeUTF8CodePoint(const string& input, size_t pos, int& cp)
{
	if (pos >= input.size())
		throw logic_error("utf8 decode past end");

	unsigned char c = (unsigned char) input[pos];
	if (c < 0x80)
	{
		cp = c;
		return 1;
	}

	if (c >= 0x80 && c <= 0xBF)
		throw logic_error("utf8 trailing code unit (10xxxxxx) at start");

	if (c >= 0xC0 && c <= 0xDF)
	{
		if (pos + 1 >= input.size() || ((unsigned char) input[pos + 1] & 0xC0) != 0x80)
			throw logic_error("utf8 expected trailing byte (10xxxxxx)");
		cp = ((c & 0x1F) << 6)
			| (((unsigned char) input[pos + 1]) & 0x3F);
		return 2;
	}

	if (c >= 0xE0 && c <= 0xEF)
	{
		if (pos + 2 >= input.size()
			|| ((unsigned char) input[pos + 1] & 0xC0) != 0x80
			|| ((unsigned char) input[pos + 2] & 0xC0) != 0x80)
		{
			throw logic_error("utf8 expected trailing byte (10xxxxxx)");
		}

		cp = ((c & 0x0F) << 12)
			| ((((unsigned char) input[pos + 1]) & 0x3F) << 6)
			| (((unsigned char) input[pos + 2]) & 0x3F);
		return 3;
	}

	if (c >= 0xF0 && c <= 0xF7)
	{
		if (pos + 3 >= input.size()
			|| ((unsigned char) input[pos + 1] & 0xC0) != 0x80
			|| ((unsigned char) input[pos + 2] & 0xC0) != 0x80
			|| ((unsigned char) input[pos + 3] & 0xC0) != 0x80)
		{
			throw logic_error("utf8 expected trailing byte (10xxxxxx)");
		}

		cp = ((c & 0x07) << 18)
			| ((((unsigned char) input[pos + 1]) & 0x3F) << 12)
			| ((((unsigned char) input[pos + 2]) & 0x3F) << 6)
			| (((unsigned char) input[pos + 3]) & 0x3F);
		if (cp > 0x10FFFF)
			throw logic_error("invalid code point");
		return 4;
	}

	throw logic_error("utf8 invalid unit (111111xx)");
}

static bool ParseEscapeSequence(const string& input, size_t& pos, int& cp)
{
	if (pos >= input.size() || input[pos] != '\\')
		return false;

	size_t p = pos + 1;
	if (p >= input.size())
		return false;

	int c = (unsigned char) input[p++];
	if (c == 'x')
	{
		if (p >= input.size() || !IsAsciiHexDigit((unsigned char) input[p]))
			return false;

		cp = 0;
		while (p < input.size() && IsAsciiHexDigit((unsigned char) input[p]))
		{
			cp = (cp << 4) + HexCharToValue((unsigned char) input[p]);
			++p;
		}
	}
	else if (IsAsciiOctDigit((unsigned char) c))
	{
		cp = c - '0';
		int count = 1;
		while (count < 3 && p < input.size() && IsAsciiOctDigit((unsigned char) input[p]))
		{
			cp = (cp << 3) + (input[p] - '0');
			++p;
			++count;
		}
	}
	else if (SimpleEscapeSequence_CodePoints.count(c) != 0)
	{
		switch (c)
		{
		case '\'': cp = '\''; break;
		case '"': cp = '"'; break;
		case '?': cp = '?'; break;
		case '\\': cp = '\\'; break;
		case 'a': cp = 0x07; break;
		case 'b': cp = 0x08; break;
		case 'f': cp = 0x0C; break;
		case 'n': cp = 0x0A; break;
		case 'r': cp = 0x0D; break;
		case 't': cp = 0x09; break;
		case 'v': cp = 0x0B; break;
		default:
			return false;
		}
	}
	else
	{
		return false;
	}

	pos = p;
	return true;
}

static bool ParseStringPrefix(const string& source, bool& raw, EncodingPrefix& encoding, size_t& pos_after_quote)
{
	raw = false;
	encoding = EncodingPrefix::Ordinary;

	if (source.size() >= 4 && source.compare(0, 4, "u8R\"") == 0)
	{
		raw = true;
		encoding = EncodingPrefix::U8;
		pos_after_quote = 4;
		return true;
	}

	if (source.size() >= 3 && source.compare(0, 3, "uR\"") == 0)
	{
		raw = true;
		encoding = EncodingPrefix::U16;
		pos_after_quote = 3;
		return true;
	}

	if (source.size() >= 3 && source.compare(0, 3, "UR\"") == 0)
	{
		raw = true;
		encoding = EncodingPrefix::U32;
		pos_after_quote = 3;
		return true;
	}

	if (source.size() >= 3 && source.compare(0, 3, "LR\"") == 0)
	{
		raw = true;
		encoding = EncodingPrefix::WChar;
		pos_after_quote = 3;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "R\"") == 0)
	{
		raw = true;
		encoding = EncodingPrefix::Ordinary;
		pos_after_quote = 2;
		return true;
	}

	if (source.size() >= 3 && source.compare(0, 3, "u8\"") == 0)
	{
		encoding = EncodingPrefix::U8;
		pos_after_quote = 3;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "u\"") == 0)
	{
		encoding = EncodingPrefix::U16;
		pos_after_quote = 2;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "U\"") == 0)
	{
		encoding = EncodingPrefix::U32;
		pos_after_quote = 2;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "L\"") == 0)
	{
		encoding = EncodingPrefix::WChar;
		pos_after_quote = 2;
		return true;
	}

	if (!source.empty() && source[0] == '"')
	{
		encoding = EncodingPrefix::Ordinary;
		pos_after_quote = 1;
		return true;
	}

	return false;
}

static ParsedStringLiteral ParseStringLiteralToken(const string& source)
{
	ParsedStringLiteral out;
	bool raw = false;
	size_t pos = 0;

	if (!ParseStringPrefix(source, raw, out.encoding, pos))
		return out;

	try
	{
		if (raw)
		{
			string delimiter;
			while (true)
			{
				if (pos >= source.size())
					return out;

				unsigned char c = (unsigned char) source[pos];
				if (c == '(')
				{
					++pos;
					break;
				}

				if (!((c >= 0x20 && c != 0x7F)
					&& c != ' ' && c != '(' && c != ')' && c != '\\'
					&& c != '\n' && c != '\t' && c != '\v' && c != '\f'))
				{
					return out;
				}

				delimiter.push_back((char) c);
				++pos;
				if (delimiter.size() > 16)
					return out;
			}

			string closing = ")" + delimiter + "\"";
			size_t close_pos = source.find(closing, pos);
			if (close_pos == string::npos)
				return out;

			string body = source.substr(pos, close_pos - pos);
			size_t p = 0;
			while (p < body.size())
			{
				int cp;
				size_t consumed = DecodeUTF8CodePoint(body, p, cp);
				if (!IsUnicodeScalarValue(cp))
					return out;
				out.code_points.push_back(cp);
				p += consumed;
			}

			pos = close_pos + closing.size();
		}
		else
		{
			bool closed = false;
			while (pos < source.size())
			{
				unsigned char c = (unsigned char) source[pos];
				if (c == '\n')
					return out;
				if (c == '"')
				{
					++pos;
					closed = true;
					break;
				}

				int cp = 0;
				if (c == '\\')
				{
					if (!ParseEscapeSequence(source, pos, cp))
						return out;
				}
				else
				{
					size_t consumed = DecodeUTF8CodePoint(source, pos, cp);
					pos += consumed;
				}

				if (!IsUnicodeScalarValue(cp))
					return out;
				out.code_points.push_back(cp);
			}

			if (!closed)
				return out;
		}

		out.ud_suffix = source.substr(pos);
		if (!out.ud_suffix.empty() && !IsUDSuffix(out.ud_suffix))
			return out;
		out.user_defined = !out.ud_suffix.empty();
		out.valid = true;
		return out;
	}
	catch (exception&)
	{
		return ParsedStringLiteral();
	}
}

static bool ParseCharacterPrefix(const string& source, EncodingPrefix& encoding, size_t& pos_after_quote)
{
	if (source.size() >= 2 && source.compare(0, 2, "u'") == 0)
	{
		encoding = EncodingPrefix::U16;
		pos_after_quote = 2;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "U'") == 0)
	{
		encoding = EncodingPrefix::U32;
		pos_after_quote = 2;
		return true;
	}

	if (source.size() >= 2 && source.compare(0, 2, "L'") == 0)
	{
		encoding = EncodingPrefix::WChar;
		pos_after_quote = 2;
		return true;
	}

	if (!source.empty() && source[0] == '\'')
	{
		encoding = EncodingPrefix::Ordinary;
		pos_after_quote = 1;
		return true;
	}

	return false;
}

static ParsedCharacterLiteral ParseCharacterLiteralToken(const string& source)
{
	ParsedCharacterLiteral out;
	size_t pos = 0;
	if (!ParseCharacterPrefix(source, out.encoding, pos))
		return out;

	try
	{
		if (pos >= source.size())
			return out;

		if (source[pos] == '\'')
			return out;

		int cp = 0;
		if (source[pos] == '\\')
		{
			if (!ParseEscapeSequence(source, pos, cp))
				return out;
		}
		else
		{
			size_t consumed = DecodeUTF8CodePoint(source, pos, cp);
			pos += consumed;
		}

		if (!IsUnicodeScalarValue(cp))
			return out;

		if (pos >= source.size() || source[pos] != '\'')
			return out;
		++pos;

		out.ud_suffix = source.substr(pos);
		if (!out.ud_suffix.empty() && !IsUDSuffix(out.ud_suffix))
			return out;

		out.user_defined = !out.ud_suffix.empty();
		out.code_point = cp;
		out.valid = true;
		return out;
	}
	catch (exception&)
	{
		return ParsedCharacterLiteral();
	}
}

static bool ParseIntegerSuffix(const string& suffix, IntegerSuffixKind& kind)
{
	if (suffix.empty())
	{
		kind = IntegerSuffixKind::None;
		return true;
	}

	if (suffix == "u" || suffix == "U")
	{
		kind = IntegerSuffixKind::U;
		return true;
	}

	if (suffix == "l" || suffix == "L")
	{
		kind = IntegerSuffixKind::L;
		return true;
	}

	if (suffix == "ll" || suffix == "LL")
	{
		kind = IntegerSuffixKind::LL;
		return true;
	}

	if (suffix == "ul" || suffix == "uL" || suffix == "Ul" || suffix == "UL"
		|| suffix == "lu" || suffix == "lU" || suffix == "Lu" || suffix == "LU")
	{
		kind = IntegerSuffixKind::UL;
		return true;
	}

	if (suffix == "ull" || suffix == "uLL" || suffix == "Ull" || suffix == "ULL"
		|| suffix == "llu" || suffix == "llU" || suffix == "LLu" || suffix == "LLU")
	{
		kind = IntegerSuffixKind::ULL;
		return true;
	}

	return false;
}

static bool ParseFloatSuffix(const string& suffix, FloatSuffixKind& kind)
{
	if (suffix.empty())
	{
		kind = FloatSuffixKind::None;
		return true;
	}

	if (suffix == "f" || suffix == "F")
	{
		kind = FloatSuffixKind::F;
		return true;
	}

	if (suffix == "l" || suffix == "L")
	{
		kind = FloatSuffixKind::L;
		return true;
	}

	return false;
}

static bool ParseIntegerCoreNoSuffix(const string& source, size_t& body_end, unsigned __int128& value, bool& is_non_decimal, bool& overflow)
{
	overflow = false;
	body_end = 0;
	value = 0;

	if (source.empty())
		return false;

	size_t pos = 0;
	if (source[pos] == '0')
	{
		is_non_decimal = true;
		++pos;

		if (pos < source.size() && (source[pos] == 'x' || source[pos] == 'X'))
		{
			++pos;
			size_t digit_start = pos;
			while (pos < source.size() && IsAsciiHexDigit((unsigned char) source[pos]))
			{
				int digit = HexCharToValue((unsigned char) source[pos]);
				const unsigned __int128 maxv = ~((unsigned __int128) 0);
				if (!overflow)
				{
					if (value > (maxv - (unsigned __int128) digit) / 16)
						overflow = true;
					else
						value = value * 16 + (unsigned __int128) digit;
				}
				++pos;
			}

			if (pos == digit_start)
				return false;
		}
		else
		{
			while (pos < source.size() && IsAsciiOctDigit((unsigned char) source[pos]))
			{
				int digit = source[pos] - '0';
				const unsigned __int128 maxv = ~((unsigned __int128) 0);
				if (!overflow)
				{
					if (value > (maxv - (unsigned __int128) digit) / 8)
						overflow = true;
					else
						value = value * 8 + (unsigned __int128) digit;
				}
				++pos;
			}
		}
	}
	else if (IsAsciiDigit((unsigned char) source[pos]))
	{
		is_non_decimal = false;
		while (pos < source.size() && IsAsciiDigit((unsigned char) source[pos]))
		{
			int digit = source[pos] - '0';
			const unsigned __int128 maxv = ~((unsigned __int128) 0);
			if (!overflow)
			{
				if (value > (maxv - (unsigned __int128) digit) / 10)
					overflow = true;
				else
					value = value * 10 + (unsigned __int128) digit;
			}
			++pos;
		}
	}
	else
	{
		return false;
	}

	body_end = pos;
	return true;
}

static bool ParseDecimalFloatCoreNoSuffix(const string& source, size_t& body_end)
{
	size_t pos = 0;
	bool saw_dot = false;
	bool saw_exp = false;

	if (source.empty())
		return false;

	if (source[pos] == '.')
	{
		saw_dot = true;
		++pos;

		size_t digit_start = pos;
		while (pos < source.size() && IsAsciiDigit((unsigned char) source[pos]))
			++pos;

		if (pos == digit_start)
			return false;
	}
	else
	{
		size_t digit_start = pos;
		while (pos < source.size() && IsAsciiDigit((unsigned char) source[pos]))
			++pos;
		if (pos == digit_start)
			return false;

		if (pos < source.size() && source[pos] == '.')
		{
			saw_dot = true;
			++pos;
			while (pos < source.size() && IsAsciiDigit((unsigned char) source[pos]))
				++pos;
		}
	}

	if (pos < source.size() && (source[pos] == 'e' || source[pos] == 'E'))
	{
		size_t exp_pos = pos + 1;
		if (exp_pos < source.size() && (source[exp_pos] == '+' || source[exp_pos] == '-'))
			++exp_pos;

		size_t exp_start = exp_pos;
		while (exp_pos < source.size() && IsAsciiDigit((unsigned char) source[exp_pos]))
			++exp_pos;

		if (exp_pos == exp_start)
			return false;

		pos = exp_pos;
		saw_exp = true;
	}

	if (!saw_dot && !saw_exp)
		return false;

	body_end = pos;
	return true;
}

static bool ParseHexFloatCoreNoSuffix(const string& source, size_t& body_end)
{
	if (source.size() < 3 || source[0] != '0' || (source[1] != 'x' && source[1] != 'X'))
		return false;

	size_t pos = 2;
	bool saw_digits_before_dot = false;
	bool saw_digits_after_dot = false;

	while (pos < source.size() && IsAsciiHexDigit((unsigned char) source[pos]))
	{
		saw_digits_before_dot = true;
		++pos;
	}

	if (pos < source.size() && source[pos] == '.')
	{
		++pos;
		while (pos < source.size() && IsAsciiHexDigit((unsigned char) source[pos]))
		{
			saw_digits_after_dot = true;
			++pos;
		}
	}

	if (!saw_digits_before_dot && !saw_digits_after_dot)
		return false;

	if (pos >= source.size() || (source[pos] != 'p' && source[pos] != 'P'))
		return false;

	++pos;
	if (pos < source.size() && (source[pos] == '+' || source[pos] == '-'))
		++pos;

	size_t exp_start = pos;
	while (pos < source.size() && IsAsciiDigit((unsigned char) source[pos]))
		++pos;

	if (pos == exp_start)
		return false;

	body_end = pos;
	return true;
}

static bool ParseFloatCoreNoSuffix(const string& source, size_t& body_end)
{
	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
		return ParseHexFloatCoreNoSuffix(source, body_end);

	return ParseDecimalFloatCoreNoSuffix(source, body_end);
}

static bool ParsePlainIntegerLiteral(const string& source, ParsedNumberLiteral& out)
{
	size_t body_end = 0;
	unsigned __int128 value = 0;
	bool is_non_decimal = false;
	bool overflow = false;
	if (!ParseIntegerCoreNoSuffix(source, body_end, value, is_non_decimal, overflow))
		return false;

	IntegerSuffixKind suffix_kind;
	if (!ParseIntegerSuffix(source.substr(body_end), suffix_kind))
		return false;
	if (overflow)
		return false;

	bool ok = false;
	if (!is_non_decimal)
	{
		switch (suffix_kind)
		{
		case IntegerSuffixKind::None:
			if (value <= (unsigned __int128) INT_MAX) { out.int_type = FT_INT; ok = true; }
			else if (value <= (unsigned __int128) LONG_MAX) { out.int_type = FT_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::U:
			if (value <= (unsigned __int128) UINT_MAX) { out.int_type = FT_UNSIGNED_INT; ok = true; }
			else if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::L:
			if (value <= (unsigned __int128) LONG_MAX) { out.int_type = FT_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::LL:
			if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::UL:
			if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::ULL:
			if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		}
	}
	else
	{
		switch (suffix_kind)
		{
		case IntegerSuffixKind::None:
			if (value <= (unsigned __int128) INT_MAX) { out.int_type = FT_INT; ok = true; }
			else if (value <= (unsigned __int128) UINT_MAX) { out.int_type = FT_UNSIGNED_INT; ok = true; }
			else if (value <= (unsigned __int128) LONG_MAX) { out.int_type = FT_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::U:
			if (value <= (unsigned __int128) UINT_MAX) { out.int_type = FT_UNSIGNED_INT; ok = true; }
			else if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::L:
			if (value <= (unsigned __int128) LONG_MAX) { out.int_type = FT_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::LL:
			if (value <= (unsigned __int128) LLONG_MAX) { out.int_type = FT_LONG_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::UL:
			if (value <= (unsigned __int128) ULONG_MAX) { out.int_type = FT_UNSIGNED_LONG_INT; ok = true; }
			else if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		case IntegerSuffixKind::ULL:
			if (value <= (unsigned __int128) ULLONG_MAX) { out.int_type = FT_UNSIGNED_LONG_LONG_INT; ok = true; }
			break;
		}
	}

	if (!ok)
		return false;

	out.kind = NumericKind::Integer;
	out.int_value = value;
	return true;
}

static bool ParseUserDefinedIntegerLiteral(const string& source, ParsedNumberLiteral& out)
{
	size_t body_end = 0;
	unsigned __int128 value = 0;
	bool is_non_decimal = false;
	bool overflow = false;
	if (!ParseIntegerCoreNoSuffix(source, body_end, value, is_non_decimal, overflow))
		return false;
	if (overflow)
		return false;

	string suffix = source.substr(body_end);
	if (!IsUDSuffix(suffix))
		return false;

	out.kind = NumericKind::UserDefinedInteger;
	out.prefix = source.substr(0, body_end);
	out.ud_suffix = suffix;
	out.int_value = value;
	return true;
}

static bool ParsePlainFloatingLiteral(const string& source, ParsedNumberLiteral& out)
{
	size_t body_end = 0;
	if (!ParseFloatCoreNoSuffix(source, body_end))
		return false;

	FloatSuffixKind suffix_kind;
	if (!ParseFloatSuffix(source.substr(body_end), suffix_kind))
		return false;

	switch (suffix_kind)
	{
	case FloatSuffixKind::None:
		out.float_type = FT_DOUBLE;
		break;
	case FloatSuffixKind::F:
		out.float_type = FT_FLOAT;
		break;
	case FloatSuffixKind::L:
		out.float_type = FT_LONG_DOUBLE;
		break;
	}

	out.kind = NumericKind::Floating;
	return true;
}

static bool ParseUserDefinedFloatingLiteral(const string& source, ParsedNumberLiteral& out)
{
	size_t body_end = 0;
	if (!ParseFloatCoreNoSuffix(source, body_end))
		return false;

	string suffix = source.substr(body_end);
	if (!IsUDSuffix(suffix))
		return false;

	out.kind = NumericKind::UserDefinedFloating;
	out.prefix = source.substr(0, body_end);
	out.ud_suffix = suffix;
	return true;
}

static ParsedNumberLiteral ParsePpNumber(const string& source)
{
	ParsedNumberLiteral out;
	if (ParsePlainFloatingLiteral(source, out))
		return out;
	if (ParseUserDefinedFloatingLiteral(source, out))
		return out;
	if (ParsePlainIntegerLiteral(source, out))
		return out;
	if (ParseUserDefinedIntegerLiteral(source, out))
		return out;
	return ParsedNumberLiteral();
}

static string JoinSources(const vector<CollectedPPToken>& tokens, size_t begin, size_t end)
{
	string out;
	bool have_string_token = false;
	bool saw_separator = false;
	for (size_t i = begin; i < end; ++i)
	{
		if (tokens[i].kind == CollectedPPTokenKind::Whitespace
			|| tokens[i].kind == CollectedPPTokenKind::NewLine)
		{
			if (have_string_token)
				saw_separator = true;
			continue;
		}

		if (have_string_token && saw_separator)
			out.push_back(' ');

		out += tokens[i].source;
		have_string_token = true;
		saw_separator = false;
	}
	return out;
}

static bool ParseAndEmitStringGroup(const vector<CollectedPPToken>& tokens, size_t begin, size_t end, DebugPostTokenOutputStream& output)
{
	string joined_source = JoinSources(tokens, begin, end);
	bool saw_u8 = false;
	bool saw_u = false;
	bool saw_U = false;
	bool saw_L = false;
	bool have_suffix = false;
	string suffix;
	vector<int> code_points;

	for (size_t i = begin; i < end; ++i)
	{
		if (tokens[i].kind == CollectedPPTokenKind::Whitespace
			|| tokens[i].kind == CollectedPPTokenKind::NewLine)
		{
			continue;
		}

		ParsedStringLiteral parsed = ParseStringLiteralToken(tokens[i].source);
		if (!parsed.valid)
		{
			output.emit_invalid(joined_source);
			return false;
		}

		code_points.insert(code_points.end(), parsed.code_points.begin(), parsed.code_points.end());
		switch (parsed.encoding)
		{
		case EncodingPrefix::Ordinary:
			break;
		case EncodingPrefix::U8:
			saw_u8 = true;
			break;
		case EncodingPrefix::U16:
			saw_u = true;
			break;
		case EncodingPrefix::U32:
			saw_U = true;
			break;
		case EncodingPrefix::WChar:
			saw_L = true;
			break;
		}

		if (!parsed.ud_suffix.empty())
		{
			if (!have_suffix)
			{
				have_suffix = true;
				suffix = parsed.ud_suffix;
			}
			else if (suffix != parsed.ud_suffix)
			{
				output.emit_invalid(joined_source);
				return false;
			}
		}
	}

	if ((saw_u || saw_U || saw_L) && saw_u8)
	{
		output.emit_invalid(joined_source);
		return false;
	}

	int non_u8_prefixes = (saw_u ? 1 : 0) + (saw_U ? 1 : 0) + (saw_L ? 1 : 0);
	if (non_u8_prefixes > 1)
	{
		output.emit_invalid(joined_source);
		return false;
	}

	try
	{
		if (saw_u)
		{
			u16string data;
			for (size_t i = 0; i < code_points.size(); ++i)
				AppendUTF16CodePoint(data, code_points[i]);
			data.push_back(0);

			if (have_suffix)
				output.emit_user_defined_literal_string_array(joined_source, suffix, data.size(), FT_CHAR16_T, data.data(), data.size() * sizeof(char16_t));
			else
				output.emit_literal_array(joined_source, data.size(), FT_CHAR16_T, data.data(), data.size() * sizeof(char16_t));
		}
		else if (saw_U)
		{
			u32string data;
			for (size_t i = 0; i < code_points.size(); ++i)
				AppendUTF32CodePoint(data, code_points[i]);
			data.push_back(0);

			if (have_suffix)
				output.emit_user_defined_literal_string_array(joined_source, suffix, data.size(), FT_CHAR32_T, data.data(), data.size() * sizeof(char32_t));
			else
				output.emit_literal_array(joined_source, data.size(), FT_CHAR32_T, data.data(), data.size() * sizeof(char32_t));
		}
		else if (saw_L)
		{
			wstring data;
			for (size_t i = 0; i < code_points.size(); ++i)
				AppendWCharCodePoint(data, code_points[i]);
			data.push_back(0);

			if (have_suffix)
				output.emit_user_defined_literal_string_array(joined_source, suffix, data.size(), FT_WCHAR_T, data.data(), data.size() * sizeof(wchar_t));
			else
				output.emit_literal_array(joined_source, data.size(), FT_WCHAR_T, data.data(), data.size() * sizeof(wchar_t));
		}
		else
		{
			string data;
			for (size_t i = 0; i < code_points.size(); ++i)
				AppendUTF8CodePoint(data, code_points[i]);
			data.push_back(0);

			if (have_suffix)
				output.emit_user_defined_literal_string_array(joined_source, suffix, data.size(), FT_CHAR, data.data(), data.size());
			else
				output.emit_literal_array(joined_source, data.size(), FT_CHAR, data.data(), data.size());
		}
	}
	catch (exception&)
	{
		output.emit_invalid(joined_source);
		return false;
	}

	return true;
}

static void EmitIntegerLiteral(DebugPostTokenOutputStream& output, const string& source, EFundamentalType type, unsigned __int128 value)
{
	switch (type)
	{
	case FT_INT:
		{
			int v = (int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	case FT_LONG_INT:
		{
			long int v = (long int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	case FT_LONG_LONG_INT:
		{
			long long int v = (long long int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	case FT_UNSIGNED_INT:
		{
			unsigned int v = (unsigned int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	case FT_UNSIGNED_LONG_INT:
		{
			unsigned long int v = (unsigned long int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	case FT_UNSIGNED_LONG_LONG_INT:
		{
			unsigned long long int v = (unsigned long long int) value;
			output.emit_literal(source, type, &v, sizeof(v));
		}
		break;
	default:
		assert(false);
		break;
	}
}

static void EmitCharacterLiteral(DebugPostTokenOutputStream& output, const string& source, const ParsedCharacterLiteral& parsed)
{
	if (!parsed.valid)
	{
		output.emit_invalid(source);
		return;
	}

	try
	{
		if (parsed.encoding == EncodingPrefix::Ordinary)
		{
			if (parsed.code_point <= 127)
			{
				char v = (char) parsed.code_point;
				if (parsed.user_defined)
					output.emit_user_defined_literal_character(source, parsed.ud_suffix, FT_CHAR, &v, sizeof(v));
				else
					output.emit_literal(source, FT_CHAR, &v, sizeof(v));
			}
			else
			{
				int v = parsed.code_point;
				if (parsed.user_defined)
					output.emit_user_defined_literal_character(source, parsed.ud_suffix, FT_INT, &v, sizeof(v));
				else
					output.emit_literal(source, FT_INT, &v, sizeof(v));
			}
			return;
		}

		if (parsed.encoding == EncodingPrefix::U16)
		{
			if (parsed.code_point > 0xFFFF)
			{
				output.emit_invalid(source);
				return;
			}

			char16_t v = (char16_t) parsed.code_point;
			if (parsed.user_defined)
				output.emit_user_defined_literal_character(source, parsed.ud_suffix, FT_CHAR16_T, &v, sizeof(v));
			else
				output.emit_literal(source, FT_CHAR16_T, &v, sizeof(v));
			return;
		}

		if (parsed.encoding == EncodingPrefix::U32)
		{
			char32_t v = (char32_t) parsed.code_point;
			if (parsed.user_defined)
				output.emit_user_defined_literal_character(source, parsed.ud_suffix, FT_CHAR32_T, &v, sizeof(v));
			else
				output.emit_literal(source, FT_CHAR32_T, &v, sizeof(v));
			return;
		}

		if (parsed.encoding == EncodingPrefix::WChar)
		{
			wchar_t v = (wchar_t) parsed.code_point;
			if (parsed.user_defined)
				output.emit_user_defined_literal_character(source, parsed.ud_suffix, FT_WCHAR_T, &v, sizeof(v));
			else
				output.emit_literal(source, FT_WCHAR_T, &v, sizeof(v));
			return;
		}

		assert(false);
	}
	catch (exception&)
	{
		output.emit_invalid(source);
	}
}

static void EmitNumberLiteral(DebugPostTokenOutputStream& output, const string& source, const ParsedNumberLiteral& parsed)
{
	switch (parsed.kind)
	{
	case NumericKind::Invalid:
		output.emit_invalid(source);
		return;
	case NumericKind::Integer:
		EmitIntegerLiteral(output, source, parsed.int_type, parsed.int_value);
		return;
	case NumericKind::Floating:
		{
			if (parsed.float_type == FT_FLOAT)
			{
				float v = PA2Decode_float(source);
				output.emit_literal(source, FT_FLOAT, &v, sizeof(v));
			}
			else if (parsed.float_type == FT_LONG_DOUBLE)
			{
				long double v = PA2Decode_long_double(source);
				output.emit_literal(source, FT_LONG_DOUBLE, &v, sizeof(v));
			}
			else
			{
				double v = PA2Decode_double(source);
				output.emit_literal(source, FT_DOUBLE, &v, sizeof(v));
			}
		}
		return;
	case NumericKind::UserDefinedInteger:
		output.emit_user_defined_literal_integer(source, parsed.ud_suffix, parsed.prefix);
		return;
	case NumericKind::UserDefinedFloating:
		output.emit_user_defined_literal_floating(source, parsed.ud_suffix, parsed.prefix);
		return;
	}
}

static void EmitSingleToken(DebugPostTokenOutputStream& output, const CollectedPPToken& token)
{
	switch (token.kind)
	{
	case CollectedPPTokenKind::HeaderName:
	case CollectedPPTokenKind::NonWhitespaceCharacter:
		output.emit_invalid(token.source);
		break;

	case CollectedPPTokenKind::Identifier:
	case CollectedPPTokenKind::PreprocessingOpOrPunc:
		{
			unordered_map<string, ETokenType>::const_iterator it = StringToTokenTypeMap.find(token.source);
			if (it != StringToTokenTypeMap.end())
			{
				output.emit_simple(token.source, it->second);
			}
			else if (token.kind == CollectedPPTokenKind::Identifier)
			{
				output.emit_identifier(token.source);
			}
			else if (token.source == "#" || token.source == "##" || token.source == "%:" || token.source == "%:%:")
			{
				output.emit_invalid(token.source);
			}
			else
			{
				output.emit_invalid(token.source);
			}
		}
		break;

	case CollectedPPTokenKind::PpNumber:
		EmitNumberLiteral(output, token.source, ParsePpNumber(token.source));
		break;

	case CollectedPPTokenKind::CharacterLiteral:
	case CollectedPPTokenKind::UserDefinedCharacterLiteral:
		EmitCharacterLiteral(output, token.source, ParseCharacterLiteralToken(token.source));
		break;

	case CollectedPPTokenKind::StringLiteral:
	case CollectedPPTokenKind::UserDefinedStringLiteral:
		assert(false);
		break;

	case CollectedPPTokenKind::Whitespace:
	case CollectedPPTokenKind::NewLine:
		break;
	}
}

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		string input = oss.str();

		CollectingPPTokenStream collector;
		PPTokenizer tokenizer(collector);

		for (size_t i = 0; i < input.size(); ++i)
		{
			unsigned char code_unit = (unsigned char) input[i];
			tokenizer.process(code_unit);
		}
		tokenizer.process(EndOfFile);

		DebugPostTokenOutputStream output;
		const vector<CollectedPPToken>& tokens = collector.tokens;

		for (size_t i = 0; i < tokens.size(); )
		{
			if (tokens[i].kind == CollectedPPTokenKind::Whitespace || tokens[i].kind == CollectedPPTokenKind::NewLine)
			{
				++i;
				continue;
			}

			if (tokens[i].kind == CollectedPPTokenKind::StringLiteral
				|| tokens[i].kind == CollectedPPTokenKind::UserDefinedStringLiteral)
			{
				size_t j = i;
				while (j < tokens.size())
				{
					if (tokens[j].kind == CollectedPPTokenKind::Whitespace
						|| tokens[j].kind == CollectedPPTokenKind::NewLine)
					{
						++j;
						continue;
					}

					if (tokens[j].kind == CollectedPPTokenKind::StringLiteral
						|| tokens[j].kind == CollectedPPTokenKind::UserDefinedStringLiteral)
					{
						++j;
						continue;
					}

					break;
				}

				ParseAndEmitStringGroup(tokens, i, j, output);
				i = j;
				continue;
			}

			EmitSingleToken(output, tokens[i]);
			++i;
		}

		output.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
