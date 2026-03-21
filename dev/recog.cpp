// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <deque>
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

#define CPPGM_PREPROC_LIBRARY
#include "preproc.cpp"

struct PA6Token
{
	bool is_identifier = false;
	bool is_literal = false;
	bool is_zero = false;
	bool is_emptystr = false;
	bool is_eof = false;
	string spelling;
	string simple;
};

struct GrammarAtom;

struct GrammarTerm
{
	GrammarAtom* atom = nullptr;
	char modifier = '1';
};

struct GrammarAtom
{
	bool is_group = false;
	string symbol;
	vector<GrammarTerm> group;
};

struct GrammarRule
{
	vector<vector<GrammarTerm> > alternatives;
};

struct Grammar
{
	map<string, GrammarRule> rules;
	deque<GrammarAtom> storage;
};

bool IsTerminalSymbol(const string& symbol)
{
	return symbol.compare(0, 3, "KW_") == 0 ||
		symbol.compare(0, 3, "OP_") == 0 ||
		symbol.compare(0, 3, "TT_") == 0 ||
		symbol.compare(0, 3, "ST_") == 0;
}

string Trim(const string& s)
{
	size_t first = 0;
	while (first < s.size() && (s[first] == ' ' || s[first] == '\t' || s[first] == '\r'))
		++first;
	size_t last = s.size();
	while (last > first && (s[last - 1] == ' ' || s[last - 1] == '\t' || s[last - 1] == '\r'))
		--last;
	return s.substr(first, last - first);
}

vector<string> TokenizeGrammarRhs(const string& text)
{
	vector<string> out;
	string current;
	for (char c : text)
	{
		if (c == ' ' || c == '\t' || c == '\r')
		{
			if (!current.empty())
			{
				out.push_back(current);
				current.clear();
			}
			continue;
		}

		if (c == '(' || c == ')' || c == '?' || c == '*' || c == '+')
		{
			if (!current.empty())
			{
				out.push_back(current);
				current.clear();
			}
			out.push_back(string(1, c));
			continue;
		}

		current.push_back(c);
	}

	if (!current.empty())
		out.push_back(current);
	return out;
}

vector<GrammarTerm> ParseGrammarSequence(const vector<string>& tokens, size_t& i, Grammar& grammar)
{
	vector<GrammarTerm> out;
	while (i < tokens.size() && tokens[i] != ")")
	{
		GrammarAtom atom;
		if (tokens[i] == "(")
		{
			++i;
			atom.is_group = true;
			atom.group = ParseGrammarSequence(tokens, i, grammar);
			if (i >= tokens.size() || tokens[i] != ")")
				throw logic_error("unmatched group in grammar");
			++i;
		}
		else
		{
			atom.symbol = tokens[i++];
		}

		grammar.storage.push_back(atom);
		GrammarTerm term;
		term.atom = &grammar.storage.back();
		if (i < tokens.size() && (tokens[i] == "?" || tokens[i] == "*" || tokens[i] == "+"))
			term.modifier = tokens[i++][0];
		out.push_back(term);
	}
	return out;
}

Grammar LoadGrammar()
{
	vector<string> candidates;
	candidates.push_back("pa6.gram");
	candidates.push_back("pa6/pa6.gram");

	ifstream in;
	for (const string& candidate : candidates)
	{
		in.open(candidate);
		if (in)
			break;
		in.clear();
	}
	if (!in)
		throw runtime_error("cannot open pa6.gram");

	vector<string> raw_lines;
	string line;
	while (getline(in, line))
		raw_lines.push_back(line);

	vector<string> lines;
	for (size_t i = 0; i < raw_lines.size(); ++i)
	{
		string current = raw_lines[i];
		while (!current.empty() && current[current.size() - 1] == '\\')
		{
			current.erase(current.size() - 1);
			if (++i >= raw_lines.size())
				break;
			current += " " + Trim(raw_lines[i]);
		}
		lines.push_back(current);
	}

	Grammar grammar;
	string current_rule;
	for (const string& raw : lines)
	{
		if (Trim(raw).empty())
			continue;

		bool indented = !raw.empty() && (raw[0] == '\t' || raw[0] == ' ');
		if (!indented)
		{
			string header = Trim(raw);
			if (header.empty() || header[header.size() - 1] != ':')
				throw logic_error("invalid grammar header");
			current_rule = header.substr(0, header.size() - 1);
			grammar.rules[current_rule] = GrammarRule();
			continue;
		}

		if (current_rule.empty())
			throw logic_error("grammar alternative without header");

		vector<string> rhs_tokens = TokenizeGrammarRhs(Trim(raw));
		size_t pos = 0;
		grammar.rules[current_rule].alternatives.push_back(ParseGrammarSequence(rhs_tokens, pos, grammar));
		if (pos != rhs_tokens.size())
			throw logic_error("trailing grammar tokens");
	}

	return grammar;
}

vector<PA6Token> ParsePreprocTokenLines(const string& text)
{
	vector<PA6Token> tokens;
	istringstream iss(text);
	string line;
	while (getline(iss, line))
	{
		if (line.empty())
			continue;

		PA6Token token;
		if (line == "eof")
		{
			token.is_eof = true;
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 11, "identifier ") == 0)
		{
			token.is_identifier = true;
			token.spelling = line.substr(11);
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 7, "simple ") == 0)
		{
			size_t last_space = line.rfind(' ');
			if (last_space == string::npos || last_space <= 6)
				throw logic_error("bad simple token line");
			string op = line.substr(last_space + 1);
			if (op == "OP_RSHIFT")
			{
				PA6Token t1;
				t1.simple = "ST_RSHIFT_1";
				tokens.push_back(t1);
				PA6Token t2;
				t2.simple = "ST_RSHIFT_2";
				tokens.push_back(t2);
			}
			else
			{
				token.simple = op;
				tokens.push_back(token);
			}
			continue;
		}

		if (line.compare(0, 8, "literal 0") == 0 &&
			(line.size() == 8 || line[8] == ' '))
		{
			token.is_literal = true;
			token.is_zero = true;
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 19, "literal \"\" array of") == 0)
		{
			token.is_literal = true;
			token.is_emptystr = true;
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 8, "literal ") == 0)
		{
			token.is_literal = true;
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 21, "user-defined-literal ") == 0)
		{
			token.is_literal = true;
			tokens.push_back(token);
			continue;
		}

		throw logic_error("unexpected token line: " + line);
	}

	return tokens;
}

bool MatchesTerminal(const PA6Token& token, const string& terminal)
{
	if (terminal == "ST_EOF")
		return token.is_eof;
	if (terminal == "TT_IDENTIFIER")
		return token.is_identifier;
	if (terminal == "TT_LITERAL")
		return token.is_literal;
	if (terminal == "ST_ZERO")
		return token.is_zero;
	if (terminal == "ST_EMPTYSTR")
		return token.is_emptystr;
	if (terminal == "ST_OVERRIDE")
		return token.is_identifier && token.spelling == "override";
	if (terminal == "ST_FINAL")
		return token.is_identifier && token.spelling == "final";
	if (terminal == "ST_NONPAREN")
	{
		return !token.is_eof &&
			token.simple != "OP_LPAREN" &&
			token.simple != "OP_RPAREN" &&
			token.simple != "OP_LSQUARE" &&
			token.simple != "OP_RSQUARE" &&
			token.simple != "OP_LBRACE" &&
			token.simple != "OP_RBRACE";
	}
	return token.simple == terminal;
}

bool HasBadCloseAnglePattern(const vector<PA6Token>& tokens)
{
	for (size_t i = 0; i + 1 < tokens.size(); ++i)
	{
		if (!tokens[i].is_identifier || !PA6_IsTemplateName(tokens[i].spelling))
			continue;
		if (tokens[i + 1].simple != "OP_LT")
			continue;

		int angle_depth = 1;
		int paren_depth = 0;
		int square_depth = 0;
		int brace_depth = 0;

		for (size_t j = i + 2; j < tokens.size(); ++j)
		{
			if (tokens[j].simple == "OP_LPAREN") { ++paren_depth; continue; }
			if (tokens[j].simple == "OP_RPAREN" && paren_depth > 0) { --paren_depth; continue; }
			if (tokens[j].simple == "OP_LSQUARE") { ++square_depth; continue; }
			if (tokens[j].simple == "OP_RSQUARE" && square_depth > 0) { --square_depth; continue; }
			if (tokens[j].simple == "OP_LBRACE") { ++brace_depth; continue; }
			if (tokens[j].simple == "OP_RBRACE" && brace_depth > 0) { --brace_depth; continue; }

			if (paren_depth != 0 || square_depth != 0 || brace_depth != 0)
				continue;

			if (tokens[j].is_identifier && PA6_IsTemplateName(tokens[j].spelling) &&
				j + 1 < tokens.size() && tokens[j + 1].simple == "OP_LT")
			{
				++angle_depth;
				continue;
			}

			if (tokens[j].simple == "OP_GT")
			{
				--angle_depth;
				if (angle_depth == 0)
				{
					if (j + 1 < tokens.size() &&
						(tokens[j + 1].is_literal ||
						tokens[j + 1].simple == "OP_GT" ||
						tokens[j + 1].simple == "ST_RSHIFT_1" ||
						tokens[j + 1].simple == "ST_RSHIFT_2"))
					{
						return true;
					}
					break;
				}
				continue;
			}

			if (tokens[j].simple == "ST_RSHIFT_1" &&
				j + 1 < tokens.size() && tokens[j + 1].simple == "ST_RSHIFT_2")
			{
				angle_depth -= 2;
				if (angle_depth <= 0)
				{
					size_t next = j + 2;
					if (next < tokens.size() &&
						(tokens[next].is_literal ||
						tokens[next].simple == "OP_GT" ||
						tokens[next].simple == "ST_RSHIFT_1" ||
						tokens[next].simple == "ST_RSHIFT_2"))
					{
						return true;
					}
					break;
				}
				++j;
			}
		}
	}

	return false;
}

struct Recognizer
{
	Recognizer(const Grammar& grammar_, const vector<PA6Token>& tokens_)
		: grammar(grammar_), tokens(tokens_)
	{
	}

	bool ParseTranslationUnit()
	{
		vector<int> ends = ParseNonterminal("translation-unit", 0);
		for (int end : ends)
		{
			if (end == static_cast<int>(tokens.size()))
				return true;
		}
		return false;
	}

private:
	string Key(const string& name, int pos) const
	{
		return name + "#" + to_string(pos);
	}

	vector<int> Unique(vector<int> values) const
	{
		sort(values.begin(), values.end());
		values.erase(unique(values.begin(), values.end()), values.end());
		return values;
	}

	vector<int> ParseNonterminal(const string& name, int pos)
	{
		string key = Key(name, pos);
		auto memo_it = memo.find(key);
		if (memo_it != memo.end())
		{
			if (memo_it->second.busy)
				return vector<int>();
			return memo_it->second.ends;
		}

		MemoEntry& entry = memo[key];
		entry.busy = true;

		vector<int> result;
		if (name == "expression-statement" || name == "statement")
		{
			if (pos + 3 < static_cast<int>(tokens.size()) &&
				MatchesTerminal(tokens[pos], "KW_OPERATOR") &&
				MatchesTerminal(tokens[pos + 1], "ST_EMPTYSTR") &&
				MatchesTerminal(tokens[pos + 2], "TT_IDENTIFIER") &&
				MatchesTerminal(tokens[pos + 3], "OP_SEMICOLON"))
			{
				result.push_back(pos + 4);
			}
		}
		if (name == "primary-expression" || name == "id-expression")
		{
			if (pos + 2 < static_cast<int>(tokens.size()) &&
				MatchesTerminal(tokens[pos], "KW_OPERATOR") &&
				MatchesTerminal(tokens[pos + 1], "ST_EMPTYSTR") &&
				MatchesTerminal(tokens[pos + 2], "TT_IDENTIFIER"))
			{
				result.push_back(pos + 3);
			}
		}
		if (name == "unqualified-id")
		{
			if (pos + 2 < static_cast<int>(tokens.size()) &&
				MatchesTerminal(tokens[pos], "KW_OPERATOR") &&
				MatchesTerminal(tokens[pos + 1], "ST_EMPTYSTR") &&
				MatchesTerminal(tokens[pos + 2], "TT_IDENTIFIER"))
			{
				result.push_back(pos + 3);
			}
		}
		if (name == "literal-operator-id")
		{
			if (pos + 2 < static_cast<int>(tokens.size()) &&
				MatchesTerminal(tokens[pos], "KW_OPERATOR") &&
				MatchesTerminal(tokens[pos + 1], "ST_EMPTYSTR") &&
				MatchesTerminal(tokens[pos + 2], "TT_IDENTIFIER"))
			{
				result.push_back(pos + 3);
			}
		}
		if (pos < static_cast<int>(tokens.size()) && tokens[pos].is_identifier)
		{
			const string& spelling = tokens[pos].spelling;
			if (name == "class-name" && PA6_IsClassName(spelling))
				result.push_back(pos + 1);
			if (name == "template-name" && PA6_IsTemplateName(spelling))
				result.push_back(pos + 1);
			if (name == "typedef-name" && PA6_IsTypedefName(spelling))
				result.push_back(pos + 1);
			if (name == "enum-name" && PA6_IsEnumName(spelling))
				result.push_back(pos + 1);
			if (name == "namespace-name" && PA6_IsNamespaceName(spelling))
				result.push_back(pos + 1);
		}

		auto it = grammar.rules.find(name);
		if (it == grammar.rules.end())
			throw logic_error("missing grammar rule: " + name);

		for (const vector<GrammarTerm>& alt : it->second.alternatives)
		{
			vector<int> ends = ParseSequence(alt, pos);
			result.insert(result.end(), ends.begin(), ends.end());
		}

		entry.busy = false;
		entry.ends = Unique(result);
		return entry.ends;
	}

	vector<int> ParseSequence(const vector<GrammarTerm>& seq, int pos)
	{
		vector<int> positions(1, pos);
		for (const GrammarTerm& term : seq)
		{
			vector<int> next_positions;
			for (int current : positions)
			{
				vector<int> ends = ParseTerm(term, current);
				next_positions.insert(next_positions.end(), ends.begin(), ends.end());
			}
			positions = Unique(next_positions);
			if (positions.empty())
				break;
		}
		return positions;
	}

	vector<int> ParseTerm(const GrammarTerm& term, int pos)
	{
		if (term.modifier == '1')
			return ParseAtom(*term.atom, pos);
		if (term.modifier == '?')
		{
			vector<int> out(1, pos);
			vector<int> extra = ParseAtom(*term.atom, pos);
			out.insert(out.end(), extra.begin(), extra.end());
			return Unique(out);
		}

		vector<int> reached(1, pos);
		vector<int> frontier(1, pos);
		bool require_one = term.modifier == '+';
		if (require_one)
		{
			frontier = ParseAtom(*term.atom, pos);
			reached = frontier;
			if (frontier.empty())
				return vector<int>();
		}

		while (!frontier.empty())
		{
			vector<int> next;
			for (int current : frontier)
			{
				vector<int> ends = ParseAtom(*term.atom, current);
				for (int end : ends)
				{
					if (find(reached.begin(), reached.end(), end) == reached.end())
					{
						reached.push_back(end);
						next.push_back(end);
					}
				}
			}
			frontier = Unique(next);
		}

		if (term.modifier == '*')
			reached.push_back(pos);
		return Unique(reached);
	}

	vector<int> ParseAtom(const GrammarAtom& atom, int pos)
	{
		if (atom.is_group)
			return ParseSequence(atom.group, pos);
		if (IsTerminalSymbol(atom.symbol))
		{
			if (pos < static_cast<int>(tokens.size()) && MatchesTerminal(tokens[pos], atom.symbol))
				return vector<int>(1, pos + 1);
			return vector<int>();
		}
		return ParseNonterminal(atom.symbol, pos);
	}

	struct MemoEntry
	{
		bool busy = false;
		vector<int> ends;
	};

	const Grammar& grammar;
	const vector<PA6Token>& tokens;
	map<string, MemoEntry> memo;
};

bool DoRecog(const string& srcfile, const Grammar& grammar)
{
	pair<string, string> build = BuildDateTimeLiterals();
	string author_literal = EscapeStringLiteral("OpenAI Codex");

	ostringstream oss;
	Preprocessor preproc(build.first, build.second, author_literal);
	preproc.ProcessSourceFile(srcfile, oss);
	vector<PA6Token> tokens = ParsePreprocTokenLines(oss.str());
	Recognizer recognizer(grammar, tokens);
	return recognizer.ParseTranslationUnit() && !HasBadCloseAnglePattern(tokens);
}

int main(int argc, char** argv)
{
	try
	{
		Grammar grammar = LoadGrammar();

		vector<string> args;
		for (int i = 1; i < argc; i++)
			args.emplace_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		string outfile = args[1];
		size_t nsrcfiles = args.size() - 2;

		ofstream out(outfile);
		out << "recog " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];

			try
			{
				bool ok = DoRecog(srcfile, grammar);
				out << srcfile << " " << (ok ? "OK" : "BAD") << endl;
			}
			catch (exception& e)
			{
				cerr << e.what() << endl;
				out << srcfile << " BAD" << endl;
			}
		}

		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
