// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

#define CPPGM_POSTTOKEN_LIBRARY
#include "posttoken.cpp"

struct MacroToken
{
	EPPTokenType type = PPT_EOF;
	string data;
	vector<string> blocked;
	bool placemarker = false;
	bool inert_hashhash = false;
};

struct MacroDef
{
	string name;
	bool function_like = false;
	bool variadic = false;
	vector<string> params;
	vector<MacroToken> replacement;
};

bool IsWhitespaceToken(const MacroToken& token)
{
	return token.type == PPT_WHITESPACE_SEQUENCE || token.type == PPT_NEW_LINE;
}

bool IsIdentifierToken(const MacroToken& token)
{
	return !token.placemarker && token.type == PPT_IDENTIFIER;
}

bool IsPuncToken(const MacroToken& token, const string& text)
{
	return !token.placemarker && token.type == PPT_PREPROCESSING_OP_OR_PUNC && token.data == text;
}

MacroToken MakeWhitespaceToken()
{
	MacroToken token;
	token.type = PPT_WHITESPACE_SEQUENCE;
	token.data = " ";
	return token;
}

MacroToken MakePlacemakerToken()
{
	MacroToken token;
	token.placemarker = true;
	return token;
}

MacroToken FromPPToken(const PPToken& token)
{
	MacroToken out;
	out.type = token.type;
	out.data = token.data;
	return out;
}

PPToken ToPPToken(const MacroToken& token)
{
	PPToken out;
	out.type = token.type;
	out.data = token.data;
	return out;
}

bool ContainsBlocked(const MacroToken& token, const string& name)
{
	for (const string& item : token.blocked)
	{
		if (item == name)
			return true;
	}
	return false;
}

void AddBlocked(MacroToken& token, const string& name)
{
	if (name.empty() || ContainsBlocked(token, name))
		return;
	token.blocked.push_back(name);
}

void UnionBlocked(MacroToken& token, const vector<string>& blocked)
{
	for (const string& name : blocked)
		AddBlocked(token, name);
}

vector<MacroToken> NormalizeWhitespace(const vector<MacroToken>& input)
{
	vector<MacroToken> out;
	bool pending_ws = false;
	for (const MacroToken& token : input)
	{
		if (IsWhitespaceToken(token))
		{
			pending_ws = !out.empty();
			continue;
		}
		if (pending_ws)
		{
			out.push_back(MakeWhitespaceToken());
			pending_ws = false;
		}
		out.push_back(token);
	}
	if (!out.empty() && IsWhitespaceToken(out.back()))
		out.pop_back();
	return out;
}

bool TokensEqual(const vector<MacroToken>& a, const vector<MacroToken>& b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].placemarker != b[i].placemarker)
			return false;
		if (a[i].inert_hashhash != b[i].inert_hashhash)
			return false;
		if (a[i].type != b[i].type)
			return false;
		if (a[i].data != b[i].data)
			return false;
	}
	return true;
}

string StringizeTokens(const vector<MacroToken>& input)
{
	vector<MacroToken> tokens = NormalizeWhitespace(input);
	string text;
	bool need_space = false;
	for (const MacroToken& token : tokens)
	{
		if (IsWhitespaceToken(token))
		{
			need_space = true;
			continue;
		}
		if (token.placemarker)
			continue;
		if (need_space && !text.empty())
			text.push_back(' ');
		need_space = false;

		bool escape_token = token.type == PPT_STRING_LITERAL ||
			token.type == PPT_USER_DEFINED_STRING_LITERAL ||
			token.type == PPT_CHARACTER_LITERAL ||
			token.type == PPT_USER_DEFINED_CHARACTER_LITERAL;
		if (!escape_token)
		{
			text += token.data;
			continue;
		}

		for (char c : token.data)
		{
			if (c == '\\' || c == '"')
				text.push_back('\\');
			text.push_back(c);
		}
	}

	return "\"" + text + "\"";
}

vector<MacroToken> RetokenizeConcat(const string& text)
{
	vector<PPToken> tokens = LexPPTokens(text);
	vector<MacroToken> out;
	for (const PPToken& token : tokens)
	{
		if (token.type == PPT_EOF || token.type == PPT_NEW_LINE || token.type == PPT_WHITESPACE_SEQUENCE)
			continue;
		out.push_back(FromPPToken(token));
	}
	return out;
}

vector<MacroToken> MarkSubstitutedTokens(vector<MacroToken> tokens)
{
	for (MacroToken& token : tokens)
		token.inert_hashhash = true;
	return tokens;
}

class MacroProcessor
{
public:
	void Process(const string& input, ostream& out)
	{
		vector<PPToken> tokens = LexPPTokens(input);
		vector<vector<PPToken> > lines = SplitLines(tokens);

		for (const vector<PPToken>& line : lines)
		{
			if (IsDirectiveLine(line))
			{
				FlushText();
				HandleDirective(line);
			}
			else
			{
				AppendTextLine(line);
			}
		}

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

		DebugPostTokenOutputStream debug(out);
		EmitPostTokenStream(final_tokens, debug, true);
	}

private:
	vector<vector<PPToken> > SplitLines(const vector<PPToken>& tokens)
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

	void Fail(const string& message) const
	{
		throw runtime_error(message);
	}

	vector<MacroToken> NormalizeReplacementTokens(const vector<PPToken>& tokens) const
	{
		vector<MacroToken> out;
		for (const PPToken& token : tokens)
			out.push_back(FromPPToken(token));
		return NormalizeWhitespace(out);
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

	int FindParamIndex(const MacroDef& def, const string& name) const
	{
		for (size_t i = 0; i < def.params.size(); ++i)
		{
			if (def.params[i] == name)
				return static_cast<int>(i);
		}
		return -1;
	}

	void CheckReplacementVarArgs(const MacroDef& def) const
	{
		for (const MacroToken& token : def.replacement)
		{
			if (IsIdentifierToken(token) && token.data == "__VA_ARGS__" && !def.variadic)
				Fail("invalid __VA_ARGS__ use");
		}
	}

	void HandleDirective(const vector<PPToken>& line)
	{
		size_t i = SkipWhitespace(line, 0);
		++i; // skip #
		i = SkipWhitespace(line, i);
		if (i >= line.size() || line[i].type != PPT_IDENTIFIER)
			Fail("expected directive identifier");

		string directive = line[i].data;
		++i;
		if (directive == "define")
			HandleDefine(line, i);
		else if (directive == "undef")
			HandleUndef(line, i);
		else
			Fail("unsupported directive");
	}

	void HandleUndef(const vector<PPToken>& line, size_t i)
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

	void HandleDefine(const vector<PPToken>& line, size_t i)
	{
		i = SkipWhitespace(line, i);
		if (i >= line.size() || line[i].type != PPT_IDENTIFIER)
			Fail("expected identifier");
		if (line[i].data == "__VA_ARGS__")
			Fail("invalid __VA_ARGS__ use");

		MacroDef def;
		def.name = line[i].data;
		++i;

		bool function_like = false;
		if (i < line.size() && IsRawPunc(line[i], "("))
			function_like = true;

		if (function_like)
		{
			def.function_like = true;
			ParseParamList(line, i, def);
		}

		vector<PPToken> replacement_tokens(line.begin() + static_cast<ptrdiff_t>(i), line.end());
		def.replacement = NormalizeReplacementTokens(replacement_tokens);

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

	void ParseParamList(const vector<PPToken>& line, size_t& i, MacroDef& def)
	{
		++i; // skip lparen
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
					Fail("expected rparen (#2): PP_NEW_LINE()");
				++i;
				return;
			}
			if (line[i].type != PPT_IDENTIFIER)
				Fail("expected identifier");
		}
	}

	void AppendTextLine(const vector<PPToken>& line)
	{
		vector<MacroToken> converted = NormalizeReplacementTokens(line);
		if (converted.empty())
			return;
		if (!text_.empty())
			text_.push_back(MakeWhitespaceToken());
		text_.insert(text_.end(), converted.begin(), converted.end());
	}

	void FlushText()
	{
		text_ = NormalizeWhitespace(text_);
		if (text_.empty())
			return;

		vector<MacroToken> expanded = ExpandSequence(text_);
		expanded = NormalizeWhitespace(expanded);
		if (!expanded.empty() && !output_.empty())
			output_.push_back(MakeWhitespaceToken());
		output_.insert(output_.end(), expanded.begin(), expanded.end());
		text_.clear();
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

	void ParseInvocation(const MacroDef& def, const vector<MacroToken>& seq, size_t lparen_index, size_t& end_index, vector<vector<MacroToken> >& args)
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
			if (!def.variadic && def.params.empty())
			{
				args.clear();
			}
			else if (def.variadic && def.params.empty())
			{
				args.clear();
			}
			else
			{
				args.push_back(vector<MacroToken>());
			}
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
		{
			get_raw_varargs();
		}

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
			UnionBlocked(token, head.blocked);
			AddBlocked(token, def.name);
		}
		return substituted;
	}

vector<MacroToken> PasteTokens(const MacroToken& left, const MacroToken& right)
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

	vector<MacroToken> ApplyHashHash(vector<MacroToken> tokens)
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

	map<string, MacroDef> macros_;
	vector<MacroToken> text_;
	vector<MacroToken> output_;
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		MacroProcessor processor;
		processor.Process(oss.str(), cout);
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
