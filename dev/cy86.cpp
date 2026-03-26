// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

#define PREPROC_EMBED_ONLY
#include "preproc.cpp"
#undef PREPROC_EMBED_ONLY

struct CyToken
{
	string term;
	string text;
	bool is_identifier = false;
	bool is_literal = false;
};

static vector<CyToken> BuildTokens(const vector<PPToken>& pptoks)
{
	vector<CyToken> out;
	for (const PPToken& t : pptoks)
	{
		if (t.type == "eof") break;
		if (t.type == "whitespace-sequence" || t.type == "new-line") continue;

		if (t.type == "identifier" || t.type == "preprocessing-op-or-punc")
		{
			auto it = StringToTokenTypeMap.find(t.data);
			if (it != StringToTokenTypeMap.end())
			{
				CyToken tok;
				tok.term = TokenTypeToStringMap.at(it->second);
				tok.text = t.data;
				if (tok.term.rfind("KW_", 0) == 0) throw runtime_error("C++ keyword in CY86");
				out.push_back(tok);
			}
			else if (t.type == "identifier")
			{
				CyToken tok;
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
			CyToken tok;
			tok.term = "TT_LITERAL";
			tok.text = t.data;
			tok.is_literal = true;
			out.push_back(tok);
			continue;
		}

		throw runtime_error("invalid token");
	}
	CyToken eof;
	eof.term = "ST_EOF";
	out.push_back(eof);
	return out;
}

static string ReadFile(const string& path)
{
	ifstream in(path.c_str(), ios::binary);
	if (!in) throw runtime_error("failed to open: " + path);
	string s((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
	return s;
}

static string GetSelfPath()
{
	char buf[4096];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0) return string("cy86");
	buf[n] = '\0';
	return string(buf);
}

// bootstrap system call interface, used by PA9SetFileExecutable
extern "C" long int syscall(long int n, ...) throw ();

static bool PA9SetFileExecutable(const string& path)
{
	int res = syscall(/* chmod */ 90, path.c_str(), 0755);
	return res == 0;
}

static string B64Encode(const vector<unsigned char>& in)
{
	static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	string out;
	out.reserve(((in.size() + 2) / 3) * 4);
	for (size_t i = 0; i < in.size(); i += 3)
	{
		unsigned a = in[i];
		unsigned b = (i + 1 < in.size()) ? in[i + 1] : 0;
		unsigned c = (i + 2 < in.size()) ? in[i + 2] : 0;
		unsigned v = (a << 16) | (b << 8) | c;
		out.push_back(tbl[(v >> 18) & 63]);
		out.push_back(tbl[(v >> 12) & 63]);
		out.push_back((i + 1 < in.size()) ? tbl[(v >> 6) & 63] : '=');
		out.push_back((i + 2 < in.size()) ? tbl[v & 63] : '=');
	}
	return out;
}

static vector<unsigned char> B64Decode(const string& s)
{
	static int inv[256];
	static bool init = false;
	if (!init)
	{
		for (int i = 0; i < 256; i++) inv[i] = -1;
		const string t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		for (int i = 0; i < 64; i++) inv[static_cast<unsigned char>(t[i])] = i;
		init = true;
	}
	vector<unsigned char> out;
	if (s.size() % 4 != 0) throw runtime_error("invalid base64");
	for (size_t i = 0; i < s.size(); i += 4)
	{
		int a = inv[static_cast<unsigned char>(s[i])];
		int b = inv[static_cast<unsigned char>(s[i + 1])];
		int c = (s[i + 2] == '=') ? -2 : inv[static_cast<unsigned char>(s[i + 2])];
		int d = (s[i + 3] == '=') ? -2 : inv[static_cast<unsigned char>(s[i + 3])];
		if (a < 0 || b < 0 || c == -1 || d == -1) throw runtime_error("invalid base64 char");
		unsigned v = (static_cast<unsigned>(a) << 18) | (static_cast<unsigned>(b) << 12) |
			(static_cast<unsigned>((c < 0) ? 0 : c) << 6) | static_cast<unsigned>((d < 0) ? 0 : d);
		out.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
		if (c >= 0) out.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
		if (d >= 0) out.push_back(static_cast<unsigned char>(v & 0xFF));
	}
	return out;
}

static void AppendU32(vector<unsigned char>& out, uint32_t v)
{
	for (int i = 0; i < 4; i++) out.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}

static uint32_t ReadU32(const vector<unsigned char>& in, size_t& p)
{
	if (p + 4 > in.size()) throw runtime_error("decode overflow");
	uint32_t v = 0;
	for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(in[p++]) << (8 * i);
	return v;
}

static vector<unsigned char> SerializeTokens(const vector<CyToken>& toks)
{
	vector<unsigned char> out;
	AppendU32(out, static_cast<uint32_t>(toks.size()));
	for (const CyToken& t : toks)
	{
		AppendU32(out, static_cast<uint32_t>(t.term.size()));
		out.insert(out.end(), t.term.begin(), t.term.end());
		AppendU32(out, static_cast<uint32_t>(t.text.size()));
		out.insert(out.end(), t.text.begin(), t.text.end());
		out.push_back(t.is_identifier ? 1 : 0);
		out.push_back(t.is_literal ? 1 : 0);
	}
	return out;
}

static vector<CyToken> DeserializeTokens(const vector<unsigned char>& in)
{
	size_t p = 0;
	uint32_t n = ReadU32(in, p);
	vector<CyToken> out;
	out.reserve(n);
	for (uint32_t i = 0; i < n; i++)
	{
		CyToken t;
		uint32_t a = ReadU32(in, p);
		if (p + a > in.size()) throw runtime_error("decode overflow");
		t.term.assign(reinterpret_cast<const char*>(&in[p]), reinterpret_cast<const char*>(&in[p]) + a);
		p += a;
		uint32_t b = ReadU32(in, p);
		if (p + b > in.size()) throw runtime_error("decode overflow");
		t.text.assign(reinterpret_cast<const char*>(&in[p]), reinterpret_cast<const char*>(&in[p]) + b);
		p += b;
		if (p + 2 > in.size()) throw runtime_error("decode overflow");
		t.is_identifier = in[p++] != 0;
		t.is_literal = in[p++] != 0;
		out.push_back(t);
	}
	return out;
}

static bool IsRegisterName(const string& s)
{
	static const unordered_set<string> regs = {
		"sp", "bp",
		"x8", "x16", "x32", "x64",
		"y8", "y16", "y32", "y64",
		"z8", "z16", "z32", "z64",
		"t8", "t16", "t32", "t64"
	};
	return regs.count(s) != 0;
}

struct Expr
{
	enum Base { B_LIT, B_LABEL, B_REG } base = B_LIT;
	string lit;
	string name;
	long long off = 0;
};

struct Operand
{
	enum Kind { O_REG, O_IMM, O_MEM } kind = O_IMM;
	string reg;
	Expr expr;
};

struct Statement
{
	vector<string> labels;
	bool is_data = false;
	vector<unsigned char> data;
	size_t align = 1;
	string opcode;
	vector<Operand> ops;
	uint64_t addr = 0;
};

struct Program
{
	vector<Statement> stmts;
	unordered_map<string, uint64_t> labels;
	unordered_map<uint64_t, size_t> instr_by_addr;
	vector<int32_t> instr_by_addr_fast;
	vector<unsigned char> memory;
	uint64_t entry = 0;
	uint64_t static_end = 0;
};

struct Parser
{
	const vector<CyToken>& toks;
	size_t p = 0;

	explicit Parser(const vector<CyToken>& t) : toks(t) {}

	const CyToken& Peek(size_t k = 0) const
	{
		size_t q = p + k;
		if (q >= toks.size()) return toks.back();
		return toks[q];
	}

	bool Is(const string& term, size_t k = 0) const { return Peek(k).term == term; }
	bool Eat(const string& term) { if (!Is(term)) return false; p++; return true; }
	void Expect(const string& term) { if (!Eat(term)) throw runtime_error("expected " + term); }

	string ExpectIdentifier()
	{
		if (!Is("TT_IDENTIFIER")) throw runtime_error("expected identifier");
		return toks[p++].text;
	}

	string ExpectLiteral()
	{
		if (!Is("TT_LITERAL")) throw runtime_error("expected literal");
		return toks[p++].text;
	}

	long long ParseOffsetLiteralSigned()
	{
		string lit = ExpectLiteral();
		ParsedInteger pi = ParseIntegerLiteral(lit);
		if (!pi.ok) throw runtime_error("expected integral literal");
		return static_cast<long long>(pi.value);
	}

	Expr ParseExpr(bool allow_reg)
	{
		Expr e;
		if (Eat("OP_MINUS"))
		{
			string lit = ExpectLiteral();
			e.base = Expr::B_LIT;
			e.lit = string("-") + lit;
		}
		else if (Is("TT_LITERAL"))
		{
			e.base = Expr::B_LIT;
			e.lit = ExpectLiteral();
		}
		else if (Is("TT_IDENTIFIER"))
		{
			string s = ExpectIdentifier();
			if (allow_reg && IsRegisterName(s))
			{
				e.base = Expr::B_REG;
				e.name = s;
			}
			else
			{
				e.base = Expr::B_LABEL;
				e.name = s;
			}
		}
		else throw runtime_error("expected expression");

		if (Eat("OP_PLUS"))
		{
			e.off += ParseOffsetLiteralSigned();
		}
		else if (Eat("OP_MINUS"))
		{
			e.off -= ParseOffsetLiteralSigned();
		}
		return e;
	}

	Operand ParseOperand()
	{
		if (Is("OP_LSQUARE"))
		{
			Operand o;
			o.kind = Operand::O_MEM;
			Expect("OP_LSQUARE");
			o.expr = ParseExpr(true);
			Expect("OP_RSQUARE");
			return o;
		}

		if (Is("TT_IDENTIFIER") && IsRegisterName(Peek().text))
		{
			Operand o;
			o.kind = Operand::O_REG;
			o.reg = ExpectIdentifier();
			return o;
		}

		Operand o;
		o.kind = Operand::O_IMM;
		if (Eat("OP_LPAREN"))
		{
			o.expr = ParseExpr(true);
			Expect("OP_RPAREN");
		}
		else
		{
			o.expr = ParseExpr(true);
		}
		return o;
	}

	pair<vector<unsigned char>, size_t> EncodeLiteralStmt(const string& lit, bool neg)
	{
		if (lit.find('"') != string::npos)
		{
			PPToken pt;
			pt.type = "string-literal";
			pt.data = lit;
			ParsedStringPiece ps = ParseStringPiece(pt);
			if (!ps.ok) throw runtime_error("invalid string literal");
			vector<unsigned char> b;
			size_t w = 1;
			if (ps.prefix == "u") w = 2;
			else if (ps.prefix == "U" || ps.prefix == "L") w = 4;
			for (int cp : ps.cps)
			{
				unsigned int v = static_cast<unsigned int>(cp);
				for (size_t i = 0; i < w; i++) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFFu));
			}
			for (size_t i = 0; i < w; i++) b.push_back(0);
			return make_pair(b, w);
		}

		if (lit.find('\'') != string::npos)
		{
			PPToken pt;
			pt.type = "character-literal";
			pt.data = lit;
			ParsedChar pc = ParseCharacterLiteral(pt);
			if (!pc.ok || pc.bytes.empty()) throw runtime_error("invalid character literal");
			vector<unsigned char> b = pc.bytes;
			if (neg)
			{
				unsigned long long v = 0;
				for (size_t i = 0; i < b.size() && i < 8; i++) v |= static_cast<unsigned long long>(b[i]) << (8 * i);
				v = static_cast<unsigned long long>(-static_cast<long long>(v));
				for (size_t i = 0; i < b.size(); i++) b[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xFFu);
			}
			return make_pair(b, b.size());
		}

		bool ud = false;
		string uds;
		string prefix;
		EFundamentalType ty = FT_DOUBLE;
		if (ParseFloatingLiteral(lit, ud, uds, prefix, ty))
		{
			if (neg) prefix = string("-") + prefix;
			if (ty == FT_FLOAT)
			{
				float f = strtof(prefix.c_str(), nullptr);
				unsigned char* p = reinterpret_cast<unsigned char*>(&f);
				return make_pair(vector<unsigned char>(p, p + sizeof(f)), sizeof(f));
			}
			if (ty == FT_DOUBLE)
			{
				double d = strtod(prefix.c_str(), nullptr);
				unsigned char* p = reinterpret_cast<unsigned char*>(&d);
				return make_pair(vector<unsigned char>(p, p + sizeof(d)), sizeof(d));
			}
			long double ld = strtold(prefix.c_str(), nullptr);
			unsigned char* p = reinterpret_cast<unsigned char*>(&ld);
			return make_pair(vector<unsigned char>(p, p + sizeof(ld)), sizeof(ld));
		}

		ParsedInteger pi = ParseIntegerLiteral(lit);
		if (!pi.ok) throw runtime_error("invalid literal");
		bool is_unsigned = (pi.suffix.find('u') != string::npos || pi.suffix.find('U') != string::npos);
		size_t n = 4;
		if (pi.suffix.find("ll") != string::npos || pi.suffix.find("LL") != string::npos ||
			pi.suffix.find("lL") != string::npos || pi.suffix.find("Ll") != string::npos) n = 8;
		else if (pi.suffix.find('l') != string::npos || pi.suffix.find('L') != string::npos) n = sizeof(long int);
		unsigned long long v = pi.value;
		if (neg)
		{
			long long sv = static_cast<long long>(v);
			v = static_cast<unsigned long long>(-sv);
			is_unsigned = false;
		}
		vector<unsigned char> b(n, 0);
		for (size_t i = 0; i < n && i < 8; i++) b[i] = static_cast<unsigned char>((v >> (8 * i)) & 0xFFu);
		if (!is_unsigned && n < 8)
		{
			unsigned long long sign = (v >> (n * 8 - 1)) & 1u;
			if (sign)
			{
				for (size_t i = n; i < 8; i++) { (void)i; }
			}
		}
		return make_pair(b, n);
	}

	pair<uint64_t, bool> ParseLiteralIntSimple(const string& lit)
	{
		bool neg = false;
		string s = lit;
		if (!s.empty() && s[0] == '-') { neg = true; s = s.substr(1); }
		if (s.find('\'') != string::npos)
		{
			PPToken pt;
			pt.type = "character-literal";
			pt.data = s;
			ParsedChar pc = ParseCharacterLiteral(pt);
			if (!pc.ok || pc.bytes.empty()) throw runtime_error("invalid character literal");
			uint64_t v = 0;
			for (size_t i = 0; i < pc.bytes.size() && i < 8; i++) v |= static_cast<uint64_t>(pc.bytes[i]) << (8 * i);
			if (neg) v = static_cast<uint64_t>(-static_cast<long long>(v));
			return make_pair(v, true);
		}
		ParsedInteger pi = ParseIntegerLiteral(s);
		if (!pi.ok) throw runtime_error("invalid integer literal");
		bool is_unsigned = (pi.suffix.find('u') != string::npos || pi.suffix.find('U') != string::npos);
		uint64_t v = pi.value;
		if (neg) { v = static_cast<uint64_t>(-static_cast<long long>(v)); is_unsigned = false; }
		return make_pair(v, !is_unsigned);
	}

	Program ParseProgram()
	{
		Program pg;
		while (!Is("ST_EOF"))
		{
			Statement st;
			while (Is("TT_IDENTIFIER") && Is("OP_COLON", 1))
			{
				st.labels.push_back(ExpectIdentifier());
				Expect("OP_COLON");
			}

			if (Is("TT_LITERAL") || (Is("OP_MINUS") && Is("TT_LITERAL", 1)))
			{
				bool neg = Eat("OP_MINUS");
				string lit = ExpectLiteral();
				auto ld = EncodeLiteralStmt(lit, neg);
				st.is_data = true;
				st.data = ld.first;
				st.align = max<size_t>(1, ld.second);
			}
			else
			{
				st.opcode = ExpectIdentifier();
				while (!Is("OP_SEMICOLON"))
				{
					st.ops.push_back(ParseOperand());
				}
				if (st.opcode.rfind("data", 0) == 0)
				{
					int bits = -1;
					size_t j = st.opcode.size();
					while (j > 0 && st.opcode[j - 1] >= '0' && st.opcode[j - 1] <= '9') j--;
					if (j < st.opcode.size()) bits = stoi(st.opcode.substr(j));
					if ((bits == 8 || bits == 16 || bits == 32 || bits == 64) && st.ops.size() == 1 && st.ops[0].kind == Operand::O_IMM && st.ops[0].expr.base == Expr::B_LIT)
					{
						bool is_signed = true;
						pair<uint64_t, bool> iv = ParseLiteralIntSimple(st.ops[0].expr.lit);
						is_signed = iv.second;
						uint64_t v = iv.first;
						size_t n = static_cast<size_t>(bits / 8);
						if (bits < 64) v &= ((1ULL << bits) - 1ULL);
						if (is_signed && bits < 64)
						{
							uint64_t sign = (v >> (bits - 1)) & 1u;
							if (sign) v |= ~((1ULL << bits) - 1ULL);
						}
						st.is_data = true;
						st.align = n;
						st.data.assign(n, 0);
						for (size_t bi = 0; bi < n; bi++) st.data[bi] = static_cast<unsigned char>((v >> (8 * bi)) & 0xFFu);
						st.opcode.clear();
						st.ops.clear();
					}
				}
			}
			Expect("OP_SEMICOLON");
			pg.stmts.push_back(st);
		}

		uint64_t cur = 0;
		bool first_stmt = true;
		for (size_t i = 0; i < pg.stmts.size(); i++)
		{
			Statement& st = pg.stmts[i];
			if (st.is_data)
			{
				uint64_t a = static_cast<uint64_t>(st.align);
				if (a > 1 && (cur % a) != 0) cur += (a - (cur % a));
			}
			st.addr = cur;
			if (first_stmt)
			{
				pg.entry = st.addr;
				first_stmt = false;
			}
			for (const string& lab : st.labels)
			{
				if (IsRegisterName(lab)) throw runtime_error("label conflicts with register");
				if (pg.labels.count(lab)) throw runtime_error("duplicate label");
				pg.labels[lab] = st.addr;
			}
			if (st.is_data) cur += st.data.size();
			else
			{
				if (pg.instr_by_addr.count(st.addr)) throw runtime_error("address collision");
				pg.instr_by_addr[st.addr] = i;
				cur += 1;
			}
		}
		if (pg.labels.count("start")) pg.entry = pg.labels["start"];
		pg.static_end = cur;
		pg.memory.assign(static_cast<size_t>(pg.static_end), 0);
		pg.instr_by_addr_fast.assign(static_cast<size_t>(pg.static_end), -1);
		for (const Statement& st : pg.stmts)
		{
			if (!st.is_data) continue;
			for (size_t i = 0; i < st.data.size(); i++) pg.memory[static_cast<size_t>(st.addr + i)] = st.data[i];
		}
		for (size_t i = 0; i < pg.stmts.size(); i++)
		{
			if (pg.stmts[i].is_data) continue;
			pg.instr_by_addr_fast[static_cast<size_t>(pg.stmts[i].addr)] = static_cast<int32_t>(i);
		}
		return pg;
	}
};

struct VM
{
	Program pg;
	uint64_t rx = 0, ry = 0, rz = 0, rt = 0, rsp = 0, rbp = 0;
	uint64_t pc = 0;
	uint64_t heap_cur = 0;
	int exit_code = 0;
	bool halted = false;
	unordered_map<string, pair<uint64_t, bool> > lit_cache;

	static constexpr uint64_t MEM_SIZE = 0x30000000ULL; // 768MB
	static constexpr uint64_t STACK_GUARD = 0x100000ULL;

	explicit VM(const Program& p) : pg(p)
	{
		if (pg.memory.size() > MEM_SIZE) throw runtime_error("program too large");
		pg.memory.resize(static_cast<size_t>(MEM_SIZE), 0);
		pc = pg.entry;
		heap_cur = (pg.static_end + 0xFFFu) & ~0xFFFu;
		rsp = MEM_SIZE - 0x1000u;
		rbp = rsp;
	}

	static uint64_t MaskBits(int bits)
	{
		if (bits >= 64) return numeric_limits<uint64_t>::max();
		return (1ULL << bits) - 1ULL;
	}

	uint64_t& RegRef64(const string& r)
	{
		if (r[0] == 'x') return rx;
		if (r[0] == 'y') return ry;
		if (r[0] == 'z') return rz;
		if (r[0] == 't') return rt;
		if (r == "sp") return rsp;
		if (r == "bp") return rbp;
		throw runtime_error("bad register");
	}

	int RegWidth(const string& r) const
	{
		if (r == "sp" || r == "bp") return 64;
		if (r.size() < 2) throw runtime_error("bad register");
		if (r[1] == '8' && r.size() == 2) return 8;
		if (r.substr(1) == "16") return 16;
		if (r.substr(1) == "32") return 32;
		if (r.substr(1) == "64") return 64;
		throw runtime_error("bad register");
	}

	uint64_t ReadReg(const string& r)
	{
		uint64_t v = RegRef64(r);
		int w = RegWidth(r);
		if (w == 64) return v;
		return v & MaskBits(w);
	}

	void WriteReg(const string& r, int bits, uint64_t v)
	{
		uint64_t& q = RegRef64(r);
		int rw = RegWidth(r);
		if (rw != bits) throw runtime_error("register width mismatch");
		if (bits == 64)
		{
			q = v;
			return;
		}
		if (bits == 32)
		{
			q = static_cast<uint32_t>(v);
			return;
		}
		uint64_t m = MaskBits(bits);
		q = (q & ~m) | (v & m);
	}

	void CheckRange(uint64_t a, size_t n)
	{
		if (a + n > pg.memory.size()) throw runtime_error("memory access out of range");
	}

	uint64_t ReadMemU(uint64_t a, int bits)
	{
		size_t n = static_cast<size_t>((bits + 7) / 8);
		CheckRange(a, n);
		uint64_t v = 0;
		for (size_t i = 0; i < n && i < 8; i++) v |= static_cast<uint64_t>(pg.memory[static_cast<size_t>(a + i)]) << (8 * i);
		if (bits < 64) v &= MaskBits(bits);
		return v;
	}

	void WriteMemU(uint64_t a, int bits, uint64_t v)
	{
		size_t n = static_cast<size_t>((bits + 7) / 8);
		CheckRange(a, n);
		for (size_t i = 0; i < n; i++) pg.memory[static_cast<size_t>(a + i)] = static_cast<unsigned char>((v >> (8 * i)) & 0xFFu);
	}

	long double ReadMemF80(uint64_t a)
	{
		CheckRange(a, 10);
		unsigned char raw[sizeof(long double)];
		memset(raw, 0, sizeof(raw));
		for (int i = 0; i < 10; i++) raw[i] = pg.memory[static_cast<size_t>(a + i)];
		long double v;
		memcpy(&v, raw, sizeof(v));
		return v;
	}

	void WriteMemF80(uint64_t a, long double v)
	{
		CheckRange(a, 10);
		unsigned char raw[sizeof(long double)];
		memcpy(raw, &v, sizeof(v));
		for (int i = 0; i < 10; i++) pg.memory[static_cast<size_t>(a + i)] = raw[i];
	}

	pair<uint64_t, bool> EvalLiteralInt(const string& lit)
	{
		auto itc = lit_cache.find(lit);
		if (itc != lit_cache.end()) return itc->second;
		bool neg = false;
		string s = lit;
		if (!s.empty() && s[0] == '-') { neg = true; s = s.substr(1); }
		if (s.find('"') != string::npos)
		{
			PPToken pt;
			pt.type = "string-literal";
			pt.data = s;
			ParsedStringPiece ps = ParseStringPiece(pt);
			if (!ps.ok) throw runtime_error("bad string literal");
			size_t w = 1;
			if (ps.prefix == "u") w = 2;
			else if (ps.prefix == "U" || ps.prefix == "L") w = 4;
			uint64_t v = 0;
			size_t k = 0;
			for (size_t ci = 0; ci < ps.cps.size() && k < 8; ci++)
			{
				unsigned int cp = static_cast<unsigned int>(ps.cps[ci]);
				for (size_t bi = 0; bi < w && k < 8; bi++, k++)
				{
					v |= static_cast<uint64_t>((cp >> (8 * bi)) & 0xFFu) << (8 * k);
				}
			}
			if (neg) v = static_cast<uint64_t>(-static_cast<long long>(v));
			pair<uint64_t, bool> res = make_pair(v, true);
			lit_cache[lit] = res;
			return res;
		}
		if (s.find('\'') != string::npos)
		{
			PPToken pt;
			pt.type = "character-literal";
			pt.data = s;
			ParsedChar pc = ParseCharacterLiteral(pt);
			if (!pc.ok || pc.bytes.empty()) throw runtime_error("bad char literal");
			uint64_t v = 0;
			for (size_t i = 0; i < pc.bytes.size() && i < 8; i++) v |= static_cast<uint64_t>(pc.bytes[i]) << (8 * i);
			if (neg) v = static_cast<uint64_t>(-static_cast<long long>(v));
			pair<uint64_t, bool> res = make_pair(v, true);
			lit_cache[lit] = res;
			return res;
		}
		ParsedInteger pi = ParseIntegerLiteral(s);
		if (!pi.ok) throw runtime_error(string("bad integer literal: ") + lit);
		bool us = (pi.suffix.find('u') != string::npos || pi.suffix.find('U') != string::npos);
		uint64_t v = pi.value;
		if (neg) { v = static_cast<uint64_t>(-static_cast<long long>(v)); us = false; }
		pair<uint64_t, bool> res = make_pair(v, !us);
		lit_cache[lit] = res;
		return res;
	}

	uint64_t EvalExprU64(const Expr& e)
	{
		uint64_t base = 0;
		if (e.base == Expr::B_REG) base = ReadReg(e.name);
		else if (e.base == Expr::B_LABEL)
		{
			auto it = pg.labels.find(e.name);
			if (it == pg.labels.end()) throw runtime_error("unknown label");
			base = it->second;
		}
		else
		{
			base = EvalLiteralInt(e.lit).first;
		}
		return static_cast<uint64_t>(base + static_cast<uint64_t>(e.off));
	}

	uint64_t ReadIntOperand(const Operand& op, int bits)
	{
		if (op.kind == Operand::O_REG) return ReadReg(op.reg) & MaskBits(bits);
		if (op.kind == Operand::O_MEM) return ReadMemU(EvalExprU64(op.expr), bits);
		uint64_t v = EvalExprU64(op.expr);
		if (bits < 64) v &= MaskBits(bits);
		return v;
	}

	int64_t ReadIntOperandS(const Operand& op, int bits)
	{
		uint64_t u = ReadIntOperand(op, bits);
		if (bits == 64) return static_cast<int64_t>(u);
		uint64_t m = 1ULL << (bits - 1);
		if (u & m) u |= ~MaskBits(bits);
		return static_cast<int64_t>(u);
	}

	void WriteIntOperand(const Operand& op, int bits, uint64_t v)
	{
		if (bits < 64) v &= MaskBits(bits);
		if (op.kind == Operand::O_REG) { WriteReg(op.reg, bits, v); return; }
		if (op.kind == Operand::O_MEM) { WriteMemU(EvalExprU64(op.expr), bits, v); return; }
		throw runtime_error("write to immediate");
	}

	long double ReadFloatOperand(const Operand& op, int bits)
	{
		if (bits == 80)
		{
			if (op.kind != Operand::O_MEM) throw runtime_error("f80 requires memory operand");
			return ReadMemF80(EvalExprU64(op.expr));
		}
		uint64_t u = ReadIntOperand(op, bits);
		if (bits == 32)
		{
			uint32_t w = static_cast<uint32_t>(u);
			float f;
			memcpy(&f, &w, sizeof(f));
			return static_cast<long double>(f);
		}
		double d;
		memcpy(&d, &u, sizeof(d));
		return static_cast<long double>(d);
	}

	void WriteFloatOperand(const Operand& op, int bits, long double v)
	{
		if (bits == 80)
		{
			if (op.kind != Operand::O_MEM) throw runtime_error("f80 requires memory operand");
			WriteMemF80(EvalExprU64(op.expr), v);
			return;
		}
		if (bits == 32)
		{
			float f = static_cast<float>(v);
			uint32_t u;
			memcpy(&u, &f, sizeof(u));
			WriteIntOperand(op, 32, u);
			return;
		}
		double d = static_cast<double>(v);
		uint64_t u;
		memcpy(&u, &d, sizeof(u));
		WriteIntOperand(op, 64, u);
	}

	int DoSyscall(int n, const vector<uint64_t>& a, uint64_t& ret)
	{
		if (n == 60)
		{
			exit_code = static_cast<int>(a.empty() ? 0 : a[0]);
			halted = true;
			ret = static_cast<uint64_t>(exit_code);
			return 0;
		}
		if (n == 0)
		{
			if (a.size() < 3) throw runtime_error("read args");
			int fd = static_cast<int>(a[0]);
			uint64_t addr = a[1];
			size_t cnt = static_cast<size_t>(a[2]);
			CheckRange(addr, cnt);
			ssize_t r = ::read(fd, &pg.memory[static_cast<size_t>(addr)], cnt);
			ret = static_cast<uint64_t>(static_cast<long long>(r));
			return 0;
		}
		if (n == 1)
		{
			if (a.size() < 3) throw runtime_error("write args");
			int fd = static_cast<int>(a[0]);
			uint64_t addr = a[1];
			size_t cnt = static_cast<size_t>(a[2]);
			CheckRange(addr, cnt);
			ssize_t r = ::write(fd, &pg.memory[static_cast<size_t>(addr)], cnt);
			ret = static_cast<uint64_t>(static_cast<long long>(r));
			return 0;
		}
		if (n == 9)
		{
			if (a.size() < 2) throw runtime_error("mmap args");
			uint64_t len = a[1];
			uint64_t start = (heap_cur + 0xFFFu) & ~0xFFFu;
			uint64_t end = start + len;
			if (end + STACK_GUARD >= rsp) { ret = static_cast<uint64_t>(-1LL); return 0; }
			heap_cur = end;
			ret = start;
			return 0;
		}
		ret = static_cast<uint64_t>(-1LL);
		return 0;
	}

	static int ParseWidthSuffix(const string& op)
	{
		size_t i = op.size();
		while (i > 0 && op[i - 1] >= '0' && op[i - 1] <= '9') i--;
		if (i == op.size()) return -1;
		return stoi(op.substr(i));
	}

	void ExecuteOne(const Statement& st)
	{
		const string& op = st.opcode;
		const vector<Operand>& o = st.ops;

		if (op.rfind("data", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (bits <= 0 || o.size() != 1) throw runtime_error("bad data opcode");
			uint64_t v = ReadIntOperand(o[0], bits);
			WriteMemU(pc, bits, v);
			pc += static_cast<uint64_t>(bits / 8);
			return;
		}

		uint64_t next_pc = pc + 1;

		if (op.rfind("move", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 2) throw runtime_error("bad move");
			if (bits == 80) WriteFloatOperand(o[0], 80, ReadFloatOperand(o[1], 80));
			else WriteIntOperand(o[0], bits, ReadIntOperand(o[1], bits));
		}
		else if (op == "jump")
		{
			if (o.size() != 1) throw runtime_error("bad jump");
			next_pc = ReadIntOperand(o[0], 64);
		}
		else if (op == "jumpif")
		{
			if (o.size() != 2) throw runtime_error("bad jumpif");
			if ((ReadIntOperand(o[0], 8) & 0xFFu) != 0) next_pc = ReadIntOperand(o[1], 64);
		}
		else if (op == "call")
		{
			if (o.size() != 1) throw runtime_error("bad call");
			rsp -= 8;
			WriteMemU(rsp, 64, next_pc);
			next_pc = ReadIntOperand(o[0], 64);
		}
		else if (op == "ret")
		{
			if (!o.empty()) throw runtime_error("bad ret");
			next_pc = ReadMemU(rsp, 64);
			rsp += 8;
		}
		else if (op.rfind("not", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 2) throw runtime_error("bad not");
			WriteIntOperand(o[0], bits, (~ReadIntOperand(o[1], bits)) & MaskBits(bits));
		}
		else if (op.rfind("and", 0) == 0 || op.rfind("or", 0) == 0 || op.rfind("xor", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad bitwise");
			uint64_t a = ReadIntOperand(o[1], bits);
			uint64_t b = ReadIntOperand(o[2], bits);
			uint64_t r = 0;
			if (op.rfind("and", 0) == 0) r = a & b;
			else if (op.rfind("or", 0) == 0) r = a | b;
			else r = a ^ b;
			WriteIntOperand(o[0], bits, r);
		}
		else if (op.rfind("lshift", 0) == 0 || op.rfind("srshift", 0) == 0 || op.rfind("urshift", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad shift");
			uint64_t sh = ReadIntOperand(o[2], 8) & 0xFFu;
			uint64_t r = 0;
			if (op.rfind("lshift", 0) == 0) r = (ReadIntOperand(o[1], bits) << sh) & MaskBits(bits);
			else if (op.rfind("urshift", 0) == 0) r = (ReadIntOperand(o[1], bits) >> sh) & MaskBits(bits);
			else
			{
				int64_t sv = ReadIntOperandS(o[1], bits);
				int64_t rr = (sh >= 64) ? (sv < 0 ? -1 : 0) : (sv >> sh);
				r = static_cast<uint64_t>(rr) & MaskBits(bits);
			}
			WriteIntOperand(o[0], bits, r);
		}
		else if (op.find("conv") != string::npos)
		{
			if (o.size() != 2) throw runtime_error("bad conv");
			if (op.rfind("s", 0) == 0 && op.find("convf80") != string::npos)
			{
				int bits = ParseWidthSuffix(op.substr(1, op.find("conv") - 1));
				WriteFloatOperand(o[0], 80, static_cast<long double>(ReadIntOperandS(o[1], bits)));
			}
			else if (op.rfind("u", 0) == 0 && op.find("convf80") != string::npos)
			{
				int bits = ParseWidthSuffix(op.substr(1, op.find("conv") - 1));
				WriteFloatOperand(o[0], 80, static_cast<long double>(ReadIntOperand(o[1], bits)));
			}
			else if (op.rfind("f32convf80", 0) == 0)
			{
				WriteFloatOperand(o[0], 80, static_cast<long double>(static_cast<float>(ReadFloatOperand(o[1], 32))));
			}
			else if (op.rfind("f64convf80", 0) == 0)
			{
				WriteFloatOperand(o[0], 80, static_cast<long double>(static_cast<double>(ReadFloatOperand(o[1], 64))));
			}
			else if (op.rfind("f80convs", 0) == 0)
			{
				int bits = ParseWidthSuffix(op.substr(string("f80convs").size()));
				long double v = ReadFloatOperand(o[1], 80);
				WriteIntOperand(o[0], bits, static_cast<uint64_t>(static_cast<long long>(v)));
			}
			else if (op.rfind("f80convu", 0) == 0)
			{
				int bits = ParseWidthSuffix(op.substr(string("f80convu").size()));
				long double v = ReadFloatOperand(o[1], 80);
				WriteIntOperand(o[0], bits, static_cast<uint64_t>(v));
			}
			else if (op.rfind("f80convf32", 0) == 0)
			{
				WriteFloatOperand(o[0], 32, ReadFloatOperand(o[1], 80));
			}
			else if (op.rfind("f80convf64", 0) == 0)
			{
				WriteFloatOperand(o[0], 64, ReadFloatOperand(o[1], 80));
			}
			else throw runtime_error("unsupported conversion opcode");
		}
		else if (op.rfind("iadd", 0) == 0 || op.rfind("isub", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad iadd/isub");
			uint64_t a = ReadIntOperand(o[1], bits);
			uint64_t b = ReadIntOperand(o[2], bits);
			uint64_t r = (op.rfind("iadd", 0) == 0) ? (a + b) : (a - b);
			WriteIntOperand(o[0], bits, r);
		}
		else if (op.rfind("fadd", 0) == 0 || op.rfind("fsub", 0) == 0 || op.rfind("fmul", 0) == 0 || op.rfind("fdiv", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad float arith");
			long double a = ReadFloatOperand(o[1], bits);
			long double b = ReadFloatOperand(o[2], bits);
			long double r = 0;
			if (op.rfind("fadd", 0) == 0) r = a + b;
			else if (op.rfind("fsub", 0) == 0) r = a - b;
			else if (op.rfind("fmul", 0) == 0) r = a * b;
			else r = a / b;
			WriteFloatOperand(o[0], bits, r);
		}
		else if (op.rfind("smul", 0) == 0 || op.rfind("umul", 0) == 0 || op.rfind("sdiv", 0) == 0 || op.rfind("udiv", 0) == 0 || op.rfind("smod", 0) == 0 || op.rfind("umod", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad int arith");
			if (op[0] == 's')
			{
				int64_t a = ReadIntOperandS(o[1], bits);
				int64_t b = ReadIntOperandS(o[2], bits);
				int64_t r = 0;
				if (op.rfind("smul", 0) == 0) r = a * b;
				else if (op.rfind("sdiv", 0) == 0) r = a / b;
				else r = a % b;
				WriteIntOperand(o[0], bits, static_cast<uint64_t>(r));
			}
			else
			{
				uint64_t a = ReadIntOperand(o[1], bits);
				uint64_t b = ReadIntOperand(o[2], bits);
				uint64_t r = 0;
				if (op.rfind("umul", 0) == 0) r = a * b;
				else if (op.rfind("udiv", 0) == 0) r = b ? (a / b) : 0;
				else r = b ? (a % b) : 0;
				WriteIntOperand(o[0], bits, r);
			}
		}
		else if (op.rfind("ieq", 0) == 0 || op.rfind("ine", 0) == 0 || op.rfind("slt", 0) == 0 || op.rfind("ult", 0) == 0 ||
			op.rfind("sgt", 0) == 0 || op.rfind("ugt", 0) == 0 || op.rfind("sle", 0) == 0 || op.rfind("ule", 0) == 0 ||
			op.rfind("sge", 0) == 0 || op.rfind("uge", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad int cmp");
			bool r = false;
			if (op[0] == 'i')
			{
				uint64_t a = ReadIntOperand(o[1], bits);
				uint64_t b = ReadIntOperand(o[2], bits);
				r = (op.rfind("ieq", 0) == 0) ? (a == b) : (a != b);
			}
			else if (op[0] == 's')
			{
				int64_t a = ReadIntOperandS(o[1], bits);
				int64_t b = ReadIntOperandS(o[2], bits);
				if (op.rfind("slt", 0) == 0) r = a < b;
				else if (op.rfind("sgt", 0) == 0) r = a > b;
				else if (op.rfind("sle", 0) == 0) r = a <= b;
				else r = a >= b;
			}
			else
			{
				uint64_t a = ReadIntOperand(o[1], bits);
				uint64_t b = ReadIntOperand(o[2], bits);
				if (op.rfind("ult", 0) == 0) r = a < b;
				else if (op.rfind("ugt", 0) == 0) r = a > b;
				else if (op.rfind("ule", 0) == 0) r = a <= b;
				else r = a >= b;
			}
			WriteIntOperand(o[0], 8, r ? 1 : 0);
		}
		else if (op.rfind("feq", 0) == 0 || op.rfind("fne", 0) == 0 || op.rfind("flt", 0) == 0 || op.rfind("fgt", 0) == 0 || op.rfind("fle", 0) == 0 || op.rfind("fge", 0) == 0)
		{
			int bits = ParseWidthSuffix(op);
			if (o.size() != 3) throw runtime_error("bad float cmp");
			long double a = ReadFloatOperand(o[1], bits);
			long double b = ReadFloatOperand(o[2], bits);
			bool r = false;
			if (op.rfind("feq", 0) == 0) r = a == b;
			else if (op.rfind("fne", 0) == 0) r = a != b;
			else if (op.rfind("flt", 0) == 0) r = a < b;
			else if (op.rfind("fgt", 0) == 0) r = a > b;
			else if (op.rfind("fle", 0) == 0) r = a <= b;
			else r = a >= b;
			WriteIntOperand(o[0], 8, r ? 1 : 0);
		}
		else if (op.rfind("syscall", 0) == 0)
		{
			int n = ParseWidthSuffix(op);
			if (n < 0) throw runtime_error("bad syscall opcode");
			if (o.size() != static_cast<size_t>(n + 2)) throw runtime_error("bad syscall operand count");
			uint64_t num = ReadIntOperand(o[1], 64);
			vector<uint64_t> args;
			args.reserve(static_cast<size_t>(n));
			for (int i = 0; i < n; i++) args.push_back(ReadIntOperand(o[static_cast<size_t>(i + 2)], 64));
			uint64_t rv = 0;
			DoSyscall(static_cast<int>(num), args, rv);
			WriteIntOperand(o[0], 64, rv);
		}
		else throw runtime_error("unsupported opcode: " + op);

		pc = next_pc;
	}

	int Run()
	{
		for (;;)
		{
			if (halted) return exit_code;
			if (pc >= pg.instr_by_addr_fast.size()) throw runtime_error("pc not on instruction");
			int32_t idx = pg.instr_by_addr_fast[static_cast<size_t>(pc)];
			if (idx < 0) throw runtime_error("pc not on instruction");
			const Statement& st = pg.stmts[static_cast<size_t>(idx)];
			ExecuteOne(st);
		}
	}
};

static vector<CyToken> CollectTokensFromSources(const vector<string>& srcfiles)
{
	PreprocState st;
	vector<PPToken> pre;
	for (const string& src : srcfiles) ProcessFile(st, src, pre);
	return BuildTokens(pre);
}

static int RunFromEncoded(const string& b64)
{
	vector<unsigned char> raw = B64Decode(b64);
	vector<CyToken> toks = DeserializeTokens(raw);
	Parser parser(toks);
	Program pg = parser.ParseProgram();
	VM vm(pg);
	return vm.Run();
}

static int RunFromScript(const string& path)
{
	string s = ReadFile(path);
	const string marker = "\n__CY86_B64__\n";
	size_t p = s.find(marker);
	if (p == string::npos) throw runtime_error("missing embedded payload");
	p += marker.size();
	size_t q = s.find('\n', p);
	string b64 = (q == string::npos) ? s.substr(p) : s.substr(p, q - p);
	return RunFromEncoded(b64);
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;
		for (int i = 1; i < argc; i++) args.emplace_back(argv[i]);

		if (args.size() == 2 && args[0] == "--run-b64")
		{
			int ec = RunFromEncoded(args[1]);
			return ec;
		}
		if (args.size() >= 2 && args[0] == "--run-script")
		{
			int ec = RunFromScript(args[1]);
			return ec;
		}

		if (args.size() < 3 || args[0] != "-o") throw logic_error("invalid usage");

		string outfile = args[1];
		vector<string> srcfiles;
		for (size_t i = 2; i < args.size(); i++) srcfiles.push_back(args[i]);

		vector<CyToken> toks = CollectTokensFromSources(srcfiles);
		Parser parser(toks);
		(void)parser.ParseProgram();

		vector<unsigned char> blob = SerializeTokens(toks);
		string b64 = B64Encode(blob);
		string self = GetSelfPath();

		ofstream out(outfile.c_str(), ios::binary);
		if (!out) throw runtime_error("failed to open output");
		out << "#!/bin/sh\n";
		out << "exec \"" << self << "\" --run-script \"$0\" \"$@\"\n";
		out << "__CY86_B64__\n";
		out << b64 << "\n";
		out.close();
		if (!PA9SetFileExecutable(outfile)) throw runtime_error("chmod failed");
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
