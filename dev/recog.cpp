// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <set>

using namespace std;

bool PA6_IsClassName(const string& identifier)
{
	return identifier.find('C') != string::npos;
}

bool PA6_IsTemplateName(const string& identifier)
{
	return identifier.find('T') != string::npos;
}

bool PA6_IsTypedefName(const string& identifier)
{
	return identifier.find('Y') != string::npos;
}

bool PA6_IsEnumName(const string& identifier)
{
	return identifier.find('E') != string::npos;
}

bool PA6_IsNamespaceName(const string& identifier)
{
	return identifier.find('N') != string::npos;
}

#define PREPROC_EMBED_ONLY
#include "preproc.cpp"
#undef PREPROC_EMBED_ONLY

enum PredKind { PRED_NONE, PRED_CLASS, PRED_TEMPLATE, PRED_TYPEDEF, PRED_ENUM, PRED_NAMESPACE };

struct Production
{
	string lhs;
	vector<string> rhs;
	PredKind pred = PRED_NONE;
};

struct GToken
{
	string term;
	string text;
	bool is_identifier = false;
	bool is_literal = false;
	bool is_emptystr = false;
	bool is_zero = false;
	bool is_nonparen = false;
};

struct Item
{
	int prod = 0;
	int dot = 0;
	int start = 0;

	bool operator==(const Item& o) const { return prod == o.prod && dot == o.dot && start == o.start; }
};

struct ItemHash
{
	size_t operator()(const Item& x) const
	{
		return (static_cast<size_t>(x.prod) * 1315423911u) ^ (static_cast<size_t>(x.dot) * 2654435761u) ^ static_cast<size_t>(x.start);
	}
};

vector<string> GrammarTokenize(const string& s)
{
	vector<string> t;
	for (size_t i = 0; i < s.size();)
	{
		if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r') { i++; continue; }
		if (s[i] == '(' || s[i] == ')' || s[i] == '?' || s[i] == '*' || s[i] == '+')
		{
			t.push_back(string(1, s[i]));
			i++;
			continue;
		}
		size_t j = i;
		while (j < s.size())
		{
			char c = s[j];
			if (c == ' ' || c == '\t' || c == '\r' || c == '(' || c == ')' || c == '?' || c == '*' || c == '+') break;
			j++;
		}
		t.push_back(s.substr(i, j - i));
		i = j;
	}
	return t;
}

struct GrammarBuilder
{
	map<string, vector<vector<string> > > rules;
	int gensym = 0;

	string NewNt(const string& base)
	{
		return "$" + base + "_" + to_string(gensym++);
	}

	string ParseAtom(const vector<string>& toks, size_t& p)
	{
		if (p >= toks.size()) throw runtime_error("bad grammar atom");
		if (toks[p] == "(")
		{
			p++;
			vector<string> seq;
			while (p < toks.size() && toks[p] != ")") seq.push_back(ParseFactor(toks, p));
			if (p >= toks.size() || toks[p] != ")") throw runtime_error("bad grammar group");
			p++;
			if (seq.size() == 1) return seq[0];
			string nt = NewNt("grp");
			rules[nt].push_back(seq);
			return nt;
		}
		return toks[p++];
	}

	string ParseFactor(const vector<string>& toks, size_t& p)
	{
		string a = ParseAtom(toks, p);
		if (p >= toks.size()) return a;
		if (toks[p] == "?")
		{
			p++;
			string nt = NewNt("opt");
			rules[nt].push_back(vector<string>());
			rules[nt].push_back(vector<string>{a});
			return nt;
		}
		if (toks[p] == "*")
		{
			p++;
			string nt = NewNt("star");
			rules[nt].push_back(vector<string>());
			rules[nt].push_back(vector<string>{a, nt});
			return nt;
		}
		if (toks[p] == "+")
		{
			p++;
			string nt = NewNt("plus");
			rules[nt].push_back(vector<string>{a});
			rules[nt].push_back(vector<string>{a, nt});
			return nt;
		}
		return a;
	}

	vector<string> ParseRhs(const string& s)
	{
		vector<string> toks = GrammarTokenize(s);
		size_t p = 0;
		vector<string> seq;
		while (p < toks.size()) seq.push_back(ParseFactor(toks, p));
		return seq;
	}
};

struct Grammar
{
	vector<Production> prods;
	unordered_map<string, vector<int> > by_lhs;
	unordered_set<string> nonterminals;
	string start = "translation-unit";

	bool IsNonterminal(const string& s) const { return nonterminals.count(s) != 0; }
};

Grammar LoadGrammar(const string& path)
{
	ifstream in(path);
	if (!in) throw runtime_error("cannot open grammar");
	GrammarBuilder gb;
	string cur;
	string line;
	while (getline(in, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;
		if (line[0] != '\t' && line[0] != ' ')
		{
			if (line.back() != ':') throw runtime_error("bad grammar header");
			cur = line.substr(0, line.size() - 1);
			gb.rules[cur];
			continue;
		}
		if (cur.empty()) continue;
		string rhs = line;
		while (!rhs.empty() && (rhs[0] == '\t' || rhs[0] == ' ')) rhs.erase(rhs.begin());
		while (!rhs.empty() && rhs.back() == '\\')
		{
			rhs.pop_back();
			string nxt;
			if (!getline(in, nxt)) break;
			if (!nxt.empty() && nxt.back() == '\r') nxt.pop_back();
			while (!nxt.empty() && (nxt[0] == '\t' || nxt[0] == ' ')) nxt.erase(nxt.begin());
			rhs += " " + nxt;
		}
		gb.rules[cur].push_back(gb.ParseRhs(rhs));
	}

	Grammar g;
	for (const auto& kv : gb.rules) g.nonterminals.insert(kv.first);
	for (const auto& kv : gb.rules)
	{
		for (const auto& rhs : kv.second)
		{
			Production p;
			p.lhs = kv.first;
			p.rhs = rhs;
			if (p.rhs.size() == 1 && p.rhs[0] == "TT_IDENTIFIER")
			{
				if (p.lhs == "class-name") p.pred = PRED_CLASS;
				else if (p.lhs == "template-name") p.pred = PRED_TEMPLATE;
				else if (p.lhs == "typedef-name") p.pred = PRED_TYPEDEF;
				else if (p.lhs == "enum-name") p.pred = PRED_ENUM;
				else if (p.lhs == "namespace-name") p.pred = PRED_NAMESPACE;
			}
			int id = static_cast<int>(g.prods.size());
			g.prods.push_back(p);
			g.by_lhs[p.lhs].push_back(id);
		}
	}
	return g;
}

bool MatchTerminal(const string& sym, const GToken& t)
{
	if (sym == "TT_IDENTIFIER") return t.is_identifier;
	if (sym == "TT_LITERAL") return t.is_literal;
	if (sym == "ST_EMPTYSTR") return t.is_emptystr;
	if (sym == "ST_ZERO") return t.is_zero;
	if (sym == "ST_NONPAREN") return t.is_nonparen;
	return t.term == sym;
}

bool CheckPredicate(PredKind pred, const GToken& t)
{
	if (pred == PRED_NONE) return true;
	if (!t.is_identifier) return false;
	if (pred == PRED_CLASS) return PA6_IsClassName(t.text);
	if (pred == PRED_TEMPLATE) return PA6_IsTemplateName(t.text);
	if (pred == PRED_TYPEDEF) return PA6_IsTypedefName(t.text);
	if (pred == PRED_ENUM) return PA6_IsEnumName(t.text);
	if (pred == PRED_NAMESPACE) return PA6_IsNamespaceName(t.text);
	return false;
}

bool IsTemplateLikeIdentifier(const GToken& t)
{
	return t.is_identifier && PA6_IsTemplateName(t.text);
}

bool AllowIdentifierForLhs(const string& lhs)
{
	return lhs == "template-name" || lhs == "class-name" || lhs == "typedef-name" || lhs == "enum-name" || lhs == "namespace-name";
}

bool TemplateArgumentSpanOK(const vector<GToken>& toks, int start, int end)
{
	int paren = 0;
	int square = 0;
	int brace = 0;
	int angle = 0;
	for (int i = start; i < end; i++)
	{
		const string& term = toks[i].term;
		if (term == "OP_LPAREN") { paren++; continue; }
		if (term == "OP_RPAREN") { if (paren > 0) paren--; continue; }
		if (term == "OP_LSQUARE") { square++; continue; }
		if (term == "OP_RSQUARE") { if (square > 0) square--; continue; }
		if (term == "OP_LBRACE") { brace++; continue; }
		if (term == "OP_RBRACE") { if (brace > 0) brace--; continue; }

		if (paren != 0 || square != 0 || brace != 0) continue;

		if (term == "OP_LT")
		{
			angle++;
			continue;
		}
		if (term == "OP_GT" || term == "ST_RSHIFT_1" || term == "ST_RSHIFT_2")
		{
			if (angle == 0) return false;
			angle--;
		}
	}
	return true;
}

bool HasTemplateCloseAngleLiteralConflict(const vector<GToken>& toks)
{
	int n = static_cast<int>(toks.size());
	for (int i = 0; i + 2 < n; i++)
	{
		if (!toks[i].is_identifier || !PA6_IsTemplateName(toks[i].text) || toks[i + 1].term != "OP_LT") continue;

		int paren = 0;
		int square = 0;
		int brace = 0;
		int angle = 1;
		int close_pos = -1;
		for (int j = i + 2; j < n; j++)
		{
			const string& term = toks[j].term;
			if (term == "OP_LPAREN") { paren++; continue; }
			if (term == "OP_RPAREN") { if (paren > 0) paren--; continue; }
			if (term == "OP_LSQUARE") { square++; continue; }
			if (term == "OP_RSQUARE") { if (square > 0) square--; continue; }
			if (term == "OP_LBRACE") { brace++; continue; }
			if (term == "OP_RBRACE") { if (brace > 0) brace--; continue; }
			if (paren != 0 || square != 0 || brace != 0) continue;

			if (term == "OP_LT")
			{
				angle++;
				continue;
			}
			if (term == "OP_GT" || term == "ST_RSHIFT_1" || term == "ST_RSHIFT_2")
			{
				angle--;
				if (angle == 0)
				{
					close_pos = j;
					break;
				}
				if (angle < 0) break;
			}
		}
		if (close_pos >= 0 && close_pos + 1 < n && toks[close_pos + 1].is_literal) return true;
	}
	return false;
}

vector<GToken> BuildGrammarTokens(const vector<PPToken>& pptoks)
{
	vector<GToken> out;
	for (const PPToken& t : pptoks)
	{
		if (t.type == "eof") break;
		if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

		if (t.type == "header-name" || t.type == "non-whitespace-character") throw runtime_error("invalid token");

		if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
		{
			if (t.data == "#" || t.data == "##" || t.data == "%:" || t.data == "%:%:") throw runtime_error("invalid token");
			auto it = StringToTokenTypeMap.find(t.data);
			if (it != StringToTokenTypeMap.end())
			{
				string term = TokenTypeToStringMap.at(it->second);
				if (term == "OP_RSHIFT")
				{
					GToken a; a.term = "ST_RSHIFT_1"; a.is_nonparen = true; out.push_back(a);
					GToken b; b.term = "ST_RSHIFT_2"; b.is_nonparen = true; out.push_back(b);
				}
				else
				{
					GToken g;
					g.term = term;
					g.is_nonparen = (term != "OP_LPAREN" && term != "OP_RPAREN" && term != "OP_LSQUARE" && term != "OP_RSQUARE" && term != "OP_LBRACE" && term != "OP_RBRACE");
					out.push_back(g);
				}
			}
			else if (t.type == "identifier")
			{
				GToken g;
				g.is_identifier = true;
				g.text = t.data;
				g.is_nonparen = true;
				if (t.data == "override") g.term = "ST_OVERRIDE";
				else if (t.data == "final") g.term = "ST_FINAL";
				else g.term = "TT_IDENTIFIER";
				out.push_back(g);
			}
			else throw runtime_error("invalid token");
			continue;
		}

		if (t.type == "pp-number")
		{
			bool is_hex_int_style = t.data.size() >= 2 && t.data[0] == '0' && (t.data[1] == 'x' || t.data[1] == 'X');
			bool is_float_like = !is_hex_int_style && ((t.data.find('.') != string::npos) || (t.data.find('e') != string::npos) || (t.data.find('E') != string::npos));
			if (is_float_like)
			{
				bool ud = false; string ud_suffix; string prefix; EFundamentalType ty = FT_DOUBLE;
				if (!ParseFloatingLiteral(t.data, ud, ud_suffix, prefix, ty)) throw runtime_error("invalid token");
			}
			else
			{
				ParsedInteger pi = ParseIntegerLiteral(t.data);
				if (!pi.ok)
				{
					size_t us = t.data.find('_');
					if (!(us != string::npos && us + 1 < t.data.size() && ParseIntegerLiteral(t.data.substr(0, us)).ok)) throw runtime_error("invalid token");
				}
			}
			GToken g;
			g.term = "TT_LITERAL";
			g.is_literal = true;
			g.is_nonparen = true;
			g.is_zero = (t.data == "0");
			out.push_back(g);
			continue;
		}

		if (t.type == "character-literal" || t.type == "user-defined-character-literal")
		{
			ParsedChar pc = ParseCharacterLiteral(t);
			if (!pc.ok) throw runtime_error("invalid token");
			GToken g; g.term = "TT_LITERAL"; g.is_literal = true; g.is_nonparen = true; out.push_back(g);
			continue;
		}

		if (t.type == "string-literal" || t.type == "user-defined-string-literal")
		{
			ParsedStringPiece ps = ParseStringPiece(t);
			if (!ps.ok) throw runtime_error("invalid token");
			GToken g; g.term = "TT_LITERAL"; g.is_literal = true; g.is_nonparen = true; g.is_emptystr = (t.type == "string-literal" && t.data == "\"\""); out.push_back(g);
			continue;
		}

		throw runtime_error("invalid token");
	}
	GToken eof;
	eof.term = "ST_EOF";
	out.push_back(eof);
	return out;
}

bool Recognize(const Grammar& g, const vector<GToken>& toks)
{
	int n = static_cast<int>(toks.size());
	vector<unordered_set<Item, ItemHash> > S(n + 1);
	auto add_item = [&](int k, const Item& it) -> bool { return S[k].insert(it).second; };
	auto mk = [](int prod, int dot, int start) { Item x; x.prod = prod; x.dot = dot; x.start = start; return x; };

	for (int pid : g.by_lhs.at(g.start)) add_item(0, mk(pid, 0, 0));

	for (int k = 0; k <= n; k++)
	{
		bool changed = true;
		while (changed)
		{
			changed = false;
			vector<Item> items(S[k].begin(), S[k].end());
			for (const Item& it : items)
			{
				const Production& p = g.prods[it.prod];
				if (it.dot < static_cast<int>(p.rhs.size()))
				{
					const string& sym = p.rhs[it.dot];
					if (g.IsNonterminal(sym))
					{
						auto jt = g.by_lhs.find(sym);
						if (jt != g.by_lhs.end()) for (int pid : jt->second) if (add_item(k, mk(pid, 0, k))) changed = true;
					}
				}
				else
				{
					bool pred_ok = true;
					if (p.pred != PRED_NONE)
					{
						pred_ok = (k == it.start + 1 && it.start >= 0 && it.start < static_cast<int>(toks.size()) && CheckPredicate(p.pred, toks[it.start]));
					}
					if (!pred_ok) continue;
					if (p.lhs == "template-argument" && !TemplateArgumentSpanOK(toks, it.start, k)) continue;

					vector<Item> prev(S[it.start].begin(), S[it.start].end());
					for (const Item& pit : prev)
					{
						const Production& pp = g.prods[pit.prod];
						if (pit.dot < static_cast<int>(pp.rhs.size()) && pp.rhs[pit.dot] == p.lhs)
						{
							if (add_item(k, mk(pit.prod, pit.dot + 1, pit.start))) changed = true;
						}
					}
				}
			}
		}

		if (k == n) break;

		for (const Item& it : S[k])
		{
			const Production& p = g.prods[it.prod];
			if (it.dot >= static_cast<int>(p.rhs.size())) continue;
			const string& sym = p.rhs[it.dot];
			if (g.IsNonterminal(sym)) continue;
			if (sym == "TT_IDENTIFIER" && toks[k].is_identifier && k + 1 < n && toks[k + 1].term == "OP_LT" && IsTemplateLikeIdentifier(toks[k]) && !AllowIdentifierForLhs(p.lhs))
			{
				continue;
			}
			if (MatchTerminal(sym, toks[k])) add_item(k + 1, mk(it.prod, it.dot + 1, it.start));
		}
	}

	for (const Item& it : S[n])
	{
		const Production& p = g.prods[it.prod];
		if (p.lhs == g.start && it.start == 0 && it.dot == static_cast<int>(p.rhs.size())) return true;
	}
	return false;
}

bool DoRecogOne(const Grammar& g, const string& srcfile)
{
	PreprocState st;
	pair<string, string> dt = BuildDateTimeLiterals();
	st.date_lit = dt.first;
	st.time_lit = dt.second;
	st.author_lit = "\"John Smith\"";
	SeedBuiltins(st);

	vector<PPToken> pre;
	ProcessFile(st, srcfile, pre);
	pre.push_back({"eof", ""});
	vector<GToken> toks = BuildGrammarTokens(pre);
	if (HasTemplateCloseAngleLiteralConflict(toks)) return false;
	return Recognize(g, toks);
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;
		for (int i = 1; i < argc; i++) args.emplace_back(argv[i]);
		if (args.size() < 3 || args[0] != "-o") throw logic_error("invalid usage");

		string outfile = args[1];
		size_t nsrcfiles = args.size() - 2;

		ofstream out(outfile);
		out << "recog " << nsrcfiles << endl;

		Grammar g = LoadGrammar("pa6.gram");

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			try
			{
				bool ok = DoRecogOne(g, srcfile);
				out << srcfile << " " << (ok ? "OK" : "BAD") << endl;
			}
			catch (exception&)
			{
				out << srcfile << " BAD" << endl;
			}
		}
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
