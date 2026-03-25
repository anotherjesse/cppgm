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
	vector<pair<string, string>> variables;
	vector<pair<string, string>> functions;
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
	bool is_decl_kw(const string& s) const
	{
		static const vector<string> kws = {"typedef", "static", "thread_local", "extern", "const", "volatile", "char", "char16_t", "char32_t", "wchar_t", "bool", "short", "int", "long", "signed", "unsigned", "float", "double", "void"};
		return find(kws.begin(), kws.end(), s) != kws.end();
	}
	string parse_decl_specifier_seq(bool& is_typedef)
	{
		vector<string> specs;
		is_typedef = false;
		while (!eof() && peek().kind == PPKind::Identifier && is_decl_kw(peek().source))
		{
			if (peek().source == "typedef") is_typedef = true;
			else specs.push_back(peek().source);
			++pos;
		}
		if (specs.empty() && !is_typedef) throw runtime_error("decl-specifier expected");
		bool is_const = false, is_volatile = false;
		int n_short = 0, n_long = 0, n_signed = 0, n_unsigned = 0, n_int = 0;
		string atom;
		for (const string& s : specs)
		{
			if (s == "const") is_const = true;
			else if (s == "volatile") is_volatile = true;
			else if (s == "short") ++n_short;
			else if (s == "long") ++n_long;
			else if (s == "signed") ++n_signed;
			else if (s == "unsigned") ++n_unsigned;
			else if (s == "int") ++n_int;
			else if (s == "char" || s == "char16_t" || s == "char32_t" || s == "wchar_t" || s == "bool" || s == "float" || s == "double" || s == "void")
				atom = s;
			else throw runtime_error("unsupported decl-specifier");
		}
		string base;
		if (atom == "char")
		{
			if (n_unsigned) base = "unsigned char";
			else if (n_signed) base = "signed char";
			else base = "char";
		}
		else if (!atom.empty() && atom != "double")
			base = atom;
		else if (atom == "double")
			base = n_long ? "long double" : "double";
		else if (n_short)
			base = (n_unsigned ? "unsigned short int" : "short int");
		else if (n_long >= 2)
			base = (n_unsigned ? "unsigned long long int" : "long long int");
		else if (n_long == 1)
			base = (n_unsigned ? "unsigned long int" : "long int");
		else
			base = n_unsigned ? "unsigned int" : "int";
		if (is_const && is_volatile) return "const volatile " + base;
		if (is_const) return "const " + base;
		if (is_volatile) return "volatile " + base;
		return base;
	}
	pair<string, string> parse_simple_declarator(const string& base)
	{
		string type = base;
		while (true)
		{
			if (match_punc("*")) type = "pointer to " + type;
			else if (match_punc("&")) type = "lvalue-reference to " + type;
			else if (match_punc("&&")) type = "rvalue-reference to " + type;
			else break;
		}
		string name = expect_identifier();
		if (peek().kind == PPKind::PreprocessingOpOrPunc && (peek().source == "(" || peek().source == "[")) throw runtime_error("unsupported declarator");
		return make_pair(name, type);
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
		bool is_typedef = false;
		string base = parse_decl_specifier_seq(is_typedef);
		vector<pair<string, string>> decls;
		decls.push_back(parse_simple_declarator(base));
		while (match_punc(","))
			decls.push_back(parse_simple_declarator(base));
		if (!match_punc(";")) throw runtime_error("; expected");
		if (!is_typedef)
			cur.variables.insert(cur.variables.end(), decls.begin(), decls.end());
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
	for (const auto& var : ns.variables)
		out << "variable " << var.first << " " << var.second << endl;
	for (const auto& fn : ns.functions)
		out << "function " << fn.first << " " << fn.second << endl;
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
