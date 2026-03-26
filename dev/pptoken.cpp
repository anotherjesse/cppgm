#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"

// EndOfFile: synthetic "character" to represent the end of source file
constexpr int EndOfFile = -1;
constexpr int RawMarkerBase = 0x110000;

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

const vector<string> PreprocessingOpsSortedByLength =
{
	"%:%:", ">>=", "<<=", "->*", "...", "##",
	"<:",":>","<%","%>","%:","::",".*","+=","-=","*=","/=","%=","^=","&=","|=","<<",">>","==","!=","<=",">=","&&","||","++","--","->",
	"{","}","[","]","#","(",")",";",":","?",".","+","-","*","/","%","^","&","|","~","!","=","<",">",","
};

struct Token
{
	string type;
	string data;
};

bool IsRawMarker(int cp)
{
	return cp >= RawMarkerBase;
}

bool IsAsciiLetter(int cp)
{
	return ('a' <= cp && cp <= 'z') || ('A' <= cp && cp <= 'Z');
}

bool IsAsciiDigit(int cp)
{
	return '0' <= cp && cp <= '9';
}

bool InRanges(const vector<pair<int, int>>& ranges, int cp)
{
	size_t lo = 0;
	size_t hi = ranges.size();
	while (lo < hi)
	{
		size_t mid = lo + (hi - lo) / 2;
		if (cp < ranges[mid].first)
		{
			hi = mid;
		}
		else if (cp > ranges[mid].second)
		{
			lo = mid + 1;
		}
		else
		{
			return true;
		}
	}
	return false;
}

bool IsIdentifierNondigit(int cp)
{
	if (cp == '_' || IsAsciiLetter(cp))
	{
		return true;
	}
	return InRanges(AnnexE1_Allowed_RangesSorted, cp);
}

bool IsIdentifierStart(int cp)
{
	if (!IsIdentifierNondigit(cp))
	{
		return false;
	}
	return !InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
}

bool IsIdentifierContinue(int cp)
{
	return IsIdentifierNondigit(cp) || IsAsciiDigit(cp) || InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
}

bool IsWhitespaceNonNewline(int cp)
{
	return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f' || cp == '\r';
}

string EncodeUTF8One(int cp)
{
	string out;
	if (cp <= 0x7F)
	{
		out.push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FF)
	{
		out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 0) & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 0) & 0x3F)));
	}
	else
	{
		out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 0) & 0x3F)));
	}
	return out;
}

string EncodeUTF8(const vector<int>& cps, size_t begin, size_t end)
{
	string out;
	for (size_t i = begin; i < end; i++)
	{
		if (IsRawMarker(cps[i]))
		{
			throw runtime_error("internal error: marker in UTF-8 encode");
		}
		out += EncodeUTF8One(cps[i]);
	}
	return out;
}

vector<int> DecodeUTF8(const string& input)
{
	vector<int> out;
	for (size_t i = 0; i < input.size();)
	{
		unsigned char b0 = static_cast<unsigned char>(input[i]);
		if ((b0 & 0x80) == 0)
		{
			out.push_back(static_cast<int>(b0));
			i++;
			continue;
		}

		if ((b0 & 0xE0) == 0xC0)
		{
			if (i + 1 >= input.size()) throw runtime_error("utf8 truncated sequence");
			unsigned char b1 = static_cast<unsigned char>(input[i + 1]);
			if ((b1 & 0xC0) != 0x80) throw runtime_error("utf8 invalid continuation");
			int cp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
			if (cp < 0x80) throw runtime_error("utf8 overlong sequence");
			out.push_back(cp);
			i += 2;
			continue;
		}

		if ((b0 & 0xF0) == 0xE0)
		{
			if (i + 2 >= input.size()) throw runtime_error("utf8 truncated sequence");
			unsigned char b1 = static_cast<unsigned char>(input[i + 1]);
			unsigned char b2 = static_cast<unsigned char>(input[i + 2]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) throw runtime_error("utf8 invalid continuation");
			int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
			if (cp < 0x800) throw runtime_error("utf8 overlong sequence");
			if (0xD800 <= cp && cp <= 0xDFFF) throw runtime_error("utf8 surrogate code point");
			out.push_back(cp);
			i += 3;
			continue;
		}

		if ((b0 & 0xF8) == 0xF0)
		{
			if (i + 3 >= input.size()) throw runtime_error("utf8 truncated sequence");
			unsigned char b1 = static_cast<unsigned char>(input[i + 1]);
			unsigned char b2 = static_cast<unsigned char>(input[i + 2]);
			unsigned char b3 = static_cast<unsigned char>(input[i + 3]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) throw runtime_error("utf8 invalid continuation");
			int cp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
			if (cp < 0x10000) throw runtime_error("utf8 overlong sequence");
			if (cp > 0x10FFFF) throw runtime_error("utf8 out of range");
			out.push_back(cp);
			i += 4;
			continue;
		}

		if (b0 == 0xFF)
		{
			throw runtime_error("utf8 invalid unit (111111xx)");
		}
		if ((b0 & 0xC0) == 0x80)
		{
			throw runtime_error("utf8 invalid unit (10xxxxxx)");
		}
		throw runtime_error("utf8 invalid unit");
	}
	return out;
}

bool IsHexCp(int cp)
{
	return ('0' <= cp && cp <= '9') || ('a' <= cp && cp <= 'f') || ('A' <= cp && cp <= 'F');
}

size_t ConsumeEscapeSequence(const vector<int>& cps, size_t i)
{
	if (i >= cps.size() || IsRawMarker(cps[i]))
	{
		throw runtime_error("unterminated string literal");
	}

	int c = cps[i];
	const unordered_set<int> simple =
	{
		'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
	};
	if (simple.count(c) != 0)
	{
		return 1;
	}

	if ('0' <= c && c <= '7')
	{
		size_t n = 1;
		while (n < 3 && i + n < cps.size() && !IsRawMarker(cps[i + n]) && ('0' <= cps[i + n] && cps[i + n] <= '7'))
		{
			n++;
		}
		return n;
	}

	if (c == 'x')
	{
		size_t j = i + 1;
		if (j >= cps.size() || IsRawMarker(cps[j]) || !IsHexCp(cps[j]))
		{
			throw runtime_error("invalid hex escape sequence");
		}
		while (j < cps.size() && !IsRawMarker(cps[j]) && IsHexCp(cps[j]))
		{
			j++;
		}
		return j - i;
	}

	if (c == 'u' || c == 'U')
	{
		size_t digits = (c == 'u') ? 4 : 8;
		if (i + 1 + digits > cps.size())
		{
			throw runtime_error("invalid universal-character-name");
		}
		for (size_t k = 0; k < digits; k++)
		{
			if (IsRawMarker(cps[i + 1 + k]) || !IsHexCp(cps[i + 1 + k]))
			{
				throw runtime_error("invalid universal-character-name");
			}
		}
		return 1 + digits;
	}

	if (c == '8' || c == '9')
	{
		throw runtime_error("invalid escape sequence");
	}

	throw runtime_error("invalid escape sequence");
}

int DecodeUCNAt(const vector<int>& cps, size_t i, size_t digits)
{
	int value = 0;
	for (size_t j = 0; j < digits; j++)
	{
		value = (value << 4) | HexCharToValue(cps[i + j]);
	}
	if (value > 0x10FFFF)
	{
		throw runtime_error("universal-character-name out of range");
	}
	if (0xD800 <= value && value <= 0xDFFF)
	{
		throw runtime_error("universal-character-name is surrogate");
	}
	return value;
}

pair<size_t, vector<int>> TryParseRawString(const vector<int>& cps, size_t i)
{
	size_t n = cps.size();
	size_t j = i;

	if (j + 1 < n && cps[j] == 'u' && cps[j + 1] == '8')
	{
		j += 2;
	}
	else if (j < n && (cps[j] == 'u' || cps[j] == 'U' || cps[j] == 'L'))
	{
		j++;
	}

	if (!(j + 1 < n && cps[j] == 'R' && cps[j + 1] == '"'))
	{
		return {0, {}};
	}
	j += 2;

	vector<int> delim;
	while (j < n && cps[j] != '(')
	{
		int cp = cps[j];
		if (cp == '\\' || cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == ')')
		{
			return {0, {}};
		}
		delim.push_back(cp);
		if (delim.size() > 16)
		{
			throw runtime_error("raw string delimiter too long");
		}
		j++;
	}
	if (j >= n || cps[j] != '(')
	{
		return {0, {}};
	}
	j++;

	while (j < n)
	{
		if (cps[j] == ')')
		{
			bool match = true;
			for (size_t k = 0; k < delim.size(); k++)
			{
				if (j + 1 + k >= n || cps[j + 1 + k] != delim[k])
				{
					match = false;
					break;
				}
			}
			if (match)
			{
				size_t quote_pos = j + 1 + delim.size();
				if (quote_pos < n && cps[quote_pos] == '"')
				{
					vector<int> whole;
					whole.insert(whole.end(), cps.begin() + static_cast<long>(i), cps.begin() + static_cast<long>(quote_pos + 1));
					return {quote_pos + 1 - i, whole};
				}
			}
		}
		j++;
	}

	throw runtime_error("unterminated raw string literal");
}

vector<int> MarkRawStringLiterals(const vector<int>& cps, vector<string>& raw_literals)
{
	auto consume_non_raw_string_or_char = [&](size_t i) -> size_t
	{
		size_t n = cps.size();
		size_t j = i;
		size_t prefix_len = 0;
		if (j + 1 < n && cps[j] == 'u' && cps[j + 1] == '8')
		{
			prefix_len = 2;
		}
		else if (j < n && (cps[j] == 'u' || cps[j] == 'U' || cps[j] == 'L'))
		{
			prefix_len = 1;
		}
		j += prefix_len;
		if (j >= n || (cps[j] != '"' && cps[j] != '\''))
		{
			return i;
		}
		int quote = cps[j];
		if (quote == '\'' && prefix_len == 2)
		{
			return i;
		}
		j++;
		while (j < n)
		{
			if (cps[j] == '\n')
			{
				return i;
			}
			if (cps[j] == '\\')
			{
				j += 2;
				continue;
			}
			if (cps[j] == quote)
			{
				j++;
				break;
			}
			j++;
		}
		if (j > n)
		{
			return i;
		}
		if (j >= n || cps[j - 1] != quote)
		{
			return i;
		}
		if (j < n && IsIdentifierStart(cps[j]))
		{
			j++;
			while (j < n && IsIdentifierContinue(cps[j]))
			{
				j++;
			}
		}
		return j;
	};

	vector<int> out;
	for (size_t i = 0; i < cps.size();)
	{
		size_t non_raw_end = consume_non_raw_string_or_char(i);
		if (non_raw_end > i)
		{
			out.insert(out.end(), cps.begin() + static_cast<long>(i), cps.begin() + static_cast<long>(non_raw_end));
			i = non_raw_end;
			continue;
		}

		auto parsed = TryParseRawString(cps, i);
		if (parsed.first == 0)
		{
			out.push_back(cps[i]);
			i++;
			continue;
		}
		raw_literals.push_back(EncodeUTF8(parsed.second, 0, parsed.second.size()));
		out.push_back(RawMarkerBase + static_cast<int>(raw_literals.size() - 1));
		i += parsed.first;
	}
	return out;
}

vector<int> ReplaceTrigraphs(const vector<int>& cps)
{
	unordered_map<int, int> tri =
	{
		{'=', '#'}, {'/', '\\'}, {'\'', '^'}, {'(', '['}, {')', ']'}, {'!', '|'}, {'<', '{'}, {'>', '}'}, {'-', '~'}
	};

	vector<int> out;
	for (size_t i = 0; i < cps.size();)
	{
		if (IsRawMarker(cps[i]))
		{
			out.push_back(cps[i]);
			i++;
			continue;
		}
		if (i + 2 < cps.size() && !IsRawMarker(cps[i + 1]) && !IsRawMarker(cps[i + 2]) && cps[i] == '?' && cps[i + 1] == '?')
		{
			auto it = tri.find(cps[i + 2]);
			if (it != tri.end())
			{
				out.push_back(it->second);
				i += 3;
				continue;
			}
		}
		out.push_back(cps[i]);
		i++;
	}
	return out;
}

vector<int> SpliceLines(const vector<int>& cps)
{
	vector<int> out;
	for (size_t i = 0; i < cps.size();)
	{
		if (IsRawMarker(cps[i]))
		{
			out.push_back(cps[i]);
			i++;
			continue;
		}
		if (cps[i] == '\\' && i + 1 < cps.size() && !IsRawMarker(cps[i + 1]) && cps[i + 1] == '\n')
		{
			i += 2;
			continue;
		}
		out.push_back(cps[i]);
		i++;
	}
	return out;
}

vector<int> ReplaceUCN(const vector<int>& cps)
{
	vector<int> out;
	for (size_t i = 0; i < cps.size();)
	{
		if (IsRawMarker(cps[i]))
		{
			out.push_back(cps[i]);
			i++;
			continue;
		}

		if (cps[i] == '\\' && i + 1 < cps.size() && !IsRawMarker(cps[i + 1]) && (cps[i + 1] == 'u' || cps[i + 1] == 'U'))
		{
			size_t digits = (cps[i + 1] == 'u') ? 4 : 8;
			if (i + 2 + digits <= cps.size())
			{
				bool ok = true;
				for (size_t j = 0; j < digits; j++)
				{
					if (IsRawMarker(cps[i + 2 + j]) || !IsHexCp(cps[i + 2 + j]))
					{
						ok = false;
						break;
					}
				}
				if (ok)
				{
					out.push_back(DecodeUCNAt(cps, i + 2, digits));
					i += 2 + digits;
					continue;
				}
			}
		}

		out.push_back(cps[i]);
		i++;
	}
	return out;
}

vector<int> ReplaceComments(const vector<int>& cps)
{
	auto consume_non_raw_string_or_char = [&](size_t i) -> size_t
	{
		size_t n = cps.size();
		size_t j = i;
		size_t prefix_len = 0;
		if (j + 1 < n && cps[j] == 'u' && cps[j + 1] == '8')
		{
			prefix_len = 2;
		}
		else if (j < n && (cps[j] == 'u' || cps[j] == 'U' || cps[j] == 'L'))
		{
			prefix_len = 1;
		}
		j += prefix_len;
		if (j >= n || (cps[j] != '"' && cps[j] != '\''))
		{
			return i;
		}
		int quote = cps[j];
		if (quote == '\'' && prefix_len == 2)
		{
			return i;
		}
		j++;
		while (j < n)
		{
			if (cps[j] == '\n')
			{
				return i;
			}
			if (cps[j] == '\\')
			{
				j += 2;
				continue;
			}
			if (cps[j] == quote)
			{
				return j + 1;
			}
			j++;
		}
		return i;
	};

	vector<int> out;
	for (size_t i = 0; i < cps.size();)
	{
		if (IsRawMarker(cps[i]))
		{
			out.push_back(cps[i]);
			i++;
			continue;
		}

		size_t literal_end = consume_non_raw_string_or_char(i);
		if (literal_end > i)
		{
			out.insert(out.end(), cps.begin() + static_cast<long>(i), cps.begin() + static_cast<long>(literal_end));
			i = literal_end;
			continue;
		}

		if (cps[i] == '/' && i + 1 < cps.size() && !IsRawMarker(cps[i + 1]))
		{
			if (cps[i + 1] == '*')
			{
				i += 2;
				bool closed = false;
				while (i + 1 < cps.size())
				{
					if (!IsRawMarker(cps[i]) && !IsRawMarker(cps[i + 1]) && cps[i] == '*' && cps[i + 1] == '/')
					{
						i += 2;
						closed = true;
						break;
					}
					i++;
				}
				if (!closed)
				{
					throw runtime_error("partial comment");
				}
				out.push_back(' ');
				continue;
			}

			if (cps[i + 1] == '/')
			{
				i += 2;
				while (i < cps.size() && (IsRawMarker(cps[i]) || cps[i] != '\n'))
				{
					i++;
				}
				out.push_back(' ');
				continue;
			}
		}

		out.push_back(cps[i]);
		i++;
	}
	return out;
}

size_t ConsumeIdentifier(const vector<int>& cps, size_t i)
{
	if (i >= cps.size() || IsRawMarker(cps[i]) || !IsIdentifierStart(cps[i]))
	{
		return i;
	}
	i++;
	while (i < cps.size() && !IsRawMarker(cps[i]) && IsIdentifierContinue(cps[i]))
	{
		i++;
	}
	return i;
}

size_t ConsumePPNumber(const vector<int>& cps, size_t i)
{
	if (i >= cps.size() || IsRawMarker(cps[i]))
	{
		return i;
	}
	size_t j = i;
	if (IsAsciiDigit(cps[j]))
	{
		j++;
	}
	else if (cps[j] == '.' && j + 1 < cps.size() && !IsRawMarker(cps[j + 1]) && IsAsciiDigit(cps[j + 1]))
	{
		j += 2;
	}
	else
	{
		return i;
	}

	while (j < cps.size() && !IsRawMarker(cps[j]))
	{
		if ((cps[j] == 'e' || cps[j] == 'E' || cps[j] == 'p' || cps[j] == 'P') && j + 1 < cps.size() && !IsRawMarker(cps[j + 1]) && (cps[j + 1] == '+' || cps[j + 1] == '-'))
		{
			j += 2;
			continue;
		}
		if (IsIdentifierContinue(cps[j]) || cps[j] == '.')
		{
			j++;
			continue;
		}
		break;
	}

	return j;
}

bool StartsWithOperatorWord(const vector<int>& cps, size_t i, const string& word)
{
	if (i + word.size() > cps.size())
	{
		return false;
	}
	for (size_t k = 0; k < word.size(); k++)
	{
		if (IsRawMarker(cps[i + k]) || cps[i + k] != word[k])
		{
			return false;
		}
	}
	if (i + word.size() < cps.size() && !IsRawMarker(cps[i + word.size()]) && IsIdentifierContinue(cps[i + word.size()]))
	{
		return false;
	}
	return true;
}

size_t ConsumeStringOrCharLiteral(const vector<int>& cps, size_t i, bool& is_string)
{
	size_t n = cps.size();
	size_t j = i;
	is_string = false;
	size_t prefix_len = 0;

	if (j < n && !IsRawMarker(cps[j]) && cps[j] == 'u' && j + 1 < n && !IsRawMarker(cps[j + 1]) && cps[j + 1] == '8')
	{
		prefix_len = 2;
	}
	else if (j < n && !IsRawMarker(cps[j]) && (cps[j] == 'u' || cps[j] == 'U' || cps[j] == 'L'))
	{
		prefix_len = 1;
	}

	j = i + prefix_len;
	if (j >= n || IsRawMarker(cps[j]) || (cps[j] != '\'' && cps[j] != '"'))
	{
		return i;
	}

	int quote = cps[j];
	if (quote == '\'' && prefix_len == 2)
	{
		return i;
	}
	is_string = (quote == '"');
	j++;
	while (j < n)
	{
		if (IsRawMarker(cps[j]))
		{
			break;
		}
		if (cps[j] == '\n')
		{
			throw runtime_error("unterminated string literal");
		}
		if (cps[j] == '\\')
		{
			j += 1 + ConsumeEscapeSequence(cps, j + 1);
			continue;
		}
		if (cps[j] == quote)
		{
			j++;
			return j;
		}
		j++;
	}

	throw runtime_error("unterminated string literal");
}

size_t ConsumeHeaderName(const vector<int>& cps, size_t i)
{
	size_t n = cps.size();
	if (i >= n || IsRawMarker(cps[i]))
	{
		return i;
	}
	if (cps[i] == '<')
	{
		size_t j = i + 1;
		while (j < n && !IsRawMarker(cps[j]) && cps[j] != '>' && cps[j] != '\n')
		{
			j++;
		}
		if (j < n && !IsRawMarker(cps[j]) && cps[j] == '>')
		{
			return j + 1;
		}
		return i;
	}
	if (cps[i] == '"')
	{
		size_t j = i + 1;
		while (j < n && !IsRawMarker(cps[j]) && cps[j] != '"' && cps[j] != '\n')
		{
			j++;
		}
		if (j < n && !IsRawMarker(cps[j]) && cps[j] == '"')
		{
			return j + 1;
		}
		return i;
	}
	return i;
}

string MarkerLiteralData(const vector<string>& raw_literals, int marker)
{
	int idx = marker - RawMarkerBase;
	if (idx < 0 || static_cast<size_t>(idx) >= raw_literals.size())
	{
		throw runtime_error("internal error: invalid raw marker");
	}
	return raw_literals[idx];
}

// Tokenizer
struct PPTokenizer
{
	IPPTokenStream& output;
	vector<int> input_cps;

	PPTokenizer(IPPTokenStream& output)
		: output(output)
	{}

	void process(int c)
	{
		if (c != EndOfFile)
		{
			input_cps.push_back(c);
			return;
		}

		vector<string> raw_literals;
		vector<int> cps = MarkRawStringLiterals(input_cps, raw_literals);
		cps = ReplaceTrigraphs(cps);
		cps = SpliceLines(cps);
		cps = ReplaceUCN(cps);
		cps = ReplaceComments(cps);

		if (!cps.empty() && cps.back() != '\n')
		{
			cps.push_back('\n');
		}

		bool at_line_start = true;
		bool expect_include = false;
		bool in_include = false;

		for (size_t i = 0; i < cps.size();)
		{
			if (IsRawMarker(cps[i]))
			{
				string lit = MarkerLiteralData(raw_literals, cps[i]);
				size_t j = i + 1;
				size_t suffix_end = ConsumeIdentifier(cps, j);
				if (suffix_end > j)
				{
					lit += EncodeUTF8(cps, j, suffix_end);
					output.emit_user_defined_string_literal(lit);
					i = suffix_end;
				}
				else
				{
					output.emit_string_literal(lit);
					i++;
				}
				at_line_start = false;
				expect_include = false;
				in_include = false;
				continue;
			}

			if (cps[i] == '\n')
			{
				output.emit_new_line();
				i++;
				at_line_start = true;
				expect_include = false;
				in_include = false;
				continue;
			}

			if (IsWhitespaceNonNewline(cps[i]))
			{
				while (i < cps.size() && !IsRawMarker(cps[i]) && IsWhitespaceNonNewline(cps[i]))
				{
					i++;
				}
				output.emit_whitespace_sequence();
				continue;
			}

			if (in_include)
			{
				size_t h = ConsumeHeaderName(cps, i);
				if (h > i)
				{
					output.emit_header_name(EncodeUTF8(cps, i, h));
					i = h;
					in_include = false;
					at_line_start = false;
					continue;
				}
				in_include = false;
			}

			bool is_string = false;
			size_t lit_end = ConsumeStringOrCharLiteral(cps, i, is_string);
			if (lit_end > i)
			{
				size_t suffix_end = ConsumeIdentifier(cps, lit_end);
				string data = EncodeUTF8(cps, i, lit_end);
				if (suffix_end > lit_end)
				{
					data += EncodeUTF8(cps, lit_end, suffix_end);
					if (is_string)
					{
						output.emit_user_defined_string_literal(data);
					}
					else
					{
						output.emit_user_defined_character_literal(data);
					}
					i = suffix_end;
				}
				else
				{
					if (is_string)
					{
						output.emit_string_literal(data);
					}
					else
					{
						output.emit_character_literal(data);
					}
					i = lit_end;
				}
				at_line_start = false;
				expect_include = false;
				in_include = false;
				continue;
			}

			size_t num_end = ConsumePPNumber(cps, i);
			if (num_end > i)
			{
				output.emit_pp_number(EncodeUTF8(cps, i, num_end));
				i = num_end;
				at_line_start = false;
				expect_include = false;
				in_include = false;
				continue;
			}

			size_t id_end = ConsumeIdentifier(cps, i);
			if (id_end > i)
			{
				string id = EncodeUTF8(cps, i, id_end);
				if (Digraph_IdentifierLike_Operators.count(id) != 0)
				{
					output.emit_preprocessing_op_or_punc(id);
				}
				else
				{
					output.emit_identifier(id);
					if (expect_include && id == "include")
					{
						in_include = true;
					}
				}
				i = id_end;
				at_line_start = false;
				expect_include = false;
				continue;
			}

			if (i + 3 < cps.size() && cps[i] == '<' && cps[i + 1] == ':' && cps[i + 2] == ':' && cps[i + 3] != ':' && cps[i + 3] != '>')
			{
				output.emit_preprocessing_op_or_punc("<");
				i += 1;
				if (at_line_start)
				{
					expect_include = false;
				}
				at_line_start = false;
				in_include = false;
				continue;
			}

			bool matched_op = false;
			for (const string& op : PreprocessingOpsSortedByLength)
			{
				if (i + op.size() > cps.size())
				{
					continue;
				}
				bool ok = true;
				for (size_t k = 0; k < op.size(); k++)
				{
					if (IsRawMarker(cps[i + k]) || cps[i + k] != op[k])
					{
						ok = false;
						break;
					}
				}
				if (!ok)
				{
					continue;
				}
				output.emit_preprocessing_op_or_punc(op);
				if (at_line_start && (op == "#" || op == "%:"))
				{
					expect_include = true;
				}
				else
				{
					expect_include = false;
				}
				at_line_start = false;
				in_include = false;
				i += op.size();
				matched_op = true;
				break;
			}
			if (matched_op)
			{
				continue;
			}

			output.emit_non_whitespace_char(EncodeUTF8One(cps[i]));
			i++;
			at_line_start = false;
			expect_include = false;
			in_include = false;
		}

		output.emit_eof();
	}
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		string input = oss.str();
		vector<int> cps = DecodeUTF8(input);

		DebugPPTokenStream output;
		PPTokenizer tokenizer(output);

		for (int cp : cps)
		{
			tokenizer.process(cp);
		}

		tokenizer.process(EndOfFile);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
