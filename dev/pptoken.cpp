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

// Translation features you need to implement:
// - utf8 decoder
// - utf8 encoder
// - universal-character-name decoder
// - trigraphs
// - line splicing
// - newline at eof
// - comment striping (can be part of whitespace-sequence)

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

// Tokenizer
struct PPTokenizer
{
	IPPTokenStream& output;
	string raw_input;
	bool finished;
	vector<int> source;
	bool line_start;
	bool directive_start;
	bool include_pending;

	enum class PendingToken
	{
		Whitespace,
		NewLine
	};

	enum class TokenKind
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

	struct Scanner
	{
		const vector<int>* source;
		size_t pos;
		bool synthetic_newline_available;
		bool synthetic_newline_emitted;

		Scanner(const vector<int>& source)
			: source(&source), pos(0),
			  synthetic_newline_available(!source.empty() && source.back() != '\n'),
			  synthetic_newline_emitted(false)
		{}

		int peek(size_t n) const
		{
			Scanner copy(*this);

			int c = EndOfFile;
			for (size_t i = 0; i <= n; ++i)
			{
				c = copy.next_normal();
			}

			return c;
		}

		int next_normal()
		{
			while (true)
			{
				if (pos >= source->size())
				{
					if (synthetic_newline_available && !synthetic_newline_emitted)
					{
						synthetic_newline_emitted = true;
						return '\n';
					}

					return EndOfFile;
				}

				int c;
				if (pos + 2 < source->size())
				{
					int a = (*source)[pos + 0];
					int b = (*source)[pos + 1];
					int d = (*source)[pos + 2];

					if (a == '?' && b == '?')
					{
						switch (d)
						{
						case '=': c = '#'; pos += 3; break;
						case '/': c = '\\'; pos += 3; break;
						case '\'': c = '^'; pos += 3; break;
						case '(': c = '['; pos += 3; break;
						case ')': c = ']'; pos += 3; break;
						case '!': c = '|'; pos += 3; break;
						case '<': c = '{'; pos += 3; break;
						case '>': c = '}'; pos += 3; break;
						case '-': c = '~'; pos += 3; break;
						default:
							c = (*source)[pos++];
							break;
						}
					}
					else
					{
						c = (*source)[pos++];
					}
				}
				else
				{
					c = (*source)[pos++];
				}

				if (c == '\\')
				{
					int ucn = 0;
					size_t consumed = 0;
					if (TryDecodeUCN(*source, pos, ucn, consumed))
					{
						c = ucn;
						pos += consumed;
					}
				}

				if (c == '\\' && pos < source->size() && (*source)[pos] == '\n')
				{
					++pos;
					continue;
				}

				return c;
			}
		}
	};

	PPTokenizer(IPPTokenStream& output)
		: output(output), finished(false), line_start(true),
		  directive_start(false), include_pending(false)
	{}

	void process(int c)
	{
		if (c == EndOfFile)
		{
			if (finished)
				return;

			finished = true;
			run();
			output.emit_eof();
			return;
		}

		raw_input.push_back((char)c);
	}

	void run()
	{
		source = decode_utf8(raw_input);
		Scanner sc(source);

		while (true)
		{
			if (scan_whitespace_run(sc))
				continue;

			if (sc.peek(0) == EndOfFile)
				break;

			if (include_pending)
			{
				if (sc.peek(0) == '<' || sc.peek(0) == '"')
				{
					scan_header_name(sc);
					continue;
				}

				directive_start = false;
				include_pending = false;
			}

			if (scan_raw_string_literal(sc))
				continue;
			if (scan_string_literal(sc))
				continue;
			if (scan_character_literal(sc))
				continue;
			if (scan_identifier_or_operator(sc))
				continue;
			if (scan_pp_number(sc))
				continue;
			if (scan_preprocessing_op_or_punc(sc))
				continue;

			scan_non_whitespace_character(sc);
		}
	}

	static bool IsIdentifierStartCodePoint(int cp)
	{
		if (cp == '_' || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))
			return true;

		return InRanges(AnnexE1_Allowed_RangesSorted, cp)
			&& !InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
	}

	static bool IsIdentifierContinueCodePoint(int cp)
	{
		if (IsIdentifierStartCodePoint(cp) || (cp >= '0' && cp <= '9'))
			return true;

		return InRanges(AnnexE2_DisallowedInitially_RangesSorted, cp);
	}

	static bool IsWhitespaceNoNewline(int cp)
	{
		return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f';
	}

	static bool IsHexDigit(int cp)
	{
		return (cp >= '0' && cp <= '9')
			|| (cp >= 'a' && cp <= 'f')
			|| (cp >= 'A' && cp <= 'F');
	}

	static bool IsOctDigit(int cp)
	{
		return cp >= '0' && cp <= '7';
	}

	static bool IsSimpleEscapeCodePoint(int cp)
	{
		return SimpleEscapeSequence_CodePoints.count(cp) != 0;
	}

	static bool InRanges(const vector<pair<int, int>>& ranges, int cp)
	{
		for (size_t i = 0; i < ranges.size(); ++i)
		{
			const pair<int, int>& r = ranges[i];
			if (cp < r.first)
				return false;
			if (cp <= r.second)
				return true;
		}

		return false;
	}

	static void AppendUTF8(string& s, int cp)
	{
		if (cp < 0)
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
		else if (cp <= 0x10FFFF)
		{
			s.push_back((char) (0xF0 | ((cp >> 18) & 0x07)));
			s.push_back((char) (0x80 | ((cp >> 12) & 0x3F)));
			s.push_back((char) (0x80 | ((cp >> 6) & 0x3F)));
			s.push_back((char) (0x80 | ((cp >> 0) & 0x3F)));
		}
		else
		{
			throw logic_error("invalid code point");
		}
	}

	static bool TryDecodeUCN(const vector<int>& source, size_t pos, int& cp, size_t& consumed)
	{
		consumed = 0;

		if (pos >= source.size() || (source[pos] != 'u' && source[pos] != 'U'))
			return false;

		bool long_form = source[pos] == 'U';
		size_t digits = long_form ? 8 : 4;
		if (pos + 1 + digits > source.size())
			return false;

		cp = 0;
		for (size_t i = 0; i < digits; ++i)
		{
			int c = source[pos + 1 + i];
			if (!IsHexDigit(c))
				return false;
			cp = (cp << 4) + HexCharToValue(c);
		}

		if (cp > 0x10FFFF)
			throw logic_error("invalid code point");

		consumed = 1 + digits;
		return true;
	}

	static bool IsRawStringDelimiterChar(int cp)
	{
		if (cp == ' ' || cp == '(' || cp == ')' || cp == '\\' || cp == '\n'
			|| cp == '\t' || cp == '\v' || cp == '\f')
		{
			return false;
		}

		return cp >= 0x20 && cp != 0x7F;
	}

	static bool IsOperatorWord(const string& s)
	{
		return Digraph_IdentifierLike_Operators.count(s) != 0;
	}

	static bool Matches(Scanner sc, const string& s)
	{
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (sc.next_normal() != (unsigned char) s[i])
				return false;
		}

		return true;
	}

	bool scan_whitespace_run(Scanner& sc)
	{
		int c = sc.peek(0);
		if (c == EndOfFile || (!IsWhitespaceNoNewline(c) && c != '\n' && !(c == '/' && (sc.peek(1) == '/' || sc.peek(1) == '*'))))
			return false;

		vector<PendingToken> pending;
		bool segment_has_nonnewline = false;

		while (true)
		{
			c = sc.peek(0);
			if (c == EndOfFile)
				break;

			if (IsWhitespaceNoNewline(c))
			{
				sc.next_normal();
				segment_has_nonnewline = true;
				continue;
			}

			if (c == '/' && sc.peek(1) == '/')
			{
				sc.next_normal();
				sc.next_normal();
				segment_has_nonnewline = true;

				while (true)
				{
					c = sc.peek(0);
					if (c == EndOfFile || c == '\n')
						break;
					sc.next_normal();
					segment_has_nonnewline = true;
				}

				continue;
			}

			if (c == '/' && sc.peek(1) == '*')
			{
				sc.next_normal();
				sc.next_normal();
				segment_has_nonnewline = true;

				while (true)
				{
					c = sc.peek(0);
					if (c == EndOfFile)
						throw logic_error("partial comment");

					if (c == '*' && sc.peek(1) == '/')
					{
						sc.next_normal();
						sc.next_normal();
						segment_has_nonnewline = true;
						break;
					}

					sc.next_normal();
					segment_has_nonnewline = true;
				}

				continue;
			}

			if (c == '\n')
			{
				if (segment_has_nonnewline)
				{
					pending.push_back(PendingToken::Whitespace);
					segment_has_nonnewline = false;
				}

				pending.push_back(PendingToken::NewLine);
				sc.next_normal();
				emit_pending(pending);
				return true;
			}

			break;
		}

		if (segment_has_nonnewline)
			pending.push_back(PendingToken::Whitespace);

		emit_pending(pending);
		return !pending.empty();
	}

	void emit_pending(const vector<PendingToken>& pending)
	{
		for (size_t i = 0; i < pending.size(); ++i)
		{
			if (pending[i] == PendingToken::Whitespace)
			{
				emit_token(TokenKind::Whitespace, "");
			}
			else
			{
				emit_token(TokenKind::NewLine, "");
			}
		}
	}

	void emit_token(TokenKind kind, const string& data)
	{
		bool at_line_start = line_start;

		switch (kind)
		{
		case TokenKind::Whitespace:
			output.emit_whitespace_sequence();
			return;
		case TokenKind::NewLine:
			output.emit_new_line();
			line_start = true;
			directive_start = false;
			include_pending = false;
			return;
		case TokenKind::HeaderName:
			output.emit_header_name(data);
			break;
		case TokenKind::Identifier:
			output.emit_identifier(data);
			break;
		case TokenKind::PpNumber:
			output.emit_pp_number(data);
			break;
		case TokenKind::CharacterLiteral:
			output.emit_character_literal(data);
			break;
		case TokenKind::UserDefinedCharacterLiteral:
			output.emit_user_defined_character_literal(data);
			break;
		case TokenKind::StringLiteral:
			output.emit_string_literal(data);
			break;
		case TokenKind::UserDefinedStringLiteral:
			output.emit_user_defined_string_literal(data);
			break;
		case TokenKind::PreprocessingOpOrPunc:
			output.emit_preprocessing_op_or_punc(data);
			break;
		case TokenKind::NonWhitespaceCharacter:
			output.emit_non_whitespace_char(data);
			break;
		}

		line_start = false;
		if (kind == TokenKind::HeaderName)
		{
			directive_start = false;
			include_pending = false;
			return;
		}

		if (kind == TokenKind::Identifier && directive_start && data == "include")
		{
			include_pending = true;
			return;
		}

		if (kind == TokenKind::PreprocessingOpOrPunc && at_line_start && (data == "#" || data == "%:"))
		{
			directive_start = true;
			include_pending = false;
			return;
		}

		directive_start = false;
		include_pending = false;
	}

	void scan_header_name(Scanner& sc)
	{
		int opener = sc.peek(0);
		if (opener != '<' && opener != '"')
			throw logic_error("internal error");

		string data;
		int closer = opener == '<' ? '>' : '"';
		AppendUTF8(data, sc.next_normal());

		while (true)
		{
			int c = sc.peek(0);
			if (c == EndOfFile || c == '\n')
				throw logic_error("unterminated header name");

			AppendUTF8(data, sc.next_normal());
			if (c == closer)
				break;
		}

		emit_token(TokenKind::HeaderName, data);
	}

	bool scan_raw_string_literal(Scanner& sc)
	{
		size_t prefix_len = 0;
		if (Matches(sc, "u8R\""))
			prefix_len = 4;
		else if (Matches(sc, "uR\"") || Matches(sc, "UR\"") || Matches(sc, "LR\""))
			prefix_len = 3;
		else if (Matches(sc, "R\""))
			prefix_len = 2;
		else
			return false;

		string data;
		for (size_t i = 0; i < prefix_len; ++i)
			AppendUTF8(data, sc.source->at(sc.pos++));

		string delimiter;
		while (true)
		{
			if (sc.pos >= sc.source->size())
				throw logic_error("invalid characters in raw string delimiter");

			int c = sc.source->at(sc.pos);
			if (c == '(')
			{
				AppendUTF8(data, c);
				++sc.pos;
				break;
			}

			if (!IsRawStringDelimiterChar(c))
				throw logic_error("invalid characters in raw string delimiter");

			AppendUTF8(delimiter, c);
			AppendUTF8(data, c);
			++sc.pos;

			if (delimiter.size() > 16)
				throw logic_error("raw string delimiter too long");
		}

		while (true)
		{
			if (sc.pos >= sc.source->size())
				throw logic_error("unterminated raw string literal");

			int c = sc.source->at(sc.pos);
			if (c == ')' )
			{
				bool match = true;
				for (size_t i = 0; i < delimiter.size(); ++i)
				{
					if (sc.pos + 1 + i >= sc.source->size() || sc.source->at(sc.pos + 1 + i) != delimiter[i])
					{
						match = false;
						break;
					}
				}
				if (match && sc.pos + 1 + delimiter.size() < sc.source->size() && sc.source->at(sc.pos + 1 + delimiter.size()) == '"')
				{
					AppendUTF8(data, c);
					++sc.pos;
					for (size_t i = 0; i < delimiter.size(); ++i)
					{
						AppendUTF8(data, sc.source->at(sc.pos));
						++sc.pos;
					}
					AppendUTF8(data, '"');
					++sc.pos;
					break;
				}
			}

			AppendUTF8(data, c);
			++sc.pos;
		}

		if (IsIdentifierStartCodePoint(sc.peek(0)))
		{
			string suffix = consume_identifier_sequence(sc);
			data += suffix;
			emit_token(TokenKind::UserDefinedStringLiteral, data);
		}
		else
		{
			emit_token(TokenKind::StringLiteral, data);
		}

		return true;
	}

	bool scan_string_literal(Scanner& sc)
	{
		string prefix;
		if (Matches(sc, "u8\""))
			prefix = "u8";
		else if (Matches(sc, "u\""))
			prefix = "u";
		else if (Matches(sc, "U\""))
			prefix = "U";
		else if (Matches(sc, "L\""))
			prefix = "L";
		else if (sc.peek(0) == '"')
			prefix.clear();
		else
			return false;

		string data = prefix;
		if (!prefix.empty())
		{
			for (size_t i = 0; i < prefix.size(); ++i)
				sc.next_normal();
		}

		if (sc.peek(0) != '"')
			return false;

		AppendUTF8(data, sc.next_normal());

		while (true)
		{
			int c = sc.peek(0);
			if (c == EndOfFile || c == '\n')
				throw logic_error("unterminated string literal");

			c = sc.next_normal();
			AppendUTF8(data, c);

			if (c == '"')
				break;

			if (c == '\\')
			{
				int n = sc.peek(0);
				if (n == EndOfFile || n == '\n')
					throw logic_error("unterminated string literal");

				if (n == 'x')
				{
					AppendUTF8(data, sc.next_normal());
					int hex = sc.peek(0);
					if (!IsHexDigit(hex))
						throw logic_error("invalid hex escape sequence");
					while (IsHexDigit(sc.peek(0)))
						AppendUTF8(data, sc.next_normal());
				}
				else if (IsOctDigit(n))
				{
					AppendUTF8(data, sc.next_normal());
					int count = 1;
					while (count < 3 && IsOctDigit(sc.peek(0)))
					{
						AppendUTF8(data, sc.next_normal());
						++count;
					}
				}
				else if (IsSimpleEscapeCodePoint(n))
				{
					AppendUTF8(data, sc.next_normal());
				}
				else
				{
					throw logic_error("invalid escape sequence");
				}
			}
		}

		if (IsIdentifierStartCodePoint(sc.peek(0)))
		{
			string suffix = consume_identifier_sequence(sc);
			data += suffix;
			emit_token(TokenKind::UserDefinedStringLiteral, data);
		}
		else
		{
			emit_token(TokenKind::StringLiteral, data);
		}

		return true;
	}

	bool scan_character_literal(Scanner& sc)
	{
		string prefix;
		if (Matches(sc, "u'"))
			prefix = "u";
		else if (Matches(sc, "U'"))
			prefix = "U";
		else if (Matches(sc, "L'"))
			prefix = "L";
		else if (sc.peek(0) == '\'')
			prefix.clear();
		else
			return false;

		string data = prefix;
		if (!prefix.empty())
		{
			sc.next_normal();
		}

		if (sc.peek(0) != '\'')
			return false;

		AppendUTF8(data, sc.next_normal());

		while (true)
		{
			int c = sc.peek(0);
			if (c == EndOfFile || c == '\n')
				throw logic_error("unterminated character literal");

			c = sc.next_normal();
			AppendUTF8(data, c);

			if (c == '\'')
				break;

			if (c == '\\')
			{
				int n = sc.peek(0);
				if (n == EndOfFile || n == '\n')
					throw logic_error("unterminated character literal");

				if (n == 'x')
				{
					AppendUTF8(data, sc.next_normal());
					int hex = sc.peek(0);
					if (!IsHexDigit(hex))
						throw logic_error("invalid hex escape sequence");
					while (IsHexDigit(sc.peek(0)))
						AppendUTF8(data, sc.next_normal());
				}
				else if (IsOctDigit(n))
				{
					AppendUTF8(data, sc.next_normal());
					int count = 1;
					while (count < 3 && IsOctDigit(sc.peek(0)))
					{
						AppendUTF8(data, sc.next_normal());
						++count;
					}
				}
				else if (IsSimpleEscapeCodePoint(n))
				{
					AppendUTF8(data, sc.next_normal());
				}
				else
				{
					throw logic_error("invalid escape sequence");
				}
			}
		}

		if (IsIdentifierStartCodePoint(sc.peek(0)))
		{
			string suffix = consume_identifier_sequence(sc);
			data += suffix;
			emit_token(TokenKind::UserDefinedCharacterLiteral, data);
		}
		else
		{
			emit_token(TokenKind::CharacterLiteral, data);
		}

		return true;
	}

	bool scan_identifier_or_operator(Scanner& sc)
	{
		if (!IsIdentifierStartCodePoint(sc.peek(0)))
			return false;

		string data = consume_identifier_sequence(sc);
		if (IsOperatorWord(data))
			emit_token(TokenKind::PreprocessingOpOrPunc, data);
		else
			emit_token(TokenKind::Identifier, data);
		return true;
	}

	string consume_identifier_sequence(Scanner& sc)
	{
		string data;
		AppendUTF8(data, sc.next_normal());
		while (IsIdentifierContinueCodePoint(sc.peek(0)))
			AppendUTF8(data, sc.next_normal());
		return data;
	}

	bool scan_pp_number(Scanner& sc)
	{
		int c = sc.peek(0);
		if (!( (c >= '0' && c <= '9') || (c == '.' && (sc.peek(1) >= '0' && sc.peek(1) <= '9')) ))
			return false;

		string data;
		if (c == '.')
		{
			AppendUTF8(data, sc.next_normal());
			AppendUTF8(data, sc.next_normal());
		}
		else
		{
			AppendUTF8(data, sc.next_normal());
		}

		while (true)
		{
			int p0 = sc.peek(0);
			int p1 = sc.peek(1);

			if (p0 >= '0' && p0 <= '9')
			{
				AppendUTF8(data, sc.next_normal());
				continue;
			}

			if ((p0 == 'e' || p0 == 'E') && (p1 == '+' || p1 == '-'))
			{
				AppendUTF8(data, sc.next_normal());
				AppendUTF8(data, sc.next_normal());
				continue;
			}

			if (IsIdentifierContinueCodePoint(p0))
			{
				AppendUTF8(data, sc.next_normal());
				continue;
			}

			if (p0 == '.')
			{
				AppendUTF8(data, sc.next_normal());
				continue;
			}

			break;
		}

		emit_token(TokenKind::PpNumber, data);
		return true;
	}

	bool scan_preprocessing_op_or_punc(Scanner& sc)
	{
		if (sc.peek(0) == '<' && sc.peek(1) == ':' && sc.peek(2) == ':' && sc.peek(3) != ':' && sc.peek(3) != '>')
		{
			string data;
			AppendUTF8(data, sc.next_normal());
			emit_token(TokenKind::PreprocessingOpOrPunc, data);
			return true;
		}

		static const char* Candidates[] =
		{
			"%:%:", "->*", "<<=", ">>=", "<<", ">>", "...",
			"<:", ":>", "<%", "%>", "%:", "##", "::", ".*",
			"+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
			"==", "!=", "<=", ">=", "&&", "||", "++", "--", "->",
			"{", "}", "[", "]", "(", ")", ";", ":", "?", ".", "+", "-", "*", "/", "%", "^", "&", "|", "~", "!", "=", "<", ">", ",", "#"
		};

		for (size_t i = 0; i < sizeof(Candidates)/sizeof(Candidates[0]); ++i)
		{
			string candidate = Candidates[i];
			if (Matches(sc, candidate))
			{
				string data;
				for (size_t j = 0; j < candidate.size(); ++j)
					AppendUTF8(data, sc.next_normal());
				emit_token(TokenKind::PreprocessingOpOrPunc, data);
				return true;
			}
		}

		return false;
	}

	void scan_non_whitespace_character(Scanner& sc)
	{
		string data;
		AppendUTF8(data, sc.next_normal());
		emit_token(TokenKind::NonWhitespaceCharacter, data);
	}

	static bool Matches(Scanner sc, const char* s)
	{
		for (const char* p = s; *p; ++p)
		{
			if (sc.next_normal() != (unsigned char) *p)
				return false;
		}

		return true;
	}

	vector<int> decode_utf8(const string& input)
	{
		vector<int> output;

		for (size_t i = 0; i < input.size(); )
		{
			unsigned char c = (unsigned char) input[i];
			if (c < 0x80)
			{
				output.push_back(c);
				++i;
				continue;
			}

			if (c >= 0x80 && c <= 0xBF)
				throw logic_error("utf8 trailing code unit (10xxxxxx) at start");

			if (c >= 0xC0 && c <= 0xDF)
			{
				if (i + 1 >= input.size() || ((unsigned char) input[i + 1] & 0xC0) != 0x80)
					throw logic_error("utf8 expected trailing byte (10xxxxxx)");
				int cp = ((c & 0x1F) << 6)
					| (((unsigned char) input[i + 1]) & 0x3F);
				output.push_back(cp);
				i += 2;
				continue;
			}

			if (c >= 0xE0 && c <= 0xEF)
			{
				if (i + 2 >= input.size()
					|| ((unsigned char) input[i + 1] & 0xC0) != 0x80
					|| ((unsigned char) input[i + 2] & 0xC0) != 0x80)
				{
					throw logic_error("utf8 expected trailing byte (10xxxxxx)");
				}

				int cp = ((c & 0x0F) << 12)
					| ((((unsigned char) input[i + 1]) & 0x3F) << 6)
					| (((unsigned char) input[i + 2]) & 0x3F);
				output.push_back(cp);
				i += 3;
				continue;
			}

			if (c >= 0xF0 && c <= 0xF7)
			{
				if (i + 3 >= input.size()
					|| ((unsigned char) input[i + 1] & 0xC0) != 0x80
					|| ((unsigned char) input[i + 2] & 0xC0) != 0x80
					|| ((unsigned char) input[i + 3] & 0xC0) != 0x80)
				{
					throw logic_error("utf8 expected trailing byte (10xxxxxx)");
				}

				int cp = ((c & 0x07) << 18)
					| ((((unsigned char) input[i + 1]) & 0x3F) << 12)
					| ((((unsigned char) input[i + 2]) & 0x3F) << 6)
					| (((unsigned char) input[i + 3]) & 0x3F);

				if (cp > 0x10FFFF)
					throw logic_error("invalid code point");

				output.push_back(cp);
				i += 4;
				continue;
			}

			throw logic_error("utf8 invalid unit (111111xx)");
		}

		return output;
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
