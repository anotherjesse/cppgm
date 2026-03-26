// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

#define PREPROC_EMBED_ONLY
#include "preproc.cpp"
#undef PREPROC_EMBED_ONLY

struct NsToken
{
	string term;
	string text;
	bool is_identifier = false;
	bool is_literal = false;
};

enum TypeKind
{
	TY_FUND,
	TY_CV,
	TY_PTR,
	TY_LREF,
	TY_RREF,
	TY_ARRAY,
	TY_FUNCTION
};

struct Type
{
	TypeKind kind = TY_FUND;
	string fund;
	bool is_const = false;
	bool is_volatile = false;
	shared_ptr<Type> sub;
	long long array_bound = -1;
	vector<shared_ptr<Type> > params;
	bool variadic = false;
};

struct NamespaceEntity;

struct VariableEntity
{
	string name;
	shared_ptr<Type> type;
};

struct FunctionEntity
{
	string name;
	shared_ptr<Type> type;
};

struct NamespaceEntity
{
	string name;
	bool named = false;
	bool is_inline = false;
	NamespaceEntity* parent = nullptr;

	vector<shared_ptr<VariableEntity> > var_order;
	vector<shared_ptr<FunctionEntity> > fn_order;
	vector<shared_ptr<NamespaceEntity> > ns_order;

	unordered_map<string, shared_ptr<VariableEntity> > vars;
	unordered_map<string, shared_ptr<FunctionEntity> > fns;
	unordered_map<string, shared_ptr<Type> > typedefs;
	unordered_map<string, shared_ptr<NamespaceEntity> > named_children;
	vector<shared_ptr<NamespaceEntity> > unnamed_children;
	shared_ptr<NamespaceEntity> unique_unnamed;
	unordered_map<string, shared_ptr<NamespaceEntity> > namespace_aliases;
	unordered_map<string, shared_ptr<Type> > using_types;
	vector<shared_ptr<NamespaceEntity> > using_directives;
};

static shared_ptr<Type> CloneType(const shared_ptr<Type>& t);

static shared_ptr<Type> MakeFundamental(const string& name)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_FUND;
	t->fund = name;
	return t;
}

static shared_ptr<Type> MakeCV(const shared_ptr<Type>& sub, bool is_const, bool is_volatile)
{
	if (!is_const && !is_volatile) return sub;
	if (sub->kind == TY_LREF || sub->kind == TY_RREF) return sub;
	if (sub->kind == TY_ARRAY)
	{
		shared_ptr<Type> out = CloneType(sub);
		out->sub = MakeCV(out->sub, is_const, is_volatile);
		return out;
	}
	if (sub->kind == TY_CV)
	{
		shared_ptr<Type> out = CloneType(sub);
		out->is_const = out->is_const || is_const;
		out->is_volatile = out->is_volatile || is_volatile;
		return out;
	}
	shared_ptr<Type> t(new Type());
	t->kind = TY_CV;
	t->sub = sub;
	t->is_const = is_const;
	t->is_volatile = is_volatile;
	return t;
}

static shared_ptr<Type> MakePointer(const shared_ptr<Type>& sub)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_PTR;
	t->sub = sub;
	return t;
}

static shared_ptr<Type> MakeLRef(const shared_ptr<Type>& sub)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_LREF;
	t->sub = sub;
	return t;
}

static shared_ptr<Type> MakeRRef(const shared_ptr<Type>& sub)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_RREF;
	t->sub = sub;
	return t;
}

static shared_ptr<Type> MakeArray(const shared_ptr<Type>& sub, long long bound)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_ARRAY;
	t->sub = sub;
	t->array_bound = bound;
	return t;
}

static shared_ptr<Type> MakeFunction(const shared_ptr<Type>& ret, const vector<shared_ptr<Type> >& params, bool variadic)
{
	shared_ptr<Type> t(new Type());
	t->kind = TY_FUNCTION;
	t->sub = ret;
	t->params = params;
	t->variadic = variadic;
	return t;
}

static shared_ptr<Type> CloneType(const shared_ptr<Type>& t)
{
	if (!t) return t;
	shared_ptr<Type> c(new Type(*t));
	c->sub = CloneType(t->sub);
	c->params.clear();
	for (const auto& p : t->params) c->params.push_back(CloneType(p));
	return c;
}

static shared_ptr<Type> RemoveTopCV(const shared_ptr<Type>& t)
{
	if (t->kind == TY_CV) return CloneType(t->sub);
	return CloneType(t);
}

static shared_ptr<Type> CollapseReference(const shared_ptr<Type>& base, bool want_rref)
{
	if (base->kind == TY_LREF) return MakeLRef(CloneType(base->sub));
	if (base->kind == TY_RREF)
	{
		if (want_rref) return MakeRRef(CloneType(base->sub));
		return MakeLRef(CloneType(base->sub));
	}
	return want_rref ? MakeRRef(CloneType(base)) : MakeLRef(CloneType(base));
}

static shared_ptr<Type> AdjustParameterType(const shared_ptr<Type>& t)
{
	shared_ptr<Type> x = RemoveTopCV(t);
	if (x->kind == TY_ARRAY) return MakePointer(CloneType(x->sub));
	if (x->kind == TY_FUNCTION) return MakePointer(x);
	return x;
}

static string TypeToString(const shared_ptr<Type>& t)
{
	switch (t->kind)
	{
		case TY_FUND: return t->fund;
		case TY_CV:
		{
			if (t->is_const && t->is_volatile) return string("const volatile ") + TypeToString(t->sub);
			if (t->is_const) return string("const ") + TypeToString(t->sub);
			return string("volatile ") + TypeToString(t->sub);
		}
		case TY_PTR: return string("pointer to ") + TypeToString(t->sub);
		case TY_LREF: return string("lvalue-reference to ") + TypeToString(t->sub);
		case TY_RREF: return string("rvalue-reference to ") + TypeToString(t->sub);
		case TY_ARRAY:
		{
			if (t->array_bound < 0) return string("array of unknown bound of ") + TypeToString(t->sub);
			return string("array of ") + to_string(t->array_bound) + " " + TypeToString(t->sub);
		}
		case TY_FUNCTION:
		{
			string s = "function of (";
			for (size_t i = 0; i < t->params.size(); i++)
			{
				if (i) s += ", ";
				s += TypeToString(t->params[i]);
			}
			if (t->variadic)
			{
				if (!t->params.empty()) s += ", ";
				s += "...";
			}
			s += ") returning ";
			s += TypeToString(t->sub);
			return s;
		}
	}
	throw logic_error("unknown type");
}

static vector<NsToken> BuildTokens(const vector<PPToken>& pptoks)
{
	vector<NsToken> out;
	for (const PPToken& t : pptoks)
	{
		if (t.type == "eof") break;
		if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

		if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
		{
			auto it = StringToTokenTypeMap.find(t.data);
			if (it != StringToTokenTypeMap.end())
			{
				NsToken tok;
				tok.term = TokenTypeToStringMap.at(it->second);
				tok.text = t.data;
				if (tok.term == "OP_RSHIFT")
				{
					NsToken a;
					a.term = "OP_GT";
					a.text = ">";
					NsToken b;
					b.term = "OP_GT";
					b.text = ">";
					out.push_back(a);
					out.push_back(b);
				}
				else out.push_back(tok);
			}
			else if (t.type == "identifier")
			{
				NsToken tok;
				tok.term = "TT_IDENTIFIER";
				tok.text = t.data;
				tok.is_identifier = true;
				out.push_back(tok);
			}
			else throw runtime_error("invalid token");
			continue;
		}

		if (t.type == "pp-number" || t.type == "character-literal" || t.type == "user-defined-character-literal" || t.type == "string-literal" || t.type == "user-defined-string-literal")
		{
			NsToken tok;
			tok.term = "TT_LITERAL";
			tok.text = t.data;
			tok.is_literal = true;
			out.push_back(tok);
			continue;
		}

		throw runtime_error("invalid token");
	}
	NsToken eof;
	eof.term = "ST_EOF";
	out.push_back(eof);
	return out;
}

struct PtrOp
{
	enum Kind { PTR, LREF, RREF } kind = PTR;
	bool is_const = false;
	bool is_volatile = false;
};

struct ParsedName
{
	bool has_name = false;
	bool absolute = false;
	vector<string> qualifiers;
	string name;
};

struct Declarator
{
	ParsedName pname;
	vector<PtrOp> ptrs;
	struct Suffix
	{
		bool is_function = false;
		bool has_array_bound = false;
		long long array_bound = -1;
		vector<shared_ptr<Type> > fn_params;
		bool fn_variadic = false;
	};
	vector<Suffix> suffixes;
	shared_ptr<Declarator> inner;
};

struct Parser
{
	vector<NsToken> toks;
	size_t p = 0;
	shared_ptr<NamespaceEntity> global_ns;
	NamespaceEntity* cur = nullptr;

	Parser(vector<NsToken> v, const shared_ptr<NamespaceEntity>& g) : toks(std::move(v)), global_ns(g), cur(g.get()) {}

	const NsToken& Peek(size_t off = 0) const
	{
		if (p + off >= toks.size()) throw runtime_error("unexpected eof");
		return toks[p + off];
	}

	bool Is(const string& term, size_t off = 0) const { return Peek(off).term == term; }

	bool Eat(const string& term)
	{
		if (!Is(term)) return false;
		p++;
		return true;
	}

	void Expect(const string& term)
	{
		if (!Eat(term)) throw runtime_error("expected token");
	}

	string ExpectIdentifier()
	{
		if (!Is("TT_IDENTIFIER")) throw runtime_error("expected identifier");
		return toks[p++].text;
	}

	bool IsDeclSpecifierStart() const
	{
		static const unordered_set<string> k = {
			"KW_TYPEDEF", "KW_STATIC", "KW_THREAD_LOCAL", "KW_EXTERN",
			"KW_CONST", "KW_VOLATILE", "KW_CHAR", "KW_CHAR16_T", "KW_CHAR32_T",
			"KW_WCHAR_T", "KW_BOOL", "KW_SHORT", "KW_INT", "KW_LONG", "KW_SIGNED",
			"KW_UNSIGNED", "KW_FLOAT", "KW_DOUBLE", "KW_VOID", "TT_IDENTIFIER", "OP_COLON2"
		};
		return k.count(Peek().term) != 0;
	}

	shared_ptr<NamespaceEntity> GetOrCreateNamedNamespace(NamespaceEntity* parent, const string& name, bool set_inline)
	{
		auto it = parent->named_children.find(name);
		if (it != parent->named_children.end())
		{
			if (set_inline) it->second->is_inline = true;
			return it->second;
		}
		shared_ptr<NamespaceEntity> ns(new NamespaceEntity());
		ns->name = name;
		ns->named = true;
		ns->is_inline = set_inline;
		ns->parent = parent;
		parent->named_children[name] = ns;
		parent->ns_order.push_back(ns);
		return ns;
	}

	shared_ptr<NamespaceEntity> CreateUnnamedNamespace(NamespaceEntity* parent, bool set_inline)
	{
		if (parent->unique_unnamed)
		{
			if (set_inline) parent->unique_unnamed->is_inline = true;
			return parent->unique_unnamed;
		}
		shared_ptr<NamespaceEntity> ns(new NamespaceEntity());
		ns->named = false;
		ns->is_inline = set_inline;
		ns->parent = parent;
		parent->unnamed_children.push_back(ns);
		parent->ns_order.push_back(ns);
		parent->unique_unnamed = ns;
		// Implicit using-directive for unnamed namespace.
		parent->using_directives.push_back(ns);
		return ns;
	}

	shared_ptr<NamespaceEntity> LookupNamespaceDirect(NamespaceEntity* ns, const string& name)
	{
		auto itn = ns->named_children.find(name);
		if (itn != ns->named_children.end()) return itn->second;
		auto ita = ns->namespace_aliases.find(name);
		if (ita != ns->namespace_aliases.end()) return ita->second;
		return shared_ptr<NamespaceEntity>();
	}

	shared_ptr<NamespaceEntity> LookupNamespaceQualifiedStart(const string& name, bool absolute)
	{
		if (absolute) return LookupNamespaceDirect(global_ns.get(), name);
		for (NamespaceEntity* s = cur; s; s = s->parent)
		{
			shared_ptr<NamespaceEntity> ns = LookupNamespaceDirect(s, name);
			if (ns) return ns;
		}
		return shared_ptr<NamespaceEntity>();
	}

	shared_ptr<NamespaceEntity> ResolveNamespacePath(bool absolute, const vector<string>& comps)
	{
		if (comps.empty()) return global_ns;
		shared_ptr<NamespaceEntity> ns = LookupNamespaceQualifiedStart(comps[0], absolute);
		if (!ns) throw runtime_error("unknown namespace");
		for (size_t i = 1; i < comps.size(); i++)
		{
			ns = LookupNamespaceDirect(ns.get(), comps[i]);
			if (!ns) throw runtime_error("unknown namespace");
		}
		return ns;
	}

	void LookupTypeInNamespaceDFS(NamespaceEntity* ns, const string& name, unordered_set<NamespaceEntity*>& vis, shared_ptr<Type>& out)
	{
		if (!ns || vis.count(ns)) return;
		vis.insert(ns);

		auto it = ns->typedefs.find(name);
		if (it != ns->typedefs.end()) { out = it->second; return; }
		auto iu = ns->using_types.find(name);
		if (iu != ns->using_types.end()) { out = iu->second; return; }

		for (const auto& ch : ns->ns_order)
		{
			if (ch->is_inline || !ch->named)
			{
				LookupTypeInNamespaceDFS(ch.get(), name, vis, out);
				if (out) return;
			}
		}
		for (const auto& ud : ns->using_directives)
		{
			LookupTypeInNamespaceDFS(ud.get(), name, vis, out);
			if (out) return;
		}
	}

	shared_ptr<Type> LookupTypeUnqualified(const string& name)
	{
		for (NamespaceEntity* s = cur; s; s = s->parent)
		{
			shared_ptr<Type> out;
			unordered_set<NamespaceEntity*> vis;
			LookupTypeInNamespaceDFS(s, name, vis, out);
			if (out) return CloneType(out);
		}
		throw runtime_error("unknown type name");
	}

	shared_ptr<Type> LookupTypeQualified(bool absolute, const vector<string>& quals, const string& name)
	{
		shared_ptr<NamespaceEntity> ns;
		if (quals.empty())
		{
			if (absolute) ns = global_ns;
			else return LookupTypeUnqualified(name);
		}
		else ns = ResolveNamespacePath(absolute, quals);

		shared_ptr<Type> out;
		unordered_set<NamespaceEntity*> vis;
		LookupTypeInNamespaceDFS(ns.get(), name, vis, out);
		if (!out) throw runtime_error("unknown type name");
		return CloneType(out);
	}

	ParsedName ParseIdExpression()
	{
		ParsedName pn;
		if (Eat("OP_COLON2"))
		{
			pn.absolute = true;
			pn.qualifiers.push_back(ExpectIdentifier());
			Expect("OP_COLON2");
			while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
			{
				pn.qualifiers.push_back(ExpectIdentifier());
				Expect("OP_COLON2");
			}
			pn.name = ExpectIdentifier();
			pn.has_name = true;
			return pn;
		}

		if (!Is("TT_IDENTIFIER")) throw runtime_error("expected identifier");
		string first = ExpectIdentifier();
		if (!Eat("OP_COLON2"))
		{
			pn.name = first;
			pn.has_name = true;
			return pn;
		}
		pn.qualifiers.push_back(first);
		while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
		{
			pn.qualifiers.push_back(ExpectIdentifier());
			Expect("OP_COLON2");
		}
		pn.name = ExpectIdentifier();
		pn.has_name = true;
		return pn;
	}

	vector<string> ParseNestedNameSpecifier(bool& absolute)
	{
		vector<string> out;
		absolute = false;
		if (Eat("OP_COLON2")) absolute = true;
		if (!Is("TT_IDENTIFIER")) throw runtime_error("expected namespace name");
		out.push_back(ExpectIdentifier());
		Expect("OP_COLON2");
		while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
		{
			out.push_back(ExpectIdentifier());
			Expect("OP_COLON2");
		}
		return out;
	}

	shared_ptr<Type> ParseDeclSpecifierSeq(bool& is_typedef, bool& is_extern)
	{
		is_typedef = false;
		is_extern = false;
		bool any_type = false;
		bool cv_const = false;
		bool cv_volatile = false;
		int n_long = 0;
		bool kw_signed = false;
		bool kw_unsigned = false;
		bool kw_short = false;
		bool kw_int = false;
		bool kw_char = false;
		bool kw_char16 = false;
		bool kw_char32 = false;
		bool kw_wchar = false;
		bool kw_bool = false;
		bool kw_float = false;
		bool kw_double = false;
		bool kw_void = false;
		shared_ptr<Type> named_type;
		bool have_type_words = false;

		while (IsDeclSpecifierStart())
		{
			if (Eat("KW_TYPEDEF")) { is_typedef = true; continue; }
			if (Eat("KW_EXTERN")) { is_extern = true; continue; }
			if (Eat("KW_STATIC") || Eat("KW_THREAD_LOCAL")) continue;
			if (Eat("KW_CONST")) { cv_const = true; any_type = true; continue; }
			if (Eat("KW_VOLATILE")) { cv_volatile = true; any_type = true; continue; }
			if (Eat("KW_SIGNED")) { kw_signed = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_UNSIGNED")) { kw_unsigned = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_SHORT")) { kw_short = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_LONG")) { n_long++; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_INT")) { kw_int = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_CHAR")) { kw_char = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_CHAR16_T")) { kw_char16 = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_CHAR32_T")) { kw_char32 = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_WCHAR_T")) { kw_wchar = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_BOOL")) { kw_bool = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_FLOAT")) { kw_float = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_DOUBLE")) { kw_double = true; any_type = true; have_type_words = true; continue; }
			if (Eat("KW_VOID")) { kw_void = true; any_type = true; have_type_words = true; continue; }

			if (Is("OP_COLON2") || Is("TT_IDENTIFIER"))
			{
				if (named_type || have_type_words) break;
				size_t save = p;
				try
				{
					bool absolute = false;
					vector<string> quals;
					if (Is("OP_COLON2"))
					{
						absolute = true;
						Expect("OP_COLON2");
					}
					if (!Is("TT_IDENTIFIER")) throw runtime_error("expected type-name");
					string first = ExpectIdentifier();
					if (Eat("OP_COLON2"))
					{
						quals.push_back(first);
						while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
						{
							quals.push_back(ExpectIdentifier());
							Expect("OP_COLON2");
						}
						string tname = ExpectIdentifier();
						named_type = LookupTypeQualified(absolute, quals, tname);
						any_type = true;
						have_type_words = true;
						continue;
					}
					// single typedef-name candidate
					named_type = LookupTypeQualified(absolute, quals, first);
					any_type = true;
					have_type_words = true;
					continue;
				}
				catch (exception&)
				{
					p = save;
					break;
				}
			}
			break;
		}

		if (!any_type) throw runtime_error("missing type");
		if (named_type)
		{
			return MakeCV(named_type, cv_const, cv_volatile);
		}

		string base;
		if (kw_char) base = kw_unsigned ? "unsigned char" : (kw_signed ? "signed char" : "char");
		else if (kw_char16) base = "char16_t";
		else if (kw_char32) base = "char32_t";
		else if (kw_wchar) base = "wchar_t";
		else if (kw_bool) base = "bool";
		else if (kw_float) base = "float";
		else if (kw_double) base = (n_long > 0 ? "long double" : "double");
		else if (kw_void) base = "void";
		else
		{
			if (kw_short) base = kw_unsigned ? "unsigned short int" : "short int";
			else if (n_long >= 2) base = kw_unsigned ? "unsigned long long int" : "long long int";
			else if (n_long == 1) base = kw_unsigned ? "unsigned long int" : "long int";
			else
			{
				if (kw_unsigned) base = "unsigned int";
				else base = "int";
			}
		}
		return MakeCV(MakeFundamental(base), cv_const, cv_volatile);
	}

	PtrOp ParsePtrOperator()
	{
		if (Eat("OP_STAR"))
		{
			PtrOp op;
			op.kind = PtrOp::PTR;
			while (true)
			{
				if (Eat("KW_CONST")) op.is_const = true;
				else if (Eat("KW_VOLATILE")) op.is_volatile = true;
				else break;
			}
			return op;
		}
		if (Eat("OP_AMP"))
		{
			PtrOp op;
			op.kind = PtrOp::LREF;
			return op;
		}
		if (Eat("OP_LAND"))
		{
			PtrOp op;
			op.kind = PtrOp::RREF;
			return op;
		}
		throw runtime_error("expected ptr operator");
	}

	shared_ptr<Type> ParseParameterDeclaration()
	{
		bool is_typedef = false;
		bool is_extern = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(is_typedef, is_extern);
		if (is_typedef) throw runtime_error("typedef parameter");

		if (Is("OP_COMMA") || Is("OP_RPAREN") || Is("OP_DOTS"))
		{
			return AdjustParameterType(base);
		}

		Declarator d = ParseDeclarator(true);
		shared_ptr<Type> t = ApplyDeclarator(base, d);
		return AdjustParameterType(t);
	}

	void ParseParameterClause(vector<shared_ptr<Type> >& params, bool& variadic)
	{
		params.clear();
		variadic = false;
		if (Eat("OP_DOTS"))
		{
			variadic = true;
			return;
		}
		if (Is("OP_RPAREN")) return;
		params.push_back(ParseParameterDeclaration());
		while (Eat("OP_COMMA"))
		{
			if (Eat("OP_DOTS"))
			{
				variadic = true;
				return;
			}
			params.push_back(ParseParameterDeclaration());
		}
		if (Eat("OP_DOTS")) variadic = true;
	}

	Declarator ParseDeclarator(bool allow_abstract)
	{
		Declarator d;
		while (Is("OP_STAR") || Is("OP_AMP") || Is("OP_LAND"))
		{
			d.ptrs.push_back(ParsePtrOperator());
		}

		if (Is("OP_LPAREN") && !(allow_abstract && Is("OP_RPAREN", 1)))
		{
			Expect("OP_LPAREN");
			Declarator inner = ParseDeclarator(allow_abstract);
			Expect("OP_RPAREN");
			d.inner = shared_ptr<Declarator>(new Declarator(inner));
		}
		else if (Is("TT_IDENTIFIER") || Is("OP_COLON2"))
		{
			d.pname = ParseIdExpression();
		}
		else if (!allow_abstract)
		{
			throw runtime_error("expected declarator id");
		}

		while (true)
		{
			if (Eat("OP_LSQUARE"))
			{
				Declarator::Suffix s;
				if (Eat("OP_RSQUARE"))
				{
					s.is_function = false;
					s.has_array_bound = false;
					d.suffixes.push_back(s);
				}
				else
				{
					if (!Is("TT_LITERAL")) throw runtime_error("expected literal");
					string lit = toks[p++].text;
					long long v = stoll(lit, nullptr, 0);
					Expect("OP_RSQUARE");
					s.is_function = false;
					s.has_array_bound = true;
					s.array_bound = v;
					d.suffixes.push_back(s);
				}
				continue;
			}
			if (Eat("OP_LPAREN"))
			{
				Declarator::Suffix s;
				s.is_function = true;
				ParseParameterClause(s.fn_params, s.fn_variadic);
				Expect("OP_RPAREN");
				d.suffixes.push_back(s);
				continue;
			}
			break;
		}
		return d;
	}

	shared_ptr<Type> ApplyPtrOps(const shared_ptr<Type>& t, const vector<PtrOp>& ptrs)
	{
		shared_ptr<Type> out = CloneType(t);
		for (int i = static_cast<int>(ptrs.size()) - 1; i >= 0; i--)
		{
			const PtrOp& op = ptrs[i];
			if (op.kind == PtrOp::PTR)
			{
				out = MakePointer(out);
				if (op.is_const || op.is_volatile) out = MakeCV(out, op.is_const, op.is_volatile);
			}
			else if (op.kind == PtrOp::LREF)
			{
				out = CollapseReference(out, false);
			}
			else
			{
				out = CollapseReference(out, true);
			}
		}
		return out;
	}

	shared_ptr<Type> ApplySuffixes(const shared_ptr<Type>& t, const vector<Declarator::Suffix>& suffixes)
	{
		shared_ptr<Type> out = CloneType(t);
		for (int i = static_cast<int>(suffixes.size()) - 1; i >= 0; i--)
		{
			const auto& s = suffixes[i];
			if (!s.is_function)
			{
				out = MakeArray(out, s.has_array_bound ? s.array_bound : -1);
				continue;
			}
			vector<shared_ptr<Type> > params = s.fn_params;
			if (params.size() == 1 && !s.fn_variadic && params[0]->kind == TY_FUND && params[0]->fund == "void") params.clear();
			out = MakeFunction(out, params, s.fn_variadic);
		}
		return out;
	}

	shared_ptr<Type> ApplyDeclarator(const shared_ptr<Type>& base, const Declarator& d)
	{
		if (d.inner)
		{
			shared_ptr<Type> t = ApplySuffixes(base, d.suffixes);
			t = ApplyDeclarator(t, *d.inner);
			t = ApplyPtrOps(t, d.ptrs);
			return t;
		}
		// For non-parenthesized declarators, prefix operators contribute to the
		// declared object/function return type before suffix derivation.
		shared_ptr<Type> t = ApplyPtrOps(base, d.ptrs);
		t = ApplySuffixes(t, d.suffixes);
		return t;
	}

	pair<NamespaceEntity*, string> ResolveDeclaratorTarget(const ParsedName& pname)
	{
		if (!pname.has_name) throw runtime_error("missing name");
		if (pname.qualifiers.empty()) return make_pair(cur, pname.name);
		shared_ptr<NamespaceEntity> ns = ResolveNamespacePath(pname.absolute, pname.qualifiers);
		return make_pair(ns.get(), pname.name);
	}

	void DeclareTypedef(const ParsedName& pname, const shared_ptr<Type>& t)
	{
		if (!pname.qualifiers.empty()) throw runtime_error("qualified typedef");
		cur->typedefs[pname.name] = CloneType(t);
	}

	void DeclareVariable(NamespaceEntity* ns, const string& name, const shared_ptr<Type>& t, bool is_extern)
	{
		(void)is_extern;
		auto it = ns->vars.find(name);
		if (it == ns->vars.end())
		{
			shared_ptr<VariableEntity> v(new VariableEntity());
			v->name = name;
			v->type = CloneType(t);
			ns->vars[name] = v;
			ns->var_order.push_back(v);
		}
		else
		{
			// Prefer complete array bound when redeclared from unknown bound.
			if (it->second->type->kind == TY_ARRAY && it->second->type->array_bound < 0 && t->kind == TY_ARRAY && t->array_bound > 0)
			{
				it->second->type = CloneType(t);
			}
		}
	}

	void DeclareFunction(NamespaceEntity* ns, const string& name, const shared_ptr<Type>& t)
	{
		auto it = ns->fns.find(name);
		if (it == ns->fns.end())
		{
			shared_ptr<FunctionEntity> f(new FunctionEntity());
			f->name = name;
			f->type = CloneType(t);
			ns->fns[name] = f;
			ns->fn_order.push_back(f);
		}
	}

	void ParseSimpleDeclaration()
	{
		bool is_typedef = false;
		bool is_extern = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(is_typedef, is_extern);

		vector<Declarator> ds;
		ds.push_back(ParseDeclarator(false));
		while (Eat("OP_COMMA")) ds.push_back(ParseDeclarator(false));
		Expect("OP_SEMICOLON");

		for (const Declarator& d : ds)
		{
			shared_ptr<Type> t = ApplyDeclarator(base, d);
			ParsedName name = d.pname;
			if (!name.has_name && d.inner) name = d.inner->pname;
			if (is_typedef)
			{
				DeclareTypedef(name, t);
				continue;
			}
			auto trg = ResolveDeclaratorTarget(name);
			if (t->kind == TY_FUNCTION) DeclareFunction(trg.first, trg.second, t);
			else DeclareVariable(trg.first, trg.second, t, is_extern);
		}
	}

	void ParseAliasDeclaration()
	{
		Expect("KW_USING");
		string name = ExpectIdentifier();
		Expect("OP_ASS");
		shared_ptr<Type> t = ParseTypeId();
		Expect("OP_SEMICOLON");
		cur->typedefs[name] = t;
	}

	shared_ptr<Type> ParseTypeId()
	{
		bool td = false;
		bool ex = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(td, ex);
		if (!Is("OP_COMMA") && !Is("OP_RPAREN") && !Is("OP_DOTS") && !Is("OP_SEMICOLON"))
		{
			if (Is("OP_STAR") || Is("OP_AMP") || Is("OP_LAND") || Is("OP_LPAREN") || Is("OP_LSQUARE") || Is("TT_IDENTIFIER") || Is("OP_COLON2"))
			{
				Declarator d = ParseDeclarator(true);
				base = ApplyDeclarator(base, d);
			}
		}
		return base;
	}

	void ParseNamespaceDefinition()
	{
		bool inl = Eat("KW_INLINE");
		Expect("KW_NAMESPACE");
		shared_ptr<NamespaceEntity> ns;
		if (Eat("OP_LBRACE"))
		{
			ns = CreateUnnamedNamespace(cur, inl);
		}
		else
		{
			string name = ExpectIdentifier();
			ns = GetOrCreateNamedNamespace(cur, name, inl);
			Expect("OP_LBRACE");
		}
		NamespaceEntity* prev = cur;
		cur = ns.get();
		while (!Eat("OP_RBRACE")) ParseDeclaration();
		cur = prev;
	}

	void ParseNamespaceAliasDefinition()
	{
		Expect("KW_NAMESPACE");
		string alias = ExpectIdentifier();
		Expect("OP_ASS");

		bool absolute = false;
		vector<string> path;
		if (Eat("OP_COLON2")) absolute = true;
		path.push_back(ExpectIdentifier());
		while (Eat("OP_COLON2")) path.push_back(ExpectIdentifier());

		Expect("OP_SEMICOLON");
		shared_ptr<NamespaceEntity> target = ResolveNamespacePath(absolute, path);
		cur->namespace_aliases[alias] = target;
	}

	void ParseUsingDirective()
	{
		Expect("KW_USING");
		Expect("KW_NAMESPACE");
		bool absolute = false;
		vector<string> path;
		if (Eat("OP_COLON2")) absolute = true;
		path.push_back(ExpectIdentifier());
		while (Eat("OP_COLON2")) path.push_back(ExpectIdentifier());
		Expect("OP_SEMICOLON");
		shared_ptr<NamespaceEntity> target = ResolveNamespacePath(absolute, path);
		if (find(cur->using_directives.begin(), cur->using_directives.end(), target) == cur->using_directives.end())
		{
			cur->using_directives.push_back(target);
		}
	}

	void ParseUsingDeclaration()
	{
		Expect("KW_USING");
		bool absolute = false;
		vector<string> path;
		if (Eat("OP_COLON2"))
		{
			absolute = true;
			string first = ExpectIdentifier();
			if (Eat("OP_COLON2"))
			{
				path.push_back(first);
				while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
				{
					path.push_back(ExpectIdentifier());
					Expect("OP_COLON2");
				}
				first = ExpectIdentifier();
			}
			Expect("OP_SEMICOLON");
			shared_ptr<NamespaceEntity> ns = ResolveNamespacePath(absolute, path);
			shared_ptr<Type> out;
			unordered_set<NamespaceEntity*> vis;
			LookupTypeInNamespaceDFS(ns.get(), first, vis, out);
			if (out) cur->using_types[first] = out;
			return;
		}

		string first = ExpectIdentifier();
		if (!Eat("OP_COLON2")) throw runtime_error("invalid using declaration");
		path.push_back(first);
		while (Is("TT_IDENTIFIER") && Is("OP_COLON2", 1))
		{
			path.push_back(ExpectIdentifier());
			Expect("OP_COLON2");
		}
		string name = ExpectIdentifier();
		Expect("OP_SEMICOLON");
		shared_ptr<NamespaceEntity> ns = ResolveNamespacePath(absolute, path);

		shared_ptr<Type> out;
		unordered_set<NamespaceEntity*> vis;
		LookupTypeInNamespaceDFS(ns.get(), name, vis, out);
		if (out) cur->using_types[name] = out;
	}

	void ParseDeclaration()
	{
		if (Eat("OP_SEMICOLON")) return;
		if (Is("KW_INLINE") || Is("KW_NAMESPACE"))
		{
			if (Is("KW_INLINE") || (Is("KW_NAMESPACE") && Is("TT_IDENTIFIER", 1) && Is("OP_ASS", 2)))
			{
				if (Is("KW_NAMESPACE") && Is("TT_IDENTIFIER", 1) && Is("OP_ASS", 2)) ParseNamespaceAliasDefinition();
				else ParseNamespaceDefinition();
				return;
			}
			if (Is("KW_NAMESPACE")) { ParseNamespaceDefinition(); return; }
		}
		if (Is("KW_USING") && Is("KW_NAMESPACE", 1)) { ParseUsingDirective(); return; }
		if (Is("KW_USING") && Is("TT_IDENTIFIER", 1) && Is("OP_ASS", 2)) { ParseAliasDeclaration(); return; }
		if (Is("KW_USING")) { ParseUsingDeclaration(); return; }
		ParseSimpleDeclaration();
	}

	void ParseTranslationUnit()
	{
		while (!Eat("ST_EOF")) ParseDeclaration();
	}
};

static void DumpNamespace(ostream& out, const NamespaceEntity* ns)
{
	if (ns->named) out << "start namespace " << ns->name << "\n";
	else out << "start unnamed namespace\n";
	if (ns->is_inline) out << "inline namespace\n";

	for (const auto& v : ns->var_order) out << "variable " << v->name << " " << TypeToString(v->type) << "\n";
	for (const auto& f : ns->fn_order) out << "function " << f->name << " " << TypeToString(f->type) << "\n";
	for (const auto& c : ns->ns_order) DumpNamespace(out, c.get());

	out << "end namespace\n";
}

static void ProcessOne(ostream& out, const string& srcfile)
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
	vector<NsToken> toks = BuildTokens(pre);

	shared_ptr<NamespaceEntity> global_ns(new NamespaceEntity());
	global_ns->named = false;
	global_ns->parent = nullptr;

	Parser parser(std::move(toks), global_ns);
	parser.ParseTranslationUnit();

	out << "start translation unit " << srcfile << "\n";
	DumpNamespace(out, global_ns.get());
	out << "end translation unit\n";
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
		out << nsrcfiles << " translation units\n";

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			ProcessOne(out, srcfile);
		}
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
