#include <algorithm>
#include <cstdint>
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

const unordered_set<string> Digraph_IdentifierLike_Operators =
{
	"new", "delete", "and", "and_eq", "bitand",
	"bitor", "compl", "not", "not_eq", "or",
	"or_eq", "xor", "xor_eq"
};

const unordered_set<int> SimpleEscapeSequence_CodePoints =
{
	'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
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

bool IsAsciiWhitespaceNoNewLine(int cp)
{
	return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f';
}

bool IsIdentifierStart(int cp)
{
	if (cp == '_' || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))
		return true;
	return InRanges(AnnexE1_Allowed_RangesSorted, cp) &&
		!InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
}

bool IsIdentifierContinue(int cp)
{
	if (IsIdentifierStart(cp) || (cp >= '0' && cp <= '9'))
		return true;
	return InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
}

bool IsHexDigit(int cp)
{
	return (cp >= '0' && cp <= '9') ||
		(cp >= 'a' && cp <= 'f') ||
		(cp >= 'A' && cp <= 'F');
}

bool IsRawStringDelimiterChar(int cp)
{
	if (cp == ' ' || cp == '(' || cp == ')' || cp == '\\')
		return false;
	if (cp == '\t' || cp == '\v' || cp == '\f' || cp == '\n' || cp == '\r')
		return false;
	return cp >= 0 && cp <= 0x7f;
}

string EncodeUtf8(int cp)
{
	if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		throw logic_error("invalid unicode code point");

	string out;
	if (cp <= 0x7F)
	{
		out.push_back(static_cast<char>(cp));
	}
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

struct Unit
{
	int cp;
	string text;
	size_t next;

	Unit(int cp = EndOfFile, string text = "", size_t next = 0)
		: cp(cp), text(move(text)), next(next)
	{}
};

struct RawStringResult
{
	bool ok;
	string token;
	size_t next;
	bool user_defined;

	RawStringResult(bool ok = false, string token = "", size_t next = 0, bool user_defined = false)
		: ok(ok), token(move(token)), next(next), user_defined(user_defined)
	{}
};

struct Scanner
{
	IPPTokenStream& output;
	string input;
	size_t pos = 0;
	bool start_of_line = true;
	bool after_hash = false;
	bool after_include = false;

	Scanner(IPPTokenStream& output, string input)
		: output(output), input(move(input))
	{
		if (!this->input.empty() && this->input.back() != '\n')
			this->input.push_back('\n');
	}

	Unit ReadPhase1Unit(size_t at) const
	{
		if (at >= input.size())
			return {EndOfFile, "", at};

		static const unordered_map<char, char> trigraphs =
		{
			{'=', '#'},
			{'/', '\\'},
			{'\'', '^'},
			{'(', '['},
			{')', ']'},
			{'!', '|'},
			{'<', '{'},
			{'>', '}'},
			{'-', '~'}
		};

		if (at + 2 < input.size() && input[at] == '?' && input[at + 1] == '?')
		{
			auto it = trigraphs.find(input[at + 2]);
			if (it != trigraphs.end())
			{
				return {static_cast<unsigned char>(it->second), string(1, it->second), at + 3};
			}
		}

		unsigned char lead = static_cast<unsigned char>(input[at]);
		if (lead <= 0x7F)
			return {lead, string(1, static_cast<char>(lead)), at + 1};

		int length = 0;
		int cp = 0;
		if ((lead & 0xE0) == 0xC0)
		{
			length = 2;
			cp = lead & 0x1F;
		}
		else if ((lead & 0xF0) == 0xE0)
		{
			length = 3;
			cp = lead & 0x0F;
		}
		else if ((lead & 0xF8) == 0xF0)
		{
			length = 4;
			cp = lead & 0x07;
		}
		else if ((lead & 0xFC) == 0xF8)
		{
			throw logic_error("utf8 invalid unit (111110xx)");
		}
		else if ((lead & 0xFE) == 0xFC)
		{
			throw logic_error("utf8 invalid unit (1111110x)");
		}
		else
		{
			throw logic_error("utf8 invalid unit (111111xx)");
		}

		if (at + length > input.size())
			throw logic_error("utf8 truncated sequence");

		for (int i = 1; i < length; ++i)
		{
			unsigned char cont = static_cast<unsigned char>(input[at + i]);
			if ((cont & 0xC0) != 0x80)
				throw logic_error("utf8 invalid continuation unit");
			cp = (cp << 6) | (cont & 0x3F);
		}

		if ((length == 2 && cp < 0x80) ||
			(length == 3 && cp < 0x800) ||
			(length == 4 && cp < 0x10000) ||
			cp > 0x10FFFF ||
			(cp >= 0xD800 && cp <= 0xDFFF))
		{
			throw logic_error("utf8 invalid code point");
		}

		return {cp, input.substr(at, length), at + static_cast<size_t>(length)};
	}

	Unit ReadNormalUnit(size_t at) const
	{
		size_t cur = at;
		while (true)
		{
			Unit first = ReadPhase1Unit(cur);
			if (first.cp == EndOfFile)
				return first;

			if (first.cp == '\\')
			{
				Unit second = ReadPhase1Unit(first.next);
				if (second.cp == '\n')
				{
					cur = second.next;
					continue;
				}

				if (second.cp == 'u' || second.cp == 'U')
				{
					int digits = second.cp == 'u' ? 4 : 8;
					size_t walk = second.next;
					int value = 0;
					for (int i = 0; i < digits; ++i)
					{
						Unit hex = ReadPhase1Unit(walk);
						if (hex.cp == EndOfFile || hex.text.size() != 1 || !IsHexDigit(hex.cp))
							throw logic_error("invalid universal-character-name");
						value = (value << 4) | HexCharToValue(hex.cp);
						walk = hex.next;
					}
					return {value, EncodeUtf8(value), walk};
				}
			}

			return first;
		}
	}

	Unit Peek() const
	{
		return ReadNormalUnit(pos);
	}

	Unit Peek2() const
	{
		Unit first = ReadNormalUnit(pos);
		if (first.cp == EndOfFile)
			return first;
		return ReadNormalUnit(first.next);
	}

	Unit Take()
	{
		Unit unit = ReadNormalUnit(pos);
		pos = unit.next;
		return unit;
	}

	void EmitNewLine()
	{
		output.emit_new_line();
		start_of_line = true;
		after_hash = false;
		after_include = false;
	}

	void NoteToken(const string& token, bool is_identifier, bool is_header_name = false)
	{
		bool was_start_of_line = start_of_line;
		start_of_line = false;

		if ((token == "#" || token == "%:") && was_start_of_line)
		{
			after_hash = true;
			after_include = false;
			return;
		}

		if (after_hash && is_identifier && token == "include")
		{
			after_hash = false;
			after_include = true;
			return;
		}

		after_hash = false;
		if (!is_header_name)
			after_include = false;
	}

	void ConsumeWhitespaceRun()
	{
		bool emitted = false;
		while (true)
		{
			Unit first = Peek();
			if (first.cp == EndOfFile)
				return;

			Unit second = first.cp != EndOfFile ? ReadNormalUnit(first.next) : first;

			if (IsAsciiWhitespaceNoNewLine(first.cp))
			{
				if (!emitted)
				{
					output.emit_whitespace_sequence();
					emitted = true;
				}
				while (IsAsciiWhitespaceNoNewLine(Peek().cp))
					Take();
				continue;
			}

			if (first.cp == '/' && second.cp == '/')
			{
				if (!emitted)
				{
					output.emit_whitespace_sequence();
					emitted = true;
				}
				Take();
				Take();
				while (true)
				{
					Unit ch = Peek();
					if (ch.cp == EndOfFile || ch.cp == '\n')
						break;
					Take();
				}
				continue;
			}

			if (first.cp == '/' && second.cp == '*')
			{
				if (!emitted)
				{
					output.emit_whitespace_sequence();
					emitted = true;
				}
				Take();
				Take();
				while (true)
				{
					Unit ch = Peek();
					if (ch.cp == EndOfFile)
						throw logic_error("partial comment");

					Unit next = ReadNormalUnit(ch.next);
					if (ch.cp == '*' && next.cp == '/')
					{
						Take();
						Take();
						break;
					}

					Take();
				}
				continue;
			}

			return;
		}
	}

	string ReadIdentifier()
	{
		string token;
		Unit first = Peek();
		if (!IsIdentifierStart(first.cp))
			return token;

		token += Take().text;
		while (IsIdentifierContinue(Peek().cp))
			token += Take().text;
		return token;
	}

	string ReadSuffix()
	{
		string suffix;
		if (!IsIdentifierStart(Peek().cp))
			return suffix;

		suffix += Take().text;
		while (IsIdentifierContinue(Peek().cp))
			suffix += Take().text;
		return suffix;
	}

	void EmitIdentifierLike(const string& token)
	{
		if (Digraph_IdentifierLike_Operators.count(token) != 0)
			output.emit_preprocessing_op_or_punc(token);
		else
			output.emit_identifier(token);

		NoteToken(token, true);
	}

	void ParseIdentifierOrOperatorWord()
	{
		string token = ReadIdentifier();
		EmitIdentifierLike(token);
	}

	void ParsePPNumber()
	{
		string token;
		token += Take().text;
		while (true)
		{
			Unit next = Peek();
			if ((next.cp >= '0' && next.cp <= '9') ||
				(next.cp >= 'A' && next.cp <= 'Z') ||
				(next.cp >= 'a' && next.cp <= 'z') ||
				next.cp == '_' ||
				next.cp == '.')
			{
				token += Take().text;
				continue;
			}

			if ((next.cp == '+' || next.cp == '-') && !token.empty())
			{
				char last = token.back();
				if (last == 'e' || last == 'E' || last == 'p' || last == 'P')
				{
					token += Take().text;
					continue;
				}
			}

			break;
		}

		output.emit_pp_number(token);
		NoteToken(token, false);
	}

	bool ParseQuotedLiteral(char quote, const string& prefix, bool is_string)
	{
		vector<Unit> prefix_units;
		size_t walk = pos;
		for (char expected : prefix)
		{
			Unit u = ReadNormalUnit(walk);
			if (u.cp != expected)
				return false;
			prefix_units.push_back(u);
			walk = u.next;
		}

		Unit q = ReadNormalUnit(walk);
		if (q.cp != quote)
			return false;

		string token;
		for (const Unit& u : prefix_units)
			token += u.text;
		token += q.text;
		pos = q.next;

		while (true)
		{
			Unit ch = Peek();
			if (ch.cp == EndOfFile || ch.cp == '\n')
				throw logic_error("unterminated string literal");

			if (ch.cp == quote)
			{
				token += Take().text;
				break;
			}

			if (ch.cp == '\\')
			{
				token += Take().text;
				Unit esc = Peek();
				if (esc.cp == EndOfFile || esc.cp == '\n')
					throw logic_error("unterminated string literal");

				token += Take().text;
				if (esc.cp == 'x')
				{
					if (!IsHexDigit(Peek().cp))
						throw logic_error("invalid hex escape sequence");
					while (IsHexDigit(Peek().cp))
						token += Take().text;
				}
				else if (esc.cp >= '0' && esc.cp <= '7')
				{
					int count = 0;
					while (count < 2 && Peek().cp >= '0' && Peek().cp <= '7')
					{
						token += Take().text;
						++count;
					}
				}
				else if (SimpleEscapeSequence_CodePoints.count(esc.cp) == 0)
				{
					throw logic_error("invalid escape sequence");
				}

				continue;
			}

			token += Take().text;
		}

		string suffix = ReadSuffix();
		if (!suffix.empty())
			token += suffix;

		if (is_string)
		{
			if (suffix.empty())
				output.emit_string_literal(token);
			else
				output.emit_user_defined_string_literal(token);
		}
		else
		{
			if (suffix.empty())
				output.emit_character_literal(token);
			else
				output.emit_user_defined_character_literal(token);
		}

		NoteToken(token, false);
		return true;
	}

	RawStringResult TryReadRawString(size_t at, const string& prefix) const
	{
		if (input.compare(at, prefix.size(), prefix) != 0)
			return {};

		size_t walk = at + prefix.size();
		if (walk + 1 >= input.size() || input[walk] != 'R' || input[walk + 1] != '"')
			return {};

		walk += 2;
		string delimiter;
		while (walk < input.size())
		{
			char c = input[walk];
			if (c == '(')
				break;
			if (!IsRawStringDelimiterChar(static_cast<unsigned char>(c)))
				return {};
			delimiter.push_back(c);
			++walk;
			if (delimiter.size() > 16)
				throw logic_error("raw string delimiter too long");
		}

		if (walk >= input.size() || input[walk] != '(')
			throw logic_error("unterminated raw string literal");
		++walk;

		size_t content = walk;
		string closing = ")" + delimiter + "\"";
		while (true)
		{
			size_t found = input.find(closing, content);
			if (found == string::npos)
				throw logic_error("unterminated raw string literal");

			string token = input.substr(at, found + closing.size() - at);
			size_t next = found + closing.size();

			size_t suffix_pos = next;
			if (suffix_pos < input.size())
			{
				unsigned char c = static_cast<unsigned char>(input[suffix_pos]);
				if (c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
				{
					++suffix_pos;
					while (suffix_pos < input.size())
					{
						unsigned char d = static_cast<unsigned char>(input[suffix_pos]);
						if (d == '_' || (d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z') || (d >= '0' && d <= '9'))
							++suffix_pos;
						else
							break;
					}
				}
			}

			if (suffix_pos > next)
			{
				token += input.substr(next, suffix_pos - next);
				return {true, token, suffix_pos, true};
			}

			return {true, token, next, false};
		}
	}

	bool ParseRawString()
	{
		vector<string> prefixes = {"u8", "u", "U", "L", ""};
		for (const string& prefix : prefixes)
		{
			RawStringResult result = TryReadRawString(pos, prefix);
			if (!result.ok)
				continue;

			pos = result.next;
			if (result.user_defined)
				output.emit_user_defined_string_literal(result.token);
			else
				output.emit_string_literal(result.token);
			NoteToken(result.token, false);
			return true;
		}
		return false;
	}

	void ParseHeaderName()
	{
		if (Peek().cp == '"')
		{
			string token;
			token += Take().text;
			while (true)
			{
				Unit ch = Peek();
				if (ch.cp == EndOfFile || ch.cp == '\n')
					throw logic_error("unterminated string literal");
				token += Take().text;
				if (ch.cp == '"')
					break;
			}
			output.emit_header_name(token);
			NoteToken(token, false, true);
			after_include = false;
			return;
		}

		if (Peek().cp == '<')
		{
			string token;
			token += Take().text;
			while (true)
			{
				Unit ch = Peek();
				if (ch.cp == EndOfFile || ch.cp == '\n')
					throw logic_error("unterminated header name");
				token += Take().text;
				if (ch.cp == '>')
					break;
			}
			output.emit_header_name(token);
			NoteToken(token, false, true);
			after_include = false;
		}
	}

	void ParseOperatorOrFallback()
	{
		static const vector<string> ops =
		{
			"%:%:", ">>=", "<<=", "->*", "...", "##",
			".*", "->", "::", "++", "--", "<<", ">>",
			"<=", ">=", "==", "!=", "&&", "||",
			"+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
			"<:", ":>", "<%", "%>", "%:",
			"{", "}", "[", "]", "#", "(", ")", ";", ":", "?",
			".", "+", "-", "*", "/", "%", "^", "&", "|", "~",
			"!", "=", "<", ">", ","
		};

		vector<Unit> lookahead;
		size_t walk = pos;
		for (int i = 0; i < 4; ++i)
		{
			Unit u = ReadNormalUnit(walk);
			lookahead.push_back(u);
			if (u.cp == EndOfFile)
				break;
			walk = u.next;
		}

		string ascii;
		for (const Unit& u : lookahead)
		{
			if (u.cp < 0 || u.cp > 0x7f)
				break;
			ascii.push_back(static_cast<char>(u.cp));
		}

		string matched;
		if (ascii.rfind("<::", 0) == 0 && !(ascii.size() >= 4 && (ascii[3] == ':' || ascii[3] == '>')))
		{
			matched = "<";
		}
		else
		{
			for (const string& op : ops)
			{
				if (ascii.rfind(op, 0) == 0)
				{
					matched = op;
					break;
				}
			}
		}

		if (!matched.empty())
		{
			for (size_t i = 0; i < matched.size(); ++i)
				Take();
			output.emit_preprocessing_op_or_punc(matched);
			NoteToken(matched, false);
			return;
		}

		Unit ch = Take();
		output.emit_non_whitespace_char(ch.text);
		NoteToken(ch.text, false);
	}

	void Run()
	{
		while (true)
		{
			Unit ch = Peek();
			if (ch.cp == EndOfFile)
			{
				output.emit_eof();
				return;
			}

			if (ch.cp == '\n')
			{
				Take();
				EmitNewLine();
				continue;
			}

			Unit next = Peek2();
			if (IsAsciiWhitespaceNoNewLine(ch.cp) ||
				(ch.cp == '/' && (next.cp == '/' || next.cp == '*')))
			{
				ConsumeWhitespaceRun();
				continue;
			}

			if (after_include && (ch.cp == '<' || ch.cp == '"'))
			{
				ParseHeaderName();
				continue;
			}

			if (ParseRawString())
				continue;

			if (ParseQuotedLiteral('\'', "u", false) ||
				ParseQuotedLiteral('\'', "U", false) ||
				ParseQuotedLiteral('\'', "L", false) ||
				ParseQuotedLiteral('\'', "", false))
			{
				continue;
			}

			if (ParseQuotedLiteral('"', "u8", true) ||
				ParseQuotedLiteral('"', "u", true) ||
				ParseQuotedLiteral('"', "U", true) ||
				ParseQuotedLiteral('"', "L", true) ||
				ParseQuotedLiteral('"', "", true))
			{
				continue;
			}

			if (IsIdentifierStart(ch.cp))
			{
				ParseIdentifierOrOperatorWord();
				continue;
			}

			if ((ch.cp >= '0' && ch.cp <= '9') ||
				(ch.cp == '.' && next.cp >= '0' && next.cp <= '9'))
			{
				ParsePPNumber();
				continue;
			}

			ParseOperatorOrFallback();
		}
	}
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		DebugPPTokenStream output;
		Scanner scanner(output, oss.str());
		scanner.Run();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
