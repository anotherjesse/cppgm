// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

#define CPPGM_PREPROC_LIBRARY
#include "preproc.cpp"

#include "Cy86RuntimeBlob.h"

struct ElfHeader
{
	unsigned char ident[16] =
	{
		0x7f, 'E', 'L', 'F',
		2,
		1,
		1,
		0,
		0,
		0, 0, 0, 0, 0, 0, 0
	};

	short int type = 2;
	short int machine = 0x3E;
	int version = 1;
	long int entry = 0;
	long int phoff = 64;
	long int shoff = 0;
	int processor_flags = 0;
	short int ehsize = 64;
	short int phentsize = 56;
	short int phnum = 1;
	short int shentsize = 0;
	short int shnum = 0;
	short int shstrndx = 0;
};

struct ProgramSegmentHeader
{
	int type = 1;

	static constexpr int executable = 1 << 0;
	static constexpr int writable = 1 << 1;
	static constexpr int readable = 1 << 2;

	int flags = executable | writable | readable;
	long int offset = 0;
	long int vaddr = 0x400000;
	long int paddr = 0;
	long int filesz = 0;
	long int memsz = 0;
	long int align = 0;
};

extern "C" long int syscall(long int n, ...) throw ();

bool PA9SetFileExecutable(const string& path)
{
	return syscall(90, path.c_str(), 0755) == 0;
}

struct CompileError : runtime_error
{
	CompileError(const string& file, long line, const string& message)
		: runtime_error(file + ":" + to_string(line > 0 ? line : 1) + ":1: error: " + message)
	{
	}
};

enum RuntimeOpcode
{
	ROP_MOVE = 1,
	ROP_NOT,
	ROP_AND,
	ROP_OR,
	ROP_XOR,
	ROP_ADD,
	ROP_SUB,
	ROP_MUL_S,
	ROP_MUL_U,
	ROP_DIV_S,
	ROP_DIV_U,
	ROP_MOD_S,
	ROP_MOD_U,
	ROP_SHL,
	ROP_SAR,
	ROP_SHR,
	ROP_CMP_EQ,
	ROP_CMP_NE,
	ROP_CMP_LT_S,
	ROP_CMP_GT_S,
	ROP_CMP_LE_S,
	ROP_CMP_GE_S,
	ROP_CMP_LT_U,
	ROP_CMP_GT_U,
	ROP_CMP_LE_U,
	ROP_CMP_GE_U,
	ROP_SYSCALL,
	ROP_JUMP,
	ROP_JUMPIF,
	ROP_CALL,
	ROP_RET,
	ROP_CONV_S_TO_F80,
	ROP_CONV_U_TO_F80,
	ROP_CONV_F32_TO_F80,
	ROP_CONV_F64_TO_F80,
	ROP_CONV_F80_TO_S,
	ROP_CONV_F80_TO_U,
	ROP_CONV_F80_TO_F32,
	ROP_CONV_F80_TO_F64,
	ROP_FADD,
	ROP_FSUB,
	ROP_FMUL,
	ROP_FDIV,
	ROP_FCMP_EQ,
	ROP_FCMP_NE,
	ROP_FCMP_LT,
	ROP_FCMP_GT,
	ROP_FCMP_LE,
	ROP_FCMP_GE
};

enum OperandMode
{
	OM_NONE = 0,
	OM_REG = 1,
	OM_IMM = 2,
	OM_MEM_ABS = 3,
	OM_MEM_REG_DISP = 4
};

enum RegisterId
{
	R_SP,
	R_BP,
	R_X8,
	R_X16,
	R_X32,
	R_X64,
	R_Y8,
	R_Y16,
	R_Y32,
	R_Y64,
	R_Z8,
	R_Z16,
	R_Z32,
	R_Z64,
	R_T8,
	R_T16,
	R_T32,
	R_T64
};

struct RegisterSpec
{
	RegisterId id;
	int width;
};

const map<string, RegisterSpec> kRegisters =
{
	{"sp", {R_SP, 8}},
	{"bp", {R_BP, 8}},
	{"x8", {R_X8, 1}},
	{"x16", {R_X16, 2}},
	{"x32", {R_X32, 4}},
	{"x64", {R_X64, 8}},
	{"y8", {R_Y8, 1}},
	{"y16", {R_Y16, 2}},
	{"y32", {R_Y32, 4}},
	{"y64", {R_Y64, 8}},
	{"z8", {R_Z8, 1}},
	{"z16", {R_Z16, 2}},
	{"z32", {R_Z32, 4}},
	{"z64", {R_Z64, 8}},
	{"t8", {R_T8, 1}},
	{"t16", {R_T16, 2}},
	{"t32", {R_T32, 4}},
	{"t64", {R_T64, 8}}
};

struct OpcodeSpec
{
	string name;
	bool is_data = false;
	RuntimeOpcode runtime_op = ROP_MOVE;
	int width = 0;
	int syscall_arity = 0;
	vector<string> operands;
};

int WidthFromBits(int bits)
{
	if (bits == 80)
		return 10;
	return bits / 8;
}

string WidthSuffix(int bits)
{
	return to_string(bits);
}

map<string, OpcodeSpec> BuildOpcodeSpecs()
{
	map<string, OpcodeSpec> out;

	auto add = [&](const string& name, RuntimeOpcode op, int width, const vector<string>& operands)
	{
		OpcodeSpec spec;
		spec.name = name;
		spec.runtime_op = op;
		spec.width = width;
		spec.operands = operands;
		out[name] = spec;
	};

	auto add_data = [&](int bits)
	{
		OpcodeSpec spec;
		spec.name = "data" + WidthSuffix(bits);
		spec.is_data = true;
		spec.width = WidthFromBits(bits);
		spec.operands.push_back("rI" + WidthSuffix(bits));
		out[spec.name] = spec;
	};

	for (int bits : vector<int>{8, 16, 32, 64})
		add_data(bits);

	add("move8", ROP_MOVE, 1, {"w8", "r8"});
	add("move16", ROP_MOVE, 2, {"w16", "r16"});
	add("move32", ROP_MOVE, 4, {"w32", "r32"});
	add("move64", ROP_MOVE, 8, {"w64", "r64"});
	add("jump", ROP_JUMP, 8, {"ar64"});
	add("jumpif", ROP_JUMPIF, 1, {"br8", "ar64"});
	add("call", ROP_CALL, 8, {"ar64"});
	add("ret", ROP_RET, 0, {});

	for (int bits : vector<int>{8, 16, 32, 64})
	{
		int width = WidthFromBits(bits);
		string suffix = WidthSuffix(bits);
		add("not" + suffix, ROP_NOT, width, {"w" + suffix, "r" + suffix});
		add("and" + suffix, ROP_AND, width, {"w" + suffix, "r" + suffix, "r" + suffix});
		add("or" + suffix, ROP_OR, width, {"w" + suffix, "r" + suffix, "r" + suffix});
		add("xor" + suffix, ROP_XOR, width, {"w" + suffix, "r" + suffix, "r" + suffix});
		add("lshift" + suffix, ROP_SHL, width, {"iw" + suffix, "ir" + suffix, "ur8"});
		add("srshift" + suffix, ROP_SAR, width, {"sw" + suffix, "sr" + suffix, "ur8"});
		add("urshift" + suffix, ROP_SHR, width, {"uw" + suffix, "ur" + suffix, "ur8"});
		add("iadd" + suffix, ROP_ADD, width, {"iw" + suffix, "ir" + suffix, "ir" + suffix});
		add("isub" + suffix, ROP_SUB, width, {"iw" + suffix, "ir" + suffix, "ir" + suffix});
		add("smul" + suffix, ROP_MUL_S, width, {"sw" + suffix, "sr" + suffix, "sr" + suffix});
		add("umul" + suffix, ROP_MUL_U, width, {"uw" + suffix, "ur" + suffix, "ur" + suffix});
		add("sdiv" + suffix, ROP_DIV_S, width, {"sw" + suffix, "sr" + suffix, "sr" + suffix});
		add("udiv" + suffix, ROP_DIV_U, width, {"uw" + suffix, "ur" + suffix, "ur" + suffix});
		add("smod" + suffix, ROP_MOD_S, width, {"sw" + suffix, "sr" + suffix, "sr" + suffix});
		add("umod" + suffix, ROP_MOD_U, width, {"uw" + suffix, "ur" + suffix, "ur" + suffix});
		add("ieq" + suffix, ROP_CMP_EQ, width, {"wb8", "ir" + suffix, "ir" + suffix});
		add("ine" + suffix, ROP_CMP_NE, width, {"wb8", "ir" + suffix, "ir" + suffix});
		add("slt" + suffix, ROP_CMP_LT_S, width, {"wb8", "sr" + suffix, "sr" + suffix});
		add("ult" + suffix, ROP_CMP_LT_U, width, {"wb8", "ur" + suffix, "ur" + suffix});
		add("sgt" + suffix, ROP_CMP_GT_S, width, {"wb8", "sr" + suffix, "sr" + suffix});
		add("ugt" + suffix, ROP_CMP_GT_U, width, {"wb8", "ur" + suffix, "ur" + suffix});
		add("sle" + suffix, ROP_CMP_LE_S, width, {"wb8", "sr" + suffix, "sr" + suffix});
		add("ule" + suffix, ROP_CMP_LE_U, width, {"wb8", "ur" + suffix, "ur" + suffix});
		add("sge" + suffix, ROP_CMP_GE_S, width, {"wb8", "sr" + suffix, "sr" + suffix});
		add("uge" + suffix, ROP_CMP_GE_U, width, {"wb8", "ur" + suffix, "ur" + suffix});
	}

	add("s8convf80", ROP_CONV_S_TO_F80, 1, {"fw80", "sr8"});
	add("s16convf80", ROP_CONV_S_TO_F80, 2, {"fw80", "sr16"});
	add("s32convf80", ROP_CONV_S_TO_F80, 4, {"fw80", "sr32"});
	add("s64convf80", ROP_CONV_S_TO_F80, 8, {"fw80", "sr64"});
	add("u8convf80", ROP_CONV_U_TO_F80, 1, {"fw80", "ur8"});
	add("u16convf80", ROP_CONV_U_TO_F80, 2, {"fw80", "ur16"});
	add("u32convf80", ROP_CONV_U_TO_F80, 4, {"fw80", "ur32"});
	add("u64convf80", ROP_CONV_U_TO_F80, 8, {"fw80", "ur64"});
	add("f32convf80", ROP_CONV_F32_TO_F80, 4, {"fw80", "fr32"});
	add("f64convf80", ROP_CONV_F64_TO_F80, 8, {"fw80", "fr64"});
	add("f80convs8", ROP_CONV_F80_TO_S, 1, {"sw8", "fr80"});
	add("f80convs16", ROP_CONV_F80_TO_S, 2, {"sw16", "fr80"});
	add("f80convs32", ROP_CONV_F80_TO_S, 4, {"sw32", "fr80"});
	add("f80convs64", ROP_CONV_F80_TO_S, 8, {"sw64", "fr80"});
	add("f80convu8", ROP_CONV_F80_TO_U, 1, {"uw8", "fr80"});
	add("f80convu16", ROP_CONV_F80_TO_U, 2, {"uw16", "fr80"});
	add("f80convu32", ROP_CONV_F80_TO_U, 4, {"uw32", "fr80"});
	add("f80convu64", ROP_CONV_F80_TO_U, 8, {"uw64", "fr80"});
	add("f80convf32", ROP_CONV_F80_TO_F32, 4, {"fw32", "fr80"});
	add("f80convf64", ROP_CONV_F80_TO_F64, 8, {"fw64", "fr80"});

	for (int bits : vector<int>{32, 64, 80})
	{
		int width = WidthFromBits(bits);
		string suffix = WidthSuffix(bits);
		add("fadd" + suffix, ROP_FADD, width, {"fw" + suffix, "fr" + suffix, "fr" + suffix});
		add("fsub" + suffix, ROP_FSUB, width, {"fw" + suffix, "fr" + suffix, "fr" + suffix});
		add("fmul" + suffix, ROP_FMUL, width, {"fw" + suffix, "fr" + suffix, "fr" + suffix});
		add("fdiv" + suffix, ROP_FDIV, width, {"fw" + suffix, "fr" + suffix, "fr" + suffix});
		add("feq" + suffix, ROP_FCMP_EQ, width, {"wb8", "fr" + suffix, "fr" + suffix});
		add("fne" + suffix, ROP_FCMP_NE, width, {"wb8", "fr" + suffix, "fr" + suffix});
		add("flt" + suffix, ROP_FCMP_LT, width, {"wb8", "fr" + suffix, "fr" + suffix});
		add("fgt" + suffix, ROP_FCMP_GT, width, {"wb8", "fr" + suffix, "fr" + suffix});
		add("fle" + suffix, ROP_FCMP_LE, width, {"wb8", "fr" + suffix, "fr" + suffix});
		add("fge" + suffix, ROP_FCMP_GE, width, {"wb8", "fr" + suffix, "fr" + suffix});
	}

	for (int n = 0; n <= 6; ++n)
	{
		OpcodeSpec spec;
		spec.name = "syscall" + to_string(n);
		spec.runtime_op = ROP_SYSCALL;
		spec.width = 8;
		spec.syscall_arity = n;
		spec.operands.push_back("w64");
		for (int i = 0; i < n + 1; ++i)
			spec.operands.push_back("r64");
		out[spec.name] = spec;
	}

	return out;
}

const map<string, OpcodeSpec> kOpcodeSpecs = BuildOpcodeSpecs();

bool IsReservedIdentifier(const MacroToken& token)
{
	if (token.type != PPT_IDENTIFIER)
		return false;
	auto it = StringToTokenTypeMap.find(token.data);
	if (it == StringToTokenTypeMap.end())
		return false;
	return true;
}

void FailAt(const MacroToken& token, const string& message)
{
	throw CompileError(token.source_file, token.source_line, message);
}

void AppendLittleEndian(vector<unsigned char>& out, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i)
		out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xff));
}

void WriteLittleEndian(vector<unsigned char>& out, size_t offset, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; ++i)
		out[offset + i] = static_cast<unsigned char>((value >> (i * 8)) & 0xff);
}

unsigned long long ReadLittleEndianUnsigned(const vector<unsigned char>& bytes)
{
	unsigned long long value = 0;
	size_t width = min(bytes.size(), sizeof(value));
	for (size_t i = 0; i < width; ++i)
		value |= static_cast<unsigned long long>(bytes[i]) << (i * 8);
	return value;
}

size_t AlignUp(size_t value, size_t align)
{
	if (align == 0 || align == 1)
		return value;
	size_t mod = value % align;
	return mod == 0 ? value : value + (align - mod);
}

size_t FundamentalSize(EFundamentalType type)
{
	switch (type)
	{
	case FT_SIGNED_CHAR: return sizeof(signed char);
	case FT_SHORT_INT: return sizeof(short int);
	case FT_INT: return sizeof(int);
	case FT_LONG_INT: return sizeof(long int);
	case FT_LONG_LONG_INT: return sizeof(long long int);
	case FT_UNSIGNED_CHAR: return sizeof(unsigned char);
	case FT_UNSIGNED_SHORT_INT: return sizeof(unsigned short int);
	case FT_UNSIGNED_INT: return sizeof(unsigned int);
	case FT_UNSIGNED_LONG_INT: return sizeof(unsigned long int);
	case FT_UNSIGNED_LONG_LONG_INT: return sizeof(unsigned long long int);
	case FT_WCHAR_T: return sizeof(wchar_t);
	case FT_CHAR: return sizeof(char);
	case FT_CHAR16_T: return sizeof(char16_t);
	case FT_CHAR32_T: return sizeof(char32_t);
	case FT_BOOL: return sizeof(bool);
	case FT_FLOAT: return sizeof(float);
	case FT_DOUBLE: return sizeof(double);
	case FT_LONG_DOUBLE: return sizeof(long double);
	case FT_NULLPTR_T: return sizeof(nullptr_t);
	default: return 0;
	}
}

bool IsIntegralFundamental(EFundamentalType type)
{
	switch (type)
	{
	case FT_SIGNED_CHAR:
	case FT_SHORT_INT:
	case FT_INT:
	case FT_LONG_INT:
	case FT_LONG_LONG_INT:
	case FT_UNSIGNED_CHAR:
	case FT_UNSIGNED_SHORT_INT:
	case FT_UNSIGNED_INT:
	case FT_UNSIGNED_LONG_INT:
	case FT_UNSIGNED_LONG_LONG_INT:
	case FT_WCHAR_T:
	case FT_CHAR:
	case FT_CHAR16_T:
	case FT_CHAR32_T:
	case FT_BOOL:
		return true;
	default:
		return false;
	}
}

bool IsSignedFundamental(EFundamentalType type)
{
	switch (type)
	{
	case FT_SIGNED_CHAR:
	case FT_SHORT_INT:
	case FT_INT:
	case FT_LONG_INT:
	case FT_LONG_LONG_INT:
		return true;
	default:
		return false;
	}
}

struct LiteralValue
{
	MacroToken token;
	EFundamentalType type = FT_VOID;
	vector<unsigned char> bytes;
	bool is_string = false;
	bool is_integral = false;
	bool is_signed_integral = false;
	size_t alignment = 1;
};

LiteralValue ParseLiteralValue(const MacroToken& token)
{
	LiteralValue out;
	out.token = token;

	if (token.type == PPT_PP_NUMBER)
	{
		ParsedNumberLiteral parsed = ParsePPNumberLiteral(token.data);
		if (!parsed.ok || parsed.is_user_defined)
			FailAt(token, "invalid literal");
		if (parsed.is_integer)
		{
			EFundamentalType type = ChooseIntegerType(parsed.int_info);
			if (type == FT_VOID)
				FailAt(token, "invalid integer literal type");
			out.type = type;
			out.is_integral = true;
			out.is_signed_integral = IsSignedFundamental(type);
			out.alignment = FundamentalSize(type);
			AppendLittleEndian(out.bytes, parsed.int_info.value, FundamentalSize(type));
		}
		else if (parsed.float_type == FT_FLOAT)
		{
			float value = PA2Decode_float(token.data);
			out.type = FT_FLOAT;
			out.alignment = sizeof(value);
			out.bytes.resize(sizeof(value));
			memcpy(out.bytes.data(), &value, sizeof(value));
		}
		else if (parsed.float_type == FT_LONG_DOUBLE)
		{
			long double value = PA2Decode_long_double(token.data);
			out.type = FT_LONG_DOUBLE;
			out.alignment = sizeof(value);
			out.bytes.resize(sizeof(value));
			memcpy(out.bytes.data(), &value, sizeof(value));
		}
		else
		{
			double value = PA2Decode_double(token.data);
			out.type = FT_DOUBLE;
			out.alignment = sizeof(value);
			out.bytes.resize(sizeof(value));
			memcpy(out.bytes.data(), &value, sizeof(value));
		}
		return out;
	}

	if (token.type == PPT_CHARACTER_LITERAL || token.type == PPT_USER_DEFINED_CHARACTER_LITERAL)
	{
		ParsedCharLiteral parsed = ParseCharacterLiteralToken(ToPPToken(token));
		if (!parsed.ok || parsed.user_defined)
			FailAt(token, "invalid literal");
		out.type = parsed.type;
		out.is_integral = true;
		out.is_signed_integral = IsSignedFundamental(parsed.type);
		out.alignment = FundamentalSize(parsed.type);
		AppendLittleEndian(out.bytes, parsed.value, FundamentalSize(parsed.type));
		return out;
	}

	if (token.type == PPT_STRING_LITERAL || token.type == PPT_USER_DEFINED_STRING_LITERAL)
	{
		ParsedStringLiteralToken parsed = ParseStringLiteralToken(ToPPToken(token));
		if (!parsed.ok || parsed.user_defined)
			FailAt(token, "invalid literal");
		EncodedStringData encoded;
		if (!EncodeStringData(parsed.codepoints, parsed.encoding, encoded))
			FailAt(token, "invalid string literal");
		out.type = encoded.type;
		out.bytes = encoded.bytes;
		out.is_string = true;
		out.alignment = FundamentalSize(encoded.type);
		return out;
	}

	FailAt(token, "expected literal");
	return out;
}

LiteralValue NegateLiteral(const LiteralValue& in)
{
	LiteralValue out = in;
	if (in.is_string)
		FailAt(in.token, "cannot negate string literal");

	if (in.type == FT_FLOAT)
	{
		float value;
		memcpy(&value, in.bytes.data(), sizeof(value));
		value = -value;
		memcpy(out.bytes.data(), &value, sizeof(value));
		return out;
	}
	if (in.type == FT_DOUBLE)
	{
		double value;
		memcpy(&value, in.bytes.data(), sizeof(value));
		value = -value;
		memcpy(out.bytes.data(), &value, sizeof(value));
		return out;
	}
	if (in.type == FT_LONG_DOUBLE)
	{
		long double value;
		memcpy(&value, in.bytes.data(), sizeof(value));
		value = -value;
		memcpy(out.bytes.data(), &value, sizeof(value));
		return out;
	}

	unsigned long long value = ReadLittleEndianUnsigned(in.bytes);
	size_t bits = in.bytes.size() * 8;
	if (bits < 64)
		value &= ((1ULL << bits) - 1ULL);
	unsigned long long negated = (~value) + 1ULL;
	out.bytes.clear();
	AppendLittleEndian(out.bytes, negated, in.bytes.size());
	return out;
}

vector<unsigned char> ResizeLiteralBytes(const LiteralValue& literal, size_t width)
{
	vector<unsigned char> out = literal.bytes;
	if (out.size() > width)
	{
		out.resize(width);
		return out;
	}

	unsigned char fill = 0;
	if (literal.is_integral && literal.is_signed_integral && !out.empty() && (out.back() & 0x80) != 0)
		fill = 0xff;
	out.resize(width, fill);
	return out;
}

unsigned long long IntegralValueForAddress(const LiteralValue& literal)
{
	if (!literal.is_integral)
		FailAt(literal.token, "expected integral literal");

	vector<unsigned char> bytes = ResizeLiteralBytes(literal, 8);
	return ReadLittleEndianUnsigned(bytes);
}

struct ImmediateExpr
{
	MacroToken token;
	bool has_label = false;
	string label;
	bool has_literal = false;
	LiteralValue literal;
};

struct MemoryExpr
{
	MacroToken token;
	bool base_is_register = false;
	RegisterId reg_id = R_SP;
	int reg_width = 0;
	bool has_label = false;
	string label;
	bool has_literal_base = false;
	LiteralValue literal_base;
	bool has_disp = false;
	LiteralValue disp;
};

struct Operand
{
	enum Kind
	{
		OK_REGISTER,
		OK_IMMEDIATE,
		OK_MEMORY
	} kind = OK_REGISTER;

	MacroToken token;
	RegisterId reg_id = R_SP;
	int reg_width = 0;
	ImmediateExpr imm;
	MemoryExpr mem;
};

struct Statement
{
	vector<pair<string, MacroToken> > labels;
	MacroToken token;
	bool is_literal_data = false;
	LiteralValue literal_data;
	const OpcodeSpec* spec = nullptr;
	string opcode;
	vector<Operand> operands;
	size_t address = 0;
	size_t size = 0;
	size_t alignment = 1;
};

struct EncodedOperand
{
	unsigned char mode = OM_NONE;
	unsigned char reg = 0;
	unsigned long long value = 0;
};

struct Program
{
	vector<Statement> statements;
	map<string, size_t> labels;
	size_t entry_address = 0;
	vector<unsigned char> image;
};

static const unsigned char kEntryStub[] =
{
	0x48, 0xbf, 0, 0, 0, 0, 0, 0, 0, 0,
	0x48, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0,
	0xff, 0xd0,
	0xb8, 0x3c, 0x00, 0x00, 0x00,
	0x31, 0xff,
	0x0f, 0x05
};

size_t FileImageOffset()
{
	return 64 + 56 + sizeof(kEntryStub) + kCy86RuntimeTextLen + kCy86RuntimeRodataLen;
}

unsigned long long FileImageVaddr()
{
	return 0x400000ULL + FileImageOffset();
}

struct Parser
{
	explicit Parser(const vector<MacroToken>& tokens_)
		: tokens(tokens_), pos(0)
	{
	}

	const MacroToken& Peek(size_t offset = 0) const
	{
		size_t idx = pos + offset;
		if (idx >= tokens.size())
			return tokens.back();
		return tokens[idx];
	}

	bool AcceptPunc(const string& text)
	{
		if (Peek().type == PPT_PREPROCESSING_OP_OR_PUNC && Peek().data == text)
		{
			++pos;
			return true;
		}
		return false;
	}

	void ExpectPunc(const string& text)
	{
		if (!AcceptPunc(text))
			FailAt(Peek(), "expected `" + text + "`");
	}

	MacroToken ExpectIdentifier()
	{
		if (Peek().type != PPT_IDENTIFIER)
			FailAt(Peek(), "expected identifier");
		if (IsReservedIdentifier(Peek()))
			FailAt(Peek(), "reserved token is not permitted in CY86");
		return tokens[pos++];
	}

	bool AtEnd() const
	{
		return Peek().type == PPT_EOF;
	}

	LiteralValue ParseLiteral()
	{
		EPPTokenType type = Peek().type;
		if (type != PPT_PP_NUMBER &&
			type != PPT_CHARACTER_LITERAL &&
			type != PPT_USER_DEFINED_CHARACTER_LITERAL &&
			type != PPT_STRING_LITERAL &&
			type != PPT_USER_DEFINED_STRING_LITERAL)
		{
			FailAt(Peek(), "expected literal");
		}
		return ParseLiteralValue(tokens[pos++]);
	}

	ImmediateExpr ParseImmediate()
	{
		ImmediateExpr out;
		out.token = Peek();

		if (AcceptPunc("("))
		{
			bool negate = AcceptPunc("-");
			if (Peek().type == PPT_IDENTIFIER)
			{
				MacroToken id = ExpectIdentifier();
				out.has_label = true;
				out.label = id.data;
				if (negate)
					FailAt(id, "expected literal after `-`");
				if (AcceptPunc("+"))
				{
					out.has_literal = true;
					out.literal = ParseLiteral();
				}
				else if (AcceptPunc("-"))
				{
					out.has_literal = true;
					out.literal = NegateLiteral(ParseLiteral());
				}
			}
			else
			{
				LiteralValue literal = ParseLiteral();
				out.has_literal = true;
				out.literal = negate ? NegateLiteral(literal) : literal;
			}
			ExpectPunc(")");
			return out;
		}

		if (Peek().type == PPT_IDENTIFIER)
		{
			MacroToken id = ExpectIdentifier();
			auto reg_it = kRegisters.find(id.data);
			if (reg_it != kRegisters.end())
				FailAt(id, "register cannot appear in immediate context");
			out.has_label = true;
			out.label = id.data;
			return out;
		}

		out.has_literal = true;
		out.literal = ParseLiteral();
		return out;
	}

	MemoryExpr ParseMemory()
	{
		MemoryExpr out;
		out.token = Peek();
		ExpectPunc("[");

		if (Peek().type == PPT_IDENTIFIER)
		{
			MacroToken id = ExpectIdentifier();
			auto reg_it = kRegisters.find(id.data);
			if (reg_it != kRegisters.end())
			{
				out.base_is_register = true;
				out.reg_id = reg_it->second.id;
				out.reg_width = reg_it->second.width;
			}
			else
			{
				out.has_label = true;
				out.label = id.data;
			}
		}
		else
		{
			out.has_literal_base = true;
			out.literal_base = ParseLiteral();
		}

		if (AcceptPunc("+"))
		{
			out.has_disp = true;
			out.disp = ParseLiteral();
		}
		else if (AcceptPunc("-"))
		{
			out.has_disp = true;
			out.disp = NegateLiteral(ParseLiteral());
		}

		ExpectPunc("]");
		return out;
	}

	Operand ParseOperand()
	{
		Operand out;
		out.token = Peek();

		if (Peek().type == PPT_PREPROCESSING_OP_OR_PUNC && Peek().data == "[")
		{
			out.kind = Operand::OK_MEMORY;
			out.mem = ParseMemory();
			return out;
		}

		if (Peek().type == PPT_IDENTIFIER)
		{
			MacroToken id = ExpectIdentifier();
			auto reg_it = kRegisters.find(id.data);
			if (reg_it != kRegisters.end())
			{
				out.kind = Operand::OK_REGISTER;
				out.reg_id = reg_it->second.id;
				out.reg_width = reg_it->second.width;
				out.token = id;
				return out;
			}

			out.kind = Operand::OK_IMMEDIATE;
			out.imm.token = id;
			out.imm.has_label = true;
			out.imm.label = id.data;
			return out;
		}

		out.kind = Operand::OK_IMMEDIATE;
		out.imm = ParseImmediate();
		return out;
	}

	Statement ParseStatement()
	{
		Statement st;

		while (Peek().type == PPT_IDENTIFIER &&
			Peek(1).type == PPT_PREPROCESSING_OP_OR_PUNC &&
			Peek(1).data == ":")
		{
			MacroToken label = ExpectIdentifier();
			ExpectPunc(":");
			st.labels.push_back(make_pair(label.data, label));
		}

		if (Peek().type == PPT_PREPROCESSING_OP_OR_PUNC && Peek().data == "-")
		{
			MacroToken minus = Peek();
			++pos;
			st.token = minus;
			st.is_literal_data = true;
			st.literal_data = NegateLiteral(ParseLiteral());
			ExpectPunc(";");
			return st;
		}

		if (Peek().type == PPT_PP_NUMBER ||
			Peek().type == PPT_CHARACTER_LITERAL ||
			Peek().type == PPT_USER_DEFINED_CHARACTER_LITERAL ||
			Peek().type == PPT_STRING_LITERAL ||
			Peek().type == PPT_USER_DEFINED_STRING_LITERAL)
		{
			st.token = Peek();
			st.is_literal_data = true;
			st.literal_data = ParseLiteral();
			ExpectPunc(";");
			return st;
		}

		MacroToken opcode = ExpectIdentifier();
		st.token = opcode;
		st.opcode = opcode.data;
		while (!(Peek().type == PPT_PREPROCESSING_OP_OR_PUNC && Peek().data == ";"))
			st.operands.push_back(ParseOperand());
		ExpectPunc(";");
		return st;
	}

	Program Parse()
	{
		Program program;
		while (!AtEnd())
			program.statements.push_back(ParseStatement());
		return program;
	}

	const vector<MacroToken>& tokens;
	size_t pos;
};

bool DescHas(const string& desc, char c)
{
	return desc.find(c) != string::npos;
}

int DescWidth(const string& desc)
{
	if (desc.find("80") != string::npos)
		return 10;
	if (desc.find("64") != string::npos)
		return 8;
	if (desc.find("32") != string::npos)
		return 4;
	if (desc.find("16") != string::npos)
		return 2;
	return 1;
}

bool DescIsFloat(const string& desc)
{
	return DescHas(desc, 'f');
}

bool DescNeedsIntegral(const string& desc)
{
	return DescHas(desc, 'i') || DescHas(desc, 's') || DescHas(desc, 'u') || DescHas(desc, 'b') || DescHas(desc, 'a');
}

void ValidateLabelDefinitions(const Program& program)
{
	set<string> labels;
	for (const Statement& st : program.statements)
	{
		for (const auto& item : st.labels)
		{
			const string& name = item.first;
			const MacroToken& token = item.second;
			if (labels.count(name))
				FailAt(token, "duplicate label `" + name + "`");
			if (kRegisters.count(name))
				FailAt(token, "label conflicts with register `" + name + "`");
			if (kOpcodeSpecs.count(name))
				FailAt(token, "label conflicts with opcode `" + name + "`");
			labels.insert(name);
		}
	}
}

void ValidateOperand(const Operand& operand, const string& desc)
{
	int width = DescWidth(desc);
	if (operand.kind == Operand::OK_REGISTER)
	{
		if (DescHas(desc, 'I'))
			FailAt(operand.token, "expected immediate operand");
		if (operand.reg_width != width)
			FailAt(operand.token, "register width mismatch");
		if (DescIsFloat(desc))
			FailAt(operand.token, "floating operands must be memory-backed");
		if (DescHas(desc, 'a') && operand.reg_width != 8)
			FailAt(operand.token, "address register must be 64-bit");
		return;
	}

	if (operand.kind == Operand::OK_IMMEDIATE)
	{
		if (DescHas(desc, 'w'))
			FailAt(operand.token, "write operands may not be immediate");
		if (DescIsFloat(desc))
			FailAt(operand.token, "floating immediate operands are not supported");
		if ((operand.imm.has_label || DescNeedsIntegral(desc)) &&
			operand.imm.has_literal &&
			!operand.imm.literal.is_integral &&
			DescHas(desc, 'a'))
		{
			FailAt(operand.token, "expected integral literal");
		}
		return;
	}

	if (DescHas(desc, 'I'))
		FailAt(operand.token, "expected immediate operand");
	if (operand.mem.base_is_register && operand.mem.reg_width != 8)
		FailAt(operand.token, "memory base register must be 64-bit");
	if (DescIsFloat(desc) && width != 4 && width != 8 && width != 10)
		FailAt(operand.token, "invalid floating width");
	if (operand.mem.has_literal_base && operand.mem.literal_base.is_string)
		FailAt(operand.token, "string literal is not valid address");
	if (operand.mem.has_disp && !operand.mem.disp.is_integral)
		FailAt(operand.token, "address displacement must be integral");
}

void AnalyzeProgram(Program& program)
{
	ValidateLabelDefinitions(program);

	for (Statement& st : program.statements)
	{
		if (st.is_literal_data)
		{
			st.alignment = max<size_t>(1, st.literal_data.alignment);
			st.size = st.literal_data.bytes.size();
			continue;
		}

		auto spec_it = kOpcodeSpecs.find(st.opcode);
		if (spec_it == kOpcodeSpecs.end())
			FailAt(st.token, "unknown opcode `" + st.opcode + "`");
		st.spec = &spec_it->second;
		if (st.operands.size() != st.spec->operands.size())
			FailAt(st.token, "incorrect operand count for `" + st.opcode + "`");
		for (size_t i = 0; i < st.operands.size(); ++i)
			ValidateOperand(st.operands[i], st.spec->operands[i]);
		if (st.spec->is_data)
		{
			st.alignment = st.spec->width;
			st.size = st.spec->width;
		}
		else
		{
			st.alignment = 1;
			st.size = 96;
		}
	}

	size_t offset = 0;
	for (Statement& st : program.statements)
	{
		offset = AlignUp(offset, st.alignment);
		st.address = offset;
		offset += st.size;
		for (const auto& item : st.labels)
			program.labels[item.first] = st.address;
	}

	if (!program.labels.empty() && program.labels.count("start"))
		program.entry_address = program.labels["start"];
	else if (!program.statements.empty())
		program.entry_address = program.statements.front().address;
	else
		program.entry_address = 0;

	program.image.assign(offset, 0);
}

unsigned long long ResolveImmediateValue(const ImmediateExpr& expr, const string& desc, const map<string, size_t>& labels, unsigned long long image_vaddr)
{
	int width = DescWidth(desc);
	if (expr.has_label)
	{
		auto it = labels.find(expr.label);
		if (it == labels.end())
			FailAt(expr.token, "unknown label `" + expr.label + "`");
		unsigned long long value = image_vaddr + static_cast<unsigned long long>(it->second);
		if (expr.has_literal)
			value += IntegralValueForAddress(expr.literal);
		vector<unsigned char> bytes;
		AppendLittleEndian(bytes, value, 8);
		bytes.resize(width, 0);
		return ReadLittleEndianUnsigned(bytes);
	}

	if (!expr.has_literal)
		return 0;
	return ReadLittleEndianUnsigned(ResizeLiteralBytes(expr.literal, width));
}

EncodedOperand ResolveOperand(const Operand& operand, const string& desc, const map<string, size_t>& labels, unsigned long long image_vaddr)
{
	EncodedOperand out;

	if (operand.kind == Operand::OK_REGISTER)
	{
		out.mode = OM_REG;
		out.reg = static_cast<unsigned char>(operand.reg_id);
		return out;
	}

	if (operand.kind == Operand::OK_IMMEDIATE)
	{
		out.mode = OM_IMM;
		out.value = ResolveImmediateValue(operand.imm, desc, labels, image_vaddr);
		return out;
	}

	if (operand.mem.base_is_register)
	{
		out.mode = OM_MEM_REG_DISP;
		out.reg = static_cast<unsigned char>(operand.mem.reg_id);
		if (operand.mem.has_disp)
			out.value = IntegralValueForAddress(operand.mem.disp);
		return out;
	}

	out.mode = OM_MEM_ABS;
	if (operand.mem.has_label)
	{
		auto it = labels.find(operand.mem.label);
		if (it == labels.end())
			FailAt(operand.mem.token, "unknown label `" + operand.mem.label + "`");
		out.value = image_vaddr + static_cast<unsigned long long>(it->second);
		if (operand.mem.has_disp)
			out.value += IntegralValueForAddress(operand.mem.disp);
		return out;
	}

	if (operand.mem.has_literal_base)
		out.value = ReadLittleEndianUnsigned(ResizeLiteralBytes(operand.mem.literal_base, 8));
	return out;
}

void EncodeProgram(Program& program, unsigned long long image_vaddr)
{
	for (const Statement& st : program.statements)
	{
		if (st.is_literal_data)
		{
			copy(st.literal_data.bytes.begin(), st.literal_data.bytes.end(), program.image.begin() + st.address);
			continue;
		}

		if (st.spec->is_data)
		{
			EncodedOperand operand = ResolveOperand(st.operands[0], st.spec->operands[0], program.labels, image_vaddr);
			if (operand.mode != OM_IMM)
				FailAt(st.token, "data operand must be immediate");
			WriteLittleEndian(program.image, st.address, operand.value, st.spec->width);
			continue;
		}

		vector<unsigned char>& image = program.image;
		size_t base = st.address;
		image[base + 0] = static_cast<unsigned char>(st.spec->runtime_op);
		image[base + 1] = static_cast<unsigned char>(st.spec->width);
		if (st.spec->runtime_op == ROP_SYSCALL)
			image[base + 2] = static_cast<unsigned char>(st.spec->syscall_arity);

		for (size_t i = 0; i < st.operands.size(); ++i)
		{
			EncodedOperand operand = ResolveOperand(st.operands[i], st.spec->operands[i], program.labels, image_vaddr);
			size_t slot = base + 4 + i * 10;
			image[slot + 0] = operand.mode;
			image[slot + 1] = operand.reg;
			WriteLittleEndian(image, slot + 2, operand.value, 8);
		}
	}
}

vector<MacroToken> GatherProgramTokens(const vector<string>& srcfiles)
{
	pair<string, string> build = BuildDateTimeLiterals();
	string author_literal = EscapeStringLiteral("OpenAI Codex");
	vector<MacroToken> out;

	for (const string& srcfile : srcfiles)
	{
		Preprocessor preproc(build.first, build.second, author_literal);
		vector<MacroToken> tokens = preproc.ProcessSourceFileTokens(srcfile);
		for (const MacroToken& token : tokens)
		{
			if (token.type == PPT_EOF)
				continue;
			out.push_back(token);
		}
	}

	MacroToken eof;
	eof.type = PPT_EOF;
	if (!out.empty())
	{
		eof.source_file = out.back().source_file;
		eof.source_line = out.back().source_line;
	}
	out.push_back(eof);
	return out;
}

void PatchRuntimeRelocations(vector<unsigned char>& segment, size_t runtime_off, size_t rodata_off)
{
	for (size_t i = 0; i < kCy86RuntimeRelocCount; ++i)
	{
		size_t reloc_off = runtime_off + kCy86RuntimeRelocOffsets[i];
		long long disp = static_cast<long long>(rodata_off + kCy86RuntimeRelocTargets[i]) -
			static_cast<long long>(runtime_off + kCy86RuntimeRelocOffsets[i] + 4);
		WriteLittleEndian(segment, reloc_off, static_cast<uint32_t>(disp), 4);
	}
}

vector<unsigned char> BuildExecutableImage(const Program& program)
{
	const size_t header_off = 0;
	const size_t phdr_off = 64;
	const size_t stub_off = 64 + 56;
	const size_t runtime_off = stub_off + sizeof(kEntryStub);
	const size_t rodata_off = runtime_off + kCy86RuntimeTextLen;
	const size_t image_off = rodata_off + kCy86RuntimeRodataLen;
	const unsigned long long vbase = 0x400000;
	const unsigned long long runtime_addr = vbase + runtime_off;
	const unsigned long long entry_addr = vbase + image_off + program.entry_address;

	vector<unsigned char> segment(image_off + program.image.size(), 0);
	copy(kEntryStub, kEntryStub + sizeof(kEntryStub), segment.begin() + stub_off);
	copy(kCy86RuntimeText, kCy86RuntimeText + kCy86RuntimeTextLen, segment.begin() + runtime_off);
	copy(kCy86RuntimeRodata, kCy86RuntimeRodata + kCy86RuntimeRodataLen, segment.begin() + rodata_off);
	copy(program.image.begin(), program.image.end(), segment.begin() + image_off);

	WriteLittleEndian(segment, stub_off + 2, entry_addr, 8);
	WriteLittleEndian(segment, stub_off + 12, runtime_addr, 8);
	PatchRuntimeRelocations(segment, runtime_off, rodata_off);

	ElfHeader elf_header;
	ProgramSegmentHeader phdr;
	elf_header.entry = vbase + stub_off;
	phdr.filesz = segment.size();
	phdr.memsz = segment.size();

	vector<unsigned char> file(segment.size(), 0);
	memcpy(file.data() + header_off, &elf_header, sizeof(elf_header));
	memcpy(file.data() + phdr_off, &phdr, sizeof(phdr));
	copy(segment.begin() + stub_off, segment.end(), file.begin() + stub_off);
	return file;
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

		string outfile = args[1];
		vector<string> srcfiles(args.begin() + 2, args.end());

		vector<MacroToken> tokens = GatherProgramTokens(srcfiles);
		Parser parser(tokens);
		Program program = parser.Parse();
		AnalyzeProgram(program);
		EncodeProgram(program, FileImageVaddr());
		vector<unsigned char> file = BuildExecutableImage(program);

		ofstream out(outfile.c_str(), ios::binary);
		if (!out)
			throw runtime_error("cannot open output file: " + outfile);
		out.write(reinterpret_cast<const char*>(file.data()), file.size());
		out.close();
		if (!out)
			throw runtime_error("failed writing output file: " + outfile);
		if (!PA9SetFileExecutable(outfile))
			throw runtime_error("failed to mark output executable: " + outfile);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
