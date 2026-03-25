// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

#define CPPGM_PREPROC_EMBED
#include "preproc.cpp"
#undef CPPGM_PREPROC_EMBED

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

vector<PPToken> RunPreproc(istream& in, const string& srcfile)
{
	ostringstream src;
	src << in.rdbuf();
	MacroProcessor proc;
	PreprocContext ctx;
	proc.enable_predefined = true;
	SeedPredefinedMacros(proc, srcfile);
	vector<PPToken> pp = preprocess_text_basic(src.str(), proc, srcfile, ctx);
	reject_obvious_invalid(pp);
	return pp;
}

vector<PPToken> SignificantPP(const vector<PPToken>& pp)
{
	vector<PPToken> out;
	for (const PPToken& tok : pp)
		if (tok.kind != PPKind::Whitespace && tok.kind != PPKind::NewLine)
			out.push_back(tok);
	return out;
}

bool IsOpenBracket(const string& s) { return s == "(" || s == "[" || s == "{"; }
bool IsCloseBracket(const string& s) { return s == ")" || s == "]" || s == "}"; }
bool MatchingBracket(const string& a, const string& b)
{
	return (a == "(" && b == ")") || (a == "[" && b == "]") || (a == "{" && b == "}");
}

bool IsTemplateNameToken(const PPToken& tok)
{
	return tok.kind == PPKind::Identifier && PA6_IsTemplateName(tok.source);
}

bool HasBadClosingAnglePattern(const vector<PPToken>& toks)
{
	for (size_t i = 0; i + 1 < toks.size(); ++i)
	{
		if (!IsTemplateNameToken(toks[i]) || toks[i + 1].kind != PPKind::PreprocessingOpOrPunc || toks[i + 1].source != "<")
			continue;
		int angle_depth = 1;
		vector<string> other;
		size_t end = toks.size();
		for (size_t j = i + 2; j < toks.size(); ++j)
		{
			if (toks[j].kind == PPKind::PreprocessingOpOrPunc)
			{
				const string& s = toks[j].source;
				if (IsOpenBracket(s)) { other.push_back(s); continue; }
				if (IsCloseBracket(s))
				{
					if (other.empty() || !MatchingBracket(other.back(), s)) return true;
					other.pop_back();
					continue;
				}
				if (!other.empty()) continue;
				if (s == "<") { ++angle_depth; continue; }
				if (s == ">")
				{
					--angle_depth;
					if (angle_depth == 0) { end = j; break; }
					if (angle_depth < 0) return true;
					continue;
				}
				if (s == ">>")
				{
					angle_depth -= 2;
					if (angle_depth <= 0) { end = j; break; }
					continue;
				}
			}
		}
		if (end >= toks.size()) continue;
		if (end + 2 < toks.size() && toks[end + 1].kind == PPKind::PPNumber &&
			toks[end + 2].kind == PPKind::PreprocessingOpOrPunc &&
			(toks[end + 2].source == ">" || toks[end + 2].source == ">>"))
			return true;
	}
	return false;
}

void DoRecog(istream& in, const string& srcfile)
{
	vector<PPToken> sig = SignificantPP(RunPreproc(in, srcfile));
	vector<string> stack;
	for (const PPToken& tok : sig)
	{
		if (tok.kind != PPKind::PreprocessingOpOrPunc) continue;
		if (IsOpenBracket(tok.source)) stack.push_back(tok.source);
		else if (IsCloseBracket(tok.source))
		{
			if (stack.empty() || !MatchingBracket(stack.back(), tok.source))
				throw runtime_error("unbalanced brackets");
			stack.pop_back();
		}
	}
	if (!stack.empty()) throw runtime_error("unbalanced brackets");
	if (HasBadClosingAnglePattern(sig)) throw runtime_error("bad closing angle bracket");
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

		out << "recog " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i+2];

			try
			{
				ifstream in(srcfile);
				DoRecog(in, srcfile);
				out << srcfile << " OK" << endl;
			}
			catch (exception& e)
			{
				cerr << e.what() << endl;
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
