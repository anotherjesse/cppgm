// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <utility>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <ctime>
#include <limits>

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

#define MACRO_EMBED_ONLY
#include "macro.cpp"
#undef MACRO_EMBED_ONLY

struct CEValue
{
	bool is_unsigned = false;
	uint64_t u = 0;
	int64_t s = 0;
};

CEValue MakeSigned(int64_t v) { CEValue x; x.s = v; x.u = static_cast<uint64_t>(v); return x; }
CEValue MakeUnsigned(uint64_t v) { CEValue x; x.is_unsigned = true; x.u = v; x.s = static_cast<int64_t>(v); return x; }
uint64_t AsU(const CEValue& v) { return v.is_unsigned ? v.u : static_cast<uint64_t>(v.s); }
int64_t AsS(const CEValue& v) { return v.is_unsigned ? static_cast<int64_t>(v.u) : v.s; }
bool IsTrue(const CEValue& v) { return v.is_unsigned ? (v.u != 0) : (v.s != 0); }

enum TokKind
{
	TK_INT, TK_ID, TK_LP, TK_RP, TK_Q, TK_COLON,
	TK_PLUS, TK_MINUS, TK_NOT, TK_COMPL, TK_STAR, TK_DIV, TK_MOD,
	TK_LSHIFT, TK_RSHIFT, TK_LT, TK_GT, TK_LE, TK_GE, TK_EQ, TK_NE,
	TK_AMP, TK_XOR, TK_BOR, TK_LAND, TK_LOR, TK_END
};

struct CEToken
{
	TokKind kind = TK_END;
	string text;
	CEValue value;
};

bool ParseIntegerLiteralValue(const string& s, CEValue& out)
{
	bool is_hex_int_style = s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
	bool is_float_like = !is_hex_int_style && ((s.find('.') != string::npos) || (s.find('e') != string::npos) || (s.find('E') != string::npos));
	if (is_float_like) return false;

	ParsedInteger pi = ParseIntegerLiteral(s);
	if (!pi.ok || pi.ud) return false;

	auto fits_signed = [&](long long mx) { return pi.value <= static_cast<unsigned long long>(mx); };
	auto fits_unsigned = [&](unsigned long long mx) { return pi.value <= mx; };
	vector<EFundamentalType> order;
	string suf = pi.suffix;
	bool has_u = (suf.find('u') != string::npos) || (suf.find('U') != string::npos);
	int lcount = 0;
	for (char c : suf) if (c == 'l' || c == 'L') lcount++;

	if (!pi.nondecimal)
	{
		if (!has_u && lcount == 0) order = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
		else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_LONG_LONG_INT};
		else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT};
		else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else order = {FT_UNSIGNED_LONG_LONG_INT};
	}
	else
	{
		if (!has_u && lcount == 0) order = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
		else order = {FT_UNSIGNED_LONG_LONG_INT};
	}

	for (EFundamentalType ty : order)
	{
		if (ty == FT_INT && fits_signed(numeric_limits<int>::max())) { out = MakeSigned(static_cast<int64_t>(static_cast<int>(pi.value))); return true; }
		if (ty == FT_LONG_INT && fits_signed(numeric_limits<long long>::max())) { out = MakeSigned(static_cast<int64_t>(static_cast<long int>(pi.value))); return true; }
		if (ty == FT_LONG_LONG_INT && fits_signed(numeric_limits<long long>::max())) { out = MakeSigned(static_cast<int64_t>(static_cast<long long int>(pi.value))); return true; }
		if (ty == FT_UNSIGNED_INT && fits_unsigned(numeric_limits<unsigned int>::max())) { out = MakeUnsigned(static_cast<uint64_t>(static_cast<unsigned int>(pi.value))); return true; }
		if (ty == FT_UNSIGNED_LONG_INT) { out = MakeUnsigned(static_cast<uint64_t>(static_cast<unsigned long int>(pi.value))); return true; }
		if (ty == FT_UNSIGNED_LONG_LONG_INT) { out = MakeUnsigned(static_cast<uint64_t>(static_cast<unsigned long long int>(pi.value))); return true; }
	}
	return false;
}

bool ParseCharLiteralValue(const string& s, CEValue& out)
{
	PPToken tok;
	tok.type = "character-literal";
	tok.data = s;
	ParsedChar pc = ParseCharacterLiteral(tok);
	if (!pc.ok || !pc.ud_suffix.empty()) return false;
	if (pc.type == FT_CHAR) { signed char v = 0; memcpy(&v, pc.bytes.data(), 1); out = MakeSigned(static_cast<int64_t>(v)); return true; }
	if (pc.type == FT_INT) { int v = 0; memcpy(&v, pc.bytes.data(), sizeof(v)); out = MakeSigned(static_cast<int64_t>(v)); return true; }
	if (pc.type == FT_WCHAR_T) { wchar_t v = 0; memcpy(&v, pc.bytes.data(), sizeof(v)); out = MakeSigned(static_cast<int64_t>(v)); return true; }
	if (pc.type == FT_CHAR16_T) { char16_t v = 0; memcpy(&v, pc.bytes.data(), sizeof(v)); out = MakeUnsigned(static_cast<uint64_t>(v)); return true; }
	if (pc.type == FT_CHAR32_T) { char32_t v = 0; memcpy(&v, pc.bytes.data(), sizeof(v)); out = MakeUnsigned(static_cast<uint64_t>(v)); return true; }
	return false;
}

bool ConvertLineToTokens(const vector<PPToken>& line, vector<CEToken>& out)
{
	for (const PPToken& p : line)
	{
		if (p.type == "whitespace-sequence" || p.type == "new-line") continue;
		CEToken t;
		if (p.type == "identifier") { t.kind = TK_ID; t.text = p.data; out.push_back(t); continue; }
		if (p.type == "pp-number") { CEValue v; if (!ParseIntegerLiteralValue(p.data, v)) return false; t.kind = TK_INT; t.value = v; out.push_back(t); continue; }
		if (p.type == "character-literal") { CEValue v; if (!ParseCharLiteralValue(p.data, v)) return false; t.kind = TK_INT; t.value = v; out.push_back(t); continue; }
		if (p.type == "preprocessing-op-or-punc")
		{
			const string& s = p.data;
			if (s == "(") t.kind = TK_LP;
			else if (s == ")") t.kind = TK_RP;
			else if (s == "?") t.kind = TK_Q;
			else if (s == ":") t.kind = TK_COLON;
			else if (s == "+") t.kind = TK_PLUS;
			else if (s == "-") t.kind = TK_MINUS;
			else if (s == "!" || s == "not") t.kind = TK_NOT;
			else if (s == "~" || s == "compl") t.kind = TK_COMPL;
			else if (s == "*") t.kind = TK_STAR;
			else if (s == "/") t.kind = TK_DIV;
			else if (s == "%") t.kind = TK_MOD;
			else if (s == "<<") t.kind = TK_LSHIFT;
			else if (s == ">>") t.kind = TK_RSHIFT;
			else if (s == "<") t.kind = TK_LT;
			else if (s == ">") t.kind = TK_GT;
			else if (s == "<=") t.kind = TK_LE;
			else if (s == ">=") t.kind = TK_GE;
			else if (s == "==") t.kind = TK_EQ;
			else if (s == "!=" || s == "not_eq") t.kind = TK_NE;
			else if (s == "&" || s == "bitand") t.kind = TK_AMP;
			else if (s == "^" || s == "xor") t.kind = TK_XOR;
			else if (s == "|" || s == "bitor") t.kind = TK_BOR;
			else if (s == "&&" || s == "and") t.kind = TK_LAND;
			else if (s == "||" || s == "or") t.kind = TK_LOR;
			else return false;
			out.push_back(t);
			continue;
		}
		return false;
	}
	CEToken e; e.kind = TK_END; out.push_back(e); return true;
}

struct EvalResult
{
	bool ok = true;
	CEValue value = MakeSigned(0);
	EvalResult() = default;
	EvalResult(bool ok_in, CEValue value_in) : ok(ok_in), value(value_in) {}
};

struct ParserCE
{
	vector<CEToken> toks;
	size_t pos = 0;
	const set<string>* defs = nullptr;

	CEToken& cur() { return toks[pos]; }
	bool eat(TokKind k) { if (cur().kind == k) { pos++; return true; } return false; }

	EvalResult parse_expression(bool eval) { return parse_cond(eval); }

	EvalResult parse_primary(bool eval)
	{
		if (cur().kind == TK_INT)
		{
			EvalResult r;
			r.value = eval ? cur().value : (cur().value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0));
			pos++;
			return r;
		}
		if (cur().kind == TK_LP)
		{
			pos++;
			EvalResult r = parse_cond(eval);
			if (!r.ok || !eat(TK_RP)) { r.ok = false; return r; }
			return r;
		}
		if (cur().kind == TK_ID)
		{
			string id = cur().text;
			pos++;
			if (id == "defined")
			{
				string target;
				if (eat(TK_LP))
				{
					if (cur().kind != TK_ID) return {false, MakeSigned(0)};
					target = cur().text;
					pos++;
					if (!eat(TK_RP)) return {false, MakeSigned(0)};
				}
				else
				{
					if (cur().kind != TK_ID) return {false, MakeSigned(0)};
					target = cur().text;
					pos++;
				}
				bool v = defs && defs->count(target);
				return {true, eval ? MakeSigned(v ? 1 : 0) : MakeSigned(0)};
			}
			if (!eval) return {true, MakeSigned(0)};
			if (id == "true") return {true, MakeSigned(1)};
			if (id == "false") return {true, MakeSigned(0)};
			return {true, MakeSigned(0)};
		}
		return {false, MakeSigned(0)};
	}

	EvalResult parse_unary(bool eval)
	{
		if (eat(TK_PLUS)) return parse_unary(eval);
		if (eat(TK_MINUS)) { EvalResult r = parse_unary(eval); if (!r.ok) return r; if (!eval) return {true, r.value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0)}; if (r.value.is_unsigned) r.value = MakeUnsigned(static_cast<uint64_t>(-AsU(r.value))); else r.value = MakeSigned(-AsS(r.value)); return r; }
		if (eat(TK_NOT)) { EvalResult r = parse_unary(eval); if (!r.ok) return r; if (!eval) return {true, MakeSigned(0)}; return {true, MakeSigned(IsTrue(r.value) ? 0 : 1)}; }
		if (eat(TK_COMPL)) { EvalResult r = parse_unary(eval); if (!r.ok) return r; if (!eval) return {true, r.value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0)}; if (r.value.is_unsigned) return {true, MakeUnsigned(~AsU(r.value))}; return {true, MakeSigned(~AsS(r.value))}; }
		return parse_primary(eval);
	}

	EvalResult parse_mul(bool eval)
	{
		EvalResult l = parse_unary(eval); if (!l.ok) return l;
		while (true)
		{
			TokKind op = cur().kind; if (op != TK_STAR && op != TK_DIV && op != TK_MOD) break; pos++;
			bool do_eval_rhs = eval;
			EvalResult r = parse_unary(do_eval_rhs); if (!r.ok) return r;
			if (!eval) continue;
			if (l.value.is_unsigned || r.value.is_unsigned)
			{
				uint64_t a = AsU(l.value), b = AsU(r.value), v = 0;
				if ((op == TK_DIV || op == TK_MOD) && b == 0) return {false, MakeSigned(0)};
				if (op == TK_STAR) v = a * b;
				else if (op == TK_DIV) v = a / b;
				else v = a % b;
				l.value = MakeUnsigned(v);
			}
			else
			{
				int64_t a = AsS(l.value), b = AsS(r.value), v = 0;
				if ((op == TK_DIV || op == TK_MOD) && b == 0) return {false, MakeSigned(0)};
				if (op == TK_STAR) v = a * b;
				else if (op == TK_DIV) v = a / b;
				else v = a % b;
				l.value = MakeSigned(v);
			}
		}
		return l;
	}

	EvalResult parse_add(bool eval)
	{
		EvalResult l = parse_mul(eval); if (!l.ok) return l;
		while (cur().kind == TK_PLUS || cur().kind == TK_MINUS)
		{
			TokKind op = cur().kind; pos++;
			EvalResult r = parse_mul(eval); if (!r.ok) return r;
			if (!eval) continue;
			if (l.value.is_unsigned || r.value.is_unsigned)
			{
				uint64_t a = AsU(l.value), b = AsU(r.value);
				l.value = MakeUnsigned(op == TK_PLUS ? a + b : a - b);
			}
			else
			{
				int64_t a = AsS(l.value), b = AsS(r.value);
				l.value = MakeSigned(op == TK_PLUS ? a + b : a - b);
			}
		}
		return l;
	}

	EvalResult parse_shift(bool eval)
	{
		EvalResult l = parse_add(eval); if (!l.ok) return l;
		while (cur().kind == TK_LSHIFT || cur().kind == TK_RSHIFT)
		{
			TokKind op = cur().kind; pos++;
			EvalResult r = parse_add(eval); if (!r.ok) return r;
			if (!eval) continue;
			uint64_t b = AsU(r.value);
			if (b >= 64) return {false, MakeSigned(0)};
			if (l.value.is_unsigned) l.value = MakeUnsigned(op == TK_LSHIFT ? (AsU(l.value) << b) : (AsU(l.value) >> b));
			else l.value = MakeSigned(op == TK_LSHIFT ? (AsS(l.value) << b) : (AsS(l.value) >> b));
		}
		return l;
	}

	EvalResult parse_rel(bool eval)
	{
		EvalResult l = parse_shift(eval); if (!l.ok) return l;
		while (cur().kind == TK_LT || cur().kind == TK_GT || cur().kind == TK_LE || cur().kind == TK_GE)
		{
			TokKind op = cur().kind; pos++;
			EvalResult r = parse_shift(eval); if (!r.ok) return r;
			if (!eval) continue;
			bool v;
			if (l.value.is_unsigned || r.value.is_unsigned)
			{
				uint64_t a = AsU(l.value), b = AsU(r.value);
				v = (op == TK_LT) ? (a < b) : (op == TK_GT) ? (a > b) : (op == TK_LE) ? (a <= b) : (a >= b);
			}
			else
			{
				int64_t a = AsS(l.value), b = AsS(r.value);
				v = (op == TK_LT) ? (a < b) : (op == TK_GT) ? (a > b) : (op == TK_LE) ? (a <= b) : (a >= b);
			}
			l.value = MakeSigned(v ? 1 : 0);
		}
		return l;
	}

	EvalResult parse_eq(bool eval)
	{
		EvalResult l = parse_rel(eval); if (!l.ok) return l;
		while (cur().kind == TK_EQ || cur().kind == TK_NE)
		{
			TokKind op = cur().kind; pos++;
			EvalResult r = parse_rel(eval); if (!r.ok) return r;
			if (!eval) continue;
			bool v = (AsU(l.value) == AsU(r.value));
			if (op == TK_NE) v = !v;
			l.value = MakeSigned(v ? 1 : 0);
		}
		return l;
	}

	EvalResult parse_band(bool eval) { EvalResult l = parse_eq(eval); if (!l.ok) return l; while (eat(TK_AMP)) { EvalResult r = parse_eq(eval); if (!r.ok) return r; if (eval) l.value = (l.value.is_unsigned || r.value.is_unsigned) ? MakeUnsigned(AsU(l.value) & AsU(r.value)) : MakeSigned(AsS(l.value) & AsS(r.value)); } return l; }
	EvalResult parse_xor(bool eval) { EvalResult l = parse_band(eval); if (!l.ok) return l; while (eat(TK_XOR)) { EvalResult r = parse_band(eval); if (!r.ok) return r; if (eval) l.value = (l.value.is_unsigned || r.value.is_unsigned) ? MakeUnsigned(AsU(l.value) ^ AsU(r.value)) : MakeSigned(AsS(l.value) ^ AsS(r.value)); } return l; }
	EvalResult parse_bor(bool eval) { EvalResult l = parse_xor(eval); if (!l.ok) return l; while (eat(TK_BOR)) { EvalResult r = parse_xor(eval); if (!r.ok) return r; if (eval) l.value = (l.value.is_unsigned || r.value.is_unsigned) ? MakeUnsigned(AsU(l.value) | AsU(r.value)) : MakeSigned(AsS(l.value) | AsS(r.value)); } return l; }
	EvalResult parse_land(bool eval) { EvalResult l = parse_bor(eval); if (!l.ok) return l; while (eat(TK_LAND)) { bool lv = eval ? IsTrue(l.value) : false; EvalResult r = parse_bor(eval && lv); if (!r.ok) return r; if (eval) l.value = MakeSigned((lv && IsTrue(r.value)) ? 1 : 0); } return l; }
	EvalResult parse_lor(bool eval) { EvalResult l = parse_land(eval); if (!l.ok) return l; while (eat(TK_LOR)) { bool lv = eval ? IsTrue(l.value) : false; EvalResult r = parse_land(eval && !lv); if (!r.ok) return r; if (eval) l.value = MakeSigned((lv || IsTrue(r.value)) ? 1 : 0); } return l; }

	EvalResult parse_cond(bool eval)
	{
		EvalResult c = parse_lor(eval); if (!c.ok) return c;
		if (!eat(TK_Q)) return c;
		bool cv = eval ? IsTrue(c.value) : false;
		EvalResult t = parse_cond(eval && cv); if (!t.ok || !eat(TK_COLON)) return {false, MakeSigned(0)};
		EvalResult f = parse_cond(eval && !cv); if (!f.ok) return f;
		if (!eval) return {true, MakeSigned(0)};
		if (t.value.is_unsigned || f.value.is_unsigned) return {true, MakeUnsigned(cv ? AsU(t.value) : AsU(f.value))};
		return {true, MakeSigned(cv ? AsS(t.value) : AsS(f.value))};
	}
};

struct IfGroup
{
	bool parent_active = true;
	bool branch_taken = false;
	bool current_active = false;
	bool seen_else = false;
};

struct Emitter
{
	ostream& out;
	bool strict_invalid;

	void emit_invalid(const string& source)
	{
		if (strict_invalid) throw runtime_error("invalid token: " + source);
		out << "invalid " << source << "\n";
	}
	void emit_simple(const string& source, ETokenType token_type) { out << "simple " << source << " " << TokenTypeToStringMap.at(token_type) << "\n"; }
	void emit_identifier(const string& source) { out << "identifier " << source << "\n"; }
	void emit_literal(const string& source, EFundamentalType type, const void* data, size_t nbytes) { out << "literal " << source << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << "\n"; }
	void emit_literal_array(const string& source, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes) { out << "literal " << source << " array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << "\n"; }
	void emit_user_defined_literal_character(const string& source, const string& ud_suffix, EFundamentalType type, const void* data, size_t nbytes) { out << "user-defined-literal " << source << " " << ud_suffix << " character " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << "\n"; }
	void emit_user_defined_literal_string_array(const string& source, const string& ud_suffix, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes) { out << "user-defined-literal " << source << " " << ud_suffix << " string array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << "\n"; }
	void emit_user_defined_literal_integer(const string& source, const string& ud_suffix, const string& prefix) { out << "user-defined-literal " << source << " " << ud_suffix << " integer " << prefix << "\n"; }
	void emit_user_defined_literal_floating(const string& source, const string& ud_suffix, const string& prefix) { out << "user-defined-literal " << source << " " << ud_suffix << " floating " << prefix << "\n"; }
	void emit_eof() { out << "eof\n"; }
};

struct PreprocState
{
	MacroEngine eng;
	set<PA5FileId> once_ids;
	string date_lit;
	string time_lit;
	string author_lit;
};

PPToken Canon(PPToken t)
{
	if (t.type == "preprocessing-op-or-punc")
	{
		if (t.data == "%:") t.data = "#";
		if (t.data == "%:%:") t.data = "##";
	}
	return t;
}

vector<PPToken> NormalizeTokens(const vector<PPToken>& in)
{
	vector<PPToken> out;
	for (PPToken t : in) out.push_back(Canon(t));
	return out;
}

struct PPTokenWithLine
{
	PPToken tok;
	int line = 1;
};

struct CollectPPTokenWithLineStream : IPPTokenStream
{
	vector<PPTokenWithLine> tokens;
	int* linep = nullptr;

	void push(const string& type, const string& data) { PPTokenWithLine x; x.tok.type = type; x.tok.data = data; x.line = *linep; tokens.push_back(x); }
	void emit_whitespace_sequence() override { push("whitespace-sequence", ""); }
	void emit_new_line() override { push("new-line", ""); }
	void emit_header_name(const string& data) override { push("header-name", data); }
	void emit_identifier(const string& data) override { push("identifier", data); }
	void emit_pp_number(const string& data) override { push("pp-number", data); }
	void emit_character_literal(const string& data) override { push("character-literal", data); }
	void emit_user_defined_character_literal(const string& data) override { push("user-defined-character-literal", data); }
	void emit_string_literal(const string& data) override { push("string-literal", data); }
	void emit_user_defined_string_literal(const string& data) override { push("user-defined-string-literal", data); }
	void emit_preprocessing_op_or_punc(const string& data) override { push("preprocessing-op-or-punc", data); }
	void emit_non_whitespace_char(const string& data) override { push("non-whitespace-character", data); }
	void emit_eof() override { push("eof", ""); }
};

vector<PPTokenWithLine> TokenizeWithLines(const string& input)
{
	vector<int> cps = DecodeUTF8(input);
	int line = 1;
	CollectPPTokenWithLineStream stream;
	stream.linep = &line;
	PPTokenizer tokenizer(stream);
	for (int cp : cps)
	{
		tokenizer.process(cp);
		if (cp == '\n') line++;
	}
	tokenizer.process(EndOfFile);
	for (PPTokenWithLine& x : stream.tokens) x.tok = Canon(x.tok);
	return stream.tokens;
}

vector<int> BuildLogicalStartLines(const string& raw)
{
	vector<int> starts;
	starts.push_back(1);
	int phys = 1;
	size_t i = 0;
	enum Mode { M_NORMAL, M_STR, M_CHR, M_RAW } mode = M_NORMAL;
	string raw_term;
	while (i < raw.size())
	{
		if (mode == M_NORMAL)
		{
			if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\n') { i += 2; phys++; continue; }
			if (raw[i] == '/' && i + 1 < raw.size() && raw[i + 1] == '/')
			{
				i += 2;
				while (i < raw.size() && raw[i] != '\n') i++;
				continue;
			}
			if (raw[i] == '/' && i + 1 < raw.size() && raw[i + 1] == '*')
			{
				i += 2;
				while (i + 1 < raw.size() && !(raw[i] == '*' && raw[i + 1] == '/'))
				{
					if (raw[i] == '\n') phys++;
					i++;
				}
				if (i + 1 < raw.size()) i += 2;
				continue;
			}
			if (raw[i] == '"') { mode = M_STR; i++; continue; }
			if (raw[i] == '\'') { mode = M_CHR; i++; continue; }
			if (raw[i] == 'R' && i + 1 < raw.size() && raw[i + 1] == '"')
			{
				size_t j = i + 2;
				while (j < raw.size() && raw[j] != '(') j++;
				if (j < raw.size())
				{
					string delim = raw.substr(i + 2, j - (i + 2));
					raw_term = ")" + delim + "\"";
					mode = M_RAW;
					i = j + 1;
					continue;
				}
			}
			if (raw[i] == '\n') { phys++; starts.push_back(phys); i++; continue; }
			i++;
			continue;
		}
		if (mode == M_STR || mode == M_CHR)
		{
			char endc = (mode == M_STR) ? '"' : '\'';
			if (raw[i] == '\\')
			{
				if (i + 1 < raw.size() && raw[i + 1] == '\n') { i += 2; phys++; continue; }
				i += (i + 1 < raw.size()) ? 2 : 1;
				continue;
			}
			if (raw[i] == endc) { mode = M_NORMAL; i++; continue; }
			if (raw[i] == '\n') { phys++; i++; continue; }
			i++;
			continue;
		}
		if (mode == M_RAW)
		{
			if (raw_term.size() && i + raw_term.size() <= raw.size() && raw.compare(i, raw_term.size(), raw_term) == 0)
			{
				i += raw_term.size();
				mode = M_NORMAL;
				continue;
			}
			if (raw[i] == '\n') phys++;
			i++;
			continue;
		}
	}
	return starts;
}

vector<PPToken> ExpandLine(PreprocState& st, const vector<PPToken>& line)
{
	vector<ExpToken> exp = ToExp(line);
	for (ExpToken& e : exp) if (e.pp.type == "new-line") e.pp.type = "whitespace-sequence";
	vector<ExpToken> rep = st.eng.Expand(exp);
	vector<PPToken> out;
	for (const ExpToken& e : rep) out.push_back(e.pp);
	return out;
}

vector<PPToken> NonWs(const vector<PPToken>& in)
{
	vector<PPToken> out;
	for (const PPToken& t : in) if (!IsWS(t)) out.push_back(t);
	return out;
}

MacroDef ObjectMacroFromToken(const PPToken& t)
{
	MacroDef m;
	m.function_like = false;
	m.variadic = false;
	m.replacement = {t};
	return m;
}

MacroDef ObjectMacroFromString(const string& s)
{
	vector<PPToken> t = TokenizeToPPTokens(s);
	MacroDef m;
	for (const PPToken& p : t) if (p.type != "eof" && !IsWS(p)) m.replacement.push_back(Canon(p));
	return m;
}

void SetDynamicBuiltins(PreprocState& st, const string& file_name, int line_num)
{
	st.eng.macros["__FILE__"] = ObjectMacroFromString("\"" + file_name + "\"");
	st.eng.macros["__LINE__"] = ObjectMacroFromString(to_string(line_num));
}

void SeedBuiltins(PreprocState& st)
{
	st.eng.macros["__CPPGM__"] = ObjectMacroFromString("201303L");
	st.eng.macros["__cplusplus"] = ObjectMacroFromString("201103L");
	st.eng.macros["__STDC_HOSTED__"] = ObjectMacroFromString("1");
	st.eng.macros["__CPPGM_AUTHOR__"] = ObjectMacroFromString(st.author_lit);
	st.eng.macros["__DATE__"] = ObjectMacroFromString(st.date_lit);
	st.eng.macros["__TIME__"] = ObjectMacroFromString(st.time_lit);
}

string DecodeStringLiteralToUtf8(const PPToken& t)
{
	ParsedStringPiece ps = ParseStringPiece(t);
	if (!ps.ok) throw runtime_error("bad string literal");
	string out;
	for (int cp : ps.cps) out += EncodeUTF8One(cp);
	return out;
}

bool IsDirectiveHash(const PPToken& t)
{
	return t.type == "preprocessing-op-or-punc" && t.data == "#";
}

bool EvaluateIfExpr(PreprocState& st, const vector<PPToken>& toks, int current_line, const string& current_file)
{
	vector<PPToken> rewritten;
	for (size_t i = 0; i < toks.size();)
	{
		if (toks[i].type == "identifier" && toks[i].data == "__LINE__")
		{
			PPToken v;
			v.type = "pp-number";
			v.data = to_string(current_line);
			rewritten.push_back(v);
			i++;
			continue;
		}
		if (toks[i].type == "identifier" && toks[i].data == "__FILE__")
		{
			PPToken v;
			v.type = "string-literal";
			v.data = "\"" + current_file + "\"";
			rewritten.push_back(v);
			i++;
			continue;
		}
		if (toks[i].type == "identifier" && toks[i].data == "defined")
		{
			size_t j = i + 1;
			while (j < toks.size() && IsWS(toks[j])) j++;
			bool paren = false;
			if (j < toks.size() && IsOp(toks[j], "(")) { paren = true; j++; }
			while (j < toks.size() && IsWS(toks[j])) j++;
			if (j >= toks.size() || toks[j].type != "identifier") throw runtime_error("left over tokens at end of controlling expression");
			string id = toks[j].data;
			j++;
			while (j < toks.size() && IsWS(toks[j])) j++;
			if (paren)
			{
				if (j >= toks.size() || !IsOp(toks[j], ")")) throw runtime_error("left over tokens at end of controlling expression");
				j++;
			}
			PPToken v;
			v.type = "pp-number";
			v.data = st.eng.macros.count(id) ? "1" : "0";
			rewritten.push_back(v);
			i = j;
			continue;
		}
		rewritten.push_back(toks[i]);
		i++;
	}

	vector<PPToken> expanded = ExpandLine(st, rewritten);
	vector<CEToken> ce;
	if (!ConvertLineToTokens(expanded, ce)) throw runtime_error("left over tokens at end of controlling expression");
	set<string> defs;
	for (const auto& kv : st.eng.macros) defs.insert(kv.first);
	ParserCE p;
	p.toks = ce;
	p.defs = &defs;
	EvalResult r = p.parse_expression(true);
	if (!r.ok || p.cur().kind != TK_END) throw runtime_error("left over tokens at end of controlling expression");
	return IsTrue(r.value);
}

void ParseDefineLikePA4(PreprocState& st, const vector<PPToken>& line, size_t p)
{
	while (p < line.size() && IsWS(line[p])) p++;
	if (p >= line.size() || line[p].type != "identifier") throw runtime_error("expected identifier");
	string name = line[p].data;
	if (name == "__VA_ARGS__") throw runtime_error("invalid __VA_ARGS__ use");
	p++;

	MacroDef m;
	bool fn = (p < line.size() && !IsWS(line[p]) && IsOp(line[p], "("));
	if (fn)
	{
		m.function_like = true;
		p++;
		while (true)
		{
			while (p < line.size() && IsWS(line[p])) p++;
			if (p >= line.size()) throw runtime_error("expected rparen (#2): PP_NEW_LINE()");
			if (IsOp(line[p], ")")) { p++; break; }
			if (IsOp(line[p], "..."))
			{
				m.variadic = true;
				p++;
				while (p < line.size() && IsWS(line[p])) p++;
				if (p >= line.size() || !IsOp(line[p], ")")) throw runtime_error("expected rparen (#2): PP_NEW_LINE()");
				p++;
				break;
			}
			if (line[p].type != "identifier") throw runtime_error("expected identifier after lparen");
			string param = line[p].data;
			if (param == "__VA_ARGS__") throw runtime_error("__VA_ARGS__ in macro parameter list");
			for (const string& q : m.params) if (q == param) throw runtime_error("duplicate parameter " + param + " in macro definition");
			m.params.push_back(param);
			p++;
			while (p < line.size() && IsWS(line[p])) p++;
			if (p >= line.size()) throw runtime_error("expected rparen (#2): PP_NEW_LINE()");
			if (IsOp(line[p], ")")) { p++; break; }
			if (!IsOp(line[p], ",")) throw runtime_error("expected rparen (#2): PP_NEW_LINE()");
			p++;
			while (p < line.size() && IsWS(line[p])) p++;
			if (p < line.size() && IsOp(line[p], "..."))
			{
				m.variadic = true;
				p++;
				while (p < line.size() && IsWS(line[p])) p++;
				if (p >= line.size() || !IsOp(line[p], ")")) throw runtime_error("expected rparen (#2): PP_NEW_LINE()");
				p++;
				break;
			}
		}
	}
	for (; p < line.size(); p++) m.replacement.push_back(line[p]);
	CheckVarArgsUse(m);
	ValidateHashHashEdges(m);
	if (m.function_like) ValidateFunctionReplacement(m);
	auto it = st.eng.macros.find(name);
	if (it != st.eng.macros.end() && !SameMacroDef(it->second, m)) throw runtime_error("macro redefined");
	st.eng.macros[name] = m;
}

void ParseUndefLikePA4(PreprocState& st, const vector<PPToken>& line, size_t p)
{
	while (p < line.size() && IsWS(line[p])) p++;
	if (p >= line.size() || line[p].type != "identifier") throw runtime_error("#undef expected id");
	string name = line[p].data;
	if (name == "__VA_ARGS__") throw runtime_error("invalid __VA_ARGS__ use");
	p++;
	while (p < line.size() && IsWS(line[p])) p++;
	if (p != line.size()) throw runtime_error("#undef expected id");
	st.eng.macros.erase(name);
}

void ProcessPragmaText(PreprocState& st, const string& cur_file, const string& pragma_text)
{
	vector<PPToken> p = NonWs(NormalizeTokens(TokenizeToPPTokens(pragma_text)));
	if (p.empty()) return;
	if (p[0].type == "identifier" && p[0].data == "once")
	{
		PA5FileId id;
		if (PA5GetFileId(cur_file, id)) st.once_ids.insert(id);
		return;
	}
}

void ApplyPragmaOperator(PreprocState& st, const string& cur_file, vector<PPToken>& tokens)
{
	vector<PPToken> out;
	for (size_t i = 0; i < tokens.size();)
	{
		if (tokens[i].type == "identifier" && tokens[i].data == "_Pragma")
		{
			size_t j = i + 1;
			while (j < tokens.size() && IsWS(tokens[j])) j++;
			if (j >= tokens.size() || !IsOp(tokens[j], "(")) throw runtime_error("bad _Pragma");
			j++;
			while (j < tokens.size() && IsWS(tokens[j])) j++;
			if (j >= tokens.size() || tokens[j].type != "string-literal") throw runtime_error("bad _Pragma");
			string txt = DecodeStringLiteralToUtf8(tokens[j]);
			j++;
			while (j < tokens.size() && IsWS(tokens[j])) j++;
			if (j >= tokens.size() || !IsOp(tokens[j], ")")) throw runtime_error("bad _Pragma");
			j++;
			ProcessPragmaText(st, cur_file, txt);
			i = j;
			continue;
		}
		out.push_back(tokens[i]);
		i++;
	}
	tokens.swap(out);
}

void EmitPostTokenized(const vector<PPToken>& pptoks, ostream& out, bool strict_invalid)
{
	Emitter output{out, strict_invalid};
	for (size_t i = 0; i < pptoks.size(); i++)
	{
		const PPToken& t = pptoks[i];
		if (t.type == "eof") break;
		if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

		if (t.type == "header-name" || t.type == "non-whitespace-character") { output.emit_invalid(t.data); continue; }

		if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
		{
			if (t.data == "#" || t.data == "##" || t.data == "%:" || t.data == "%:%:") { output.emit_invalid(t.data); continue; }
			auto it = StringToTokenTypeMap.find(t.data);
			if (it != StringToTokenTypeMap.end()) output.emit_simple(t.data, it->second);
			else if (t.type == "identifier") output.emit_identifier(t.data);
			else output.emit_invalid(t.data);
			continue;
		}

		if (t.type == "pp-number")
		{
			bool is_hex_int_style = t.data.size() >= 2 && t.data[0] == '0' && (t.data[1] == 'x' || t.data[1] == 'X');
			bool is_float_like = !is_hex_int_style && ((t.data.find('.') != string::npos) || (t.data.find('e') != string::npos) || (t.data.find('E') != string::npos));
			if (is_float_like)
			{
				bool ud = false; string ud_suffix; string prefix; EFundamentalType ty = FT_DOUBLE;
				if (!ParseFloatingLiteral(t.data, ud, ud_suffix, prefix, ty)) { output.emit_invalid(t.data); continue; }
				if (ud) { output.emit_user_defined_literal_floating(t.data, ud_suffix, prefix); continue; }
				if (ty == FT_FLOAT) { float v = PA2Decode_float(prefix); output.emit_literal(t.data, ty, &v, sizeof(v)); }
				else if (ty == FT_LONG_DOUBLE) { long double v = PA2Decode_long_double(prefix); output.emit_literal(t.data, ty, &v, sizeof(v)); }
				else { double v = PA2Decode_double(prefix); output.emit_literal(t.data, ty, &v, sizeof(v)); }
				continue;
			}

			ParsedInteger pi = ParseIntegerLiteral(t.data);
			if (!pi.ok)
			{
				size_t us = t.data.find('_');
				if (us != string::npos && us + 1 < t.data.size())
				{
					string core = t.data.substr(0, us);
					ParsedInteger cpi = ParseIntegerLiteral(core);
					if (cpi.ok && !cpi.ud) { output.emit_user_defined_literal_integer(t.data, t.data.substr(us), core); continue; }
				}
				output.emit_invalid(t.data); continue;
			}
			if (pi.ud) { output.emit_user_defined_literal_integer(t.data, pi.ud_suffix, pi.core); continue; }

			vector<EFundamentalType> order;
			bool has_u = (pi.suffix.find('u') != string::npos) || (pi.suffix.find('U') != string::npos);
			int lcount = 0; for (char c : pi.suffix) if (c == 'l' || c == 'L') lcount++;
			if (!pi.nondecimal)
			{
				if (!has_u && lcount == 0) order = {FT_INT, FT_LONG_INT, FT_LONG_LONG_INT};
				else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_LONG_LONG_INT};
				else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT};
				else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else order = {FT_UNSIGNED_LONG_LONG_INT};
			}
			else
			{
				if (!has_u && lcount == 0) order = {FT_INT, FT_UNSIGNED_INT, FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else if (!has_u && lcount == 1) order = {FT_LONG_INT, FT_UNSIGNED_LONG_INT, FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else if (!has_u && lcount == 2) order = {FT_LONG_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else if (has_u && lcount == 0) order = {FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else if (has_u && lcount == 1) order = {FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT};
				else order = {FT_UNSIGNED_LONG_LONG_INT};
			}
			bool emitted = false;
			auto fits_signed = [&](long long mx) { return pi.value <= static_cast<unsigned long long>(mx); };
			auto fits_unsigned = [&](unsigned long long mx) { return pi.value <= mx; };
			for (EFundamentalType ty : order)
			{
				if (ty == FT_INT && fits_signed(numeric_limits<int>::max())) { int v = static_cast<int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				if (ty == FT_LONG_INT && fits_signed(numeric_limits<long int>::max())) { long int v = static_cast<long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				if (ty == FT_LONG_LONG_INT && fits_signed(numeric_limits<long long int>::max())) { long long int v = static_cast<long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				if (ty == FT_UNSIGNED_INT && fits_unsigned(numeric_limits<unsigned int>::max())) { unsigned int v = static_cast<unsigned int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				if (ty == FT_UNSIGNED_LONG_INT && fits_unsigned(numeric_limits<unsigned long int>::max())) { unsigned long int v = static_cast<unsigned long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
				if (ty == FT_UNSIGNED_LONG_LONG_INT && fits_unsigned(numeric_limits<unsigned long long int>::max())) { unsigned long long int v = static_cast<unsigned long long int>(pi.value); output.emit_literal(t.data, ty, &v, sizeof(v)); emitted = true; break; }
			}
			if (!emitted) output.emit_invalid(t.data);
			continue;
		}

		if (t.type == "character-literal" || t.type == "user-defined-character-literal")
		{
			ParsedChar pc = ParseCharacterLiteral(t);
			if (!pc.ok) { output.emit_invalid(t.data); continue; }
			if (pc.ud_suffix.empty()) output.emit_literal(t.data, pc.type, pc.bytes.data(), pc.bytes.size());
			else output.emit_user_defined_literal_character(t.data, pc.ud_suffix, pc.type, pc.bytes.data(), pc.bytes.size());
			continue;
		}

		if (t.type == "string-literal" || t.type == "user-defined-string-literal")
		{
			size_t j = i;
			vector<PPToken> seq;
			while (j < pptoks.size())
			{
				if (pptoks[j].type == "whitespace-sequence" || pptoks[j].type == "new-line") { j++; continue; }
				if (pptoks[j].type == "string-literal" || pptoks[j].type == "user-defined-string-literal") { seq.push_back(pptoks[j]); j++; continue; }
				break;
			}
			i = j - 1;
			string src; for (size_t k = 0; k < seq.size(); k++) { if (k) src += " "; src += seq[k].data; }
			vector<ParsedStringPiece> parts;
			bool ok = true;
			for (const PPToken& p : seq) { ParsedStringPiece ps = ParseStringPiece(p); if (!ps.ok) { ok = false; break; } parts.push_back(move(ps)); }
			if (!ok) { output.emit_invalid(src); continue; }
			set<string> encs, uds;
			for (const auto& p : parts) { if (!p.prefix.empty()) encs.insert(p.prefix); if (!p.ud_suffix.empty()) uds.insert(p.ud_suffix); }
			if (encs.size() > 1 || uds.size() > 1) { output.emit_invalid(src); continue; }
			string enc = encs.empty() ? "" : *encs.begin();
			string udsuf = uds.empty() ? "" : *uds.begin();
			vector<int> cps; for (const auto& p : parts) cps.insert(cps.end(), p.cps.begin(), p.cps.end()); cps.push_back(0);
			vector<unsigned char> bytes;
			size_t num_elements = 0;
			EFundamentalType ty = FT_CHAR;
			if (enc.empty() || enc == "u8")
			{
				ty = FT_CHAR;
				string s8; for (int cp : cps) s8 += EncodeUTF8One(cp);
				bytes.assign(s8.begin(), s8.end());
				num_elements = bytes.size();
			}
			else if (enc == "u")
			{
				ty = FT_CHAR16_T;
				for (int cp : cps)
				{
					if (!IsValidCodePoint(cp)) { ok = false; break; }
					if (cp <= 0xFFFF)
					{
						char16_t v = static_cast<char16_t>(cp);
						unsigned char* p = reinterpret_cast<unsigned char*>(&v);
						bytes.insert(bytes.end(), p, p + 2);
						num_elements++;
					}
					else
					{
						unsigned x = static_cast<unsigned>(cp - 0x10000);
						char16_t hi = static_cast<char16_t>(0xD800 | ((x >> 10) & 0x3FF));
						char16_t lo = static_cast<char16_t>(0xDC00 | (x & 0x3FF));
						unsigned char* p1 = reinterpret_cast<unsigned char*>(&hi);
						unsigned char* p2 = reinterpret_cast<unsigned char*>(&lo);
						bytes.insert(bytes.end(), p1, p1 + 2);
						bytes.insert(bytes.end(), p2, p2 + 2);
						num_elements += 2;
					}
				}
			}
			else if (enc == "U" || enc == "L")
			{
				ty = (enc == "U") ? FT_CHAR32_T : FT_WCHAR_T;
				for (int cp : cps)
				{
					if (!IsValidCodePoint(cp)) { ok = false; break; }
					uint32_t v = static_cast<uint32_t>(cp);
					unsigned char* p = reinterpret_cast<unsigned char*>(&v);
					bytes.insert(bytes.end(), p, p + 4);
					num_elements++;
				}
			}
			if (!ok) { output.emit_invalid(src); continue; }
			if (udsuf.empty()) output.emit_literal_array(src, num_elements, ty, bytes.data(), bytes.size());
			else output.emit_user_defined_literal_string_array(src, udsuf, num_elements, ty, bytes.data(), bytes.size());
			continue;
		}

		output.emit_invalid(t.data);
	}
	output.emit_eof();
}

struct FileFrame
{
	string physical_path;
	string presumed_file;
	int line = 1;
	vector<IfGroup> ifs;
};

bool CurrentActive(const FileFrame& f)
{
	for (const IfGroup& g : f.ifs) if (!g.current_active) return false;
	return true;
}

bool ResolveIncludePath(const string& cur_file, const string& nextf, string& out_path, PA5FileId& out_id)
{
	string rel;
	size_t p = cur_file.find_last_of('/');
	if (p != string::npos) rel = cur_file.substr(0, p + 1) + nextf;
	if (!rel.empty() && PA5GetFileId(rel, out_id)) { out_path = rel; return true; }
	if (PA5GetFileId(nextf, out_id)) { out_path = nextf; return true; }
	return false;
}

void ProcessFile(PreprocState& st, const string& path, vector<PPToken>& out_tokens)
{
	ifstream in(path);
	if (!in) throw runtime_error("cannot open include: " + path);
	ostringstream oss;
	oss << in.rdbuf();
	string raw = oss.str();
	vector<int> logical_starts = BuildLogicalStartLines(raw);
	vector<PPToken> all = NormalizeTokens(TokenizeToPPTokens(raw));

	FileFrame frame;
	frame.physical_path = path;
	frame.presumed_file = path;
	frame.line = 1;
	bool manual_line = false;
	size_t logical_idx = 0;
	vector<PPToken> pending_seq;
	int pending_paren_depth = 0;
	auto flush_text = [&]()
	{
		if (pending_seq.empty()) return;
		SetDynamicBuiltins(st, frame.presumed_file, frame.line);
		vector<PPToken> expanded = ExpandLine(st, pending_seq);
		ApplyPragmaOperator(st, frame.physical_path, expanded);
		for (const PPToken& t : expanded) if (!IsWS(t)) out_tokens.push_back(t);
		pending_seq.clear();
		pending_paren_depth = 0;
	};

	for (size_t i = 0; i < all.size() && all[i].type != "eof";)
	{
		if (!manual_line)
		{
			if (logical_idx < logical_starts.size()) frame.line = logical_starts[logical_idx];
		}
		SetDynamicBuiltins(st, frame.presumed_file, frame.line);

		size_t line_end = i;
		while (line_end < all.size() && all[line_end].type != "new-line" && all[line_end].type != "eof") line_end++;
		vector<PPToken> line;
		for (size_t k = i; k < line_end; k++) line.push_back(all[k]);

		bool active = CurrentActive(frame);
		size_t p = 0;
		while (p < line.size() && line[p].type == "whitespace-sequence") p++;
		bool is_directive = (p < line.size() && IsDirectiveHash(line[p]));

		if (is_directive)
		{
			flush_text();
			size_t q = p + 1;
			while (q < line.size() && IsWS(line[q])) q++;
			if (q >= line.size())
			{
				// null directive
			}
			else if (line[q].type != "identifier")
			{
				if (active) throw runtime_error("active non-directive found: " + line[q].data);
			}
			else
			{
				string kw = line[q].data;
				size_t body = q + 1;
				if (kw == "if")
				{
					bool cond = false;
					if (active)
					{
						vector<PPToken> expr(line.begin() + static_cast<long>(body), line.end());
						cond = EvaluateIfExpr(st, expr, frame.line, frame.presumed_file);
					}
					IfGroup g;
					g.parent_active = active;
					g.branch_taken = active && cond;
					g.current_active = active && cond;
					frame.ifs.push_back(g);
				}
				else if (kw == "ifdef" || kw == "ifndef")
				{
					bool cond = false;
					if (body < line.size() && line[body].type == "whitespace-sequence") body++;
					while (body < line.size() && IsWS(line[body])) body++;
					if (body >= line.size() || line[body].type != "identifier") throw runtime_error("expected identifier");
					string id = line[body].data;
					cond = st.eng.macros.count(id) != 0;
					if (kw == "ifndef") cond = !cond;
					IfGroup g;
					g.parent_active = active;
					g.branch_taken = active && cond;
					g.current_active = active && cond;
					frame.ifs.push_back(g);
				}
				else if (kw == "elif")
				{
					if (frame.ifs.empty()) throw runtime_error("unexpected #elif");
					IfGroup& g = frame.ifs.back();
					if (g.seen_else) throw runtime_error("unexpected #elif");
					if (!g.parent_active || g.branch_taken) g.current_active = false;
					else
					{
						vector<PPToken> expr(line.begin() + static_cast<long>(body), line.end());
						bool cond = EvaluateIfExpr(st, expr, frame.line, frame.presumed_file);
						g.current_active = cond;
						if (cond) g.branch_taken = true;
					}
				}
				else if (kw == "else")
				{
					if (frame.ifs.empty()) throw runtime_error("unexpected #else");
					IfGroup& g = frame.ifs.back();
					if (g.seen_else) throw runtime_error("unexpected #else");
					g.seen_else = true;
					g.current_active = g.parent_active && !g.branch_taken;
					g.branch_taken = true;
				}
				else if (kw == "endif")
				{
					if (frame.ifs.empty()) throw runtime_error("unexpected #endif");
					frame.ifs.pop_back();
				}
				else if (!active)
				{
					// ignored in inactive section
				}
				else if (kw == "define")
				{
					vector<PPToken> body_t(line.begin() + static_cast<long>(body), line.end());
					ParseDefineLikePA4(st, body_t, 0);
				}
				else if (kw == "undef")
				{
					vector<PPToken> body_t(line.begin() + static_cast<long>(body), line.end());
					ParseUndefLikePA4(st, body_t, 0);
				}
				else if (kw == "error")
				{
					string msg;
					for (size_t z = body; z < line.size(); z++) msg += line[z].data;
					throw runtime_error("#error \"" + msg + "\"");
				}
				else if (kw == "pragma")
				{
					vector<PPToken> ptoks(line.begin() + static_cast<long>(body), line.end());
					ptoks = NonWs(ExpandLine(st, ptoks));
					if (!ptoks.empty() && ptoks[0].type == "identifier" && ptoks[0].data == "once")
					{
						PA5FileId id;
						if (PA5GetFileId(frame.physical_path, id)) st.once_ids.insert(id);
					}
				}
				else if (kw == "include")
				{
					vector<PPToken> itoks(line.begin() + static_cast<long>(body), line.end());
					itoks = NonWs(ExpandLine(st, itoks));
					if (itoks.size() != 1) throw runtime_error("bad include");
					string nextf;
					if (itoks[0].type == "header-name")
					{
						string s = itoks[0].data;
						if (s.size() < 2) throw runtime_error("bad include");
						nextf = s.substr(1, s.size() - 2);
					}
					else if (itoks[0].type == "string-literal")
					{
						nextf = DecodeStringLiteralToUtf8(itoks[0]);
					}
					else throw runtime_error("bad include");

					string incpath;
					PA5FileId id;
					if (!ResolveIncludePath(frame.presumed_file, nextf, incpath, id)) throw runtime_error("bad include");
					if (st.once_ids.count(id) == 0) ProcessFile(st, incpath, out_tokens);
				}
				else if (kw == "line")
				{
					vector<PPToken> ltoks(line.begin() + static_cast<long>(body), line.end());
					ltoks = NonWs(ExpandLine(st, ltoks));
					if (ltoks.empty() || ltoks.size() > 2 || ltoks[0].type != "pp-number") throw runtime_error("bad #line");
					ParsedInteger pi = ParseIntegerLiteral(ltoks[0].data);
					if (!pi.ok || pi.ud) throw runtime_error("bad #line");
					frame.line = static_cast<int>(pi.value) - 1;
					manual_line = true;
					if (ltoks.size() == 2)
					{
						if (ltoks[1].type != "string-literal") throw runtime_error("bad #line");
						frame.presumed_file = DecodeStringLiteralToUtf8(ltoks[1]);
					}
				}
				else
				{
					if (active) throw runtime_error("active non-directive found: " + kw);
				}
			}
		}
		else if (active)
		{
			PPToken last_non_ws;
			bool has_last_non_ws = false;
			for (PPToken t : line)
			{
				pending_seq.push_back(t);
				if (!IsWS(t)) { last_non_ws = t; has_last_non_ws = true; }
				if (t.type == "preprocessing-op-or-punc" && t.data == "(") pending_paren_depth++;
				if (t.type == "preprocessing-op-or-punc" && t.data == ")") pending_paren_depth--;
			}
			PPToken nl;
			nl.type = "new-line";
			pending_seq.push_back(nl);
			bool trailing_fnlike_name = false;
			if (has_last_non_ws && last_non_ws.type == "identifier")
			{
				auto it = st.eng.macros.find(last_non_ws.data);
				if (it != st.eng.macros.end() && it->second.function_like) trailing_fnlike_name = true;
			}
			if (pending_paren_depth <= 0 && !trailing_fnlike_name) flush_text();
		}
		else
		{
			flush_text();
		}

		i = line_end;
		if (i < all.size() && all[i].type == "new-line") i++;
		if (manual_line)
		{
			frame.line++;
		}
		logical_idx++;
	}
	flush_text();

	if (!frame.ifs.empty()) throw runtime_error("include completed in bad group state (maybe unterminated #if)");
}

pair<string, string> BuildDateTimeLiterals()
{
	time_t now = time(nullptr);
	tm* tmv = localtime(&now);
	string asc = asctime(tmv);
	if (!asc.empty() && asc.back() == '\n') asc.pop_back();
	string mon = asc.substr(4, 3);
	string day = asc.substr(8, 2);
	string tim = asc.substr(11, 8);
	string yr = asc.substr(20, 4);
	string date = "\"" + mon + " " + day + " " + yr + "\"";
	string time_s = "\"" + tim + "\"";
	return make_pair(date, time_s);
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
		out << "preproc " << nsrcfiles << "\n";

		pair<string, string> dt = BuildDateTimeLiterals();

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i + 2];
			out << "sof " << srcfile << "\n";

			PreprocState st;
			st.date_lit = dt.first;
			st.time_lit = dt.second;
			st.author_lit = "\"John Smith\"";
			SeedBuiltins(st);

			vector<PPToken> result;
			ProcessFile(st, srcfile, result);
			result.push_back({"eof", ""});
			EmitPostTokenized(result, out, true);
		}

		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
