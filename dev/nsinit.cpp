// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cstdlib>
#include <cstring>
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
struct TempObject;
struct StringLiteralObject;

typedef shared_ptr<Type> TypePtr;
typedef function<TypePtr(TypePtr)> TypeBuilder;

enum LinkageKind
{
	LK_EXTERNAL,
	LK_INTERNAL
};

struct SemanticError : runtime_error
{
	SemanticError(const string& file, long line, const string& message)
		: runtime_error(file + ":" + to_string(line > 0 ? line : 1) + ":1: error: " + message)
	{
	}
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

struct TempObject
{
	TypePtr type;
	vector<unsigned char> bytes;
	size_t order = 0;
	size_t image_offset = 0;
};

struct StringLiteralObject
{
	TypePtr type;
	vector<unsigned char> bytes;
	size_t order = 0;
	size_t image_offset = 0;
};

struct VariableEntity
{
	string name;
	TypePtr type;
	LinkageKind linkage = LK_EXTERNAL;
	bool defined = false;
	bool constexpr_spec = false;
	bool const_spec = false;
	bool inline_spec = false;
	bool has_initializer = false;
	bool is_constant = false;
	bool constant_truthy = false;
	bool has_const_uint = false;
	unsigned long long const_uint = 0;
	bool init_from_temp = false;
	bool init_from_array_variable = false;
	bool init_from_function = false;
	bool init_from_variable = false;
	vector<unsigned char> init_bytes;
	shared_ptr<VariableEntity> pointed_variable;
	shared_ptr<FunctionEntity> pointed_function;
	shared_ptr<TempObject> temp_object;
	shared_ptr<VariableEntity> reference_target;
	shared_ptr<TempObject> reference_temp;
	size_t order = 0;
	size_t image_offset = 0;
	string key;
};

struct FunctionEntity
{
	string name;
	TypePtr type;
	LinkageKind linkage = LK_EXTERNAL;
	bool defined = false;
	bool inline_spec = false;
	size_t order = 0;
	size_t image_offset = 0;
	string key;
};

struct Namespace
{
	string name;
	bool is_named = false;
	bool is_inline = false;
	bool has_unnamed_ancestor = false;
	string identity_key;
	string external_key;
	int next_unnamed_index = 0;
	Namespace* parent = nullptr;

	map<string, TypePtr> types;
	map<string, shared_ptr<VariableEntity> > variables;
	map<string, map<string, shared_ptr<FunctionEntity> > > functions;
	map<string, shared_ptr<Namespace> > named_namespaces;
	map<string, shared_ptr<Namespace> > namespace_aliases;
	shared_ptr<Namespace> unnamed_namespace;
	map<string, shared_ptr<VariableEntity> > variable_aliases;
	map<string, map<string, shared_ptr<FunctionEntity> > > function_aliases;

	vector<shared_ptr<Namespace> > using_directives;
};

struct Expression
{
	TypePtr type;
	bool is_lvalue = false;
	bool has_immediate = false;
	vector<unsigned char> immediate;
	bool constant_truthy = false;
	bool has_const_uint = false;
	unsigned long long const_uint = 0;
	shared_ptr<VariableEntity> variable;
	shared_ptr<FunctionEntity> function;
	shared_ptr<StringLiteralObject> string_literal;
	string initializer_source;
	string debug_initializer_expression;
};

struct DeclSpec
{
	bool is_typedef = false;
	bool is_constexpr = false;
	bool is_const = false;
	bool is_volatile = false;
	bool is_static = false;
	bool is_thread_local = false;
	bool is_extern = false;
	bool is_inline = false;
	TypePtr base_type;
	vector<string> source_flags;
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

struct QualifiedName
{
	bool global = false;
	vector<string> qualifiers;
	string name;
};

struct ParsedDeclarator
{
	bool has_name = false;
	QualifiedName name;
	TypeBuilder apply;
	TypePtr raw_base_after_ref;
	Namespace* lookup_scope = nullptr;
	MacroToken root_token;
	bool direct_reference_repeat = false;

	ParsedDeclarator()
		: apply([](TypePtr base) { return base; })
	{
	}
};

struct ProgramState
{
	vector<shared_ptr<VariableEntity> > ordered_variables;
	vector<shared_ptr<FunctionEntity> > ordered_functions;
	vector<shared_ptr<TempObject> > temporaries;
	vector<shared_ptr<StringLiteralObject> > string_literals;
	map<string, shared_ptr<VariableEntity> > linked_variables;
	map<string, shared_ptr<FunctionEntity> > linked_functions;
	vector<string> link_logs;
	size_t next_order = 0;
};

bool IsKeyword(const MacroToken& token, const string& text)
{
	return token.type == PPT_IDENTIFIER && token.data == text;
}

bool IsIdentifierLike(const MacroToken& token)
{
	return token.type == PPT_IDENTIFIER;
}

bool IsLiteralToken(const MacroToken& token)
{
	return token.type == PPT_PP_NUMBER ||
		token.type == PPT_CHARACTER_LITERAL ||
		token.type == PPT_USER_DEFINED_CHARACTER_LITERAL ||
		token.type == PPT_STRING_LITERAL ||
		token.type == PPT_USER_DEFINED_STRING_LITERAL;
}

bool IsPunc(const MacroToken& token, const string& text)
{
	return token.type == PPT_PREPROCESSING_OP_OR_PUNC && token.data == text;
}

bool IsStorageClassKeyword(const MacroToken& token)
{
	return IsKeyword(token, "static") || IsKeyword(token, "thread_local") || IsKeyword(token, "extern");
}

bool IsFundamentalKeyword(const MacroToken& token)
{
	return IsKeyword(token, "char") ||
		IsKeyword(token, "char16_t") ||
		IsKeyword(token, "char32_t") ||
		IsKeyword(token, "wchar_t") ||
		IsKeyword(token, "bool") ||
		IsKeyword(token, "short") ||
		IsKeyword(token, "int") ||
		IsKeyword(token, "long") ||
		IsKeyword(token, "signed") ||
		IsKeyword(token, "unsigned") ||
		IsKeyword(token, "float") ||
		IsKeyword(token, "double") ||
		IsKeyword(token, "void");
}

bool IsCvKeyword(const MacroToken& token)
{
	return IsKeyword(token, "const") || IsKeyword(token, "volatile");
}

void FailAt(const MacroToken& token, const string& message)
{
	throw SemanticError(token.source_file, token.source_line, message);
}

void AppendLittleEndian64(vector<unsigned char>& out, unsigned long long value)
{
	for (int i = 0; i < 8; ++i)
		out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xff));
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
	if (nested->kind == Type::TK_LVALUE_REF || nested->kind == Type::TK_RVALUE_REF)
		return TypePtr();
	TypePtr out(new Type());
	out->kind = Type::TK_POINTER;
	out->nested = MakeCv(nested, is_const, is_volatile);
	return out;
}

TypePtr MakeLValueReference(TypePtr nested)
{
	if (nested->kind == Type::TK_LVALUE_REF || nested->kind == Type::TK_RVALUE_REF)
		return TypePtr();
	if (nested->kind == Type::TK_FUNDAMENTAL && nested->fundamental == "void")
		return TypePtr();
	TypePtr out(new Type());
	out->kind = Type::TK_LVALUE_REF;
	out->nested = nested;
	return out;
}

TypePtr MakeRValueReference(TypePtr nested)
{
	if (nested->kind == Type::TK_LVALUE_REF || nested->kind == Type::TK_RVALUE_REF)
		return TypePtr();
	if (nested->kind == Type::TK_FUNDAMENTAL && nested->fundamental == "void")
		return TypePtr();
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

TypePtr RemoveTopLevelCv(TypePtr type)
{
	if (type->kind == Type::TK_CV)
		return type->nested;
	return type;
}

bool IsVoidType(const TypePtr& type)
{
	return type->kind == Type::TK_FUNDAMENTAL && type->fundamental == "void";
}

bool IsConstObjectType(const TypePtr& type)
{
	if (type->kind == Type::TK_CV)
		return type->is_const;
	if (type->kind == Type::TK_ARRAY)
		return IsConstObjectType(type->nested);
	return false;
}

bool IsIncompleteType(const TypePtr& type)
{
	if (type->kind == Type::TK_FUNDAMENTAL)
		return type->fundamental == "void";
	if (type->kind == Type::TK_ARRAY)
		return !type->has_bound || IsIncompleteType(type->nested);
	return false;
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

TypePtr MergeRedeclarationTypes(const TypePtr& lhs, const TypePtr& rhs)
{
	if (DescribeType(lhs) == DescribeType(rhs))
		return lhs;

	if (lhs->kind == Type::TK_ARRAY && rhs->kind == Type::TK_ARRAY)
	{
		TypePtr elem = MergeRedeclarationTypes(lhs->nested, rhs->nested);
		if (lhs->has_bound)
			return MakeArray(elem, true, lhs->bound);
		if (rhs->has_bound)
			return MakeArray(elem, true, rhs->bound);
		return MakeArray(elem, false, 0);
	}

	return rhs;
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

	for (size_t i = 0; i < specs.size(); ++i)
	{
		if (specs[i] == "char") seen_char = true;
		else if (specs[i] == "char16_t") seen_char16 = true;
		else if (specs[i] == "char32_t") seen_char32 = true;
		else if (specs[i] == "wchar_t") seen_wchar = true;
		else if (specs[i] == "bool") seen_bool = true;
		else if (specs[i] == "short") seen_short = true;
		else if (specs[i] == "long") ++long_count;
		else if (specs[i] == "signed") seen_signed = true;
		else if (specs[i] == "unsigned") seen_unsigned = true;
		else if (specs[i] == "float") seen_float = true;
		else if (specs[i] == "double") seen_double = true;
		else if (specs[i] == "void") seen_void = true;
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

size_t TypeSize(const TypePtr& type)
{
	if (type->kind == Type::TK_CV)
		return TypeSize(type->nested);
	if (type->kind == Type::TK_POINTER || type->kind == Type::TK_LVALUE_REF || type->kind == Type::TK_RVALUE_REF)
		return 8;
	if (type->kind == Type::TK_ARRAY)
		return static_cast<size_t>(type->bound) * TypeSize(type->nested);
	if (type->kind == Type::TK_FUNCTION)
		return 4;
	if (type->kind != Type::TK_FUNDAMENTAL)
		throw logic_error("bad sized type");

	if (type->fundamental == "signed char" || type->fundamental == "unsigned char" || type->fundamental == "char" || type->fundamental == "bool")
		return 1;
	if (type->fundamental == "short int" || type->fundamental == "unsigned short int" || type->fundamental == "char16_t")
		return 2;
	if (type->fundamental == "int" || type->fundamental == "unsigned int" || type->fundamental == "wchar_t" || type->fundamental == "char32_t" || type->fundamental == "float")
		return 4;
	if (type->fundamental == "long int" || type->fundamental == "long long int" ||
		type->fundamental == "unsigned long int" || type->fundamental == "unsigned long long int" ||
		type->fundamental == "double" || type->fundamental == "nullptr_t")
		return 8;
	if (type->fundamental == "long double")
		return 16;
	throw logic_error("incomplete sized type");
}

size_t TypeAlign(const TypePtr& type)
{
	if (type->kind == Type::TK_CV)
		return TypeAlign(type->nested);
	if (type->kind == Type::TK_POINTER || type->kind == Type::TK_LVALUE_REF || type->kind == Type::TK_RVALUE_REF)
		return 8;
	if (type->kind == Type::TK_ARRAY)
		return TypeAlign(type->nested);
	if (type->kind == Type::TK_FUNCTION)
		return 4;
	return TypeSize(type);
}

vector<unsigned char> ZeroBytes(size_t n)
{
	return vector<unsigned char>(n, 0);
}

void AlignImage(vector<unsigned char>& image, size_t align)
{
	while (image.size() % align != 0)
		image.push_back(0);
}

vector<unsigned char> EncodeUnsignedLongLong(unsigned long long value, size_t bytes)
{
	vector<unsigned char> out;
	for (size_t i = 0; i < bytes; ++i)
		out.push_back(static_cast<unsigned char>((value >> (8 * i)) & 0xff));
	return out;
}

vector<unsigned char> ConvertImmediateBytes(const vector<unsigned char>& source, size_t src_size, const string& dest)
{
	if (dest == "signed char" || dest == "char" || dest == "unsigned char" || dest == "bool")
		return vector<unsigned char>(1, source.empty() ? 0 : source[0]);
	if (dest == "short int" || dest == "unsigned short int" || dest == "char16_t")
	{
		unsigned long long value = 0;
		for (size_t i = 0; i < src_size && i < 8; ++i)
			value |= static_cast<unsigned long long>(source[i]) << (8 * i);
		return EncodeUnsignedLongLong(value, 2);
	}
	if (dest == "int" || dest == "unsigned int" || dest == "wchar_t" || dest == "char32_t")
	{
		unsigned long long value = 0;
		for (size_t i = 0; i < src_size && i < 8; ++i)
			value |= static_cast<unsigned long long>(source[i]) << (8 * i);
		return EncodeUnsignedLongLong(value, 4);
	}
	if (dest == "long int" || dest == "long long int" || dest == "unsigned long int" || dest == "unsigned long long int" || dest == "nullptr_t")
	{
		unsigned long long value = 0;
		for (size_t i = 0; i < src_size && i < 8; ++i)
			value |= static_cast<unsigned long long>(source[i]) << (8 * i);
		return EncodeUnsignedLongLong(value, 8);
	}
	return source;
}

string JoinFlags(const vector<string>& flags)
{
	if (flags.empty())
		return "";
	string out = flags[0];
	for (size_t i = 1; i < flags.size(); ++i)
		out += "|" + flags[i];
	return out;
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

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<Namespace> found = LookupNamespaceIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	for (map<string, shared_ptr<Namespace> >::const_iterator it = scope->named_namespaces.begin(); it != scope->named_namespaces.end(); ++it)
	{
		if (it->second->is_inline)
		{
			shared_ptr<Namespace> found = LookupNamespaceIn(it->second.get(), name, visited);
			if (found)
				return found;
		}
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

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		TypePtr found = LookupTypeIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	for (map<string, shared_ptr<Namespace> >::const_iterator ns_it = scope->named_namespaces.begin(); ns_it != scope->named_namespaces.end(); ++ns_it)
	{
		if (ns_it->second->is_inline)
		{
			TypePtr found = LookupTypeIn(ns_it->second.get(), name, visited);
			if (found)
				return found;
		}
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

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<VariableEntity> found = LookupVariableIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	for (map<string, shared_ptr<Namespace> >::const_iterator ns_it = scope->named_namespaces.begin(); ns_it != scope->named_namespaces.end(); ++ns_it)
	{
		if (ns_it->second->is_inline)
		{
			shared_ptr<VariableEntity> found = LookupVariableIn(ns_it->second.get(), name, visited);
			if (found)
				return found;
		}
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

	map<string, map<string, shared_ptr<FunctionEntity> > >::const_iterator it = scope->functions.find(name);
	if (it != scope->functions.end() && !it->second.empty())
		return it->second.begin()->second;

	map<string, map<string, shared_ptr<FunctionEntity> > >::const_iterator alias_it = scope->function_aliases.find(name);
	if (alias_it != scope->function_aliases.end() && !alias_it->second.empty())
		return alias_it->second.begin()->second;

	for (size_t i = 0; i < scope->using_directives.size(); ++i)
	{
		shared_ptr<FunctionEntity> found = LookupFunctionIn(scope->using_directives[i].get(), name, visited);
		if (found)
			return found;
	}

	for (map<string, shared_ptr<Namespace> >::const_iterator ns_it = scope->named_namespaces.begin(); ns_it != scope->named_namespaces.end(); ++ns_it)
	{
		if (ns_it->second->is_inline)
		{
			shared_ptr<FunctionEntity> found = LookupFunctionIn(ns_it->second.get(), name, visited);
			if (found)
				return found;
		}
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

class Parser
{
public:
	Parser(const vector<MacroToken>& tokens_, shared_ptr<Namespace> global_, ProgramState& program_, int tu_index_)
		: tokens(tokens_), global(global_), current(global_.get()), program(program_), tu_index(tu_index_)
	{
	}

	void ParseTranslationUnit()
	{
		while (!PeekEof())
			ParseDeclaration();
		if (!PeekEof())
			throw logic_error("expected eof");
	}

private:
	const vector<MacroToken>& tokens;
	shared_ptr<Namespace> global;
	Namespace* current;
	ProgramState& program;
	int tu_index = 0;
	size_t pos = 0;

	const MacroToken& PeekToken(size_t offset = 0) const
	{
		if (pos + offset >= tokens.size())
			throw logic_error("unexpected end");
		return tokens[pos + offset];
	}

	bool PeekEof(size_t offset = 0) const
	{
		return PeekToken(offset).type == PPT_EOF;
	}

	bool AcceptKeyword(const string& text)
	{
		if (IsKeyword(PeekToken(), text))
		{
			++pos;
			return true;
		}
		return false;
	}

	bool AcceptPunc(const string& text)
	{
		if (IsPunc(PeekToken(), text))
		{
			++pos;
			return true;
		}
		return false;
	}

	void ExpectKeyword(const string& text)
	{
		if (!AcceptKeyword(text))
			FailAt(PeekToken(), "expected " + text);
	}

	void ExpectPunc(const string& text)
	{
		if (!AcceptPunc(text))
			FailAt(PeekToken(), "expected " + text);
	}

	string ExpectIdentifier()
	{
		if (!IsIdentifierLike(PeekToken()))
			FailAt(PeekToken(), "expected identifier");
		return tokens[pos++].data;
	}

	const MacroToken& ExpectLiteral()
	{
		if (!IsLiteralToken(PeekToken()))
			FailAt(PeekToken(), "expected literal");
		return tokens[pos++];
	}

	bool StartsNestedNameSpecifier() const
	{
		return IsPunc(PeekToken(), "::") || (IsIdentifierLike(PeekToken()) && IsPunc(PeekToken(1), "::"));
	}

	bool StartsDeclSpecifier() const
	{
		return AcceptableTypeNameStart(PeekToken()) ||
			IsKeyword(PeekToken(), "typedef") ||
			IsKeyword(PeekToken(), "constexpr") ||
			IsKeyword(PeekToken(), "inline") ||
			IsStorageClassKeyword(PeekToken()) ||
			IsCvKeyword(PeekToken()) ||
			IsFundamentalKeyword(PeekToken());
	}

	bool AcceptableTypeNameStart(const MacroToken& token) const
	{
		return IsIdentifierLike(token) || IsPunc(token, "::");
	}

	bool StartsPtrOperator() const
	{
		return IsPunc(PeekToken(), "*") || IsPunc(PeekToken(), "&") || IsPunc(PeekToken(), "&&");
	}

	bool IsDeclaratorTerminator() const
	{
		return IsPunc(PeekToken(), ",") || IsPunc(PeekToken(), ";") || IsPunc(PeekToken(), ")") || IsPunc(PeekToken(), "]");
	}

	vector<string> NormalizeSourceFlags(const vector<string>& raw)
	{
		vector<string> out;
		int long_count = 0;
		for (size_t i = 0; i < raw.size(); ++i)
		{
			if (raw[i] == "long")
				++long_count;
		}
		bool used_long = false;
		for (size_t i = 0; i < raw.size(); ++i)
		{
			if (raw[i] == "const") out.push_back("SP_CONST");
			else if (raw[i] == "volatile") out.push_back("SP_VOLATILE");
			else if (raw[i] == "constexpr") out.push_back("SP_CONSTEXPR");
			else if (raw[i] == "static") out.push_back("SP_STATIC");
			else if (raw[i] == "thread_local") out.push_back("SP_THREAD_LOCAL");
			else if (raw[i] == "extern") out.push_back("SP_EXTERN");
			else if (raw[i] == "inline") out.push_back("SP_INLINE");
			else if (raw[i] == "char") out.push_back("SP_CHAR");
			else if (raw[i] == "char16_t") out.push_back("SP_CHAR16_T");
			else if (raw[i] == "char32_t") out.push_back("SP_CHAR32_T");
			else if (raw[i] == "wchar_t") out.push_back("SP_WCHAR_T");
			else if (raw[i] == "bool") out.push_back("SP_BOOL");
			else if (raw[i] == "short") out.push_back("SP_SHORT");
			else if (raw[i] == "int") out.push_back("SP_INT");
			else if (raw[i] == "signed") out.push_back("SP_SIGNED");
			else if (raw[i] == "unsigned") out.push_back("SP_UNSIGNED");
			else if (raw[i] == "float") out.push_back("SP_FLOAT");
			else if (raw[i] == "double") out.push_back("SP_DOUBLE");
			else if (raw[i] == "void") out.push_back("SP_VOID");
			else if (raw[i] == "long" && !used_long)
			{
				out.push_back(long_count >= 2 ? "SP_LONG_2" : "SP_LONG_1");
				used_long = true;
			}
		}
		map<string, int> order;
		order["SP_STATIC"] = 1;
		order["SP_THREAD_LOCAL"] = 1;
		order["SP_EXTERN"] = 1;
		order["SP_INLINE"] = 1;
		order["SP_CONSTEXPR"] = 1;
		order["SP_CONST"] = 2;
		order["SP_VOLATILE"] = 2;
		order["SP_CHAR"] = 3;
		order["SP_CHAR16_T"] = 3;
		order["SP_CHAR32_T"] = 3;
		order["SP_WCHAR_T"] = 3;
		order["SP_BOOL"] = 3;
		order["SP_SHORT"] = 3;
		order["SP_LONG_1"] = 3;
		order["SP_LONG_2"] = 3;
		order["SP_FLOAT"] = 3;
		order["SP_DOUBLE"] = 3;
		order["SP_VOID"] = 3;
		order["SP_INT"] = 4;
		order["SP_SIGNED"] = 5;
		order["SP_UNSIGNED"] = 5;
		for (size_t i = 0; i < out.size(); ++i)
		{
			for (size_t j = i + 1; j < out.size(); ++j)
			{
				if (order[out[j]] < order[out[i]])
					swap(out[i], out[j]);
			}
		}
		return out;
	}

	QualifiedName ParseQualifiedName()
	{
		QualifiedName out;
		if (AcceptPunc("::"))
			out.global = true;
		while (IsIdentifierLike(PeekToken()) && IsPunc(PeekToken(1), "::"))
		{
			out.qualifiers.push_back(ExpectIdentifier());
			ExpectPunc("::");
		}
		out.name = ExpectIdentifier();
		return out;
	}

	shared_ptr<Namespace> ResolveQualifiedNamespace(const QualifiedName& name, Namespace* lookup_scope)
	{
		shared_ptr<Namespace> scope = name.global ? global : shared_ptr<Namespace>(lookup_scope, [](Namespace*) {});
		for (size_t i = 0; i < name.qualifiers.size(); ++i)
		{
			if (i == 0 && !name.global)
				scope = LookupNamespaceUnqualified(scope.get(), name.qualifiers[i]);
			else
			{
				set<const Namespace*> visited;
				scope = LookupNamespaceQualified(scope.get(), name.qualifiers[i], visited);
			}
			if (!scope)
				FailAt(PeekToken(), name.qualifiers[i] + " not found");
		}
		return scope;
	}

	TypePtr ResolveQualifiedType(const QualifiedName& name, Namespace* lookup_scope)
	{
		if (name.qualifiers.empty())
		{
			TypePtr out;
			if (name.global)
			{
				set<const Namespace*> visited;
				out = LookupTypeQualified(global.get(), name.name, visited);
			}
			else
			{
				out = LookupTypeUnqualified(lookup_scope, name.name);
			}
			if (!out)
				FailAt(PeekToken(), name.name + " not found");
			return out;
		}

		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name, lookup_scope);
		set<const Namespace*> visited;
		TypePtr out = LookupTypeQualified(scope.get(), name.name, visited);
		if (!out)
			FailAt(PeekToken(), name.name + " not found");
		return out;
	}

	shared_ptr<Namespace> ResolveNamespaceName(const QualifiedName& name, Namespace* lookup_scope)
	{
		if (name.qualifiers.empty())
		{
			if (name.global)
			{
				set<const Namespace*> visited;
				shared_ptr<Namespace> out = LookupNamespaceQualified(global.get(), name.name, visited);
				if (!out)
					FailAt(PeekToken(), name.name + " not found");
				return out;
			}
			shared_ptr<Namespace> out = LookupNamespaceUnqualified(lookup_scope, name.name);
			if (!out)
				FailAt(PeekToken(), name.name + " not found");
			return out;
		}

		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name, lookup_scope);
		set<const Namespace*> visited;
		shared_ptr<Namespace> out = LookupNamespaceQualified(scope.get(), name.name, visited);
		if (!out)
			FailAt(PeekToken(), name.name + " not found");
		return out;
	}

	unsigned long long ParseArrayBoundFromExpression(const Expression& expr, const MacroToken& token)
	{
		if (!expr.has_const_uint || expr.const_uint == 0)
			FailAt(token, "array bound not a converted constant expression");
		return expr.const_uint;
	}

	DeclSpec ParseDeclSpecifierSeq(bool allow_typedef, bool allow_storage, Namespace* lookup_scope)
	{
		DeclSpec out;
		vector<string> fundamentals;
		vector<string> raw_flags;
		bool saw_base = false;

		while (StartsDeclSpecifier())
		{
			if (allow_typedef && AcceptKeyword("typedef"))
			{
				out.is_typedef = true;
				raw_flags.push_back("typedef");
				continue;
			}
			if (AcceptKeyword("constexpr"))
			{
				out.is_constexpr = true;
				raw_flags.push_back("constexpr");
				continue;
			}
			if (AcceptKeyword("inline"))
			{
				out.is_inline = true;
				raw_flags.push_back("inline");
				continue;
			}
			if (allow_storage && AcceptKeyword("static"))
			{
				out.is_static = true;
				raw_flags.push_back("static");
				continue;
			}
			if (allow_storage && AcceptKeyword("thread_local"))
			{
				out.is_thread_local = true;
				raw_flags.push_back("thread_local");
				continue;
			}
			if (allow_storage && AcceptKeyword("extern"))
			{
				out.is_extern = true;
				raw_flags.push_back("extern");
				continue;
			}
			if (AcceptKeyword("const"))
			{
				out.is_const = true;
				raw_flags.push_back("const");
				continue;
			}
			if (AcceptKeyword("volatile"))
			{
				out.is_volatile = true;
				raw_flags.push_back("volatile");
				continue;
			}
			if (IsFundamentalKeyword(PeekToken()))
			{
				fundamentals.push_back(PeekToken().data);
				raw_flags.push_back(PeekToken().data);
				saw_base = true;
				++pos;
				continue;
			}
			if (!saw_base && AcceptableTypeNameStart(PeekToken()))
			{
				QualifiedName name = ParseQualifiedName();
				out.base_type = ResolveQualifiedType(name, lookup_scope);
				saw_base = true;
				continue;
			}
			break;
		}

		if (!out.base_type)
		{
			if (fundamentals.empty())
				FailAt(PeekToken(), "missing type specifier");
			out.base_type = CanonicalizeFundamental(fundamentals);
		}
		out.base_type = MakeCv(out.base_type, out.is_const, out.is_volatile);
		out.source_flags = NormalizeSourceFlags(raw_flags);
		return out;
	}

	PtrOperator ParsePtrOperator()
	{
		PtrOperator out;
		if (AcceptPunc("*"))
		{
			out.kind = PtrOperator::PO_POINTER;
			while (true)
			{
				if (AcceptKeyword("const"))
					out.is_const = true;
				else if (AcceptKeyword("volatile"))
					out.is_volatile = true;
				else
					break;
			}
			return out;
		}
		if (AcceptPunc("&"))
		{
			out.kind = PtrOperator::PO_LVALUE_REF;
			return out;
		}
		if (AcceptPunc("&&"))
		{
			out.kind = PtrOperator::PO_RVALUE_REF;
			return out;
		}
		FailAt(PeekToken(), "expected ptr-operator");
		return out;
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
		if (AcceptPunc("("))
		{
			out = ParseDeclarator(current);
			ExpectPunc(")");
		}
		else
		{
			out.has_name = true;
			out.root_token = PeekToken();
			out.name = ParseQualifiedName();
			out.lookup_scope = ResolveQualifiedNamespace(out.name, current).get();
		}

		Namespace* suffix_scope = out.lookup_scope ? out.lookup_scope : current;

		while (IsPunc(PeekToken(), "(") || IsPunc(PeekToken(), "["))
		{
			if (AcceptPunc("("))
			{
				vector<TypePtr> params;
				bool variadic = false;
				ParseParameterClause(params, variadic, suffix_scope);
				ExpectPunc(")");
				params = NormalizeFunctionParams(params, variadic);
				TypeBuilder prev = out.apply;
				out.apply = [prev, params, variadic](TypePtr base) { return prev(MakeFunction(base, params, variadic)); };
			}
			else
			{
				ExpectPunc("[");
				bool has_bound = false;
				unsigned long long bound = 0;
				if (!IsPunc(PeekToken(), "]"))
				{
					Expression expr = ParseExpression(suffix_scope, true);
					has_bound = true;
					bound = ParseArrayBoundFromExpression(expr, PeekToken());
				}
				ExpectPunc("]");
				TypeBuilder prev = out.apply;
				out.apply = [prev, has_bound, bound](TypePtr base) { return prev(MakeArray(base, has_bound, bound)); };
			}
		}
		return out;
	}

	ParsedDeclarator ParseDeclarator(Namespace* lookup_scope)
	{
		vector<PtrOperator> ops;
		MacroToken first_op = PeekToken();
		while (StartsPtrOperator())
			ops.push_back(ParsePtrOperator());

		ParsedDeclarator out = ParseDirectDeclarator();
		out.lookup_scope = out.lookup_scope ? out.lookup_scope : lookup_scope;

		for (size_t i = ops.size(); i > 0; --i)
		{
			PtrOperator op = ops[i - 1];
			TypeBuilder prev = out.apply;
			out.apply = [prev, op, first_op, &out](TypePtr base) {
				TypePtr next;
				if (op.kind == PtrOperator::PO_POINTER)
					next = MakePointer(base, op.is_const, op.is_volatile);
				else if (op.kind == PtrOperator::PO_LVALUE_REF)
				{
					if (base->kind == Type::TK_LVALUE_REF || base->kind == Type::TK_RVALUE_REF)
						return TypePtr();
					next = MakeLValueReference(base);
				}
				else
				{
					if (base->kind == Type::TK_LVALUE_REF || base->kind == Type::TK_RVALUE_REF)
						return TypePtr();
					next = MakeRValueReference(base);
				}
				return next ? prev(next) : TypePtr();
			};
		}
		return out;
	}

	ParsedDeclarator ParseDirectAbstractDeclarator(Namespace* lookup_scope)
	{
		ParsedDeclarator out;
		bool have_root = false;

		if (IsPunc(PeekToken(), "("))
		{
			size_t save = pos;
			try
			{
				ExpectPunc("(");
				out = ParseAbstractDeclarator(lookup_scope);
				ExpectPunc(")");
				have_root = true;
			}
			catch (const exception&)
			{
				pos = save;
				out = ParsedDeclarator();
			}
		}

		bool saw_suffix = false;
		while (IsPunc(PeekToken(), "(") || IsPunc(PeekToken(), "["))
		{
			saw_suffix = true;
			if (AcceptPunc("("))
			{
				vector<TypePtr> params;
				bool variadic = false;
				ParseParameterClause(params, variadic, lookup_scope);
				ExpectPunc(")");
				params = NormalizeFunctionParams(params, variadic);
				TypeBuilder prev = out.apply;
				out.apply = [prev, params, variadic](TypePtr base) { return prev(MakeFunction(base, params, variadic)); };
			}
			else
			{
				ExpectPunc("[");
				bool has_bound = false;
				unsigned long long bound = 0;
				if (!IsPunc(PeekToken(), "]"))
				{
					Expression expr = ParseExpression(lookup_scope, true);
					has_bound = true;
					bound = ParseArrayBoundFromExpression(expr, PeekToken());
				}
				ExpectPunc("]");
				TypeBuilder prev = out.apply;
				out.apply = [prev, has_bound, bound](TypePtr base) { return prev(MakeArray(base, has_bound, bound)); };
			}
		}

		if (!have_root && !saw_suffix)
			FailAt(PeekToken(), "bad abstract declarator");
		return out;
	}

	ParsedDeclarator ParseAbstractDeclarator(Namespace* lookup_scope)
	{
		vector<PtrOperator> ops;
		while (StartsPtrOperator())
			ops.push_back(ParsePtrOperator());

		ParsedDeclarator out;
		bool have_direct = false;
		if (IsPunc(PeekToken(), "(") || IsPunc(PeekToken(), "["))
		{
			size_t save = pos;
			try
			{
				out = ParseDirectAbstractDeclarator(lookup_scope);
				have_direct = true;
			}
			catch (const exception&)
			{
				pos = save;
			}
		}

		if (!have_direct && ops.empty())
			FailAt(PeekToken(), "expected abstract declarator");

		for (size_t i = ops.size(); i > 0; --i)
		{
			PtrOperator op = ops[i - 1];
			TypeBuilder prev = out.apply;
			out.apply = [prev, op](TypePtr base) {
				TypePtr next;
				if (op.kind == PtrOperator::PO_POINTER)
					next = MakePointer(base, op.is_const, op.is_volatile);
				else if (op.kind == PtrOperator::PO_LVALUE_REF)
					next = MakeLValueReference(base);
				else
					next = MakeRValueReference(base);
				return next ? prev(next) : TypePtr();
			};
		}
		return out;
	}

	TypePtr ParseTypeId(Namespace* lookup_scope)
	{
		DeclSpec spec = ParseDeclSpecifierSeq(false, false, lookup_scope);
		if (IsDeclaratorTerminator())
			return spec.base_type;
		size_t save = pos;
		try
		{
			ParsedDeclarator decl = ParseAbstractDeclarator(lookup_scope);
			TypePtr out = decl.apply(spec.base_type);
			if (!out)
				FailAt(decl.root_token.type == PPT_EOF ? PeekToken() : decl.root_token, "invalid type");
			return out;
		}
		catch (const exception&)
		{
			pos = save;
			return spec.base_type;
		}
	}

	TypePtr ParseParameterDeclaration(Namespace* lookup_scope)
	{
		DeclSpec spec = ParseDeclSpecifierSeq(false, true, lookup_scope);
		if (IsDeclaratorTerminator())
			return spec.base_type;

		size_t save = pos;
		try
		{
			ParsedDeclarator decl = ParseDeclarator(lookup_scope);
			TypePtr out = decl.apply(spec.base_type);
			if (!out)
				FailAt(decl.root_token.type == PPT_EOF ? PeekToken() : decl.root_token, "invalid type");
			return out;
		}
		catch (const exception&)
		{
			pos = save;
			ParsedDeclarator decl = ParseAbstractDeclarator(lookup_scope);
			TypePtr out = decl.apply(spec.base_type);
			if (!out)
				FailAt(PeekToken(), "invalid type");
			return out;
		}
	}

	void ParseParameterClause(vector<TypePtr>& params, bool& variadic, Namespace* lookup_scope)
	{
		if (IsPunc(PeekToken(), ")"))
			return;
		if (AcceptPunc("..."))
		{
			variadic = true;
			return;
		}
		params.push_back(ParseParameterDeclaration(lookup_scope));
		while (AcceptPunc(","))
		{
			if (AcceptPunc("..."))
			{
				variadic = true;
				return;
			}
			params.push_back(ParseParameterDeclaration(lookup_scope));
		}
		if (AcceptPunc("..."))
			variadic = true;
	}

	bool ScopeIsWithin(Namespace* outer, Namespace* candidate) const
	{
		for (Namespace* cur = candidate; cur != nullptr; cur = cur->parent)
		{
			if (cur == outer)
				return true;
		}
		return false;
	}

	void EnsureNoNamespaceConflict(Namespace* scope, const string& name, const MacroToken& token)
	{
		if (scope->namespace_aliases.count(name))
			FailAt(token, name + " already exists");
		if (scope->named_namespaces.count(name))
			return;
		if (scope->variables.count(name) || scope->variable_aliases.count(name) || scope->types.count(name) ||
			scope->functions.count(name) || scope->function_aliases.count(name))
			FailAt(token, name + " already exists");
	}

	void EnsureFunctionNameAvailable(Namespace* scope, const string& name, const MacroToken& token)
	{
		if (scope->namespace_aliases.count(name) || scope->named_namespaces.count(name) ||
			scope->variables.count(name) || scope->variable_aliases.count(name) || scope->types.count(name))
			FailAt(token, name + " already exists");
	}

	void EnsureVariableNameAvailable(Namespace* scope, const string& name, const MacroToken& token)
	{
		if (scope->namespace_aliases.count(name) || scope->named_namespaces.count(name) ||
			scope->types.count(name) || scope->functions.count(name) || scope->function_aliases.count(name))
			FailAt(token, name + " already exists");
	}

	void EnsureNameAvailableForNamespace(Namespace* scope, const string& name, const MacroToken& token)
	{
		if (scope->namespace_aliases.count(name))
			FailAt(token, name + " already exists");
		if (scope->variables.count(name) || scope->variable_aliases.count(name) || scope->types.count(name) ||
			scope->functions.count(name) || scope->function_aliases.count(name))
			FailAt(token, name + " already exists");
	}

	shared_ptr<Namespace> GetOrCreateNamedNamespace(Namespace* parent, const string& name, bool is_inline, const MacroToken& token)
	{
		EnsureNameAvailableForNamespace(parent, name, token);
		map<string, shared_ptr<Namespace> >::iterator it = parent->named_namespaces.find(name);
		if (it != parent->named_namespaces.end())
		{
			if (is_inline && !it->second->is_inline)
				FailAt(token, "extension namespace cannot be inline");
			return it->second;
		}

		shared_ptr<Namespace> out(new Namespace());
		out->name = name;
		out->is_named = true;
		out->is_inline = is_inline;
		out->parent = parent;
		out->has_unnamed_ancestor = parent->has_unnamed_ancestor;
		out->identity_key = parent->identity_key + "::" + name;
		out->external_key = parent->external_key + "::" + name;
		parent->named_namespaces[name] = out;
		return out;
	}

	shared_ptr<Namespace> GetOrCreateUnnamedNamespace(Namespace* parent, bool is_inline)
	{
		if (parent->unnamed_namespace)
			return parent->unnamed_namespace;
		shared_ptr<Namespace> out(new Namespace());
		out->is_named = false;
		out->is_inline = is_inline;
		out->parent = parent;
		out->has_unnamed_ancestor = true;
		out->identity_key = parent->identity_key + "::<unnamed#" + to_string(++parent->next_unnamed_index) + ">";
		out->external_key = parent->external_key + "::<unnamed>";
		parent->unnamed_namespace = out;
		parent->using_directives.push_back(out);
		return out;
	}

	LinkageKind ComputeLinkage(const DeclSpec& spec, Namespace* scope, const TypePtr& type) const
	{
		if (spec.is_static)
			return LK_INTERNAL;
		if (scope->has_unnamed_ancestor)
			return LK_INTERNAL;
		if (spec.is_constexpr)
			return LK_EXTERNAL;
		if (type->kind != Type::TK_FUNCTION && IsConstObjectType(type) && !spec.is_extern)
			return LK_INTERNAL;
		return LK_EXTERNAL;
	}

	string EntityScopeKey(Namespace* scope, bool external) const
	{
		return external ? scope->external_key : scope->identity_key;
	}

	shared_ptr<VariableEntity> GetOrCreateVariableEntity(Namespace* scope, const string& name, const TypePtr& type, LinkageKind linkage)
	{
		bool external = linkage == LK_EXTERNAL;
		string key = (external ? "E" : "I") + string(":") +
			(external ? EntityScopeKey(scope, true) : ("TU" + to_string(tu_index) + ":" + EntityScopeKey(scope, false))) +
			":" + name;
		map<string, shared_ptr<VariableEntity> >::iterator linked_it = program.linked_variables.find(key);
		if (linked_it != program.linked_variables.end())
		{
			linked_it->second->type = MergeRedeclarationTypes(linked_it->second->type, type);
			scope->variables[name] = linked_it->second;
			return linked_it->second;
		}

		shared_ptr<VariableEntity> out(new VariableEntity());
		out->name = name;
		out->type = type;
		out->linkage = linkage;
		out->order = program.next_order++;
		out->key = key;
		program.linked_variables[key] = out;
		program.ordered_variables.push_back(out);
		scope->variables[name] = out;
		return out;
	}

	shared_ptr<FunctionEntity> GetOrCreateFunctionEntity(Namespace* scope, const string& name, const TypePtr& type, LinkageKind linkage)
	{
		bool external = linkage == LK_EXTERNAL;
		string sig = DescribeType(type);
		string key = (external ? "E" : "I") + string(":") +
			(external ? EntityScopeKey(scope, true) : ("TU" + to_string(tu_index) + ":" + EntityScopeKey(scope, false))) +
			":" + name + ":" + sig;
		map<string, shared_ptr<FunctionEntity> >::iterator linked_it = program.linked_functions.find(key);
		if (linked_it != program.linked_functions.end())
		{
			scope->functions[name][sig] = linked_it->second;
			return linked_it->second;
		}

		shared_ptr<FunctionEntity> out(new FunctionEntity());
		out->name = name;
		out->type = type;
		out->linkage = linkage;
		out->order = program.next_order++;
		out->key = key;
		program.linked_functions[key] = out;
		program.ordered_functions.push_back(out);
		scope->functions[name][sig] = out;
		return out;
	}

	shared_ptr<StringLiteralObject> RegisterStringLiteral(const vector<unsigned char>& bytes, const TypePtr& type)
	{
		shared_ptr<StringLiteralObject> out(new StringLiteralObject());
		out->bytes = bytes;
		out->type = type;
		out->order = program.string_literals.size();
		program.string_literals.push_back(out);
		return out;
	}

	Expression ParseLiteralExpression(const MacroToken& token, bool register_strings)
	{
		Expression out;
		out.is_lvalue = false;

		if (token.type == PPT_PP_NUMBER)
		{
			ParsedNumberLiteral parsed = ParsePPNumberLiteral(token.data);
			if (!parsed.ok)
				FailAt(token, "invalid literal");
			if (parsed.is_integer)
			{
				EFundamentalType ft = ChooseIntegerType(parsed.int_info);
				out.type = MakeFundamental(FundamentalTypeToStringMap.at(ft));
				out.immediate = EncodeUnsignedLongLong(parsed.int_info.value, TypeSize(out.type));
				out.has_immediate = true;
				out.has_const_uint = true;
				out.const_uint = parsed.int_info.value;
				out.constant_truthy = parsed.int_info.value != 0;
				out.initializer_source = "LiteralExpression (TT_LITERAL:" + token.data + ")";
				out.debug_initializer_expression = "Immediate (VC_PRVALUE " + DescribeType(out.type) + " " + HexDump(out.immediate.data(), out.immediate.size()) + ")";
				return out;
			}

			long double value = strtold(token.data.c_str(), nullptr);
			if (parsed.float_type == FT_FLOAT)
			{
				float x = static_cast<float>(value);
				out.type = MakeFundamental("float");
				out.immediate.resize(4);
				memcpy(&out.immediate[0], &x, 4);
			}
			else if (parsed.float_type == FT_DOUBLE)
			{
				double x = static_cast<double>(value);
				out.type = MakeFundamental("double");
				out.immediate.resize(8);
				memcpy(&out.immediate[0], &x, 8);
			}
			else
			{
				long double x = value;
				out.type = MakeFundamental("long double");
				out.immediate.resize(sizeof(long double));
				memcpy(&out.immediate[0], &x, sizeof(long double));
				while (out.immediate.size() < 16)
					out.immediate.push_back(0);
			}
			out.has_immediate = true;
			out.constant_truthy = true;
			out.initializer_source = "LiteralExpression (TT_LITERAL:" + token.data + ")";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE " + DescribeType(out.type) + " " + HexDump(out.immediate.data(), out.immediate.size()) + ")";
			return out;
		}

		if (token.type == PPT_CHARACTER_LITERAL || token.type == PPT_USER_DEFINED_CHARACTER_LITERAL)
		{
			PPToken pp = ToPPToken(token);
			ParsedCharLiteral parsed = ParseCharacterLiteralToken(pp);
			if (!parsed.ok)
				FailAt(token, "invalid literal");
			out.type = MakeFundamental(FundamentalTypeToStringMap.at(parsed.type));
			out.immediate = EncodeUnsignedLongLong(parsed.value, TypeSize(out.type));
			out.has_immediate = true;
			out.has_const_uint = true;
			out.const_uint = parsed.value;
			out.constant_truthy = parsed.value != 0;
			out.initializer_source = "LiteralExpression (TT_LITERAL:" + token.data + ")";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE " + DescribeType(out.type) + " " + HexDump(out.immediate.data(), out.immediate.size()) + ")";
			return out;
		}

		if (token.type == PPT_STRING_LITERAL || token.type == PPT_USER_DEFINED_STRING_LITERAL)
		{
			PPToken pp = ToPPToken(token);
			ParsedStringLiteralToken parsed = ParseStringLiteralToken(pp);
			if (!parsed.ok)
				FailAt(token, "invalid literal");
			EncodedStringData encoded;
			if (!EncodeStringData(parsed.codepoints, parsed.encoding, encoded))
				FailAt(token, "invalid string literal");
			out.type = MakeArray(MakeFundamental(FundamentalTypeToStringMap.at(encoded.type)), true, encoded.num_elements);
			out.is_lvalue = true;
			out.has_immediate = true;
			out.immediate = encoded.bytes;
			out.constant_truthy = true;
			out.string_literal = register_strings ? RegisterStringLiteral(encoded.bytes, out.type) : shared_ptr<StringLiteralObject>();
			out.initializer_source = "LiteralExpression (TT_LITERAL:" + token.data + ")";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE " + DescribeType(out.type) + " " + HexDump(out.immediate.data(), out.immediate.size()) + ")";
			return out;
		}

		FailAt(token, "bad literal expression");
		return out;
	}

	Expression ParseExpression(Namespace* lookup_scope, bool register_strings)
	{
		if (AcceptKeyword("true"))
		{
			Expression out;
			out.type = MakeFundamental("bool");
			out.has_immediate = true;
			out.immediate.push_back(1);
			out.has_const_uint = true;
			out.const_uint = 1;
			out.constant_truthy = true;
			out.initializer_source = "LiteralExpression (KW_TRUE)";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE bool 01)";
			return out;
		}
		if (AcceptKeyword("false"))
		{
			Expression out;
			out.type = MakeFundamental("bool");
			out.has_immediate = true;
			out.immediate.push_back(0);
			out.has_const_uint = true;
			out.const_uint = 0;
			out.constant_truthy = false;
			out.initializer_source = "LiteralExpression (KW_FALSE)";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE bool 00)";
			return out;
		}
		if (AcceptKeyword("nullptr"))
		{
			Expression out;
			out.type = MakeFundamental("nullptr_t");
			out.has_immediate = true;
			out.immediate = ZeroBytes(8);
			out.constant_truthy = false;
			out.initializer_source = "LiteralExpression (KW_NULLPTR)";
			out.debug_initializer_expression = "Immediate (VC_PRVALUE nullptr_t 0000000000000000)";
			return out;
		}
		if (IsLiteralToken(PeekToken()))
			return ParseLiteralExpression(tokens[pos++], register_strings);
		if (AcceptPunc("("))
		{
			Expression out = ParseExpression(lookup_scope, register_strings);
			ExpectPunc(")");
			return out;
		}

		QualifiedName name = ParseQualifiedName();
		Namespace* base_scope = lookup_scope;
		if (!name.qualifiers.empty() || name.global)
			base_scope = ResolveQualifiedNamespace(name, lookup_scope).get();

		if (name.qualifiers.empty() && !name.global)
		{
			shared_ptr<VariableEntity> var = LookupVariableUnqualified(base_scope, name.name);
			if (var)
				return MakeVariableExpression(var, name.name);
			shared_ptr<FunctionEntity> fn = LookupFunctionUnqualified(base_scope, name.name);
			if (fn)
				return MakeFunctionExpression(fn, name.name);
		}
		else
		{
			set<const Namespace*> visited;
			shared_ptr<VariableEntity> var = LookupVariableQualified(base_scope, name.name, visited);
			if (var)
				return MakeVariableExpression(var, name.name);
			visited.clear();
			shared_ptr<FunctionEntity> fn = LookupFunctionQualified(base_scope, name.name, visited);
			if (fn)
				return MakeFunctionExpression(fn, name.name);
		}

		FailAt(PeekToken(), name.name + " not found");
		return Expression();
	}

	Expression MakeVariableExpression(const shared_ptr<VariableEntity>& var, const string& text)
	{
		Expression out;
		out.is_lvalue = true;
		out.variable = var;
		if (var->type->kind == Type::TK_LVALUE_REF || var->type->kind == Type::TK_RVALUE_REF)
		{
			out.type = var->type->nested;
			out.initializer_source = "IdExpression (" + text + ")";
			if (var->reference_target)
				out.variable = var->reference_target;
			if (var->reference_temp)
			{
				out.variable.reset();
				out.type = var->reference_temp->type;
				out.debug_initializer_expression = "VariableExpression (VC_LVALUE " + DescribeType(out.type) + " <temp>)";
			}
			else if (out.variable)
				out.debug_initializer_expression = "VariableExpression (VC_LVALUE " + DescribeType(out.type) + " " + out.variable->name + ")";
			if (var->is_constant)
			{
				out.constant_truthy = var->constant_truthy;
				out.has_const_uint = var->has_const_uint;
				out.const_uint = var->const_uint;
			}
			return out;
		}

		out.type = var->type;
		out.initializer_source = "IdExpression (" + text + ")";
		out.debug_initializer_expression = "VariableExpression (VC_LVALUE " + DescribeType(out.type) + " " + var->name + ")";
		if (var->is_constant)
		{
			out.constant_truthy = var->constant_truthy;
			out.has_const_uint = var->has_const_uint;
			out.const_uint = var->const_uint;
		}
		return out;
	}

	Expression MakeFunctionExpression(const shared_ptr<FunctionEntity>& fn, const string& text)
	{
		Expression out;
		out.is_lvalue = true;
		out.function = fn;
		out.type = fn->type;
		out.initializer_source = "IdExpression (" + text + ")";
		return out;
	}

	bool SameUnqualifiedType(const TypePtr& a, const TypePtr& b) const
	{
		return DescribeType(a) == DescribeType(b);
	}

	void EmitVariableLog(const string& name, const vector<string>& flags, LinkageKind linkage, const TypePtr& type,
		const string& initializer, const string& init_expr, bool is_constant)
	{
		cout << "LINKING: variable " << name << " " << JoinFlags(flags) << " "
			<< (linkage == LK_EXTERNAL ? "LK_EXTERNAL" : "LK_INTERNAL") << " "
			<< DescribeType(type) << " initializer=(" << initializer << ") initializer_expression=("
			<< init_expr << ") is_constant = " << (is_constant ? 1 : 0) << endl;
	}

	void EmitFunctionLog(const string& name, const vector<string>& flags, const TypePtr& type, bool defined)
	{
		cout << "LINKING: function " << name << " " << JoinFlags(flags) << " "
			<< DescribeType(type) << " defined=" << (defined ? 1 : 0) << endl;
	}

	string BuildVariableInitExprDesc(const shared_ptr<VariableEntity>& entity, const Expression& expr) const
	{
		if (!entity->has_initializer)
			return "null";
		if (entity->type->kind == Type::TK_LVALUE_REF || entity->type->kind == Type::TK_RVALUE_REF)
		{
			if (entity->reference_temp)
				return "VariableExpression (VC_LVALUE " + DescribeType(entity->reference_temp->type) + " <temp>)";
			if (entity->reference_target)
				return "VariableExpression (VC_LVALUE " + DescribeType(entity->reference_target->type) + " " + entity->reference_target->name + ")";
		}
		if (entity->init_from_function)
			return "FunctionAddress (VC_PRVALUE " + DescribeType(entity->type) + " " + entity->pointed_function->name + ")";
		if (entity->init_from_array_variable)
			return "ArrayVariablePointer (VC_PRVALUE " + DescribeType(entity->type) + " " + entity->pointed_variable->name + ")";
		if (!entity->init_bytes.empty())
		{
			TypePtr shown = entity->type;
			if (shown->kind == Type::TK_CV && shown->nested->kind == Type::TK_FUNDAMENTAL)
				shown = shown->nested;
			return "Immediate (VC_PRVALUE " + DescribeType(shown) + " " + HexDump(entity->init_bytes.data(), entity->init_bytes.size()) + ")";
		}
		return expr.debug_initializer_expression.empty() ? "null" : expr.debug_initializer_expression;
	}

	void CheckCompleteVariableType(const TypePtr& type, const MacroToken& token)
	{
		if (IsIncompleteType(type))
			FailAt(token, "variable defined with incomplete type");
	}

	bool CanDefaultInitialize(const TypePtr& type) const
	{
		if (type->kind == Type::TK_LVALUE_REF || type->kind == Type::TK_RVALUE_REF)
			return false;
		if (IsIncompleteType(type))
			return false;
		if (IsConstObjectType(type))
			return false;
		return true;
	}

	vector<unsigned char> ConvertArithmeticTo(const Expression& expr, const TypePtr& dest, const MacroToken& token)
	{
		if (!expr.has_immediate)
			FailAt(token, "initializer is not a constant expression");
		if (dest->kind == Type::TK_CV)
			return ConvertArithmeticTo(expr, dest->nested, token);
		if (dest->kind != Type::TK_FUNDAMENTAL)
			FailAt(token, "unsupported conversion");

		if (expr.type && expr.type->kind == Type::TK_FUNDAMENTAL)
		{
			if (dest->fundamental == "float")
			{
				float value = 0.0f;
				if (expr.type->fundamental == "float")
					memcpy(&value, &expr.immediate[0], 4);
				else if (expr.type->fundamental == "double")
				{
					double x = 0.0;
					memcpy(&x, &expr.immediate[0], 8);
					value = static_cast<float>(x);
				}
				else if (expr.type->fundamental == "long double")
				{
					long double x = 0.0;
					memcpy(&x, &expr.immediate[0], sizeof(long double));
					value = static_cast<float>(x);
				}
				else if (expr.has_const_uint)
				{
					value = static_cast<float>(expr.const_uint);
				}
				vector<unsigned char> out(4);
				memcpy(&out[0], &value, 4);
				return out;
			}
			if (dest->fundamental == "double")
			{
				double value = 0.0;
				if (expr.type->fundamental == "float")
				{
					float x = 0.0f;
					memcpy(&x, &expr.immediate[0], 4);
					value = static_cast<double>(x);
				}
				else if (expr.type->fundamental == "double")
				{
					memcpy(&value, &expr.immediate[0], 8);
				}
				else if (expr.type->fundamental == "long double")
				{
					long double x = 0.0;
					memcpy(&x, &expr.immediate[0], sizeof(long double));
					value = static_cast<double>(x);
				}
				else if (expr.has_const_uint)
				{
					value = static_cast<double>(expr.const_uint);
				}
				vector<unsigned char> out(8);
				memcpy(&out[0], &value, 8);
				return out;
			}
			if (dest->fundamental == "long double")
			{
				long double value = 0.0;
				if (expr.type->fundamental == "float")
				{
					float x = 0.0f;
					memcpy(&x, &expr.immediate[0], 4);
					value = static_cast<long double>(x);
				}
				else if (expr.type->fundamental == "double")
				{
					double x = 0.0;
					memcpy(&x, &expr.immediate[0], 8);
					value = static_cast<long double>(x);
				}
				else if (expr.type->fundamental == "long double")
				{
					memcpy(&value, &expr.immediate[0], sizeof(long double));
				}
				else if (expr.has_const_uint)
				{
					value = static_cast<long double>(expr.const_uint);
				}
				vector<unsigned char> out(sizeof(long double));
				memcpy(&out[0], &value, sizeof(long double));
				while (out.size() < 16)
					out.push_back(0);
				return out;
			}
		}

		return ConvertImmediateBytes(expr.immediate, expr.immediate.size(), dest->fundamental);
	}

	void InitializeReferenceEntity(const shared_ptr<VariableEntity>& entity, const Expression& expr, const MacroToken& token)
	{
		TypePtr referred = entity->type->nested;
		entity->has_initializer = true;
		if (expr.variable)
		{
			entity->reference_target = expr.variable;
			entity->init_from_variable = true;
			entity->is_constant = expr.variable->is_constant;
			entity->constant_truthy = expr.variable->constant_truthy;
			entity->has_const_uint = expr.variable->has_const_uint;
			entity->const_uint = expr.variable->const_uint;
			entity->init_bytes.clear();
			return;
		}
		if (expr.function || expr.string_literal)
			FailAt(token, "invalid type for reference to");
		if (!expr.has_immediate)
			FailAt(token, "invalid type for reference to");
		if (!(entity->const_spec || (referred->kind == Type::TK_CV && referred->is_const)))
			FailAt(token, "invalid type for reference to");
		shared_ptr<TempObject> temp(new TempObject());
		temp->type = referred;
		temp->bytes = ConvertArithmeticTo(expr, referred, token);
		temp->order = program.temporaries.size();
		program.temporaries.push_back(temp);
		entity->reference_temp = temp;
		entity->init_from_temp = true;
		entity->is_constant = false;
		entity->constant_truthy = false;
	}

	void ApplyVariableInitializer(const shared_ptr<VariableEntity>& entity, const Expression& expr, const MacroToken& token)
	{
		entity->has_initializer = true;
		if (entity->type->kind == Type::TK_LVALUE_REF || entity->type->kind == Type::TK_RVALUE_REF)
		{
			InitializeReferenceEntity(entity, expr, token);
			return;
		}

		TypePtr dest = entity->type;
		if (dest->kind == Type::TK_ARRAY)
		{
			if (expr.string_literal)
			{
				TypePtr elem = dest->nested;
				TypePtr src_elem = expr.type->nested;
				if ((DescribeType(elem) == DescribeType(src_elem)) ||
					((DescribeType(elem) == "signed char" || DescribeType(elem) == "unsigned char") && DescribeType(src_elem) == "char"))
				{
					vector<unsigned char> data = expr.immediate;
					size_t dest_size = TypeSize(dest);
					if (data.size() < dest_size)
						data.resize(dest_size, 0);
					entity->init_bytes = data;
					entity->is_constant = false;
					return;
				}
			}
			FailAt(token, "unsupported array initializer");
		}

		if (dest->kind == Type::TK_POINTER)
		{
			if (expr.function)
			{
				entity->init_from_function = true;
				entity->pointed_function = expr.function;
				entity->is_constant = false;
				return;
			}
			if (expr.variable && expr.variable->type->kind == Type::TK_ARRAY)
			{
				entity->init_from_array_variable = true;
				entity->pointed_variable = expr.variable;
				entity->is_constant = entity->constexpr_spec;
				entity->constant_truthy = true;
				return;
			}
			if (expr.string_literal)
			{
				entity->init_bytes = ZeroBytes(8);
				entity->is_constant = false;
				return;
			}
			if (expr.has_immediate)
			{
				entity->init_bytes = ConvertImmediateBytes(expr.immediate, expr.immediate.size(), "nullptr_t");
				entity->is_constant = false;
				return;
			}
		}

		if (expr.variable && entity->constexpr_spec)
		{
			if (!expr.variable->is_constant)
				FailAt(token, "initializer is not a constant expression");
			entity->init_bytes = expr.variable->init_bytes;
			entity->is_constant = true;
			entity->constant_truthy = expr.variable->constant_truthy;
			entity->has_const_uint = expr.variable->has_const_uint;
			entity->const_uint = expr.variable->const_uint;
			return;
		}

		if (expr.has_immediate)
		{
			entity->init_bytes = ConvertArithmeticTo(expr, dest, token);
			entity->constant_truthy = expr.constant_truthy;
			if (expr.has_const_uint)
			{
				entity->has_const_uint = true;
				entity->const_uint = expr.const_uint;
			}
			entity->is_constant = entity->const_spec || entity->constexpr_spec;
			return;
		}

		if (expr.variable)
		{
			if (!expr.variable->is_constant)
				FailAt(token, "initializer is not a constant expression");
			entity->init_bytes = expr.variable->init_bytes;
			entity->is_constant = entity->const_spec || entity->constexpr_spec;
			entity->constant_truthy = expr.variable->constant_truthy;
			entity->has_const_uint = expr.variable->has_const_uint;
			entity->const_uint = expr.variable->const_uint;
			return;
		}

		FailAt(token, "unsupported initializer");
	}

	void ParseDeclaration()
	{
		if (AcceptPunc(";"))
			return;

		if (AcceptKeyword("static_assert"))
		{
			ExpectPunc("(");
			Expression expr = ParseExpression(current, false);
			ExpectPunc(",");
			ExpectLiteral();
			ExpectPunc(")");
			ExpectPunc(";");
			if (!expr.constant_truthy)
				FailAt(tokens[pos - 1], "static_assert on non-constant expression");
			return;
		}

		if (IsKeyword(PeekToken(), "inline"))
		{
			size_t save = pos;
			MacroToken tok = PeekToken();
			++pos;
			if (IsKeyword(PeekToken(), "namespace"))
			{
				pos = save;
				ParseNamespaceDefinition();
				return;
			}
			pos = save;
		}

		if (IsKeyword(PeekToken(), "namespace"))
		{
			if (IsIdentifierLike(PeekToken(1)) && IsPunc(PeekToken(2), "="))
				ParseNamespaceAliasDefinition();
			else
				ParseNamespaceDefinition();
			return;
		}

		if (IsKeyword(PeekToken(), "using"))
		{
			if (IsKeyword(PeekToken(1), "namespace"))
				ParseUsingDirective();
			else if (IsIdentifierLike(PeekToken(1)) && IsPunc(PeekToken(2), "="))
				ParseAliasDeclaration();
			else
				ParseUsingDeclaration();
			return;
		}

		ParseSimpleOrFunctionDefinition();
	}

	void ParseNamespaceDefinition()
	{
		bool is_inline = AcceptKeyword("inline");
		MacroToken tok = PeekToken();
		ExpectKeyword("namespace");
		shared_ptr<Namespace> inner;
		if (IsIdentifierLike(PeekToken()))
		{
			string name = ExpectIdentifier();
			inner = GetOrCreateNamedNamespace(current, name, is_inline, tok);
		}
		else
		{
			inner = GetOrCreateUnnamedNamespace(current, is_inline);
		}
		ExpectPunc("{");
		Namespace* saved = current;
		current = inner.get();
		while (!IsPunc(PeekToken(), "}"))
			ParseDeclaration();
		ExpectPunc("}");
		current = saved;
	}

	void ParseNamespaceAliasDefinition()
	{
		MacroToken tok = PeekToken();
		ExpectKeyword("namespace");
		string alias = ExpectIdentifier();
		if (current->named_namespaces.count(alias))
			FailAt(tok, alias + " is an original-namespace-name");
		EnsureNoNamespaceConflict(current, alias, tok);
		ExpectPunc("=");
		QualifiedName target = ParseQualifiedName();
		ExpectPunc(";");
		current->namespace_aliases[alias] = ResolveNamespaceName(target, current);
	}

	void ParseUsingDirective()
	{
		ExpectKeyword("using");
		ExpectKeyword("namespace");
		QualifiedName name = ParseQualifiedName();
		ExpectPunc(";");
		current->using_directives.push_back(ResolveNamespaceName(name, current));
	}

	void ParseAliasDeclaration()
	{
		ExpectKeyword("using");
		string name = ExpectIdentifier();
		ExpectPunc("=");
		TypePtr type = ParseTypeId(current);
		ExpectPunc(";");
		current->types[name] = type;
	}

	void ParseUsingDeclaration()
	{
		MacroToken tok = PeekToken();
		ExpectKeyword("using");
		QualifiedName name = ParseQualifiedName();
		ExpectPunc(";");
		if (name.qualifiers.empty() && !name.global)
			FailAt(tok, name.name + " not found");
		shared_ptr<Namespace> scope = ResolveQualifiedNamespace(name, current);
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
		map<string, map<string, shared_ptr<FunctionEntity> > >::const_iterator fit = scope->functions.find(name.name);
		if (fit != scope->functions.end())
		{
			current->function_aliases[name.name] = fit->second;
			return;
		}
		FailAt(tok, name.name + " not found");
	}

	void ParseSimpleOrFunctionDefinition()
	{
		DeclSpec spec = ParseDeclSpecifierSeq(true, true, current);
		ParsedDeclarator decl = ParseDeclarator(current);
		TypePtr type = decl.apply(spec.base_type);
		if (!type)
		{
			if (spec.base_type->kind == Type::TK_LVALUE_REF || spec.base_type->kind == Type::TK_RVALUE_REF)
				FailAt(decl.root_token, "reference to reference in declarator");
			if ((spec.base_type->kind == Type::TK_LVALUE_REF || spec.base_type->kind == Type::TK_RVALUE_REF) && StartsPtrOperator())
				FailAt(decl.root_token, "pointer to that type not allowed");
			FailAt(decl.root_token, "invalid type for reference to");
		}

		Namespace* target_scope = decl.lookup_scope ? decl.lookup_scope : current;
		if (!ScopeIsWithin(current, target_scope) && current != global.get())
			FailAt(decl.root_token, "qualified name not from enclosed namespace");

		if (type->kind == Type::TK_FUNCTION && IsPunc(PeekToken(), "{"))
		{
			shared_ptr<FunctionEntity> fn = DeclareFunction(target_scope, decl.name.name, type, spec, true, decl.root_token);
			ExpectPunc("{");
			ExpectPunc("}");
			return;
		}

		vector<pair<ParsedDeclarator, bool> > declarators;
		Expression first_initializer;
		bool first_has_initializer = false;
		if (AcceptPunc("="))
		{
			first_has_initializer = true;
			first_initializer = ParseExpression(target_scope, true);
		}
		declarators.push_back(make_pair(decl, first_has_initializer));
		vector<Expression> init_exprs;
		init_exprs.push_back(first_initializer);

		while (AcceptPunc(","))
		{
			ParsedDeclarator more = ParseDeclarator(current);
			bool has_init = false;
			Expression init;
			Namespace* more_scope = more.lookup_scope ? more.lookup_scope : current;
			if (AcceptPunc("="))
			{
				has_init = true;
				init = ParseExpression(more_scope, true);
			}
			declarators.push_back(make_pair(more, has_init));
			init_exprs.push_back(init);
		}
		ExpectPunc(";");

		for (size_t i = 0; i < declarators.size(); ++i)
		{
			TypePtr decl_type = declarators[i].first.apply(spec.base_type);
			if (!decl_type)
			{
				if (spec.base_type->kind == Type::TK_LVALUE_REF || spec.base_type->kind == Type::TK_RVALUE_REF)
					FailAt(declarators[i].first.root_token, "reference to reference in declarator");
				FailAt(declarators[i].first.root_token, "pointer to that type not allowed");
			}
			Namespace* decl_scope = declarators[i].first.lookup_scope ? declarators[i].first.lookup_scope : current;
			if (!ScopeIsWithin(current, decl_scope) && current != global.get())
				FailAt(declarators[i].first.root_token, "qualified name not from enclosed namespace");
			if (spec.is_typedef)
			{
				if ((decl_type->kind == Type::TK_POINTER && (decl_type->nested->kind == Type::TK_LVALUE_REF || decl_type->nested->kind == Type::TK_RVALUE_REF)))
					FailAt(declarators[i].first.root_token, "pointer to that type not allowed");
				if (decl_type->kind == Type::TK_LVALUE_REF && IsVoidType(decl_type->nested))
					FailAt(declarators[i].first.root_token, "invalid type for reference to");
				decl_scope->types[declarators[i].first.name.name] = decl_type;
				continue;
			}
			if (decl_type->kind == Type::TK_FUNCTION)
			{
				DeclareFunction(decl_scope, declarators[i].first.name.name, decl_type, spec, false, declarators[i].first.root_token);
			}
			else
			{
				DeclareVariable(decl_scope, declarators[i].first.name.name, decl_type, spec, declarators[i].second, init_exprs[i], declarators[i].first.root_token);
			}
		}
	}

	shared_ptr<FunctionEntity> DeclareFunction(Namespace* scope, const string& name, const TypePtr& type, const DeclSpec& spec, bool defined, const MacroToken& token)
	{
		EnsureFunctionNameAvailable(scope, name, token);
		LinkageKind linkage = ComputeLinkage(spec, scope, type);
		string sig = DescribeType(type);
		shared_ptr<FunctionEntity> entity = GetOrCreateFunctionEntity(scope, name, type, linkage);
		if (defined && entity->defined && !entity->inline_spec && !spec.is_inline)
			FailAt(token, "function " + name + " already defined");
		entity->defined = entity->defined || defined;
		entity->inline_spec = entity->inline_spec || spec.is_inline;
		EmitFunctionLog(name, spec.source_flags, type, defined);
		return entity;
	}

	shared_ptr<VariableEntity> DeclareVariable(Namespace* scope, const string& name, const TypePtr& type, const DeclSpec& spec,
		bool has_initializer, const Expression& expr, const MacroToken& token)
	{
		EnsureVariableNameAvailable(scope, name, token);
		LinkageKind linkage = ComputeLinkage(spec, scope, type);
		if (scope->variables.count(name))
			linkage = scope->variables[name]->linkage;
		shared_ptr<VariableEntity> entity = GetOrCreateVariableEntity(scope, name, type, linkage);
		entity->type = MergeRedeclarationTypes(entity->type, type);
		entity->constexpr_spec = entity->constexpr_spec || spec.is_constexpr;
		entity->const_spec = entity->const_spec || spec.is_const;
		entity->inline_spec = entity->inline_spec || spec.is_inline;

		bool is_definition = !spec.is_extern || has_initializer;
		string init_desc = spec.is_extern && !has_initializer ? "null" : (has_initializer ? "CopyInitializer " : "DefaultInitializer");
		string init_expr_desc = "null";
		bool log_constant = false;

		if (has_initializer)
		{
			init_desc = "CopyInitializer (" + InitializerSource(expr, token) + ")";
			init_expr_desc = expr.debug_initializer_expression.empty() ? "null" : expr.debug_initializer_expression;
		}

		if (is_definition)
		{
			if (has_initializer && type->kind == Type::TK_ARRAY && !type->has_bound && expr.string_literal)
			{
				entity->type = MakeArray(type->nested, true, expr.type->bound);
			}
			CheckCompleteVariableType(entity->type, token);
			if (!has_initializer)
			{
				if (!CanDefaultInitialize(type))
					FailAt(token, "type cannot be default initialized");
				entity->defined = true;
				entity->init_bytes = ZeroBytes(TypeSize(type));
			}
			else
			{
				ApplyVariableInitializer(entity, expr, token);
				entity->defined = true;
				init_expr_desc = BuildVariableInitExprDesc(entity, expr);
			}
		}

		log_constant = entity->is_constant;
		if (!(scope->is_named && !is_definition && !has_initializer))
			EmitVariableLog(name, spec.source_flags, linkage, entity->type, init_desc, init_expr_desc, log_constant);
		return entity;
	}

	string InitializerSource(const Expression& expr, const MacroToken& token)
	{
		if (!expr.initializer_source.empty())
			return expr.initializer_source;
		return "LiteralExpression (" + token.data + ")";
	}
};

shared_ptr<Namespace> BuildGlobalNamespace()
{
	shared_ptr<Namespace> global(new Namespace());
	global->is_named = false;
	global->identity_key = "::";
	global->external_key = "::";
	return global;
}

vector<unsigned char> BuildProgramImage(ProgramState& program)
{
	vector<unsigned char> image;
	image.push_back('P');
	image.push_back('A');
	image.push_back('8');
	image.push_back(0);

	struct Block1Item
	{
		size_t order = 0;
		bool is_function = false;
		shared_ptr<VariableEntity> variable;
		shared_ptr<FunctionEntity> function;
	};

	vector<Block1Item> block1;
	for (size_t i = 0; i < program.ordered_variables.size(); ++i)
	{
		if (program.ordered_variables[i]->defined)
		{
			Block1Item item;
			item.order = program.ordered_variables[i]->order;
			item.variable = program.ordered_variables[i];
			block1.push_back(item);
		}
	}
	for (size_t i = 0; i < program.ordered_functions.size(); ++i)
	{
		Block1Item item;
		item.order = program.ordered_functions[i]->order;
		item.is_function = true;
		item.function = program.ordered_functions[i];
		block1.push_back(item);
	}

	for (size_t i = 0; i < block1.size(); ++i)
	{
		for (size_t j = i + 1; j < block1.size(); ++j)
		{
			if (block1[j].order < block1[i].order)
				swap(block1[i], block1[j]);
		}
	}

	for (size_t i = 0; i < block1.size(); ++i)
	{
		if (block1[i].is_function)
		{
			AlignImage(image, 4);
			block1[i].function->image_offset = image.size();
			image.push_back('f');
			image.push_back('u');
			image.push_back('n');
			image.push_back(0);
		}
		else
		{
			AlignImage(image, TypeAlign(block1[i].variable->type));
			block1[i].variable->image_offset = image.size();
			image.resize(image.size() + TypeSize(block1[i].variable->type), 0);
		}
	}

	for (size_t i = 0; i < program.temporaries.size(); ++i)
	{
		AlignImage(image, TypeAlign(program.temporaries[i]->type));
		program.temporaries[i]->image_offset = image.size();
		image.insert(image.end(), program.temporaries[i]->bytes.begin(), program.temporaries[i]->bytes.end());
	}

	for (size_t i = 0; i < program.string_literals.size(); ++i)
	{
		AlignImage(image, TypeAlign(program.string_literals[i]->type));
		program.string_literals[i]->image_offset = image.size();
		image.insert(image.end(), program.string_literals[i]->bytes.begin(), program.string_literals[i]->bytes.end());
	}

	for (size_t i = 0; i < program.ordered_variables.size(); ++i)
	{
		shared_ptr<VariableEntity> var = program.ordered_variables[i];
		if (!var->defined)
			continue;
		size_t offset = var->image_offset;
		vector<unsigned char> bytes;
		if (var->type->kind == Type::TK_LVALUE_REF || var->type->kind == Type::TK_RVALUE_REF)
		{
			unsigned long long addr = 0;
			if (var->reference_target)
				addr = var->reference_target->image_offset;
			else if (var->reference_temp)
				addr = var->reference_temp->image_offset;
			bytes = EncodeUnsignedLongLong(addr, 8);
		}
		else if (var->init_from_function)
		{
			bytes = EncodeUnsignedLongLong(var->pointed_function->image_offset, 8);
		}
		else if (var->init_from_array_variable)
		{
			bytes = EncodeUnsignedLongLong(var->pointed_variable->image_offset, 8);
		}
		else if (var->has_initializer)
		{
			bytes = var->init_bytes;
		}
		else
		{
			bytes = ZeroBytes(TypeSize(var->type));
		}
		if (bytes.size() < TypeSize(var->type))
			bytes.resize(TypeSize(var->type), 0);
		for (size_t j = 0; j < bytes.size(); ++j)
			image[offset + j] = bytes[j];
	}

	return image;
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

		pair<string, string> build = BuildDateTimeLiterals();
		string author_literal = EscapeStringLiteral("OpenAI Codex");
		ProgramState program;

		for (size_t i = 2; i < args.size(); ++i)
		{
			shared_ptr<Namespace> global = BuildGlobalNamespace();
			Preprocessor preproc(build.first, build.second, author_literal);
			vector<MacroToken> tokens = preproc.ProcessSourceFileTokens(args[i]);
			Parser parser(tokens, global, program, static_cast<int>(i - 2));
			parser.ParseTranslationUnit();
		}

		vector<unsigned char> image = BuildProgramImage(program);
		ofstream out(args[1], ios::binary);
		if (!out)
			throw runtime_error("cannot open output file: " + args[1]);
		if (!image.empty())
			out.write(reinterpret_cast<const char*>(&image[0]), image.size());
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}
}
