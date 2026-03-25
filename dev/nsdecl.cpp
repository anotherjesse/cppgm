// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>

using namespace std;

#define CPPGM_PREPROC_EMBED
#include "preproc.cpp"
#undef CPPGM_PREPROC_EMBED

struct NamespaceNode
{
	string name;
	bool named = false;
	bool inline_ns = false;
	vector<unique_ptr<NamespaceNode>> children;

	NamespaceNode() {}
	NamespaceNode(string name, bool named, bool inline_ns) : name(name), named(named), inline_ns(inline_ns) {}
};

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
	vector<PPToken> sig;
	for (const PPToken& tok : pp)
		if (tok.kind != PPKind::Whitespace && tok.kind != PPKind::NewLine)
			sig.push_back(tok);
	return sig;
}

struct Parser
{
	vector<PPToken> toks;
	size_t pos = 0;

	bool eof() const { return pos >= toks.size(); }
	const PPToken& peek(size_t off = 0) const
	{
		static PPToken eof_tok(PPKind::PreprocessingOpOrPunc, "");
		return pos + off < toks.size() ? toks[pos + off] : eof_tok;
	}
	bool match_punc(const string& s)
	{
		if (!eof() && peek().kind == PPKind::PreprocessingOpOrPunc && peek().source == s) { ++pos; return true; }
		return false;
	}
	bool match_kw(const string& s)
	{
		if (!eof() && peek().kind == PPKind::Identifier && peek().source == s) { ++pos; return true; }
		return false;
	}
	string expect_identifier()
	{
		if (eof() || peek().kind != PPKind::Identifier) throw runtime_error("identifier expected");
		return toks[pos++].source;
	}

	NamespaceNode* get_or_add_named_namespace(NamespaceNode& cur, const string& name, bool inline_ns)
	{
		for (const auto& child : cur.children)
			if (child->named && child->name == name)
			{
				if (inline_ns) child->inline_ns = true;
				return child.get();
			}
		cur.children.emplace_back(new NamespaceNode(name, true, inline_ns));
		return cur.children.back().get();
	}

	NamespaceNode* add_unnamed_namespace(NamespaceNode& cur, bool inline_ns)
	{
		for (const auto& child : cur.children)
			if (!child->named)
			{
				if (inline_ns) child->inline_ns = true;
				return child.get();
			}
		cur.children.emplace_back(new NamespaceNode("", false, inline_ns));
		return cur.children.back().get();
	}

	void parse_declaration(NamespaceNode& cur)
	{
		if (match_punc(";")) return;
		bool inline_ns = match_kw("inline");
		if (match_kw("namespace"))
		{
			string name;
			bool named = false;
			if (!eof() && peek().kind == PPKind::Identifier)
			{
				name = toks[pos++].source;
				named = true;
			}
			if (!match_punc("{")) throw runtime_error("{ expected");
			NamespaceNode* child = named ? get_or_add_named_namespace(cur, name, inline_ns) : add_unnamed_namespace(cur, inline_ns);
			while (!match_punc("}"))
			{
				if (eof()) throw runtime_error("} expected");
				parse_declaration(*child);
			}
			return;
		}
		throw runtime_error("unsupported declaration");
	}

	NamespaceNode parse_translation_unit()
	{
		NamespaceNode root("", false, false);
		while (!eof())
			parse_declaration(root);
		return root;
	}
};

void EmitNamespace(ostream& out, const NamespaceNode& ns)
{
	if (ns.named) out << "start namespace " << ns.name << endl;
	else out << "start unnamed namespace" << endl;
	if (ns.inline_ns) out << "inline namespace" << endl;
	for (const auto& child : ns.children)
		EmitNamespace(out, *child);
	out << "end namespace" << endl;
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
		out << nsrcfiles << " translation units" << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			ifstream in(srcfile);
			if (!in) throw runtime_error("could not open source file");
			vector<PPToken> toks = RunPreproc(in, srcfile);
			Parser p;
			p.toks = toks;
			NamespaceNode root = p.parse_translation_unit();

			out << "start translation unit " << srcfile << endl;
			EmitNamespace(out, root);
			out << "end translation unit" << endl;
		}
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
