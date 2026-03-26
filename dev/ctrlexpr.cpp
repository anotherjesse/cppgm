// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <string>
#include <vector>
#include <cstdint>
#include <limits>
#include <sstream>
#include <iostream>
#include <cstring>

using namespace std;

#define POSTTOKEN_EMBED_ONLY
#include "posttoken.cpp"
#undef POSTTOKEN_EMBED_ONLY

// mock implementation of IsDefinedIdentifier for PA3
// return true iff first code point is odd
bool PA3Mock_IsDefinedIdentifier(const string& identifier)
{
	if (identifier.empty())
		return false;
	else
		return identifier[0] % 2;
}

struct CEValue
{
	bool is_unsigned = false;
	uint64_t u = 0;
	int64_t s = 0;
};

CEValue MakeSigned(int64_t v)
{
	CEValue x;
	x.is_unsigned = false;
	x.s = v;
	x.u = static_cast<uint64_t>(v);
	return x;
}

CEValue MakeUnsigned(uint64_t v)
{
	CEValue x;
	x.is_unsigned = true;
	x.u = v;
	x.s = static_cast<int64_t>(v);
	return x;
}

uint64_t AsU(const CEValue& v)
{
	return v.is_unsigned ? v.u : static_cast<uint64_t>(v.s);
}

int64_t AsS(const CEValue& v)
{
	return v.is_unsigned ? static_cast<int64_t>(v.u) : v.s;
}

bool IsTrue(const CEValue& v)
{
	return v.is_unsigned ? (v.u != 0) : (v.s != 0);
}

string RenderValue(const CEValue& v)
{
	if (v.is_unsigned)
	{
		return to_string(v.u) + "u";
	}
	return to_string(v.s);
}

enum TokKind
{
	TK_INT,
	TK_ID,
	TK_LP,
	TK_RP,
	TK_Q,
	TK_COLON,
	TK_PLUS,
	TK_MINUS,
	TK_NOT,
	TK_COMPL,
	TK_STAR,
	TK_DIV,
	TK_MOD,
	TK_LSHIFT,
	TK_RSHIFT,
	TK_LT,
	TK_GT,
	TK_LE,
	TK_GE,
	TK_EQ,
	TK_NE,
	TK_AMP,
	TK_XOR,
	TK_BOR,
	TK_LAND,
	TK_LOR,
	TK_END
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
	if (is_float_like)
	{
		return false;
	}

	ParsedInteger pi = ParseIntegerLiteral(s);
	if (!pi.ok || pi.ud)
	{
		return false;
	}

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
	if (!pc.ok || !pc.ud_suffix.empty())
	{
		return false;
	}

	if (pc.type == FT_CHAR)
	{
		signed char v = 0;
		memcpy(&v, pc.bytes.data(), 1);
		out = MakeSigned(static_cast<int64_t>(v));
		return true;
	}
	if (pc.type == FT_INT)
	{
		int v = 0;
		memcpy(&v, pc.bytes.data(), sizeof(v));
		out = MakeSigned(static_cast<int64_t>(v));
		return true;
	}
	if (pc.type == FT_WCHAR_T)
	{
		wchar_t v = 0;
		memcpy(&v, pc.bytes.data(), sizeof(v));
		out = MakeSigned(static_cast<int64_t>(v));
		return true;
	}
	if (pc.type == FT_CHAR16_T)
	{
		char16_t v = 0;
		memcpy(&v, pc.bytes.data(), sizeof(v));
		out = MakeUnsigned(static_cast<uint64_t>(v));
		return true;
	}
	if (pc.type == FT_CHAR32_T)
	{
		char32_t v = 0;
		memcpy(&v, pc.bytes.data(), sizeof(v));
		out = MakeUnsigned(static_cast<uint64_t>(v));
		return true;
	}
	return false;
}

bool ConvertLineToTokens(const vector<PPToken>& line, vector<CEToken>& out)
{
	for (const PPToken& p : line)
	{
		CEToken t;
		if (p.type == "identifier")
		{
			t.kind = TK_ID;
			t.text = p.data;
			out.push_back(t);
			continue;
		}
		if (p.type == "pp-number")
		{
			CEValue v;
			if (!ParseIntegerLiteralValue(p.data, v)) return false;
			t.kind = TK_INT;
			t.value = v;
			out.push_back(t);
			continue;
		}
		if (p.type == "character-literal")
		{
			CEValue v;
			if (!ParseCharLiteralValue(p.data, v)) return false;
			t.kind = TK_INT;
			t.value = v;
			out.push_back(t);
			continue;
		}
		if (p.type == "preprocessing-op-or-punc")
		{
			const string& s = p.data;
			if (s == "(") t.kind = TK_LP;
			else if (s == ")") t.kind = TK_RP;
			else if (s == "?") t.kind = TK_Q;
			else if (s == ":") t.kind = TK_COLON;
			else if (s == "+") t.kind = TK_PLUS;
			else if (s == "-") t.kind = TK_MINUS;
			else if (s == "!") t.kind = TK_NOT;
			else if (s == "not") t.kind = TK_NOT;
			else if (s == "~") t.kind = TK_COMPL;
			else if (s == "compl") t.kind = TK_COMPL;
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
			else if (s == "!=") t.kind = TK_NE;
			else if (s == "not_eq") t.kind = TK_NE;
			else if (s == "&") t.kind = TK_AMP;
			else if (s == "bitand") t.kind = TK_AMP;
			else if (s == "^") t.kind = TK_XOR;
			else if (s == "xor") t.kind = TK_XOR;
			else if (s == "|") t.kind = TK_BOR;
			else if (s == "bitor") t.kind = TK_BOR;
			else if (s == "&&") t.kind = TK_LAND;
			else if (s == "and") t.kind = TK_LAND;
			else if (s == "||") t.kind = TK_LOR;
			else if (s == "or") t.kind = TK_LOR;
			else return false;
			out.push_back(t);
			continue;
		}

		return false;
	}

	CEToken end;
	end.kind = TK_END;
	out.push_back(end);
	return true;
}

struct EvalResult
{
	bool ok = true;
	CEValue value = MakeSigned(0);

	EvalResult() = default;
	EvalResult(bool ok_in, CEValue value_in) : ok(ok_in), value(value_in) {}
};

struct Parser
{
	vector<CEToken> toks;
	size_t pos = 0;

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
				return {true, eval ? MakeSigned(PA3Mock_IsDefinedIdentifier(target) ? 1 : 0) : MakeSigned(0)};
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
		if (eat(TK_MINUS))
		{
			EvalResult r = parse_unary(eval);
			if (!r.ok) return r;
			if (!eval) return {true, r.value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0)};
			if (r.value.is_unsigned) r.value = MakeUnsigned(static_cast<uint64_t>(-AsU(r.value)));
			else r.value = MakeSigned(-AsS(r.value));
			return r;
		}
		if (eat(TK_NOT))
		{
			EvalResult r = parse_unary(eval);
			if (!r.ok) return r;
			if (!eval) return {true, MakeSigned(0)};
			r.value = MakeSigned(IsTrue(r.value) ? 0 : 1);
			return r;
		}
		if (eat(TK_COMPL))
		{
			EvalResult r = parse_unary(eval);
			if (!r.ok) return r;
			if (!eval) return {true, r.value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0)};
			if (r.value.is_unsigned) r.value = MakeUnsigned(~AsU(r.value));
			else r.value = MakeSigned(~AsS(r.value));
			return r;
		}
		return parse_primary(eval);
	}

	EvalResult bin_arith(EvalResult a, EvalResult b, TokKind op, bool eval)
	{
		if (!a.ok || !b.ok) return {false, MakeSigned(0)};
		if (!eval) return {true, (a.value.is_unsigned || b.value.is_unsigned) ? MakeUnsigned(0) : MakeSigned(0)};
		if (a.value.is_unsigned || b.value.is_unsigned)
		{
			uint64_t x = AsU(a.value), y = AsU(b.value), z = 0;
			if ((op == TK_DIV || op == TK_MOD) && y == 0) return {false, MakeSigned(0)};
			if (op == TK_PLUS) z = x + y;
			else if (op == TK_MINUS) z = x - y;
			else if (op == TK_STAR) z = x * y;
			else if (op == TK_DIV) z = x / y;
			else z = x % y;
			return {true, MakeUnsigned(z)};
		}
		int64_t x = AsS(a.value), y = AsS(b.value), z = 0;
		if ((op == TK_DIV || op == TK_MOD) && y == 0) return {false, MakeSigned(0)};
		if ((op == TK_DIV || op == TK_MOD) && x == numeric_limits<int64_t>::min() && y == -1) return {false, MakeSigned(0)};
		if (op == TK_PLUS) z = x + y;
		else if (op == TK_MINUS) z = x - y;
		else if (op == TK_STAR) z = x * y;
		else if (op == TK_DIV) z = x / y;
		else z = x % y;
		return {true, MakeSigned(z)};
	}

	EvalResult parse_mul(bool eval)
	{
		EvalResult r = parse_unary(eval);
		while (cur().kind == TK_STAR || cur().kind == TK_DIV || cur().kind == TK_MOD)
		{
			TokKind op = cur().kind;
			pos++;
			EvalResult rhs = parse_unary(eval);
			r = bin_arith(r, rhs, op, eval);
		}
		return r;
	}

	EvalResult parse_add(bool eval)
	{
		EvalResult r = parse_mul(eval);
		while (cur().kind == TK_PLUS || cur().kind == TK_MINUS)
		{
			TokKind op = cur().kind;
			pos++;
			EvalResult rhs = parse_mul(eval);
			r = bin_arith(r, rhs, op, eval);
		}
		return r;
	}

	EvalResult parse_shift(bool eval)
	{
		EvalResult r = parse_add(eval);
		while (cur().kind == TK_LSHIFT || cur().kind == TK_RSHIFT)
		{
			TokKind op = cur().kind;
			pos++;
			EvalResult rhs = parse_add(eval);
			if (!r.ok || !rhs.ok) return {false, MakeSigned(0)};
			if (!eval) { r = {true, r.value.is_unsigned ? MakeUnsigned(0) : MakeSigned(0)}; continue; }
			int64_t sh = rhs.value.is_unsigned ? static_cast<int64_t>(rhs.value.u) : rhs.value.s;
			if (sh < 0 || sh >= 64) return {false, MakeSigned(0)};
			if (r.value.is_unsigned)
			{
				uint64_t x = AsU(r.value);
				r.value = MakeUnsigned(op == TK_LSHIFT ? (x << sh) : (x >> sh));
			}
			else
			{
				int64_t x = AsS(r.value);
				r.value = MakeSigned(op == TK_LSHIFT ? static_cast<int64_t>(static_cast<uint64_t>(x) << sh) : (x >> sh));
			}
		}
		return r;
	}

	EvalResult cmp(EvalResult a, EvalResult b, TokKind op, bool eval)
	{
		if (!a.ok || !b.ok) return {false, MakeSigned(0)};
		if (!eval) return {true, MakeSigned(0)};
		bool ans;
		if (a.value.is_unsigned || b.value.is_unsigned)
		{
			uint64_t x = AsU(a.value), y = AsU(b.value);
			if (op == TK_LT) ans = x < y;
			else if (op == TK_GT) ans = x > y;
			else if (op == TK_LE) ans = x <= y;
			else if (op == TK_GE) ans = x >= y;
			else if (op == TK_EQ) ans = x == y;
			else ans = x != y;
		}
		else
		{
			int64_t x = AsS(a.value), y = AsS(b.value);
			if (op == TK_LT) ans = x < y;
			else if (op == TK_GT) ans = x > y;
			else if (op == TK_LE) ans = x <= y;
			else if (op == TK_GE) ans = x >= y;
			else if (op == TK_EQ) ans = x == y;
			else ans = x != y;
		}
		return {true, MakeSigned(ans ? 1 : 0)};
	}

	EvalResult parse_rel(bool eval)
	{
		EvalResult r = parse_shift(eval);
		while (cur().kind == TK_LT || cur().kind == TK_GT || cur().kind == TK_LE || cur().kind == TK_GE)
		{
			TokKind op = cur().kind;
			pos++;
			EvalResult rhs = parse_shift(eval);
			r = cmp(r, rhs, op, eval);
		}
		return r;
	}

	EvalResult parse_eq(bool eval)
	{
		EvalResult r = parse_rel(eval);
		while (cur().kind == TK_EQ || cur().kind == TK_NE)
		{
			TokKind op = cur().kind;
			pos++;
			EvalResult rhs = parse_rel(eval);
			r = cmp(r, rhs, op, eval);
		}
		return r;
	}

	EvalResult bitop(EvalResult a, EvalResult b, TokKind op, bool eval)
	{
		if (!a.ok || !b.ok) return {false, MakeSigned(0)};
		if (!eval) return {true, (a.value.is_unsigned || b.value.is_unsigned) ? MakeUnsigned(0) : MakeSigned(0)};
		if (a.value.is_unsigned || b.value.is_unsigned)
		{
			uint64_t x = AsU(a.value), y = AsU(b.value), z = 0;
			if (op == TK_AMP) z = x & y;
			else if (op == TK_XOR) z = x ^ y;
			else z = x | y;
			return {true, MakeUnsigned(z)};
		}
		int64_t x = AsS(a.value), y = AsS(b.value), z = 0;
		if (op == TK_AMP) z = x & y;
		else if (op == TK_XOR) z = x ^ y;
		else z = x | y;
		return {true, MakeSigned(z)};
	}

	EvalResult parse_band(bool eval)
	{
		EvalResult r = parse_eq(eval);
		while (eat(TK_AMP))
		{
			EvalResult rhs = parse_eq(eval);
			r = bitop(r, rhs, TK_AMP, eval);
		}
		return r;
	}

	EvalResult parse_bxor(bool eval)
	{
		EvalResult r = parse_band(eval);
		while (eat(TK_XOR))
		{
			EvalResult rhs = parse_band(eval);
			r = bitop(r, rhs, TK_XOR, eval);
		}
		return r;
	}

	EvalResult parse_bor(bool eval)
	{
		EvalResult r = parse_bxor(eval);
		while (eat(TK_BOR))
		{
			EvalResult rhs = parse_bxor(eval);
			r = bitop(r, rhs, TK_BOR, eval);
		}
		return r;
	}

	EvalResult parse_land(bool eval)
	{
		EvalResult left = parse_bor(eval);
		while (eat(TK_LAND))
		{
			bool lhs_truth = eval && left.ok && IsTrue(left.value);
			EvalResult right = parse_bor(eval && lhs_truth);
			if (!left.ok || !right.ok) return {false, MakeSigned(0)};
			if (eval) left = {true, MakeSigned((lhs_truth && IsTrue(right.value)) ? 1 : 0)};
			else left = {true, MakeSigned(0)};
		}
		return left;
	}

	EvalResult parse_lor(bool eval)
	{
		EvalResult left = parse_land(eval);
		while (eat(TK_LOR))
		{
			bool lhs_truth = eval && left.ok && IsTrue(left.value);
			EvalResult right = parse_land(eval && !lhs_truth);
			if (!left.ok || !right.ok) return {false, MakeSigned(0)};
			if (eval) left = {true, MakeSigned((lhs_truth || IsTrue(right.value)) ? 1 : 0)};
			else left = {true, MakeSigned(0)};
		}
		return left;
	}

	EvalResult parse_cond(bool eval)
	{
		EvalResult cond = parse_lor(eval);
		if (!eat(TK_Q)) return cond;
		bool cv = eval && cond.ok && IsTrue(cond.value);
		EvalResult t = parse_cond(eval && cv);
		if (!eat(TK_COLON)) return {false, MakeSigned(0)};
		EvalResult f = parse_cond(eval && !cv);
		if (!cond.ok || !t.ok || !f.ok) return {false, MakeSigned(0)};
		if (!eval)
		{
			return {true, (t.value.is_unsigned || f.value.is_unsigned) ? MakeUnsigned(0) : MakeSigned(0)};
		}
		CEValue chosen = cv ? t.value : f.value;
		if (t.value.is_unsigned || f.value.is_unsigned)
		{
			chosen = MakeUnsigned(cv ? AsU(t.value) : AsU(f.value));
		}
		else
		{
			chosen = MakeSigned(cv ? AsS(t.value) : AsS(f.value));
		}
		return {true, chosen};
	}
};

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		vector<PPToken> all = TokenizeToPPTokens(oss.str());

		vector<PPToken> line;
		for (const PPToken& p : all)
		{
			if (p.type == "eof") break;
			if (p.type == "whitespace-sequence") continue;
			if (p.type == "new-line")
			{
				if (!line.empty())
				{
					vector<CEToken> toks;
					if (!ConvertLineToTokens(line, toks))
					{
						cout << "error" << endl;
					}
					else
					{
						Parser parser;
						parser.toks = move(toks);
						EvalResult r = parser.parse_expression(true);
						if (!r.ok || parser.cur().kind != TK_END)
						{
							cout << "error" << endl;
						}
						else
						{
							cout << RenderValue(r.value) << endl;
						}
					}
					line.clear();
				}
				continue;
			}
			line.push_back(p);
		}
		if (!line.empty())
		{
			vector<CEToken> toks;
			if (!ConvertLineToTokens(line, toks))
			{
				cout << "error" << endl;
			}
			else
			{
				Parser parser;
				parser.toks = move(toks);
				EvalResult r = parser.parse_expression(true);
				if (!r.ok || parser.cur().kind != TK_END)
				{
					cout << "error" << endl;
				}
				else
				{
					cout << RenderValue(r.value) << endl;
				}
			}
		}

		cout << "eof" << endl;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
