// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <cstdint>

using namespace std;

bool PA3Mock_IsDefinedIdentifier(const string& identifier)
{
	if (identifier.empty())
		return false;
	return static_cast<unsigned char>(identifier[0]) % 2;
}

#define main posttoken_embedded_main
#include "posttoken.cpp"
#undef main

struct Value
{
	bool uns = false;
	uint64_t bits = 0;
};

int64_t AsSigned(const Value& v) { return static_cast<int64_t>(v.bits); }
uint64_t AsUnsigned(const Value& v) { return v.bits; }
bool IsTrue(const Value& v) { return v.bits != 0; }
Value MakeSigned(int64_t x) { Value v; v.uns = false; v.bits = static_cast<uint64_t>(x); return v; }
Value MakeUnsigned(uint64_t x) { Value v; v.uns = true; v.bits = x; return v; }

Value ConvertLiteralToValue(EFundamentalType type, const vector<unsigned char>& bytes)
{
	switch (type)
	{
	case FT_BOOL: return MakeSigned(bytes[0] ? 1 : 0);
	case FT_CHAR:
		{ char x; memcpy(&x, bytes.data(), 1); return MakeSigned(x); }
	case FT_WCHAR_T:
		{ wchar_t x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_CHAR16_T:
		{ char16_t x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_CHAR32_T:
		{ char32_t x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_SIGNED_CHAR:
		{ signed char x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_SHORT_INT:
		{ short x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_INT:
		{ int x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_LONG_INT:
		{ long x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_LONG_LONG_INT:
		{ long long x; memcpy(&x, bytes.data(), sizeof(x)); return MakeSigned(x); }
	case FT_UNSIGNED_CHAR:
		{ unsigned char x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_UNSIGNED_SHORT_INT:
		{ unsigned short x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_UNSIGNED_INT:
		{ unsigned int x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_UNSIGNED_LONG_INT:
		{ unsigned long x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	case FT_UNSIGNED_LONG_LONG_INT:
		{ unsigned long long x; memcpy(&x, bytes.data(), sizeof(x)); return MakeUnsigned(x); }
	default: throw runtime_error("non-integral literal");
	}
}

enum class TokKind { value, ident, op };
struct Tok
{
	TokKind kind;
	string text;
	Value value;
	ETokenType op = KW_AUTO;

	Tok() {}
	Tok(TokKind kind, const string& text, Value value, ETokenType op)
		: kind(kind), text(text), value(value), op(op)
	{}
};

struct Parser
{
	vector<Tok> toks;
	size_t pos = 0;

	Parser() {}
	explicit Parser(const vector<Tok>& toks) : toks(toks) {}

	bool eof() const { return pos >= toks.size(); }
	bool match(ETokenType op) { if (!eof() && toks[pos].kind == TokKind::op && toks[pos].op == op) { ++pos; return true; } return false; }
	bool match_ident(const string& s) { if (!eof() && toks[pos].kind == TokKind::ident && toks[pos].text == s) { ++pos; return true; } return false; }
	Tok take() { return toks[pos++]; }

	Value parse_expr(bool eval) { return parse_cond(eval); }

	Value parse_primary(bool eval)
	{
		if (match(OP_LPAREN))
		{
			Value v = parse_expr(eval);
			if (!match(OP_RPAREN)) throw runtime_error("missing )");
			return v;
		}
		if (match_ident("defined"))
		{
			string id;
			if (match(OP_LPAREN))
			{
				if (eof() || toks[pos].kind != TokKind::ident) throw runtime_error("defined");
				id = take().text;
				if (!match(OP_RPAREN)) throw runtime_error("defined");
			}
			else
			{
				if (eof() || toks[pos].kind != TokKind::ident) throw runtime_error("defined");
				id = take().text;
			}
			return MakeSigned(eval && PA3Mock_IsDefinedIdentifier(id) ? 1 : 0);
		}
		if (eof()) throw runtime_error("eof");
		Tok t = take();
		if (t.kind == TokKind::value) return t.value;
		if (t.kind == TokKind::ident)
		{
			if (t.text == "true") return MakeSigned(1);
			if (t.text == "false") return MakeSigned(0);
			return MakeSigned(0);
		}
		throw runtime_error("primary");
	}

	Value parse_unary(bool eval)
	{
		if (match(OP_PLUS)) return parse_unary(eval);
		if (match(OP_MINUS)) { Value v = parse_unary(eval); return !eval ? (v.uns ? MakeUnsigned(0) : MakeSigned(0)) : (v.uns ? MakeUnsigned(0 - AsUnsigned(v)) : MakeSigned(-AsSigned(v))); }
		if (match(OP_LNOT)) { Value v = parse_unary(eval); return MakeSigned(eval ? !IsTrue(v) : 0); }
		if (match(OP_COMPL)) { Value v = parse_unary(eval); return v.uns ? MakeUnsigned(eval ? ~AsUnsigned(v) : 0) : MakeSigned(eval ? ~AsSigned(v) : 0); }
		return parse_primary(eval);
	}

	Value parse_mul(bool eval)
	{
		Value lhs = parse_unary(eval);
		while (!eof() && toks[pos].kind == TokKind::op && (toks[pos].op == OP_STAR || toks[pos].op == OP_DIV || toks[pos].op == OP_MOD))
		{
			ETokenType op = take().op;
			Value rhs = parse_unary(eval);
			if (!eval) { lhs = lhs.uns || rhs.uns ? MakeUnsigned(0) : MakeSigned(0); continue; }
			if ((op == OP_DIV || op == OP_MOD) && AsUnsigned(rhs) == 0) throw runtime_error("div0");
			if (!lhs.uns && !rhs.uns && (op == OP_DIV || op == OP_MOD) && AsSigned(lhs) == INT64_MIN && AsSigned(rhs) == -1)
				throw runtime_error("overflow");
			if (lhs.uns || rhs.uns)
			{
				if (op == OP_STAR) lhs = MakeUnsigned(AsUnsigned(lhs) * AsUnsigned(rhs));
				else if (op == OP_DIV) lhs = MakeUnsigned(AsUnsigned(lhs) / AsUnsigned(rhs));
				else lhs = MakeUnsigned(AsUnsigned(lhs) % AsUnsigned(rhs));
			}
			else
			{
				if (op == OP_STAR) lhs = MakeSigned(AsSigned(lhs) * AsSigned(rhs));
				else if (op == OP_DIV) lhs = MakeSigned(AsSigned(lhs) / AsSigned(rhs));
				else lhs = MakeSigned(AsSigned(lhs) % AsSigned(rhs));
			}
		}
		return lhs;
	}

	Value parse_add(bool eval)
	{
		Value lhs = parse_mul(eval);
		while (!eof() && toks[pos].kind == TokKind::op && (toks[pos].op == OP_PLUS || toks[pos].op == OP_MINUS))
		{
			ETokenType op = take().op;
			Value rhs = parse_mul(eval);
			if (!eval) { lhs = lhs.uns || rhs.uns ? MakeUnsigned(0) : MakeSigned(0); continue; }
			if (lhs.uns || rhs.uns)
				lhs = op == OP_PLUS ? MakeUnsigned(AsUnsigned(lhs) + AsUnsigned(rhs)) : MakeUnsigned(AsUnsigned(lhs) - AsUnsigned(rhs));
			else
				lhs = op == OP_PLUS ? MakeSigned(AsSigned(lhs) + AsSigned(rhs)) : MakeSigned(AsSigned(lhs) - AsSigned(rhs));
		}
		return lhs;
	}

	Value parse_shift(bool eval)
	{
		Value lhs = parse_add(eval);
		while (!eof() && toks[pos].kind == TokKind::op && (toks[pos].op == OP_LSHIFT || toks[pos].op == OP_RSHIFT))
		{
			ETokenType op = take().op;
			Value rhs = parse_add(eval);
			if (!eval) { lhs = lhs.uns ? MakeUnsigned(0) : MakeSigned(0); continue; }
			int64_t sh = rhs.uns ? static_cast<int64_t>(rhs.bits) : AsSigned(rhs);
			if (sh < 0 || sh >= 64) throw runtime_error("shift");
			if (op == OP_LSHIFT) lhs = lhs.uns ? MakeUnsigned(AsUnsigned(lhs) << sh) : MakeSigned(static_cast<int64_t>(AsUnsigned(lhs) << sh));
			else lhs = lhs.uns ? MakeUnsigned(AsUnsigned(lhs) >> sh) : MakeSigned(AsSigned(lhs) >> sh);
		}
		return lhs;
	}

	template<typename Next>
	Value cmp_chain(bool eval, Next next, const vector<ETokenType>& ops)
	{
		Value lhs = (this->*next)(eval);
		while (!eof() && toks[pos].kind == TokKind::op && find(ops.begin(), ops.end(), toks[pos].op) != ops.end())
		{
			ETokenType op = take().op;
			Value rhs = (this->*next)(eval);
			if (!eval) { lhs = MakeSigned(0); continue; }
			bool res;
			if (lhs.uns || rhs.uns)
			{
				uint64_t a = AsUnsigned(lhs), b = AsUnsigned(rhs);
				res = op == OP_LT ? a < b : op == OP_GT ? a > b : op == OP_LE ? a <= b : op == OP_GE ? a >= b : op == OP_EQ ? a == b : a != b;
			}
			else
			{
				int64_t a = AsSigned(lhs), b = AsSigned(rhs);
				res = op == OP_LT ? a < b : op == OP_GT ? a > b : op == OP_LE ? a <= b : op == OP_GE ? a >= b : op == OP_EQ ? a == b : a != b;
			}
			lhs = MakeSigned(res ? 1 : 0);
		}
		return lhs;
	}

	Value parse_rel(bool eval) { return cmp_chain(eval, &Parser::parse_shift, {OP_LT, OP_GT, OP_LE, OP_GE}); }
	Value parse_eq(bool eval) { return cmp_chain(eval, &Parser::parse_rel, {OP_EQ, OP_NE}); }

	Value parse_band(bool eval)
	{
		Value lhs = parse_eq(eval);
		while (match(OP_AMP)) { Value rhs = parse_eq(eval); lhs = !eval ? (lhs.uns || rhs.uns ? MakeUnsigned(0) : MakeSigned(0)) : (lhs.uns || rhs.uns ? MakeUnsigned(AsUnsigned(lhs) & AsUnsigned(rhs)) : MakeSigned(AsSigned(lhs) & AsSigned(rhs))); }
		return lhs;
	}
	Value parse_xor(bool eval)
	{
		Value lhs = parse_band(eval);
		while (match(OP_XOR)) { Value rhs = parse_band(eval); lhs = !eval ? (lhs.uns || rhs.uns ? MakeUnsigned(0) : MakeSigned(0)) : (lhs.uns || rhs.uns ? MakeUnsigned(AsUnsigned(lhs) ^ AsUnsigned(rhs)) : MakeSigned(AsSigned(lhs) ^ AsSigned(rhs))); }
		return lhs;
	}
	Value parse_bor(bool eval)
	{
		Value lhs = parse_xor(eval);
		while (match(OP_BOR)) { Value rhs = parse_xor(eval); lhs = !eval ? (lhs.uns || rhs.uns ? MakeUnsigned(0) : MakeSigned(0)) : (lhs.uns || rhs.uns ? MakeUnsigned(AsUnsigned(lhs) | AsUnsigned(rhs)) : MakeSigned(AsSigned(lhs) | AsSigned(rhs))); }
		return lhs;
	}
	Value parse_land(bool eval)
	{
		Value lhs = parse_bor(eval);
		while (match(OP_LAND))
		{
			bool do_rhs = eval && !(!IsTrue(lhs));
			Value rhs = parse_bor(do_rhs);
			lhs = MakeSigned(eval ? (IsTrue(lhs) && IsTrue(rhs)) : 0);
		}
		return lhs;
	}
	Value parse_lor(bool eval)
	{
		Value lhs = parse_land(eval);
		while (match(OP_LOR))
		{
			bool do_rhs = eval && !IsTrue(lhs);
			Value rhs = parse_land(do_rhs);
			lhs = MakeSigned(eval ? (IsTrue(lhs) || IsTrue(rhs)) : 0);
		}
		return lhs;
	}
	Value common(Value a, Value b) { return a.uns || b.uns ? MakeUnsigned(a.uns ? a.bits : static_cast<uint64_t>(AsSigned(a))) : MakeSigned(0); }
	Value parse_cond(bool eval)
	{
		Value cond = parse_lor(eval);
		if (!match(OP_QMARK)) return cond;
		bool choose_true = eval && IsTrue(cond);
		Value t = parse_cond(choose_true);
		if (!match(OP_COLON)) throw runtime_error(":");
		Value f = parse_cond(eval && !choose_true);
		if (!eval) return common(t, f);
		Value sample = common(t, f);
		return choose_true ? (sample.uns ? MakeUnsigned(t.bits) : MakeSigned(AsSigned(t))) : (sample.uns ? MakeUnsigned(f.bits) : MakeSigned(AsSigned(f)));
	}
};

bool line_to_tokens(const vector<PPToken>& line, vector<Tok>& out)
{
	for (const PPToken& pp : line)
	{
		if (pp.kind == PPKind::Whitespace) continue;
		if (pp.kind == PPKind::Identifier)
		{
			auto it = StringToTokenTypeMap.find(pp.source);
			if (it != StringToTokenTypeMap.end() && (pp.source == "and" || pp.source == "or" || pp.source == "not" || pp.source == "compl" || pp.source == "bitand" || pp.source == "bitor" || pp.source == "xor"))
				out.push_back(Tok(TokKind::op, pp.source, Value(), it->second));
			else
				out.push_back(Tok(TokKind::ident, pp.source, Value(), KW_AUTO));
		}
		else if (pp.kind == PPKind::PreprocessingOpOrPunc)
		{
			auto it = StringToTokenTypeMap.find(pp.source);
			if (it == StringToTokenTypeMap.end()) return false;
			out.push_back(Tok(TokKind::op, pp.source, Value(), it->second));
		}
		else if (pp.kind == PPKind::PPNumber)
		{
			string prefix, uds; EFundamentalType type; vector<unsigned char> bytes;
			if (!parse_integer_literal(pp.source, prefix, uds, type, bytes) || !uds.empty()) return false;
			out.push_back(Tok(TokKind::value, pp.source, ConvertLiteralToValue(type, bytes), KW_AUTO));
		}
		else if (pp.kind == PPKind::CharacterLiteral)
		{
			StringData d; string uds; bool is_char = false;
			if (!decode_string_or_char(pp.source, false, d, uds, is_char) || !is_char || !uds.empty()) return false;
			out.push_back(Tok(TokKind::value, pp.source, ConvertLiteralToValue(d.type, d.bytes), KW_AUTO));
		}
		else
			return false;
	}
	return true;
}

int main()
{
	try
	{
		ostringstream oss; oss << cin.rdbuf();
		PPTokenizer tokenizer;
		vector<PPToken> pp = tokenizer.run(oss.str());
		vector<PPToken> line;
		for (const PPToken& tok : pp)
		{
			if (tok.kind == PPKind::NewLine)
			{
				bool nonempty = false;
				for (const PPToken& t : line) if (t.kind != PPKind::Whitespace) nonempty = true;
				if (nonempty)
				{
					try
					{
						vector<Tok> toks;
						if (!line_to_tokens(line, toks)) throw runtime_error("invalid");
						Parser p{toks};
						Value v = p.parse_expr(true);
						if (!p.eof()) throw runtime_error("trailing");
						cout << (v.uns ? to_string(v.bits) + "u" : to_string(AsSigned(v))) << endl;
					}
					catch (...)
					{
						cout << "error" << endl;
					}
				}
				line.clear();
			}
			else
				line.push_back(tok);
		}
		cout << "eof" << endl;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
