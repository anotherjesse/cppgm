// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <utility>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <set>

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

#define CPPGM_MACRO_EMBED
#include "macro.cpp"
#undef CPPGM_MACRO_EMBED

struct CondFrame
{
	bool parent_active;
	bool active;
	bool seen_true;
	bool saw_else;
};

struct PreprocContext
{
	set<PA5FileId> pragma_once_files;
};

struct ExprParser
{
	vector<PPToken> toks;
	size_t pos = 0;
	const MacroProcessor* proc = nullptr;

	bool eof() const { return pos >= toks.size(); }
	bool match(const string& s)
	{
		if (!eof() && toks[pos].kind == PPKind::PreprocessingOpOrPunc && toks[pos].source == s) { ++pos; return true; }
		return false;
	}
	bool match_id(const string& s)
	{
		if (!eof() && toks[pos].kind == PPKind::Identifier && toks[pos].source == s) { ++pos; return true; }
		return false;
	}
	long long primary()
	{
		if (match("("))
		{
			long long v = expr();
			if (!match(")")) throw runtime_error("bad #if");
			return v;
		}
		if (match_id("defined"))
		{
			string name;
			if (match("("))
			{
				if (eof() || toks[pos].kind != PPKind::Identifier) throw runtime_error("bad #if");
				name = toks[pos++].source;
				if (!match(")")) throw runtime_error("bad #if");
			}
			else
			{
				if (eof() || toks[pos].kind != PPKind::Identifier) throw runtime_error("bad #if");
				name = toks[pos++].source;
			}
			return (proc && proc->macros.find(name) != proc->macros.end()) ? 1 : 0;
		}
		if (eof()) throw runtime_error("bad #if");
		if (toks[pos].kind == PPKind::Identifier)
		{
			string name = toks[pos++].source;
			if (name == "true") return 1;
			if (name == "false") return 0;
			return 0;
		}
		if (toks[pos].kind == PPKind::PPNumber)
		{
			string prefix, uds; EFundamentalType type; vector<unsigned char> bytes;
			if (!parse_integer_literal(toks[pos].source, prefix, uds, type, bytes) || !uds.empty())
				throw runtime_error("bad #if");
			++pos;
			if (type == FT_INT) { int v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			if (type == FT_LONG_INT) { long v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			if (type == FT_LONG_LONG_INT) { long long v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			if (type == FT_UNSIGNED_INT) { unsigned int v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			if (type == FT_UNSIGNED_LONG_INT) { unsigned long v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			if (type == FT_UNSIGNED_LONG_LONG_INT) { unsigned long long v; memcpy(&v, bytes.data(), sizeof(v)); return v; }
			return 0;
		}
		throw runtime_error("bad #if");
	}
	long long unary()
	{
		if (match("+")) return unary();
		if (match("-")) return -unary();
		if (match("!")) return !unary();
		if (match("~")) return ~unary();
		return primary();
	}
	long long mul()
	{
		long long v = unary();
		while (true)
		{
			if (match("*")) v = v * unary();
			else if (match("/")) { long long r = unary(); if (!r) throw runtime_error("bad #if"); v = v / r; }
			else if (match("%")) { long long r = unary(); if (!r) throw runtime_error("bad #if"); v = v % r; }
			else break;
		}
		return v;
	}
	long long add()
	{
		long long v = mul();
		while (true)
		{
			if (match("+")) v += mul();
			else if (match("-")) v -= mul();
			else break;
		}
		return v;
	}
	long long rel()
	{
		long long v = add();
		while (true)
		{
			if (match("<")) v = v < add();
			else if (match(">")) v = v > add();
			else if (match("<=")) v = v <= add();
			else if (match(">=")) v = v >= add();
			else break;
		}
		return v;
	}
	long long eq()
	{
		long long v = rel();
		while (true)
		{
			if (match("==")) v = v == rel();
			else if (match("!=")) v = v != rel();
			else break;
		}
		return v;
	}
	long long land()
	{
		long long v = eq();
		while (match("&&")) v = (v && eq());
		return v;
	}
	long long expr()
	{
		long long v = land();
		while (match("||")) v = (v || land());
		return v;
	}
};

bool eval_if_expr(MacroProcessor& proc, const vector<PPToken>& line, size_t begin, size_t end, int current_line_no)
{
	vector<MacroToken> text = ToMacroTokens(line, begin, end);
	for (size_t i = 0; i < text.size(); ++i)
		if (text[i].pp.kind == PPKind::Identifier && text[i].pp.source == "__LINE__")
			text[i].pp = PPToken{PPKind::PPNumber, to_string(current_line_no)};
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i].pp.kind != PPKind::Identifier || text[i].pp.source != "defined") continue;
		size_t j = i + 1;
		while (j < text.size() && text[j].pp.kind == PPKind::Whitespace) ++j;
		if (j < text.size() && text[j].pp.kind == PPKind::PreprocessingOpOrPunc && text[j].pp.source == "(")
		{
			++j;
			while (j < text.size() && text[j].pp.kind == PPKind::Whitespace) ++j;
			if (j < text.size() && text[j].pp.kind == PPKind::Identifier)
				text[j].unavailable = true;
		}
		else if (j < text.size() && text[j].pp.kind == PPKind::Identifier)
			text[j].unavailable = true;
	}
	vector<MacroToken> expanded = proc.expand_text(text);
	vector<PPToken> pp;
	for (const MacroToken& t : expanded)
		if (t.pp.kind != PPKind::Whitespace && t.pp.kind != PPKind::NewLine)
			pp.push_back(t.pp);
	ExprParser parser;
	parser.toks = pp;
	parser.proc = &proc;
	long long v = parser.expr();
	if (!parser.eof()) throw runtime_error("bad #if");
	return v != 0;
}

string DirOf(const string& path)
{
	size_t slash = path.rfind('/');
	return slash == string::npos ? "" : path.substr(0, slash + 1);
}

string HeaderNameToPath(const string& s)
{
	return s.size() >= 2 ? s.substr(1, s.size() - 2) : "";
}

string StringLiteralToPath(const string& s)
{
	StringData data; string uds; bool is_char = false;
	if (!decode_string_or_char(s, false, data, uds, is_char) || is_char || data.type != FT_CHAR) throw runtime_error("bad include");
	if (data.bytes.empty()) return "";
	return string(data.bytes.begin(), data.bytes.end() - 1);
}

bool ReadFileText(const string& path, string& out)
{
	ifstream in(path.c_str(), ios::binary);
	if (!in) return false;
	ostringstream oss;
	oss << in.rdbuf();
	out = oss.str();
	return true;
}

bool DecodePragmaString(const string& src, string& out)
{
	StringData data; string uds; bool is_char = false;
	if (!decode_string_or_char(src, false, data, uds, is_char) || is_char || !uds.empty()) return false;
	if (data.bytes.empty()) return false;
	if (data.type == FT_CHAR)
		out.assign(data.bytes.begin(), data.bytes.end() - 1);
	else
	{
		vector<unsigned char> body = data.bytes;
		body.resize(body.size() - (data.type == FT_CHAR16_T ? 2 : 4));
		vector<int> cps = bytes_to_codepoints(data.type, body);
		out.clear();
		for (int cp : cps)
		{
			if (cp < 0 || cp > 0x7F) return false;
			out.push_back(static_cast<char>(cp));
		}
	}
	return true;
}

void MarkPragmaOnce(const string& current_file, PreprocContext& ctx)
{
	PA5FileId fileid;
	if (PA5GetFileId(current_file, fileid))
		ctx.pragma_once_files.insert(fileid);
}

vector<PPToken> preprocess_text_basic(const string& input, MacroProcessor& proc, const string& current_file, PreprocContext& ctx);
vector<MacroToken> apply_pragma_ops(const vector<MacroToken>& in);
MacroDef ObjectMacro(const string& body);
vector<PPToken> preprocess_file_basic(const string& path, MacroProcessor& proc, PreprocContext& ctx)
{
	PA5FileId fileid;
	if (PA5GetFileId(path, fileid) && ctx.pragma_once_files.count(fileid))
		return {};
	string text;
	if (!ReadFileText(path, text)) throw runtime_error("could not open include file");
	return preprocess_text_basic(text, proc, path, ctx);
}

vector<MacroToken> apply_pragma_ops(const vector<MacroToken>& in, const string& current_file, PreprocContext& ctx)
{
	vector<MacroToken> out;
	for (size_t i = 0; i < in.size(); ++i)
	{
		if (in[i].pp.kind == PPKind::Identifier && in[i].pp.source == "_Pragma")
		{
			size_t j = i + 1;
			while (j < in.size() && in[j].pp.kind == PPKind::Whitespace) ++j;
			if (j >= in.size() || in[j].pp.kind != PPKind::PreprocessingOpOrPunc || in[j].pp.source != "(") throw runtime_error("bad _Pragma");
			++j;
			while (j < in.size() && in[j].pp.kind == PPKind::Whitespace) ++j;
			if (j >= in.size() || in[j].pp.kind != PPKind::StringLiteral) throw runtime_error("bad _Pragma");
			string pragma_text;
			if (!DecodePragmaString(in[j].pp.source, pragma_text)) throw runtime_error("bad _Pragma");
			++j;
			while (j < in.size() && in[j].pp.kind == PPKind::Whitespace) ++j;
			if (j >= in.size() || in[j].pp.kind != PPKind::PreprocessingOpOrPunc || in[j].pp.source != ")") throw runtime_error("bad _Pragma");
			if (pragma_text == "once")
				MarkPragmaOnce(current_file, ctx);
			i = j;
			continue;
		}
		out.push_back(in[i]);
	}
	return out;
}

vector<PPToken> preprocess_text_basic(const string& input, MacroProcessor& proc, const string& current_file, PreprocContext& ctx)
{
	PPTokenizer tokenizer;
	vector<PPToken> toks = tokenizer.run(input);
	for (PPToken& tok : toks)
	{
		if (tok.kind == PPKind::PreprocessingOpOrPunc && tok.source == "%:")
			tok.source = "#";
		else if (tok.kind == PPKind::PreprocessingOpOrPunc && tok.source == "%:%:")
			tok.source = "##";
	}
	vector<PPToken> final_pp;
	vector<CondFrame> conds;
	size_t i = 0;
	int line_no = toks.empty() ? 1 : toks[0].line_no;
	int line_delta = 0;
	string presumed_file = current_file;
	auto presumed_line = [&](int physical_line) { return physical_line + line_delta; };
	auto is_directive_line = [&](size_t begin, size_t end) {
		size_t j = begin;
		while (j < end && toks[j].kind == PPKind::Whitespace) ++j;
		return j < end && toks[j].kind == PPKind::PreprocessingOpOrPunc && (toks[j].source == "#" || toks[j].source == "%:");
	};
	auto append_text_line = [&](vector<MacroToken>& dst, size_t begin, size_t end) {
		for (size_t k = begin; k < end; ++k)
		{
			MacroToken mt(toks[k]);
			mt.line_no = presumed_line(toks[k].line_no);
			mt.file_name = presumed_file;
			dst.push_back(mt);
		}
	};
	while (i < toks.size())
	{
		line_no = toks[i].line_no;
		proc.macros["__FILE__"] = ObjectMacro("\"" + presumed_file + "\"");
		proc.macros["__LINE__"] = ObjectMacro(to_string(presumed_line(line_no)));
		size_t line_end = i;
		while (line_end < toks.size() && toks[line_end].kind != PPKind::NewLine) ++line_end;
		bool has_nl = line_end < toks.size();
		auto advance_line = [&]() {
			i = line_end + has_nl;
			if (i < toks.size()) line_no = toks[i].line_no;
			else if (has_nl) ++line_no;
		};
		bool current_active = true;
		for (const CondFrame& f : conds) current_active = current_active && f.active;
		if (is_directive_line(i, line_end))
		{
			size_t j = i;
			while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
			if (j < line_end && toks[j].kind == PPKind::PreprocessingOpOrPunc && (toks[j].source == "#" || toks[j].source == "%:"))
			{
				++j;
				while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
				if (j >= line_end)
				{
					advance_line();
					continue;
				}
				if (toks[j].kind != PPKind::Identifier)
				{
					if (current_active) throw runtime_error("non-directive");
					i = line_end + has_nl;
					continue;
				}
				string directive = toks[j++].source;
				if (directive == "if")
				{
					bool parent = current_active;
					bool value = false;
					if (parent) value = eval_if_expr(proc, toks, j, line_end, presumed_line(line_no));
					conds.push_back({parent, parent && value, parent && value, false});
					advance_line();
					continue;
				}
				if (directive == "ifdef" || directive == "ifndef")
				{
					bool parent = current_active;
					while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= line_end || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					bool def = proc.macros.find(toks[j].source) != proc.macros.end();
					bool value = directive == "ifdef" ? def : !def;
					conds.push_back({parent, parent && value, parent && value, false});
					advance_line();
					continue;
				}
				if (directive == "elif")
				{
					if (conds.empty() || conds.back().saw_else) throw runtime_error("unexpected #elif");
					CondFrame& f = conds.back();
					bool value = false;
					if (f.parent_active && !f.seen_true) value = eval_if_expr(proc, toks, j, line_end, presumed_line(line_no));
					f.active = f.parent_active && !f.seen_true && value;
					f.seen_true = f.seen_true || f.active;
					advance_line();
					continue;
				}
				if (directive == "else")
				{
					if (conds.empty() || conds.back().saw_else) throw runtime_error("unexpected #else");
					CondFrame& f = conds.back();
					f.saw_else = true;
					f.active = f.parent_active && !f.seen_true;
					f.seen_true = true;
					advance_line();
					continue;
				}
				if (directive == "endif")
				{
					if (conds.empty()) throw runtime_error("unexpected #endif");
					conds.pop_back();
					advance_line();
					continue;
				}
				if (!current_active)
				{
					advance_line();
					continue;
				}
				if (directive == "line")
				{
					vector<MacroToken> text = ToMacroTokens(toks, j, line_end);
					vector<MacroToken> expanded = proc.expand_text(text);
					vector<PPToken> pp;
					for (const MacroToken& t : expanded)
						if (t.pp.kind != PPKind::Whitespace && t.pp.kind != PPKind::NewLine)
							pp.push_back(t.pp);
					if (pp.empty() || pp.size() > 2 || pp[0].kind != PPKind::PPNumber) throw runtime_error("bad #line");
					string prefix, uds; EFundamentalType type; vector<unsigned char> bytes;
					if (!parse_integer_literal(pp[0].source, prefix, uds, type, bytes) || !uds.empty()) throw runtime_error("bad #line");
					unsigned long long value = 0;
					if (type == FT_INT) { int v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else if (type == FT_LONG_INT) { long v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else if (type == FT_LONG_LONG_INT) { long long v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else if (type == FT_UNSIGNED_INT) { unsigned int v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else if (type == FT_UNSIGNED_LONG_INT) { unsigned long v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else if (type == FT_UNSIGNED_LONG_LONG_INT) { unsigned long long v; memcpy(&v, bytes.data(), sizeof(v)); value = v; }
					else throw runtime_error("bad #line");
					if (value == 0) throw runtime_error("bad #line");
					string next_presumed_file = presumed_file;
					if (pp.size() == 2)
					{
						if (pp[1].kind != PPKind::StringLiteral) throw runtime_error("bad #line");
						if (!DecodePragmaString(pp[1].source, next_presumed_file)) throw runtime_error("bad #line");
					}
					advance_line();
					if (i < toks.size())
					{
						presumed_file = next_presumed_file;
						line_delta = static_cast<int>(value) - toks[i].line_no;
					}
					continue;
				}
				if (directive == "define")
				{
					while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= line_end || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					string name = toks[j++].source;
					if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
					MacroDef def;
					if (j < line_end && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "(")
					{
						def.function_like = true;
						++j;
						while (j < line_end)
						{
							while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
							if (j >= line_end) throw runtime_error("unterminated define");
							if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
							if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
							{
								def.variadic = true;
								++j;
								while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
								if (j >= line_end || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
								++j;
								break;
							}
							if (toks[j].kind != PPKind::Identifier) throw runtime_error("bad parameter");
							string param = toks[j++].source;
							if (param == "__VA_ARGS__") throw runtime_error("bad variadic macro");
							if (find(def.params.begin(), def.params.end(), param) != def.params.end()) throw runtime_error("duplicate parameter " + param + " in macro definition");
							def.params.push_back(param);
							while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
							if (j < line_end && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ",")
							{
								++j;
								while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
								if (j < line_end && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
								{
									def.variadic = true;
									++j;
									while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
									if (j >= line_end || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
									++j;
									break;
								}
								continue;
							}
							if (j < line_end && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
							throw runtime_error("bad parameter list");
						}
					}
					else if (j < line_end && toks[j].kind == PPKind::Whitespace)
					{
						while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
					}
					def.replacement = ToMacroTokens(toks, j, line_end);
					if (!def.variadic)
					{
						for (const MacroToken& tok : def.replacement)
							if (!tok.placemarker && tok.pp.kind == PPKind::Identifier && tok.pp.source == "__VA_ARGS__")
								throw runtime_error("__VA_ARGS__ outside variadic macro");
					}
					ValidateReplacement(def);
					proc.define_macro(name, def);
					advance_line();
					continue;
				}
				if (directive == "undef")
				{
					while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= line_end || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					string name = toks[j++].source;
					if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
					while (j < line_end && toks[j].kind == PPKind::Whitespace) ++j;
					if (j < line_end) throw runtime_error("extra tokens after undef");
					proc.macros.erase(name);
					advance_line();
					continue;
				}
				if (directive == "include")
				{
					vector<MacroToken> text = ToMacroTokens(toks, j, line_end);
					vector<MacroToken> expanded = proc.expand_text(text);
					vector<PPToken> inc;
					for (const MacroToken& t : expanded)
						if (t.pp.kind != PPKind::Whitespace && t.pp.kind != PPKind::NewLine)
							inc.push_back(t.pp);
					if (inc.size() != 1) throw runtime_error("bad include");
					string nextf;
					if (inc[0].kind == PPKind::HeaderName) nextf = HeaderNameToPath(inc[0].source);
					else if (inc[0].kind == PPKind::StringLiteral) nextf = StringLiteralToPath(inc[0].source);
					else throw runtime_error("bad include");
					string pathrel = DirOf(current_file) + nextf;
					string usepath, tmp;
					if (!pathrel.empty() && ReadFileText(pathrel, tmp)) usepath = pathrel;
					else if (ReadFileText(nextf, tmp)) usepath = nextf;
					else throw runtime_error("bad include");
					vector<PPToken> nested = preprocess_file_basic(usepath, proc, ctx);
					final_pp.insert(final_pp.end(), nested.begin(), nested.end());
					advance_line();
					continue;
				}
				if (directive == "pragma")
				{
					size_t k = j;
					while (k < line_end && toks[k].kind == PPKind::Whitespace) ++k;
					if (k < line_end && toks[k].kind == PPKind::Identifier && toks[k].source == "once")
						MarkPragmaOnce(current_file, ctx);
					advance_line();
					continue;
				}
				if (directive == "error")
					throw runtime_error("#error");
				if (directive == "if" || directive == "ifdef" || directive == "ifndef" || directive == "elif" || directive == "else" || directive == "endif" || directive == "line")
					throw runtime_error("pa5 directive not implemented");
				throw runtime_error("non-directive");
			}
		}
		if (current_active)
		{
			vector<MacroToken> text;
			while (true)
			{
				append_text_line(text, i, line_end);
				if (has_nl)
				{
					MacroToken nl(PPToken{PPKind::NewLine, ""});
					nl.line_no = presumed_line(toks[line_end].line_no);
					nl.file_name = presumed_file;
					text.push_back(nl);
				}
				i = line_end + has_nl;
				if (i < toks.size()) line_no = toks[i].line_no;
				else if (has_nl) ++line_no;
				if (i >= toks.size()) break;
				line_end = i;
				while (line_end < toks.size() && toks[line_end].kind != PPKind::NewLine) ++line_end;
				has_nl = line_end < toks.size();
				current_active = true;
				for (const CondFrame& f : conds) current_active = current_active && f.active;
				if (!current_active || is_directive_line(i, line_end)) break;
				proc.macros["__FILE__"] = ObjectMacro("\"" + presumed_file + "\"");
				proc.macros["__LINE__"] = ObjectMacro(to_string(presumed_line(line_no)));
			}
			vector<MacroToken> expanded = proc.expand_text(text);
			expanded = apply_pragma_ops(expanded, current_file, ctx);
			for (const MacroToken& t : expanded)
				final_pp.push_back(t.pp);
			continue;
		}
		i = line_end + has_nl;
		if (has_nl) ++line_no;
	}
	if (!conds.empty()) throw runtime_error("unterminated #if");
	return final_pp;
}

void reject_obvious_invalid(const vector<PPToken>& pp)
{
	for (const PPToken& tok : pp)
	{
		if (tok.kind == PPKind::HeaderName || tok.kind == PPKind::NonWhitespaceCharacter)
			throw runtime_error("invalid token");
		if (tok.kind == PPKind::PreprocessingOpOrPunc &&
			(tok.source == "#" || tok.source == "##" || tok.source == "%:" || tok.source == "%:%:"))
			throw runtime_error("invalid token");
	}
}

MacroDef ObjectMacro(const string& body)
{
	PPTokenizer tok;
	MacroDef def;
	vector<PPToken> pp = tok.run(body);
	def.replacement = ToMacroTokens(pp, 0, pp.size());
	while (!def.replacement.empty() && (def.replacement.back().pp.kind == PPKind::Whitespace || def.replacement.back().pp.kind == PPKind::NewLine))
		def.replacement.pop_back();
	return def;
}

void SeedPredefinedMacros(MacroProcessor& proc, const string& current_file)
{
	proc.macros["__CPPGM__"] = ObjectMacro("201303L");
	proc.macros["__cplusplus"] = ObjectMacro("201103L");
	proc.macros["__STDC_HOSTED__"] = ObjectMacro("1");
	proc.macros["__CPPGM_AUTHOR__"] = ObjectMacro("\"John Smith\"");
	proc.macros["__FILE__"] = ObjectMacro("\"" + current_file + "\"");
	proc.macros["__LINE__"] = ObjectMacro("1");
	proc.macros["__DATE__"] = ObjectMacro("\"Jan 01 1970\"");
	proc.macros["__TIME__"] = ObjectMacro("\"00:00:00\"");
}

#ifndef CPPGM_PREPROC_EMBED
int main(int argc, char** argv)
{
	try
	{
		vector<string> args;

		for (int i = 1; i < argc; i++)
			args.emplace_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		string outfile = args[1];
		size_t nsrcfiles = args.size() - 2;

		ofstream out(outfile);
		if (!out) throw runtime_error("could not open outfile");

		out << "preproc " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			out << "sof " << srcfile << endl;

			ifstream in(srcfile.c_str(), ios::binary);
			if (!in) throw runtime_error("could not open source file");
			ostringstream src;
			src << in.rdbuf();

			MacroProcessor proc;
			PreprocContext ctx;
			proc.enable_predefined = true;
			SeedPredefinedMacros(proc, srcfile);
			vector<PPToken> pp = preprocess_text_basic(src.str(), proc, srcfile, ctx);
			reject_obvious_invalid(pp);

			streambuf* old = cout.rdbuf(out.rdbuf());
			DebugPostTokenOutputStream output;
			EmitFromPP(pp, output);
			output.emit_eof();
			cout.rdbuf(old);
		}
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
#endif
