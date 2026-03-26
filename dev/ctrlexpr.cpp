// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Reuse PA1 preprocessing-tokenizer implementation.
#define main pa1_pptoken_main
#include "pptoken.cpp"
#undef main

// mock implementation of IsDefinedIdentifier for PA3
// return true iff first code point is odd
bool PA3Mock_IsDefinedIdentifier(const string& identifier)
{
	if (identifier.empty())
		return false;
	else
		return identifier[0] % 2;
}

enum EFundamentalType
{
	FT_SIGNED_CHAR,
	FT_SHORT_INT,
	FT_INT,
	FT_LONG_INT,
	FT_LONG_LONG_INT,
	FT_UNSIGNED_CHAR,
	FT_UNSIGNED_SHORT_INT,
	FT_UNSIGNED_INT,
	FT_UNSIGNED_LONG_INT,
	FT_UNSIGNED_LONG_LONG_INT,
	FT_WCHAR_T,
	FT_CHAR,
	FT_CHAR16_T,
	FT_CHAR32_T,
	FT_BOOL,
	FT_FLOAT,
	FT_DOUBLE,
	FT_LONG_DOUBLE,
	FT_VOID,
	FT_NULLPTR_T
};

namespace pa3
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

	void emit_whitespace_sequence() override { tokens.push_back({PPTokenType::WhitespaceSequence, ""}); }
	void emit_new_line() override { tokens.push_back({PPTokenType::NewLine, ""}); }
	void emit_header_name(const string& data) override { tokens.push_back({PPTokenType::HeaderName, data}); }
	void emit_identifier(const string& data) override { tokens.push_back({PPTokenType::Identifier, data}); }
	void emit_pp_number(const string& data) override { tokens.push_back({PPTokenType::PPNumber, data}); }
	void emit_character_literal(const string& data) override { tokens.push_back({PPTokenType::CharacterLiteral, data}); }
	void emit_user_defined_character_literal(const string& data) override { tokens.push_back({PPTokenType::UserDefinedCharacterLiteral, data}); }
	void emit_string_literal(const string& data) override { tokens.push_back({PPTokenType::StringLiteral, data}); }
	void emit_user_defined_string_literal(const string& data) override { tokens.push_back({PPTokenType::UserDefinedStringLiteral, data}); }
	void emit_preprocessing_op_or_punc(const string& data) override { tokens.push_back({PPTokenType::PreprocessingOpOrPunc, data}); }
	void emit_non_whitespace_char(const string& data) override { tokens.push_back({PPTokenType::NonWhitespaceCharacter, data}); }
	void emit_eof() override {}
};

bool IsIdentifierStartAscii(char c)
{
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsIdentifierContinueAscii(char c)
{
	return IsIdentifierStartAscii(c) || (c >= '0' && c <= '9');
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

struct ParsedCharacterLiteral
{
	bool ok;
	EFundamentalType type;
	uint32_t value;
};

bool ParseCharacterLiteral(const string& source, ParsedCharacterLiteral& out)
{
	out.ok = false;
	out.type = FT_INT;
	out.value = 0;

	vector<uint32_t> cps;
	if (!DecodeUtf8(source, cps))
		return false;

	size_t i = 0;
	int prefix = 0;
	if (i + 1 < cps.size() && (cps[i] == 'u' || cps[i] == 'U' || cps[i] == 'L') && cps[i + 1] == '\'')
	{
		prefix = cps[i];
		++i;
	}

	if (i >= cps.size() || cps[i] != '\'')
		return false;
	++i;

	vector<uint32_t> chars;
	while (i < cps.size() && cps[i] != '\'')
	{
		if (cps[i] == '\\')
		{
			uint32_t cp = 0;
			if (!DecodeEscape(cps, i, cp))
				return false;
			chars.push_back(cp);
		}
		else
		{
			if (cps[i] == '\n')
				return false;
			chars.push_back(cps[i]);
			++i;
		}
	}

	if (i >= cps.size() || cps[i] != '\'')
		return false;
	++i;

	if (i != cps.size())
		return false;

	if (chars.size() != 1)
		return false;

	uint32_t cp = chars[0];
	if (!IsValidCodePoint(cp))
		return false;

	out.value = cp;
	if (prefix == 0)
		out.type = cp <= 127 ? FT_CHAR : FT_INT;
	else if (prefix == 'u')
	{
		if (cp > 0xFFFF)
			return false;
		out.type = FT_CHAR16_T;
	}
	else if (prefix == 'U')
		out.type = FT_CHAR32_T;
	else
		out.type = FT_WCHAR_T;

	out.ok = true;
	return true;
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

bool IsPromotedSigned(EFundamentalType t)
{
	switch (t)
	{
	case FT_SIGNED_CHAR:
	case FT_SHORT_INT:
	case FT_INT:
	case FT_LONG_INT:
	case FT_LONG_LONG_INT:
	case FT_WCHAR_T:
	case FT_CHAR:
	case FT_BOOL:
		return true;

	case FT_UNSIGNED_CHAR:
	case FT_UNSIGNED_SHORT_INT:
	case FT_UNSIGNED_INT:
	case FT_UNSIGNED_LONG_INT:
	case FT_UNSIGNED_LONG_LONG_INT:
	case FT_CHAR16_T:
	case FT_CHAR32_T:
		return false;

	default:
		return false;
	}
}

enum class ValueType
{
	Signed,
	Unsigned
};

struct EvalValue
{
	ValueType type;
	uint64_t bits;
};

EvalValue MakeSigned(int64_t v)
{
	EvalValue out;
	out.type = ValueType::Signed;
	out.bits = static_cast<uint64_t>(v);
	return out;
}

EvalValue MakeUnsigned(uint64_t v)
{
	EvalValue out;
	out.type = ValueType::Unsigned;
	out.bits = v;
	return out;
}

bool IsTrue(const EvalValue& v)
{
	return v.bits != 0;
}

ValueType UsualArithmetic(ValueType a, ValueType b)
{
	return (a == ValueType::Unsigned || b == ValueType::Unsigned) ? ValueType::Unsigned : ValueType::Signed;
}

EvalValue ConvertToType(const EvalValue& v, ValueType to)
{
	if (v.type == to)
		return v;

	EvalValue out = v;
	out.type = to;
	return out;
}

enum class TokenKind
{
	Literal,
	Identifier,
	KwDefined,
	KwTrue,
	KwFalse,
	UnsupportedIdentifierOp,
	OpLParen,
	OpRParen,
	OpPlus,
	OpMinus,
	OpLNot,
	OpCompl,
	OpStar,
	OpDiv,
	OpMod,
	OpLShift,
	OpRShift,
	OpLT,
	OpGT,
	OpLE,
	OpGE,
	OpEQ,
	OpNE,
	OpAmp,
	OpXor,
	OpBor,
	OpLand,
	OpLor,
	OpQMark,
	OpColon
};

struct Token
{
	TokenKind kind;
	bool from_identifier;
	string source;
	EvalValue value;
};

bool ConvertPPToken(const PPToken& in, Token& out)
{
	out.from_identifier = false;
	out.source.clear();
	out.value = MakeSigned(0);

	if (in.type == PPTokenType::Identifier)
	{
		out.from_identifier = true;
		out.source = in.source;

		if (in.source == "defined") { out.kind = TokenKind::KwDefined; return true; }
		if (in.source == "true") { out.kind = TokenKind::KwTrue; return true; }
		if (in.source == "false") { out.kind = TokenKind::KwFalse; return true; }
		if (in.source == "and") { out.kind = TokenKind::OpLand; return true; }
		if (in.source == "or") { out.kind = TokenKind::OpLor; return true; }
		if (in.source == "not") { out.kind = TokenKind::OpLNot; return true; }
		if (in.source == "bitand") { out.kind = TokenKind::OpAmp; return true; }
		if (in.source == "bitor") { out.kind = TokenKind::OpBor; return true; }
		if (in.source == "xor") { out.kind = TokenKind::OpXor; return true; }
		if (in.source == "compl") { out.kind = TokenKind::OpCompl; return true; }
		if (in.source == "not_eq") { out.kind = TokenKind::OpNE; return true; }
		if (in.source == "and_eq" || in.source == "or_eq" || in.source == "xor_eq")
		{
			out.kind = TokenKind::UnsupportedIdentifierOp;
			return true;
		}

		out.kind = TokenKind::Identifier;
		return true;
	}

	if (in.type == PPTokenType::PPNumber)
	{
		ParsedBuiltinInteger lit = ParseBuiltinInteger(in.source);
		if (!lit.ok)
			return false;

		out.kind = TokenKind::Literal;
		if (IsPromotedSigned(lit.type))
			out.value = MakeSigned(static_cast<int64_t>(lit.value));
		else
			out.value = MakeUnsigned(static_cast<uint64_t>(lit.value));
		return true;
	}

	if (in.type == PPTokenType::CharacterLiteral)
	{
		ParsedCharacterLiteral lit;
		if (!ParseCharacterLiteral(in.source, lit) || !lit.ok)
			return false;

		out.kind = TokenKind::Literal;
		if (IsPromotedSigned(lit.type))
			out.value = MakeSigned(static_cast<int64_t>(lit.value));
		else
			out.value = MakeUnsigned(static_cast<uint64_t>(lit.value));
		return true;
	}

	if (in.type == PPTokenType::PreprocessingOpOrPunc)
	{
		static const unordered_map<string, TokenKind> map =
		{
			{"(", TokenKind::OpLParen},
			{")", TokenKind::OpRParen},
			{"+", TokenKind::OpPlus},
			{"-", TokenKind::OpMinus},
			{"!", TokenKind::OpLNot},
			{"~", TokenKind::OpCompl},
			{"*", TokenKind::OpStar},
			{"/", TokenKind::OpDiv},
			{"%", TokenKind::OpMod},
			{"<<", TokenKind::OpLShift},
			{">>", TokenKind::OpRShift},
			{"<", TokenKind::OpLT},
			{">", TokenKind::OpGT},
			{"<=", TokenKind::OpLE},
			{">=", TokenKind::OpGE},
			{"==", TokenKind::OpEQ},
			{"!=", TokenKind::OpNE},
			{"&", TokenKind::OpAmp},
			{"^", TokenKind::OpXor},
			{"|", TokenKind::OpBor},
			{"&&", TokenKind::OpLand},
			{"||", TokenKind::OpLor},
			{"not", TokenKind::OpLNot},
			{"compl", TokenKind::OpCompl},
			{"bitand", TokenKind::OpAmp},
			{"xor", TokenKind::OpXor},
			{"bitor", TokenKind::OpBor},
			{"and", TokenKind::OpLand},
			{"or", TokenKind::OpLor},
			{"not_eq", TokenKind::OpNE},
			{"and_eq", TokenKind::UnsupportedIdentifierOp},
			{"xor_eq", TokenKind::UnsupportedIdentifierOp},
			{"or_eq", TokenKind::UnsupportedIdentifierOp},
			{"?", TokenKind::OpQMark},
			{":", TokenKind::OpColon}
		};

		auto it = map.find(in.source);
		if (it == map.end())
			return false;
		out.kind = it->second;
		return true;
	}

	return false;
}

enum class ExprKind
{
	Literal,
	Defined,
	Unary,
	Binary,
	Conditional
};

struct Expr
{
	ExprKind kind;
	TokenKind op;
	ValueType type;
	EvalValue literal;
	string identifier;
	unique_ptr<Expr> a;
	unique_ptr<Expr> b;
	unique_ptr<Expr> c;
};

struct Parser
{
	const vector<Token>& toks;
	size_t i;

	Parser(const vector<Token>& toks)
		: toks(toks), i(0)
	{}

	bool at_end() const
	{
		return i >= toks.size();
	}

	const Token* peek() const
	{
		return at_end() ? nullptr : &toks[i];
	}

	bool match(TokenKind k)
	{
		if (!at_end() && toks[i].kind == k)
		{
			++i;
			return true;
		}
		return false;
	}

	unique_ptr<Expr> make_literal(EvalValue v)
	{
		unique_ptr<Expr> e(new Expr());
		e->kind = ExprKind::Literal;
		e->op = TokenKind::Literal;
		e->type = v.type;
		e->literal = v;
		return e;
	}

	unique_ptr<Expr> make_defined(const string& identifier)
	{
		unique_ptr<Expr> e(new Expr());
		e->kind = ExprKind::Defined;
		e->op = TokenKind::KwDefined;
		e->type = ValueType::Signed;
		e->identifier = identifier;
		return e;
	}

	unique_ptr<Expr> make_unary(TokenKind op, unique_ptr<Expr> x)
	{
		unique_ptr<Expr> e(new Expr());
		e->kind = ExprKind::Unary;
		e->op = op;
		e->a = move(x);
		e->type = (op == TokenKind::OpLNot) ? ValueType::Signed : e->a->type;
		return e;
	}

	unique_ptr<Expr> make_binary(TokenKind op, unique_ptr<Expr> lhs, unique_ptr<Expr> rhs)
	{
		unique_ptr<Expr> e(new Expr());
		e->kind = ExprKind::Binary;
		e->op = op;
		e->a = move(lhs);
		e->b = move(rhs);

		switch (op)
		{
		case TokenKind::OpStar:
		case TokenKind::OpDiv:
		case TokenKind::OpMod:
		case TokenKind::OpPlus:
		case TokenKind::OpMinus:
		case TokenKind::OpAmp:
		case TokenKind::OpXor:
		case TokenKind::OpBor:
			e->type = UsualArithmetic(e->a->type, e->b->type);
			break;

		case TokenKind::OpLShift:
		case TokenKind::OpRShift:
			e->type = e->a->type;
			break;

		case TokenKind::OpLT:
		case TokenKind::OpGT:
		case TokenKind::OpLE:
		case TokenKind::OpGE:
		case TokenKind::OpEQ:
		case TokenKind::OpNE:
		case TokenKind::OpLand:
		case TokenKind::OpLor:
			e->type = ValueType::Signed;
			break;

		default:
			e->type = ValueType::Signed;
			break;
		}

		return e;
	}

	unique_ptr<Expr> make_conditional(unique_ptr<Expr> cond, unique_ptr<Expr> t, unique_ptr<Expr> f)
	{
		unique_ptr<Expr> e(new Expr());
		e->kind = ExprKind::Conditional;
		e->op = TokenKind::OpQMark;
		e->a = move(cond);
		e->b = move(t);
		e->c = move(f);
		e->type = UsualArithmetic(e->b->type, e->c->type);
		return e;
	}

	bool parse_primary(unique_ptr<Expr>& out)
	{
		if (at_end())
			return false;

		const Token& t = toks[i];

		if (t.kind == TokenKind::Literal)
		{
			++i;
			out = make_literal(t.value);
			return true;
		}

		if (t.kind == TokenKind::KwTrue)
		{
			++i;
			out = make_literal(MakeSigned(1));
			return true;
		}

		if (t.kind == TokenKind::KwFalse)
		{
			++i;
			out = make_literal(MakeSigned(0));
			return true;
		}

		if (t.kind == TokenKind::Identifier)
		{
			++i;
			out = make_literal(MakeSigned(0));
			return true;
		}

		if (t.kind == TokenKind::KwDefined)
		{
			++i;
			if (match(TokenKind::OpLParen))
			{
				if (at_end() || !toks[i].from_identifier)
					return false;
				string id = toks[i].source;
				++i;
				if (!match(TokenKind::OpRParen))
					return false;
				out = make_defined(id);
				return true;
			}
			else
			{
				if (at_end() || !toks[i].from_identifier)
					return false;
				string id = toks[i].source;
				++i;
				out = make_defined(id);
				return true;
			}
		}

		if (match(TokenKind::OpLParen))
		{
			if (!parse_controlling_expression(out))
				return false;
			if (!match(TokenKind::OpRParen))
				return false;
			return true;
		}

		return false;
	}

	bool parse_unary(unique_ptr<Expr>& out)
	{
		if (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k == TokenKind::OpPlus || k == TokenKind::OpMinus || k == TokenKind::OpLNot || k == TokenKind::OpCompl)
			{
				++i;
				unique_ptr<Expr> inner;
				if (!parse_unary(inner))
					return false;
				out = make_unary(k, move(inner));
				return true;
			}
		}

		return parse_primary(out);
	}

	bool parse_multiplicative(unique_ptr<Expr>& out)
	{
		if (!parse_unary(out))
			return false;

		while (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k != TokenKind::OpStar && k != TokenKind::OpDiv && k != TokenKind::OpMod)
				break;
			++i;
			unique_ptr<Expr> rhs;
			if (!parse_unary(rhs))
				return false;
			out = make_binary(k, move(out), move(rhs));
		}

		return true;
	}

	bool parse_additive(unique_ptr<Expr>& out)
	{
		if (!parse_multiplicative(out))
			return false;

		while (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k != TokenKind::OpPlus && k != TokenKind::OpMinus)
				break;
			++i;
			unique_ptr<Expr> rhs;
			if (!parse_multiplicative(rhs))
				return false;
			out = make_binary(k, move(out), move(rhs));
		}

		return true;
	}

	bool parse_shift(unique_ptr<Expr>& out)
	{
		if (!parse_additive(out))
			return false;

		while (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k != TokenKind::OpLShift && k != TokenKind::OpRShift)
				break;
			++i;
			unique_ptr<Expr> rhs;
			if (!parse_additive(rhs))
				return false;
			out = make_binary(k, move(out), move(rhs));
		}

		return true;
	}

	bool parse_relational(unique_ptr<Expr>& out)
	{
		if (!parse_shift(out))
			return false;

		while (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k != TokenKind::OpLT && k != TokenKind::OpGT && k != TokenKind::OpLE && k != TokenKind::OpGE)
				break;
			++i;
			unique_ptr<Expr> rhs;
			if (!parse_shift(rhs))
				return false;
			out = make_binary(k, move(out), move(rhs));
		}

		return true;
	}

	bool parse_equality(unique_ptr<Expr>& out)
	{
		if (!parse_relational(out))
			return false;

		while (!at_end())
		{
			TokenKind k = toks[i].kind;
			if (k != TokenKind::OpEQ && k != TokenKind::OpNE)
				break;
			++i;
			unique_ptr<Expr> rhs;
			if (!parse_relational(rhs))
				return false;
			out = make_binary(k, move(out), move(rhs));
		}

		return true;
	}

	bool parse_and(unique_ptr<Expr>& out)
	{
		if (!parse_equality(out))
			return false;

		while (match(TokenKind::OpAmp))
		{
			unique_ptr<Expr> rhs;
			if (!parse_equality(rhs))
				return false;
			out = make_binary(TokenKind::OpAmp, move(out), move(rhs));
		}

		return true;
	}

	bool parse_xor(unique_ptr<Expr>& out)
	{
		if (!parse_and(out))
			return false;

		while (match(TokenKind::OpXor))
		{
			unique_ptr<Expr> rhs;
			if (!parse_and(rhs))
				return false;
			out = make_binary(TokenKind::OpXor, move(out), move(rhs));
		}

		return true;
	}

	bool parse_or(unique_ptr<Expr>& out)
	{
		if (!parse_xor(out))
			return false;

		while (match(TokenKind::OpBor))
		{
			unique_ptr<Expr> rhs;
			if (!parse_xor(rhs))
				return false;
			out = make_binary(TokenKind::OpBor, move(out), move(rhs));
		}

		return true;
	}

	bool parse_logical_and(unique_ptr<Expr>& out)
	{
		if (!parse_or(out))
			return false;

		while (match(TokenKind::OpLand))
		{
			unique_ptr<Expr> rhs;
			if (!parse_or(rhs))
				return false;
			out = make_binary(TokenKind::OpLand, move(out), move(rhs));
		}

		return true;
	}

	bool parse_logical_or(unique_ptr<Expr>& out)
	{
		if (!parse_logical_and(out))
			return false;

		while (match(TokenKind::OpLor))
		{
			unique_ptr<Expr> rhs;
			if (!parse_logical_and(rhs))
				return false;
			out = make_binary(TokenKind::OpLor, move(out), move(rhs));
		}

		return true;
	}

	bool parse_controlling_expression(unique_ptr<Expr>& out)
	{
		if (!parse_logical_or(out))
			return false;

		if (match(TokenKind::OpQMark))
		{
			unique_ptr<Expr> t;
			unique_ptr<Expr> f;
			if (!parse_controlling_expression(t))
				return false;
			if (!match(TokenKind::OpColon))
				return false;
			if (!parse_controlling_expression(f))
				return false;
			out = make_conditional(move(out), move(t), move(f));
		}

		return true;
	}

	bool parse(unique_ptr<Expr>& out)
	{
		if (!parse_controlling_expression(out))
			return false;
		return i == toks.size();
	}
};

bool CompareLess(const EvalValue& lhs, const EvalValue& rhs)
{
	ValueType t = UsualArithmetic(lhs.type, rhs.type);
	EvalValue a = ConvertToType(lhs, t);
	EvalValue b = ConvertToType(rhs, t);

	if (t == ValueType::Unsigned)
		return a.bits < b.bits;
	return static_cast<int64_t>(a.bits) < static_cast<int64_t>(b.bits);
}

bool CompareEqual(const EvalValue& lhs, const EvalValue& rhs)
{
	ValueType t = UsualArithmetic(lhs.type, rhs.type);
	EvalValue a = ConvertToType(lhs, t);
	EvalValue b = ConvertToType(rhs, t);
	return a.bits == b.bits;
}

bool EvalExpr(const Expr* e, EvalValue& out)
{
	if (e->kind == ExprKind::Literal)
	{
		out = e->literal;
		return true;
	}

	if (e->kind == ExprKind::Defined)
	{
		out = MakeSigned(PA3Mock_IsDefinedIdentifier(e->identifier) ? 1 : 0);
		return true;
	}

	if (e->kind == ExprKind::Unary)
	{
		EvalValue x;
		if (!EvalExpr(e->a.get(), x))
			return false;

		switch (e->op)
		{
		case TokenKind::OpPlus:
			out = x;
			return true;

		case TokenKind::OpMinus:
			out = x;
			out.bits = uint64_t(0) - x.bits;
			return true;

		case TokenKind::OpLNot:
			out = MakeSigned(IsTrue(x) ? 0 : 1);
			return true;

		case TokenKind::OpCompl:
			out = x;
			out.bits = ~x.bits;
			return true;

		default:
			return false;
		}
	}

	if (e->kind == ExprKind::Conditional)
	{
		EvalValue cond;
		if (!EvalExpr(e->a.get(), cond))
			return false;

		EvalValue branch;
		if (IsTrue(cond))
		{
			if (!EvalExpr(e->b.get(), branch))
				return false;
		}
		else
		{
			if (!EvalExpr(e->c.get(), branch))
				return false;
		}

		out = ConvertToType(branch, e->type);
		return true;
	}

	if (e->kind != ExprKind::Binary)
		return false;

	if (e->op == TokenKind::OpLand)
	{
		EvalValue lhs;
		if (!EvalExpr(e->a.get(), lhs))
			return false;
		if (!IsTrue(lhs))
		{
			out = MakeSigned(0);
			return true;
		}

		EvalValue rhs;
		if (!EvalExpr(e->b.get(), rhs))
			return false;
		out = MakeSigned(IsTrue(rhs) ? 1 : 0);
		return true;
	}

	if (e->op == TokenKind::OpLor)
	{
		EvalValue lhs;
		if (!EvalExpr(e->a.get(), lhs))
			return false;
		if (IsTrue(lhs))
		{
			out = MakeSigned(1);
			return true;
		}

		EvalValue rhs;
		if (!EvalExpr(e->b.get(), rhs))
			return false;
		out = MakeSigned(IsTrue(rhs) ? 1 : 0);
		return true;
	}

	EvalValue lhs;
	EvalValue rhs;
	if (!EvalExpr(e->a.get(), lhs) || !EvalExpr(e->b.get(), rhs))
		return false;

	if (e->op == TokenKind::OpLT) { out = MakeSigned(CompareLess(lhs, rhs) ? 1 : 0); return true; }
	if (e->op == TokenKind::OpGT) { out = MakeSigned(CompareLess(rhs, lhs) ? 1 : 0); return true; }
	if (e->op == TokenKind::OpLE) { out = MakeSigned(!CompareLess(rhs, lhs) ? 1 : 0); return true; }
	if (e->op == TokenKind::OpGE) { out = MakeSigned(!CompareLess(lhs, rhs) ? 1 : 0); return true; }
	if (e->op == TokenKind::OpEQ) { out = MakeSigned(CompareEqual(lhs, rhs) ? 1 : 0); return true; }
	if (e->op == TokenKind::OpNE) { out = MakeSigned(!CompareEqual(lhs, rhs) ? 1 : 0); return true; }

	if (e->op == TokenKind::OpLShift || e->op == TokenKind::OpRShift)
	{
		uint64_t sh = 0;
		if (rhs.type == ValueType::Unsigned)
		{
			if (rhs.bits >= 64)
				return false;
			sh = rhs.bits;
		}
		else
		{
			if ((rhs.bits >> 63) != 0 || rhs.bits >= 64)
				return false;
			sh = rhs.bits;
		}

		if (e->op == TokenKind::OpLShift)
		{
			out = lhs;
			out.bits = lhs.bits << sh;
			return true;
		}

		out = lhs;
		if (lhs.type == ValueType::Unsigned)
		{
			out.bits = lhs.bits >> sh;
		}
		else
		{
			if (sh == 0)
			{
				out.bits = lhs.bits;
			}
			else
			{
				bool neg = (lhs.bits >> 63) != 0;
				out.bits = lhs.bits >> sh;
				if (neg)
					out.bits |= (~uint64_t(0) << (64 - sh));
			}
		}
		return true;
	}

	ValueType t = UsualArithmetic(lhs.type, rhs.type);
	EvalValue a = ConvertToType(lhs, t);
	EvalValue b = ConvertToType(rhs, t);

	out.type = t;

	switch (e->op)
	{
	case TokenKind::OpStar:
		out.bits = a.bits * b.bits;
		return true;

	case TokenKind::OpPlus:
		out.bits = a.bits + b.bits;
		return true;

	case TokenKind::OpMinus:
		out.bits = a.bits - b.bits;
		return true;

	case TokenKind::OpAmp:
		out.bits = a.bits & b.bits;
		return true;

	case TokenKind::OpXor:
		out.bits = a.bits ^ b.bits;
		return true;

	case TokenKind::OpBor:
		out.bits = a.bits | b.bits;
		return true;

	case TokenKind::OpDiv:
		if (t == ValueType::Unsigned)
		{
			if (b.bits == 0)
				return false;
			out.bits = a.bits / b.bits;
			return true;
		}
		else
		{
			int64_t x = static_cast<int64_t>(a.bits);
			int64_t y = static_cast<int64_t>(b.bits);
			if (y == 0)
				return false;
			if (x == numeric_limits<int64_t>::min() && y == -1)
				return false;
			out.bits = static_cast<uint64_t>(x / y);
			return true;
		}

	case TokenKind::OpMod:
		if (t == ValueType::Unsigned)
		{
			if (b.bits == 0)
				return false;
			out.bits = a.bits % b.bits;
			return true;
		}
		else
		{
			int64_t x = static_cast<int64_t>(a.bits);
			int64_t y = static_cast<int64_t>(b.bits);
			if (y == 0)
				return false;
			if (x == numeric_limits<int64_t>::min() && y == -1)
				return false;
			out.bits = static_cast<uint64_t>(x % y);
			return true;
		}

	default:
		return false;
	}
}

bool ProcessLogicalLine(const vector<PPToken>& line, bool& has_output, string& output_line)
{
	has_output = false;
	output_line.clear();

	vector<PPToken> filtered;
	for (size_t i = 0; i < line.size(); ++i)
	{
		if (line[i].type == PPTokenType::WhitespaceSequence)
			continue;
		filtered.push_back(line[i]);
	}

	if (filtered.empty())
		return true;

	vector<Token> toks;
	for (size_t i = 0; i < filtered.size(); ++i)
	{
		Token t;
		if (!ConvertPPToken(filtered[i], t))
		{
			has_output = true;
			output_line = "error";
			return true;
		}
		toks.push_back(t);
	}

	Parser parser(toks);
	unique_ptr<Expr> root;
	if (!parser.parse(root))
	{
		has_output = true;
		output_line = "error";
		return true;
	}

	EvalValue value;
	if (!EvalExpr(root.get(), value))
	{
		has_output = true;
		output_line = "error";
		return true;
	}

	has_output = true;
	if (value.type == ValueType::Unsigned)
		output_line = to_string(value.bits) + "u";
	else
		output_line = to_string(static_cast<int64_t>(value.bits));

	return true;
}

void RunCtrlExpr(const string& input)
{
	PPTokenCollector collector;
	PPTokenizer tokenizer(collector);
	for (size_t i = 0; i < input.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(input[i]);
		tokenizer.process(c);
	}
	tokenizer.process(EndOfFile);

	vector<PPToken> line;
	for (size_t i = 0; i < collector.tokens.size(); ++i)
	{
		if (collector.tokens[i].type == PPTokenType::NewLine)
		{
			bool has_output = false;
			string output_line;
			ProcessLogicalLine(line, has_output, output_line);
			if (has_output)
				cout << output_line << endl;
			line.clear();
		}
		else
		{
			line.push_back(collector.tokens[i]);
		}
	}

	cout << "eof" << endl;
}

} // namespace pa3

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		pa3::RunCtrlExpr(oss.str());
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
