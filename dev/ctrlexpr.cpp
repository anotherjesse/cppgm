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

#define POSTTOKEN_MAIN posttoken_main
#include "posttoken.cpp"
#undef POSTTOKEN_MAIN

// mock implementation of IsDefinedIdentifier for PA3
// return true iff first code unit is odd
bool PA3Mock_IsDefinedIdentifier(const string& identifier)
{
	if (identifier.empty())
		return false;

	return (identifier[0] & 1) != 0;
}

static bool StartsWith(const string& s, const char* prefix)
{
	size_t n = strlen(prefix);
	return s.size() >= n && s.compare(0, n, prefix) == 0;
}

static bool IsUnsignedFundamentalType(EFundamentalType type)
{
	switch (type)
	{
	case FT_UNSIGNED_CHAR:
	case FT_UNSIGNED_SHORT_INT:
	case FT_UNSIGNED_INT:
	case FT_UNSIGNED_LONG_INT:
	case FT_UNSIGNED_LONG_LONG_INT:
	case FT_CHAR16_T:
	case FT_CHAR32_T:
		return true;

	default:
		return false;
	}
}

struct Value
{
	bool is_unsigned;
	uint64_t bits;

	Value()
		: is_unsigned(false), bits(0)
	{
	}

	Value(bool is_unsigned, uint64_t bits)
		: is_unsigned(is_unsigned), bits(bits)
	{
	}
};

static Value MakeSigned(int64_t v)
{
	return Value(false, (uint64_t) v);
}

static Value MakeUnsigned(uint64_t v)
{
	return Value(true, v);
}

static bool IsTrue(const Value& v)
{
	return v.bits != 0;
}

static string FormatValue(const Value& v)
{
	ostringstream oss;
	if (v.is_unsigned)
		oss << (unsigned long long) v.bits << 'u';
	else
		oss << (long long) (int64_t) v.bits;
	return oss.str();
}

static Value PromoteIntegerLiteral(const ParsedNumberLiteral& parsed)
{
	if (IsUnsignedFundamentalType(parsed.int_type))
		return MakeUnsigned((uint64_t) parsed.int_value);
	return MakeSigned((int64_t) parsed.int_value);
}

struct CtrlExprError : runtime_error
{
	explicit CtrlExprError(const string& msg)
		: runtime_error(msg)
	{
	}
};

enum class ExprTokenKind
{
	Value,
	Identifier,
	LParen,
	RParen,
	Question,
	Colon,
	Plus,
	Minus,
	LNot,
	Compl,
	Star,
	Div,
	Mod,
	LShift,
	RShift,
	LT,
	GT,
	LE,
	GE,
	EQ,
	NE,
	Amp,
	Xor,
	Bor,
	LAnd,
	LOr,
	OperatorWordOrPunc
};

struct ExprToken
{
	ExprTokenKind kind;
	string text;
	Value value;

	ExprToken()
		: kind(ExprTokenKind::OperatorWordOrPunc), value()
	{
	}
};

static bool IsHashLikeOperator(const string& s)
{
	return s == "#" || s == "##" || s == "%:" || s == "%:%:";
}

static bool MatchPreprocessingOpOrPunc(const string& source, ExprTokenKind& kind)
{
	if (source == "(") { kind = ExprTokenKind::LParen; return true; }
	if (source == ")") { kind = ExprTokenKind::RParen; return true; }
	if (source == "?") { kind = ExprTokenKind::Question; return true; }
	if (source == ":") { kind = ExprTokenKind::Colon; return true; }
	if (source == "+") { kind = ExprTokenKind::Plus; return true; }
	if (source == "-") { kind = ExprTokenKind::Minus; return true; }
	if (source == "!") { kind = ExprTokenKind::LNot; return true; }
	if (source == "~") { kind = ExprTokenKind::Compl; return true; }
	if (source == "*") { kind = ExprTokenKind::Star; return true; }
	if (source == "/") { kind = ExprTokenKind::Div; return true; }
	if (source == "%") { kind = ExprTokenKind::Mod; return true; }
	if (source == "<<") { kind = ExprTokenKind::LShift; return true; }
	if (source == ">>") { kind = ExprTokenKind::RShift; return true; }
	if (source == "<") { kind = ExprTokenKind::LT; return true; }
	if (source == ">") { kind = ExprTokenKind::GT; return true; }
	if (source == "<=") { kind = ExprTokenKind::LE; return true; }
	if (source == ">=") { kind = ExprTokenKind::GE; return true; }
	if (source == "==") { kind = ExprTokenKind::EQ; return true; }
	if (source == "!=" || source == "not_eq") { kind = ExprTokenKind::NE; return true; }
	if (source == "&" || source == "bitand") { kind = ExprTokenKind::Amp; return true; }
	if (source == "^" || source == "xor") { kind = ExprTokenKind::Xor; return true; }
	if (source == "|" || source == "bitor") { kind = ExprTokenKind::Bor; return true; }
	if (source == "&&" || source == "and") { kind = ExprTokenKind::LAnd; return true; }
	if (source == "||" || source == "or") { kind = ExprTokenKind::LOr; return true; }
	if (source == "not") { kind = ExprTokenKind::LNot; return true; }
	if (source == "compl") { kind = ExprTokenKind::Compl; return true; }
	return false;
}

static bool IsStdIntegerSuffixPrefix(const string& s, size_t& prefix_len)
{
	static const char* prefixes[] = {
		"ULL", "Ull", "uLL", "ull",
		"LLU", "LLu", "lLU", "llu",
		"UL", "Ul", "uL", "ul",
		"LU", "Lu", "lU", "lu",
		"LL", "ll",
		"U", "u",
		"L", "l"
	};

	for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
	{
		const char* p = prefixes[i];
		size_t n = strlen(p);
		if (s.size() >= n && s.compare(0, n, p) == 0)
		{
			prefix_len = n;
			return true;
		}
	}
	return false;
}

static string ClassifyMalformedInteger(const string& source, bool is_non_decimal, bool has_hex_prefix, const string& suffix)
{
	if (!source.empty() && source.size() > 2 && (source[0] == '0') && (source[1] == 'x' || source[1] == 'X')
		&& !IsAsciiHexDigit((unsigned char) source[2]))
	{
		return "malformed number (#4): " + source;
	}

	if (!suffix.empty() && suffix[0] == '_')
		return "malformed number (#2): " + source;

	size_t std_prefix_len = 0;
	if (!suffix.empty() && IsStdIntegerSuffixPrefix(suffix, std_prefix_len))
	{
		if (suffix.size() > std_prefix_len && suffix[std_prefix_len] == '_')
			return "malformed number (#2): " + source;

		if (has_hex_prefix)
			return "malformed number (#3): " + source;
		if (is_non_decimal)
			return "malformed number (#4): " + source;
		return "malformed number (#5): " + source;
	}

	if (has_hex_prefix)
		return "malformed number (#3): " + source;

	if (is_non_decimal)
		return "malformed number (#4): " + source;

	return "malformed number (#5): " + source;
}

static string ClassifyInvalidFloating(const string& source, size_t body_end)
{
	string suffix = source.substr(body_end);
	bool has_hex_prefix = StartsWith(source, "0x") || StartsWith(source, "0X");
	bool has_exp = source.find('e') != string::npos || source.find('E') != string::npos;
	bool has_hex_exp = source.find('p') != string::npos || source.find('P') != string::npos;

	if (!suffix.empty() && suffix[0] == '_')
		return "malformed number (#2): " + source;

	if (!suffix.empty())
	{
		if (suffix[0] == 'f' || suffix[0] == 'F' || suffix[0] == 'l' || suffix[0] == 'L')
		{
			if (suffix.size() > 1 && suffix[1] == '_')
				return "malformed number (#2): " + source;

			if (has_hex_prefix)
				return "malformed number (#3): " + source;
			return "invalid floating literal (#1): " + source;
		}

		if (has_hex_prefix)
			return "malformed number (#3): " + source;
		if (has_exp)
			return "invalid floating literal (#5): " + source;
		return "invalid floating literal (#1): " + source;
	}

	if (has_hex_prefix && has_hex_exp)
		return "malformed number (#3): " + source;

	if (has_exp)
		return "invalid floating literal (#2): " + source;

	return "invalid floating literal (#1): " + source;
}

static bool ClassifyNumberToken(const string& source, ExprToken& out, vector<string>& errors)
{
	struct CacheEntry
	{
		bool success;
		ExprToken token;
		string error;

		CacheEntry()
			: success(false)
		{
		}
	};

	static unordered_map<string, CacheEntry> cache;
	unordered_map<string, CacheEntry>::const_iterator cached = cache.find(source);
	if (cached != cache.end())
	{
		if (cached->second.success)
		{
			out = cached->second.token;
			return true;
		}

		errors.push_back(cached->second.error);
		return false;
	}

	CacheEntry entry;
	ParsedNumberLiteral parsed = ParsePpNumber(source);
	if (parsed.kind == NumericKind::Integer)
	{
		out.kind = ExprTokenKind::Value;
		out.value = PromoteIntegerLiteral(parsed);
		out.text = source;
		entry.success = true;
		entry.token = out;
		cache[source] = entry;
		return true;
	}

	if (parsed.kind == NumericKind::Floating)
	{
		entry.error = "floating literal in controlling expression";
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	if (parsed.kind == NumericKind::UserDefinedInteger || parsed.kind == NumericKind::UserDefinedFloating)
	{
		entry.error = "user-defined-literal in controlling expression";
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	if (source.size() >= 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X')
		&& (source.size() == 2 || !IsAsciiHexDigit((unsigned char) source[2])))
	{
		entry.error = "malformed number (#4): " + source;
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	size_t body_end = 0;
	unsigned __int128 value = 0;
	bool is_non_decimal = false;
	bool overflow = false;

	if (ParseIntegerCoreNoSuffix(source, body_end, value, is_non_decimal, overflow))
	{
		string suffix = source.substr(body_end);
		if (!is_non_decimal && suffix.empty())
		{
			if (overflow || value > (unsigned __int128) ULLONG_MAX)
			{
				entry.error = "decimal integer literal out of range(#2): " + source;
				cache[source] = entry;
				errors.push_back(entry.error);
				return false;
			}
			if (value > (unsigned __int128) LLONG_MAX)
			{
				entry.error = "decimal integer literal out of range(#4): " + source;
				cache[source] = entry;
				errors.push_back(entry.error);
				return false;
			}
		}

		entry.error = ClassifyMalformedInteger(source, is_non_decimal, source.size() > 1 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'), suffix);
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	size_t float_body_end = 0;
	if (ParseFloatCoreNoSuffix(source, float_body_end))
	{
		entry.error = ClassifyInvalidFloating(source, float_body_end);
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	if (source.size() > 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
	{
		entry.error = "malformed number (#4): " + source;
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	if (!source.empty() && (IsAsciiDigit((unsigned char) source[0]) || source[0] == '.'))
	{
		entry.error = "malformed number (#5): " + source;
		cache[source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	entry.error = "expected identifier or literal in controlling expression";
	cache[source] = entry;
	errors.push_back(entry.error);
	return false;
}

static bool ClassifyCharacterToken(const CollectedPPToken& raw, ExprToken& out, vector<string>& errors)
{
	struct CacheEntry
	{
		bool success;
		ExprToken token;
		string error;

		CacheEntry()
			: success(false)
		{
		}
	};

	static unordered_map<string, CacheEntry> cache;
	unordered_map<string, CacheEntry>::const_iterator cached = cache.find(raw.source);
	if (cached != cache.end())
	{
		if (cached->second.success)
		{
			out = cached->second.token;
			return true;
		}

		errors.push_back(cached->second.error);
		return false;
	}

	CacheEntry entry;
	ParsedCharacterLiteral parsed = ParseCharacterLiteralToken(raw.source);
	if (!parsed.valid)
	{
		entry.error = (raw.kind == CollectedPPTokenKind::UserDefinedCharacterLiteral)
			? "user-defined-literal in controlling expression"
			: "expected identifier or literal in controlling expression";
		cache[raw.source] = entry;
		if (raw.kind == CollectedPPTokenKind::UserDefinedCharacterLiteral)
			errors.push_back(entry.error);
		else
			errors.push_back(entry.error);
		return false;
	}

	if (parsed.encoding == EncodingPrefix::U16 && parsed.code_point > 0xFFFF)
	{
		entry.error = "expected identifier or literal in controlling expression";
		cache[raw.source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	if (raw.kind == CollectedPPTokenKind::UserDefinedCharacterLiteral || parsed.user_defined)
	{
		entry.error = "user-defined-literal in controlling expression";
		cache[raw.source] = entry;
		errors.push_back(entry.error);
		return false;
	}

	out.kind = ExprTokenKind::Value;
	out.text = raw.source;
	if (parsed.encoding == EncodingPrefix::U16 || parsed.encoding == EncodingPrefix::U32)
		out.value = MakeUnsigned((uint64_t) parsed.code_point);
	else
		out.value = MakeSigned((int64_t) parsed.code_point);
	entry.success = true;
	entry.token = out;
	cache[raw.source] = entry;
	return true;
}

static bool ClassifyCollectedToken(const CollectedPPToken& raw, ExprToken& out, vector<string>& errors)
{
	switch (raw.kind)
	{
	case CollectedPPTokenKind::Whitespace:
	case CollectedPPTokenKind::NewLine:
		return false;

	case CollectedPPTokenKind::HeaderName:
		errors.push_back("expected identifier or literal in controlling expression");
		return false;

	case CollectedPPTokenKind::NonWhitespaceCharacter:
		errors.push_back("non-whitespace-char in controlling expression");
		return false;

	case CollectedPPTokenKind::StringLiteral:
		errors.push_back("string-literal in controlling expression");
		return false;

	case CollectedPPTokenKind::UserDefinedStringLiteral:
		errors.push_back("user-defined-literal in controlling expression");
		return false;

	case CollectedPPTokenKind::CharacterLiteral:
	case CollectedPPTokenKind::UserDefinedCharacterLiteral:
		return ClassifyCharacterToken(raw, out, errors);

	case CollectedPPTokenKind::PpNumber:
		return ClassifyNumberToken(raw.source, out, errors);

	case CollectedPPTokenKind::Identifier:
		out.kind = ExprTokenKind::Identifier;
		out.text = raw.source;
		return true;

	case CollectedPPTokenKind::PreprocessingOpOrPunc:
		if (raw.source == "#") { out.kind = ExprTokenKind::OperatorWordOrPunc; out.text = raw.source; return true; }
		if (raw.source == "##" || raw.source == "%:" || raw.source == "%:%:") { out.kind = ExprTokenKind::OperatorWordOrPunc; out.text = raw.source; return true; }
		if (MatchPreprocessingOpOrPunc(raw.source, out.kind))
		{
			out.text = raw.source;
			return true;
		}

		out.kind = ExprTokenKind::OperatorWordOrPunc;
		out.text = raw.source;
		return true;
	}

	return false;
}

struct ExprParser
{
	const vector<ExprToken>& tokens;
	size_t pos;

	ExprParser(const vector<ExprToken>& tokens)
		: tokens(tokens), pos(0)
	{
	}

	bool at_end() const
	{
		return pos >= tokens.size();
	}

	const ExprToken& peek(size_t offset = 0) const
	{
		if (pos + offset >= tokens.size())
			throw CtrlExprError("expected identifier or literal in controlling expression");
		return tokens[pos + offset];
	}

	bool match(ExprTokenKind kind)
	{
		if (!at_end() && tokens[pos].kind == kind)
		{
			++pos;
			return true;
		}
		return false;
	}

	const ExprToken& consume()
	{
		if (at_end())
			throw CtrlExprError("expected identifier or literal in controlling expression");
		return tokens[pos++];
	}

	[[noreturn]] void throw_primary_error(const ExprToken& tok)
	{
		if (tok.kind == ExprTokenKind::OperatorWordOrPunc && IsHashLikeOperator(tok.text))
			throw CtrlExprError("unexpected operator in controlling expression");

		throw CtrlExprError("expected identifier or literal in controlling expression");
	}

	[[noreturn]] void throw_unexpected_operator(const ExprToken& tok)
	{
		(void) tok;
		throw CtrlExprError("unexpected operator in controlling expression");
	}

	[[noreturn]] void throw_evaluation_error()
	{
		throw CtrlExprError("evaluation error in controlling expression");
	}

	[[noreturn]] void throw_expected_colon()
	{
		throw CtrlExprError("expected colon");
	}

	[[noreturn]] void throw_closing_bracket_expected()
	{
		throw CtrlExprError("closing bracket expected in controlling expression");
	}

	[[noreturn]] void throw_left_over_tokens()
	{
		throw CtrlExprError("left over tokens at end of controlling expression");
	}

	Value eval_unary_plus(const Value& v)
	{
		return v;
	}

	Value eval_unary_minus(const Value& v)
	{
		return Value(v.is_unsigned, 0 - v.bits);
	}

	Value eval_unary_compl(const Value& v)
	{
		return Value(v.is_unsigned, ~v.bits);
	}

	Value eval_unary_lnot(const Value& v)
	{
		return MakeSigned(IsTrue(v) ? 0 : 1);
	}

	Value eval_add(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits + b.bits);
	}

	Value eval_sub(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits - b.bits);
	}

	Value eval_mul(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits * b.bits);
	}

	bool shift_count_ok(const Value& rhs, uint64_t& count)
	{
		if (rhs.is_unsigned)
		{
			if (rhs.bits >= 64)
				return false;
			count = rhs.bits;
			return true;
		}

		int64_t signed_count = (int64_t) rhs.bits;
		if (signed_count < 0 || signed_count >= 64)
			return false;
		count = (uint64_t) signed_count;
		return true;
	}

	Value eval_lshift(const Value& a, const Value& b, bool strict)
	{
		uint64_t count = 0;
		if (!shift_count_ok(b, count))
		{
			if (strict)
				throw_evaluation_error();
			return Value(a.is_unsigned, 0);
		}

		return Value(a.is_unsigned, a.bits << count);
	}

	Value eval_rshift(const Value& a, const Value& b, bool strict)
	{
		uint64_t count = 0;
		if (!shift_count_ok(b, count))
		{
			if (strict)
				throw_evaluation_error();
			return Value(a.is_unsigned, 0);
		}

		if (a.is_unsigned)
			return Value(true, a.bits >> count);

		if (count == 0)
			return a;

		int64_t signed_value = (int64_t) a.bits;
		if (signed_value >= 0)
			return MakeSigned(signed_value >> count);

		uint64_t shifted = a.bits >> count;
		uint64_t fill = (~(uint64_t) 0) << (64 - count);
		return Value(false, shifted | fill);
	}

	Value eval_div(const Value& a, const Value& b, bool strict)
	{
		bool is_unsigned = a.is_unsigned || b.is_unsigned;
		if (b.bits == 0)
		{
			if (strict)
				throw_evaluation_error();
			return Value(is_unsigned, 0);
		}

		if (is_unsigned)
			return Value(true, a.bits / b.bits);

		int64_t lhs = (int64_t) a.bits;
		int64_t rhs = (int64_t) b.bits;
		if (lhs == LLONG_MIN && rhs == -1)
		{
			if (strict)
				throw_evaluation_error();
			return Value(false, 0);
		}

		return MakeSigned(lhs / rhs);
	}

	Value eval_mod(const Value& a, const Value& b, bool strict)
	{
		bool is_unsigned = a.is_unsigned || b.is_unsigned;
		if (b.bits == 0)
		{
			if (strict)
				throw_evaluation_error();
			return Value(is_unsigned, 0);
		}

		if (is_unsigned)
			return Value(true, a.bits % b.bits);

		int64_t lhs = (int64_t) a.bits;
		int64_t rhs = (int64_t) b.bits;
		if (lhs == LLONG_MIN && rhs == -1)
		{
			if (strict)
				throw_evaluation_error();
			return Value(false, 0);
		}

		return MakeSigned(lhs % rhs);
	}

	Value eval_bitand(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits & b.bits);
	}

	Value eval_bitxor(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits ^ b.bits);
	}

	Value eval_bitor(const Value& a, const Value& b)
	{
		return Value(a.is_unsigned || b.is_unsigned, a.bits | b.bits);
	}

	Value eval_compare(const Value& a, const Value& b, ExprTokenKind kind)
	{
		bool result = false;
		if (!a.is_unsigned && !b.is_unsigned)
		{
			int64_t lhs = (int64_t) a.bits;
			int64_t rhs = (int64_t) b.bits;
			switch (kind)
			{
			case ExprTokenKind::LT: result = lhs < rhs; break;
			case ExprTokenKind::GT: result = lhs > rhs; break;
			case ExprTokenKind::LE: result = lhs <= rhs; break;
			case ExprTokenKind::GE: result = lhs >= rhs; break;
			case ExprTokenKind::EQ: result = lhs == rhs; break;
			case ExprTokenKind::NE: result = lhs != rhs; break;
			default: assert(false); break;
			}
		}
		else
		{
			uint64_t lhs = a.bits;
			uint64_t rhs = b.bits;
			switch (kind)
			{
			case ExprTokenKind::LT: result = lhs < rhs; break;
			case ExprTokenKind::GT: result = lhs > rhs; break;
			case ExprTokenKind::LE: result = lhs <= rhs; break;
			case ExprTokenKind::GE: result = lhs >= rhs; break;
			case ExprTokenKind::EQ: result = lhs == rhs; break;
			case ExprTokenKind::NE: result = lhs != rhs; break;
			default: assert(false); break;
			}
		}
		return MakeSigned(result ? 1 : 0);
	}

	Value parse_primary(bool strict)
	{
		(void) strict;
		if (at_end())
			throw CtrlExprError("expected identifier or literal in controlling expression");

		const ExprToken& tok = peek();
		switch (tok.kind)
		{
		case ExprTokenKind::Value:
			++pos;
			return tok.value;

		case ExprTokenKind::Identifier:
			{
				++pos;
				if (tok.text == "defined")
					return parse_defined();
				if (tok.text == "true")
					return MakeSigned(1);
				if (tok.text == "false")
					return MakeSigned(0);
				return MakeSigned(0);
			}

		case ExprTokenKind::LParen:
			{
				++pos;
				if (at_end() || tokens[pos].kind == ExprTokenKind::RParen)
					throw CtrlExprError("expected identifier or literal in controlling expression");
				Value v = parse_conditional(strict);
				if (!match(ExprTokenKind::RParen))
				{
					if (!at_end() && tokens[pos].kind == ExprTokenKind::OperatorWordOrPunc
						&& IsHashLikeOperator(tokens[pos].text))
						throw_unexpected_operator(tokens[pos]);
					throw_closing_bracket_expected();
				}
				return v;
			}

		case ExprTokenKind::OperatorWordOrPunc:
			throw_primary_error(tok);

		default:
			throw_primary_error(tok);
		}
	}

	Value parse_defined()
	{
		if (match(ExprTokenKind::LParen))
		{
			if (at_end() || tokens[pos].kind == ExprTokenKind::RParen)
				throw CtrlExprError("no closing paren on defined(identifier) in controlling expression");

			if (tokens[pos].kind != ExprTokenKind::Identifier)
				throw CtrlExprError("non-identifier after defined in controlling expression");

			string ident = tokens[pos].text;
			++pos;

			if (!match(ExprTokenKind::RParen))
				throw CtrlExprError("no closing paren on defined(identifier) in controlling expression");

			return MakeSigned(PA3Mock_IsDefinedIdentifier(ident) ? 1 : 0);
		}

		if (at_end() || tokens[pos].kind != ExprTokenKind::Identifier)
			throw CtrlExprError("non-identifier after defined in controlling expression");

		string ident = tokens[pos].text;
		++pos;
		return MakeSigned(PA3Mock_IsDefinedIdentifier(ident) ? 1 : 0);
	}

	Value parse_unary(bool strict)
	{
		if (!at_end())
		{
			const ExprToken& tok = peek();
			switch (tok.kind)
			{
			case ExprTokenKind::Plus:
				++pos;
				return eval_unary_plus(parse_unary(strict));
			case ExprTokenKind::Minus:
				++pos;
				return eval_unary_minus(parse_unary(strict));
			case ExprTokenKind::LNot:
				++pos;
				return eval_unary_lnot(parse_unary(strict));
			case ExprTokenKind::Compl:
				++pos;
				return eval_unary_compl(parse_unary(strict));
			default:
				break;
			}
		}

		return parse_primary(strict);
	}

	Value parse_multiplicative(bool strict)
	{
		Value lhs = parse_unary(strict);
		while (!at_end())
		{
			ExprTokenKind kind = peek().kind;
			if (kind != ExprTokenKind::Star && kind != ExprTokenKind::Div && kind != ExprTokenKind::Mod)
				break;

			++pos;
			Value rhs = parse_unary(strict);
			if (kind == ExprTokenKind::Star)
				lhs = eval_mul(lhs, rhs);
			else if (kind == ExprTokenKind::Div)
				lhs = eval_div(lhs, rhs, strict);
			else
				lhs = eval_mod(lhs, rhs, strict);
		}
		return lhs;
	}

	Value parse_additive(bool strict)
	{
		Value lhs = parse_multiplicative(strict);
		while (!at_end())
		{
			ExprTokenKind kind = peek().kind;
			if (kind != ExprTokenKind::Plus && kind != ExprTokenKind::Minus)
				break;

			++pos;
			Value rhs = parse_multiplicative(strict);
			if (kind == ExprTokenKind::Plus)
				lhs = eval_add(lhs, rhs);
			else
				lhs = eval_sub(lhs, rhs);
		}
		return lhs;
	}

	Value parse_shift(bool strict)
	{
		Value lhs = parse_additive(strict);
		while (!at_end())
		{
			ExprTokenKind kind = peek().kind;
			if (kind != ExprTokenKind::LShift && kind != ExprTokenKind::RShift)
				break;

			++pos;
			Value rhs = parse_additive(strict);
			if (kind == ExprTokenKind::LShift)
				lhs = eval_lshift(lhs, rhs, strict);
			else
				lhs = eval_rshift(lhs, rhs, strict);
		}
		return lhs;
	}

	Value parse_relational(bool strict)
	{
		Value lhs = parse_shift(strict);
		while (!at_end())
		{
			ExprTokenKind kind = peek().kind;
			if (kind != ExprTokenKind::LT && kind != ExprTokenKind::GT
				&& kind != ExprTokenKind::LE && kind != ExprTokenKind::GE)
				break;

			++pos;
			Value rhs = parse_shift(strict);
			lhs = eval_compare(lhs, rhs, kind);
		}
		return lhs;
	}

	Value parse_equality(bool strict)
	{
		Value lhs = parse_relational(strict);
		while (!at_end())
		{
			ExprTokenKind kind = peek().kind;
			if (kind != ExprTokenKind::EQ && kind != ExprTokenKind::NE)
				break;

			++pos;
			Value rhs = parse_relational(strict);
			lhs = eval_compare(lhs, rhs, kind);
		}
		return lhs;
	}

	Value parse_and(bool strict)
	{
		Value lhs = parse_equality(strict);
		while (!at_end() && peek().kind == ExprTokenKind::Amp)
		{
			++pos;
			Value rhs = parse_equality(strict);
			lhs = eval_bitand(lhs, rhs);
		}
		return lhs;
	}

	Value parse_xor(bool strict)
	{
		Value lhs = parse_and(strict);
		while (!at_end() && peek().kind == ExprTokenKind::Xor)
		{
			++pos;
			Value rhs = parse_and(strict);
			lhs = eval_bitxor(lhs, rhs);
		}
		return lhs;
	}

	Value parse_or(bool strict)
	{
		Value lhs = parse_xor(strict);
		while (!at_end() && peek().kind == ExprTokenKind::Bor)
		{
			++pos;
			Value rhs = parse_xor(strict);
			lhs = eval_bitor(lhs, rhs);
		}
		return lhs;
	}

	Value parse_logical_and(bool strict)
	{
		Value lhs = parse_or(strict);
		while (!at_end() && peek().kind == ExprTokenKind::LAnd)
		{
			++pos;
			bool lhs_truth = IsTrue(lhs);
			Value rhs = parse_or(strict && lhs_truth);
			lhs = MakeSigned((lhs_truth && IsTrue(rhs)) ? 1 : 0);
		}
		return lhs;
	}

	Value parse_logical_or(bool strict)
	{
		Value lhs = parse_logical_and(strict);
		while (!at_end() && peek().kind == ExprTokenKind::LOr)
		{
			++pos;
			bool lhs_truth = IsTrue(lhs);
			Value rhs = parse_logical_and(strict && !lhs_truth);
			lhs = MakeSigned((lhs_truth || IsTrue(rhs)) ? 1 : 0);
		}
		return lhs;
	}

	Value parse_conditional(bool strict)
	{
		Value lhs = parse_logical_or(strict);
		if (match(ExprTokenKind::Question))
		{
			bool cond = IsTrue(lhs);
			Value true_value = parse_conditional(strict && cond);
			if (!match(ExprTokenKind::Colon))
				throw_expected_colon();
			Value false_value = parse_conditional(strict && !cond);
			bool result_unsigned = true_value.is_unsigned || false_value.is_unsigned;
			Value chosen = cond ? true_value : false_value;
			if (result_unsigned)
				return MakeUnsigned(chosen.bits);
			return MakeSigned((int64_t) chosen.bits);
		}
		return lhs;
	}

	Value parse_top_level()
	{
		Value v = parse_conditional(true);
		if (!at_end())
		{
			if (peek().kind == ExprTokenKind::OperatorWordOrPunc && IsHashLikeOperator(peek().text))
				throw_unexpected_operator(peek());
			throw_left_over_tokens();
		}
		return v;
	}
};

static void EmitLineErrors(const vector<string>& errors)
{
	for (size_t i = 0; i < errors.size(); ++i)
		cerr << "ERROR: " << errors[i] << '\n';
}

static bool ProcessLogicalLine(const vector<CollectedPPToken>& raw_tokens)
{
	vector<ExprToken> tokens;
	vector<string> errors;

	for (size_t i = 0; i < raw_tokens.size(); ++i)
	{
		const CollectedPPToken& raw = raw_tokens[i];
		ExprToken tok;
		if (ClassifyCollectedToken(raw, tok, errors))
			tokens.push_back(tok);
	}

	if (!errors.empty())
	{
		EmitLineErrors(errors);
		cout << "error" << endl;
		return false;
	}

	if (tokens.empty())
		return true;

	try
	{
		ExprParser parser(tokens);
		Value value = parser.parse_top_level();
		cout << FormatValue(value) << endl;
	}
	catch (CtrlExprError& e)
	{
		cerr << "ERROR: " << e.what() << '\n';
		cout << "error" << endl;
	}

	return true;
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

		const vector<CollectedPPToken>& raw_tokens = collector.tokens;
		vector<CollectedPPToken> line_tokens;

		for (size_t i = 0; i < raw_tokens.size(); ++i)
		{
			if (raw_tokens[i].kind == CollectedPPTokenKind::NewLine)
			{
				ProcessLogicalLine(line_tokens);
				line_tokens.clear();
				continue;
			}

			if (raw_tokens[i].kind != CollectedPPTokenKind::Whitespace)
				line_tokens.push_back(raw_tokens[i]);
		}

		if (!line_tokens.empty())
			ProcessLogicalLine(line_tokens);

		cout << "eof" << endl;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
