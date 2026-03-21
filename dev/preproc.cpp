// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// For pragma once implementation:
// system-wide unique file id type `PA5FileId`
typedef pair<unsigned long int, unsigned long int> PA5FileId;

// bootstrap system call interface, used by PA5GetFileId
extern "C" long int syscall(long int n, ...) throw ();

// PA5GetFileId returns true iff file found at path `path`.
// out parameter `out_fileid` is set to file id
bool PA5GetFileId(const string& path, PA5FileId& out_fileid)
{
	struct
	{
		unsigned long int dev;
		unsigned long int ino;
		long int unused[16];
	} data;

	int res = syscall(4, path.c_str(), &data);

	out_fileid = make_pair(data.dev, data.ino);

	return res == 0;
}

// OPTIONAL: Also search `PA5StdIncPaths` on `--stdinc` command-line switch (not by default)
vector<string> PA5StdIncPaths =
{
	"/usr/include/c++/4.7/",
	"/usr/include/c++/4.7/x86_64-linux-gnu/",
	"/usr/include/c++/4.7/backward/",
	"/usr/lib/gcc/x86_64-linux-gnu/4.7/include/",
	"/usr/local/include/",
	"/usr/lib/gcc/x86_64-linux-gnu/4.7/include-fixed/",
	"/usr/include/x86_64-linux-gnu/",
	"/usr/include/"
};

#define CPPGM_MACRO_LIBRARY
#include "macro.cpp"

struct StrictPostTokenOutputStream : DebugPostTokenOutputStream
{
	explicit StrictPostTokenOutputStream(ostream& out_)
		: DebugPostTokenOutputStream(out_)
	{
	}

	void emit_invalid(const string& source)
	{
		throw runtime_error("invalid token: " + source);
	}
};

string ReadFileText(const string& path)
{
	ifstream in(path);
	if (!in)
		throw runtime_error("cannot open file: " + path);
	ostringstream oss;
	oss << in.rdbuf();
	return oss.str();
}

vector<long> ComputeLogicalLineStarts(const string& text)
{
	vector<long> starts(1, 1);
	long physical_line = 1;
	size_t i = 0;
	bool in_line_comment = false;
	bool in_block_comment = false;
	bool in_string = false;
	bool in_char = false;
	bool escape = false;

	while (i < text.size())
	{
		char c = text[i];
		char next = i + 1 < text.size() ? text[i + 1] : '\0';

		if ((in_string || in_char) && c == '\\' && next == '\n')
		{
			++physical_line;
			i += 2;
			continue;
		}

		if (!in_line_comment && !in_block_comment && !in_string && !in_char && c == '\\' && next == '\n')
		{
			++physical_line;
			i += 2;
			continue;
		}

		if (in_line_comment)
		{
			if (c == '\n')
			{
				++physical_line;
				starts.push_back(physical_line);
				in_line_comment = false;
			}
			++i;
			continue;
		}

		if (in_block_comment)
		{
			if (c == '*' && next == '/')
			{
				in_block_comment = false;
				i += 2;
				continue;
			}
			if (c == '\n')
				++physical_line;
			++i;
			continue;
		}

		if (in_string)
		{
			if (escape)
			{
				escape = false;
			}
			else if (c == '\\')
			{
				escape = true;
			}
			else if (c == '"')
			{
				in_string = false;
			}
			else if (c == '\n')
			{
				++physical_line;
				starts.push_back(physical_line);
			}
			++i;
			continue;
		}

		if (in_char)
		{
			if (escape)
			{
				escape = false;
			}
			else if (c == '\\')
			{
				escape = true;
			}
			else if (c == '\'')
			{
				in_char = false;
			}
			else if (c == '\n')
			{
				++physical_line;
				starts.push_back(physical_line);
			}
			++i;
			continue;
		}

		if (c == '/' && next == '/')
		{
			in_line_comment = true;
			i += 2;
			continue;
		}
		if (c == '/' && next == '*')
		{
			in_block_comment = true;
			i += 2;
			continue;
		}
		if (c == '"')
		{
			in_string = true;
			++i;
			continue;
		}
		if (c == '\'')
		{
			in_char = true;
			++i;
			continue;
		}
		if (c == '\n')
		{
			++physical_line;
			starts.push_back(physical_line);
		}
		++i;
	}

	return starts;
}

string EscapeStringLiteral(const string& s)
{
	string out = "\"";
	for (char c : s)
	{
		if (c == '\\' || c == '"')
			out.push_back('\\');
		out.push_back(c);
	}
	out.push_back('"');
	return out;
}

enum ExprTokenKind
{
	ET_END,
	ET_LITERAL,
	ET_IDENTIFIER,
	ET_LPAREN,
	ET_RPAREN,
	ET_QMARK,
	ET_COLON,
	ET_PLUS,
	ET_MINUS,
	ET_LNOT,
	ET_COMPL,
	ET_STAR,
	ET_DIV,
	ET_MOD,
	ET_LSHIFT,
	ET_RSHIFT,
	ET_LT,
	ET_GT,
	ET_LE,
	ET_GE,
	ET_EQ,
	ET_NE,
	ET_AMP,
	ET_XOR,
	ET_BOR,
	ET_LAND,
	ET_LOR
};

struct ExprToken
{
	ExprTokenKind kind = ET_END;
	long long value = 0;
	string text;
};

struct ExprParser
{
	explicit ExprParser(const vector<ExprToken>& tokens_, const map<string, bool>& defined_)
		: tokens(tokens_), defined(defined_)
	{
	}

	bool ok() const
	{
		return ok_;
	}

	bool at_end() const
	{
		return Peek().kind == ET_END;
	}

	long long Parse()
	{
		return ParseConditional();
	}

private:
	const ExprToken& Peek() const
	{
		return tokens[pos];
	}

	bool Consume(ExprTokenKind kind)
	{
		if (Peek().kind != kind)
			return false;
		++pos;
		return true;
	}

	long long ParsePrimary()
	{
		if (Peek().kind == ET_LITERAL)
		{
			long long value = Peek().value;
			++pos;
			return value;
		}

		if (Consume(ET_LPAREN))
		{
			long long value = ParseConditional();
			if (!Consume(ET_RPAREN))
				return Error();
			return value;
		}

		if (Peek().kind != ET_IDENTIFIER)
			return Error();

		string name = Peek().text;
		++pos;

		if (name == "defined")
		{
			string identifier;
			if (Peek().kind == ET_IDENTIFIER)
			{
				identifier = Peek().text;
				++pos;
			}
			else if (Consume(ET_LPAREN))
			{
				if (Peek().kind != ET_IDENTIFIER)
					return Error();
				identifier = Peek().text;
				++pos;
				if (!Consume(ET_RPAREN))
					return Error();
			}
			else
			{
				return Error();
			}

			return IsDefined(identifier) ? 1 : 0;
		}

		if (name == "true")
			return 1;
		if (name == "false")
			return 0;
		return 0;
	}

	long long ParseUnary()
	{
		if (Consume(ET_PLUS))
			return +ParseUnary();
		if (Consume(ET_MINUS))
			return -ParseUnary();
		if (Consume(ET_LNOT))
			return !ParseUnary();
		if (Consume(ET_COMPL))
			return ~ParseUnary();
		return ParsePrimary();
	}

	long long ParseMultiplicative()
	{
		long long value = ParseUnary();
		while (ok_)
		{
			if (Consume(ET_STAR))
				value = value * ParseUnary();
			else if (Consume(ET_DIV))
			{
				long long rhs = ParseUnary();
				if (rhs == 0)
					return Error();
				value = value / rhs;
			}
			else if (Consume(ET_MOD))
			{
				long long rhs = ParseUnary();
				if (rhs == 0)
					return Error();
				value = value % rhs;
			}
			else
				break;
		}
		return value;
	}

	long long ParseAdditive()
	{
		long long value = ParseMultiplicative();
		while (ok_)
		{
			if (Consume(ET_PLUS))
				value = value + ParseMultiplicative();
			else if (Consume(ET_MINUS))
				value = value - ParseMultiplicative();
			else
				break;
		}
		return value;
	}

	long long ParseShift()
	{
		long long value = ParseAdditive();
		while (ok_)
		{
			if (Consume(ET_LSHIFT))
				value = value << ParseAdditive();
			else if (Consume(ET_RSHIFT))
				value = value >> ParseAdditive();
			else
				break;
		}
		return value;
	}

	long long ParseRelational()
	{
		long long value = ParseShift();
		while (ok_)
		{
			if (Consume(ET_LT))
				value = value < ParseShift();
			else if (Consume(ET_GT))
				value = value > ParseShift();
			else if (Consume(ET_LE))
				value = value <= ParseShift();
			else if (Consume(ET_GE))
				value = value >= ParseShift();
			else
				break;
		}
		return value;
	}

	long long ParseEquality()
	{
		long long value = ParseRelational();
		while (ok_)
		{
			if (Consume(ET_EQ))
				value = value == ParseRelational();
			else if (Consume(ET_NE))
				value = value != ParseRelational();
			else
				break;
		}
		return value;
	}

	long long ParseBitAnd()
	{
		long long value = ParseEquality();
		while (Consume(ET_AMP))
			value = value & ParseEquality();
		return value;
	}

	long long ParseBitXor()
	{
		long long value = ParseBitAnd();
		while (Consume(ET_XOR))
			value = value ^ ParseBitAnd();
		return value;
	}

	long long ParseBitOr()
	{
		long long value = ParseBitXor();
		while (Consume(ET_BOR))
			value = value | ParseBitXor();
		return value;
	}

	long long ParseLogicalAnd()
	{
		long long value = ParseBitOr();
		while (Consume(ET_LAND))
			value = (value && ParseBitOr()) ? 1 : 0;
		return value;
	}

	long long ParseLogicalOr()
	{
		long long value = ParseLogicalAnd();
		while (Consume(ET_LOR))
			value = (value || ParseLogicalAnd()) ? 1 : 0;
		return value;
	}

	long long ParseConditional()
	{
		long long condition = ParseLogicalOr();
		if (!Consume(ET_QMARK))
			return condition;

		long long if_true = ParseConditional();
		if (!Consume(ET_COLON))
			return Error();
		long long if_false = ParseConditional();
		return condition ? if_true : if_false;
	}

	bool IsDefined(const string& name) const
	{
		auto it = defined.find(name);
		return it != defined.end() && it->second;
	}

	long long Error()
	{
		ok_ = false;
		return 0;
	}

	const vector<ExprToken>& tokens;
	const map<string, bool>& defined;
	size_t pos = 0;
	bool ok_ = true;
};

ExprTokenKind MapExprPunc(const string& text)
{
	if (text == "(") return ET_LPAREN;
	if (text == ")") return ET_RPAREN;
	if (text == "?") return ET_QMARK;
	if (text == ":") return ET_COLON;
	if (text == "+") return ET_PLUS;
	if (text == "-") return ET_MINUS;
	if (text == "!") return ET_LNOT;
	if (text == "~") return ET_COMPL;
	if (text == "*") return ET_STAR;
	if (text == "/") return ET_DIV;
	if (text == "%") return ET_MOD;
	if (text == "<<") return ET_LSHIFT;
	if (text == ">>") return ET_RSHIFT;
	if (text == "<") return ET_LT;
	if (text == ">") return ET_GT;
	if (text == "<=") return ET_LE;
	if (text == ">=") return ET_GE;
	if (text == "==") return ET_EQ;
	if (text == "!=") return ET_NE;
	if (text == "&") return ET_AMP;
	if (text == "^") return ET_XOR;
	if (text == "|") return ET_BOR;
	if (text == "&&") return ET_LAND;
	if (text == "||") return ET_LOR;
	return ET_END;
}

bool TokenizeIfExpression(const vector<MacroToken>& tokens, vector<ExprToken>& out)
{
	for (const MacroToken& token : tokens)
	{
		if (token.placemarker || IsWhitespaceToken(token))
			continue;

		if (token.type == PPT_IDENTIFIER)
		{
			ExprToken item;
			item.kind = ET_IDENTIFIER;
			item.text = token.data;
			out.push_back(item);
			continue;
		}

		if (token.type == PPT_PP_NUMBER)
		{
			ParsedNumberLiteral parsed = ParsePPNumberLiteral(token.data);
			if (!parsed.ok || !parsed.is_integer || parsed.is_user_defined)
				return false;

			ExprToken item;
			item.kind = ET_LITERAL;
			item.value = static_cast<long long>(parsed.int_info.value);
			out.push_back(item);
			continue;
		}

		if (token.type == PPT_CHARACTER_LITERAL || token.type == PPT_USER_DEFINED_CHARACTER_LITERAL)
		{
			if (token.type == PPT_USER_DEFINED_CHARACTER_LITERAL)
				return false;
			ParsedCharLiteral parsed = ParseCharacterLiteralToken(ToPPToken(token));
			if (!parsed.ok || parsed.user_defined)
				return false;

			ExprToken item;
			item.kind = ET_LITERAL;
			item.value = static_cast<long long>(parsed.value);
			out.push_back(item);
			continue;
		}

		if (token.type != PPT_PREPROCESSING_OP_OR_PUNC)
			return false;

		ExprTokenKind kind = MapExprPunc(token.data);
		if (kind == ET_END)
			return false;

		ExprToken item;
		item.kind = kind;
		out.push_back(item);
	}

	out.push_back(ExprToken());
	return true;
}

struct IfFrame
{
	bool parent_active = true;
	bool current_active = false;
	bool branch_taken = false;
	bool saw_else = false;
};

class Preprocessor
{
public:
	Preprocessor(const string& date_literal, const string& time_literal, const string& author_literal)
		: date_literal_(date_literal), time_literal_(time_literal), author_literal_(author_literal)
	{
		InstallFixedPredefinedMacros();
	}

	void ProcessSourceFile(const string& srcfile, ostream& out)
	{
		output_.clear();
		text_.clear();
		text_actual_file_.clear();
		ProcessFile(srcfile);
		FlushText();

		vector<PPToken> final_tokens;
		for (const MacroToken& token : output_)
		{
			if (token.placemarker)
				continue;
			if (token.type == PPT_WHITESPACE_SEQUENCE || token.type == PPT_NEW_LINE)
				continue;
			final_tokens.push_back(ToPPToken(token));
		}
		PPToken eof_token;
		eof_token.type = PPT_EOF;
		final_tokens.push_back(eof_token);

		StrictPostTokenOutputStream debug(out);
		EmitPostTokenStream(final_tokens, debug, true);
	}

private:
	void InstallFixedPredefinedMacros()
	{
		DefineObjectMacro("__CPPGM__", "201303L");
		DefineObjectMacro("__cplusplus", "201103L");
		DefineObjectMacro("__STDC_HOSTED__", "1");
		DefineObjectMacro("__CPPGM_AUTHOR__", author_literal_);
	}

	void DefineObjectMacro(const string& name, const string& replacement)
	{
		vector<PPToken> raw = LexPPTokens(replacement);
		vector<PPToken> line;
		for (const PPToken& token : raw)
		{
			if (token.type == PPT_EOF || token.type == PPT_NEW_LINE)
				continue;
			line.push_back(token);
		}

		MacroDef def;
		def.name = name;
		def.replacement = ConvertTokens(line, "", 0);
		def.replacement = NormalizeWhitespace(def.replacement);
		macros_[name] = def;
	}

	vector<vector<PPToken> > SplitLines(const vector<PPToken>& tokens) const
	{
		vector<vector<PPToken> > lines;
		vector<PPToken> current;
		for (const PPToken& token : tokens)
		{
			if (token.type == PPT_NEW_LINE)
			{
				lines.push_back(current);
				current.clear();
			}
			else if (token.type == PPT_EOF)
			{
				lines.push_back(current);
				break;
			}
			else
			{
				current.push_back(token);
			}
		}
		return lines;
	}

	size_t SkipWhitespace(const vector<PPToken>& line, size_t i) const
	{
		while (i < line.size() && line[i].type == PPT_WHITESPACE_SEQUENCE)
			++i;
		return i;
	}

	bool IsDirectiveLine(const vector<PPToken>& line) const
	{
		size_t i = SkipWhitespace(line, 0);
		if (i >= line.size())
			return false;
		return line[i].type == PPT_PREPROCESSING_OP_OR_PUNC &&
			(line[i].data == "#" || line[i].data == "%:");
	}

	vector<MacroToken> ConvertTokens(const vector<PPToken>& tokens, const string& file, long line) const
	{
		vector<MacroToken> out;
		for (const PPToken& token : tokens)
		{
			MacroToken converted = FromPPToken(token);
			if (converted.type == PPT_PREPROCESSING_OP_OR_PUNC)
			{
				if (converted.data == "%:")
					converted.data = "#";
				else if (converted.data == "%:%:")
					converted.data = "##";
			}
			converted.source_file = file;
			converted.source_line = line;
			out.push_back(converted);
		}
		return NormalizeWhitespace(out);
	}

	void Fail(const string& message) const
	{
		throw runtime_error(message);
	}

	bool IsDefinedMacro(const string& name) const
	{
		return macros_.count(name) != 0 ||
			name == "__FILE__" ||
			name == "__LINE__" ||
			name == "__DATE__" ||
			name == "__TIME__";
	}

	vector<MacroToken> ExpandBuiltin(const MacroToken& token) const
	{
		vector<MacroToken> out(1);
		out[0].source_file = token.source_file;
		out[0].source_line = token.source_line;

		if (token.data == "__FILE__")
		{
			out[0].type = PPT_STRING_LITERAL;
			out[0].data = EscapeStringLiteral(token.source_file);
		}
		else if (token.data == "__LINE__")
		{
			out[0].type = PPT_PP_NUMBER;
			out[0].data = to_string(token.source_line);
		}
		else if (token.data == "__DATE__")
		{
			out[0].type = PPT_STRING_LITERAL;
			out[0].data = date_literal_;
		}
		else if (token.data == "__TIME__")
		{
			out[0].type = PPT_STRING_LITERAL;
			out[0].data = time_literal_;
		}
		else
		{
			Fail("unknown builtin macro: " + token.data);
		}

		return out;
	}

	bool MacroDefsEqual(const MacroDef& lhs, const MacroDef& rhs) const
	{
		return lhs.function_like == rhs.function_like &&
			lhs.variadic == rhs.variadic &&
			lhs.params == rhs.params &&
			TokensEqual(lhs.replacement, rhs.replacement);
	}

	bool IsRawPunc(const PPToken& token, const string& data) const
	{
		return token.type == PPT_PREPROCESSING_OP_OR_PUNC && token.data == data;
	}

	int FindParamIndex(const MacroDef& def, const string& name) const
	{
		for (size_t i = 0; i < def.params.size(); ++i)
		{
			if (def.params[i] == name)
				return static_cast<int>(i);
		}
		return -1;
	}

	void ValidateHashHashEdges(const vector<MacroToken>& replacement) const
	{
		int first = -1;
		int last = -1;
		for (size_t i = 0; i < replacement.size(); ++i)
		{
			if (IsWhitespaceToken(replacement[i]))
				continue;
			if (first == -1)
				first = static_cast<int>(i);
			last = static_cast<int>(i);
		}

		if (first != -1 && IsPuncToken(replacement[static_cast<size_t>(first)], "##"))
			Fail("## at edge of replacement list");
		if (last != -1 && IsPuncToken(replacement[static_cast<size_t>(last)], "##"))
			Fail("## at edge of replacement list");
	}

	void ValidateStringizeOperators(const MacroDef& def) const
	{
		if (!def.function_like)
			return;

		for (size_t i = 0; i < def.replacement.size(); ++i)
		{
			const MacroToken& token = def.replacement[i];
			if (!IsPuncToken(token, "#"))
				continue;

			size_t j = i + 1;
			while (j < def.replacement.size() && IsWhitespaceToken(def.replacement[j]))
				++j;
			if (j >= def.replacement.size())
				Fail("# at end of function-like macro replacement list");
			if (!IsIdentifierToken(def.replacement[j]))
				Fail("# must be followed by parameter in function-like macro");
			if (FindParamIndex(def, def.replacement[j].data) < 0 &&
				!(def.variadic && def.replacement[j].data == "__VA_ARGS__"))
			{
				Fail("# must be followed by parameter in function-like macro");
			}
		}
	}

	void CheckReplacementVarArgs(const MacroDef& def) const
	{
		for (const MacroToken& token : def.replacement)
		{
			if (IsIdentifierToken(token) && token.data == "__VA_ARGS__" && !def.variadic)
				Fail("invalid __VA_ARGS__ use");
		}
	}

	void ParseParamList(const vector<PPToken>& line, size_t& i, MacroDef& def) const
	{
		++i;
		i = SkipWhitespace(line, i);

		if (i >= line.size())
			Fail("expected identifier after lparen");

		if (IsRawPunc(line[i], ")"))
		{
			++i;
			return;
		}

		while (true)
		{
			i = SkipWhitespace(line, i);
			if (i >= line.size())
				Fail("expected identifier after lparen");

			if (line[i].type == PPT_PREPROCESSING_OP_OR_PUNC && line[i].data == "...")
			{
				def.variadic = true;
				++i;
				i = SkipWhitespace(line, i);
				if (i >= line.size() || !IsRawPunc(line[i], ")"))
					Fail("expected rparen");
				++i;
				return;
			}

			if (line[i].type != PPT_IDENTIFIER)
				Fail("expected identifier");
			if (line[i].data == "__VA_ARGS__")
				Fail("__VA_ARGS__ in macro parameter list");
			for (const string& param : def.params)
			{
				if (param == line[i].data)
					Fail("duplicate parameter " + line[i].data + " in macro definition");
			}
			def.params.push_back(line[i].data);
			++i;
			i = SkipWhitespace(line, i);
			if (i >= line.size())
				Fail("expected rparen");
			if (IsRawPunc(line[i], ")"))
			{
				++i;
				return;
			}
			if (!IsRawPunc(line[i], ","))
				Fail("expected comma");
			++i;
			i = SkipWhitespace(line, i);
			if (i >= line.size())
				Fail("expected identifier");
			if (line[i].type == PPT_PREPROCESSING_OP_OR_PUNC && line[i].data == "...")
			{
				def.variadic = true;
				++i;
				i = SkipWhitespace(line, i);
				if (i >= line.size() || !IsRawPunc(line[i], ")"))
					Fail("expected rparen");
				++i;
				return;
			}
			if (line[i].type != PPT_IDENTIFIER)
				Fail("expected identifier");
		}
	}

	void HandleDefine(const vector<PPToken>& line, size_t i, const string& file, long line_no)
	{
		i = SkipWhitespace(line, i);
		if (i >= line.size() || line[i].type != PPT_IDENTIFIER)
			Fail("expected identifier");
		if (line[i].data == "__VA_ARGS__")
			Fail("invalid __VA_ARGS__ use");

		MacroDef def;
		def.name = line[i].data;
		++i;

		if (i < line.size() && IsRawPunc(line[i], "("))
		{
			def.function_like = true;
			ParseParamList(line, i, def);
		}

		vector<PPToken> replacement_tokens(line.begin() + static_cast<ptrdiff_t>(i), line.end());
		def.replacement = ConvertTokens(replacement_tokens, file, line_no);
		def.replacement = NormalizeWhitespace(def.replacement);

		ValidateHashHashEdges(def.replacement);
		ValidateStringizeOperators(def);
		CheckReplacementVarArgs(def);

		auto it = macros_.find(def.name);
		if (it != macros_.end())
		{
			if (!MacroDefsEqual(it->second, def))
				Fail("macro redefined");
		}
		else
		{
			macros_[def.name] = def;
		}
	}

	void HandleUndef(const vector<PPToken>& line, size_t i) const
	{
		(void) line;
		(void) i;
	}

	void UndefDirective(const vector<PPToken>& line, size_t i)
	{
		i = SkipWhitespace(line, i);
		if (i >= line.size() || line[i].type != PPT_IDENTIFIER)
			Fail("expected identifier");
		if (line[i].data == "__VA_ARGS__")
			Fail("invalid __VA_ARGS__ use");

		string name = line[i].data;
		++i;
		i = SkipWhitespace(line, i);
		if (i != line.size())
			Fail("expected new line");
		macros_.erase(name);
	}

	void AppendTextLine(const vector<MacroToken>& line, const string& actual_file)
	{
		if (line.empty())
			return;
		if (text_.empty())
			text_actual_file_ = actual_file;
		else
			text_.push_back(MakeWhitespaceToken());
		text_.insert(text_.end(), line.begin(), line.end());
	}

	vector<MacroToken> CombineVarArgs(const vector<vector<MacroToken> >& args, size_t start) const
	{
		vector<MacroToken> out;
		for (size_t i = start; i < args.size(); ++i)
		{
			if (!out.empty())
			{
				MacroToken comma;
				comma.type = PPT_PREPROCESSING_OP_OR_PUNC;
				comma.data = ",";
				out.push_back(comma);
				out.push_back(MakeWhitespaceToken());
			}
			out.insert(out.end(), args[i].begin(), args[i].end());
		}
		return NormalizeWhitespace(out);
	}

	void ParseInvocation(const MacroDef& def, const vector<MacroToken>& seq, size_t lparen_index, size_t& end_index, vector<vector<MacroToken> >& args) const
	{
		size_t j = lparen_index + 1;
		int depth = 0;
		vector<MacroToken> current;
		bool saw_separator = false;
		while (j < seq.size())
		{
			const MacroToken& token = seq[j];
			if (IsPuncToken(token, "("))
			{
				++depth;
				current.push_back(token);
			}
			else if (IsPuncToken(token, ")"))
			{
				if (depth == 0)
					break;
				--depth;
				current.push_back(token);
			}
			else if (IsPuncToken(token, ",") && depth == 0)
			{
				args.push_back(NormalizeWhitespace(current));
				current.clear();
				saw_separator = true;
			}
			else
			{
				current.push_back(token);
			}
			++j;
		}

		if (j >= seq.size() || !IsPuncToken(seq[j], ")"))
			Fail("macro function-like invocation missing rparen: " + def.name);

		vector<MacroToken> normalized_current = NormalizeWhitespace(current);
		bool empty_inside = normalized_current.empty() && !saw_separator;
		if (empty_inside)
		{
			if (def.params.empty())
				args.clear();
			else
				args.push_back(vector<MacroToken>());
		}
		else
		{
			args.push_back(normalized_current);
		}

		size_t argc = args.size();
		if (!def.variadic)
		{
			if (argc != def.params.size())
				Fail("macro function-like invocation wrong num of params: " + def.name);
		}
		else if (argc < def.params.size())
		{
			Fail("macro function-like invocation wrong num of params: " + def.name);
		}

		end_index = j + 1;
	}

	vector<MacroToken> PasteTokens(const MacroToken& left, const MacroToken& right) const
	{
		if (left.placemarker && right.placemarker)
			return vector<MacroToken>(1, MakePlacemakerToken());
		if (left.placemarker)
			return vector<MacroToken>(1, right);
		if (right.placemarker)
			return vector<MacroToken>(1, left);

		string joined_text = left.data + right.data;
		vector<MacroToken> pasted = RetokenizeConcat(joined_text);
		if (pasted.size() > 1)
		{
			ParsedNumberLiteral parsed = ParsePPNumberLiteral(joined_text);
			if (parsed.ok)
			{
				MacroToken token;
				token.type = PPT_PP_NUMBER;
				token.data = joined_text;
				token.inert_hashhash = true;
				pasted.assign(1, token);
			}
		}
		if (pasted.empty())
			return vector<MacroToken>(1, MakePlacemakerToken());
		for (MacroToken& token : pasted)
			token.inert_hashhash = true;
		UnionBlocked(pasted[0], left.blocked);
		UnionBlocked(pasted[0], right.blocked);
		for (size_t i = 1; i < pasted.size(); ++i)
		{
			UnionBlocked(pasted[i], left.blocked);
			UnionBlocked(pasted[i], right.blocked);
		}
		return pasted;
	}

	vector<MacroToken> ApplyHashHash(vector<MacroToken> tokens) const
	{
		while (true)
		{
			size_t join_index = tokens.size();
			for (size_t i = 0; i < tokens.size(); ++i)
			{
				if (IsPuncToken(tokens[i], "##"))
				{
					if (tokens[i].inert_hashhash)
						continue;
					join_index = i;
					break;
				}
			}
			if (join_index == tokens.size())
				break;

			size_t left = join_index;
			while (left > 0 && IsWhitespaceToken(tokens[left - 1]))
				--left;
			if (left == 0)
				Fail("## at edge of replacement list");
			--left;

			size_t right = join_index + 1;
			while (right < tokens.size() && IsWhitespaceToken(tokens[right]))
				++right;
			if (right >= tokens.size())
				Fail("## at edge of replacement list");

			vector<MacroToken> pasted = PasteTokens(tokens[left], tokens[right]);
			vector<MacroToken> next;
			next.insert(next.end(), tokens.begin(), tokens.begin() + static_cast<ptrdiff_t>(left));
			next.insert(next.end(), pasted.begin(), pasted.end());
			next.insert(next.end(), tokens.begin() + static_cast<ptrdiff_t>(right + 1), tokens.end());
			tokens.swap(next);
		}

		vector<MacroToken> out;
		for (const MacroToken& token : tokens)
		{
			if (!token.placemarker)
				out.push_back(token);
		}
		return out;
	}

	vector<MacroToken> ExpandInvocation(const MacroDef& def, const MacroToken& head, const vector<vector<MacroToken> >& raw_args)
	{
		vector<MacroToken> raw_varargs;
		bool have_raw_varargs = false;
		vector<MacroToken> expanded_varargs;
		bool have_expanded_varargs = false;
		vector<vector<MacroToken> > expanded_args(raw_args.size());
		vector<bool> have_expanded_args(raw_args.size(), false);

		auto get_expanded_arg = [&](size_t index) -> const vector<MacroToken>&
		{
			if (!have_expanded_args[index])
			{
				expanded_args[index] = ExpandSequence(raw_args[index]);
				have_expanded_args[index] = true;
			}
			return expanded_args[index];
		};

		auto get_raw_varargs = [&]() -> const vector<MacroToken>&
		{
			if (!have_raw_varargs)
			{
				raw_varargs = CombineVarArgs(raw_args, def.params.size());
				have_raw_varargs = true;
			}
			return raw_varargs;
		};

		auto get_expanded_varargs = [&]() -> const vector<MacroToken>&
		{
			if (!have_expanded_varargs)
			{
				vector<vector<MacroToken> > expanded_extra;
				for (size_t i = def.params.size(); i < raw_args.size(); ++i)
					expanded_extra.push_back(get_expanded_arg(i));
				expanded_varargs = CombineVarArgs(expanded_extra, 0);
				have_expanded_varargs = true;
			}
			return expanded_varargs;
		};

		if (def.variadic)
			get_raw_varargs();

		vector<MacroToken> substituted;
		for (size_t i = 0; i < def.replacement.size(); ++i)
		{
			const MacroToken& token = def.replacement[i];
			if (token.placemarker)
				continue;

			if (def.function_like && IsPuncToken(token, "#"))
			{
				size_t j = i + 1;
				while (j < def.replacement.size() && IsWhitespaceToken(def.replacement[j]))
					++j;
				const MacroToken& param_token = def.replacement[j];
				vector<MacroToken> stringized_source;
				if (param_token.data == "__VA_ARGS__")
					stringized_source = get_raw_varargs();
				else
					stringized_source = raw_args[static_cast<size_t>(FindParamIndex(def, param_token.data))];

				MacroToken out;
				out.type = PPT_STRING_LITERAL;
				out.data = StringizeTokens(stringized_source);
				substituted.push_back(out);
				i = j;
				continue;
			}

			if (IsIdentifierToken(token))
			{
				int param_index = FindParamIndex(def, token.data);
				bool is_varargs = def.variadic && token.data == "__VA_ARGS__";
				if (param_index >= 0 || is_varargs)
				{
					bool adjacent_to_hashhash = false;
					for (size_t j = i; j > 0; --j)
					{
						if (IsWhitespaceToken(def.replacement[j - 1]))
							continue;
						adjacent_to_hashhash = IsPuncToken(def.replacement[j - 1], "##");
						break;
					}
					for (size_t j = i + 1; j < def.replacement.size(); ++j)
					{
						if (IsWhitespaceToken(def.replacement[j]))
							continue;
						adjacent_to_hashhash = adjacent_to_hashhash || IsPuncToken(def.replacement[j], "##");
						break;
					}

					vector<MacroToken> source;
					if (param_index >= 0)
						source = adjacent_to_hashhash ? raw_args[static_cast<size_t>(param_index)] : get_expanded_arg(static_cast<size_t>(param_index));
					else
						source = adjacent_to_hashhash ? get_raw_varargs() : get_expanded_varargs();
					source = MarkSubstitutedTokens(source);

					if (source.empty() && adjacent_to_hashhash)
					{
						substituted.push_back(MakePlacemakerToken());
					}
					else
					{
						substituted.insert(substituted.end(), source.begin(), source.end());
					}
					continue;
				}
			}

			substituted.push_back(token);
		}

		substituted = ApplyHashHash(substituted);
		substituted = NormalizeWhitespace(substituted);
		for (MacroToken& token : substituted)
		{
			if (token.placemarker || IsWhitespaceToken(token))
				continue;
			token.source_file = head.source_file;
			token.source_line = head.source_line;
			UnionBlocked(token, head.blocked);
			AddBlocked(token, def.name);
		}
		return substituted;
	}

	vector<MacroToken> ExpandSequence(const vector<MacroToken>& input)
	{
		vector<MacroToken> seq = NormalizeWhitespace(input);
		size_t i = 0;
		while (i < seq.size())
		{
			if (IsWhitespaceToken(seq[i]) || seq[i].placemarker)
			{
				++i;
				continue;
			}

			if (IsIdentifierToken(seq[i]) && seq[i].data == "__VA_ARGS__")
				Fail("__VA_ARGS__ token in text-lines: __VA_ARGS__");

			if (!IsIdentifierToken(seq[i]))
			{
				++i;
				continue;
			}

			if ((seq[i].data == "__FILE__" || seq[i].data == "__LINE__" ||
				seq[i].data == "__DATE__" || seq[i].data == "__TIME__") &&
				!ContainsBlocked(seq[i], seq[i].data))
			{
				vector<MacroToken> replacement = ExpandBuiltin(seq[i]);
				seq.erase(seq.begin() + static_cast<ptrdiff_t>(i));
				seq.insert(seq.begin() + static_cast<ptrdiff_t>(i), replacement.begin(), replacement.end());
				seq = NormalizeWhitespace(seq);
				continue;
			}

			auto it = macros_.find(seq[i].data);
			if (it == macros_.end() || ContainsBlocked(seq[i], seq[i].data))
			{
				++i;
				continue;
			}

			const MacroDef& def = it->second;
			size_t invocation_end = i + 1;
			vector<vector<MacroToken> > raw_args;
			if (def.function_like)
			{
				size_t lparen_index = i + 1;
				while (lparen_index < seq.size() && IsWhitespaceToken(seq[lparen_index]))
					++lparen_index;
				if (lparen_index >= seq.size() || !IsPuncToken(seq[lparen_index], "("))
				{
					++i;
					continue;
				}
				ParseInvocation(def, seq, lparen_index, invocation_end, raw_args);
			}

			vector<MacroToken> replacement = ExpandInvocation(def, seq[i], raw_args);
			seq.erase(seq.begin() + static_cast<ptrdiff_t>(i), seq.begin() + static_cast<ptrdiff_t>(invocation_end));
			seq.insert(seq.begin() + static_cast<ptrdiff_t>(i), replacement.begin(), replacement.end());
			seq = NormalizeWhitespace(seq);
		}

		return seq;
	}

	void MarkPragmaOnce(const string& actual_file)
	{
		PA5FileId fileid;
		if (PA5GetFileId(actual_file, fileid))
			pragma_once_files_.insert(fileid);
	}

	void ExecutePragmaTokens(const vector<MacroToken>& raw_tokens, const string& actual_file)
	{
		vector<MacroToken> tokens = NormalizeWhitespace(raw_tokens);
		if (tokens.empty())
			return;
		if (!IsIdentifierToken(tokens[0]))
			return;

		if (tokens[0].data == "once" && tokens.size() == 1)
		{
			MarkPragmaOnce(actual_file);
			return;
		}

		if (tokens[0].data == "cppgm_mock_unknown")
			return;
	}

	vector<MacroToken> ApplyPragmaOperators(const vector<MacroToken>& input, const string& actual_file)
	{
		vector<MacroToken> out;
		for (size_t i = 0; i < input.size(); ++i)
		{
			if (!IsIdentifierToken(input[i]) || input[i].data != "_Pragma")
			{
				out.push_back(input[i]);
				continue;
			}

			size_t j = i + 1;
			while (j < input.size() && IsWhitespaceToken(input[j]))
				++j;
			if (j >= input.size() || !IsPuncToken(input[j], "("))
				Fail("_Pragma missing lparen");
			++j;
			while (j < input.size() && IsWhitespaceToken(input[j]))
				++j;
			if (j >= input.size())
				Fail("_Pragma missing string-literal");
			MacroToken literal = input[j];
			if (literal.placemarker)
				Fail("_Pragma missing string-literal");
			if (literal.type != PPT_STRING_LITERAL && literal.type != PPT_USER_DEFINED_STRING_LITERAL)
				Fail("_Pragma missing string-literal");
			++j;
			while (j < input.size() && IsWhitespaceToken(input[j]))
				++j;
			if (j >= input.size() || !IsPuncToken(input[j], ")"))
				Fail("_Pragma missing rparen");

			ParsedStringLiteralToken parsed = ParseStringLiteralToken(ToPPToken(literal));
			if (!parsed.ok || parsed.user_defined)
				Fail("invalid _Pragma string-literal");

			string content;
			for (int cp : parsed.codepoints)
			{
				if (cp < 0 || cp > 0x7F)
					Fail("non-ascii _Pragma");
				content.push_back(static_cast<char>(cp));
			}

			vector<PPToken> pragma_pp = LexPPTokens(content);
			vector<PPToken> pragma_line;
			for (const PPToken& token : pragma_pp)
			{
				if (token.type == PPT_EOF || token.type == PPT_NEW_LINE)
					continue;
				pragma_line.push_back(token);
			}
			ExecutePragmaTokens(ConvertTokens(pragma_line, literal.source_file, literal.source_line), actual_file);
			i = j;
		}
		return NormalizeWhitespace(out);
	}

	void FlushText()
	{
		text_ = NormalizeWhitespace(text_);
		if (text_.empty())
			return;

		vector<MacroToken> expanded = ExpandSequence(text_);
		expanded = NormalizeWhitespace(expanded);
		expanded = ApplyPragmaOperators(expanded, text_actual_file_);
		expanded = NormalizeWhitespace(expanded);

		if (!expanded.empty() && !output_.empty())
			output_.push_back(MakeWhitespaceToken());
		output_.insert(output_.end(), expanded.begin(), expanded.end());
		text_.clear();
		text_actual_file_.clear();
	}

	bool CurrentActive(const vector<IfFrame>& ifs) const
	{
		return ifs.empty() || ifs.back().current_active;
	}

	long long EvaluateControllingExpression(const vector<MacroToken>& tokens)
	{
		vector<MacroToken> protected_tokens = NormalizeWhitespace(tokens);
		for (size_t i = 0; i < protected_tokens.size(); ++i)
		{
			if (!IsIdentifierToken(protected_tokens[i]) || protected_tokens[i].data != "defined")
				continue;

			size_t j = i + 1;
			while (j < protected_tokens.size() && IsWhitespaceToken(protected_tokens[j]))
				++j;
			if (j >= protected_tokens.size())
				continue;

			if (IsPuncToken(protected_tokens[j], "("))
			{
				++j;
				while (j < protected_tokens.size() && IsWhitespaceToken(protected_tokens[j]))
					++j;
			}

			if (j < protected_tokens.size() && IsIdentifierToken(protected_tokens[j]))
				AddBlocked(protected_tokens[j], protected_tokens[j].data);
		}

		vector<MacroToken> expanded = ExpandSequence(protected_tokens);
		vector<ExprToken> expr_tokens;
		if (!TokenizeIfExpression(expanded, expr_tokens))
			Fail("invalid #if expression");

		map<string, bool> defined;
		for (const auto& item : macros_)
			defined[item.first] = true;
		defined["__FILE__"] = true;
		defined["__LINE__"] = true;
		defined["__DATE__"] = true;
		defined["__TIME__"] = true;

		ExprParser parser(expr_tokens, defined);
		long long value = parser.Parse();
		if (!parser.ok() || !parser.at_end())
			Fail("invalid #if expression");
		return value;
	}

	string DecodeIncludePath(const MacroToken& token) const
	{
		if (token.type == PPT_HEADER_NAME)
		{
			if (token.data.size() < 2)
				Fail("invalid header-name");
			return token.data.substr(1, token.data.size() - 2);
		}

		if (token.type != PPT_STRING_LITERAL && token.type != PPT_USER_DEFINED_STRING_LITERAL)
			Fail("invalid include target");

		ParsedStringLiteralToken parsed = ParseStringLiteralToken(ToPPToken(token));
		if (!parsed.ok || parsed.user_defined || parsed.prefix_kind != SPK_ORDINARY)
			Fail("invalid include string-literal");

		string out;
		for (int cp : parsed.codepoints)
		{
			if (cp < 0 || cp > 0x7F)
				Fail("non-ascii include path");
			out.push_back(static_cast<char>(cp));
		}
		return out;
	}

	string ResolveInclude(const string& current_file, const string& nextf)
	{
		string pathrel;
		size_t slash = current_file.rfind('/');
		if (slash != string::npos)
			pathrel = current_file.substr(0, slash + 1) + nextf;

		PA5FileId fileid;
		if (!pathrel.empty() && PA5GetFileId(pathrel, fileid))
			return pathrel;
		if (PA5GetFileId(nextf, fileid))
			return nextf;
		Fail("include file not found: " + nextf);
		return "";
	}

	long ParseLineNumberToken(const MacroToken& token) const
	{
		if (token.type != PPT_PP_NUMBER)
			Fail("invalid #line number");
		ParsedNumberLiteral parsed = ParsePPNumberLiteral(token.data);
		if (!parsed.ok || !parsed.is_integer || parsed.is_user_defined || parsed.int_info.value == 0)
			Fail("invalid #line number");
		return static_cast<long>(parsed.int_info.value);
	}

	void HandleDirective(const vector<PPToken>& raw_line, const string& actual_file, string& presumed_file, long& next_line, bool& line_overridden, vector<IfFrame>& ifs)
	{
		size_t i = SkipWhitespace(raw_line, 0);
		++i;
		i = SkipWhitespace(raw_line, i);

		if (i >= raw_line.size())
			return;

		if (raw_line[i].type != PPT_IDENTIFIER)
		{
			if (CurrentActive(ifs))
				Fail("non-directive in active section");
			return;
		}

		string directive = raw_line[i].data;
		++i;

		if (directive == "if")
		{
			IfFrame frame;
			frame.parent_active = CurrentActive(ifs);
			if (!frame.parent_active)
			{
				frame.current_active = false;
			}
			else
			{
				frame.current_active = EvaluateControllingExpression(ConvertTokens(vector<PPToken>(raw_line.begin() + static_cast<ptrdiff_t>(i), raw_line.end()), presumed_file, next_line)) != 0;
				frame.branch_taken = frame.current_active;
			}
			ifs.push_back(frame);
			return;
		}

		if (directive == "ifdef" || directive == "ifndef")
		{
			IfFrame frame;
			frame.parent_active = CurrentActive(ifs);
			if (!frame.parent_active)
			{
				frame.current_active = false;
			}
			else
			{
				i = SkipWhitespace(raw_line, i);
				if (i >= raw_line.size() || raw_line[i].type != PPT_IDENTIFIER)
					Fail("expected identifier");
				bool defined = IsDefinedMacro(raw_line[i].data);
				frame.current_active = directive == "ifdef" ? defined : !defined;
				frame.branch_taken = frame.current_active;
			}
			ifs.push_back(frame);
			return;
		}

		if (directive == "elif")
		{
			if (ifs.empty())
				Fail("unexpected #elif");
			IfFrame& frame = ifs.back();
			if (frame.saw_else)
				Fail("#elif after #else");
			if (!frame.parent_active || frame.branch_taken)
			{
				frame.current_active = false;
			}
			else
			{
				frame.current_active = EvaluateControllingExpression(ConvertTokens(vector<PPToken>(raw_line.begin() + static_cast<ptrdiff_t>(i), raw_line.end()), presumed_file, next_line)) != 0;
				frame.branch_taken = frame.current_active;
			}
			return;
		}

		if (directive == "else")
		{
			if (ifs.empty())
				Fail("unexpected #else");
			IfFrame& frame = ifs.back();
			if (frame.saw_else)
				Fail("duplicate #else");
			frame.saw_else = true;
			frame.current_active = frame.parent_active && !frame.branch_taken;
			frame.branch_taken = true;
			return;
		}

		if (directive == "endif")
		{
			if (ifs.empty())
				Fail("unexpected #endif");
			ifs.pop_back();
			return;
		}

		if (!CurrentActive(ifs))
			return;

		if (directive == "define")
		{
			HandleDefine(raw_line, i, presumed_file, next_line);
			return;
		}

		if (directive == "undef")
		{
			UndefDirective(raw_line, i);
			return;
		}

		if (directive == "error")
		{
			Fail("#error encountered");
		}

		if (directive == "pragma")
		{
			vector<PPToken> tail(raw_line.begin() + static_cast<ptrdiff_t>(i), raw_line.end());
			ExecutePragmaTokens(ConvertTokens(tail, presumed_file, next_line), actual_file);
			return;
		}

		if (directive == "include")
		{
			vector<PPToken> tail(raw_line.begin() + static_cast<ptrdiff_t>(i), raw_line.end());
			vector<MacroToken> expanded = ExpandSequence(ConvertTokens(tail, presumed_file, next_line));
			expanded = NormalizeWhitespace(expanded);
			vector<MacroToken> cleaned;
			for (const MacroToken& token : expanded)
			{
				if (!IsWhitespaceToken(token) && !token.placemarker)
					cleaned.push_back(token);
			}
			if (cleaned.size() != 1)
				Fail("invalid include target");

			string nextf = DecodeIncludePath(cleaned[0]);
			string resolved = ResolveInclude(presumed_file, nextf);
			PA5FileId fileid;
			if (PA5GetFileId(resolved, fileid) && pragma_once_files_.count(fileid) != 0)
				return;
			ProcessFile(resolved);
			return;
		}

		if (directive == "line")
		{
			vector<PPToken> tail(raw_line.begin() + static_cast<ptrdiff_t>(i), raw_line.end());
			vector<MacroToken> expanded = ExpandSequence(ConvertTokens(tail, presumed_file, next_line));
			expanded = NormalizeWhitespace(expanded);
			vector<MacroToken> cleaned;
			for (const MacroToken& token : expanded)
			{
				if (!IsWhitespaceToken(token) && !token.placemarker)
					cleaned.push_back(token);
			}
			if (cleaned.empty() || cleaned.size() > 2)
				Fail("invalid #line");
			long new_line = ParseLineNumberToken(cleaned[0]);
			if (cleaned.size() == 2)
			{
				ParsedStringLiteralToken parsed = ParseStringLiteralToken(ToPPToken(cleaned[1]));
				if (!parsed.ok || parsed.user_defined || parsed.prefix_kind != SPK_ORDINARY)
					Fail("invalid #line filename");
				string new_file;
				for (int cp : parsed.codepoints)
				{
					if (cp < 0 || cp > 0x7F)
						Fail("non-ascii #line filename");
					new_file.push_back(static_cast<char>(cp));
				}
				presumed_file = new_file;
			}
			next_line = new_line;
			line_overridden = true;
			return;
		}

		Fail("unsupported directive");
	}

	void ProcessFile(const string& actual_file)
	{
		string raw_text = ReadFileText(actual_file);
		vector<vector<PPToken> > lines = SplitLines(LexPPTokens(raw_text));
		vector<long> logical_starts = ComputeLogicalLineStarts(raw_text);
		vector<IfFrame> ifs;
		string presumed_file = actual_file;
		long line_offset = 0;
		long current_line = logical_starts.empty() ? 1 : logical_starts[0];

		for (size_t line_index = 0; line_index < lines.size(); ++line_index)
		{
			const vector<PPToken>& raw_line = lines[line_index];
			bool line_overridden = false;
			long next_line = current_line;

			if (IsDirectiveLine(raw_line))
			{
				FlushText();
				HandleDirective(raw_line, actual_file, presumed_file, next_line, line_overridden, ifs);
			}
			else if (CurrentActive(ifs))
			{
				AppendTextLine(ConvertTokens(raw_line, presumed_file, current_line), actual_file);
			}

			long base_next_line = line_index + 1 < logical_starts.size() ?
				logical_starts[line_index + 1] :
				current_line + 1;
			if (line_overridden)
			{
				line_offset = next_line - base_next_line;
				current_line = next_line;
			}
			else
			{
				current_line = base_next_line + line_offset;
			}
		}

		if (!ifs.empty())
			Fail("unterminated #if");
	}

	map<string, MacroDef> macros_;
	vector<MacroToken> text_;
	vector<MacroToken> output_;
	set<PA5FileId> pragma_once_files_;
	string text_actual_file_;
	string date_literal_;
	string time_literal_;
	string author_literal_;
};

pair<string, string> BuildDateTimeLiterals()
{
	time_t now = time(nullptr);
	string asc = asctime(localtime(&now));
	string date = "\"" + asc.substr(4, 7) + asc.substr(20, 4) + "\"";
	string tm = "\"" + asc.substr(11, 8) + "\"";
	return make_pair(date, tm);
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;
		for (int i = 1; i < argc; ++i)
			args.emplace_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		string outfile = args[1];
		ofstream out(outfile);
		if (!out)
			throw runtime_error("cannot open output file: " + outfile);

		pair<string, string> build = BuildDateTimeLiterals();
		string author_literal = EscapeStringLiteral("OpenAI Codex");

		size_t nsrcfiles = args.size() - 2;
		out << "preproc " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; ++i)
		{
			string srcfile = args[i + 2];
			out << "sof " << srcfile << endl;
			Preprocessor preproc(build.first, build.second, author_literal);
			preproc.ProcessSourceFile(srcfile, out);
		}

		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
