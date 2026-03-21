// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cctype>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

#define CPPGM_PREPROC_LIBRARY
#include "preproc.cpp"

struct Type;
struct Namespace;
struct VariableEntity;
struct FunctionEntity;

typedef shared_ptr<Type> TypePtr;
typedef function<TypePtr(TypePtr)> TypeBuilder;

struct Token
{
	bool is_identifier = false;
	bool is_literal = false;
	bool is_eof = false;
	string spelling;
	string simple;
	string line;
};

struct QualifiedName
{
	bool global = false;
	vector<string> qualifiers;
	string name;
};

struct Type
{
	enum Kind
	{
		TK_FUNDAMENTAL,
		TK_CV,
		TK_POINTER,
		TK_LVALUE_REF,
		TK_RVALUE_REF,
		TK_ARRAY,
		TK_FUNCTION
	} kind = TK_FUNDAMENTAL;

	string fundamental;
	bool is_const = false;
	bool is_volatile = false;
	TypePtr nested;
	bool has_bound = false;
	unsigned long long bound = 0;
	vector<TypePtr> params;
	bool variadic = false;
};

struct VariableEntity
{
	string name;
	TypePtr type;
};

struct FunctionEntity
{
	string name;
	TypePtr type;
};

struct Namespace
{
	string name;
	bool is_named = false;
	bool is_inline = false;
	Namespace* parent = nullptr;

	vector<shared_ptr<VariableEntity> > variables_in_order;
	vector<shared_ptr<FunctionEntity> > functions_in_order;
	vector<shared_ptr<Namespace> > namespaces_in_order;

	map<string, shared_ptr<VariableEntity> > variables;
	map<string, shared_ptr<FunctionEntity> > functions;
	map<string, TypePtr> types;
	map<string, shared_ptr<Namespace> > named_namespaces;
	map<string, shared_ptr<Namespace> > namespace_aliases;
	map<string, shared_ptr<VariableEntity> > variable_aliases;
	map<string, shared_ptr<FunctionEntity> > function_aliases;
	shared_ptr<Namespace> unnamed_namespace;

	vector<shared_ptr<Namespace> > using_directives;
};

TypeBuilder IdentityBuilder()
{
	return [](TypePtr base) { return base; };
}

TypePtr MakeFundamental(const string& name)
{
	TypePtr out(new Type());
	out->kind = Type::TK_FUNDAMENTAL;
	out->fundamental = name;
	return out;
}

TypePtr MakeCv(TypePtr nested, bool is_const, bool is_volatile)
{
	if (!is_const && !is_volatile)
		return nested;

	if (nested->kind == Type::TK_LVALUE_REF || nested->kind == Type::TK_RVALUE_REF)
		return nested;

	if (nested->kind == Type::TK_ARRAY)
	{
		TypePtr out(new Type(*nested));
		out->nested = MakeCv(nested->nested, is_const, is_volatile);
		return out;
	}

	if (nested->kind == Type::TK_CV)
	{
		TypePtr out(new Type(*nested));
		out->is_const = out->is_const || is_const;
		out->is_volatile = out->is_volatile || is_volatile;
		return out;
	}

	TypePtr out(new Type());
	out->kind = Type::TK_CV;
	out->nested = nested;
	out->is_const = is_const;
	out->is_volatile = is_volatile;
	return out;
}

TypePtr MakePointer(TypePtr nested, bool is_const, bool is_volatile)
{
	TypePtr out(new Type());
	out->kind = Type::TK_POINTER;
	out->nested = MakeCv(nested, is_const, is_volatile);
	return out;
}

TypePtr MakeLValueReference(TypePtr nested)
{
	if (nested->kind == Type::TK_LVALUE_REF || nested->kind == Type::TK_RVALUE_REF)
		return MakeLValueReference(nested->nested);

	TypePtr out(new Type());
	out->kind = Type::TK_LVALUE_REF;
	out->nested = nested;
	return out;
}

TypePtr MakeRValueReference(TypePtr nested)
{
	if (nested->kind == Type::TK_LVALUE_REF)
		return MakeLValueReference(nested->nested);
	if (nested->kind == Type::TK_RVALUE_REF)
		return MakeRValueReference(nested->nested);

	TypePtr out(new Type());
	out->kind = Type::TK_RVALUE_REF;
	out->nested = nested;
	return out;
}

TypePtr MakeArray(TypePtr nested, bool has_bound, unsigned long long bound)
{
	TypePtr out(new Type());
	out->kind = Type::TK_ARRAY;
	out->nested = nested;
	out->has_bound = has_bound;
	out->bound = bound;
	return out;
}

TypePtr MakeFunction(TypePtr result, const vector<TypePtr>& params, bool variadic)
{
	TypePtr out(new Type());
	out->kind = Type::TK_FUNCTION;
	out->nested = result;
	out->params = params;
	out->variadic = variadic;
	return out;
}

string DescribeType(const TypePtr& type);

bool IsVoidType(const TypePtr& type)
{
	return type->kind == Type::TK_FUNDAMENTAL && type->fundamental == "void";
}

TypePtr RemoveTopLevelCv(TypePtr type)
{
	if (type->kind == Type::TK_CV)
		return type->nested;
	return type;
}

TypePtr AdjustParameterType(TypePtr type)
{
	type = RemoveTopLevelCv(type);
	if (type->kind == Type::TK_ARRAY)
		return MakePointer(type->nested, false, false);
	if (type->kind == Type::TK_FUNCTION)
		return MakePointer(type, false, false);
	return type;
}

string DescribeType(const TypePtr& type)
{
	switch (type->kind)
	{
	case Type::TK_FUNDAMENTAL:
		return type->fundamental;
	case Type::TK_CV:
		if (type->is_const && type->is_volatile)
			return "const volatile " + DescribeType(type->nested);
		if (type->is_const)
			return "const " + DescribeType(type->nested);
		return "volatile " + DescribeType(type->nested);
	case Type::TK_POINTER:
		return "pointer to " + DescribeType(type->nested);
	case Type::TK_LVALUE_REF:
		return "lvalue-reference to " + DescribeType(type->nested);
	case Type::TK_RVALUE_REF:
		return "rvalue-reference to " + DescribeType(type->nested);
	case Type::TK_ARRAY:
		if (!type->has_bound)
			return "array of unknown bound of " + DescribeType(type->nested);
		return "array of " + to_string(type->bound) + " " + DescribeType(type->nested);
	case Type::TK_FUNCTION:
		{
			string out = "function of (";
			for (size_t i = 0; i < type->params.size(); ++i)
			{
				if (i)
					out += ", ";
				out += DescribeType(type->params[i]);
			}
			if (type->variadic)
			{
				if (!type->params.empty())
					out += ", ";
				out += "...";
			}
			out += ") returning " + DescribeType(type->nested);
			return out;
		}
	}
	throw logic_error("bad type");
}

TypePtr MergeRedeclarationTypes(const TypePtr& lhs, const TypePtr& rhs)
{
	if (DescribeType(lhs) == DescribeType(rhs))
		return lhs;

	if (lhs->kind != rhs->kind)
		return rhs;

	switch (lhs->kind)
	{
	case Type::TK_ARRAY:
		{
			TypePtr elem = MergeRedeclarationTypes(lhs->nested, rhs->nested);
			if (DescribeType(lhs->nested) != DescribeType(rhs->nested) &&
				DescribeType(elem) != DescribeType(lhs->nested) &&
				DescribeType(elem) != DescribeType(rhs->nested))
				return rhs;

			if (lhs->has_bound && rhs->has_bound && lhs->bound != rhs->bound)
				return rhs;

			if (lhs->has_bound)
				return MakeArray(elem, true, lhs->bound);
			if (rhs->has_bound)
				return MakeArray(elem, true, rhs->bound);
			return MakeArray(elem, false, 0);
		}
	case Type::TK_CV:
		if (lhs->is_const != rhs->is_const || lhs->is_volatile != rhs->is_volatile)
			return rhs;
		return MakeCv(MergeRedeclarationTypes(lhs->nested, rhs->nested), lhs->is_const, lhs->is_volatile);
	case Type::TK_POINTER:
		return MakePointer(MergeRedeclarationTypes(lhs->nested, rhs->nested), false, false);
	case Type::TK_LVALUE_REF:
		return MakeLValueReference(MergeRedeclarationTypes(lhs->nested, rhs->nested));
	case Type::TK_RVALUE_REF:
		return MakeRValueReference(MergeRedeclarationTypes(lhs->nested, rhs->nested));
	case Type::TK_FUNCTION:
		{
			if (lhs->variadic != rhs->variadic || lhs->params.size() != rhs->params.size())
				return rhs;
			vector<TypePtr> params;
			for (size_t i = 0; i < lhs->params.size(); ++i)
			{
				TypePtr merged = MergeRedeclarationTypes(lhs->params[i], rhs->params[i]);
				if (DescribeType(merged) != DescribeType(lhs->params[i]) &&
					DescribeType(merged) != DescribeType(rhs->params[i]))
					return rhs;
				params.push_back(merged);
			}
			return MakeFunction(MergeRedeclarationTypes(lhs->nested, rhs->nested), params, lhs->variadic);
		}
	case Type::TK_FUNDAMENTAL:
		return rhs;
	}
	return rhs;
}

vector<Token> ParsePreprocTokenLines(const string& text)
{
	vector<Token> tokens;
	istringstream iss(text);
	string line;
	while (getline(iss, line))
	{
		if (line.empty())
			continue;

		Token token;
		token.line = line;

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
			token.simple = line.substr(last_space + 1);
			tokens.push_back(token);
			continue;
		}

		if (line.compare(0, 8, "literal ") == 0 || line.compare(0, 21, "user-defined-literal ") == 0)
		{
			token.is_literal = true;
			size_t first_space = line.find(' ');
			size_t second_space = line.find(' ', first_space + 1);
			if (second_space != string::npos)
				token.spelling = line.substr(first_space + 1, second_space - first_space - 1);
			tokens.push_back(token);
			continue;
		}

		throw logic_error("unexpected token line: " + line);
	}
	return tokens;
}

bool IsStorageClass(const Token& token)
{
	return token.simple == "KW_STATIC" ||
		token.simple == "KW_THREAD_LOCAL" ||
		token.simple == "KW_EXTERN";
}

bool IsFundamentalSpecifier(const Token& token)
{
	return token.simple == "KW_CHAR" ||
		token.simple == "KW_CHAR16_T" ||
		token.simple == "KW_CHAR32_T" ||
		token.simple == "KW_WCHAR_T" ||
		token.simple == "KW_BOOL" ||
		token.simple == "KW_SHORT" ||
		token.simple == "KW_INT" ||
		token.simple == "KW_LONG" ||
		token.simple == "KW_SIGNED" ||
		token.simple == "KW_UNSIGNED" ||
		token.simple == "KW_FLOAT" ||
		token.simple == "KW_DOUBLE" ||
		token.simple == "KW_VOID";
}

bool IsCvQualifier(const Token& token)
{
	return token.simple == "KW_CONST" || token.simple == "KW_VOLATILE";
}

string KeywordToSpecifier(const string& simple)
{
	if (simple == "KW_CHAR") return "char";
	if (simple == "KW_CHAR16_T") return "char16_t";
	if (simple == "KW_CHAR32_T") return "char32_t";
	if (simple == "KW_WCHAR_T") return "wchar_t";
	if (simple == "KW_BOOL") return "bool";
	if (simple == "KW_SHORT") return "short";
	if (simple == "KW_INT") return "int";
	if (simple == "KW_LONG") return "long";
	if (simple == "KW_SIGNED") return "signed";
	if (simple == "KW_UNSIGNED") return "unsigned";
	if (simple == "KW_FLOAT") return "float";
	if (simple == "KW_DOUBLE") return "double";
	if (simple == "KW_VOID") return "void";
	throw logic_error("bad keyword specifier");
}

TypePtr CanonicalizeFundamental(const vector<string>& specs)
{
	bool seen_char = false;
	bool seen_char16 = false;
	bool seen_char32 = false;
	bool seen_wchar = false;
	bool seen_bool = false;
	bool seen_short = false;
	int long_count = 0;
	bool seen_signed = false;
	bool seen_unsigned = false;
	bool seen_float = false;
	bool seen_double = false;
	bool seen_void = false;

	for (const string& spec : specs)
	{
		if (spec == "char") seen_char = true;
		else if (spec == "char16_t") seen_char16 = true;
		else if (spec == "char32_t") seen_char32 = true;
		else if (spec == "wchar_t") seen_wchar = true;
		else if (spec == "bool") seen_bool = true;
		else if (spec == "short") seen_short = true;
		else if (spec == "long") ++long_count;
		else if (spec == "signed") seen_signed = true;
		else if (spec == "unsigned") seen_unsigned = true;
		else if (spec == "int") { }
		else if (spec == "float") seen_float = true;
		else if (spec == "double") seen_double = true;
		else if (spec == "void") seen_void = true;
		else throw logic_error("bad type specifier");
	}

	if (seen_char) return MakeFundamental(seen_unsigned ? "unsigned char" : seen_signed ? "signed char" : "char");
	if (seen_char16) return MakeFundamental("char16_t");
	if (seen_char32) return MakeFundamental("char32_t");
	if (seen_wchar) return MakeFundamental("wchar_t");
	if (seen_bool) return MakeFundamental("bool");
	if (seen_float) return MakeFundamental("float");
	if (seen_void) return MakeFundamental("void");
	if (seen_double && long_count == 1) return MakeFundamental("long double");
	if (seen_double) return MakeFundamental("double");
	if (seen_short) return MakeFundamental(seen_unsigned ? "unsigned short int" : "short int");
	if (long_count >= 2) return MakeFundamental(seen_unsigned ? "unsigned long long int" : "long long int");
	if (long_count == 1) return MakeFundamental(seen_unsigned ? "unsigned long int" : "long int");
	if (seen_unsigned) return MakeFundamental("unsigned int");
	return MakeFundamental("int");
}

shared_ptr<Namespace> LookupNamespaceQualified(Namespace* scope, const string& name, set<const Namespace*>& visited);
TypePtr LookupTypeQualified(Namespace* scope, const string& name, set<const Namespace*>& visited);
shared_ptr<VariableEntity> LookupVariableQualified(Namespace* scope, const string& name, set<const Namespace*>& visited);
shared_ptr<FunctionEntity> LookupFunctionQualified(Namespace* scope, const string& name, set<const Namespace*>& visited);

shared_ptr<Namespace> LookupNamespaceIn(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	if (!scope || visited.count(scope))
		return shared_ptr<Namespace>();
	visited.insert(scope);

	map<string, shared_ptr<Namespace> >::const_iterator named_it = scope->named_namespaces.find(name);
	if (named_it != scope->named_namespaces.end())
		return named_it->second;

	map<string, shared_ptr<Namespace> >::const_iterator alias_it = scope->namespace_aliases.find(name);
	if (alias_it != scope->namespace_aliases.end())
		return alias_it->second;

	for (size_t i = 0; i < scope->namespaces_in_order.size(); ++i)
	{
		if (scope->namespaces_in_order[i]->is_inline)
		{
			shared_ptr<Namespace> found = LookupNamespaceIn(scope->namespaces_in_order[i].get(), name, visited);
			if (found)
				return found;
		}
	}

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<Namespace> found = LookupNamespaceIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	return shared_ptr<Namespace>();
}

shared_ptr<Namespace> LookupNamespaceQualified(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	return LookupNamespaceIn(scope, name, visited);
}

shared_ptr<Namespace> LookupNamespaceUnqualified(Namespace* scope, const string& name)
{
	for (Namespace* cur = scope; cur != nullptr; cur = cur->parent)
	{
		set<const Namespace*> visited;
		shared_ptr<Namespace> found = LookupNamespaceQualified(cur, name, visited);
		if (found)
			return found;
	}
	return shared_ptr<Namespace>();
}

TypePtr LookupTypeIn(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	if (!scope || visited.count(scope))
		return TypePtr();
	visited.insert(scope);

	map<string, TypePtr>::const_iterator it = scope->types.find(name);
	if (it != scope->types.end())
		return it->second;

	for (size_t i = 0; i < scope->namespaces_in_order.size(); ++i)
	{
		if (scope->namespaces_in_order[i]->is_inline)
		{
			TypePtr found = LookupTypeIn(scope->namespaces_in_order[i].get(), name, visited);
			if (found)
				return found;
		}
	}

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		TypePtr found = LookupTypeIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	return TypePtr();
}

TypePtr LookupTypeQualified(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	return LookupTypeIn(scope, name, visited);
}

TypePtr LookupTypeUnqualified(Namespace* scope, const string& name)
{
	for (Namespace* cur = scope; cur != nullptr; cur = cur->parent)
	{
		set<const Namespace*> visited;
		TypePtr found = LookupTypeQualified(cur, name, visited);
		if (found)
			return found;
	}
	return TypePtr();
}

shared_ptr<VariableEntity> LookupVariableIn(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	if (!scope || visited.count(scope))
		return shared_ptr<VariableEntity>();
	visited.insert(scope);

	map<string, shared_ptr<VariableEntity> >::const_iterator it = scope->variables.find(name);
	if (it != scope->variables.end())
		return it->second;

	map<string, shared_ptr<VariableEntity> >::const_iterator alias_it = scope->variable_aliases.find(name);
	if (alias_it != scope->variable_aliases.end())
		return alias_it->second;

	for (size_t i = 0; i < scope->namespaces_in_order.size(); ++i)
	{
		if (scope->namespaces_in_order[i]->is_inline)
		{
			shared_ptr<VariableEntity> found = LookupVariableIn(scope->namespaces_in_order[i].get(), name, visited);
			if (found)
				return found;
		}
	}

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<VariableEntity> found = LookupVariableIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	return shared_ptr<VariableEntity>();
}

shared_ptr<VariableEntity> LookupVariableQualified(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	return LookupVariableIn(scope, name, visited);
}

shared_ptr<VariableEntity> LookupVariableUnqualified(Namespace* scope, const string& name)
{
	for (Namespace* cur = scope; cur != nullptr; cur = cur->parent)
	{
		set<const Namespace*> visited;
		shared_ptr<VariableEntity> found = LookupVariableQualified(cur, name, visited);
		if (found)
			return found;
	}
	return shared_ptr<VariableEntity>();
}

shared_ptr<FunctionEntity> LookupFunctionIn(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	if (!scope || visited.count(scope))
		return shared_ptr<FunctionEntity>();
	visited.insert(scope);

	map<string, shared_ptr<FunctionEntity> >::const_iterator it = scope->functions.find(name);
	if (it != scope->functions.end())
		return it->second;

	map<string, shared_ptr<FunctionEntity> >::const_iterator alias_it = scope->function_aliases.find(name);
	if (alias_it != scope->function_aliases.end())
		return alias_it->second;

	for (size_t i = 0; i < scope->namespaces_in_order.size(); ++i)
	{
		if (scope->namespaces_in_order[i]->is_inline)
		{
			shared_ptr<FunctionEntity> found = LookupFunctionIn(scope->namespaces_in_order[i].get(), name, visited);
			if (found)
				return found;
		}
	}

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<FunctionEntity> found = LookupFunctionIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	return shared_ptr<FunctionEntity>();
}

shared_ptr<FunctionEntity> LookupFunctionQualified(Namespace* scope, const string& name, set<const Namespace*>& visited)
{
	return LookupFunctionIn(scope, name, visited);
}

shared_ptr<FunctionEntity> LookupFunctionUnqualified(Namespace* scope, const string& name)
{
	for (Namespace* cur = scope; cur != nullptr; cur = cur->parent)
	{
		set<const Namespace*> visited;
		shared_ptr<FunctionEntity> found = LookupFunctionQualified(cur, name, visited);
		if (found)
			return found;
	}
	return shared_ptr<FunctionEntity>();
}

struct DeclSpec
{
	bool is_typedef = false;
	bool is_const = false;
	bool is_volatile = false;
	TypePtr base_type;
};

struct PtrOperator
{
	enum Kind
	{
		PO_POINTER,
		PO_LVALUE_REF,
		PO_RVALUE_REF
	} kind = PO_POINTER;

	bool is_const = false;
	bool is_volatile = false;
};

struct ParsedDeclarator
{
	bool has_name = false;
	QualifiedName name;
	TypeBuilder apply;

	ParsedDeclarator()
		: apply(IdentityBuilder())
	{
	}
};

class Parser
{
public:
	Parser(const vector<Token>& tokens_, shared_ptr<Namespace> global_)
		: tokens(tokens_), global(global_), current(global_.get())
	{
	}

	void ParseTranslationUnit()
	{
		while (!PeekEof())
			ParseDeclaration();
		if (!PeekEof())
			throw logic_error("expected eof");
		++pos;
	}

private:
	const vector<Token>& tokens;
	shared_ptr<Namespace> global;
	Namespace* current;
	size_t pos = 0;

	const Token& PeekToken(size_t offset = 0) const
	{
		if (pos + offset >= tokens.size())
			throw logic_error("unexpected end of token stream");
		return tokens[pos + offset];
	}

	bool Peek(const string& simple, size_t offset = 0) const
	{
		return !PeekToken(offset).is_identifier &&
			!PeekToken(offset).is_literal &&
			!PeekToken(offset).is_eof &&
			PeekToken(offset).simple == simple;
	}

	bool PeekIdentifier(size_t offset = 0) const
	{
		return PeekToken(offset).is_identifier;
	}

	bool PeekLiteral(size_t offset = 0) const
	{
		return PeekToken(offset).is_literal;
	}

	bool PeekEof() const
	{
		return PeekToken().is_eof;
	}

	bool Accept(const string& simple)
	{
		if (Peek(simple))
		{
			++pos;
			return true;
		}
		return false;
	}

	string ExpectIdentifier()
	{
		if (!PeekIdentifier())
			throw logic_error("expected identifier");
		return tokens[pos++].spelling;
	}

	const Token& ExpectLiteral()
	{
		if (!PeekLiteral())
			throw logic_error("expected literal");
		return tokens[pos++];
	}

	void Expect(const string& simple)
	{
		if (!Accept(simple))
			throw logic_error("expected " + simple);
	}

	bool StartsNestedNameSpecifier() const
	{
		if (Peek("OP_COLON2"))
			return true;
		return PeekIdentifier() && Peek("OP_COLON2", 1);
	}

	bool StartsDeclSpecifier() const
	{
		if (Peek("KW_TYPEDEF") || IsStorageClass(PeekToken()) || IsCvQualifier(PeekToken()) || IsFundamentalSpecifier(PeekToken()))
			return true;
		return PeekIdentifier() || Peek("OP_COLON2");
	}

	bool StartsPtrOperator() const
	{
		return Peek("OP_STAR") || Peek("OP_AMP") || Peek("OP_LAND");
	}

	bool StartsDeclarator() const
	{
		return StartsPtrOperator() || PeekIdentifier() || Peek("OP_COLON2") || Peek("OP_LPAREN");
	}

	bool IsDeclaratorTerminator() const
	{
		return Peek("OP_COMMA") || Peek("OP_SEMICOLON") || Peek("OP_RPAREN") || Peek("OP_RSQUARE");
	}

	QualifiedName ParseQualifiedName()
	{
		QualifiedName out;
		if (Accept("OP_COLON2"))
			out.global = true;

		while (PeekIdentifier() && Peek("OP_COLON2", 1))
		{
			out.qualifiers.push_back(ExpectIdentifier());
			Expect("OP_COLON2");
		}

		out.name = ExpectIdentifier();
		return out;
	}

	shared_ptr<Namespace> ResolveQualifiedNamespace(const QualifiedName& name)
	{
		shared_ptr<Namespace> scope = name.global ? global : shared_ptr<Namespace>(current, [](Namespace*) {});
		for (size_t i = 0; i < name.qualifiers.size(); ++i)
		{
			set<const Namespace*> visited;
			if (i == 0 && !name.global)
				scope = LookupNamespaceUnqualified(scope.get(), name.qualifiers[i]);
			else
				scope = LookupNamespaceQualified(scope.get(), name.qualifiers[i], visited);
			if (!scope)
				throw logic_error("unknown namespace: " + name.qualifiers[i]);
		}
		return scope;
	}

	TypePtr ResolveQualifiedType(const QualifiedName& name)
	{
		if (name.qualifiers.empty())
		{
			if (name.global)
			{
				set<const Namespace*> visited;
				TypePtr out = LookupTypeQualified(global.get(), name.name, visited);
				if (!out)
					throw logic_error("unknown type: " + name.name);
				return out;
			}

			TypePtr out = LookupTypeUnqualified(current, name.name);
			if (!out)
				throw logic_error("unknown type: " + name.name);
			return out;
		}

		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name);
		set<const Namespace*> visited;
		TypePtr out = LookupTypeQualified(scope.get(), name.name, visited);
		if (!out)
			throw logic_error("unknown type: " + name.name);
		return out;
	}

	shared_ptr<Namespace> ResolveNamespaceName(const QualifiedName& name)
	{
		if (name.qualifiers.empty())
		{
			if (name.global)
			{
				set<const Namespace*> visited;
				shared_ptr<Namespace> out = LookupNamespaceQualified(global.get(), name.name, visited);
				if (!out)
					throw logic_error("unknown namespace: " + name.name);
				return out;
			}

			shared_ptr<Namespace> out = LookupNamespaceUnqualified(current, name.name);
			if (!out)
				throw logic_error("unknown namespace: " + name.name);
			return out;
		}

		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name);
		set<const Namespace*> visited;
		shared_ptr<Namespace> out = LookupNamespaceQualified(scope.get(), name.name, visited);
		if (!out)
			throw logic_error("unknown namespace: " + name.name);
		return out;
	}

	unsigned long long ParseArrayBoundLiteral(const Token& token)
	{
		string source = token.spelling;
		while (!source.empty())
		{
			char c = source[source.size() - 1];
			if (c == 'u' || c == 'U' || c == 'l' || c == 'L')
				source.erase(source.size() - 1);
			else
				break;
		}

		int base = 10;
		size_t index = 0;
		if (source.size() > 2 && source[0] == '0' && (source[1] == 'x' || source[1] == 'X'))
		{
			base = 16;
			index = 2;
		}
		else if (source.size() > 1 && source[0] == '0')
		{
			base = 8;
			index = 1;
		}

		unsigned long long value = 0;
		for (; index < source.size(); ++index)
		{
			char c = source[index];
			int digit = 0;
			if (c >= '0' && c <= '9') digit = c - '0';
			else if (c >= 'a' && c <= 'f') digit = 10 + c - 'a';
			else if (c >= 'A' && c <= 'F') digit = 10 + c - 'A';
			else throw logic_error("bad array bound literal");
			value = value * base + static_cast<unsigned long long>(digit);
		}
		return value;
	}

	DeclSpec ParseDeclSpecifierSeq(bool allow_typedef, bool allow_storage)
	{
		DeclSpec out;
		vector<string> fundamentals;
		bool saw_base = false;

		while (StartsDeclSpecifier())
		{
			if (allow_typedef && Accept("KW_TYPEDEF"))
			{
				out.is_typedef = true;
				continue;
			}

			if (allow_storage && IsStorageClass(PeekToken()))
			{
				++pos;
				continue;
			}

			if (Accept("KW_CONST"))
			{
				out.is_const = true;
				continue;
			}

			if (Accept("KW_VOLATILE"))
			{
				out.is_volatile = true;
				continue;
			}

			if (IsFundamentalSpecifier(PeekToken()))
			{
				fundamentals.push_back(KeywordToSpecifier(PeekToken().simple));
				saw_base = true;
				++pos;
				continue;
			}

			if (!saw_base && (PeekIdentifier() || Peek("OP_COLON2")))
			{
				QualifiedName name = ParseQualifiedName();
				out.base_type = ResolveQualifiedType(name);
				saw_base = true;
				continue;
			}

			break;
		}

		if (!out.base_type)
		{
			if (fundamentals.empty())
				throw logic_error("missing type specifier");
			out.base_type = CanonicalizeFundamental(fundamentals);
		}

		out.base_type = MakeCv(out.base_type, out.is_const, out.is_volatile);
		return out;
	}

	PtrOperator ParsePtrOperator()
	{
		PtrOperator out;
		if (Accept("OP_STAR"))
		{
			out.kind = PtrOperator::PO_POINTER;
			while (true)
			{
				if (Accept("KW_CONST"))
					out.is_const = true;
				else if (Accept("KW_VOLATILE"))
					out.is_volatile = true;
				else
					break;
			}
			return out;
		}
		if (Accept("OP_AMP"))
		{
			out.kind = PtrOperator::PO_LVALUE_REF;
			return out;
		}
		if (Accept("OP_LAND"))
		{
			out.kind = PtrOperator::PO_RVALUE_REF;
			return out;
		}
		throw logic_error("expected ptr operator");
	}

	TypePtr ApplyPtrOperator(const PtrOperator& op, TypePtr base)
	{
		if (op.kind == PtrOperator::PO_POINTER)
			return MakePointer(base, op.is_const, op.is_volatile);
		if (op.kind == PtrOperator::PO_LVALUE_REF)
			return MakeLValueReference(base);
		return MakeRValueReference(base);
	}

	vector<TypePtr> NormalizeFunctionParams(vector<TypePtr> params, bool variadic)
	{
		for (size_t i = 0; i < params.size(); ++i)
			params[i] = AdjustParameterType(params[i]);
		if (!variadic && params.size() == 1 && IsVoidType(params[0]))
			params.clear();
		return params;
	}

	ParsedDeclarator ParseDirectDeclarator()
	{
		ParsedDeclarator out;
		if (Accept("OP_LPAREN"))
		{
			out = ParseDeclarator();
			Expect("OP_RPAREN");
		}
		else
		{
			out.has_name = true;
			out.name = ParseQualifiedName();
		}

		while (Peek("OP_LPAREN") || Peek("OP_LSQUARE"))
		{
			if (Accept("OP_LPAREN"))
			{
				vector<TypePtr> params;
				bool variadic = false;
				ParseParameterClause(params, variadic);
				Expect("OP_RPAREN");
				params = NormalizeFunctionParams(params, variadic);
				TypeBuilder prev = out.apply;
				out.apply = [prev, params, variadic](TypePtr base) { return prev(MakeFunction(base, params, variadic)); };
			}
			else
			{
				Expect("OP_LSQUARE");
				bool has_bound = false;
				unsigned long long bound = 0;
				if (!Peek("OP_RSQUARE"))
				{
					has_bound = true;
					bound = ParseArrayBoundLiteral(ExpectLiteral());
				}
				Expect("OP_RSQUARE");
				TypeBuilder prev = out.apply;
				out.apply = [prev, has_bound, bound](TypePtr base) { return prev(MakeArray(base, has_bound, bound)); };
			}
		}
		return out;
	}

	ParsedDeclarator ParseDeclarator()
	{
		vector<PtrOperator> ops;
		while (StartsPtrOperator())
			ops.push_back(ParsePtrOperator());

		ParsedDeclarator out = ParseDirectDeclarator();
		for (size_t i = ops.size(); i > 0; --i)
		{
			TypeBuilder prev = out.apply;
			PtrOperator op = ops[i - 1];
			out.apply = [prev, op, this](TypePtr base) { return prev(ApplyPtrOperator(op, base)); };
		}
		return out;
	}

	ParsedDeclarator ParseDirectAbstractDeclarator()
	{
		ParsedDeclarator out;
		bool have_root = false;

		if (Peek("OP_LPAREN"))
		{
			size_t save = pos;
			try
			{
				Expect("OP_LPAREN");
				out = ParseAbstractDeclarator();
				Expect("OP_RPAREN");
				have_root = true;
			}
			catch (const exception&)
			{
				pos = save;
				out = ParsedDeclarator();
			}
		}

		bool saw_suffix = false;
		while (Peek("OP_LPAREN") || Peek("OP_LSQUARE"))
		{
			saw_suffix = true;
			if (Accept("OP_LPAREN"))
			{
				vector<TypePtr> params;
				bool variadic = false;
				ParseParameterClause(params, variadic);
				Expect("OP_RPAREN");
				params = NormalizeFunctionParams(params, variadic);
				TypeBuilder prev = out.apply;
				out.apply = [prev, params, variadic](TypePtr base) { return prev(MakeFunction(base, params, variadic)); };
			}
			else
			{
				Expect("OP_LSQUARE");
				bool has_bound = false;
				unsigned long long bound = 0;
				if (!Peek("OP_RSQUARE"))
				{
					has_bound = true;
					bound = ParseArrayBoundLiteral(ExpectLiteral());
				}
				Expect("OP_RSQUARE");
				TypeBuilder prev = out.apply;
				out.apply = [prev, has_bound, bound](TypePtr base) { return prev(MakeArray(base, has_bound, bound)); };
			}
		}

		if (!have_root && !saw_suffix)
			throw logic_error("bad abstract declarator");
		return out;
	}

	ParsedDeclarator ParseAbstractDeclarator()
	{
		vector<PtrOperator> ops;
		while (StartsPtrOperator())
			ops.push_back(ParsePtrOperator());

		ParsedDeclarator out;
		bool have_direct = false;
		if (Peek("OP_LPAREN") || Peek("OP_LSQUARE"))
		{
			size_t save = pos;
			try
			{
				out = ParseDirectAbstractDeclarator();
				have_direct = true;
			}
			catch (const exception&)
			{
				pos = save;
				out = ParsedDeclarator();
			}
		}

		if (!have_direct && ops.empty())
			throw logic_error("expected abstract declarator");

		for (size_t i = ops.size(); i > 0; --i)
		{
			TypeBuilder prev = out.apply;
			PtrOperator op = ops[i - 1];
			out.apply = [prev, op, this](TypePtr base) { return prev(ApplyPtrOperator(op, base)); };
		}
		return out;
	}

	TypePtr ParseTypeId()
	{
		DeclSpec spec = ParseDeclSpecifierSeq(false, false);
		if (IsDeclaratorTerminator())
			return spec.base_type;

		size_t save = pos;
		try
		{
			ParsedDeclarator decl = ParseAbstractDeclarator();
			return decl.apply(spec.base_type);
		}
		catch (const exception&)
		{
			pos = save;
			return spec.base_type;
		}
	}

	TypePtr ParseParameterDeclaration()
	{
		DeclSpec spec = ParseDeclSpecifierSeq(false, true);
		if (IsDeclaratorTerminator())
			return spec.base_type;

		size_t save = pos;
		try
		{
			ParsedDeclarator decl = ParseDeclarator();
			return decl.apply(spec.base_type);
		}
		catch (const exception&)
		{
			pos = save;
			ParsedDeclarator decl = ParseAbstractDeclarator();
			return decl.apply(spec.base_type);
		}
	}

	void ParseParameterClause(vector<TypePtr>& params, bool& variadic)
	{
		if (Peek("OP_RPAREN"))
			return;

		if (Accept("OP_DOTS"))
		{
			variadic = true;
			return;
		}

		params.push_back(ParseParameterDeclaration());
		while (Accept("OP_COMMA"))
		{
			if (Accept("OP_DOTS"))
			{
				variadic = true;
				return;
			}
			params.push_back(ParseParameterDeclaration());
		}

		if (Accept("OP_DOTS"))
			variadic = true;
	}

	void ParseDeclaration()
	{
		if (Accept("OP_SEMICOLON"))
			return;
		if (Peek("KW_INLINE"))
		{
			ParseNamespaceDefinition();
			return;
		}
		if (Peek("KW_NAMESPACE"))
		{
			if (PeekIdentifier(1) && Peek("OP_ASS", 2))
				ParseNamespaceAliasDefinition();
			else
				ParseNamespaceDefinition();
			return;
		}
		if (Peek("KW_USING"))
		{
			if (Peek("KW_NAMESPACE", 1))
				ParseUsingDirective();
			else if (PeekIdentifier(1) && Peek("OP_ASS", 2))
				ParseAliasDeclaration();
			else
				ParseUsingDeclaration();
			return;
		}
		ParseSimpleDeclaration();
	}

	shared_ptr<Namespace> GetOrCreateNamedNamespace(Namespace* parent, const string& name, bool is_inline)
	{
		map<string, shared_ptr<Namespace> >::iterator it = parent->named_namespaces.find(name);
		if (it != parent->named_namespaces.end())
		{
			it->second->is_inline = it->second->is_inline || is_inline;
			return it->second;
		}

		shared_ptr<Namespace> out(new Namespace());
		out->name = name;
		out->is_named = true;
		out->is_inline = is_inline;
		out->parent = parent;
		parent->named_namespaces[name] = out;
		parent->namespaces_in_order.push_back(out);
		return out;
	}

	shared_ptr<Namespace> CreateUnnamedNamespace(Namespace* parent, bool is_inline)
	{
		if (parent->unnamed_namespace)
		{
			parent->unnamed_namespace->is_inline = parent->unnamed_namespace->is_inline || is_inline;
			return parent->unnamed_namespace;
		}

		shared_ptr<Namespace> out(new Namespace());
		out->is_named = false;
		out->is_inline = is_inline;
		out->parent = parent;
		parent->unnamed_namespace = out;
		parent->namespaces_in_order.push_back(out);
		parent->using_directives.push_back(out);
		return out;
	}

	void ParseNamespaceDefinition()
	{
		bool is_inline = Accept("KW_INLINE");
		Expect("KW_NAMESPACE");
		shared_ptr<Namespace> inner;
		if (PeekIdentifier())
			inner = GetOrCreateNamedNamespace(current, ExpectIdentifier(), is_inline);
		else
			inner = CreateUnnamedNamespace(current, is_inline);
		Expect("OP_LBRACE");
		Namespace* saved = current;
		current = inner.get();
		while (!Peek("OP_RBRACE"))
			ParseDeclaration();
		Expect("OP_RBRACE");
		current = saved;
	}

	void ParseNamespaceAliasDefinition()
	{
		Expect("KW_NAMESPACE");
		string alias = ExpectIdentifier();
		Expect("OP_ASS");
		QualifiedName target = ParseQualifiedName();
		Expect("OP_SEMICOLON");
		current->namespace_aliases[alias] = ResolveNamespaceName(target);
	}

	void ParseUsingDirective()
	{
		Expect("KW_USING");
		Expect("KW_NAMESPACE");
		QualifiedName name = ParseQualifiedName();
		Expect("OP_SEMICOLON");
		current->using_directives.push_back(ResolveNamespaceName(name));
	}

	void ParseAliasDeclaration()
	{
		Expect("KW_USING");
		string name = ExpectIdentifier();
		Expect("OP_ASS");
		TypePtr type = ParseTypeId();
		Expect("OP_SEMICOLON");
		current->types[name] = type;
	}

	void ParseUsingDeclaration()
	{
		Expect("KW_USING");
		QualifiedName name = ParseQualifiedName();
		Expect("OP_SEMICOLON");
		if (name.qualifiers.empty() && !name.global)
			throw logic_error("using declaration requires qualification");

		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name);
		set<const Namespace*> visited;
		TypePtr type = LookupTypeQualified(scope.get(), name.name, visited);
		if (type)
		{
			current->types[name.name] = type;
			return;
		}

		visited.clear();
		shared_ptr<VariableEntity> var = LookupVariableQualified(scope.get(), name.name, visited);
		if (var)
		{
			current->variable_aliases[name.name] = var;
			return;
		}

		visited.clear();
		shared_ptr<FunctionEntity> fn = LookupFunctionQualified(scope.get(), name.name, visited);
		if (fn)
		{
			current->function_aliases[name.name] = fn;
			return;
		}

		throw logic_error("unknown using target: " + name.name);
	}

	void AddTypeAlias(Namespace* scope, const string& name, TypePtr type)
	{
		scope->types[name] = type;
	}

	void AddVariable(Namespace* scope, const string& name, TypePtr type)
	{
		map<string, shared_ptr<VariableEntity> >::iterator it = scope->variables.find(name);
		if (it == scope->variables.end())
		{
			shared_ptr<VariableEntity> entity(new VariableEntity());
			entity->name = name;
			entity->type = type;
			scope->variables[name] = entity;
			scope->variables_in_order.push_back(entity);
			return;
		}
		it->second->type = MergeRedeclarationTypes(it->second->type, type);
	}

	void AddFunction(Namespace* scope, const string& name, TypePtr type)
	{
		map<string, shared_ptr<FunctionEntity> >::iterator it = scope->functions.find(name);
		if (it == scope->functions.end())
		{
			shared_ptr<FunctionEntity> entity(new FunctionEntity());
			entity->name = name;
			entity->type = type;
			scope->functions[name] = entity;
			scope->functions_in_order.push_back(entity);
			return;
		}
		it->second->type = MergeRedeclarationTypes(it->second->type, type);
	}

	Namespace* ResolveDeclaratorNamespace(const QualifiedName& name)
	{
		if (name.qualifiers.empty() && !name.global)
			return current;
		return ResolveQualifiedNamespace(name).get();
	}

	void ParseSimpleDeclaration()
	{
		DeclSpec spec = ParseDeclSpecifierSeq(true, true);
		vector<ParsedDeclarator> declarators;
		declarators.push_back(ParseDeclarator());
		while (Accept("OP_COMMA"))
			declarators.push_back(ParseDeclarator());
		Expect("OP_SEMICOLON");

		for (size_t i = 0; i < declarators.size(); ++i)
		{
			TypePtr type = declarators[i].apply(spec.base_type);
			Namespace* target_scope = ResolveDeclaratorNamespace(declarators[i].name);
			const string& name = declarators[i].name.name;
			if (spec.is_typedef)
				AddTypeAlias(target_scope, name, type);
			else if (type->kind == Type::TK_FUNCTION)
				AddFunction(target_scope, name, type);
			else
				AddVariable(target_scope, name, type);
		}
	}
};

void PrintNamespace(const shared_ptr<Namespace>& ns, ostream& out)
{
	if (ns->is_named)
		out << "start namespace " << ns->name << endl;
	else
		out << "start unnamed namespace" << endl;

	if (ns->is_inline)
		out << "inline namespace" << endl;

	for (size_t i = 0; i < ns->variables_in_order.size(); ++i)
		out << "variable " << ns->variables_in_order[i]->name << " " << DescribeType(ns->variables_in_order[i]->type) << endl;

	for (size_t i = 0; i < ns->functions_in_order.size(); ++i)
		out << "function " << ns->functions_in_order[i]->name << " " << DescribeType(ns->functions_in_order[i]->type) << endl;

	for (size_t i = 0; i < ns->namespaces_in_order.size(); ++i)
		PrintNamespace(ns->namespaces_in_order[i], out);

	out << "end namespace" << endl;
}

shared_ptr<Namespace> AnalyzeTranslationUnit(const string& srcfile)
{
	pair<string, string> build = BuildDateTimeLiterals();
	string author_literal = EscapeStringLiteral("OpenAI Codex");
	Preprocessor preproc(build.first, build.second, author_literal);
	ostringstream oss;
	preproc.ProcessSourceFile(srcfile, oss);
	vector<Token> tokens = ParsePreprocTokenLines(oss.str());

	shared_ptr<Namespace> global(new Namespace());
	global->is_named = false;
	Parser parser(tokens, global);
	parser.ParseTranslationUnit();
	return global;
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;
		for (int i = 1; i < argc; ++i)
			args.push_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		ofstream out(args[1]);
		if (!out)
			throw runtime_error("cannot open output file: " + args[1]);

		size_t nsrcfiles = args.size() - 2;
		out << nsrcfiles << " translation units" << endl;
		for (size_t i = 0; i < nsrcfiles; ++i)
		{
			const string& srcfile = args[i + 2];
			shared_ptr<Namespace> global = AnalyzeTranslationUnit(srcfile);
			out << "start translation unit " << srcfile << endl;
			PrintNamespace(global, out);
			out << "end translation unit" << endl;
		}
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
