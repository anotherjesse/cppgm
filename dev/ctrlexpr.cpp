// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

#include "PPLexer.h"

// mock implementation of IsDefinedIdentifier for PA3
// return true iff first code point is odd
bool PA3Mock_IsDefinedIdentifier(const string& identifier)
{
	if (identifier.empty())
		return false;
	else
		return identifier[0] % 2;
}

enum ELiteralType
{
	LT_INT,
	LT_LONG,
	LT_LONG_LONG,
	LT_UNSIGNED_INT,
	LT_UNSIGNED_LONG,
	LT_UNSIGNED_LONG_LONG,
	LT_CHAR,
	LT_WCHAR,
	LT_CHAR16,
	LT_CHAR32
};

struct IntegerLiteralInfo
{
	bool ok = false;
	unsigned long long value = 0;
	bool decimal = false;
	bool unsigned_suffix = false;
	int length_suffix = 0; // 0 none, 1 long, 2 long long
};

struct CharacterLiteralInfo
{
	bool ok = false;
	ELiteralType type = LT_INT;
	uint32_t value = 0;
};

struct Value
{
	bool is_unsigned = false;
	uint64_t bits = 0;
};

enum ETokenKind
{
	TK_END,
	TK_LITERAL,
	TK_IDENTIFIER,
	TK_LPAREN,
	TK_RPAREN,
	TK_QMARK,
	TK_COLON,
	TK_PLUS,
	TK_MINUS,
	TK_LNOT,
	TK_COMPL,
	TK_STAR,
	TK_DIV,
	TK_MOD,
	TK_LSHIFT,
	TK_RSHIFT,
	TK_LT,
	TK_GT,
	TK_LE,
	TK_GE,
	TK_EQ,
	TK_NE,
	TK_AMP,
	TK_XOR,
	TK_BOR,
	TK_LAND,
	TK_LOR
};

struct Token
{
	ETokenKind kind = TK_END;
	string text;
	Value value;
};

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

bool IsValidUnicodeCodePoint(int cp)
{
	return cp >= 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
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

bool DecodeFixedHexEscape(const string& s, size_t& i, int digits, int& cp)
{
	if (i + static_cast<size_t>(digits) > s.size())
		return false;

	unsigned int value = 0;
	for (int j = 0; j < digits; ++j)
	{
		if (!IsHexDigitChar(s[i + static_cast<size_t>(j)]))
			return false;
		value = value * 16u + static_cast<unsigned int>(HexCharValue(s[i + static_cast<size_t>(j)]));
	}

	i += static_cast<size_t>(digits);
	cp = static_cast<int>(value);
	return IsValidUnicodeCodePoint(cp);
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
	case 'u':
		return DecodeFixedHexEscape(s, i, 4, cp);
	case 'U':
		return DecodeFixedHexEscape(s, i, 8, cp);
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
		if (out > (numeric_limits<unsigned long long>::max() - static_cast<unsigned long long>(digit)) /
			static_cast<unsigned long long>(base))
		{
			return false;
		}
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
			unsigned_suffix = true;
		else if (suffix == "l" || suffix == "L")
			length_suffix = 1;
		else if (suffix == "ll" || suffix == "LL")
			length_suffix = 2;
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

template<typename T>
bool FitsUnsignedValue(unsigned long long value)
{
	return value <= static_cast<unsigned long long>(numeric_limits<T>::max());
}

ELiteralType ChooseIntegerLiteralType(const IntegerLiteralInfo& info)
{
	vector<ELiteralType> candidates;
	if (info.decimal)
	{
		if (!info.unsigned_suffix && info.length_suffix == 0)
			candidates = {LT_INT, LT_LONG, LT_LONG_LONG};
		else if (!info.unsigned_suffix && info.length_suffix == 1)
			candidates = {LT_LONG, LT_LONG_LONG};
		else if (!info.unsigned_suffix && info.length_suffix == 2)
			candidates = {LT_LONG_LONG};
		else if (info.unsigned_suffix && info.length_suffix == 0)
			candidates = {LT_UNSIGNED_INT, LT_UNSIGNED_LONG, LT_UNSIGNED_LONG_LONG};
		else if (info.unsigned_suffix && info.length_suffix == 1)
			candidates = {LT_UNSIGNED_LONG, LT_UNSIGNED_LONG_LONG};
		else
			candidates = {LT_UNSIGNED_LONG_LONG};
	}
	else
	{
		if (!info.unsigned_suffix && info.length_suffix == 0)
			candidates = {LT_INT, LT_UNSIGNED_INT, LT_LONG, LT_UNSIGNED_LONG, LT_LONG_LONG, LT_UNSIGNED_LONG_LONG};
		else if (!info.unsigned_suffix && info.length_suffix == 1)
			candidates = {LT_LONG, LT_UNSIGNED_LONG, LT_LONG_LONG, LT_UNSIGNED_LONG_LONG};
		else if (!info.unsigned_suffix && info.length_suffix == 2)
			candidates = {LT_LONG_LONG, LT_UNSIGNED_LONG_LONG};
		else if (info.unsigned_suffix && info.length_suffix == 0)
			candidates = {LT_UNSIGNED_INT, LT_UNSIGNED_LONG, LT_UNSIGNED_LONG_LONG};
		else if (info.unsigned_suffix && info.length_suffix == 1)
			candidates = {LT_UNSIGNED_LONG, LT_UNSIGNED_LONG_LONG};
		else
			candidates = {LT_UNSIGNED_LONG_LONG};
	}

	for (ELiteralType type : candidates)
	{
		switch (type)
		{
		case LT_INT:
			if (FitsUnsignedValue<int>(info.value))
				return type;
			break;
		case LT_LONG:
			if (FitsUnsignedValue<long int>(info.value))
				return type;
			break;
		case LT_LONG_LONG:
			if (FitsUnsignedValue<long long int>(info.value))
				return type;
			break;
		case LT_UNSIGNED_INT:
			if (FitsUnsignedValue<unsigned int>(info.value))
				return type;
			break;
		case LT_UNSIGNED_LONG:
			if (FitsUnsignedValue<unsigned long int>(info.value))
				return type;
			break;
		case LT_UNSIGNED_LONG_LONG:
			if (FitsUnsignedValue<unsigned long long int>(info.value))
				return type;
			break;
		default:
			break;
		}
	}

	return LT_CHAR;
}

bool ParseCharacterLiteralToken(const string& source, CharacterLiteralInfo& out)
{
	size_t pos = 0;
	string prefix;
	if (source.compare(0, 1, "u") == 0)
	{
		prefix = "u";
		pos = 1;
	}
	else if (source.compare(0, 1, "U") == 0)
	{
		prefix = "U";
		pos = 1;
	}
	else if (source.compare(0, 1, "L") == 0)
	{
		prefix = "L";
		pos = 1;
	}

	if (pos >= source.size() || source[pos] != '\'')
		return false;
	++pos;

	vector<int> cps;
	while (pos < source.size())
	{
		if (source[pos] == '\'')
		{
			++pos;
			break;
		}

		int cp = 0;
		if (source[pos] == '\\')
		{
			if (!DecodeEscapeSequence(source, pos, cp))
				return false;
		}
		else if (!DecodeUtf8One(source, pos, cp))
		{
			return false;
		}

		if (!IsValidUnicodeCodePoint(cp))
			return false;
		cps.push_back(cp);
	}

	if (pos != source.size() || cps.size() != 1)
		return false;

	int cp = cps[0];
	if (prefix.empty())
		out.type = cp <= 127 ? LT_CHAR : LT_INT;
	else if (prefix == "u")
	{
		if (cp > 0xFFFF)
			return false;
		out.type = LT_CHAR16;
	}
	else if (prefix == "U")
		out.type = LT_CHAR32;
	else
		out.type = LT_WCHAR;

	out.ok = true;
	out.value = static_cast<uint32_t>(cp);
	return true;
}

Value MakeSignedValue(int64_t value)
{
	Value out;
	out.is_unsigned = false;
	out.bits = static_cast<uint64_t>(value);
	return out;
}

Value MakeUnsignedValue(uint64_t value)
{
	Value out;
	out.is_unsigned = true;
	out.bits = value;
	return out;
}

int64_t AsSigned(const Value& value)
{
	return static_cast<int64_t>(value.bits);
}

uint64_t AsUnsigned(const Value& value)
{
	return value.bits;
}

Value ZeroValue(bool is_unsigned)
{
	return is_unsigned ? MakeUnsignedValue(0) : MakeSignedValue(0);
}

bool IsSignedLiteralType(ELiteralType type)
{
	switch (type)
	{
	case LT_UNSIGNED_INT:
	case LT_UNSIGNED_LONG:
	case LT_UNSIGNED_LONG_LONG:
	case LT_CHAR16:
	case LT_CHAR32:
		return false;
	default:
		return true;
	}
}

Value PromoteLiteralValue(ELiteralType type, unsigned long long raw_value)
{
	switch (type)
	{
	case LT_INT:
		return MakeSignedValue(static_cast<intmax_t>(static_cast<int>(raw_value)));
	case LT_LONG:
		return MakeSignedValue(static_cast<intmax_t>(static_cast<long int>(raw_value)));
	case LT_LONG_LONG:
		return MakeSignedValue(static_cast<intmax_t>(static_cast<long long int>(raw_value)));
	case LT_UNSIGNED_INT:
		return MakeUnsignedValue(static_cast<uintmax_t>(static_cast<unsigned int>(raw_value)));
	case LT_UNSIGNED_LONG:
		return MakeUnsignedValue(static_cast<uintmax_t>(static_cast<unsigned long int>(raw_value)));
	case LT_UNSIGNED_LONG_LONG:
		return MakeUnsignedValue(static_cast<uintmax_t>(static_cast<unsigned long long int>(raw_value)));
	case LT_CHAR:
		return MakeSignedValue(static_cast<intmax_t>(static_cast<char>(raw_value)));
	case LT_WCHAR:
		return MakeSignedValue(static_cast<intmax_t>(static_cast<wchar_t>(raw_value)));
	case LT_CHAR16:
		return MakeUnsignedValue(static_cast<uintmax_t>(static_cast<char16_t>(raw_value)));
	case LT_CHAR32:
		return MakeUnsignedValue(static_cast<uintmax_t>(static_cast<char32_t>(raw_value)));
	}

	return MakeSignedValue(0);
}

bool ConvertPPTokenToToken(const PPToken& input, Token& out)
{
	static const unordered_map<string, ETokenKind> punctuator_map =
	{
		{"(", TK_LPAREN},
		{")", TK_RPAREN},
		{"?", TK_QMARK},
		{":", TK_COLON},
		{"+", TK_PLUS},
		{"-", TK_MINUS},
		{"!", TK_LNOT},
		{"~", TK_COMPL},
		{"*", TK_STAR},
		{"/", TK_DIV},
		{"%", TK_MOD},
		{"<<", TK_LSHIFT},
		{">>", TK_RSHIFT},
		{"<", TK_LT},
		{">", TK_GT},
		{"<=", TK_LE},
		{">=", TK_GE},
		{"==", TK_EQ},
		{"!=", TK_NE},
		{"&", TK_AMP},
		{"^", TK_XOR},
		{"|", TK_BOR},
		{"&&", TK_LAND},
		{"||", TK_LOR},
		{"not", TK_LNOT},
		{"compl", TK_COMPL},
		{"bitand", TK_AMP},
		{"xor", TK_XOR},
		{"bitor", TK_BOR},
		{"and", TK_LAND},
		{"or", TK_LOR}
	};

	switch (input.type)
	{
	case PPT_IDENTIFIER:
		out.kind = TK_IDENTIFIER;
		out.text = input.data;
		return true;

	case PPT_PP_NUMBER:
	{
		IntegerLiteralInfo info;
		if (!ParseIntegerLiteralInfo(input.data, info))
			return false;
		ELiteralType type = ChooseIntegerLiteralType(info);
		if (!info.ok)
			return false;
		out.kind = TK_LITERAL;
		out.value = PromoteLiteralValue(type, info.value);
		return type != LT_CHAR;
	}

	case PPT_CHARACTER_LITERAL:
	{
		CharacterLiteralInfo info;
		if (!ParseCharacterLiteralToken(input.data, info) || !info.ok)
			return false;
		out.kind = TK_LITERAL;
		out.value = PromoteLiteralValue(info.type, info.value);
		return true;
	}

	case PPT_PREPROCESSING_OP_OR_PUNC:
	{
		auto it = punctuator_map.find(input.data);
		if (it == punctuator_map.end())
			return false;
		out.kind = it->second;
		out.text = input.data;
		return true;
	}

	default:
		return false;
	}
}

bool TokenizeLine(const vector<PPToken>& line, vector<Token>& out)
{
	out.clear();
	for (const PPToken& token : line)
	{
		if (token.type == PPT_WHITESPACE_SEQUENCE || token.type == PPT_NEW_LINE)
			continue;

		Token converted;
		if (!ConvertPPTokenToToken(token, converted))
			return false;
		out.push_back(converted);
	}

	Token end;
	end.kind = TK_END;
	out.push_back(end);
	return true;
}

bool IsAltIdentifier(const Token& token, const string& text)
{
	return token.kind == TK_IDENTIFIER && token.text == text;
}

class Parser
{
public:
	explicit Parser(const vector<Token>& tokens)
		: tokens_(tokens)
	{
	}

	bool ok() const
	{
		return ok_;
	}

	bool at_end() const
	{
		return Peek().kind == TK_END;
	}

	Value Parse(bool eval)
	{
		return ParseConditional(eval);
	}

private:
	const Token& Peek() const
	{
		return tokens_[pos_];
	}

	const Token& Previous() const
	{
		return tokens_[pos_ - 1];
	}

	bool Consume(ETokenKind kind)
	{
		if (Peek().kind != kind)
			return false;
		++pos_;
		return true;
	}

	bool ConsumeIdentifier(const string& text)
	{
		if (!IsAltIdentifier(Peek(), text))
			return false;
		++pos_;
		return true;
	}

	Value ErrorValue()
	{
		ok_ = false;
		return MakeSignedValue(0);
	}

	bool IsTrueValue(const Value& value) const
	{
		return value.bits != 0;
	}

	bool CommonUnsigned(const Value& lhs, const Value& rhs) const
	{
		return lhs.is_unsigned || rhs.is_unsigned;
	}

	Value CastValue(const Value& value, bool to_unsigned) const
	{
		return to_unsigned ? MakeUnsignedValue(value.bits) : MakeSignedValue(static_cast<int64_t>(value.bits));
	}

	Value DummyValue(bool is_unsigned) const
	{
		return ZeroValue(is_unsigned);
	}

	Value ParsePrimary(bool eval)
	{
		if (Peek().kind == TK_LITERAL)
		{
			Value out = Peek().value;
			++pos_;
			return out;
		}

		if (Consume(TK_LPAREN))
		{
			Value out = ParseConditional(eval);
			if (!Consume(TK_RPAREN))
				return ErrorValue();
			return out;
		}

		if (Peek().kind == TK_IDENTIFIER)
		{
			string name = Peek().text;
			++pos_;

			if (name == "defined")
			{
				string identifier;
				if (Peek().kind == TK_IDENTIFIER)
				{
					identifier = Peek().text;
					++pos_;
				}
				else if (Consume(TK_LPAREN))
				{
					if (Peek().kind != TK_IDENTIFIER)
						return ErrorValue();
					identifier = Peek().text;
					++pos_;
					if (!Consume(TK_RPAREN))
						return ErrorValue();
				}
				else
				{
					return ErrorValue();
				}

				if (!eval)
					return MakeSignedValue(0);
				return MakeSignedValue(PA3Mock_IsDefinedIdentifier(identifier) ? 1 : 0);
			}

			if (!eval)
				return MakeSignedValue(0);
			if (name == "true")
				return MakeSignedValue(1);
			if (name == "false")
				return MakeSignedValue(0);
			return MakeSignedValue(0);
		}

		return ErrorValue();
	}

	Value ParseUnary(bool eval)
	{
		if (Consume(TK_PLUS))
		{
			Value value = ParseUnary(eval);
			if (!ok_)
				return value;
			return eval ? value : DummyValue(value.is_unsigned);
		}

		if (Consume(TK_MINUS))
		{
			Value value = ParseUnary(eval);
			if (!ok_)
				return value;
			if (!eval)
				return DummyValue(value.is_unsigned);
			if (value.is_unsigned)
				return MakeUnsignedValue(uint64_t(0) - value.bits);
			return MakeSignedValue(static_cast<int64_t>(uint64_t(0) - value.bits));
		}

		if (Consume(TK_LNOT) || ConsumeIdentifier("not"))
		{
			Value value = ParseUnary(eval);
			if (!ok_)
				return value;
			return eval ? MakeSignedValue(IsTrueValue(value) ? 0 : 1) : MakeSignedValue(0);
		}

		if (Consume(TK_COMPL) || ConsumeIdentifier("compl"))
		{
			Value value = ParseUnary(eval);
			if (!ok_)
				return value;
			if (!eval)
				return DummyValue(value.is_unsigned);
			return value.is_unsigned ? MakeUnsignedValue(~value.bits) : MakeSignedValue(static_cast<int64_t>(~value.bits));
		}

		return ParsePrimary(eval);
	}

	Value ParseMultiplicative(bool eval)
	{
		Value lhs = ParseUnary(eval);
		while (ok_)
		{
			ETokenKind op = TK_END;
			if (Consume(TK_STAR))
				op = TK_STAR;
			else if (Consume(TK_DIV))
				op = TK_DIV;
			else if (Consume(TK_MOD))
				op = TK_MOD;
			else
				break;

			Value rhs = ParseUnary(eval);
			if (!ok_)
				return rhs;

			bool result_unsigned = CommonUnsigned(lhs, rhs);
			if (!eval)
			{
				lhs = DummyValue(result_unsigned);
				continue;
			}

			if (op == TK_STAR)
			{
				lhs = result_unsigned
					? MakeUnsignedValue(lhs.bits * rhs.bits)
					: MakeSignedValue(static_cast<int64_t>(lhs.bits * rhs.bits));
				continue;
			}

			if (rhs.bits == 0)
				return ErrorValue();

			if (result_unsigned)
			{
				uint64_t left = lhs.bits;
				uint64_t right = rhs.bits;
				lhs = (op == TK_DIV) ? MakeUnsignedValue(left / right) : MakeUnsignedValue(left % right);
				continue;
			}

			int64_t left = AsSigned(lhs);
			int64_t right = AsSigned(rhs);
			if (right == -1 && left == numeric_limits<int64_t>::min())
				return ErrorValue();
			lhs = (op == TK_DIV) ? MakeSignedValue(left / right) : MakeSignedValue(left % right);
		}

		return lhs;
	}

	Value ParseAdditive(bool eval)
	{
		Value lhs = ParseMultiplicative(eval);
		while (ok_)
		{
			ETokenKind op = TK_END;
			if (Consume(TK_PLUS))
				op = TK_PLUS;
			else if (Consume(TK_MINUS))
				op = TK_MINUS;
			else
				break;

			Value rhs = ParseMultiplicative(eval);
			if (!ok_)
				return rhs;

			bool result_unsigned = CommonUnsigned(lhs, rhs);
			if (!eval)
			{
				lhs = DummyValue(result_unsigned);
				continue;
			}

			if (op == TK_PLUS)
				lhs = result_unsigned ? MakeUnsignedValue(lhs.bits + rhs.bits) : MakeSignedValue(static_cast<int64_t>(lhs.bits + rhs.bits));
			else
				lhs = result_unsigned ? MakeUnsignedValue(lhs.bits - rhs.bits) : MakeSignedValue(static_cast<int64_t>(lhs.bits - rhs.bits));
		}

		return lhs;
	}

	Value ParseShift(bool eval)
	{
		Value lhs = ParseAdditive(eval);
		while (ok_)
		{
			ETokenKind op = TK_END;
			if (Consume(TK_LSHIFT))
				op = TK_LSHIFT;
			else if (Consume(TK_RSHIFT))
				op = TK_RSHIFT;
			else
				break;

			Value rhs = ParseAdditive(eval);
			if (!ok_)
				return rhs;

			if (!eval)
			{
				lhs = DummyValue(lhs.is_unsigned);
				continue;
			}

			int64_t shift = rhs.is_unsigned ? static_cast<int64_t>(rhs.bits) : AsSigned(rhs);
			if (shift < 0 || shift >= 64)
				return ErrorValue();

			if (op == TK_LSHIFT)
			{
				lhs = lhs.is_unsigned
					? MakeUnsignedValue(lhs.bits << shift)
					: MakeSignedValue(static_cast<int64_t>(lhs.bits << shift));
			}
			else if (lhs.is_unsigned)
			{
				lhs = MakeUnsignedValue(lhs.bits >> shift);
			}
			else
			{
				lhs = MakeSignedValue(AsSigned(lhs) >> shift);
			}
		}

		return lhs;
	}

	Value ParseRelational(bool eval)
	{
		Value lhs = ParseShift(eval);
		while (ok_)
		{
			ETokenKind op = TK_END;
			if (Consume(TK_LT))
				op = TK_LT;
			else if (Consume(TK_GT))
				op = TK_GT;
			else if (Consume(TK_LE))
				op = TK_LE;
			else if (Consume(TK_GE))
				op = TK_GE;
			else
				break;

			Value rhs = ParseShift(eval);
			if (!ok_)
				return rhs;

			if (!eval)
			{
				lhs = MakeSignedValue(0);
				continue;
			}

			bool result = false;
			if (CommonUnsigned(lhs, rhs))
			{
				uint64_t left = lhs.bits;
				uint64_t right = rhs.bits;
				if (op == TK_LT)
					result = left < right;
				else if (op == TK_GT)
					result = left > right;
				else if (op == TK_LE)
					result = left <= right;
				else
					result = left >= right;
			}
			else
			{
				int64_t left = AsSigned(lhs);
				int64_t right = AsSigned(rhs);
				if (op == TK_LT)
					result = left < right;
				else if (op == TK_GT)
					result = left > right;
				else if (op == TK_LE)
					result = left <= right;
				else
					result = left >= right;
			}

			lhs = MakeSignedValue(result ? 1 : 0);
		}

		return lhs;
	}

	Value ParseEquality(bool eval)
	{
		Value lhs = ParseRelational(eval);
		while (ok_)
		{
			ETokenKind op = TK_END;
			if (Consume(TK_EQ))
				op = TK_EQ;
			else if (Consume(TK_NE))
				op = TK_NE;
			else
				break;

			Value rhs = ParseRelational(eval);
			if (!ok_)
				return rhs;

			if (!eval)
			{
				lhs = MakeSignedValue(0);
				continue;
			}

			bool result = lhs.bits == rhs.bits;
			if (op == TK_NE)
				result = !result;
			lhs = MakeSignedValue(result ? 1 : 0);
		}

		return lhs;
	}

	Value ParseBitAnd(bool eval)
	{
		Value lhs = ParseEquality(eval);
		while (ok_)
		{
			if (!(Consume(TK_AMP) || ConsumeIdentifier("bitand")))
				break;

			Value rhs = ParseEquality(eval);
			if (!ok_)
				return rhs;

			bool result_unsigned = CommonUnsigned(lhs, rhs);
			if (!eval)
			{
				lhs = DummyValue(result_unsigned);
				continue;
			}

			lhs = result_unsigned
				? MakeUnsignedValue(lhs.bits & rhs.bits)
				: MakeSignedValue(static_cast<int64_t>(lhs.bits & rhs.bits));
		}

		return lhs;
	}

	Value ParseBitXor(bool eval)
	{
		Value lhs = ParseBitAnd(eval);
		while (ok_)
		{
			if (!(Consume(TK_XOR) || ConsumeIdentifier("xor")))
				break;

			Value rhs = ParseBitAnd(eval);
			if (!ok_)
				return rhs;

			bool result_unsigned = CommonUnsigned(lhs, rhs);
			if (!eval)
			{
				lhs = DummyValue(result_unsigned);
				continue;
			}

			lhs = result_unsigned
				? MakeUnsignedValue(lhs.bits ^ rhs.bits)
				: MakeSignedValue(static_cast<int64_t>(lhs.bits ^ rhs.bits));
		}

		return lhs;
	}

	Value ParseBitOr(bool eval)
	{
		Value lhs = ParseBitXor(eval);
		while (ok_)
		{
			if (!(Consume(TK_BOR) || ConsumeIdentifier("bitor")))
				break;

			Value rhs = ParseBitXor(eval);
			if (!ok_)
				return rhs;

			bool result_unsigned = CommonUnsigned(lhs, rhs);
			if (!eval)
			{
				lhs = DummyValue(result_unsigned);
				continue;
			}

			lhs = result_unsigned
				? MakeUnsignedValue(lhs.bits | rhs.bits)
				: MakeSignedValue(static_cast<int64_t>(lhs.bits | rhs.bits));
		}

		return lhs;
	}

	Value ParseLogicalAnd(bool eval)
	{
		Value lhs = ParseBitOr(eval);
		while (ok_)
		{
			if (!(Consume(TK_LAND) || ConsumeIdentifier("and")))
				break;

			bool lhs_true = eval && IsTrueValue(lhs);
			Value rhs = ParseBitOr(eval && lhs_true);
			if (!ok_)
				return rhs;

			if (!eval)
			{
				lhs = MakeSignedValue(0);
				continue;
			}

			lhs = MakeSignedValue((lhs_true && IsTrueValue(rhs)) ? 1 : 0);
		}

		return lhs;
	}

	Value ParseLogicalOr(bool eval)
	{
		Value lhs = ParseLogicalAnd(eval);
		while (ok_)
		{
			if (!(Consume(TK_LOR) || ConsumeIdentifier("or")))
				break;

			bool lhs_true = eval && IsTrueValue(lhs);
			Value rhs = ParseLogicalAnd(eval && !lhs_true);
			if (!ok_)
				return rhs;

			if (!eval)
			{
				lhs = MakeSignedValue(0);
				continue;
			}

			lhs = MakeSignedValue((lhs_true || IsTrueValue(rhs)) ? 1 : 0);
		}

		return lhs;
	}

	Value ParseConditional(bool eval)
	{
		Value condition = ParseLogicalOr(eval);
		if (!ok_ || !Consume(TK_QMARK))
			return condition;

		bool cond_true = eval && IsTrueValue(condition);
		Value if_true = ParseConditional(eval && cond_true);
		if (!Consume(TK_COLON))
			return ErrorValue();
		Value if_false = ParseConditional(eval && !cond_true);
		if (!ok_)
			return if_false;

		bool result_unsigned = CommonUnsigned(if_true, if_false);
		if (!eval)
			return DummyValue(result_unsigned);

		Value chosen = cond_true ? if_true : if_false;
		return CastValue(chosen, result_unsigned);
	}

	const vector<Token>& tokens_;
	size_t pos_ = 0;
	bool ok_ = true;
};

string FormatValue(const Value& value)
{
	if (value.is_unsigned)
		return to_string(static_cast<unsigned long long>(value.bits)) + "u";
	return to_string(static_cast<long long>(AsSigned(value)));
}

bool EvaluateLine(const vector<PPToken>& line, string& output)
{
	vector<Token> tokens;
	if (!TokenizeLine(line, tokens))
	{
		output = "error";
		return true;
	}

	if (tokens.size() == 1 && tokens[0].kind == TK_END)
	{
		output.clear();
		return true;
	}

	Parser parser(tokens);
	Value value = parser.Parse(true);
	if (!parser.ok() || !parser.at_end())
	{
		output = "error";
		return true;
	}

	output = FormatValue(value);
	return true;
}

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		vector<PPToken> tokens = LexPPTokens(oss.str());
		vector<PPToken> line;

		for (const PPToken& token : tokens)
		{
			if (token.type == PPT_NEW_LINE || token.type == PPT_EOF)
			{
				string result;
				if (!line.empty())
				{
					EvaluateLine(line, result);
					if (!result.empty())
						cout << result << endl;
				}
				line.clear();

				if (token.type == PPT_EOF)
				{
					cout << "eof" << endl;
					return EXIT_SUCCESS;
				}
				continue;
			}

			if (token.type != PPT_WHITESPACE_SEQUENCE)
				line.push_back(token);
		}
	}
	catch (const exception&)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
