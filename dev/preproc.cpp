// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <utility>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>

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

vector<PPToken> preprocess_text_basic(const string& input)
{
	PPTokenizer tokenizer;
	vector<PPToken> toks = tokenizer.run(input);
	MacroProcessor proc;
	vector<PPToken> final_pp;
	size_t i = 0;
	while (i < toks.size())
	{
		bool at_directive = (i == 0 || toks[i - 1].kind == PPKind::NewLine);
		if (at_directive)
		{
			size_t j = i;
			while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
			if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && (toks[j].source == "#" || toks[j].source == "%:"))
			{
				++j;
				while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
				if (j >= toks.size() || toks[j].kind == PPKind::NewLine)
				{
					while (j < toks.size() && toks[j].kind != PPKind::NewLine) ++j;
					i = j + (j < toks.size());
					continue;
				}
				if (toks[j].kind != PPKind::Identifier)
					throw runtime_error("non-directive");
				string directive = toks[j++].source;
				if (directive == "define")
				{
					while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= toks.size() || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					string name = toks[j++].source;
					if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
					MacroDef def;
					if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "(")
					{
						def.function_like = true;
						++j;
						while (j < toks.size())
						{
							while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
							if (j >= toks.size()) throw runtime_error("unterminated define");
							if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
							if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
							{
								def.variadic = true;
								++j;
								while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
								if (j >= toks.size() || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
								++j;
								break;
							}
							if (toks[j].kind != PPKind::Identifier) throw runtime_error("bad parameter");
							string param = toks[j++].source;
							if (param == "__VA_ARGS__") throw runtime_error("bad variadic macro");
							if (find(def.params.begin(), def.params.end(), param) != def.params.end()) throw runtime_error("duplicate parameter " + param + " in macro definition");
							def.params.push_back(param);
							while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
							if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ",")
							{
								++j;
								while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
								if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
								{
									def.variadic = true;
									++j;
									while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
									if (j >= toks.size() || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
									++j;
									break;
								}
								continue;
							}
							if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
							throw runtime_error("bad parameter list");
						}
					}
					else if (j < toks.size() && toks[j].kind == PPKind::Whitespace)
					{
						while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
					}
					size_t end = j;
					while (end < toks.size() && toks[end].kind != PPKind::NewLine) ++end;
					def.replacement = ToMacroTokens(toks, j, end);
					if (!def.variadic)
					{
						for (const MacroToken& tok : def.replacement)
							if (!tok.placemarker && tok.pp.kind == PPKind::Identifier && tok.pp.source == "__VA_ARGS__")
								throw runtime_error("__VA_ARGS__ outside variadic macro");
					}
					ValidateReplacement(def);
					proc.define_macro(name, def);
					i = end + (end < toks.size());
					continue;
				}
				if (directive == "undef")
				{
					while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= toks.size() || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					string name = toks[j++].source;
					if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
					while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
					if (j < toks.size() && toks[j].kind != PPKind::NewLine) throw runtime_error("extra tokens after undef");
					proc.macros.erase(name);
					i = j + (j < toks.size());
					continue;
				}
				if (directive == "error")
					throw runtime_error("#error");
				if (directive == "if" || directive == "ifdef" || directive == "ifndef" || directive == "elif" || directive == "else" || directive == "endif" || directive == "include" || directive == "line" || directive == "pragma")
					throw runtime_error("pa5 directive not implemented");
				throw runtime_error("non-directive");
			}
		}
		size_t end = i;
		while (end < toks.size())
		{
			if (end == i ? false : toks[end - 1].kind == PPKind::NewLine)
			{
				size_t k = end;
				while (k < toks.size() && toks[k].kind == PPKind::Whitespace) ++k;
				if (k < toks.size() && toks[k].kind == PPKind::PreprocessingOpOrPunc && (toks[k].source == "#" || toks[k].source == "%:"))
					break;
			}
			++end;
		}
		vector<MacroToken> text = ToMacroTokens(toks, i, end);
		vector<MacroToken> expanded = proc.expand_text(text);
		for (const MacroToken& t : expanded)
			final_pp.push_back(t.pp);
		i = end;
	}
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

			vector<PPToken> pp = preprocess_text_basic(src.str());
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
