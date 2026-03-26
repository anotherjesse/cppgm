// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <limits>

using namespace std;

#define POSTTOKEN_EMBED_ONLY
#include "posttoken.cpp"
#undef POSTTOKEN_EMBED_ONLY

struct ExpToken
{
	PPToken pp;
	set<string> blacklist;
	bool noninvokable = false;
	bool is_concat_op = false;
};

struct MacroDef
{
	bool function_like = false;
	bool variadic = false;
	vector<string> params;
	vector<PPToken> replacement;
};

struct MacroError : runtime_error
{
	explicit MacroError(const string& s) : runtime_error(s) {}
};

bool IsWS(const PPToken& t)
{
	return t.type == "whitespace-sequence" || t.type == "new-line";
}

bool IsIdent(const PPToken& t, const string& s)
{
	return t.type == "identifier" && t.data == s;
}

bool IsOp(const PPToken& t, const string& s)
{
	return t.type == "preprocessing-op-or-punc" && t.data == s;
}

string RenderQuoted(const vector<ExpToken>& arg)
{
	string core;
	bool pending_space = false;
	for (const ExpToken& t : arg)
	{
		if (IsWS(t.pp))
		{
			if (!core.empty()) pending_space = true;
			continue;
		}
		if (pending_space)
		{
			core.push_back(' ');
			pending_space = false;
		}
		bool escape_backslash = (t.pp.type == "string-literal" ||
			t.pp.type == "user-defined-string-literal" ||
			t.pp.type == "character-literal" ||
			t.pp.type == "user-defined-character-literal");
		for (char c : t.pp.data)
		{
			if (c == '"') core.push_back('\\');
			if (c == '\\' && escape_backslash) core.push_back('\\');
			core.push_back(c);
		}
	}

	string out = "\"";
	for (char c : core) out.push_back(c);
	out.push_back('"');
	return out;
}

vector<ExpToken> ToExp(const vector<PPToken>& in)
{
	vector<ExpToken> out;
	for (const PPToken& t : in)
	{
		if (t.type == "eof") continue;
		ExpToken e;
		e.pp = t;
		out.push_back(e);
	}
	return out;
}

vector<ExpToken> TokenizeAsExp(const string& s)
{
	vector<PPToken> toks = TokenizeToPPTokens(s);
	vector<ExpToken> out;
	for (const PPToken& t : toks)
	{
		if (t.type == "eof" || IsWS(t)) continue;
		ExpToken e;
		e.pp = t;
		out.push_back(e);
	}
	return out;
}

vector<PPToken> CanonicalReplacement(const vector<PPToken>& in)
{
	vector<PPToken> out;
	bool in_ws = false;
	for (const PPToken& t : in)
	{
		if (IsWS(t))
		{
			in_ws = true;
			continue;
		}
		if (!out.empty() && in_ws)
		{
			out.push_back({"whitespace-sequence", ""});
		}
		in_ws = false;
		out.push_back(t);
	}
	return out;
}

bool SameMacroDef(const MacroDef& a, const MacroDef& b)
{
	if (a.function_like != b.function_like) return false;
	if (a.variadic != b.variadic) return false;
	if (a.params != b.params) return false;
	vector<PPToken> ca = CanonicalReplacement(a.replacement);
	vector<PPToken> cb = CanonicalReplacement(b.replacement);
	if (ca.size() != cb.size()) return false;
	for (size_t i = 0; i < ca.size(); i++)
	{
		if (ca[i].type != cb[i].type) return false;
		if (ca[i].data != cb[i].data) return false;
	}
	return true;
}

void CheckVarArgsUse(const MacroDef& m)
{
	for (const PPToken& t : m.replacement)
	{
		if (IsIdent(t, "__VA_ARGS__") && !m.variadic)
		{
			throw MacroError("invalid __VA_ARGS__ use");
		}
	}
}

void ValidateFunctionReplacement(const MacroDef& m)
{
	vector<PPToken> nw;
	for (const PPToken& t : m.replacement) if (!IsWS(t)) nw.push_back(t);

	for (size_t i = 0; i < m.replacement.size(); i++)
	{
		const PPToken& t = m.replacement[i];
		if (!IsOp(t, "#")) continue;
		size_t j = i + 1;
		while (j < m.replacement.size() && IsWS(m.replacement[j])) j++;
		if (j >= m.replacement.size()) throw MacroError("# at end of function-like macro replacement list");
		if (m.replacement[j].type != "identifier") throw MacroError("# must be followed by parameter in function-like macro");
		const string& id = m.replacement[j].data;
		bool ok = false;
		for (const string& p : m.params) if (p == id) ok = true;
		if (m.variadic && id == "__VA_ARGS__") ok = true;
		if (!ok) throw MacroError("# must be followed by parameter in function-like macro");
	}
}

void ValidateHashHashEdges(const MacroDef& m)
{
	vector<PPToken> nw;
	for (const PPToken& t : m.replacement) if (!IsWS(t)) nw.push_back(t);
	for (size_t i = 0; i < nw.size(); i++)
	{
		if (IsOp(nw[i], "##") && (i == 0 || i + 1 == nw.size()))
		{
			throw MacroError("## at edge of replacement list");
		}
	}
}

struct MacroEngine
{
	map<string, MacroDef> macros;

	vector<ExpToken> Expand(vector<ExpToken> toks)
	{
		size_t i = 0;
		while (i < toks.size())
		{
			ExpToken& head = toks[i];
			if (head.pp.type != "identifier")
			{
				i++;
				continue;
			}

			if (head.pp.data == "__VA_ARGS__") throw MacroError("__VA_ARGS__ token in text-lines: __VA_ARGS__");

			if (head.noninvokable || head.blacklist.count(head.pp.data))
			{
				head.noninvokable = true;
				i++;
				continue;
			}

			auto mit = macros.find(head.pp.data);
			if (mit == macros.end())
			{
				i++;
				continue;
			}

			const MacroDef& m = mit->second;
			if (!m.function_like)
			{
				vector<ExpToken> repl = ApplyObjectLike(m, head);
				toks.erase(toks.begin() + static_cast<long>(i));
				toks.insert(toks.begin() + static_cast<long>(i), repl.begin(), repl.end());
				continue;
			}

			size_t lp = i + 1;
			while (lp < toks.size() && IsWS(toks[lp].pp)) lp++;
			if (lp >= toks.size() || !IsOp(toks[lp].pp, "("))
			{
				i++;
				continue;
			}

			size_t rp = lp;
			int depth = 0;
			bool found = false;
			for (; rp < toks.size(); rp++)
			{
				if (IsOp(toks[rp].pp, "(")) depth++;
				else if (IsOp(toks[rp].pp, ")"))
				{
					depth--;
					if (depth == 0)
					{
						found = true;
						break;
					}
				}
			}
			if (!found)
			{
				i++;
				continue;
			}

			vector<vector<ExpToken> > args = ParseArgs(toks, lp, rp);
			if (m.params.empty() && !m.variadic && args.size() == 1 && ArgIsEmpty(args[0])) args.clear();
			if (!m.variadic)
			{
				if (args.size() != m.params.size()) throw MacroError("macro function-like invocation wrong num of params: " + head.pp.data);
			}
			else
			{
				if (args.size() < m.params.size()) throw MacroError("macro function-like invocation wrong num of params: " + head.pp.data);
			}

			vector<ExpToken> repl = ApplyFunctionLike(m, head, args);
			toks.erase(toks.begin() + static_cast<long>(i), toks.begin() + static_cast<long>(rp + 1));
			toks.insert(toks.begin() + static_cast<long>(i), repl.begin(), repl.end());
		}
		return toks;
	}

	vector<vector<ExpToken> > ParseArgs(const vector<ExpToken>& toks, size_t lp, size_t rp)
	{
		vector<vector<ExpToken> > args;
		vector<ExpToken> cur;
		int depth = 0;
		for (size_t k = lp + 1; k < rp; k++)
		{
			if (IsOp(toks[k].pp, "("))
			{
				depth++;
				cur.push_back(toks[k]);
			}
			else if (IsOp(toks[k].pp, ")"))
			{
				depth--;
				cur.push_back(toks[k]);
			}
			else if (depth == 0 && IsOp(toks[k].pp, ","))
			{
				args.push_back(cur);
				cur.clear();
			}
			else
			{
				cur.push_back(toks[k]);
			}
		}

		args.push_back(cur);
		return args;
	}

	bool ArgIsEmpty(const vector<ExpToken>& a)
	{
		for (const ExpToken& t : a) if (!IsWS(t.pp)) return false;
		return true;
	}

	vector<ExpToken> NormalizeProduced(const vector<ExpToken>& in, const ExpToken& head)
	{
		vector<ExpToken> out = in;
		for (ExpToken& t : out)
		{
			set<string> nb = head.blacklist;
			nb.insert(head.pp.data);
			t.blacklist = nb;
		}
		return out;
	}

	vector<ExpToken> FoldConcat(const vector<ExpToken>& substituted)
	{
		vector<ExpToken> folded;
		for (size_t i = 0; i < substituted.size(); i++)
		{
			if (!substituted[i].is_concat_op)
			{
				folded.push_back(substituted[i]);
				continue;
			}

			string lhs;
			set<string> bl;
			while (!folded.empty() && IsWS(folded.back().pp)) folded.pop_back();
			if (!folded.empty())
			{
				lhs = folded.back().pp.data;
				bl = folded.back().blacklist;
				folded.pop_back();
			}

			size_t j = i + 1;
			while (j < substituted.size() && IsWS(substituted[j].pp)) j++;
			string rhs;
			if (j < substituted.size() && !substituted[j].is_concat_op)
			{
				rhs = substituted[j].pp.data;
				bl.insert(substituted[j].blacklist.begin(), substituted[j].blacklist.end());
				i = j;
			}

			vector<ExpToken> pasted = TokenizeAsExp(lhs + rhs);
			for (ExpToken& p : pasted)
			{
				p.blacklist = bl;
			}
			folded.insert(folded.end(), pasted.begin(), pasted.end());
		}
		return folded;
	}

	vector<ExpToken> ApplyObjectLike(const MacroDef& m, const ExpToken& head)
	{
		vector<ExpToken> repl;
		for (const PPToken& t : m.replacement)
		{
			if (IsWS(t)) continue;
			ExpToken e;
			e.pp = t;
			if (IsOp(t, "##")) e.is_concat_op = true;
			repl.push_back(e);
		}
		repl = FoldConcat(repl);
		return NormalizeProduced(repl, head);
	}

	static bool IsParam(const MacroDef& m, const string& id)
	{
		for (const string& p : m.params) if (p == id) return true;
		if (m.variadic && id == "__VA_ARGS__") return true;
		return false;
	}

	vector<ExpToken> ExpandArg(const vector<ExpToken>& a)
	{
		return Expand(a);
	}

	vector<ExpToken> ApplyFunctionLike(const MacroDef& m, const ExpToken& head, const vector<vector<ExpToken> >& args)
	{
		map<string, vector<ExpToken> > raw;
		for (size_t i = 0; i < m.params.size(); i++) raw[m.params[i]] = args[i];
		if (m.variadic)
		{
			vector<ExpToken> va;
			for (size_t i = m.params.size(); i < args.size(); i++)
			{
				if (!va.empty())
				{
					ExpToken c;
					c.pp = {"preprocessing-op-or-punc", ","};
					va.push_back(c);
				}
				va.insert(va.end(), args[i].begin(), args[i].end());
			}
			raw["__VA_ARGS__"] = va;
		}

		map<string, vector<ExpToken> > expanded;
		auto get_expanded = [&](const string& pname) -> const vector<ExpToken>&
		{
			auto it = expanded.find(pname);
			if (it == expanded.end())
			{
				it = expanded.insert(make_pair(pname, ExpandArg(raw[pname]))).first;
			}
			return it->second;
		};

		vector<ExpToken> substituted;
		for (size_t i = 0; i < m.replacement.size(); i++)
		{
			const PPToken& t = m.replacement[i];

			if (IsOp(t, "#"))
			{
				size_t j = i + 1;
				while (j < m.replacement.size() && IsWS(m.replacement[j])) j++;
				const string& pname = m.replacement[j].data;
				ExpToken st;
				st.pp = {"string-literal", RenderQuoted(raw[pname])};
				substituted.push_back(st);
				i = j;
				continue;
			}

			if (t.type == "identifier" && IsParam(m, t.data))
			{
				bool near_hashhash = false;
				for (size_t l = i; l-- > 0;)
				{
					if (IsWS(m.replacement[l])) continue;
					if (IsOp(m.replacement[l], "##")) near_hashhash = true;
					break;
				}
				for (size_t r = i + 1; r < m.replacement.size(); r++)
				{
					if (IsWS(m.replacement[r])) continue;
					if (IsOp(m.replacement[r], "##")) near_hashhash = true;
					break;
				}

				const vector<ExpToken>& src = near_hashhash ? raw[t.data] : get_expanded(t.data);
				substituted.insert(substituted.end(), src.begin(), src.end());
				continue;
			}

			ExpToken e;
			e.pp = t;
			if (IsOp(t, "##")) e.is_concat_op = true;
			substituted.push_back(e);
		}

		vector<ExpToken> folded = FoldConcat(substituted);
		return NormalizeProduced(folded, head);
	}
};

struct Driver
{
	MacroEngine eng;
	vector<PPToken> out_pp;
	vector<PPToken> text_acc;

	static bool AtLineStart(const vector<PPToken>& toks, size_t idx)
	{
		if (idx == 0) return true;
		return toks[idx - 1].type == "new-line";
	}

	void FlushText()
	{
		if (text_acc.empty()) return;
		vector<ExpToken> exp = ToExp(text_acc);
		for (ExpToken& e : exp)
		{
			if (e.pp.type == "new-line") e.pp.type = "whitespace-sequence";
		}
		vector<ExpToken> rep = eng.Expand(exp);
		for (const ExpToken& e : rep)
		{
			if (IsWS(e.pp)) continue;
			out_pp.push_back(e.pp);
		}
		text_acc.clear();
	}

	void Parse(const vector<PPToken>& toks)
	{
		size_t i = 0;
		while (i < toks.size() && toks[i].type != "eof")
		{
			size_t line_end = i;
			while (line_end < toks.size() && toks[line_end].type != "new-line" && toks[line_end].type != "eof") line_end++;

			bool directive = false;
			size_t p = i;
			if (AtLineStart(toks, i))
			{
				while (p < line_end && toks[p].type == "whitespace-sequence") p++;
				if (p < line_end && IsOp(toks[p], "#")) directive = true;
			}

			if (directive)
			{
				FlushText();
				ParseDirective(toks, i, line_end, p);
			}
			else
			{
				for (size_t k = i; k < line_end; k++) text_acc.push_back(toks[k]);
				if (line_end < toks.size() && toks[line_end].type == "new-line") text_acc.push_back(toks[line_end]);
			}

			i = line_end;
			if (i < toks.size() && toks[i].type == "new-line") i++;
		}
		FlushText();
	}

	void ParseDirective(const vector<PPToken>& toks, size_t line_start, size_t line_end, size_t hash_pos)
	{
		size_t p = hash_pos + 1;
		while (p < line_end && IsWS(toks[p])) p++;
		if (p >= line_end || toks[p].type != "identifier") throw MacroError("expected identifier");
		string kw = toks[p].data;
		p++;
		if (kw == "define") ParseDefine(toks, line_end, p);
		else if (kw == "undef") ParseUndef(toks, line_end, p);
		else throw MacroError("expected identifier");
	}

	void ParseUndef(const vector<PPToken>& toks, size_t line_end, size_t& p)
	{
		while (p < line_end && IsWS(toks[p])) p++;
		if (p >= line_end || toks[p].type != "identifier") throw MacroError("expected identifier");
		string name = toks[p].data;
		if (name == "__VA_ARGS__") throw MacroError("invalid __VA_ARGS__ use");
		p++;
		while (p < line_end && IsWS(toks[p])) p++;
		if (p != line_end) throw MacroError("expected new line");
		eng.macros.erase(name);
	}

	void ParseDefine(const vector<PPToken>& toks, size_t line_end, size_t& p)
	{
		while (p < line_end && IsWS(toks[p])) p++;
		if (p >= line_end || toks[p].type != "identifier") throw MacroError("expected identifier");
		string name = toks[p].data;
		if (name == "__VA_ARGS__") throw MacroError("invalid __VA_ARGS__ use");
		p++;

		MacroDef m;

		bool fn = false;
		if (p < line_end && !IsWS(toks[p]) && IsOp(toks[p], "(")) fn = true;
		if (fn)
		{
			m.function_like = true;
			p++;
			while (true)
			{
				while (p < line_end && IsWS(toks[p])) p++;
				if (p >= line_end) throw MacroError("expected rparen (#2): PP_NEW_LINE()");
				if (IsOp(toks[p], ")"))
				{
					p++;
					break;
				}
				if (IsOp(toks[p], "..."))
				{
					m.variadic = true;
					p++;
					while (p < line_end && IsWS(toks[p])) p++;
					if (p >= line_end || !IsOp(toks[p], ")")) throw MacroError("expected rparen (#2): PP_NEW_LINE()");
					p++;
					break;
				}
				if (toks[p].type != "identifier") throw MacroError("expected identifier after lparen");
				string param = toks[p].data;
				if (param == "__VA_ARGS__") throw MacroError("__VA_ARGS__ in macro parameter list");
				for (const string& q : m.params) if (q == param) throw MacroError("duplicate parameter " + param + " in macro definition");
				m.params.push_back(param);
				p++;
				while (p < line_end && IsWS(toks[p])) p++;
				if (p >= line_end) throw MacroError("expected rparen (#2): PP_NEW_LINE()");
				if (IsOp(toks[p], ")"))
				{
					p++;
					break;
				}
				if (!IsOp(toks[p], ",")) throw MacroError("expected rparen (#2): PP_NEW_LINE()");
				p++;
				while (p < line_end && IsWS(toks[p])) p++;
				if (p < line_end && IsOp(toks[p], "..."))
				{
					m.variadic = true;
					p++;
					while (p < line_end && IsWS(toks[p])) p++;
					if (p >= line_end || !IsOp(toks[p], ")")) throw MacroError("expected rparen (#2): PP_NEW_LINE()");
					p++;
					break;
				}
				if (p >= line_end) throw MacroError("expected identifier");
			}
		}
		else
		{
			m.function_like = false;
		}

		for (; p < line_end; p++) m.replacement.push_back(toks[p]);

		if (!m.function_like)
		{
			CheckVarArgsUse(m);
			ValidateHashHashEdges(m);
		}
		else
		{
			CheckVarArgsUse(m);
			ValidateHashHashEdges(m);
			ValidateFunctionReplacement(m);
		}

		auto it = eng.macros.find(name);
		if (it != eng.macros.end() && !SameMacroDef(it->second, m))
		{
			throw MacroError("macro redefined");
		}
		eng.macros[name] = m;
	}

	void EmitPostTokens()
	{
		DebugPostTokenOutputStream output;
		for (size_t i = 0; i < out_pp.size(); i++)
		{
			const PPToken& t = out_pp[i];
			if (t.type == "eof") break;
			if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

			if (t.type == "header-name" || t.type == "non-whitespace-character")
			{
				output.emit_invalid(t.data);
				continue;
			}

			if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
			{
				if (t.data == "#" || t.data == "##" || t.data == "%:" || t.data == "%:%:")
				{
					output.emit_invalid(t.data);
					continue;
				}
				auto it = StringToTokenTypeMap.find(t.data);
				if (it != StringToTokenTypeMap.end())
				{
					output.emit_simple(t.data, it->second);
				}
				else if (t.type == "identifier")
				{
					output.emit_identifier(t.data);
				}
				else
				{
					output.emit_invalid(t.data);
				}
				continue;
			}

			if (t.type == "pp-number")
			{
				bool is_hex_int_style = t.data.size() >= 2 && t.data[0] == '0' && (t.data[1] == 'x' || t.data[1] == 'X');
				bool is_float_like = !is_hex_int_style && ((t.data.find('.') != string::npos) || (t.data.find('e') != string::npos) || (t.data.find('E') != string::npos));
				if (is_float_like)
				{
					bool ud = false;
					string ud_suffix;
					string prefix;
					EFundamentalType ty = FT_DOUBLE;
					if (!ParseFloatingLiteral(t.data, ud, ud_suffix, prefix, ty))
					{
						output.emit_invalid(t.data);
						continue;
					}
					if (ud)
					{
						output.emit_user_defined_literal_floating(t.data, ud_suffix, prefix);
						continue;
					}
					if (ty == FT_FLOAT)
					{
						float v = PA2Decode_float(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					else if (ty == FT_LONG_DOUBLE)
					{
						long double v = PA2Decode_long_double(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					else
					{
						double v = PA2Decode_double(prefix);
						output.emit_literal(t.data, ty, &v, sizeof(v));
					}
					continue;
				}

				ParsedInteger pi = ParseIntegerLiteral(t.data);
				if (!pi.ok)
				{
					size_t us = t.data.find('_');
					if (us != string::npos && us + 1 < t.data.size())
					{
						string core = t.data.substr(0, us);
						ParsedInteger core_pi = ParseIntegerLiteral(core);
						if (core_pi.ok && !core_pi.ud)
						{
							output.emit_user_defined_literal_integer(t.data, t.data.substr(us), core);
							continue;
						}
					}
					output.emit_invalid(t.data);
					continue;
				}
				if (pi.ud)
				{
					output.emit_user_defined_literal_integer(t.data, pi.ud_suffix, pi.core);
					continue;
				}

				vector<EFundamentalType> order;
				bool has_u = (pi.suffix.find('u') != string::npos) || (pi.suffix.find('U') != string::npos);
				int lcount = 0;
				for (char c : pi.suffix) if (c == 'l' || c == 'L') lcount++;
				if (!pi.nondecimal)
				{
					if (!has_u && lcount == 0) order = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
					else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_LONG_LONG_INT};
					else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT};
					else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else order = {FT_UNSIGNED_LONG_LONG_INT};
				}
				else
				{
					if (!has_u && lcount == 0) order = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
					else order = {FT_UNSIGNED_LONG_LONG_INT};
				}

				bool emitted = false;
				auto fits_signed = [&](long long mx) { return pi.value <= static_cast<unsigned long long>(mx); };
				auto fits_unsigned = [&](unsigned long long mx) { return pi.value <= mx; };
				for (EFundamentalType ty : order)
				{
					if (ty == FT_INT && fits_signed(numeric_limits<int>::max())) { int v = static_cast<int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_LONG_INT && fits_signed(numeric_limits<long int>::max())) { long int v = static_cast<long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_LONG_LONG_INT && fits_signed(numeric_limits<long long int>::max())) { long long int v = static_cast<long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_INT && fits_unsigned(numeric_limits<unsigned int>::max())) { unsigned int v = static_cast<unsigned int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_LONG_INT && fits_unsigned(numeric_limits<unsigned long int>::max())) { unsigned long int v = static_cast<unsigned long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
					if (ty == FT_UNSIGNED_LONG_LONG_INT && fits_unsigned(numeric_limits<unsigned long long int>::max())) { unsigned long long int v = static_cast<unsigned long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				}
				if (!emitted) output.emit_invalid(t.data);
				continue;
			}

			if (t.type == "character-literal" || t.type == "user-defined-character-literal")
			{
				ParsedChar pc = ParseCharacterLiteral(t);
				if (!pc.ok)
				{
					output.emit_invalid(t.data);
					continue;
				}
				if (pc.ud_suffix.empty())
				{
					output.emit_literal(t.data, pc.type, pc.bytes.data(), pc.bytes.size());
				}
				else
				{
					output.emit_user_defined_literal_character(t.data, pc.ud_suffix, pc.type, pc.bytes.data(), pc.bytes.size());
				}
				continue;
			}

			if (t.type == "string-literal" || t.type == "user-defined-string-literal")
			{
				size_t j = i;
				vector<PPToken> seq;
				while (j < out_pp.size())
				{
					if (out_pp[j].type == "whitespace-sequence" || out_pp[j].type == "new-line")
					{
						j++;
						continue;
					}
					if (out_pp[j].type == "string-literal" || out_pp[j].type == "user-defined-string-literal")
					{
						seq.push_back(out_pp[j]);
						j++;
						continue;
					}
					break;
				}
				i = j - 1;

				string src;
				for (size_t k = 0; k < seq.size(); k++) { if (k) src += " "; src += seq[k].data; }

				vector<ParsedStringPiece> parts;
				bool ok = true;
				for (const PPToken& p : seq)
				{
					ParsedStringPiece ps = ParseStringPiece(p);
					if (!ps.ok) { ok = false; break; }
					parts.push_back(move(ps));
				}
				if (!ok) { output.emit_invalid(src); continue; }

				set<string> encs;
				set<string> uds;
				for (const auto& p : parts)
				{
					if (!p.prefix.empty()) encs.insert(p.prefix);
					if (!p.ud_suffix.empty()) uds.insert(p.ud_suffix);
				}
				if (encs.size() > 1 || uds.size() > 1) { output.emit_invalid(src); continue; }
				string enc = encs.empty() ? "" : *encs.begin();
				string udsuf = uds.empty() ? "" : *uds.begin();

				vector<int> cps;
				for (const auto& p : parts) cps.insert(cps.end(), p.cps.begin(), p.cps.end());
				cps.push_back(0);

				vector<unsigned char> bytes;
				size_t num_elements = 0;
				EFundamentalType ty = FT_CHAR;
				if (enc.empty() || enc == "u8")
				{
					ty = FT_CHAR;
					string s8;
					for (int cp : cps) s8 += EncodeUTF8One(cp);
					bytes.assign(s8.begin(), s8.end());
					num_elements = bytes.size();
				}
				else if (enc == "u")
				{
					ty = FT_CHAR16_T;
					for (int cp : cps)
					{
						if (!IsValidCodePoint(cp)) { ok = false; break; }
						if (cp <= 0xFFFF)
						{
							char16_t v = static_cast<char16_t>(cp);
							unsigned char* p = reinterpret_cast<unsigned char*>(&v);
							bytes.insert(bytes.end(), p, p + 2);
							num_elements++;
						}
						else
						{
							unsigned x = static_cast<unsigned>(cp - 0x10000);
							char16_t hi = static_cast<char16_t>(0xD800 | ((x >> 10) & 0x3FF));
							char16_t lo = static_cast<char16_t>(0xDC00 | (x & 0x3FF));
							unsigned char* p1 = reinterpret_cast<unsigned char*>(&hi);
							unsigned char* p2 = reinterpret_cast<unsigned char*>(&lo);
							bytes.insert(bytes.end(), p1, p1 + 2);
							bytes.insert(bytes.end(), p2, p2 + 2);
							num_elements += 2;
						}
					}
				}
				else if (enc == "U" || enc == "L")
				{
					ty = (enc == "U") ? FT_CHAR32_T : FT_WCHAR_T;
					for (int cp : cps)
					{
						if (!IsValidCodePoint(cp)) { ok = false; break; }
						uint32_t v = static_cast<uint32_t>(cp);
						unsigned char* p = reinterpret_cast<unsigned char*>(&v);
						bytes.insert(bytes.end(), p, p + 4);
						num_elements++;
					}
				}
				if (!ok) { output.emit_invalid(src); continue; }

				if (udsuf.empty())
				{
					output.emit_literal_array(src, num_elements, ty, bytes.data(), bytes.size());
				}
				else
				{
					output.emit_user_defined_literal_string_array(src, udsuf, num_elements, ty, bytes.data(), bytes.size());
				}
				continue;
			}

			output.emit_invalid(t.data);
		}
		output.emit_eof();
	}
};

#ifndef MACRO_EMBED_ONLY
int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		vector<PPToken> toks = TokenizeToPPTokens(oss.str());
		Driver d;
		d.Parse(toks);
		d.out_pp.push_back({"eof", ""});
		d.EmitPostTokens();
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
}
#endif
