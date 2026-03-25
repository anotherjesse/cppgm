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
#include <vector>

using namespace std;

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

constexpr int EndOfFile = -1;

struct CodePoint
{
	int value;
	bool from_ucn;
};

enum class PPKind
{
	Whitespace,
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
	PPKind kind;
	string source;
};

int HexCharToValue(int c)
{
	switch (c)
	{
	case '0': return 0; case '1': return 1; case '2': return 2; case '3': return 3;
	case '4': return 4; case '5': return 5; case '6': return 6; case '7': return 7;
	case '8': return 8; case '9': return 9; case 'a': case 'A': return 10;
	case 'b': case 'B': return 11; case 'c': case 'C': return 12; case 'd': case 'D': return 13;
	case 'e': case 'E': return 14; case 'f': case 'F': return 15;
	default: throw runtime_error("invalid hex digit");
	}
}

bool IsDigit(int cp) { return '0' <= cp && cp <= '9'; }
bool IsHexDigit(int cp) { return IsDigit(cp) || ('a' <= cp && cp <= 'f') || ('A' <= cp && cp <= 'F'); }
bool IsNondigit(int cp) { return cp == '_' || ('a' <= cp && cp <= 'z') || ('A' <= cp && cp <= 'Z'); }
bool IsWhitespaceNoNewline(int cp) { return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f'; }

string EncodeUTF8(int cp)
{
	string out;
	if (cp <= 0x7F)
		out.push_back(static_cast<char>(cp));
	else if (cp <= 0x7FF)
	{
		out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else
	{
		out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	return out;
}

vector<int> DecodeUTF8(const string& input)
{
	vector<int> out;
	for (size_t i = 0; i < input.size(); )
	{
		unsigned char c0 = static_cast<unsigned char>(input[i]);
		int cp = 0;
		size_t need = 0;
		int minv = 0;
		if (c0 <= 0x7F) { cp = c0; need = 1; }
		else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; need = 2; minv = 0x80; }
		else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; need = 3; minv = 0x800; }
		else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; need = 4; minv = 0x10000; }
		else throw runtime_error("invalid utf-8");
		if (i + need > input.size()) throw runtime_error("truncated utf-8");
		for (size_t j = 1; j < need; ++j)
		{
			unsigned char cj = static_cast<unsigned char>(input[i + j]);
			if ((cj & 0xC0) != 0x80) throw runtime_error("invalid utf-8");
			cp = (cp << 6) | (cj & 0x3F);
		}
		if (need > 1 && cp < minv) throw runtime_error("overlong utf-8");
		out.push_back(cp);
		i += need;
	}
	return out;
}

vector<CodePoint> Phase123(const string& input)
{
	unordered_map<int, int> tri = {{'=', '#'}, {'/', '\\'}, {'\'', '^'}, {'(', '['}, {')', ']'}, {'!', '|'}, {'<', '{'}, {'>', '}'}, {'-', '~'}};
	vector<int> raw = DecodeUTF8(input);
	vector<CodePoint> out;
	auto match_ascii = [&](size_t at, const string& text)
	{
		if (at + text.size() > raw.size()) return false;
		for (size_t i = 0; i < text.size(); ++i)
			if (raw[at + i] != static_cast<unsigned char>(text[i])) return false;
		return true;
	};
	auto copy_raw_string = [&](size_t& i) -> bool
	{
		if (i > 0)
		{
			int prev = raw[i - 1];
			if (prev == '"' || prev == '\'' || IsDigit(prev) || IsNondigit(prev)) return false;
		}
		size_t prefix = match_ascii(i, "u8R\"") ? 4 : match_ascii(i, "uR\"") || match_ascii(i, "UR\"") || match_ascii(i, "LR\"") ? 3 : match_ascii(i, "R\"") ? 2 : 0;
		if (!prefix) return false;
		size_t j = i;
		string delim;
		for (size_t k = 0; k < prefix; ++k, ++j) out.push_back({raw[j], false});
		while (j < raw.size() && raw[j] != '(') { delim.push_back(static_cast<char>(raw[j])); out.push_back({raw[j], false}); ++j; }
		if (j == raw.size()) { i = j; return true; }
		out.push_back({raw[j++], false});
		while (j < raw.size())
		{
			out.push_back({raw[j], false});
			if (raw[j] == ')')
			{
				bool ok = j + delim.size() + 1 < raw.size();
				for (size_t k = 0; ok && k < delim.size(); ++k) ok = raw[j + 1 + k] == static_cast<unsigned char>(delim[k]);
				if (ok && raw[j + delim.size() + 1] == '"')
				{
					for (size_t k = 0; k < delim.size() + 1; ++k) out.push_back({raw[j + 1 + k], false});
					j += delim.size() + 2;
					break;
				}
			}
			++j;
		}
		i = j;
		return true;
	};
	for (size_t i = 0; i < raw.size(); )
	{
		if (copy_raw_string(i)) continue;
		int cp = raw[i];
		if (cp == '?' && i + 2 < raw.size() && raw[i + 1] == '?' && tri.count(raw[i + 2])) { cp = tri[raw[i + 2]]; i += 3; }
		else ++i;
		if (cp == '\\' && i < raw.size() && (raw[i] == 'u' || raw[i] == 'U'))
		{
			int digits = raw[i] == 'u' ? 4 : 8;
			if (i + digits >= raw.size()) throw runtime_error("truncated universal-character-name");
			int value = 0;
			for (int j = 0; j < digits; ++j)
			{
				if (!IsHexDigit(raw[i + 1 + j])) throw runtime_error("invalid universal-character-name");
				value = value * 16 + HexCharToValue(raw[i + 1 + j]);
			}
			i += digits + 1;
			out.push_back({value, true});
			continue;
		}
		if (cp == '\\' && i < raw.size() && raw[i] == '\n') { ++i; continue; }
		out.push_back({cp, false});
	}
	if (!out.empty() && out.back().value != '\n') out.push_back({'\n', false});
	return out;
}

bool IsCombiningMark(int cp) { return 0x300 <= cp && cp <= 0x36F; }
bool IsIdentifierInitial(int cp) { return IsNondigit(cp) || (cp >= 0x80 && !IsCombiningMark(cp)); }
bool IsIdentifierContinue(int cp) { return IsIdentifierInitial(cp) || IsDigit(cp) || IsCombiningMark(cp); }

struct PPTokenizer
{
	vector<CodePoint> cps;
	size_t pos = 0;
	bool line_start = true;
	int include_state = 0;
	vector<PPToken> out;

	int peek(size_t off = 0) const { size_t i = pos + off; return i < cps.size() ? cps[i].value : EndOfFile; }
	string take() { return EncodeUTF8(cps[pos++].value); }
	bool match(const string& s) const { for (size_t i = 0; i < s.size(); ++i) if (peek(i) != static_cast<unsigned char>(s[i])) return false; return true; }

	string scan_identifier() { string s; s += take(); while (IsIdentifierContinue(peek())) s += take(); return s; }
	string scan_ppnum()
	{
		string s;
		if (peek() == '.') s += take();
		while (true)
		{
			int cp = peek();
			if (IsDigit(cp) || cp == '.') s += take();
			else if ((cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P') && (IsDigit(peek(1)) || peek(1) == '+' || peek(1) == '-'))
			{
				s += take();
				if (peek() == '+' || peek() == '-') s += take();
			}
			else if (IsIdentifierContinue(cp)) s += take();
			else break;
		}
		return s;
	}
	string scan_escape()
	{
		string s; s += take(); s += take();
		if (s[1] == 'x') while (IsHexDigit(peek())) s += take();
		else if ('0' <= s[1] && s[1] <= '7') { for (int i = 0; i < 2 && '0' <= peek() && peek() <= '7'; ++i) s += take(); }
		return s;
	}
	string scan_quoted()
	{
		string s;
		if (match("u8") && peek(2) == '"') { s += take(); s += take(); }
		else if ((peek() == 'u' || peek() == 'U' || peek() == 'L') && (peek(1) == '"' || peek(1) == '\'')) s += take();
		int quote = peek(); s += take();
		while (true)
		{
			int cp = peek();
			if (cp == EndOfFile || cp == '\n') throw runtime_error("unterminated literal");
			if (cp == '\\') s += scan_escape();
			else { s += take(); if (cp == quote) break; }
		}
		return s;
	}
	string scan_raw()
	{
		string s;
		if (match("u8R\"")) { s += take(); s += take(); s += take(); s += take(); }
		else if ((peek() == 'u' || peek() == 'U' || peek() == 'L') && peek(1) == 'R' && peek(2) == '"') { s += take(); s += take(); s += take(); }
		else { s += take(); s += take(); }
		string delim;
		while (peek() != '(') { if (peek() == EndOfFile || peek() == '\n') throw runtime_error("unterminated raw string"); delim.push_back(static_cast<char>(peek())); s += take(); }
		s += take();
		while (true)
		{
			if (peek() == EndOfFile) throw runtime_error("unterminated raw string");
			int cp = peek(); s += take();
			if (cp == ')')
			{
				bool ok = true;
				for (size_t i = 0; i < delim.size(); ++i) if (peek(i) != static_cast<unsigned char>(delim[i])) ok = false;
				if (ok && peek(delim.size()) == '"')
				{
					for (size_t i = 0; i < delim.size() + 1; ++i) s += take();
					break;
				}
			}
		}
		return s;
	}
	bool is_raw_prefix() const { return match("R\"") || match("u8R\"") || match("uR\"") || match("UR\"") || match("LR\""); }

	vector<PPToken> run(const string& input)
	{
		cps = Phase123(input);
		while (pos < cps.size())
		{
			bool ws = false;
			while (IsWhitespaceNoNewline(peek()) || (peek() == '/' && peek(1) == '/') || (peek() == '/' && peek(1) == '*'))
			{
				ws = true;
				if (IsWhitespaceNoNewline(peek())) while (IsWhitespaceNoNewline(peek())) ++pos;
				else if (peek() == '/' && peek(1) == '/'){ pos += 2; while (peek() != EndOfFile && peek() != '\n') ++pos; }
				else { pos += 2; while (!(peek() == '*' && peek(1) == '/')) { if (peek() == EndOfFile) throw runtime_error("unterminated comment"); ++pos; } pos += 2; }
			}
			if (ws) { out.push_back({PPKind::Whitespace, ""}); continue; }
			if (peek() == '\n') { ++pos; out.push_back({PPKind::NewLine, ""}); line_start = true; include_state = 0; continue; }
			if (include_state == 2 && (peek() == '<' || peek() == '"'))
			{
				string s; int end = peek() == '<' ? '>' : '"'; s += take();
				while (peek() != end) { if (peek() == EndOfFile || peek() == '\n') throw runtime_error("unterminated header-name"); s += take(); }
				s += take(); out.push_back({PPKind::HeaderName, s}); include_state = 0; line_start = false; continue;
			}
			if (peek() == '.' && IsDigit(peek(1))) { out.push_back({PPKind::PPNumber, scan_ppnum()}); include_state = 0; line_start = false; continue; }
			if (IsDigit(peek())) { out.push_back({PPKind::PPNumber, scan_ppnum()}); include_state = 0; line_start = false; continue; }
			if (is_raw_prefix()) { string s = scan_raw(); if (IsIdentifierInitial(peek())) out.push_back({PPKind::UserDefinedStringLiteral, s + scan_identifier()}); else out.push_back({PPKind::StringLiteral, s}); include_state = 0; line_start = false; continue; }
			if (peek() == '"' || peek() == '\'' || ((peek() == 'u' || peek() == 'U' || peek() == 'L') && (peek(1) == '"' || peek(1) == '\'')) || (match("u8") && peek(2) == '"'))
			{
				string s = scan_quoted(); bool is_char = s[s[0] == 'u' || s[0] == 'U' || s[0] == 'L' ? 1 : (s.size() >= 2 && s[0] == 'u' && s[1] == '8' ? 2 : 0)] == '\'';
				if (IsIdentifierInitial(peek())) out.push_back({is_char ? PPKind::UserDefinedCharacterLiteral : PPKind::UserDefinedStringLiteral, s + scan_identifier()});
				else out.push_back({is_char ? PPKind::CharacterLiteral : PPKind::StringLiteral, s});
				include_state = 0; line_start = false; continue;
			}
			if (IsIdentifierInitial(peek()))
			{
				string s = scan_identifier();
				out.push_back({PPKind::Identifier, s});
				include_state = (include_state == 1 && s == "include") ? 2 : 0;
				line_start = false;
				continue;
			}
			static const vector<string> ops = {"%:%:", "...", ">>=", "<<=", "->*", ".*", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "::", "##", "<:", ":>", "<%", "%>", "%:", "{", "}", "[", "]", "#", "(", ")", ";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|", "~", "!", "=", "<", ">", ","};
			if (match("<::") && peek(3) != ':' && peek(3) != '>') { out.push_back({PPKind::PreprocessingOpOrPunc, take()}); include_state = 0; line_start = false; continue; }
			bool found = false;
			for (const string& op : ops) if (match(op)) { string s; for (size_t i = 0; i < op.size(); ++i) s += take(); out.push_back({PPKind::PreprocessingOpOrPunc, s}); include_state = (line_start && (s == "#" || s == "%:")) ? 1 : 0; line_start = false; found = true; break; }
			if (found) continue;
			out.push_back({PPKind::NonWhitespaceCharacter, take()}); include_state = 0; line_start = false;
		}
		return out;
	}
};

template<typename T>
void append_scalar(vector<unsigned char>& out, T value)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(&value);
	out.insert(out.end(), p, p + sizeof(T));
}

struct StringData
{
	EFundamentalType type;
	vector<unsigned char> bytes;
	size_t elements = 0;
};

int string_prefix_kind(const string& src)
{
	if (src.compare(0, 2, "u8") == 0) return 1;
	if (!src.empty() && src[0] == 'u') return 2;
	if (!src.empty() && src[0] == 'U') return 3;
	if (!src.empty() && src[0] == 'L') return 4;
	return 0;
}

vector<int> bytes_to_codepoints(EFundamentalType type, const vector<unsigned char>& bytes)
{
	if (type == FT_CHAR)
		return DecodeUTF8(string(bytes.begin(), bytes.end()));
	vector<int> cps;
	if (type == FT_CHAR16_T)
	{
		for (size_t i = 0; i + 1 < bytes.size(); i += 2)
		{
			char16_t v = static_cast<char16_t>(bytes[i] | (bytes[i + 1] << 8));
			if (0xD800 <= v && v <= 0xDBFF && i + 3 < bytes.size())
			{
				char16_t w = static_cast<char16_t>(bytes[i + 2] | (bytes[i + 3] << 8));
				if (0xDC00 <= w && w <= 0xDFFF)
				{
					cps.push_back(0x10000 + (((v - 0xD800) << 10) | (w - 0xDC00)));
					i += 2;
					continue;
				}
			}
			cps.push_back(v);
		}
	}
	else
	{
		for (size_t i = 0; i + 3 < bytes.size(); i += 4)
		{
			char32_t v = static_cast<char32_t>(bytes[i] | (bytes[i + 1] << 8) | (bytes[i + 2] << 16) | (bytes[i + 3] << 24));
			cps.push_back(v);
		}
	}
	return cps;
}

void append_codepoints_as(EFundamentalType type, const vector<int>& cps, vector<unsigned char>& out)
{
	for (int cp : cps)
	{
		if (type == FT_CHAR)
		{
			string enc = EncodeUTF8(cp);
			out.insert(out.end(), enc.begin(), enc.end());
		}
		else if (type == FT_CHAR16_T)
		{
			if (cp <= 0xFFFF)
			{
				char16_t v = cp;
				append_scalar(out, v);
			}
			else
			{
				int x = cp - 0x10000;
				char16_t hi = 0xD800 + (x >> 10);
				char16_t lo = 0xDC00 + (x & 0x3FF);
				append_scalar(out, hi);
				append_scalar(out, lo);
			}
		}
		else
		{
			char32_t v = cp;
			append_scalar(out, v);
		}
	}
}

pair<string, string> split_ud_suffix(const string& s)
{
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i] == '_' && i + 1 < s.size() && IsNondigit(static_cast<unsigned char>(s[i + 1])))
		{
			bool ok = true;
			for (size_t j = i + 1; j < s.size(); ++j)
				if (!IsIdentifierContinue(static_cast<unsigned char>(s[j])))
					ok = false;
			if (ok)
				return make_pair(s.substr(0, i), s.substr(i));
			return make_pair(s, "");
		}
	for (size_t i = 0; i < s.size(); ++i)
		if (s[i] == '_' && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) >= 0x80)
		{
			bool ok = true;
			vector<int> cps = DecodeUTF8(s.substr(i + 1));
			if (cps.empty()) ok = false;
			for (int cp : cps)
				if (!IsIdentifierContinue(cp))
					ok = false;
			if (ok)
				return make_pair(s.substr(0, i), s.substr(i));
			return make_pair(s, "");
		}
	return make_pair(s, "");
}

bool parse_integer_core(const string& s, unsigned __int128& value, int& base)
{
	base = 10;
	size_t i = 0;
	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; i = 2; if (i == s.size()) return false; }
	else if (s.size() > 1 && s[0] == '0') { base = 8; i = 1; }
	value = 0;
	for (; i < s.size(); ++i)
	{
		int d;
		if (base == 16 && IsHexDigit(s[i])) d = HexCharToValue(s[i]);
		else if (base != 16 && IsDigit(s[i])) d = s[i] - '0';
		else return false;
		if (d >= base) return false;
		value = value * base + d;
	}
	return true;
}

bool parse_integer_literal(const string& src, string& prefix, string& ud_suffix, EFundamentalType& type, vector<unsigned char>& bytes)
{
	pair<string,string> split = split_ud_suffix(src);
	prefix = split.first;
	ud_suffix = split.second;
	string digits = prefix;
	if (!ud_suffix.empty())
	{
		unsigned __int128 tmp = 0;
		int tmp_base = 10;
		return !digits.empty() && parse_integer_core(digits, tmp, tmp_base);
	}
	string suffix;
	while (!digits.empty() && (digits.back() == 'u' || digits.back() == 'U' || digits.back() == 'l' || digits.back() == 'L'))
	{
		suffix.insert(suffix.begin(), digits.back());
		digits.pop_back();
	}
	unsigned __int128 value = 0; int base = 10;
	if (!parse_integer_core(digits, value, base)) return false;
	bool is_unsigned = false; int longness = 0;
	size_t i = 0;
	if (i < suffix.size() && (suffix[i] == 'u' || suffix[i] == 'U')) { is_unsigned = true; ++i; }
	if (i < suffix.size() && (suffix[i] == 'l' || suffix[i] == 'L')) { ++longness; ++i; if (i < suffix.size() && suffix[i] == suffix[i - 1]) { ++longness; ++i; } }
	if (!is_unsigned && i < suffix.size() && (suffix[i] == 'u' || suffix[i] == 'U')) { is_unsigned = true; ++i; }
	if (i != suffix.size() || longness > 2) return false;
	struct Cand { EFundamentalType type; bool uns; int bits; };
	vector<Cand> cands;
	if (!is_unsigned && longness == 0 && base == 10) cands = {{FT_INT,false,32},{FT_LONG_INT,false,64},{FT_LONG_LONG_INT,false,64}};
	else if (!is_unsigned && longness == 0) cands = {{FT_INT,false,32},{FT_UNSIGNED_INT,true,32},{FT_LONG_INT,false,64},{FT_UNSIGNED_LONG_INT,true,64},{FT_LONG_LONG_INT,false,64},{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	else if (!is_unsigned && longness == 1 && base == 10) cands = {{FT_LONG_INT,false,64},{FT_LONG_LONG_INT,false,64}};
	else if (!is_unsigned && longness == 1) cands = {{FT_LONG_INT,false,64},{FT_UNSIGNED_LONG_INT,true,64},{FT_LONG_LONG_INT,false,64},{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	else if (!is_unsigned && longness == 2 && base == 10) cands = {{FT_LONG_LONG_INT,false,64}};
	else if (!is_unsigned && longness == 2) cands = {{FT_LONG_LONG_INT,false,64},{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	else if (is_unsigned && longness == 0) cands = {{FT_UNSIGNED_INT,true,32},{FT_UNSIGNED_LONG_INT,true,64},{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	else if (is_unsigned && longness == 1) cands = {{FT_UNSIGNED_LONG_INT,true,64},{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	else cands = {{FT_UNSIGNED_LONG_LONG_INT,true,64}};
	for (const Cand& c : cands)
	{
		unsigned __int128 maxv = c.uns ? ((static_cast<unsigned __int128>(1) << c.bits) - 1) : ((static_cast<unsigned __int128>(1) << (c.bits - 1)) - 1);
		if (value > maxv) continue;
		type = c.type;
		switch (type)
		{
		case FT_INT: { int v = static_cast<int>(value); append_scalar(bytes, v); break; }
		case FT_LONG_INT: { long v = static_cast<long>(value); append_scalar(bytes, v); break; }
		case FT_LONG_LONG_INT: { long long v = static_cast<long long>(value); append_scalar(bytes, v); break; }
		case FT_UNSIGNED_INT: { unsigned int v = static_cast<unsigned int>(value); append_scalar(bytes, v); break; }
		case FT_UNSIGNED_LONG_INT: { unsigned long v = static_cast<unsigned long>(value); append_scalar(bytes, v); break; }
		case FT_UNSIGNED_LONG_LONG_INT: { unsigned long long v = static_cast<unsigned long long>(value); append_scalar(bytes, v); break; }
		default: break;
		}
		return true;
	}
	return false;
}

bool looks_like_float(const string& s)
{
	if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		return false;
	return s.find('.') != string::npos || s.find('e') != string::npos || s.find('E') != string::npos;
}

bool parse_float_literal(const string& src, string& prefix, string& ud_suffix, EFundamentalType& type, vector<unsigned char>& bytes)
{
	pair<string,string> split = split_ud_suffix(src);
	string body = split.first;
	ud_suffix = split.second;
	char suff = 0;
	if (!body.empty() && (body.back() == 'f' || body.back() == 'F' || body.back() == 'l' || body.back() == 'L')) { suff = body.back(); body.pop_back(); }
	if (!looks_like_float(body)) return false;
	size_t exp = body.find_first_of("eE");
	string left = exp == string::npos ? body : body.substr(0, exp);
	string right = exp == string::npos ? "" : body.substr(exp + 1);
	if (left.empty()) return false;
	bool left_ok = false;
	if (left.find('.') != string::npos)
	{
		size_t dot = left.find('.');
		string a = left.substr(0, dot), b = left.substr(dot + 1);
		bool a_ok = a.empty() || all_of(a.begin(), a.end(), IsDigit);
		bool b_ok = b.empty() || all_of(b.begin(), b.end(), IsDigit);
		left_ok = (a_ok && b_ok && (!a.empty() || !b.empty()));
	}
	else left_ok = all_of(left.begin(), left.end(), IsDigit) && exp != string::npos;
	if (!left_ok) return false;
	if (!right.empty())
	{
		size_t i = (right[0] == '+' || right[0] == '-') ? 1 : 0;
		if (i == right.size()) return false;
		for (; i < right.size(); ++i) if (!IsDigit(right[i])) return false;
	}
	if (!ud_suffix.empty()) { prefix = body; return true; }
	prefix = body + (suff ? string(1, suff) : "");
	if (suff == 'f' || suff == 'F') { type = FT_FLOAT; float v = PA2Decode_float(prefix); append_scalar(bytes, v); }
	else if (suff == 'l' || suff == 'L') { type = FT_LONG_DOUBLE; long double v = PA2Decode_long_double(prefix); append_scalar(bytes, v); }
	else { type = FT_DOUBLE; double v = PA2Decode_double(prefix); append_scalar(bytes, v); }
	return true;
}

int decode_escape_cp(const string& s, size_t& i)
{
	char c = s[i++];
	switch (c)
	{
	case '\'': return '\'';
	case '"': return '"';
	case '?': return '?';
	case '\\': return '\\';
	case 'a': return '\a';
	case 'b': return '\b';
	case 'f': return '\f';
	case 'n': return '\n';
	case 'r': return '\r';
	case 't': return '\t';
	case 'v': return '\v';
	case 'x': { int value = 0; while (i < s.size() && IsHexDigit(s[i])) value = value * 16 + HexCharToValue(s[i++]); return value; }
	case 'u': { int value = 0; for (int j = 0; j < 4; ++j) value = value * 16 + HexCharToValue(s[i++]); return value; }
	case 'U': { int value = 0; for (int j = 0; j < 8; ++j) value = value * 16 + HexCharToValue(s[i++]); return value; }
	default:
		if ('0' <= c && c <= '7') { int value = c - '0'; for (int j = 0; j < 2 && i < s.size() && '0' <= s[i] && s[i] <= '7'; ++j) value = value * 8 + (s[i++] - '0'); return value; }
		throw runtime_error("invalid escape");
	}
}

bool literal_prefix_info(const string& src, bool& is_raw, size_t& start_content, size_t& end_content, EFundamentalType& elem_type, bool& is_char, string& ud)
{
	is_raw = false; start_content = 0; end_content = string::npos; ud.clear();
	size_t prefix = 0;
	if (src.compare(0, 2, "u8") == 0 && src.size() > 2 && (src[2] == '"' || (src.size() > 3 && src[2] == 'R' && src[3] == '"'))) { prefix = 2; elem_type = FT_CHAR; }
	else if (!src.empty() && src[0] == 'u') { prefix = 1; elem_type = FT_CHAR16_T; }
	else if (!src.empty() && src[0] == 'U') { prefix = 1; elem_type = FT_CHAR32_T; }
	else if (!src.empty() && src[0] == 'L') { prefix = 1; elem_type = FT_WCHAR_T; }
	else elem_type = FT_CHAR;
	size_t q = prefix;
	if (q < src.size() && src[q] == 'R') { is_raw = true; ++q; }
	if (q >= src.size() || (src[q] != '"' && src[q] != '\'')) return false;
	is_char = src[q] == '\'';
	if (is_raw)
	{
		size_t open = src.find('(', q + 1);
		size_t close = src.rfind(')');
		if (open == string::npos || close == string::npos || close < open) return false;
		start_content = open + 1;
		end_content = close;
		size_t suffix_start = src.find('"', close);
		if (suffix_start == string::npos) return false;
		ud = src.substr(suffix_start + 1);
		return true;
	}
	size_t end = src.find_last_of(src[q]);
	if (end == string::npos || end == q) return false;
	start_content = q + 1;
	end_content = end;
	ud = src.substr(end + 1);
	return true;
}

bool decode_string_or_char(const string& src, bool allow_ud, StringData& data, string& ud_suffix, bool& is_char_token)
{
	bool is_raw = false;
	size_t begin = 0, end = 0;
	EFundamentalType elem_type = FT_CHAR;
	if (!literal_prefix_info(src, is_raw, begin, end, elem_type, is_char_token, ud_suffix)) return false;
	if (!allow_ud && !ud_suffix.empty()) return false;
	vector<int> cps;
	if (is_raw) cps = DecodeUTF8(src.substr(begin, end - begin));
	else
	{
		string content = src.substr(begin, end - begin);
		for (size_t i = 0; i < content.size(); )
		{
			if (content[i] == '\\') { ++i; cps.push_back(decode_escape_cp(content, i)); }
			else
			{
				unsigned char c = static_cast<unsigned char>(content[i]);
				if (c < 0x80) { cps.push_back(c); ++i; }
				else
				{
					size_t n = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
					vector<int> d = DecodeUTF8(content.substr(i, n));
					cps.push_back(d[0]);
					i += n;
				}
			}
		}
	}
	data.type = elem_type;
	if (is_char_token)
	{
		if (cps.size() != 1) return false;
		int cp = cps[0];
		if (elem_type == FT_CHAR)
		{
			if (cp <= 0x7F) { char v = static_cast<char>(cp); append_scalar(data.bytes, v); data.type = FT_CHAR; }
			else { int v = cp; append_scalar(data.bytes, v); data.type = FT_INT; }
		}
		else if (elem_type == FT_CHAR16_T)
		{
			if (cp > 0xFFFF) return false;
			char16_t v = cp;
			append_scalar(data.bytes, v);
		}
		else if (elem_type == FT_CHAR32_T || elem_type == FT_WCHAR_T)
		{
			char32_t v = cp; append_scalar(data.bytes, v);
		}
		data.elements = 1;
		return true;
	}
	for (int cp : cps)
	{
		if (elem_type == FT_CHAR)
		{
			string enc = EncodeUTF8(cp);
			data.bytes.insert(data.bytes.end(), enc.begin(), enc.end());
		}
		else if (elem_type == FT_CHAR16_T)
		{
			if (cp > 0x10FFFF) return false;
			if (cp <= 0xFFFF)
			{
				char16_t v = cp;
				append_scalar(data.bytes, v);
			}
			else
			{
				int x = cp - 0x10000;
				char16_t hi = 0xD800 + (x >> 10);
				char16_t lo = 0xDC00 + (x & 0x3FF);
				append_scalar(data.bytes, hi);
				append_scalar(data.bytes, lo);
			}
		}
		else
		{
			char32_t v = cp; append_scalar(data.bytes, v);
		}
	}
	if (elem_type == FT_CHAR) data.bytes.push_back(0);
	else if (elem_type == FT_CHAR16_T) { char16_t z = 0; append_scalar(data.bytes, z); }
	else { char32_t z = 0; append_scalar(data.bytes, z); }
	data.elements = elem_type == FT_CHAR ? data.bytes.size() : data.bytes.size() / (elem_type == FT_CHAR16_T ? 2 : 4);
	return true;
}

int main()
{
	try
	{
		ostringstream oss; oss << cin.rdbuf();
		PPTokenizer tokenizer;
		vector<PPToken> pp = tokenizer.run(oss.str());
		DebugPostTokenOutputStream output;
		for (size_t i = 0; i < pp.size(); ++i)
		{
			const PPToken& tok = pp[i];
			if (tok.kind == PPKind::Whitespace || tok.kind == PPKind::NewLine) continue;
			if (tok.kind == PPKind::HeaderName || tok.kind == PPKind::NonWhitespaceCharacter) { output.emit_invalid(tok.source); continue; }
			if (tok.kind == PPKind::Identifier)
			{
				auto it = StringToTokenTypeMap.find(tok.source);
				if (it != StringToTokenTypeMap.end()) output.emit_simple(tok.source, it->second);
				else output.emit_identifier(tok.source);
				continue;
			}
			if (tok.kind == PPKind::PreprocessingOpOrPunc)
			{
				if (tok.source == "#" || tok.source == "##" || tok.source == "%:" || tok.source == "%:%:") output.emit_invalid(tok.source);
				else output.emit_simple(tok.source, StringToTokenTypeMap.at(tok.source));
				continue;
			}
			if (tok.kind == PPKind::PPNumber)
			{
				string prefix, uds; EFundamentalType type; vector<unsigned char> bytes;
				if (looks_like_float(tok.source))
				{
					if (!parse_float_literal(tok.source, prefix, uds, type, bytes)) output.emit_invalid(tok.source);
					else if (!uds.empty()) output.emit_user_defined_literal_floating(tok.source, uds, prefix);
					else output.emit_literal(tok.source, type, bytes.data(), bytes.size());
				}
				else
				{
					if (!parse_integer_literal(tok.source, prefix, uds, type, bytes)) output.emit_invalid(tok.source);
					else if (!uds.empty()) output.emit_user_defined_literal_integer(tok.source, uds, prefix);
					else output.emit_literal(tok.source, type, bytes.data(), bytes.size());
				}
				continue;
			}
			if (tok.kind == PPKind::CharacterLiteral || tok.kind == PPKind::UserDefinedCharacterLiteral)
			{
				StringData data; string uds; bool is_char = false;
				if (!decode_string_or_char(tok.source, tok.kind == PPKind::UserDefinedCharacterLiteral, data, uds, is_char) || !is_char) output.emit_invalid(tok.source);
				else if (!uds.empty())
				{
					if (uds[0] != '_') output.emit_invalid(tok.source);
					else output.emit_user_defined_literal_character(tok.source, uds, data.type, data.bytes.data(), data.bytes.size());
				}
				else output.emit_literal(tok.source, data.type, data.bytes.data(), data.bytes.size());
				continue;
			}
			if (tok.kind == PPKind::StringLiteral || tok.kind == PPKind::UserDefinedStringLiteral)
			{
				vector<PPToken> group(1, tok);
				while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::Whitespace || pp[i + 1].kind == PPKind::NewLine)) ++i;
				while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::StringLiteral || pp[i + 1].kind == PPKind::UserDefinedStringLiteral))
				{
					group.push_back(pp[++i]);
					while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::Whitespace || pp[i + 1].kind == PPKind::NewLine)) ++i;
				}
				string joined;
				StringData combined;
				string final_ud;
				bool ok = true;
				bool saw_u8 = false;
				int target_kind = 0;
				vector<int> combined_cps;
				for (size_t gi = 0; gi < group.size(); ++gi)
				{
					if (gi) joined += " ";
					joined += group[gi].source;
					StringData part; string uds; bool is_char = false;
					if (!decode_string_or_char(group[gi].source, group[gi].kind == PPKind::UserDefinedStringLiteral, part, uds, is_char) || is_char) { ok = false; continue; }
					int kind = string_prefix_kind(group[gi].source);
					if (kind == 1) saw_u8 = true;
					if (kind >= 2)
					{
						if (target_kind == 0) target_kind = kind;
						else if (target_kind != kind) ok = false;
					}
					if (target_kind >= 2 && saw_u8) ok = false;
					if (gi == 0) combined.type = part.type;
					if (part.type != FT_CHAR && combined.type == FT_CHAR) combined.type = part.type;
					else if (part.type != FT_CHAR && combined.type != FT_CHAR && part.type != combined.type) { ok = false; continue; }
					if (!uds.empty())
					{
						if (uds[0] != '_') ok = false;
						else if (final_ud.empty()) final_ud = uds;
						else if (final_ud != uds) ok = false;
					}
					part.bytes.resize(part.bytes.size() - (part.type == FT_CHAR ? 1 : part.type == FT_CHAR16_T ? 2 : 4));
					vector<int> cps = bytes_to_codepoints(part.type, part.bytes);
					combined_cps.insert(combined_cps.end(), cps.begin(), cps.end());
					combined.elements += part.elements - 1;
				}
				if (!ok) { output.emit_invalid(joined); continue; }
				combined.bytes.clear();
				append_codepoints_as(combined.type, combined_cps, combined.bytes);
				if (combined.type == FT_CHAR) combined.bytes.push_back(0);
				else if (combined.type == FT_CHAR16_T) { char16_t z = 0; append_scalar(combined.bytes, z); }
				else { char32_t z = 0; append_scalar(combined.bytes, z); }
				combined.elements = combined.type == FT_CHAR ? combined.bytes.size() : combined.bytes.size() / (combined.type == FT_CHAR16_T ? 2 : 4);
				if (!final_ud.empty()) output.emit_user_defined_literal_string_array(joined, final_ud, combined.elements, combined.type, combined.bytes.data(), combined.bytes.size());
				else output.emit_literal_array(joined, combined.elements, combined.type, combined.bytes.data(), combined.bytes.size());
				continue;
			}
		}
		output.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
