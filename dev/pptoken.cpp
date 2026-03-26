#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"

// EndOfFile: synthetic "character" to represent the end of source file
constexpr int EndOfFile = -1;

// given hex digit character c, return its value
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
	case 'A': return 10;
	case 'a': return 10;
	case 'B': return 11;
	case 'b': return 11;
	case 'C': return 12;
	case 'c': return 12;
	case 'D': return 13;
	case 'd': return 13;
	case 'E': return 14;
	case 'e': return 14;
	case 'F': return 15;
	case 'f': return 15;
	default: throw logic_error("HexCharToValue of nonhex char");
	}
}

// See C++ standard 2.11 Identifiers and Appendix/Annex E.1
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

// See C++ standard 2.11 Identifiers and Appendix/Annex E.2
const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted =
{
	{0x300,0x36F},
	{0x1DC0,0x1DFF},
	{0x20D0,0x20FF},
	{0xFE20,0xFE2F}
};

// See C++ standard 2.13 Operators and punctuators
const unordered_set<string> Digraph_IdentifierLike_Operators =
{
	"new", "delete", "and", "and_eq", "bitand",
	"bitor", "compl", "not", "not_eq", "or",
	"or_eq", "xor", "xor_eq"
};

// See `simple-escape-sequence` grammar
const unordered_set<int> SimpleEscapeSequence_CodePoints =
{
	'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
};

namespace
{

struct Span
{
	size_t begin;
	size_t end;
};

bool IsHexDigit(int c)
{
	return
		(c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') ||
		(c >= 'A' && c <= 'F');
}

bool IsOctDigit(int c)
{
	return c >= '0' && c <= '7';
}

bool IsDigit(int c)
{
	return c >= '0' && c <= '9';
}

bool IsBasicIdentifierStart(int c)
{
	return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsWhitespaceNotNewline(int c)
{
	return c == ' ' || c == '\t' || c == '\v' || c == '\f';
}

bool IsInRanges(const vector<pair<int, int>>& ranges, int cp)
{
	auto it = lower_bound(ranges.begin(), ranges.end(), make_pair(cp, cp),
		[](const pair<int, int>& lhs, const pair<int, int>& rhs)
		{
			return lhs.second < rhs.first;
		});

	if (it == ranges.end())
		return false;

	return it->first <= cp && cp <= it->second;
}

bool IsIdentifierStart(int cp)
{
	if (IsBasicIdentifierStart(cp))
		return true;

	if (cp < 0x80)
		return false;

	if (!IsInRanges(AnnexE1_Allowed_RangesSorted, cp))
		return false;

	if (IsInRanges(AnnexE2_DisallowedInitially_RangesSorted, cp))
		return false;

	return true;
}

bool IsIdentifierContinue(int cp)
{
	if (IsIdentifierStart(cp) || IsDigit(cp))
		return true;

	if (cp < 0x80)
		return false;

	return IsInRanges(AnnexE1_Allowed_RangesSorted, cp);
}

string EncodeCodePointUtf8(int cp)
{
	if (cp < 0 || cp > 0x10FFFF)
		throw runtime_error("invalid code point");

	string out;

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

	return out;
}

bool StartsWith(const vector<int>& cps, size_t i, const string& s)
{
	if (i + s.size() > cps.size())
		return false;

	for (size_t k = 0; k < s.size(); ++k)
		if (cps[i + k] != static_cast<unsigned char>(s[k]))
			return false;

	return true;
}

} // namespace

// Tokenizer
struct PPTokenizer
{
	IPPTokenStream& output;
	vector<unsigned char> input_units;

	enum DirectiveState
	{
		DS_NONE,
		DS_AFTER_HASH,
		DS_AFTER_INCLUDE
	};

	PPTokenizer(IPPTokenStream& output)
		: output(output)
	{}

	void process(int c)
	{
		if (c != EndOfFile)
		{
			if (c < 0 || c > 255)
				throw runtime_error("invalid input byte");

			input_units.push_back(static_cast<unsigned char>(c));
			return;
		}

		tokenize_all();
	}

	void tokenize_all()
	{
		vector<Span> spans;
		vector<int> cps = decode_utf8(input_units, spans);
		const vector<int> original_cps = cps;

		replace_trigraphs(cps, spans);
		splice_lines(cps, spans);
		replace_ucn(cps, spans);

		if (!cps.empty() && cps.back() != '\n')
			cps.push_back('\n');
		if (!spans.empty() && cps.size() == spans.size() + 1)
			spans.push_back(spans.back());

		lex(cps, spans, original_cps);
		output.emit_eof();
	}

	vector<int> decode_utf8(const vector<unsigned char>& units, vector<Span>& spans)
	{
		vector<int> cps;
		size_t cp_index = 0;

		size_t i = 0;
		while (i < units.size())
		{
			unsigned char b0 = units[i];

			if (b0 <= 0x7F)
			{
				cps.push_back(b0);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				++i;
				continue;
			}

			auto expect_cont = [&](size_t idx) -> unsigned char
			{
				if (idx >= units.size())
					throw runtime_error("utf8 truncated sequence");

				unsigned char bx = units[idx];
				if ((bx & 0xC0) != 0x80)
					throw runtime_error("utf8 invalid continuation unit");

				return bx;
			};

			if (b0 >= 0xC2 && b0 <= 0xDF)
			{
				unsigned char b1 = expect_cont(i + 1);
				int cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 2;
				continue;
			}

			if (b0 == 0xE0)
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				if (b1 < 0xA0 || b1 > 0xBF)
					throw runtime_error("utf8 overlong 3-byte sequence");
				int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 3;
				continue;
			}

			if ((b0 >= 0xE1 && b0 <= 0xEC) || (b0 >= 0xEE && b0 <= 0xEF))
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 3;
				continue;
			}

			if (b0 == 0xED)
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				if (b1 < 0x80 || b1 > 0x9F)
					throw runtime_error("utf8 surrogate sequence");
				int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 3;
				continue;
			}

			if (b0 == 0xF0)
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				unsigned char b3 = expect_cont(i + 3);
				if (b1 < 0x90 || b1 > 0xBF)
					throw runtime_error("utf8 overlong 4-byte sequence");
				int cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 4;
				continue;
			}

			if (b0 >= 0xF1 && b0 <= 0xF3)
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				unsigned char b3 = expect_cont(i + 3);
				int cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 4;
				continue;
			}

			if (b0 == 0xF4)
			{
				unsigned char b1 = expect_cont(i + 1);
				unsigned char b2 = expect_cont(i + 2);
				unsigned char b3 = expect_cont(i + 3);
				if (b1 < 0x80 || b1 > 0x8F)
					throw runtime_error("utf8 code point out of range");
				int cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
				cps.push_back(cp);
				spans.push_back({cp_index, cp_index + 1});
				++cp_index;
				i += 4;
				continue;
			}

			if ((b0 & 0xFC) == 0xFC)
				throw runtime_error("utf8 invalid unit (111111xx)");

			throw runtime_error("utf8 invalid leading unit");
		}

		return cps;
	}

	void replace_trigraphs(vector<int>& cps, vector<Span>& spans)
	{
		vector<int> out_cps;
		vector<Span> out_spans;
		out_cps.reserve(cps.size());
		out_spans.reserve(spans.size());

		size_t i = 0;
		while (i < cps.size())
		{
			if (i + 2 < cps.size() && cps[i] == '?' && cps[i + 1] == '?')
			{
				switch (cps[i + 2])
				{
				case '=': out_cps.push_back('#'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '/': out_cps.push_back('\\'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '\'': out_cps.push_back('^'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '(': out_cps.push_back('['); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case ')': out_cps.push_back(']'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '!': out_cps.push_back('|'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '<': out_cps.push_back('{'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '>': out_cps.push_back('}'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				case '-': out_cps.push_back('~'); out_spans.push_back({spans[i].begin, spans[i + 2].end}); i += 3; continue;
				default: break;
				}
			}

			out_cps.push_back(cps[i]);
			out_spans.push_back(spans[i]);
			++i;
		}

		cps.swap(out_cps);
		spans.swap(out_spans);
	}

	void splice_lines(vector<int>& cps, vector<Span>& spans)
	{
		vector<int> out_cps;
		vector<Span> out_spans;
		out_cps.reserve(cps.size());
		out_spans.reserve(spans.size());

		size_t i = 0;
		while (i < cps.size())
		{
			if (i + 1 < cps.size() && cps[i] == '\\' && cps[i + 1] == '\n')
			{
				i += 2;
				continue;
			}

			out_cps.push_back(cps[i]);
			out_spans.push_back(spans[i]);
			++i;
		}

		cps.swap(out_cps);
		spans.swap(out_spans);
	}

	void replace_ucn(vector<int>& cps, vector<Span>& spans)
	{
		vector<int> out_cps;
		vector<Span> out_spans;
		out_cps.reserve(cps.size());
		out_spans.reserve(spans.size());

		size_t i = 0;
		while (i < cps.size())
		{
			if (cps[i] == '\\' && i + 1 < cps.size() && (cps[i + 1] == 'u' || cps[i + 1] == 'U'))
			{
				const bool short_form = cps[i + 1] == 'u';
				const size_t ndigits = short_form ? 4 : 8;

				if (i + 2 + ndigits > cps.size())
					throw runtime_error("invalid universal-character-name");

				int cp = 0;
				for (size_t k = 0; k < ndigits; ++k)
				{
					int ch = cps[i + 2 + k];
					if (!IsHexDigit(ch))
						throw runtime_error("invalid universal-character-name");
					cp = (cp << 4) | HexCharToValue(ch);
				}

				if (cp > 0x10FFFF)
					throw runtime_error("invalid universal-character-name");
				if (cp >= 0xD800 && cp <= 0xDFFF)
					throw runtime_error("invalid universal-character-name");
				if (cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60)
					throw runtime_error("invalid universal-character-name");

				out_cps.push_back(cp);
				out_spans.push_back({spans[i].begin, spans[i + 1 + ndigits].end});
				i += 2 + ndigits;
				continue;
			}

			out_cps.push_back(cps[i]);
			out_spans.push_back(spans[i]);
			++i;
		}

		cps.swap(out_cps);
		spans.swap(out_spans);
	}

	string encode_range(const vector<int>& cps, size_t b, size_t e)
	{
		string s;
		for (size_t i = b; i < e; ++i)
			s += EncodeCodePointUtf8(cps[i]);
		return s;
	}

	string encode_one(int cp)
	{
		return EncodeCodePointUtf8(cp);
	}

	string encode_original_range(const vector<int>& original_cps, const vector<Span>& spans, size_t b, size_t e)
	{
		if (b >= e)
			return "";

		size_t ob = spans[b].begin;
		size_t oe = spans[e - 1].end;
		return encode_range(original_cps, ob, oe);
	}

	void validate_escape_sequence(const vector<int>& cps, size_t& i)
	{
		if (i + 1 >= cps.size())
			throw runtime_error("invalid escape sequence");

		int c1 = cps[i + 1];
		if (SimpleEscapeSequence_CodePoints.count(c1))
		{
			i += 2;
			return;
		}

		if (c1 == 'x')
		{
			i += 2;
			if (i >= cps.size() || !IsHexDigit(cps[i]))
				throw runtime_error("invalid hex escape sequence");

			while (i < cps.size() && IsHexDigit(cps[i]))
				++i;

			return;
		}

		if (IsOctDigit(c1))
		{
			i += 2;
			int consumed = 1;
			while (consumed < 3 && i < cps.size() && IsOctDigit(cps[i]))
			{
				++i;
				++consumed;
			}
			return;
		}

		throw runtime_error("invalid escape sequence");
	}

	bool try_parse_identifier(const vector<int>& cps, size_t& i, string& data)
	{
		if (i >= cps.size() || !IsIdentifierStart(cps[i]))
			return false;

		size_t j = i + 1;
		while (j < cps.size() && IsIdentifierContinue(cps[j]))
			++j;

		data = encode_range(cps, i, j);
		i = j;
		return true;
	}

	bool try_parse_ud_suffix(const vector<int>& cps, size_t& i)
	{
		if (i >= cps.size() || !IsIdentifierStart(cps[i]))
			return false;

		++i;
		while (i < cps.size() && IsIdentifierContinue(cps[i]))
			++i;

		return true;
	}

	bool try_parse_pp_number(const vector<int>& cps, size_t& i, string& data)
	{
		if (i >= cps.size())
			return false;

		if (!(IsDigit(cps[i]) || (cps[i] == '.' && i + 1 < cps.size() && IsDigit(cps[i + 1]))))
			return false;

		size_t j = i + 1;
		while (j < cps.size())
		{
			int c = cps[j];
			if (c == '.' || IsIdentifierContinue(c) || IsDigit(c))
			{
				++j;
				continue;
			}

			if ((c == '+' || c == '-') && j > i)
			{
				int p = cps[j - 1];
				if (p == 'e' || p == 'E' || p == 'p' || p == 'P')
				{
					++j;
					continue;
				}
			}

			break;
		}

		data = encode_range(cps, i, j);
		i = j;
		return true;
	}

	bool try_parse_operator_or_punc(const vector<int>& cps, size_t& i, string& data)
	{
		static const vector<string> ops =
		{
			"%:%:", ">>=", "<<=", "...", "->*", "##",
			"<:", ":>", "<%", "%>", "%:", "::", ".*",
			"+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
			"<<", ">>", "==", "!=", "<=", ">=", "&&", "||",
			"++", "--", "->",
			"{", "}", "[", "]", "#", "(", ")", ";", ":", "?",
			".", "+", "-", "*", "/", "%", "^", "&", "|", "~",
			"!", "=", "<", ">", ","
		};

		for (const string& op : ops)
		{
			if (!StartsWith(cps, i, op))
				continue;

			if (op == "<:" && i + 2 < cps.size() && cps[i + 2] == ':')
			{
				if (i + 3 >= cps.size() || (cps[i + 3] != ':' && cps[i + 3] != '>'))
					continue;
			}

			data = op;
			i += op.size();
			return true;
		}

		return false;
	}

	bool is_valid_raw_delim_char(int cp)
	{
		if (cp == ' ' || cp == '(' || cp == ')' || cp == '\\')
			return false;
		if (cp == '\t' || cp == '\v' || cp == '\f' || cp == '\n' || cp == '\r')
			return false;
		if (cp < 0x20 || cp > 0x7E)
			return false;
		return true;
	}

	bool try_parse_string_or_character_literal(
		const vector<int>& cps,
		const vector<Span>& spans,
		const vector<int>& original_cps,
		size_t& i,
		string& data,
		bool& is_string,
		bool& is_ud)
	{
		if (i >= cps.size())
			return false;

		size_t start = i;
		size_t j = i;
		bool raw = false;
		int quote = 0;

		if (StartsWith(cps, i, "u8R\""))
		{
			raw = true;
			is_string = true;
			j += 3;
			quote = '"';
		}
		else if (StartsWith(cps, i, "uR\"") || StartsWith(cps, i, "UR\"") || StartsWith(cps, i, "LR\""))
		{
			raw = true;
			is_string = true;
			j += 2;
			quote = '"';
		}
		else if (StartsWith(cps, i, "R\""))
		{
			raw = true;
			is_string = true;
			j += 1;
			quote = '"';
		}
		else if (StartsWith(cps, i, "u8\""))
		{
			is_string = true;
			j += 2;
			quote = '"';
		}
		else if (StartsWith(cps, i, "u\"") || StartsWith(cps, i, "U\"") || StartsWith(cps, i, "L\"") || StartsWith(cps, i, "\""))
		{
			is_string = true;
			if (cps[i] == '"')
				j += 0;
			else
				j += 1;
			quote = '"';
		}
		else if (StartsWith(cps, i, "u\'") || StartsWith(cps, i, "U\'") || StartsWith(cps, i, "L\'"))
		{
			is_string = false;
			j += 1;
			quote = '\'';
		}
		else if (StartsWith(cps, i, "\'"))
		{
			is_string = false;
			j += 0;
			quote = '\'';
		}
		else
		{
			return false;
		}

		if (raw)
		{
			const size_t orig_start = spans[start].begin;
			size_t oj = orig_start;

			if (StartsWith(original_cps, oj, "u8R\""))
				oj += 3;
			else if (StartsWith(original_cps, oj, "uR\"") || StartsWith(original_cps, oj, "UR\"") || StartsWith(original_cps, oj, "LR\""))
				oj += 2;
			else if (StartsWith(original_cps, oj, "R\""))
				oj += 1;
			else
				return false;

			if (oj >= original_cps.size() || original_cps[oj] != '"')
				throw runtime_error("unterminated raw string literal");

			++oj;
			size_t delim_begin = oj;
			while (oj < original_cps.size() && original_cps[oj] != '(')
			{
				if (!is_valid_raw_delim_char(original_cps[oj]))
					throw runtime_error("invalid raw string delimiter");
				++oj;
				if (oj - delim_begin > 16)
					throw runtime_error("raw string delimiter too long");
			}

			if (oj >= original_cps.size() || original_cps[oj] != '(')
				throw runtime_error("unterminated raw string literal");

			const size_t delim_len = oj - delim_begin;
			++oj;
			size_t ok = oj;
			for (;;)
			{
				if (ok >= original_cps.size())
					throw runtime_error("unterminated raw string literal");

				if (original_cps[ok] != ')')
				{
					++ok;
					continue;
				}

				if (ok + 1 + delim_len >= original_cps.size())
				{
					++ok;
					continue;
				}

				bool delim_ok = true;
				for (size_t d = 0; d < delim_len; ++d)
					if (original_cps[ok + 1 + d] != original_cps[delim_begin + d])
						delim_ok = false;

				if (!delim_ok || original_cps[ok + 1 + delim_len] != '"')
				{
					++ok;
					continue;
				}

				const size_t orig_end = ok + 1 + delim_len + 1;
				size_t end = start;
				while (end < spans.size() && spans[end].begin < orig_end)
					++end;

				string raw_data = encode_range(original_cps, orig_start, orig_end);
				i = end;
				is_ud = try_parse_ud_suffix(cps, i);
				if (is_ud)
					data = raw_data + encode_range(cps, end, i);
				else
					data = raw_data;
				return true;
			}
		}

		if (j >= cps.size() || cps[j] != quote)
			return false;

		++j;
		while (j < cps.size())
		{
			if (cps[j] == '\n')
				throw runtime_error(is_string ? "unterminated string literal" : "unterminated character literal");

			if (cps[j] == quote)
			{
				++j;
				break;
			}

			if (cps[j] == '\\')
			{
				validate_escape_sequence(cps, j);
				continue;
			}

			++j;
		}

		if (j > cps.size() || (j == cps.size() && cps[j - 1] != quote))
			throw runtime_error(is_string ? "unterminated string literal" : "unterminated character literal");

		data = encode_range(cps, start, j);
		i = j;

		is_ud = try_parse_ud_suffix(cps, i);
		if (is_ud)
			data = encode_range(cps, start, i);

		return true;
	}

	bool try_parse_header_name(const vector<int>& cps, size_t& i, string& data)
	{
		if (i >= cps.size())
			return false;

		if (cps[i] == '<')
		{
			size_t j = i + 1;
			while (j < cps.size() && cps[j] != '\n' && cps[j] != '>')
				++j;
			if (j >= cps.size() || cps[j] != '>')
				throw runtime_error("unterminated header-name");
			++j;
			data = encode_range(cps, i, j);
			i = j;
			return true;
		}

		if (cps[i] == '"')
		{
			size_t j = i + 1;
			while (j < cps.size() && cps[j] != '\n' && cps[j] != '"')
				++j;
			if (j >= cps.size() || cps[j] != '"')
				throw runtime_error("unterminated header-name");
			++j;
			data = encode_range(cps, i, j);
			i = j;
			return true;
		}

		return false;
	}

	void update_directive_state_after_non_ws_token(bool& at_line_start, DirectiveState& state, const string& kind, const string& data)
	{
		const bool is_hash = kind == "op" && (data == "#" || data == "%:");
		const bool is_identifier = kind == "identifier";

		if (at_line_start)
		{
			at_line_start = false;
			state = is_hash ? DS_AFTER_HASH : DS_NONE;
			return;
		}

		if (state == DS_AFTER_HASH)
		{
			if (is_identifier && data == "include")
				state = DS_AFTER_INCLUDE;
			else
				state = DS_NONE;
			return;
		}

		if (state == DS_AFTER_INCLUDE)
			state = DS_NONE;
	}

	void lex(const vector<int>& cps, const vector<Span>& spans, const vector<int>& original_cps)
	{
		size_t i = 0;
		bool at_line_start = true;
		DirectiveState directive_state = DS_NONE;

		while (i < cps.size())
		{
			if (cps[i] == '\n')
			{
				output.emit_new_line();
				++i;
				at_line_start = true;
				directive_state = DS_NONE;
				continue;
			}

			bool saw_ws = false;
			while (i < cps.size())
			{
				if (cps[i] == '\n')
					break;

				if (IsWhitespaceNotNewline(cps[i]))
				{
					saw_ws = true;
					++i;
					continue;
				}

				if (i + 1 < cps.size() && cps[i] == '/' && cps[i + 1] == '/')
				{
					saw_ws = true;
					i += 2;
					while (i < cps.size() && cps[i] != '\n')
						++i;
					continue;
				}

				if (i + 1 < cps.size() && cps[i] == '/' && cps[i + 1] == '*')
				{
					saw_ws = true;
					i += 2;
					bool closed = false;
					while (i + 1 < cps.size())
					{
						if (cps[i] == '*' && cps[i + 1] == '/')
						{
							closed = true;
							i += 2;
							break;
						}
						++i;
					}

					if (!closed)
						throw runtime_error("partial comment");

					continue;
				}

				break;
			}

			if (saw_ws)
			{
				output.emit_whitespace_sequence();
				continue;
			}

			if (directive_state == DS_AFTER_INCLUDE)
			{
				string header;
				size_t j = i;
				if (try_parse_header_name(cps, j, header))
				{
					output.emit_header_name(header);
					i = j;
					directive_state = DS_NONE;
					at_line_start = false;
					continue;
				}
			}

			{
				string literal;
				bool is_string = false;
				bool is_ud = false;
				size_t j = i;
				if (try_parse_string_or_character_literal(cps, spans, original_cps, j, literal, is_string, is_ud))
				{
					if (is_string)
					{
						if (is_ud)
							output.emit_user_defined_string_literal(literal);
						else
							output.emit_string_literal(literal);
					}
					else
					{
						if (is_ud)
							output.emit_user_defined_character_literal(literal);
						else
							output.emit_character_literal(literal);
					}

					i = j;
					update_directive_state_after_non_ws_token(at_line_start, directive_state, is_string ? (is_ud ? "ud-string" : "string") : (is_ud ? "ud-char" : "char"), literal);
					continue;
				}
			}

			{
				string ppn;
				size_t j = i;
				if (try_parse_pp_number(cps, j, ppn))
				{
					output.emit_pp_number(ppn);
					i = j;
					update_directive_state_after_non_ws_token(at_line_start, directive_state, "pp-number", ppn);
					continue;
				}
			}

			{
				string ident;
				size_t j = i;
				if (try_parse_identifier(cps, j, ident))
				{
					if (Digraph_IdentifierLike_Operators.count(ident))
					{
						output.emit_preprocessing_op_or_punc(ident);
						update_directive_state_after_non_ws_token(at_line_start, directive_state, "op", ident);
					}
					else
					{
						output.emit_identifier(ident);
						update_directive_state_after_non_ws_token(at_line_start, directive_state, "identifier", ident);
					}
					i = j;
					continue;
				}
			}

			{
				string op;
				size_t j = i;
				if (try_parse_operator_or_punc(cps, j, op))
				{
					output.emit_preprocessing_op_or_punc(op);
					i = j;
					update_directive_state_after_non_ws_token(at_line_start, directive_state, "op", op);
					continue;
				}
			}

			string other = encode_one(cps[i]);
			output.emit_non_whitespace_char(other);
			++i;
			update_directive_state_after_non_ws_token(at_line_start, directive_state, "other", other);
		}
	}
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		string input = oss.str();

		DebugPPTokenStream output;

		PPTokenizer tokenizer(output);

		for (char c : input)
		{
			unsigned char code_unit = c;
			tokenizer.process(code_unit);
		}

		tokenizer.process(EndOfFile);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
