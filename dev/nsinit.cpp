// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cstdlib>
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
struct ProgramEntry;

struct VariableEntity
{
	string name;
	shared_ptr<Type> type;
	bool is_constexpr = false;
	bool has_init = false;
	string init_literal;
	bool init_is_id = false;
	string init_id;
};

struct InitExpr
{
	bool present = false;
	bool is_literal = false;
	string lit;
	bool is_id = false;
	string id;
};

struct FunctionEntity
{
	string name;
	shared_ptr<Type> type;
	bool is_defined = false;
	bool inline_def = false;
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
	vector<shared_ptr<ProgramEntry> > program_order;
	vector<string> string_literals;
};

struct ProgramEntry
{
	bool is_function = false;
	shared_ptr<VariableEntity> var;
	shared_ptr<FunctionEntity> fn;
};

static shared_ptr<Type> CloneType(const shared_ptr<Type>& t);
static bool IsConstObjectType(const shared_ptr<Type>& t);

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

	// PA8 function bodies are restricted to `{}`. Convert them to declarations
	// so the PA7-style declarator parser can continue to parse signatures.
	for (size_t i = 1; i + 2 < out.size(); i++)
	{
		if (out[i - 1].term == "OP_RPAREN" && out[i].term == "OP_LBRACE" && out[i + 1].term == "OP_RBRACE")
		{
			out[i].term = "OP_SEMICOLON";
			out[i].text = "{def}";
			out.erase(out.begin() + static_cast<long long>(i + 1));
		}
	}
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
	int tu_index = 0;

	Parser(vector<NsToken> v, const shared_ptr<NamespaceEntity>& g, int tu) : toks(std::move(v)), global_ns(g), cur(g.get()), tu_index(tu) {}

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
			"KW_TYPEDEF", "KW_CONSTEXPR", "KW_INLINE", "KW_STATIC", "KW_THREAD_LOCAL", "KW_EXTERN",
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
			if (set_inline && !it->second->is_inline) throw runtime_error("namespace inline mismatch");
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

	bool HasFunctionNamed(NamespaceEntity* ns, const string& name) const
	{
		string pref = name + "|";
		for (const auto& kv : ns->fns) if (kv.first.rfind(pref, 0) == 0) return true;
		return false;
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

	shared_ptr<Type> ParseDeclSpecifierSeq(bool& is_typedef, bool& is_extern, bool& is_static, bool& is_inline_spec, bool& is_constexpr_spec)
	{
		is_typedef = false;
		is_extern = false;
		is_static = false;
		is_inline_spec = false;
		is_constexpr_spec = false;
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
			if (Eat("KW_CONSTEXPR")) { is_constexpr_spec = true; continue; }
			if (Eat("KW_INLINE")) { is_inline_spec = true; continue; }
			if (Eat("KW_EXTERN")) { is_extern = true; continue; }
			if (Eat("KW_STATIC")) { is_static = true; continue; }
			if (Eat("KW_THREAD_LOCAL")) continue;
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
		bool is_static = false;
		bool is_inline_spec = false;
		bool is_constexpr_spec = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(is_typedef, is_extern, is_static, is_inline_spec, is_constexpr_spec);
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
		int nrefs = 0;
		for (const PtrOp& op : d.ptrs) if (op.kind != PtrOp::PTR) nrefs++;
		if (nrefs > 1) throw runtime_error("invalid multiple reference declarator");
		bool saw_ref = false;
		for (const PtrOp& op : d.ptrs)
		{
			if (op.kind != PtrOp::PTR) saw_ref = true;
			else if (saw_ref) throw runtime_error("invalid pointer-to-reference declarator");
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
					long long v = 0;
					if (Is("TT_LITERAL"))
					{
						string lit = toks[p++].text;
						v = stoll(lit, nullptr, 0);
					}
					else if (Is("TT_IDENTIFIER"))
					{
						string name = ExpectIdentifier();
						unsigned long long uv = 0;
						bool ok = false;
						if (d.pname.has_name && !d.pname.qualifiers.empty())
						{
							shared_ptr<NamespaceEntity> ns = ResolveNamespacePath(d.pname.absolute, d.pname.qualifiers);
							ok = TryResolveConstIntInScope(ns.get(), name, uv);
						}
						if (!ok) ok = TryResolveConstIntInScope(cur, name, uv);
						if (!ok) throw runtime_error("expected constant bound");
						v = static_cast<long long>(uv);
					}
					else throw runtime_error("expected array bound");
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

	void ValidateType(const shared_ptr<Type>& t)
	{
		if (!t) return;
		if (t->kind == TY_PTR)
		{
			if (t->sub && (t->sub->kind == TY_LREF || t->sub->kind == TY_RREF)) throw runtime_error("pointer to reference is invalid");
			ValidateType(t->sub);
			return;
		}
		if (t->kind == TY_LREF || t->kind == TY_RREF)
		{
			shared_ptr<Type> u = t->sub;
			while (u && u->kind == TY_CV) u = u->sub;
			if (u && u->kind == TY_FUND && u->fund == "void") throw runtime_error("reference to void is invalid");
			ValidateType(t->sub);
			return;
		}
		if (t->kind == TY_CV || t->kind == TY_ARRAY) { ValidateType(t->sub); return; }
		if (t->kind == TY_FUNCTION)
		{
			ValidateType(t->sub);
			for (const auto& p : t->params) ValidateType(p);
		}
	}

	pair<NamespaceEntity*, string> ResolveDeclaratorTarget(const ParsedName& pname)
	{
		if (!pname.has_name) throw runtime_error("missing name");
		if (pname.qualifiers.empty()) return make_pair(cur, pname.name);
		shared_ptr<NamespaceEntity> ns = ResolveNamespacePath(pname.absolute, pname.qualifiers);
		return make_pair(ns.get(), pname.name);
	}

	bool Encloses(NamespaceEntity* maybe_parent, NamespaceEntity* target) const
	{
		for (NamespaceEntity* pns = target; pns; pns = pns->parent)
		{
			if (pns == maybe_parent) return true;
		}
		return false;
	}

	void DeclareTypedef(const ParsedName& pname, const shared_ptr<Type>& t)
	{
		if (!pname.qualifiers.empty()) throw runtime_error("qualified typedef");
		cur->typedefs[pname.name] = CloneType(t);
	}

	bool InUnnamedNamespace(NamespaceEntity* ns) const
	{
		for (NamespaceEntity* pns = ns; pns; pns = pns->parent)
		{
			if (pns == global_ns.get()) continue;
			if (!pns->named) return true;
		}
		return false;
	}

	void DeclareVariable(NamespaceEntity* ns, const string& name, const shared_ptr<Type>& t, bool is_extern, bool internal_linkage)
	{
		(void)is_extern;
		string key = internal_linkage ? (name + "#tu" + to_string(tu_index)) : name;
		auto it = ns->vars.find(key);
		if (it == ns->vars.end())
		{
			shared_ptr<VariableEntity> v(new VariableEntity());
			v->name = name;
			v->type = CloneType(t);
			ns->vars[key] = v;
			ns->var_order.push_back(v);
			shared_ptr<ProgramEntry> pe(new ProgramEntry());
			pe->is_function = false;
			pe->var = v;
			global_ns->program_order.push_back(pe);
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

	void DeclareFunction(NamespaceEntity* ns, const string& name, const shared_ptr<Type>& t, bool internal_linkage, bool is_definition, bool is_inline_spec)
	{
		string sig = TypeToString(t);
		string base_key = name + "|" + sig;
		string key = internal_linkage ? (base_key + "#tu" + to_string(tu_index)) : base_key;
		auto it = ns->fns.find(key);
		if (it == ns->fns.end())
		{
			shared_ptr<FunctionEntity> f(new FunctionEntity());
			f->name = name;
			f->type = CloneType(t);
			f->is_defined = is_definition;
			f->inline_def = is_inline_spec;
			ns->fns[key] = f;
			ns->fn_order.push_back(f);
			shared_ptr<ProgramEntry> pe(new ProgramEntry());
			pe->is_function = true;
			pe->fn = f;
			global_ns->program_order.push_back(pe);
		}
		else if (is_definition)
		{
			if (it->second->is_defined && !(it->second->inline_def && is_inline_spec)) throw runtime_error("multiple function definitions");
			it->second->is_defined = true;
			it->second->inline_def = it->second->inline_def || is_inline_spec;
		}
	}

	void ParseSimpleDeclaration()
	{
		bool is_typedef = false;
		bool is_extern = false;
		bool is_static = false;
		bool is_inline_spec = false;
		bool is_constexpr_spec = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(is_typedef, is_extern, is_static, is_inline_spec, is_constexpr_spec);

		struct InitDecl { Declarator d; InitExpr init; };
		vector<InitDecl> ds;
		InitDecl first;
		first.d = ParseDeclarator(false);
			if (Eat("OP_ASS"))
			{
				first.init.present = true;
			if (Is("OP_LPAREN"))
			{
				Expect("OP_LPAREN");
				first.init = first.init;
				if (Is("TT_LITERAL"))
				{
					first.init.is_literal = true;
					first.init.lit = toks[p++].text;
				}
				Expect("OP_RPAREN");
			}
				else if (Is("TT_LITERAL"))
				{
					first.init.is_literal = true;
					first.init.lit = toks[p++].text;
				}
				else if (Is("TT_IDENTIFIER"))
				{
					first.init.is_id = true;
					first.init.id = ExpectIdentifier();
				}
				else
			{
				int depth = 0;
				while (!(depth == 0 && (Is("OP_COMMA") || Is("OP_SEMICOLON"))))
				{
					if (Eat("OP_LPAREN") || Eat("OP_LSQUARE") || Eat("OP_LBRACE")) { depth++; continue; }
					if (Eat("OP_RPAREN") || Eat("OP_RSQUARE") || Eat("OP_RBRACE")) { if (depth > 0) depth--; continue; }
					p++;
				}
			}
		}
		ds.push_back(first);
		while (Eat("OP_COMMA"))
		{
			InitDecl x;
			x.d = ParseDeclarator(false);
				if (Eat("OP_ASS"))
				{
					x.init.present = true;
				if (Is("OP_LPAREN"))
				{
					Expect("OP_LPAREN");
					if (Is("TT_LITERAL"))
					{
						x.init.is_literal = true;
						x.init.lit = toks[p++].text;
					}
					Expect("OP_RPAREN");
				}
					else if (Is("TT_LITERAL"))
					{
						x.init.is_literal = true;
						x.init.lit = toks[p++].text;
					}
					else if (Is("TT_IDENTIFIER"))
					{
						x.init.is_id = true;
						x.init.id = ExpectIdentifier();
					}
					else
				{
					int depth = 0;
					while (!(depth == 0 && (Is("OP_COMMA") || Is("OP_SEMICOLON"))))
					{
						if (Eat("OP_LPAREN") || Eat("OP_LSQUARE") || Eat("OP_LBRACE")) { depth++; continue; }
						if (Eat("OP_RPAREN") || Eat("OP_RSQUARE") || Eat("OP_RBRACE")) { if (depth > 0) depth--; continue; }
						p++;
					}
				}
			}
			ds.push_back(x);
		}
		bool is_funcdef_stmt = Is("OP_SEMICOLON") && Peek().text == "{def}";
		Expect("OP_SEMICOLON");

		for (const InitDecl& idecl : ds)
		{
			shared_ptr<Type> t = ApplyDeclarator(base, idecl.d);
			ValidateType(t);
			ParsedName name = idecl.d.pname;
			if (!name.has_name && idecl.d.inner) name = idecl.d.inner->pname;
			if (is_typedef)
			{
				DeclareTypedef(name, t);
				continue;
			}
			auto trg = ResolveDeclaratorTarget(name);
			if (!name.qualifiers.empty() && !Encloses(cur, trg.first)) throw runtime_error("qualified declaration in non-enclosing namespace");
			bool internal_linkage = is_static || InUnnamedNamespace(trg.first);
			if (t->kind == TY_FUNCTION) DeclareFunction(trg.first, trg.second, t, internal_linkage, is_funcdef_stmt, is_inline_spec);
			else
			{
				shared_ptr<Type> ut = t;
				while (ut && ut->kind == TY_CV) ut = ut->sub;
				if (ut && ut->kind == TY_FUND && ut->fund == "void") throw runtime_error("object of type void is invalid");
				if (!idecl.init.present && (t->kind == TY_LREF || t->kind == TY_RREF)) throw runtime_error("reference requires initializer");
				if (!idecl.init.present && !is_extern && IsConstObjectType(t)) throw runtime_error("const object requires initializer");
				DeclareVariable(trg.first, trg.second, t, is_extern, internal_linkage);
				string key = internal_linkage ? (trg.second + "#tu" + to_string(tu_index)) : trg.second;
				auto itv = trg.first->vars.find(key);
				if (itv != trg.first->vars.end() && idecl.init.present && idecl.init.is_literal)
				{
					itv->second->is_constexpr = is_constexpr_spec;
					itv->second->has_init = true;
					itv->second->init_literal = idecl.init.lit;
					itv->second->init_is_id = false;
					itv->second->init_id.clear();
					if (idecl.init.lit.find('"') != string::npos) global_ns->string_literals.push_back(idecl.init.lit);
				}
				if (itv != trg.first->vars.end() && idecl.init.present && idecl.init.is_id)
				{
					itv->second->is_constexpr = is_constexpr_spec;
					itv->second->has_init = true;
					bool keep_as_id = true;
					shared_ptr<Type> ut = t;
					while (ut && ut->kind == TY_CV) ut = ut->sub;
					if (ut && ut->kind == TY_FUND)
					{
						const string& f = ut->fund;
						bool integral_fund = (f == "bool" || f == "char" || f == "signed char" || f == "unsigned char" || f == "wchar_t" ||
							f == "char16_t" || f == "char32_t" || f == "short int" || f == "unsigned short int" ||
							f == "int" || f == "unsigned int" || f == "long int" || f == "unsigned long int" ||
							f == "long long int" || f == "unsigned long long int");
						if (integral_fund)
						{
							unsigned long long cv = 0;
							bool ok = TryResolveConstIntInScope(trg.first, idecl.init.id, cv);
							if (!ok) ok = TryResolveConstIntInScope(cur, idecl.init.id, cv);
							if (ok)
							{
								itv->second->init_literal = to_string(static_cast<long long>(cv));
								itv->second->init_is_id = false;
								itv->second->init_id.clear();
								keep_as_id = false;
							}
						}
					}
					if (keep_as_id)
					{
						itv->second->init_is_id = true;
						itv->second->init_id = idecl.init.id;
					}
				}
			}
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
		bool st = false;
		bool inl = false;
		bool cexpr = false;
		shared_ptr<Type> base = ParseDeclSpecifierSeq(td, ex, st, inl, cexpr);
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
			if (cur->namespace_aliases.find(name) != cur->namespace_aliases.end()) throw runtime_error("namespace alias misuse");
			if (cur->typedefs.find(name) != cur->typedefs.end()) throw runtime_error("namespace/type conflict");
			if (cur->using_types.find(name) != cur->using_types.end()) throw runtime_error("namespace/type conflict");
			if (cur->vars.find(name) != cur->vars.end()) throw runtime_error("namespace/var conflict");
			if (HasFunctionNamed(cur, name)) throw runtime_error("namespace/function conflict");
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
		if (!absolute && path.size() == 1 && path[0] == alias) throw runtime_error("namespace alias to self");
		if (cur->named_children.find(alias) != cur->named_children.end()) throw runtime_error("namespace alias conflict");

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
				if (LookupNamespaceDirect(ns.get(), first)) throw runtime_error("using declaration to namespace");
				shared_ptr<Type> out;
				unordered_set<NamespaceEntity*> vis;
				LookupTypeInNamespaceDFS(ns.get(), first, vis, out);
				if (!out) throw runtime_error("invalid using declaration target");
				cur->using_types[first] = out;
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
		if (LookupNamespaceDirect(ns.get(), name)) throw runtime_error("using declaration to namespace");

		shared_ptr<Type> out;
		unordered_set<NamespaceEntity*> vis;
		LookupTypeInNamespaceDFS(ns.get(), name, vis, out);
		if (!out) throw runtime_error("invalid using declaration target");
		cur->using_types[name] = out;
	}

	shared_ptr<VariableEntity> LookupVariableSimple(const string& name)
	{
		for (NamespaceEntity* s = cur; s; s = s->parent)
		{
			for (const auto& kv : s->vars)
			{
				if (kv.second->name == name) return kv.second;
			}
		}
		return shared_ptr<VariableEntity>();
	}

	shared_ptr<VariableEntity> LookupVariableInScopeChain(NamespaceEntity* scope, const string& name)
	{
		for (NamespaceEntity* s = scope; s; s = s->parent)
		{
			for (const auto& kv : s->vars)
			{
				if (kv.second->name == name) return kv.second;
			}
		}
		return shared_ptr<VariableEntity>();
	}

	bool EvalConstIntVar(const shared_ptr<VariableEntity>& v, NamespaceEntity* lookup_scope, unsigned long long& out, unordered_set<const VariableEntity*>& seen)
	{
		if (!v) return false;
		if (seen.count(v.get())) return false;
		seen.insert(v.get());
		shared_ptr<Type> vt = v->type;
		while (vt && vt->kind == TY_CV) vt = vt->sub;
		bool is_ref = vt && (vt->kind == TY_LREF || vt->kind == TY_RREF);
		if (!(v->is_constexpr || IsConstObjectType(v->type) || is_ref)) return false;
		if (!v->init_literal.empty())
		{
			ParsedInteger pi = ParseIntegerLiteral(v->init_literal);
			if (!pi.ok) return false;
			out = pi.value;
			return true;
		}
		if (v->init_is_id)
		{
			shared_ptr<VariableEntity> w = LookupVariableInScopeChain(lookup_scope, v->init_id);
			if (!w) w = LookupVariableSimple(v->init_id);
			if (!w) return false;
			return EvalConstIntVar(w, lookup_scope, out, seen);
		}
		return false;
	}

	bool TryResolveConstIntInScope(NamespaceEntity* scope, const string& name, unsigned long long& out)
	{
		shared_ptr<VariableEntity> v = LookupVariableInScopeChain(scope, name);
		if (v)
		{
			unordered_set<const VariableEntity*> seen;
			if (EvalConstIntVar(v, scope, out, seen)) return true;
		}
		return false;
	}

	bool EvalStaticAssertExpr()
	{
		if (Eat("OP_LPAREN"))
		{
			bool v = EvalStaticAssertExpr();
			Expect("OP_RPAREN");
			return v;
		}
		if (Eat("KW_TRUE")) return true;
		if (Eat("KW_FALSE")) return false;
		if (Eat("KW_NULLPTR")) return false;
		if (Is("TT_LITERAL"))
		{
			string lit = toks[p++].text;
			ParsedInteger pi = ParseIntegerLiteral(lit);
			if (pi.ok) return pi.value != 0;
			return false;
		}
		if (Is("TT_IDENTIFIER"))
		{
			string name = ExpectIdentifier();
			shared_ptr<VariableEntity> v = LookupVariableSimple(name);
			if (!v) return false;
			if (!(v->is_constexpr || IsConstObjectType(v->type))) return false;
			if (v->init_is_id) return true;
			if (!v->init_literal.empty())
			{
				ParsedInteger pi = ParseIntegerLiteral(v->init_literal);
				if (pi.ok) return pi.value != 0;
				return true;
			}
		}
		throw runtime_error("bad static_assert expression");
	}

	void ParseStaticAssertDeclaration()
	{
		Expect("KW_STATIC_ASSERT");
		Expect("OP_LPAREN");
		bool cond = EvalStaticAssertExpr();
		Expect("OP_COMMA");
		if (!Is("TT_LITERAL")) throw runtime_error("bad static_assert message");
		p++;
		Expect("OP_RPAREN");
		Expect("OP_SEMICOLON");
		if (!cond) throw runtime_error("static_assert failed");
	}

		void ParseDeclaration()
		{
			if (Eat("OP_SEMICOLON")) return;
			if (Is("KW_NAMESPACE") || (Is("KW_INLINE") && Is("KW_NAMESPACE", 1)))
			{
				if ((Is("KW_INLINE") && Is("KW_NAMESPACE", 1)) || (Is("KW_NAMESPACE") && Is("TT_IDENTIFIER", 1) && Is("OP_ASS", 2)))
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
		if (Is("KW_STATIC_ASSERT")) { ParseStaticAssertDeclaration(); return; }
		ParseSimpleDeclaration();
	}

	void ParseTranslationUnit()
	{
		while (!Eat("ST_EOF")) ParseDeclaration();
	}
};

static void ProcessOne(const string& srcfile, const shared_ptr<NamespaceEntity>& global_ns, int tu_index)
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

	// Unnamed namespaces are TU-local in PA8.
	global_ns->unique_unnamed.reset();
	Parser parser(std::move(toks), global_ns, tu_index);
	parser.ParseTranslationUnit();
}

static void CollectEntities(const shared_ptr<NamespaceEntity>& ns, vector<shared_ptr<VariableEntity> >& vars, vector<shared_ptr<FunctionEntity> >& fns)
{
	for (const auto& v : ns->var_order) vars.push_back(v);
	for (const auto& f : ns->fn_order) fns.push_back(f);
	for (const auto& c : ns->ns_order) CollectEntities(c, vars, fns);
}

static size_t AlignTo(size_t x, size_t a)
{
	if (a == 0) return x;
	return (x + a - 1) / a * a;
}

static pair<size_t, size_t> SizeAlignOf(const shared_ptr<Type>& t)
{
	if (t->kind == TY_CV) return SizeAlignOf(t->sub);
	if (t->kind == TY_PTR || t->kind == TY_LREF || t->kind == TY_RREF) return make_pair(8u, 8u);
	if (t->kind == TY_ARRAY)
	{
		pair<size_t, size_t> sa = SizeAlignOf(t->sub);
		if (t->array_bound < 0) return make_pair(0u, sa.second);
		return make_pair(sa.first * static_cast<size_t>(t->array_bound), sa.second);
	}
	if (t->kind == TY_FUNCTION) return make_pair(4u, 4u);
	if (t->kind == TY_FUND)
	{
		const string& f = t->fund;
		if (f == "char" || f == "signed char" || f == "unsigned char" || f == "bool") return make_pair(1u, 1u);
		if (f == "short int" || f == "unsigned short int" || f == "char16_t") return make_pair(2u, 2u);
		if (f == "int" || f == "unsigned int" || f == "wchar_t" || f == "char32_t" || f == "float") return make_pair(4u, 4u);
		if (f == "long int" || f == "long long int" || f == "unsigned long int" || f == "unsigned long long int" || f == "double") return make_pair(8u, 8u);
		if (f == "long double") return make_pair(16u, 16u);
		if (f == "void") return make_pair(0u, 1u);
	}
	return make_pair(0u, 1u);
}

static void WriteLE(vector<char>& img, size_t off, unsigned long long v, size_t n)
{
	for (size_t i = 0; i < n; i++) img[off + i] = static_cast<char>((v >> (8 * i)) & 0xffu);
}

static unsigned long long ReadLE(const vector<char>& img, size_t off, size_t n)
{
	unsigned long long v = 0;
	for (size_t i = 0; i < n && (off + i) < img.size() && i < 8; i++)
	{
		v |= (static_cast<unsigned long long>(static_cast<unsigned char>(img[off + i])) << (8 * i));
	}
	return v;
}

static void WriteBytes(vector<char>& img, size_t off, const vector<unsigned char>& b, size_t n)
{
	for (size_t i = 0; i < n && i < b.size(); i++) img[off + i] = static_cast<char>(b[i]);
}

static shared_ptr<Type> StripCV(const shared_ptr<Type>& t)
{
	shared_ptr<Type> x = t;
	while (x && x->kind == TY_CV) x = x->sub;
	return x;
}

static bool IsConstObjectType(const shared_ptr<Type>& t)
{
	if (!t) return false;
	if (t->kind == TY_CV) return t->is_const || IsConstObjectType(t->sub);
	if (t->kind == TY_ARRAY) return IsConstObjectType(t->sub);
	return false;
}

static vector<unsigned char> EncodeStringLiteral(const string& lit)
{
	PPToken pt;
	pt.type = "string-literal";
	pt.data = lit;
	ParsedStringPiece ps = ParseStringPiece(pt);
	vector<unsigned char> out;
	if (!ps.ok) return out;
	auto push16 = [&](unsigned int v) { out.push_back(static_cast<unsigned char>(v & 0xffu)); out.push_back(static_cast<unsigned char>((v >> 8) & 0xffu)); };
	auto push32 = [&](unsigned int v) {
		out.push_back(static_cast<unsigned char>(v & 0xffu));
		out.push_back(static_cast<unsigned char>((v >> 8) & 0xffu));
		out.push_back(static_cast<unsigned char>((v >> 16) & 0xffu));
		out.push_back(static_cast<unsigned char>((v >> 24) & 0xffu));
	};
	if (ps.prefix == "u")
	{
		for (int cp : ps.cps) push16(static_cast<unsigned int>(cp));
		push16(0);
		return out;
	}
	if (ps.prefix == "U" || ps.prefix == "L")
	{
		for (int cp : ps.cps) push32(static_cast<unsigned int>(cp));
		push32(0);
		return out;
	}
	for (int cp : ps.cps) out.push_back(static_cast<unsigned char>(cp & 0xff));
	out.push_back(0);
	return out;
}

static size_t StringLiteralAlign(const string& lit)
{
	if (lit.rfind("u8", 0) == 0) return 1;
	if (!lit.empty() && lit[0] == 'u') return 2;
	if (!lit.empty() && (lit[0] == 'U' || lit[0] == 'L')) return 4;
	return 1;
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
		shared_ptr<NamespaceEntity> global_ns(new NamespaceEntity());
		global_ns->named = false;
		global_ns->parent = nullptr;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			ProcessOne(srcfile, global_ns, static_cast<int>(i));
		}

		vector<char> img;
		img.push_back('P');
		img.push_back('A');
		img.push_back('8');
		img.push_back('\0');
		unordered_map<string, size_t> fn_offsets;
		unordered_map<string, size_t> var_offsets;
		unordered_map<string, shared_ptr<VariableEntity> > var_entities;
		struct PendingTemp { size_t ptr_off = 0; size_t align = 1; vector<char> bytes; };
		vector<PendingTemp> pending_temps;
		for (const auto& e : global_ns->program_order)
		{
			if (!e->is_function)
			{
				const auto& v = e->var;
				shared_ptr<Type> vt = StripCV(v->type);
				pair<size_t, size_t> sa = SizeAlignOf(v->type);
				size_t old = img.size();
				size_t at = AlignTo(old, sa.second);
				img.resize(at, '\0');
				size_t obj_size = sa.first;
				vector<unsigned char> str_init;
				if (v->has_init && vt && vt->kind == TY_ARRAY && v->init_literal.find('"') != string::npos)
				{
					str_init = EncodeStringLiteral(v->init_literal);
					shared_ptr<Type> et = StripCV(vt->sub);
					pair<size_t, size_t> esa = SizeAlignOf(et);
					if (vt->array_bound < 0 && esa.first > 0) obj_size = str_init.size();
				}
				img.resize(at + obj_size, '\0');
				var_offsets[v->name] = at;
				var_entities[v->name] = v;
				if (v->has_init && obj_size > 0 && (v->type->kind == TY_FUND || (v->type->kind == TY_CV && v->type->sub->kind == TY_FUND)))
				{
					string f = (v->type->kind == TY_FUND) ? v->type->fund : v->type->sub->fund;
					const string& lit = v->init_literal;
					if (lit.find('\'') != string::npos)
					{
						PPToken pt;
						pt.type = "character-literal";
						pt.data = lit;
						ParsedChar pc = ParseCharacterLiteral(pt);
						if (pc.ok) WriteBytes(img, at, pc.bytes, sa.first);
					}
					else if (lit.find('.') != string::npos || lit.find('e') != string::npos || lit.find('E') != string::npos)
					{
						bool ud = false; string uds; string pref; EFundamentalType ty = FT_DOUBLE;
						if (ParseFloatingLiteral(lit, ud, uds, pref, ty))
						{
							if (f == "float")
							{
								float fv = strtof(pref.c_str(), nullptr);
								unsigned char* p = reinterpret_cast<unsigned char*>(&fv);
								vector<unsigned char> b(p, p + sizeof(fv));
								WriteBytes(img, at, b, sa.first);
							}
							else
							{
								double dv = strtod(pref.c_str(), nullptr);
								unsigned char* p = reinterpret_cast<unsigned char*>(&dv);
								vector<unsigned char> b(p, p + sizeof(dv));
								WriteBytes(img, at, b, sa.first);
							}
						}
					}
					else
					{
						ParsedInteger pi = ParseIntegerLiteral(lit);
						unsigned long long iv = pi.ok ? pi.value : 0;
						WriteLE(img, at, iv, sa.first);
					}
				}
				if (v->has_init && vt && vt->kind == TY_ARRAY && !str_init.empty())
				{
					for (size_t i = 0; i < str_init.size() && i < obj_size; i++) img[at + i] = static_cast<char>(str_init[i]);
				}
				if (v->has_init && v->init_is_id && vt && vt->kind == TY_PTR && vt->sub && vt->sub->kind == TY_FUNCTION && obj_size >= 8)
				{
					auto itf = fn_offsets.find(v->init_id);
					unsigned long long offv = (itf == fn_offsets.end()) ? 0 : static_cast<unsigned long long>(itf->second);
					WriteLE(img, at, offv, 8);
				}
				if (v->has_init && v->init_is_id && vt && vt->kind == TY_PTR && (!vt->sub || vt->sub->kind != TY_FUNCTION) && obj_size >= 8)
				{
					auto itv = var_offsets.find(v->init_id);
					unsigned long long offv = (itv == var_offsets.end()) ? 0 : static_cast<unsigned long long>(itv->second);
					WriteLE(img, at, offv, 8);
				}
				if (v->has_init && v->init_is_id && vt && (vt->kind == TY_LREF || vt->kind == TY_RREF) && obj_size >= 8)
				{
					unsigned long long offv = 0;
					auto itv = var_offsets.find(v->init_id);
					if (itv != var_offsets.end())
					{
						offv = static_cast<unsigned long long>(itv->second);
						auto ite = var_entities.find(v->init_id);
						if (ite != var_entities.end())
						{
							shared_ptr<Type> it = StripCV(ite->second->type);
							if (it && (it->kind == TY_LREF || it->kind == TY_RREF)) offv = ReadLE(img, itv->second, 8);
						}
					}
					WriteLE(img, at, offv, 8);
				}
				if (v->has_init && vt && (vt->kind == TY_LREF || vt->kind == TY_RREF) && !v->init_literal.empty() && obj_size >= 8)
				{
					shared_ptr<Type> rt = StripCV(vt->sub);
					pair<size_t, size_t> rsa = SizeAlignOf(rt);
					if (rsa.first > 0)
					{
						PendingTemp pt;
						pt.ptr_off = at;
						pt.align = rsa.second;
						pt.bytes.assign(rsa.first, 0);
						ParsedInteger pi = ParseIntegerLiteral(v->init_literal);
						unsigned long long iv = pi.ok ? pi.value : 0;
						for (size_t bi = 0; bi < rsa.first && bi < 8; bi++) pt.bytes[bi] = static_cast<char>((iv >> (8 * bi)) & 0xffu);
						pending_temps.push_back(pt);
					}
				}
			}
			else
			{
				size_t foff = img.size();
				img.push_back('f');
				img.push_back('u');
				img.push_back('n');
				img.push_back('\0');
				fn_offsets[e->fn->name] = foff;
			}
		}
		for (PendingTemp& pt : pending_temps)
		{
			size_t off = AlignTo(img.size(), pt.align);
			img.resize(off, '\0');
			img.insert(img.end(), pt.bytes.begin(), pt.bytes.end());
			WriteLE(img, pt.ptr_off, static_cast<unsigned long long>(off), 8);
		}
		for (const string& lit : global_ns->string_literals)
		{
			size_t al = StringLiteralAlign(lit);
			img.resize(AlignTo(img.size(), al), '\0');
			vector<unsigned char> b = EncodeStringLiteral(lit);
			for (unsigned char c : b) img.push_back(static_cast<char>(c));
		}

		ofstream out(outfile, ios::binary);
		out.write(img.data(), static_cast<streamsize>(img.size()));
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
