#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"

constexpr int EndOfFile = -1;

int HexCharToValue(int c)
{
	switch (c)
	{
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A': case 'a': return 10;
	case 'B': case 'b': return 11;
	case 'C': case 'c': return 12;
	case 'D': case 'd': return 13;
	case 'E': case 'e': return 14;
	case 'F': case 'f': return 15;
	default: throw logic_error("HexCharToValue of nonhex char");
	}
}

const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted =
{
	{0xA8,0xA8}, {0xAA,0xAA}, {0xAD,0xAD}, {0xAF,0xAF}, {0xB2,0xB5},
	{0xB7,0xBA}, {0xBC,0xBE}, {0xC0,0xD6}, {0xD8,0xF6}, {0xF8,0xFF},
	{0x100,0x167F}, {0x1681,0x180D}, {0x180F,0x1FFF}, {0x200B,0x200D},
	{0x202A,0x202E}, {0x203F,0x2040}, {0x2054,0x2054}, {0x2060,0x206F},
	{0x2070,0x218F}, {0x2460,0x24FF}, {0x2776,0x2793}, {0x2C00,0x2DFF},
	{0x2E80,0x2FFF}, {0x3004,0x3007}, {0x3021,0x302F}, {0x3031,0x303F},
	{0x3040,0xD7FF}, {0xF900,0xFD3D}, {0xFD40,0xFDCF}, {0xFDF0,0xFE44},
	{0xFE47,0xFFFD}, {0x10000,0x1FFFD}, {0x20000,0x2FFFD}, {0x30000,0x3FFFD},
	{0x40000,0x4FFFD}, {0x50000,0x5FFFD}, {0x60000,0x6FFFD}, {0x70000,0x7FFFD},
	{0x80000,0x8FFFD}, {0x90000,0x9FFFD}, {0xA0000,0xAFFFD}, {0xB0000,0xBFFFD},
	{0xC0000,0xCFFFD}, {0xD0000,0xDFFFD}, {0xE0000,0xEFFFD}
};

const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted =
{
	{0x300,0x36F}, {0x1DC0,0x1DFF}, {0x20D0,0x20FF}, {0xFE20,0xFE2F}
};

const unordered_set<string> Digraph_IdentifierLike_Operators =
{
	"new", "delete", "and", "and_eq", "bitand", "bitor", "compl",
	"not", "not_eq", "or", "or_eq", "xor", "xor_eq"
};

const unordered_set<int> SimpleEscapeSequence_CodePoints =
{
	'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
};

struct CodePoint
{
	int value;
	bool from_ucn;
};

bool IsInRanges(int cp, const vector<pair<int, int>>& ranges)
{
	auto it = lower_bound(ranges.begin(), ranges.end(), make_pair(cp, cp),
		[](const pair<int, int>& lhs, const pair<int, int>& rhs)
		{
			return lhs.second < rhs.first;
		});
	return it != ranges.end() && it->first <= cp && cp <= it->second;
}

bool IsWhitespaceNoNewline(int cp)
{
	return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f';
}

bool IsDigit(int cp)
{
	return '0' <= cp && cp <= '9';
}

bool IsHexDigit(int cp)
{
	return IsDigit(cp) || ('a' <= cp && cp <= 'f') || ('A' <= cp && cp <= 'F');
}

bool IsNondigit(int cp)
{
	return cp == '_' || ('a' <= cp && cp <= 'z') || ('A' <= cp && cp <= 'Z');
}

bool IsIdentifierInitial(int cp, bool from_ucn)
{
	(void) from_ucn;
	if (IsNondigit(cp))
		return true;
	if (!IsInRanges(cp, AnnexE1_Allowed_RangesSorted))
		return false;
	return !IsInRanges(cp, AnnexE2_DisallowedInitially_RangesSorted);
}

bool IsIdentifierContinue(int cp, bool from_ucn)
{
	(void) from_ucn;
	if (IsIdentifierInitial(cp, from_ucn) || IsDigit(cp))
		return true;
	return IsInRanges(cp, AnnexE1_Allowed_RangesSorted);
}

string EncodeUTF8(int cp)
{
	string out;
	if (cp < 0 || cp > 0x10FFFF)
		throw runtime_error("invalid code point");
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
		if (c0 <= 0x7F)
		{
			cp = c0;
			need = 1;
		}
		else if ((c0 & 0xE0) == 0xC0)
		{
			cp = c0 & 0x1F;
			need = 2;
			minv = 0x80;
		}
		else if ((c0 & 0xF0) == 0xE0)
		{
			cp = c0 & 0x0F;
			need = 3;
			minv = 0x800;
		}
		else if ((c0 & 0xF8) == 0xF0)
		{
			cp = c0 & 0x07;
			need = 4;
			minv = 0x10000;
		}
		else
			throw runtime_error("invalid utf-8");
		if (i + need > input.size())
			throw runtime_error("truncated utf-8");
		for (size_t j = 1; j < need; ++j)
		{
			unsigned char cj = static_cast<unsigned char>(input[i + j]);
			if ((cj & 0xC0) != 0x80)
				throw runtime_error("invalid utf-8");
			cp = (cp << 6) | (cj & 0x3F);
		}
		if (need > 1 && cp < minv)
			throw runtime_error("overlong utf-8");
		if (0xD800 <= cp && cp <= 0xDFFF)
			throw runtime_error("invalid utf-8 surrogate");
		if (cp > 0x10FFFF)
			throw runtime_error("invalid utf-8");
		out.push_back(cp);
		i += need;
	}
	return out;
}

vector<CodePoint> TranslatePhase123(const string& input)
{
	const unordered_map<int, int> trigraphs =
	{
		{'=', '#'}, {'/', '\\'}, {'\'', '^'}, {'(', '['}, {')', ']'},
		{'!', '|'}, {'<', '{'}, {'>', '}'}, {'-', '~'}
	};

	vector<int> raw = DecodeUTF8(input);
	vector<CodePoint> out;

	auto match_ascii = [&](size_t at, const string& text) -> bool
	{
		if (at + text.size() > raw.size())
			return false;
		for (size_t i = 0; i < text.size(); ++i)
			if (raw[at + i] != static_cast<unsigned char>(text[i]))
				return false;
		return true;
	};

	auto copy_raw_string_literal = [&](size_t& i) -> bool
	{
		if (i > 0)
		{
			int prev = raw[i - 1];
			if (prev == '"' || prev == '\'' || IsIdentifierContinue(prev, false))
				return false;
		}

		size_t prefix_len = 0;
		if (match_ascii(i, "u8R\""))
			prefix_len = 4;
		else if (match_ascii(i, "uR\"") || match_ascii(i, "UR\"") || match_ascii(i, "LR\""))
			prefix_len = 3;
		else if (match_ascii(i, "R\""))
			prefix_len = 2;
		else
			return false;

		size_t j = i;
		for (size_t k = 0; k < prefix_len; ++k, ++j)
			out.push_back({raw[j], false});

		string delim;
		while (j < raw.size() && raw[j] != '(')
		{
			delim.push_back(static_cast<char>(raw[j]));
			out.push_back({raw[j], false});
			++j;
		}
		if (j == raw.size())
		{
			i = j;
			return true;
		}
		out.push_back({raw[j], false});
		++j;

		while (j < raw.size())
		{
			out.push_back({raw[j], false});
			if (raw[j] == ')')
			{
				bool matches = j + delim.size() < raw.size();
				for (size_t k = 0; matches && k < delim.size(); ++k)
					matches = raw[j + 1 + k] == static_cast<unsigned char>(delim[k]);
				if (matches && j + delim.size() + 1 < raw.size() && raw[j + delim.size() + 1] == '"')
				{
					for (size_t k = 0; k < delim.size() + 1; ++k)
						out.push_back({raw[j + 1 + k], false});
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
		if (copy_raw_string_literal(i))
			continue;

		int cp = raw[i];

		if (cp == '?' && i + 2 < raw.size() && raw[i + 1] == '?')
		{
			auto it = trigraphs.find(raw[i + 2]);
			if (it != trigraphs.end())
			{
				cp = it->second;
				i += 3;
			}
			else
				++i;
		}
		else
			++i;

		if (cp == '\\' && i < raw.size() && raw[i] == 'u')
		{
			if (i + 4 >= raw.size())
				throw runtime_error("truncated universal-character-name");
			int value = 0;
			for (size_t j = 0; j < 4; ++j)
			{
				if (!IsHexDigit(raw[i + 1 + j]))
					throw runtime_error("invalid universal-character-name");
				value = value * 16 + HexCharToValue(raw[i + 1 + j]);
			}
			cp = value;
			i += 5;
			out.push_back({cp, true});
			continue;
		}
		if (cp == '\\' && i < raw.size() && raw[i] == 'U')
		{
			if (i + 8 >= raw.size())
				throw runtime_error("truncated universal-character-name");
			int value = 0;
			for (size_t j = 0; j < 8; ++j)
			{
				if (!IsHexDigit(raw[i + 1 + j]))
					throw runtime_error("invalid universal-character-name");
				value = value * 16 + HexCharToValue(raw[i + 1 + j]);
			}
			cp = value;
			i += 9;
			out.push_back({cp, true});
			continue;
		}

		if (cp == '\\' && i < raw.size() && raw[i] == '\n')
		{
			++i;
			continue;
		}

		out.push_back({cp, false});
	}

	if (!out.empty() && out.back().value != '\n')
		out.push_back({'\n', false});

	return out;
}

struct PPTokenizer
{
	IPPTokenStream& output;
	vector<CodePoint> cps;
	size_t pos = 0;
	bool line_start = true;
	int include_state = 0;

	explicit PPTokenizer(IPPTokenStream& out)
		: output(out)
	{}

	int peek(size_t offset = 0) const
	{
		size_t idx = pos + offset;
		return idx < cps.size() ? cps[idx].value : EndOfFile;
	}

	bool peek_from_ucn(size_t offset = 0) const
	{
		size_t idx = pos + offset;
		return idx < cps.size() && cps[idx].from_ucn;
	}

	void emit_whitespace()
	{
		output.emit_whitespace_sequence();
	}

	string take_codepoint()
	{
		string s = EncodeUTF8(cps[pos].value);
		++pos;
		return s;
	}

	bool MatchLiteral(const string& text) const
	{
		for (size_t i = 0; i < text.size(); ++i)
			if (peek(i) != static_cast<unsigned char>(text[i]))
				return false;
		return true;
	}

	bool IsRawPrefix() const
	{
		static const vector<string> prefixes = {"R\"", "u8R\"", "uR\"", "UR\"", "LR\""};
		for (const string& prefix : prefixes)
			if (MatchLiteral(prefix))
				return true;
		return false;
	}

	string ScanEscape(bool in_string)
	{
		string out;
		out += take_codepoint();
		int cp = peek();
		if (cp == EndOfFile || cp == '\n')
			throw runtime_error("unterminated escape sequence");
		out += take_codepoint();
		if (SimpleEscapeSequence_CodePoints.count(cp))
			return out;
		if (cp == 'x')
		{
			if (!IsHexDigit(peek()))
				throw runtime_error("invalid hex escape sequence");
			while (IsHexDigit(peek()))
				out += take_codepoint();
			return out;
		}
		if ('0' <= cp && cp <= '7')
		{
			for (int i = 0; i < 2; ++i)
			{
				int d = peek();
				if ('0' <= d && d <= '7')
					out += take_codepoint();
				else
					break;
			}
			return out;
		}
		if (cp == '8' || cp == '9')
			throw runtime_error("invalid escape sequence");
		if (cp == 'u' || cp == 'U')
		{
			int digits = cp == 'u' ? 4 : 8;
			for (int i = 0; i < digits; ++i)
			{
				if (!IsHexDigit(peek()))
					throw runtime_error("invalid universal-character-name");
				out += take_codepoint();
			}
			return out;
		}
		if (!in_string)
			return out;
		return out;
	}

	string ScanQuotedLiteral()
	{
		string out;
		if (MatchLiteral("u8") && peek(2) == '"')
		{
			out += take_codepoint();
			out += take_codepoint();
		}
		else if (peek() == 'u' || peek() == 'U' || peek() == 'L')
			out += take_codepoint();

		if (peek() != '\'' && peek() != '"')
			throw runtime_error("invalid literal prefix");
		int quote = peek();
		out += take_codepoint();
		while (true)
		{
			int cp = peek();
			if (cp == EndOfFile || cp == '\n')
				throw runtime_error("unterminated string or character literal");
			if (cp == '\\')
				out += ScanEscape(true);
			else
			{
				out += take_codepoint();
				if (cp == quote)
					break;
			}
		}
		return out;
	}

	bool IsCharacterLiteralText(const string& lit) const
	{
		size_t i = 0;
		if (!lit.empty() && (lit[0] == 'u' || lit[0] == 'U' || lit[0] == 'L'))
			i = 1;
		return i < lit.size() && lit[i] == '\'';
	}

	string ScanRawLiteral()
	{
		string out;
		if (MatchLiteral("u8R\""))
		{
			out += take_codepoint();
			out += take_codepoint();
			out += take_codepoint();
			out += take_codepoint();
		}
		else if ((peek() == 'u' || peek() == 'U' || peek() == 'L') && peek(1) == 'R' && peek(2) == '"')
		{
			out += take_codepoint();
			out += take_codepoint();
			out += take_codepoint();
		}
		else if (peek() == 'R' && peek(1) == '"')
		{
			out += take_codepoint();
			out += take_codepoint();
		}
		else
			throw runtime_error("invalid raw string prefix");

		string delim;
		while (true)
		{
			int cp = peek();
			if (cp == EndOfFile || cp == '\n')
				throw runtime_error("unterminated raw string literal");
			if (cp == '(')
			{
				out += take_codepoint();
				break;
			}
			if (delim.size() >= 16)
				throw runtime_error("raw string delimiter too long");
			if (cp == ' ' || cp == '\\' || cp == ')' || cp == '(' || cp == '\t' || cp == '\v' || cp == '\f')
				throw runtime_error("invalid raw string delimiter");
			out += take_codepoint();
			delim += static_cast<char>(cp);
		}

		while (true)
		{
			int cp = peek();
			if (cp == EndOfFile)
				throw runtime_error("unterminated raw string literal");
			out += take_codepoint();
			if (cp == ')')
			{
				bool matches = true;
				for (size_t i = 0; i < delim.size(); ++i)
					if (peek(i) != static_cast<unsigned char>(delim[i]))
						matches = false;
				if (matches && peek(delim.size()) == '"')
				{
					for (size_t i = 0; i < delim.size(); ++i)
						out += take_codepoint();
					out += take_codepoint();
					break;
				}
			}
		}
		return out;
	}

	string ScanIdentifier()
	{
		string out;
		if (!IsIdentifierInitial(peek(), peek_from_ucn()))
			throw logic_error("identifier scan invariant");
		out += take_codepoint();
		while (IsIdentifierContinue(peek(), peek_from_ucn()))
			out += take_codepoint();
		return out;
	}

	string ScanPPNumber()
	{
		string out;
		if (peek() == '.')
			out += take_codepoint();
		while (true)
		{
			int cp = peek();
			if (IsDigit(cp) || cp == '.')
			{
				out += take_codepoint();
				continue;
			}
			if ((cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P') &&
				(IsDigit(peek(static_cast<size_t>(1))) || peek(1) == '+' || peek(1) == '-'))
			{
				out += take_codepoint();
				if (peek() == '+' || peek() == '-')
					out += take_codepoint();
				continue;
			}
			if (IsIdentifierContinue(cp, peek_from_ucn()))
			{
				out += take_codepoint();
				continue;
			}
			break;
		}
		return out;
	}

	bool ScanCommentOrWhitespace()
	{
		bool any = false;
		while (true)
		{
			if (IsWhitespaceNoNewline(peek()))
			{
				any = true;
				while (IsWhitespaceNoNewline(peek()))
					++pos;
				continue;
			}
			if (peek() == '/' && peek(1) == '/')
			{
				any = true;
				pos += 2;
				while (peek() != EndOfFile && peek() != '\n')
					++pos;
				continue;
			}
			if (peek() == '/' && peek(1) == '*')
			{
				any = true;
				pos += 2;
				while (true)
				{
					if (peek() == EndOfFile)
						throw runtime_error("unterminated comment");
					if (peek() == '*' && peek(1) == '/')
					{
						pos += 2;
						break;
					}
					++pos;
				}
				continue;
			}
			break;
		}
		return any;
	}

	bool ShouldTreatAsHeaderName() const
	{
		return include_state == 2;
	}

	bool TryHeaderName()
	{
		if (!ShouldTreatAsHeaderName())
			return false;
		if (peek() == '"')
		{
			string out;
			out += take_codepoint();
			while (true)
			{
				int cp = peek();
				if (cp == EndOfFile || cp == '\n')
					throw runtime_error("unterminated header name");
				if (cp == '\\')
					out += ScanEscape(true);
				else
				{
					out += take_codepoint();
					if (cp == '"')
						break;
				}
			}
			output.emit_header_name(out);
			include_state = 0;
			line_start = false;
			return true;
		}
		if (peek() == '<')
		{
			string out;
			out += take_codepoint();
			while (true)
			{
				int cp = peek();
				if (cp == EndOfFile || cp == '\n')
					throw runtime_error("unterminated header name");
				out += take_codepoint();
				if (cp == '>')
					break;
			}
			output.emit_header_name(out);
			include_state = 0;
			line_start = false;
			return true;
		}
		include_state = 0;
		return false;
	}

	bool TryOpOrPunc()
	{
		if (MatchLiteral("<::") && peek(3) != ':' && peek(3) != '>')
		{
			output.emit_preprocessing_op_or_punc(take_codepoint());
			include_state = 0;
			line_start = false;
			return true;
		}
		static const vector<string> ops =
		{
			"%:%:", "...", ">>=", "<<=", "->*", ".*", "->", "++", "--", "<<", ">>",
			"<=", ">=", "==", "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "^=",
			"&=", "|=", "::", "##", "<:", ":>", "<%", "%>", "%:", "{", "}", "[", "]",
			"#", "(", ")", ";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|",
			"~", "!", "=", "<", ">", ","
		};
		for (const string& op : ops)
		{
			if (!MatchLiteral(op))
				continue;
			string out;
			for (size_t i = 0; i < op.size(); ++i)
				out += take_codepoint();
			output.emit_preprocessing_op_or_punc(out);
			include_state = (line_start && (out == "#" || out == "%:")) ? 1 : 0;
			line_start = false;
			return true;
		}
		return false;
	}

	void Run(const string& input)
	{
		cps = TranslatePhase123(input);
		pos = 0;
		line_start = true;
		include_state = 0;

		while (pos < cps.size())
		{
			if (ScanCommentOrWhitespace())
			{
				emit_whitespace();
				continue;
			}

			if (peek() == '\n')
			{
				++pos;
				output.emit_new_line();
				line_start = true;
				include_state = 0;
				continue;
			}

			if (TryHeaderName())
				continue;

			if (peek() == '.' && IsDigit(peek(1)))
			{
				output.emit_pp_number(ScanPPNumber());
				include_state = 0;
				line_start = false;
				continue;
			}

			if (IsDigit(peek()))
			{
				output.emit_pp_number(ScanPPNumber());
				include_state = 0;
				line_start = false;
				continue;
			}

			if (IsRawPrefix())
			{
				string lit = ScanRawLiteral();
				if (IsIdentifierInitial(peek(), peek_from_ucn()))
					output.emit_user_defined_string_literal(lit + ScanIdentifier());
				else
					output.emit_string_literal(lit);
				include_state = 0;
				line_start = false;
				continue;
			}

			if (peek() == '"' || peek() == '\'' ||
				((peek() == 'u' || peek() == 'U' || peek() == 'L') && (peek(1) == '"' || peek(1) == '\'')) ||
				(MatchLiteral("u8") && peek(2) == '"'))
			{
				string lit = ScanQuotedLiteral();
				bool is_char = IsCharacterLiteralText(lit);
				if (IsIdentifierInitial(peek(), peek_from_ucn()))
				{
					string suffix = ScanIdentifier();
					if (is_char)
						output.emit_user_defined_character_literal(lit + suffix);
					else
						output.emit_user_defined_string_literal(lit + suffix);
				}
				else
				{
					if (is_char)
						output.emit_character_literal(lit);
					else
						output.emit_string_literal(lit);
				}
				include_state = 0;
				line_start = false;
				continue;
			}

			if (IsIdentifierInitial(peek(), peek_from_ucn()))
			{
				string id = ScanIdentifier();
				include_state = (include_state == 1 && id == "include") ? 2 : 0;
				if (Digraph_IdentifierLike_Operators.count(id))
					output.emit_preprocessing_op_or_punc(id);
				else
					output.emit_identifier(id);
				line_start = false;
				continue;
			}

			if (TryOpOrPunc())
				continue;

			output.emit_non_whitespace_char(take_codepoint());
			include_state = 0;
			line_start = false;
		}

		output.emit_eof();
	}

	void process(int c)
	{
		if (c == EndOfFile)
			return;
		throw logic_error("streaming interface not used in this implementation");
	}
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		DebugPPTokenStream output;
		PPTokenizer tokenizer(output);
		tokenizer.Run(oss.str());
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
