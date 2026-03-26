// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

#define CPPGM_EMBED_POSTTOKEN 1
#include "posttoken.cpp"
#undef CPPGM_EMBED_POSTTOKEN

namespace pa4
{

using PPToken = pa2::PPToken;
using PPTokenType = pa2::PPTokenType;

bool IsWhitespace(const PPToken& t) { return t.type == PPTokenType::WhitespaceSequence; }
bool IsNewLine(const PPToken& t) { return t.type == PPTokenType::NewLine; }
bool IsIdentifier(const PPToken& t) { return t.type == PPTokenType::Identifier; }
bool IsOp(const PPToken& t, const string& s) { return t.type == PPTokenType::PreprocessingOpOrPunc && t.source == s; }
bool IsHash(const PPToken& t) { return t.type == PPTokenType::PreprocessingOpOrPunc && (t.source == "#" || t.source == "%:"); }

struct MToken
{
	PPTokenType type;
	string source;
	set<string> hide;
	bool unavailable = false;
	bool paste_op = false;
	bool placemarker = false;
	bool ws_before = false;
};

bool IsIdentifier(const MToken& t) { return !t.placemarker && t.type == PPTokenType::Identifier; }
bool IsOp(const MToken& t, const string& s) { return !t.placemarker && t.type == PPTokenType::PreprocessingOpOrPunc && t.source == s; }

struct Macro
{
	bool function_like = false;
	bool variadic = false;
	vector<string> params;                 // named params (without __VA_ARGS__)
	vector<PPToken> repl_tokens;           // no whitespace/newline
	vector<bool> repl_ws_before;           // same length as repl_tokens, ws before token i from previous token
};

struct MacroProcessor
{
	map<string, Macro> macros;
	vector<MToken> output_tokens;

	static vector<PPToken> LexPP(const string& text)
	{
		pa2::PPTokenCollector collector;
		PPTokenizer tokenizer(collector);
		for (size_t i = 0; i < text.size(); ++i)
			tokenizer.process(static_cast<unsigned char>(text[i]));
		tokenizer.process(EndOfFile);
		return collector.tokens;
	}

	static vector<MToken> LexPPNoWS(const string& text)
	{
		vector<PPToken> toks = LexPP(text);
		vector<MToken> out;
		for (size_t i = 0; i < toks.size(); ++i)
		{
			if (toks[i].type == PPTokenType::WhitespaceSequence || toks[i].type == PPTokenType::NewLine)
				continue;
			MToken t;
			t.type = toks[i].type;
			t.source = toks[i].source;
			out.push_back(t);
		}
		return out;
	}

	static string JoinForRetokenize(const vector<MToken>& toks)
	{
		string out;
		for (size_t i = 0; i < toks.size(); ++i)
		{
			if (toks[i].placemarker)
				continue;
			if (!out.empty())
				out.push_back(' ');
			out += toks[i].source;
		}
		return out;
	}

	static vector<PPToken> SplitLines(const vector<PPToken>& toks, vector<vector<PPToken>>& lines)
	{
		lines.clear();
		vector<PPToken> cur;
		for (size_t i = 0; i < toks.size(); ++i)
		{
			if (toks[i].type == PPTokenType::NewLine)
			{
				lines.push_back(cur);
				cur.clear();
			}
			else
			{
				cur.push_back(toks[i]);
			}
		}
		if (!cur.empty())
			lines.push_back(cur);
		return toks;
	}

	static set<string> MergeHide(const set<string>& a, const set<string>& b)
	{
		set<string> r = a;
		r.insert(b.begin(), b.end());
		return r;
	}

	static set<string> IntersectHide(const set<string>& a, const set<string>& b)
	{
		set<string> r;
		for (set<string>::const_iterator it = a.begin(); it != a.end(); ++it)
			if (b.count(*it))
				r.insert(*it);
		return r;
	}

	static MToken MakePP(const PPToken& t)
	{
		MToken out;
		out.type = t.type;
		out.source = t.source;
		return out;
	}

	static MToken MakeOp(const string& op)
	{
		MToken t;
		t.type = PPTokenType::PreprocessingOpOrPunc;
		t.source = op;
		return t;
	}

	static string EscapeStringLiteral(const string& s)
	{
		string out;
		out.push_back('"');
		for (size_t i = 0; i < s.size(); ++i)
		{
			char c = s[i];
			if (c == '\\' || c == '"')
				out.push_back('\\');
			out.push_back(c);
		}
		out.push_back('"');
		return out;
	}

	static string EscapeBackslashAndQuote(const string& s)
	{
		string out;
		for (size_t i = 0; i < s.size(); ++i)
		{
			char c = s[i];
			if (c == '\\' || c == '"')
				out.push_back('\\');
			out.push_back(c);
		}
		return out;
	}

	static vector<MToken> Stringize(const vector<MToken>& arg)
	{
		string merged;
		for (size_t i = 0; i < arg.size(); ++i)
		{
			if (i != 0 && (arg[i].ws_before || arg[i].source == "##" || arg[i - 1].source == "##"))
				merged.push_back(' ');

			if (arg[i].type == PPTokenType::StringLiteral ||
				arg[i].type == PPTokenType::UserDefinedStringLiteral ||
				arg[i].type == PPTokenType::CharacterLiteral ||
				arg[i].type == PPTokenType::UserDefinedCharacterLiteral)
			{
				merged += EscapeBackslashAndQuote(arg[i].source);
			}
			else
			{
				merged += arg[i].source;
			}
		}

		MToken t;
		t.type = PPTokenType::StringLiteral;
		t.source = "\"" + merged + "\"";
		return {t};
	}

	static vector<MToken> Paste(const MToken& lhs, const MToken& rhs)
	{
		string joined = lhs.source + rhs.source;
		vector<MToken> toks = LexPPNoWS(joined);
		for (size_t i = 0; i < toks.size(); ++i)
		{
			toks[i].hide = MergeHide(lhs.hide, rhs.hide);
			toks[i].unavailable = false;
			toks[i].ws_before = (i == 0) ? lhs.ws_before : false;
		}
		return toks;
	}

	void ValidateVaArgsToken(const MToken& t, const string& ctx)
	{
		if (IsIdentifier(t) && t.source == "__VA_ARGS__")
			throw runtime_error(ctx);
	}

	bool ParseInvocationArgs(const vector<MToken>& in, size_t lparen_i, size_t& end_i, vector<vector<MToken>>& args)
	{
		args.clear();
		if (lparen_i >= in.size() || !IsOp(in[lparen_i], "("))
			return false;

		vector<MToken> cur;
		int depth = 0;

		for (size_t i = lparen_i + 1; i < in.size(); ++i)
		{
			if (IsOp(in[i], "("))
			{
				++depth;
				cur.push_back(in[i]);
				continue;
			}
			if (IsOp(in[i], ")"))
			{
				if (depth == 0)
				{
					args.push_back(cur);
					end_i = i + 1;
					return true;
				}
				--depth;
				cur.push_back(in[i]);
				continue;
			}
			if (depth == 0 && IsOp(in[i], ","))
			{
				args.push_back(cur);
				cur.clear();
				continue;
			}
			cur.push_back(in[i]);
		}

		return false;
	}

	vector<MToken> ExpandTokens(const vector<MToken>& in)
	{
		vector<MToken> out;
		size_t i = 0;
		while (i < in.size())
		{
			MToken t = in[i];

			if (IsIdentifier(t) && t.source == "__VA_ARGS__")
				throw runtime_error("__VA_ARGS__ token in text-lines: __VA_ARGS__");

			if (!IsIdentifier(t))
			{
				out.push_back(t);
				++i;
				continue;
			}

			map<string, Macro>::iterator mit = macros.find(t.source);
			if (mit == macros.end())
			{
				out.push_back(t);
				++i;
				continue;
			}

			Macro& m = mit->second;
			const string macro_name = t.source;
			if (t.unavailable || t.hide.count(macro_name))
			{
				t.unavailable = true;
				out.push_back(t);
				++i;
				continue;
			}

			if (m.function_like)
			{
				if (i + 1 >= in.size() || !IsOp(in[i + 1], "("))
				{
					out.push_back(t);
					++i;
					continue;
				}

				size_t end_i = 0;
				vector<vector<MToken>> args;
				if (!ParseInvocationArgs(in, i + 1, end_i, args))
				{
					out.push_back(t);
					++i;
					continue;
				}

				const size_t fixed = m.params.size();
				if (fixed == 0 && args.size() == 1 && args[0].empty())
					args.clear();
				if (!m.variadic)
				{
					if (args.size() != fixed)
						throw runtime_error("macro function-like invocation wrong num of params: " + macro_name);
				}
				else
				{
					if (args.size() < fixed)
						throw runtime_error("macro function-like invocation wrong num of params: " + macro_name);
				}

				vector<MToken> vararg_raw;
				if (m.variadic)
				{
					for (size_t a = fixed; a < args.size(); ++a)
					{
						if (a != fixed)
							vararg_raw.push_back(MakeOp(","));
						vararg_raw.insert(vararg_raw.end(), args[a].begin(), args[a].end());
					}
				}

				map<string, vector<MToken>> raw_map;
				map<string, vector<MToken>> expanded_map;
				for (size_t p = 0; p < fixed; ++p)
					raw_map[m.params[p]] = args[p];
				if (m.variadic)
					raw_map["__VA_ARGS__"] = vararg_raw;
				set<string> disable_after_arg = t.hide;
				disable_after_arg.insert(macro_name);

				auto get_expanded = [&](const string& name) -> const vector<MToken>&
				{
					map<string, vector<MToken>>::iterator it = expanded_map.find(name);
					if (it != expanded_map.end())
						return it->second;
					vector<MToken> ex = ExpandTokens(raw_map[name]);
					for (size_t ai = 0; ai < ex.size(); ++ai)
					{
						if (IsIdentifier(ex[ai]) && disable_after_arg.count(ex[ai].source))
							ex[ai].unavailable = true;
					}
					expanded_map[name] = ex;
					return expanded_map[name];
				};

				set<string> base_hide = t.hide;
				base_hide.insert(macro_name);

				vector<MToken> subst;
				for (size_t r = 0; r < m.repl_tokens.size(); ++r)
				{
					const PPToken& rt = m.repl_tokens[r];

					if (IsOp(rt, "#"))
					{
						if (r + 1 >= m.repl_tokens.size() || !IsIdentifier(m.repl_tokens[r + 1]))
							throw runtime_error("# must be followed by parameter in function-like macro");
						const string& pname = m.repl_tokens[r + 1].source;
						if (!raw_map.count(pname))
							throw runtime_error("# must be followed by parameter in function-like macro");

						vector<MToken> str_toks = Stringize(raw_map[pname]);
						for (size_t k = 0; k < str_toks.size(); ++k)
						{
							str_toks[k].hide = base_hide;
							str_toks[k].unavailable = false;
							str_toks[k].ws_before = (k == 0) ? m.repl_ws_before[r] : str_toks[k].ws_before;
						}
						subst.insert(subst.end(), str_toks.begin(), str_toks.end());
						++r;
						continue;
					}

					if (IsOp(rt, "##"))
					{
						MToken p = MakeOp("##");
						p.paste_op = true;
						subst.push_back(p);
						continue;
					}

					if (IsIdentifier(rt) && raw_map.count(rt.source))
					{
						bool adj_left = (r > 0 && IsOp(m.repl_tokens[r - 1], "##"));
						bool adj_right = (r + 1 < m.repl_tokens.size() && IsOp(m.repl_tokens[r + 1], "##"));
						const vector<MToken>& use = (adj_left || adj_right) ? raw_map[rt.source] : get_expanded(rt.source);
						if (!use.empty())
						{
							vector<MToken> copied = use;
							for (size_t ck = 0; ck < copied.size(); ++ck)
								copied[ck].hide = MergeHide(copied[ck].hide, base_hide);
							copied[0].ws_before = copied[0].ws_before || m.repl_ws_before[r];
							subst.insert(subst.end(), copied.begin(), copied.end());
						}
						else
						{
							// keep empty substitution
						}
						continue;
					}

					MToken mt = MakePP(rt);
					mt.hide = base_hide;
					mt.ws_before = m.repl_ws_before[r];
					subst.push_back(mt);
				}

				vector<MToken> pasted;
				for (size_t s = 0; s < subst.size(); ++s)
				{
					if (!subst[s].paste_op)
					{
						pasted.push_back(subst[s]);
						continue;
					}

					MToken lhs;
					bool lhs_ok = false;
					if (!pasted.empty())
					{
						lhs = pasted.back();
						pasted.pop_back();
						lhs_ok = true;
					}
					else
					{
						lhs.placemarker = true;
					}

					MToken rhs;
					bool rhs_ok = false;
					if (s + 1 < subst.size() && !subst[s + 1].paste_op)
					{
						rhs = subst[s + 1];
						++s;
						rhs_ok = true;
					}
					else
					{
						rhs.placemarker = true;
					}

					if (!lhs_ok || lhs.placemarker)
					{
						if (!rhs_ok || rhs.placemarker)
						{
							MToken p;
							p.placemarker = true;
							pasted.push_back(p);
						}
						else
						{
							pasted.push_back(rhs);
						}
						continue;
					}

					if (!rhs_ok || rhs.placemarker)
					{
						pasted.push_back(lhs);
						continue;
					}

					vector<MToken> ptoks = Paste(lhs, rhs);
					if (ptoks.empty())
					{
						MToken p;
						p.placemarker = true;
						pasted.push_back(p);
					}
					else
					{
						pasted.insert(pasted.end(), ptoks.begin(), ptoks.end());
					}
				}

				vector<MToken> resc_in;
				for (size_t p = 0; p < pasted.size(); ++p)
					if (!pasted[p].placemarker)
						resc_in.push_back(pasted[p]);

				vector<MToken> tail = resc_in;
				tail.insert(tail.end(), in.begin() + end_i, in.end());
				vector<MToken> resc = ExpandTokens(tail);
				out.insert(out.end(), resc.begin(), resc.end());
				return out;
			}

			// object-like
			set<string> base_hide = t.hide;
			base_hide.insert(macro_name);

			vector<MToken> repl;
			for (size_t r = 0; r < m.repl_tokens.size(); ++r)
			{
				if (IsOp(m.repl_tokens[r], "##"))
				{
					MToken p = MakeOp("##");
					p.paste_op = true;
					p.hide = base_hide;
					repl.push_back(p);
				}
				else
				{
					MToken mt = MakePP(m.repl_tokens[r]);
					mt.hide = base_hide;
					mt.ws_before = m.repl_ws_before[r];
					repl.push_back(mt);
				}
			}

			vector<MToken> pasted;
			for (size_t s = 0; s < repl.size(); ++s)
			{
				if (!repl[s].paste_op)
				{
					pasted.push_back(repl[s]);
					continue;
				}

				MToken lhs;
				bool lhs_ok = false;
				if (!pasted.empty())
				{
					lhs = pasted.back();
					pasted.pop_back();
					lhs_ok = true;
				}
				else
				{
					lhs.placemarker = true;
				}

				MToken rhs;
				bool rhs_ok = false;
				if (s + 1 < repl.size() && !repl[s + 1].paste_op)
				{
					rhs = repl[s + 1];
					++s;
					rhs_ok = true;
				}
				else
				{
					rhs.placemarker = true;
				}

				if (!lhs_ok || lhs.placemarker)
				{
					if (!rhs_ok || rhs.placemarker)
					{
						MToken p;
						p.placemarker = true;
						pasted.push_back(p);
					}
					else
					{
						pasted.push_back(rhs);
					}
					continue;
				}

				if (!rhs_ok || rhs.placemarker)
				{
					pasted.push_back(lhs);
					continue;
				}

				vector<MToken> ptoks = Paste(lhs, rhs);
				if (ptoks.empty())
				{
					MToken p;
					p.placemarker = true;
					pasted.push_back(p);
				}
				else
				{
					pasted.insert(pasted.end(), ptoks.begin(), ptoks.end());
				}
			}

			vector<MToken> resc_in;
			for (size_t p = 0; p < pasted.size(); ++p)
				if (!pasted[p].placemarker)
					resc_in.push_back(pasted[p]);

			vector<MToken> tail = resc_in;
			tail.insert(tail.end(), in.begin() + i + 1, in.end());
			vector<MToken> resc = ExpandTokens(tail);
			out.insert(out.end(), resc.begin(), resc.end());
			return out;
		}

		return out;
	}

	static bool IsParamName(const vector<string>& params, const string& s)
	{
		return find(params.begin(), params.end(), s) != params.end();
	}

	void ValidateMacroReplacement(const Macro& m)
	{
		if (m.repl_tokens.empty())
			return;

		if (IsOp(m.repl_tokens.front(), "##") || IsOp(m.repl_tokens.back(), "##"))
			throw runtime_error("## at edge of replacement list");

		for (size_t i = 0; i < m.repl_tokens.size(); ++i)
		{
			const PPToken& t = m.repl_tokens[i];

			if (IsOp(t, "#"))
			{
				if (!m.function_like)
					continue;
				if (i + 1 >= m.repl_tokens.size())
					throw runtime_error("# at end of function-like macro replacement list");
				if (!IsIdentifier(m.repl_tokens[i + 1]) || !IsParamName(m.params, m.repl_tokens[i + 1].source) || m.repl_tokens[i + 1].source == "__VA_ARGS__")
				{
					if (!(m.variadic && IsIdentifier(m.repl_tokens[i + 1]) && m.repl_tokens[i + 1].source == "__VA_ARGS__"))
						throw runtime_error("# must be followed by parameter in function-like macro");
				}
			}

			if (IsIdentifier(t) && t.source == "__VA_ARGS__" && !m.variadic)
				throw runtime_error("invalid __VA_ARGS__ use");
		}
	}

	static bool MacrosEquivalent(const Macro& a, const Macro& b)
	{
		if (a.function_like != b.function_like || a.variadic != b.variadic)
			return false;
		if (a.params != b.params)
			return false;
		if (a.repl_tokens.size() != b.repl_tokens.size())
			return false;
		if (a.repl_ws_before.size() != b.repl_ws_before.size())
			return false;
		for (size_t i = 0; i < a.repl_tokens.size(); ++i)
		{
			if (a.repl_tokens[i].type != b.repl_tokens[i].type || a.repl_tokens[i].source != b.repl_tokens[i].source)
				return false;
			if (a.repl_ws_before[i] != b.repl_ws_before[i])
				return false;
		}
		return true;
	}

	void DefineMacro(const string& name, const Macro& m)
	{
		map<string, Macro>::iterator it = macros.find(name);
		if (it == macros.end())
		{
			macros[name] = m;
			return;
		}

		if (!MacrosEquivalent(it->second, m))
			throw runtime_error("macro redefined");
	}

	void ParseDirectiveLine(const vector<PPToken>& line)
	{
		size_t i = 0;
		while (i < line.size() && IsWhitespace(line[i]))
			++i;
		if (i >= line.size() || !IsHash(line[i]))
			throw runtime_error("invalid directive");
		++i;
		while (i < line.size() && IsWhitespace(line[i]))
			++i;
		if (i >= line.size() || !IsIdentifier(line[i]))
			throw runtime_error("expected directive name");

		const string directive = line[i].source;
		++i;

		if (directive == "undef")
		{
			while (i < line.size() && IsWhitespace(line[i]))
				++i;
			if (i >= line.size() || !IsIdentifier(line[i]))
				throw runtime_error("expected identifier");
			string name = line[i].source;
			if (name == "__VA_ARGS__")
				throw runtime_error("invalid __VA_ARGS__ use");
			++i;
			while (i < line.size() && IsWhitespace(line[i]))
				++i;
			if (i != line.size())
				throw runtime_error("expected new line");
			macros.erase(name);
			return;
		}

		if (directive != "define")
			throw runtime_error("unsupported directive");

		while (i < line.size() && IsWhitespace(line[i]))
			++i;
		if (i >= line.size() || !IsIdentifier(line[i]))
			throw runtime_error("expected identifier");

		string name = line[i].source;
		if (name == "__VA_ARGS__")
			throw runtime_error("invalid __VA_ARGS__ use");
		++i;

		Macro m;
		bool function_like = false;
		size_t after_name = i;
		if (after_name < line.size() && IsOp(line[after_name], "("))
			function_like = true;

		m.function_like = function_like;

		if (function_like)
		{
			++i; // consume '('

			vector<string> params;
			bool variadic = false;

			auto skip_ws = [&]()
			{
				while (i < line.size() && IsWhitespace(line[i]))
					++i;
			};

			skip_ws();
			if (i < line.size() && IsOp(line[i], ")"))
			{
				++i;
			}
			else
			{
				for (;;)
				{
					skip_ws();
					if (i >= line.size())
						throw runtime_error("expected rparen (#2)");

					if (IsOp(line[i], "..."))
					{
						variadic = true;
						++i;
						skip_ws();
						if (i >= line.size() || !IsOp(line[i], ")"))
							throw runtime_error("expected rparen (#1)");
						++i;
						break;
					}

					if (!IsIdentifier(line[i]))
						throw runtime_error("expected identifier");
					if (line[i].source == "__VA_ARGS__")
						throw runtime_error("__VA_ARGS__ in macro parameter list");
					if (find(params.begin(), params.end(), line[i].source) != params.end())
						throw runtime_error("duplicate parameter " + line[i].source + " in macro definition");
					params.push_back(line[i].source);
					++i;

					skip_ws();
					if (i < line.size() && IsOp(line[i], ")"))
					{
						++i;
						break;
					}
					if (i < line.size() && IsOp(line[i], ","))
					{
						++i;
						skip_ws();
						if (i < line.size() && IsOp(line[i], "..."))
						{
							variadic = true;
							++i;
							skip_ws();
							if (i >= line.size() || !IsOp(line[i], ")"))
								throw runtime_error("expected rparen (#3)");
							++i;
							break;
						}
						continue;
					}

					throw runtime_error("expected rparen (#4)");
				}
			}

			m.params = params;
			m.variadic = variadic;
		}

		while (i < line.size() && IsWhitespace(line[i]))
			++i;

		bool have_prev = false;
		bool saw_ws = false;
		for (; i < line.size(); ++i)
		{
			if (IsWhitespace(line[i]))
			{
				saw_ws = true;
				continue;
			}
			m.repl_tokens.push_back(line[i]);
			m.repl_ws_before.push_back(have_prev && saw_ws);
			have_prev = true;
			saw_ws = false;
		}

		ValidateMacroReplacement(m);
		DefineMacro(name, m);
	}

	void FlushText(vector<PPToken>& text_tokens)
	{
		if (text_tokens.empty())
			return;

		vector<MToken> toks;
		bool saw_ws = true;
		for (size_t i = 0; i < text_tokens.size(); ++i)
		{
			if (text_tokens[i].type == PPTokenType::WhitespaceSequence || text_tokens[i].type == PPTokenType::NewLine)
			{
				saw_ws = true;
				continue;
			}
			MToken t = MakePP(text_tokens[i]);
			t.ws_before = saw_ws;
			saw_ws = false;
			ValidateVaArgsToken(t, "__VA_ARGS__ token in text-lines: __VA_ARGS__");
			toks.push_back(t);
		}

		vector<MToken> ex = ExpandTokens(toks);
		for (size_t i = 0; i < ex.size(); ++i)
		{
			ValidateVaArgsToken(ex[i], "__VA_ARGS__ token in text-lines: __VA_ARGS__");
			if (!ex[i].placemarker)
				output_tokens.push_back(ex[i]);
		}

		text_tokens.clear();
	}

	void Run(const string& input)
	{
		vector<PPToken> toks = LexPP(input);
		vector<vector<PPToken>> lines;
		SplitLines(toks, lines);

		vector<PPToken> text_accum;

		for (size_t li = 0; li < lines.size(); ++li)
		{
			const vector<PPToken>& line = lines[li];
			size_t i = 0;
			while (i < line.size() && IsWhitespace(line[i]))
				++i;

			bool is_directive = i < line.size() && IsHash(line[i]);
			if (is_directive)
			{
				FlushText(text_accum);
				ParseDirectiveLine(line);
			}
			else
			{
				text_accum.insert(text_accum.end(), line.begin(), line.end());
				PPToken nl;
				nl.type = PPTokenType::WhitespaceSequence;
				text_accum.push_back(nl);
			}
		}

		FlushText(text_accum);
	}
};

void EmitPostTokensFromExpanded(const vector<MToken>& expanded, DebugPostTokenOutputStream& output)
{
	vector<pa2::PPToken> tokens;
	for (size_t i = 0; i < expanded.size(); ++i)
	{
		if (expanded[i].placemarker)
			continue;
		pa2::PPToken t;
		t.type = expanded[i].type;
		t.source = expanded[i].source;
		tokens.push_back(t);
	}

	size_t i = 0;
	while (i < tokens.size())
	{
		pa2::PPTokenType t = tokens[i].type;
		const string& source = tokens[i].source;

		if (t == pa2::PPTokenType::StringLiteral || t == pa2::PPTokenType::UserDefinedStringLiteral)
		{
			size_t j = i;
			while (j < tokens.size() && (tokens[j].type == pa2::PPTokenType::StringLiteral || tokens[j].type == pa2::PPTokenType::UserDefinedStringLiteral))
				++j;
			pa2::ProcessStringRun(output, tokens, i, j);
			i = j;
			continue;
		}

		if (t == pa2::PPTokenType::HeaderName || t == pa2::PPTokenType::NonWhitespaceCharacter)
		{
			output.emit_invalid(source);
			++i;
			continue;
		}

		if (t == pa2::PPTokenType::Identifier)
		{
			auto it = StringToTokenTypeMap.find(source);
			if (it != StringToTokenTypeMap.end())
				output.emit_simple(source, it->second);
			else
				output.emit_identifier(source);
			++i;
			continue;
		}

		if (t == pa2::PPTokenType::PreprocessingOpOrPunc)
		{
			if (source == "#" || source == "##" || source == "%:" || source == "%:%:")
			{
				output.emit_invalid(source);
			}
			else
			{
				auto it = StringToTokenTypeMap.find(source);
				if (it != StringToTokenTypeMap.end())
					output.emit_simple(source, it->second);
				else
					output.emit_invalid(source);
			}
			++i;
			continue;
		}

		if (t == pa2::PPTokenType::PPNumber)
		{
			pa2::ProcessPPNumber(output, source);
			++i;
			continue;
		}

		if (t == pa2::PPTokenType::CharacterLiteral)
		{
			pa2::ProcessCharacterLiteral(output, source, false);
			++i;
			continue;
		}

		if (t == pa2::PPTokenType::UserDefinedCharacterLiteral)
		{
			pa2::ProcessCharacterLiteral(output, source, true);
			++i;
			continue;
		}

		output.emit_invalid(source);
		++i;
	}
}

} // namespace pa4

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		string input = oss.str();

		pa4::MacroProcessor p;
		p.Run(input);

		DebugPostTokenOutputStream out;
		pa4::EmitPostTokensFromExpanded(p.output_tokens, out);
		out.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
