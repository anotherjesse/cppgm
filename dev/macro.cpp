// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#define main posttoken_internal_main
#include "posttoken.cpp"
#undef main

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>
#include <map>

using namespace std;

struct MacroDef
{
	bool function_like;
	bool variadic;
	vector<string> params;
	vector<PPToken> replacement;

	MacroDef() : function_like(false), variadic(false) {}
};

string MacroKey(const MacroDef& def)
{
	ostringstream oss;
	oss << def.function_like << "|" << def.variadic;
	for (size_t i = 0; i < def.params.size(); ++i)
	{
		oss << "|" << def.params[i];
	}
	oss << "|/";
	for (size_t i = 0; i < def.replacement.size(); ++i)
	{
		oss << static_cast<int>(def.replacement[i].kind) << ":" << def.replacement[i].data << "/";
	}
	return oss.str();
}

vector<PPToken> CollapseWhitespace(const vector<PPToken>& tokens)
{
	vector<PPToken> out;
	bool in_ws = false;
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (tokens[i].kind == PP_WS || tokens[i].kind == PP_NL)
		{
			if (!in_ws)
			{
				out.push_back({PP_WS, ""});
				in_ws = true;
			}
		}
		else
		{
			out.push_back(tokens[i]);
			in_ws = false;
		}
	}
	if (!out.empty() && out.front().kind == PP_WS) out.erase(out.begin());
	if (!out.empty() && out.back().kind == PP_WS) out.pop_back();
	return out;
}

bool TokensEqual(const vector<PPToken>& a, const vector<PPToken>& b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].kind != b[i].kind || a[i].data != b[i].data) return false;
	}
	return true;
}

vector<PPToken> ExpandTokens(const vector<PPToken>& tokens, map<string, MacroDef>& macros, set<string> active);

vector<PPToken> RetokenizeFragment(const string& text)
{
	vector<int> cps = TransformSource(DecodeUTF8(text));
	PPCollector collector;
	PPTokenizer tokenizer(collector);
	vector<PPToken> toks = tokenizer.tokenize(cps);
	vector<PPToken> out;
	for (size_t i = 0; i < toks.size(); ++i)
	{
		if (toks[i].kind != PP_WS && toks[i].kind != PP_NL)
		{
			out.push_back(toks[i]);
		}
	}
	return out;
}

vector<PPToken> Substitute(const MacroDef& def, const vector<vector<PPToken> >& args, map<string, MacroDef>& macros, set<string> active)
{
	vector<PPToken> expanded;
	for (size_t i = 0; i < def.replacement.size(); ++i)
	{
		const PPToken& tok = def.replacement[i];
		bool adjacent_hashhash =
			(i > 0 && def.replacement[i - 1].kind == PP_OP && def.replacement[i - 1].data == "##") ||
			(i + 1 < def.replacement.size() && def.replacement[i + 1].kind == PP_OP && def.replacement[i + 1].data == "##");
		if (tok.kind == PP_IDENTIFIER)
		{
			bool replaced = false;
			for (size_t p = 0; p < def.params.size(); ++p)
			{
				if (tok.data == def.params[p])
				{
					vector<PPToken> repl = adjacent_hashhash ? args[p] : ExpandTokens(args[p], macros, active);
					expanded.insert(expanded.end(), repl.begin(), repl.end());
					replaced = true;
					break;
				}
			}
			if (!replaced && def.variadic && tok.data == "__VA_ARGS__")
			{
				size_t var_index = def.params.size();
				if (var_index < args.size())
				{
					vector<PPToken> repl = adjacent_hashhash ? args[var_index] : ExpandTokens(args[var_index], macros, active);
					expanded.insert(expanded.end(), repl.begin(), repl.end());
				}
				replaced = true;
			}
			if (replaced) continue;
		}
		expanded.push_back(tok);
	}

	vector<PPToken> out;
	for (size_t i = 0; i < expanded.size(); ++i)
	{
		if (expanded[i].kind == PP_OP && expanded[i].data == "##")
		{
			while (!out.empty() && out.back().kind == PP_WS) out.pop_back();
			if (out.empty()) continue;
			size_t rhs_index = i + 1;
			while (rhs_index < expanded.size() && expanded[rhs_index].kind == PP_WS) ++rhs_index;
			string lhs_text = out.back().data;
			out.pop_back();
			string rhs_text = rhs_index < expanded.size() ? expanded[rhs_index].data : "";
			if (lhs_text.empty() && rhs_text.empty()) continue;
			vector<PPToken> pasted = RetokenizeFragment(lhs_text + rhs_text);
			out.insert(out.end(), pasted.begin(), pasted.end());
			i = rhs_index;
			continue;
		}
		out.push_back(expanded[i]);
	}
	return out;
}

vector<PPToken> ExpandTokens(const vector<PPToken>& tokens, map<string, MacroDef>& macros, set<string> active)
{
	vector<PPToken> out;
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		const PPToken& tok = tokens[i];
		if (tok.kind != PP_IDENTIFIER || active.count(tok.data) != 0 || macros.count(tok.data) == 0)
		{
			out.push_back(tok);
			continue;
		}

		MacroDef def = macros[tok.data];
		if (!def.function_like)
		{
			set<string> next_active = active;
			next_active.insert(tok.data);
			vector<PPToken> repl = ExpandTokens(def.replacement, macros, next_active);
			out.insert(out.end(), repl.begin(), repl.end());
			continue;
		}

		if (i + 1 >= tokens.size() || tokens[i + 1].kind == PP_WS || tokens[i + 1].kind != PP_OP || tokens[i + 1].data != "(")
		{
			out.push_back(tok);
			continue;
		}

		size_t j = i + 2;
		int depth = 1;
		vector<vector<PPToken> > args(1);
		while (j < tokens.size() && depth > 0)
		{
			if (tokens[j].kind == PP_OP && tokens[j].data == "(")
			{
				++depth;
				args.back().push_back(tokens[j]);
			}
			else if (tokens[j].kind == PP_OP && tokens[j].data == ")")
			{
				--depth;
				if (depth > 0)
				{
					args.back().push_back(tokens[j]);
				}
			}
			else if (depth == 1 && tokens[j].kind == PP_OP && tokens[j].data == ",")
			{
				args.push_back(vector<PPToken>());
			}
			else
			{
				args.back().push_back(tokens[j]);
			}
			++j;
		}
		if (depth != 0)
		{
			throw runtime_error("unterminated macro invocation");
		}
		if (args.size() == 1 && args[0].empty() && def.params.empty() && !def.variadic)
		{
			args.clear();
		}

		if ((!def.variadic && args.size() != def.params.size()) ||
			(def.variadic && args.size() < def.params.size()))
		{
			out.push_back(tok);
			continue;
		}

		set<string> next_active = active;
		next_active.insert(tok.data);
		vector<PPToken> repl = Substitute(def, args, macros, next_active);
		repl = ExpandTokens(repl, macros, next_active);
		out.insert(out.end(), repl.begin(), repl.end());
		i = j - 1;
	}
	return out;
}

void EmitPostTokenSequence(const vector<PPToken>& flat, DebugPostTokenOutputStream& output)
{
	for (size_t i = 0; i < flat.size(); ++i)
	{
		const PPToken& tok = flat[i];
		if (tok.kind == PP_HEADER || tok.kind == PP_NONWS)
		{
			output.emit_invalid(tok.data);
			continue;
		}
		if (tok.kind == PP_IDENTIFIER || tok.kind == PP_OP)
		{
			if (tok.kind == PP_OP &&
				(tok.data == "#" || tok.data == "##" || tok.data == "%:" || tok.data == "%:%:"))
			{
				output.emit_invalid(tok.data);
				continue;
			}
			auto it = StringToTokenTypeMap.find(tok.data);
			if (it == StringToTokenTypeMap.end()) output.emit_identifier(tok.data);
			else output.emit_simple(tok.data, it->second);
			continue;
		}
		if (tok.kind == PP_NUMBER)
		{
			if (ParseFloatingLiteral(tok.data, output) || ParseIntegerLiteral(tok.data, output)) continue;
			output.emit_invalid(tok.data);
			continue;
		}
		if (tok.kind == PP_CHAR || tok.kind == PP_UD_CHAR)
		{
			if (tok.kind == PP_UD_CHAR)
			{
				string suffix;
				string core = StripUDSuffix(tok.data, suffix);
				EFundamentalType type;
				vector<unsigned char> bytes;
				if (!suffix.empty() && IsValidUDSuffix(suffix) && ParseCharacterLiteralCore(core, type, bytes))
					output.emit_user_defined_literal_character(tok.data, suffix, type, bytes.data(), bytes.size());
				else
					output.emit_invalid(tok.data);
			}
			else
			{
				EFundamentalType type;
				vector<unsigned char> bytes;
				if (ParseCharacterLiteralCore(tok.data, type, bytes))
					output.emit_literal(tok.data, type, bytes.data(), bytes.size());
				else
					output.emit_invalid(tok.data);
			}
			continue;
		}
		if (tok.kind == PP_STRING || tok.kind == PP_UD_STRING)
		{
			EFundamentalType type;
			vector<unsigned char> bytes;
			size_t elems = 0;
			if (tok.kind == PP_UD_STRING)
			{
				string suffix;
				string core = StripUDSuffix(tok.data, suffix);
				if (!suffix.empty() && IsValidUDSuffix(suffix) && ParseStringLiteralCore(core, type, bytes, elems))
					output.emit_user_defined_literal_string_array(tok.data, suffix, elems, type, bytes.data(), bytes.size());
				else
					output.emit_invalid(tok.data);
			}
			else
			{
				if (ParseStringLiteralCore(tok.data, type, bytes, elems))
					output.emit_literal_array(tok.data, elems, type, bytes.data(), bytes.size());
				else
					output.emit_invalid(tok.data);
			}
			continue;
		}
		output.emit_invalid(tok.data);
	}
}

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		vector<int> cps = TransformSource(DecodeUTF8(oss.str()));
		if (!cps.empty() && cps.back() != '\n')
		{
			cps.push_back('\n');
		}

		PPCollector collector;
		PPTokenizer tokenizer(collector);
		vector<PPToken> tokens = tokenizer.tokenize(cps);

		map<string, MacroDef> macros;
		vector<PPToken> line;
		DebugPostTokenOutputStream output;

		for (size_t i = 0; i <= tokens.size(); ++i)
		{
			bool flush = i == tokens.size() || tokens[i].kind == PP_NL;
			if (!flush)
			{
				line.push_back(tokens[i]);
				continue;
			}

			vector<PPToken> trimmed = line;
			while (!trimmed.empty() && trimmed.front().kind == PP_WS) trimmed.erase(trimmed.begin());
			if (!trimmed.empty() && trimmed.front().kind == PP_OP && trimmed.front().data == "#")
			{
				size_t p = 1;
				while (p < trimmed.size() && trimmed[p].kind == PP_WS) ++p;
				if (p >= trimmed.size() || trimmed[p].kind != PP_IDENTIFIER)
				{
					throw runtime_error("invalid directive");
				}
				string directive = trimmed[p].data;
				++p;
				if (directive == "define")
				{
					while (p < trimmed.size() && trimmed[p].kind == PP_WS) ++p;
					if (p >= trimmed.size() || trimmed[p].kind != PP_IDENTIFIER)
					{
						throw runtime_error("invalid define");
					}
					string name = trimmed[p].data;
					if (name == "__VA_ARGS__")
					{
						throw runtime_error("invalid define");
					}
					MacroDef def;
					++p;
					if (p < trimmed.size() && trimmed[p].kind == PP_OP && trimmed[p].data == "(")
					{
						def.function_like = true;
						++p;
						bool expect_param = true;
						while (p < trimmed.size() && !(trimmed[p].kind == PP_OP && trimmed[p].data == ")"))
						{
							if (trimmed[p].kind == PP_WS)
							{
								++p;
								continue;
							}
							if (trimmed[p].kind == PP_IDENTIFIER)
							{
								if (!expect_param)
								{
									throw runtime_error("invalid function-like define");
								}
								def.params.push_back(trimmed[p].data);
								expect_param = false;
								++p;
							}
							else if (trimmed[p].kind == PP_OP && trimmed[p].data == ",")
							{
								if (expect_param)
								{
									throw runtime_error("invalid function-like define");
								}
								expect_param = true;
								++p;
							}
							else if (trimmed[p].kind == PP_OP && trimmed[p].data == "...")
							{
								if (!expect_param)
								{
									throw runtime_error("invalid function-like define");
								}
								def.variadic = true;
								expect_param = false;
								++p;
								break;
							}
							else
							{
								throw runtime_error("invalid function-like define");
							}
						}
						if (p >= trimmed.size() || !(trimmed[p].kind == PP_OP && trimmed[p].data == ")"))
						{
							throw runtime_error("invalid function-like define");
						}
						if (expect_param && !def.params.empty() && !def.variadic)
						{
							throw runtime_error("invalid function-like define");
						}
						++p;
					}
					if (p < trimmed.size() && trimmed[p].kind == PP_WS) ++p;
					def.replacement.assign(trimmed.begin() + p, trimmed.end());
					for (size_t r = 0; r < def.replacement.size(); ++r)
					{
						if (def.replacement[r].kind == PP_IDENTIFIER &&
							def.replacement[r].data == "__VA_ARGS__" &&
							!def.variadic)
						{
							throw runtime_error("invalid define");
						}
						if (def.replacement[r].kind == PP_OP && def.replacement[r].data == "#")
						{
							bool ok = def.function_like &&
								r + 1 < def.replacement.size() &&
								def.replacement[r + 1].kind == PP_IDENTIFIER;
							if (ok)
							{
								ok = false;
								for (size_t q = 0; q < def.params.size(); ++q)
								{
									if (def.params[q] == def.replacement[r + 1].data)
									{
										ok = true;
										break;
									}
								}
								if (def.variadic && def.replacement[r + 1].data == "__VA_ARGS__")
								{
									ok = true;
								}
							}
							if (!ok)
							{
								throw runtime_error("invalid define");
							}
						}
						if (def.replacement[r].kind == PP_OP && def.replacement[r].data == "##")
						{
							size_t left = r;
							while (left > 0 && def.replacement[left - 1].kind == PP_WS) --left;
							size_t right = r + 1;
							while (right < def.replacement.size() && def.replacement[right].kind == PP_WS) ++right;
							if (left == 0 || right >= def.replacement.size())
							{
								throw runtime_error("invalid define");
							}
						}
					}
					string key = MacroKey(def);
					if (macros.count(name) != 0 && MacroKey(macros[name]) != key)
					{
						throw runtime_error("invalid redefine");
					}
					macros[name] = def;
				}
				else if (directive == "undef")
				{
					while (p < trimmed.size() && trimmed[p].kind == PP_WS) ++p;
					if (p >= trimmed.size() || trimmed[p].kind != PP_IDENTIFIER)
					{
						throw runtime_error("invalid undef");
					}
					string name = trimmed[p].data;
					++p;
					while (p < trimmed.size() && trimmed[p].kind == PP_WS) ++p;
					if (p != trimmed.size())
					{
						throw runtime_error("extra tokens after undef");
					}
					macros.erase(name);
				}
				else
				{
					throw runtime_error("unsupported directive");
				}
			}
			else if (!trimmed.empty())
			{
				vector<PPToken> text = CollapseWhitespace(trimmed);
				for (size_t k = 0; k < text.size(); ++k)
				{
					if (text[k].kind == PP_IDENTIFIER && text[k].data == "__VA_ARGS__")
					{
						throw runtime_error("invalid __VA_ARGS__");
					}
				}
				vector<PPToken> expanded = ExpandTokens(text, macros, set<string>());
				vector<PPToken> no_ws;
				for (size_t k = 0; k < expanded.size(); ++k)
				{
					if (expanded[k].kind != PP_WS) no_ws.push_back(expanded[k]);
				}
				EmitPostTokenSequence(no_ws, output);
			}
			line.clear();
		}

		output.emit_eof();
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
