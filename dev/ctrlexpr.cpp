// PA2 Implementation

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <memory>
#include <cstring>
#include <cstdint>
#include <climits>
#include <map>
#include <set>
#include <regex>

using namespace std;

// === PA2 STARTER CODE START ===
enum EFundamentalType {
	FT_SIGNED_CHAR, FT_SHORT_INT, FT_INT, FT_LONG_INT, FT_LONG_LONG_INT,
	FT_UNSIGNED_CHAR, FT_UNSIGNED_SHORT_INT, FT_UNSIGNED_INT, FT_UNSIGNED_LONG_INT, FT_UNSIGNED_LONG_LONG_INT,
	FT_WCHAR_T, FT_CHAR, FT_CHAR16_T, FT_CHAR32_T, FT_BOOL,
	FT_FLOAT, FT_DOUBLE, FT_LONG_DOUBLE, FT_VOID, FT_NULLPTR_T
};

const map<EFundamentalType, string> FundamentalTypeToStringMap {
	{FT_SIGNED_CHAR, "signed char"}, {FT_SHORT_INT, "short int"}, {FT_INT, "int"},
	{FT_LONG_INT, "long int"}, {FT_LONG_LONG_INT, "long long int"}, {FT_UNSIGNED_CHAR, "unsigned char"},
	{FT_UNSIGNED_SHORT_INT, "unsigned short int"}, {FT_UNSIGNED_INT, "unsigned int"},
	{FT_UNSIGNED_LONG_INT, "unsigned long int"}, {FT_UNSIGNED_LONG_LONG_INT, "unsigned long long int"},
	{FT_WCHAR_T, "wchar_t"}, {FT_CHAR, "char"}, {FT_CHAR16_T, "char16_t"}, {FT_CHAR32_T, "char32_t"},
	{FT_BOOL, "bool"}, {FT_FLOAT, "float"}, {FT_DOUBLE, "double"}, {FT_LONG_DOUBLE, "long double"},
	{FT_VOID, "void"}, {FT_NULLPTR_T, "nullptr_t"}
};

enum ETokenType {
	KW_ALIGNAS, KW_ALIGNOF, KW_ASM, KW_AUTO, KW_BOOL, KW_BREAK, KW_CASE, KW_CATCH, KW_CHAR, KW_CHAR16_T, KW_CHAR32_T,
	KW_CLASS, KW_CONST, KW_CONSTEXPR, KW_CONST_CAST, KW_CONTINUE, KW_DECLTYPE, KW_DEFAULT, KW_DELETE, KW_DO, KW_DOUBLE,
	KW_DYNAMIC_CAST, KW_ELSE, KW_ENUM, KW_EXPLICIT, KW_EXPORT, KW_EXTERN, KW_FALSE, KW_FLOAT, KW_FOR, KW_FRIEND, KW_GOTO,
	KW_IF, KW_INLINE, KW_INT, KW_LONG, KW_MUTABLE, KW_NAMESPACE, KW_NEW, KW_NOEXCEPT, KW_NULLPTR, KW_OPERATOR, KW_PRIVATE,
	KW_PROTECTED, KW_PUBLIC, KW_REGISTER, KW_REINTERPET_CAST, KW_RETURN, KW_SHORT, KW_SIGNED, KW_SIZEOF, KW_STATIC,
	KW_STATIC_ASSERT, KW_STATIC_CAST, KW_STRUCT, KW_SWITCH, KW_TEMPLATE, KW_THIS, KW_THREAD_LOCAL, KW_THROW, KW_TRUE,
	KW_TRY, KW_TYPEDEF, KW_TYPEID, KW_TYPENAME, KW_UNION, KW_UNSIGNED, KW_USING, KW_VIRTUAL, KW_VOID, KW_VOLATILE,
	KW_WCHAR_T, KW_WHILE, OP_LBRACE, OP_RBRACE, OP_LSQUARE, OP_RSQUARE, OP_LPAREN, OP_RPAREN, OP_BOR, OP_XOR, OP_COMPL,
	OP_AMP, OP_LNOT, OP_SEMICOLON, OP_COLON, OP_DOTS, OP_QMARK, OP_COLON2, OP_DOT, OP_DOTSTAR, OP_PLUS, OP_MINUS, OP_STAR,
	OP_DIV, OP_MOD, OP_ASS, OP_LT, OP_GT, OP_PLUSASS, OP_MINUSASS, OP_STARASS, OP_DIVASS, OP_MODASS, OP_XORASS, OP_BANDASS,
	OP_BORASS, OP_LSHIFT, OP_RSHIFT, OP_RSHIFTASS, OP_LSHIFTASS, OP_EQ, OP_NE, OP_LE, OP_GE, OP_LAND, OP_LOR, OP_INC,
	OP_DEC, OP_COMMA, OP_ARROWSTAR, OP_ARROW
};

const unordered_map<string, ETokenType> StringToTokenTypeMap = {
	{"alignas", KW_ALIGNAS}, {"alignof", KW_ALIGNOF}, {"asm", KW_ASM}, {"auto", KW_AUTO}, {"bool", KW_BOOL},
	{"break", KW_BREAK}, {"case", KW_CASE}, {"catch", KW_CATCH}, {"char", KW_CHAR}, {"char16_t", KW_CHAR16_T},
	{"char32_t", KW_CHAR32_T}, {"class", KW_CLASS}, {"const", KW_CONST}, {"constexpr", KW_CONSTEXPR},
	{"const_cast", KW_CONST_CAST}, {"continue", KW_CONTINUE}, {"decltype", KW_DECLTYPE}, {"default", KW_DEFAULT},
	{"delete", KW_DELETE}, {"do", KW_DO}, {"double", KW_DOUBLE}, {"dynamic_cast", KW_DYNAMIC_CAST}, {"else", KW_ELSE},
	{"enum", KW_ENUM}, {"explicit", KW_EXPLICIT}, {"export", KW_EXPORT}, {"extern", KW_EXTERN}, {"false", KW_FALSE},
	{"float", KW_FLOAT}, {"for", KW_FOR}, {"friend", KW_FRIEND}, {"goto", KW_GOTO}, {"if", KW_IF}, {"inline", KW_INLINE},
	{"int", KW_INT}, {"long", KW_LONG}, {"mutable", KW_MUTABLE}, {"namespace", KW_NAMESPACE}, {"new", KW_NEW},
	{"noexcept", KW_NOEXCEPT}, {"nullptr", KW_NULLPTR}, {"operator", KW_OPERATOR}, {"private", KW_PRIVATE},
	{"protected", KW_PROTECTED}, {"public", KW_PUBLIC}, {"register", KW_REGISTER}, {"reinterpret_cast", KW_REINTERPET_CAST},
	{"return", KW_RETURN}, {"short", KW_SHORT}, {"signed", KW_SIGNED}, {"sizeof", KW_SIZEOF}, {"static", KW_STATIC},
	{"static_assert", KW_STATIC_ASSERT}, {"static_cast", KW_STATIC_CAST}, {"struct", KW_STRUCT}, {"switch", KW_SWITCH},
	{"template", KW_TEMPLATE}, {"this", KW_THIS}, {"thread_local", KW_THREAD_LOCAL}, {"throw", KW_THROW},
	{"true", KW_TRUE}, {"try", KW_TRY}, {"typedef", KW_TYPEDEF}, {"typeid", KW_TYPEID}, {"typename", KW_TYPENAME},
	{"union", KW_UNION}, {"unsigned", KW_UNSIGNED}, {"using", KW_USING}, {"virtual", KW_VIRTUAL}, {"void", KW_VOID},
	{"volatile", KW_VOLATILE}, {"wchar_t", KW_WCHAR_T}, {"while", KW_WHILE}, {"{", OP_LBRACE}, {"<%", OP_LBRACE},
	{"}", OP_RBRACE}, {"%>", OP_RBRACE}, {"[", OP_LSQUARE}, {"<:", OP_LSQUARE}, {"]", OP_RSQUARE}, {":>", OP_RSQUARE},
	{"(", OP_LPAREN}, {")", OP_RPAREN}, {"|", OP_BOR}, {"bitor", OP_BOR}, {"^", OP_XOR}, {"xor", OP_XOR},
	{"~", OP_COMPL}, {"compl", OP_COMPL}, {"&", OP_AMP}, {"bitand", OP_AMP}, {"!", OP_LNOT}, {"not", OP_LNOT},
	{";", OP_SEMICOLON}, {":", OP_COLON}, {"...", OP_DOTS}, {"?", OP_QMARK}, {"::", OP_COLON2}, {".", OP_DOT},
	{".*", OP_DOTSTAR}, {"+", OP_PLUS}, {"-", OP_MINUS}, {"*", OP_STAR}, {"/", OP_DIV}, {"%", OP_MOD}, {"=", OP_ASS},
	{"<", OP_LT}, {">", OP_GT}, {"+=", OP_PLUSASS}, {"-=", OP_MINUSASS}, {"*=", OP_STARASS}, {"/=", OP_DIVASS},
	{"%=", OP_MODASS}, {"^=", OP_XORASS}, {"xor_eq", OP_XORASS}, {"&=", OP_BANDASS}, {"and_eq", OP_BANDASS},
	{"|=", OP_BORASS}, {"or_eq", OP_BORASS}, {"<<", OP_LSHIFT}, {">>", OP_RSHIFT}, {">>=", OP_RSHIFTASS},
	{"<<=", OP_LSHIFTASS}, {"==", OP_EQ}, {"!=", OP_NE}, {"not_eq", OP_NE}, {"<=", OP_LE}, {">=", OP_GE},
	{"&&", OP_LAND}, {"and", OP_LAND}, {"||", OP_LOR}, {"or", OP_LOR}, {"++", OP_INC}, {"--", OP_DEC},
	{",", OP_COMMA}, {"->*", OP_ARROWSTAR}, {"->", OP_ARROW}
};

const map<ETokenType, string> TokenTypeToStringMap = {
	{KW_ALIGNAS, "KW_ALIGNAS"}, {KW_ALIGNOF, "KW_ALIGNOF"}, {KW_ASM, "KW_ASM"}, {KW_AUTO, "KW_AUTO"},
	{KW_BOOL, "KW_BOOL"}, {KW_BREAK, "KW_BREAK"}, {KW_CASE, "KW_CASE"}, {KW_CATCH, "KW_CATCH"},
	{KW_CHAR, "KW_CHAR"}, {KW_CHAR16_T, "KW_CHAR16_T"}, {KW_CHAR32_T, "KW_CHAR32_T"}, {KW_CLASS, "KW_CLASS"},
	{KW_CONST, "KW_CONST"}, {KW_CONSTEXPR, "KW_CONSTEXPR"}, {KW_CONST_CAST, "KW_CONST_CAST"},
	{KW_CONTINUE, "KW_CONTINUE"}, {KW_DECLTYPE, "KW_DECLTYPE"}, {KW_DEFAULT, "KW_DEFAULT"},
	{KW_DELETE, "KW_DELETE"}, {KW_DO, "KW_DO"}, {KW_DOUBLE, "KW_DOUBLE"}, {KW_DYNAMIC_CAST, "KW_DYNAMIC_CAST"},
	{KW_ELSE, "KW_ELSE"}, {KW_ENUM, "KW_ENUM"}, {KW_EXPLICIT, "KW_EXPLICIT"}, {KW_EXPORT, "KW_EXPORT"},
	{KW_EXTERN, "KW_EXTERN"}, {KW_FALSE, "KW_FALSE"}, {KW_FLOAT, "KW_FLOAT"}, {KW_FOR, "KW_FOR"},
	{KW_FRIEND, "KW_FRIEND"}, {KW_GOTO, "KW_GOTO"}, {KW_IF, "KW_IF"}, {KW_INLINE, "KW_INLINE"},
	{KW_INT, "KW_INT"}, {KW_LONG, "KW_LONG"}, {KW_MUTABLE, "KW_MUTABLE"}, {KW_NAMESPACE, "KW_NAMESPACE"},
	{KW_NEW, "KW_NEW"}, {KW_NOEXCEPT, "KW_NOEXCEPT"}, {KW_NULLPTR, "KW_NULLPTR"}, {KW_OPERATOR, "KW_OPERATOR"},
	{KW_PRIVATE, "KW_PRIVATE"}, {KW_PROTECTED, "KW_PROTECTED"}, {KW_PUBLIC, "KW_PUBLIC"},
	{KW_REGISTER, "KW_REGISTER"}, {KW_REINTERPET_CAST, "KW_REINTERPET_CAST"}, {KW_RETURN, "KW_RETURN"},
	{KW_SHORT, "KW_SHORT"}, {KW_SIGNED, "KW_SIGNED"}, {KW_SIZEOF, "KW_SIZEOF"}, {KW_STATIC, "KW_STATIC"},
	{KW_STATIC_ASSERT, "KW_STATIC_ASSERT"}, {KW_STATIC_CAST, "KW_STATIC_CAST"}, {KW_STRUCT, "KW_STRUCT"},
	{KW_SWITCH, "KW_SWITCH"}, {KW_TEMPLATE, "KW_TEMPLATE"}, {KW_THIS, "KW_THIS"}, {KW_THREAD_LOCAL, "KW_THREAD_LOCAL"},
	{KW_THROW, "KW_THROW"}, {KW_TRUE, "KW_TRUE"}, {KW_TRY, "KW_TRY"}, {KW_TYPEDEF, "KW_TYPEDEF"},
	{KW_TYPEID, "KW_TYPEID"}, {KW_TYPENAME, "KW_TYPENAME"}, {KW_UNION, "KW_UNION"}, {KW_UNSIGNED, "KW_UNSIGNED"},
	{KW_USING, "KW_USING"}, {KW_VIRTUAL, "KW_VIRTUAL"}, {KW_VOID, "KW_VOID"}, {KW_VOLATILE, "KW_VOLATILE"},
	{KW_WCHAR_T, "KW_WCHAR_T"}, {KW_WHILE, "KW_WHILE"}, {OP_LBRACE, "OP_LBRACE"}, {OP_RBRACE, "OP_RBRACE"},
	{OP_LSQUARE, "OP_LSQUARE"}, {OP_RSQUARE, "OP_RSQUARE"}, {OP_LPAREN, "OP_LPAREN"}, {OP_RPAREN, "OP_RPAREN"},
	{OP_BOR, "OP_BOR"}, {OP_XOR, "OP_XOR"}, {OP_COMPL, "OP_COMPL"}, {OP_AMP, "OP_AMP"}, {OP_LNOT, "OP_LNOT"},
	{OP_SEMICOLON, "OP_SEMICOLON"}, {OP_COLON, "OP_COLON"}, {OP_DOTS, "OP_DOTS"}, {OP_QMARK, "OP_QMARK"},
	{OP_COLON2, "OP_COLON2"}, {OP_DOT, "OP_DOT"}, {OP_DOTSTAR, "OP_DOTSTAR"}, {OP_PLUS, "OP_PLUS"},
	{OP_MINUS, "OP_MINUS"}, {OP_STAR, "OP_STAR"}, {OP_DIV, "OP_DIV"}, {OP_MOD, "OP_MOD"}, {OP_ASS, "OP_ASS"},
	{OP_LT, "OP_LT"}, {OP_GT, "OP_GT"}, {OP_PLUSASS, "OP_PLUSASS"}, {OP_MINUSASS, "OP_MINUSASS"},
	{OP_STARASS, "OP_STARASS"}, {OP_DIVASS, "OP_DIVASS"}, {OP_MODASS, "OP_MODASS"}, {OP_XORASS, "OP_XORASS"},
	{OP_BANDASS, "OP_BANDASS"}, {OP_BORASS, "OP_BORASS"}, {OP_LSHIFT, "OP_LSHIFT"}, {OP_RSHIFT, "OP_RSHIFT"},
	{OP_RSHIFTASS, "OP_RSHIFTASS"}, {OP_LSHIFTASS, "OP_LSHIFTASS"}, {OP_EQ, "OP_EQ"}, {OP_NE, "OP_NE"},
	{OP_LE, "OP_LE"}, {OP_GE, "OP_GE"}, {OP_LAND, "OP_LAND"}, {OP_LOR, "OP_LOR"}, {OP_INC, "OP_INC"},
	{OP_DEC, "OP_DEC"}, {OP_COMMA, "OP_COMMA"}, {OP_ARROWSTAR, "OP_ARROWSTAR"}, {OP_ARROW, "OP_ARROW"}
};

char ValueToHexChar(int c) {
	switch (c) {
	case 0: return '0'; case 1: return '1'; case 2: return '2'; case 3: return '3';
	case 4: return '4'; case 5: return '5'; case 6: return '6'; case 7: return '7';
	case 8: return '8'; case 9: return '9'; case 10: return 'A'; case 11: return 'B';
	case 12: return 'C'; case 13: return 'D'; case 14: return 'E'; case 15: return 'F';
	default: throw logic_error("ValueToHexChar of nonhex value");
	}
}

string HexDump(const void* pdata, size_t nbytes) {
	unsigned char* p = (unsigned char*) pdata;
	string s(nbytes*2, '?');
	for (size_t i = 0; i < nbytes; i++) {
		s[2*i+0] = ValueToHexChar((p[i] & 0xF0) >> 4);
		s[2*i+1] = ValueToHexChar((p[i] & 0x0F) >> 0);
	}
	return s;
}

struct DebugPostTokenOutputStream {
	void emit_invalid(const string& source) { cout << "invalid " << source << endl; }
	void emit_simple(const string& source, ETokenType token_type) { cout << "simple " << source << " " << TokenTypeToStringMap.at(token_type) << endl; }
	void emit_identifier(const string& source) { cout << "identifier " << source << endl; }
	void emit_literal(const string& source, EFundamentalType type, const void* data, size_t nbytes) { cout << "literal " << source << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl; }
	void emit_literal_array(const string& source, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes) { cout << "literal " << source << " array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl; }
	void emit_user_defined_literal_character(const string& source, const string& ud_suffix, EFundamentalType type, const void* data, size_t nbytes) { cout << "user-defined-literal " << source << " " << ud_suffix << " character " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl; }
	void emit_user_defined_literal_string_array(const string& source, const string& ud_suffix, size_t num_elements, EFundamentalType type, const void* data, size_t nbytes) { cout << "user-defined-literal " << source << " " << ud_suffix << " string array of " << num_elements << " " << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes) << endl; }
	void emit_user_defined_literal_integer(const string& source, const string& ud_suffix, const string& prefix) { cout << "user-defined-literal " << source << " " << ud_suffix << " integer " << prefix << endl; }
	void emit_user_defined_literal_floating(const string& source, const string& ud_suffix, const string& prefix) { cout << "user-defined-literal " << source << " " << ud_suffix << " floating " << prefix << endl; }
	void emit_eof() { cout << "eof" << endl; }
};

float PA2Decode_float(const string& s) { istringstream iss(s); float x; iss >> x; return x; }
double PA2Decode_double(const string& s) { istringstream iss(s); double x; iss >> x; return x; }
long double PA2Decode_long_double(const string& s) { istringstream iss(s); long double x; iss >> x; return x; }
// === PA2 STARTER CODE END ===

#define main pptoken_main
#include "pptoken.cpp"
#undef main

struct PPToken { string type; string data; };

void process_line(const vector<PPToken>& line);
class VectorPPTokenStream : public IPPTokenStream {
public:
    vector<PPToken> tokens;
    void emit(const string& type, const string& data) {
        if (type == "whitespace-sequence") return;
        if (type == "new-line" || type == "eof") {
            if (!tokens.empty()) {
                process_line(tokens);
                tokens.clear();
            }
        } else {
            tokens.push_back({type, data});
        }
    }
    void emit_whitespace_sequence() override { emit("whitespace-sequence", ""); }
    void emit_new_line() override { emit("new-line", ""); }
    void emit_header_name(const string& data) override { emit("header-name", data); }
    void emit_identifier(const string& data) override { emit("identifier", data); }
    void emit_pp_number(const string& data) override { emit("pp-number", data); }
    void emit_character_literal(const string& data) override { emit("character-literal", data); }
    void emit_user_defined_character_literal(const string& data) override { emit("user-defined-character-literal", data); }
    void emit_string_literal(const string& data) override { emit("string-literal", data); }
    void emit_user_defined_string_literal(const string& data) override { emit("user-defined-string-literal", data); }
    void emit_preprocessing_op_or_punc(const string& data) override { emit("preprocessing-op-or-punc", data); }
    void emit_non_whitespace_char(const string& data) override { emit("non-whitespace-character", data); }
    void emit_eof() override { emit("eof", ""); }
};


struct LiteralInfo {
    string prefix;
    bool is_raw;
    string content_utf8;
    string suffix;
};

LiteralInfo parse_literal(const string& source, bool is_char) {
    LiteralInfo info;
    info.is_raw = false;

    size_t quote_pos = source.find(is_char ? '\'' : '"');
    string pref = source.substr(0, quote_pos);
    if (pref.find('R') != string::npos) info.is_raw = true;
    info.prefix = pref;
    if (info.is_raw) {
        info.prefix.erase(remove(info.prefix.begin(), info.prefix.end(), 'R'), info.prefix.end());
        size_t paren1 = source.find('(', quote_pos + 1);
        string delim = source.substr(quote_pos + 1, paren1 - quote_pos - 1);
        string end_delim = ")" + delim + "\"";
        size_t end_pos = source.rfind(end_delim);
        info.content_utf8 = source.substr(paren1 + 1, end_pos - paren1 - 1);
        info.suffix = source.substr(end_pos + end_delim.size());
    } else {
        size_t end_quote = string::npos;
        bool escaped = false;
        for (size_t i = quote_pos + 1; i < source.size(); ++i) {
            if (escaped) escaped = false;
            else if (source[i] == '\\') escaped = true;
            else if (source[i] == (is_char ? '\'' : '"')) {
                end_quote = i;
                break;
            }
        }
        info.content_utf8 = source.substr(quote_pos + 1, end_quote - quote_pos - 1);
        info.suffix = source.substr(end_quote + 1);
    }
    return info;
}

vector<int> utf8_decode(const string& s) {
    vector<int> out;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = s[i++];
        if ((c & 0x80) == 0) out.push_back(c);
        else if ((c & 0xE0) == 0xC0) {
            unsigned char c2 = s[i++];
            out.push_back(((c & 0x1F) << 6) | (c2 & 0x3F));
        } else if ((c & 0xF0) == 0xE0) {
            unsigned char c2 = s[i++];
            unsigned char c3 = s[i++];
            out.push_back(((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F));
        } else if ((c & 0xF8) == 0xF0) {
            unsigned char c2 = s[i++];
            unsigned char c3 = s[i++];
            unsigned char c4 = s[i++];
            out.push_back(((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F));
        }
    }
    return out;
}

vector<int> decode_escapes(const string& utf8_content, bool is_raw) {
    vector<int> cps = utf8_decode(utf8_content);
    if (is_raw) return cps;
    
    vector<int> out;
    for (size_t i = 0; i < cps.size(); ++i) {
        if (cps[i] == '\\') {
            i++;
            int c = cps[i];
            if (c == 'n') out.push_back(0x0A);
            else if (c == 't') out.push_back(0x09);
            else if (c == 'v') out.push_back(0x0B);
            else if (c == 'b') out.push_back(0x08);
            else if (c == 'r') out.push_back(0x0D);
            else if (c == 'f') out.push_back(0x0C);
            else if (c == 'a') out.push_back(0x07);
            else if (c == '\\') out.push_back('\\');
            else if (c == '?') out.push_back('?');
            else if (c == '\'') out.push_back('\'');
            else if (c == '"') out.push_back('"');
            else if (c >= '0' && c <= '7') {
                int val = c - '0';
                if (i + 1 < cps.size() && cps[i+1] >= '0' && cps[i+1] <= '7') {
                    val = (val << 3) + (cps[++i] - '0');
                    if (i + 1 < cps.size() && cps[i+1] >= '0' && cps[i+1] <= '7') {
                        val = (val << 3) + (cps[++i] - '0');
                    }
                }
                if (val >= 0x110000 || (val >= 0xD800 && val < 0xE000) || val < 0) {
                    throw logic_error("octal escape out of range");
                }
                out.push_back(val);
            } else if (c == 'x') {
                int hc = cps[i+1];
                int val = 0;
                while (i + 1 < cps.size()) {
                    hc = cps[i+1];
                    int hval = -1;
                    if (hc >= '0' && hc <= '9') hval = hc - '0';
                    else if (hc >= 'a' && hc <= 'f') hval = hc - 'a' + 10;
                    else if (hc >= 'A' && hc <= 'F') hval = hc - 'A' + 10;
                    
                    if (hval != -1) {
                        val = (val << 4) + hval;
                        i++;
                    } else {
                        break;
                    }
                }
                if (val >= 0x110000 || (val >= 0xD800 && val < 0xE000) || val < 0) {
                    throw logic_error("hex escape out of range");
                }
                out.push_back(val);
            } else {
                out.push_back(c); // Invalid fallback
            }
        } else {
            out.push_back(cps[i]);
        }
    }
    return out;
}

void process_string_group(const vector<PPToken>& group, DebugPostTokenOutputStream& output) {
    string combined_source = "";
    for (size_t i = 0; i < group.size(); ++i) {
        if (i > 0) combined_source += " ";
        combined_source += group[i].data;
    }
    
    try {
        set<string> prefixes;
        string ud_suffix = "";
        vector<int> final_cps;
        
        for (auto& t : group) {
            LiteralInfo info = parse_literal(t.data, false);
            if (!info.prefix.empty() && info.prefix != "u8") prefixes.insert(info.prefix);
            else if (info.prefix == "u8") prefixes.insert("u8"); // "u8" and "" mix is allowed, handled as "u8"
            
            if (!info.suffix.empty()) {
                if (!ud_suffix.empty() && ud_suffix != info.suffix) throw logic_error("multiple ud-suffixes");
                if (info.suffix[0] != '_') throw logic_error("ud_suffix does not start with _");
                ud_suffix = info.suffix;
            }
            vector<int> dec = decode_escapes(info.content_utf8, info.is_raw);
            final_cps.insert(final_cps.end(), dec.begin(), dec.end());
        }
        
        if (prefixes.size() > 1) throw logic_error("conflicting prefixes");
        string final_prefix = prefixes.empty() ? "" : *prefixes.begin();
        
        final_cps.push_back(0);
        
        EFundamentalType ftype;
        vector<uint8_t> data;
        
        if (final_prefix == "" || final_prefix == "u8") {
            ftype = FT_CHAR;
            for (int cp : final_cps) {
                if (cp <= 0x7F) {
                    data.push_back(cp);
                } else if (cp <= 0x7FF) {
                    data.push_back(0xC0 | ((cp >> 6) & 0x1F));
                    data.push_back(0x80 | (cp & 0x3F));
                } else if (cp <= 0xFFFF) {
                    data.push_back(0xE0 | ((cp >> 12) & 0x0F));
                    data.push_back(0x80 | ((cp >> 6) & 0x3F));
                    data.push_back(0x80 | (cp & 0x3F));
                } else {
                    data.push_back(0xF0 | ((cp >> 18) & 0x07));
                    data.push_back(0x80 | ((cp >> 12) & 0x3F));
                    data.push_back(0x80 | ((cp >> 6) & 0x3F));
                    data.push_back(0x80 | (cp & 0x3F));
                }
            }
        } else if (final_prefix == "u") {
            ftype = FT_CHAR16_T;
            for (int cp : final_cps) {
                if (cp > 0xFFFF) {
                    int uprime = cp - 0x10000;
                    int w1 = 0xD800 | (uprime >> 10);
                    int w2 = 0xDC00 | (uprime & 0x3FF);
                    data.push_back(w1 & 0xFF); data.push_back((w1 >> 8) & 0xFF);
                    data.push_back(w2 & 0xFF); data.push_back((w2 >> 8) & 0xFF);
                } else {
                    data.push_back(cp & 0xFF); data.push_back((cp >> 8) & 0xFF);
                }
            }
        } else if (final_prefix == "U" || final_prefix == "L") {
            ftype = (final_prefix == "U") ? FT_CHAR32_T : FT_WCHAR_T;
            for (int cp : final_cps) {
                data.push_back(cp & 0xFF); data.push_back((cp >> 8) & 0xFF);
                data.push_back((cp >> 16) & 0xFF); data.push_back((cp >> 24) & 0xFF);
            }
        }
        
        size_t num_elements = 0;
        if (ftype == FT_CHAR) num_elements = data.size();
        else if (ftype == FT_CHAR16_T) num_elements = data.size() / 2;
        else num_elements = data.size() / 4;
        
        if (ud_suffix.empty()) {
            output.emit_literal_array(combined_source, num_elements, ftype, data.data(), data.size());
        } else {
            output.emit_user_defined_literal_string_array(combined_source, ud_suffix, num_elements, ftype, data.data(), data.size());
        }
    } catch (exception& e) {
        cerr << "ERROR: " << e.what() << ": " << combined_source << endl;
        output.emit_invalid(combined_source);
    }
}

bool parse_integer(const string& s, string& prefix, string& suffix, int& base) {
    if (s.empty()) return false;
    size_t idx = 0;
    if (s.size() > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        idx = 2;
        if (idx == s.size()) return false;
        int hex_digits = 0;
        while (idx < s.size()) {
            char c = s[idx];
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) { idx++; hex_digits++; }
            else break;
        }
        if (hex_digits == 0) return false;
    } else if (s[0] == '0') {
        base = 8;
        idx = 1;
        while (idx < s.size()) {
            char c = s[idx];
            if (c >= '0' && c <= '7') idx++;
            else if (c == '8' || c == '9') return false;
            else break;
        }
    } else if (s[0] >= '1' && s[0] <= '9') {
        base = 10;
        while (idx < s.size()) {
            char c = s[idx];
            if (c >= '0' && c <= '9') idx++;
            else break;
        }
    } else {
        return false;
    }
    
    prefix = s.substr(0, idx);
    suffix = s.substr(idx);
    return true;
}

uint64_t string_to_uint64(const string& s, int base) {
    uint64_t val = 0;
    size_t start = (base == 16) ? 2 : 0;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        int v = -1;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        if (val > (0xFFFFFFFFFFFFFFFFull - v) / base) throw logic_error("integer literal out of range");
        val = val * base + v;
    }
    return val;
}

bool is_standard_integer_suffix(string suf, bool& is_u, bool& is_l, bool& is_ll) {
    is_u = is_l = is_ll = false;
    if (suf == "") return true;
    if (suf == "u" || suf == "U") { is_u = true; return true; }
    if (suf == "l" || suf == "L") { is_l = true; return true; }
    if (suf == "ll" || suf == "LL") { is_ll = true; return true; }
    if (suf == "ul" || suf == "uL" || suf == "Ul" || suf == "UL") { is_u = true; is_l = true; return true; }
    if (suf == "lu" || suf == "Lu" || suf == "lU" || suf == "LU") { is_u = true; is_l = true; return true; }
    if (suf == "ull" || suf == "uLL" || suf == "Ull" || suf == "ULL") { is_u = true; is_ll = true; return true; }
    if (suf == "llu" || suf == "LLu" || suf == "llU" || suf == "LLU") { is_u = true; is_ll = true; return true; }
    return false;
}

bool is_valid_ud_suffix(const string& suf) {
    for (char c : suf) {
        if (c == '+' || c == '-' || c == '.') return false;
    }
    return true;
}


bool PA3Mock_IsDefinedIdentifier(const string& identifier) {
    if (identifier.empty()) return false;
    else return identifier[0] % 2;
}

bool is_integral_unsigned(EFundamentalType ftype) {
    switch(ftype) {
        case FT_UNSIGNED_CHAR:
        case FT_UNSIGNED_SHORT_INT:
        case FT_UNSIGNED_INT:
        case FT_UNSIGNED_LONG_INT:
        case FT_UNSIGNED_LONG_LONG_INT:
        case FT_CHAR16_T:
        case FT_CHAR32_T:
            return true;
        default:
            return false;
    }
}

EFundamentalType determine_int_type(int base, uint64_t val, bool is_u, bool is_l, bool is_ll) {
    uint64_t max_int = INT_MAX, max_uint = UINT_MAX, max_long = LONG_MAX;
    uint64_t max_ulong = ULONG_MAX, max_llong = LLONG_MAX, max_ullong = ULLONG_MAX;

    if (!is_u && !is_l && !is_ll) {
        if (base == 10) {
            if (val <= max_int) return FT_INT;
            if (val <= max_long) return FT_LONG_INT;
            if (val <= max_llong) return FT_LONG_LONG_INT;
        } else {
            if (val <= max_int) return FT_INT;
            if (val <= max_uint) return FT_UNSIGNED_INT;
            if (val <= max_long) return FT_LONG_INT;
            if (val <= max_ulong) return FT_UNSIGNED_LONG_INT;
            if (val <= max_llong) return FT_LONG_LONG_INT;
            if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
        }
    } else if (is_u && !is_l && !is_ll) {
        if (val <= max_uint) return FT_UNSIGNED_INT;
        if (val <= max_ulong) return FT_UNSIGNED_LONG_INT;
        if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
    } else if (!is_u && is_l) {
        if (base == 10) {
            if (val <= max_long) return FT_LONG_INT;
            if (val <= max_llong) return FT_LONG_LONG_INT;
        } else {
            if (val <= max_long) return FT_LONG_INT;
            if (val <= max_ulong) return FT_UNSIGNED_LONG_INT;
            if (val <= max_llong) return FT_LONG_LONG_INT;
            if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
        }
    } else if (is_u && is_l) {
        if (val <= max_ulong) return FT_UNSIGNED_LONG_INT;
        if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
    } else if (!is_u && is_ll) {
        if (base == 10) {
            if (val <= max_llong) return FT_LONG_LONG_INT;
        } else {
            if (val <= max_llong) return FT_LONG_LONG_INT;
            if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
        }
    } else if (is_u && is_ll) {
        if (val <= max_ullong) return FT_UNSIGNED_LONG_LONG_INT;
    }
    throw logic_error("out of range");
}

struct Value {
    bool is_unsigned;
    union {
        int64_t s_val;
        uint64_t u_val;
    };
};

enum PA3TokenType {
    TOK_IDENTIFIER,
    TOK_LITERAL,
    TOK_OP
};

struct PA3Token {
    PA3TokenType type;
    string str_val;
    Value lit_val;
    ETokenType op;
};

struct Expr {
    virtual Value eval() = 0;
    virtual bool is_unsigned_type() const = 0;
    virtual ~Expr() {}
};

struct LiteralExpr : Expr {
    Value val;
    LiteralExpr(Value v) : val(v) {}
    Value eval() override { return val; }
    bool is_unsigned_type() const override { return val.is_unsigned; }
};

struct UnaryExpr : Expr {
    ETokenType op;
    unique_ptr<Expr> expr;
    UnaryExpr(ETokenType o, unique_ptr<Expr> e) : op(o), expr(move(e)) {}
    bool is_unsigned_type() const override {
        if (op == OP_LNOT) return false;
        return expr->is_unsigned_type();
    }
    Value eval() override {
        Value v = expr->eval();
        if (op == OP_PLUS) return v;
        if (op == OP_MINUS) {
            if (v.is_unsigned) v.u_val = -v.u_val;
            else v.s_val = -v.s_val;
            return v;
        }
        if (op == OP_LNOT) {
            bool is_zero = v.is_unsigned ? (v.u_val == 0) : (v.s_val == 0);
            Value r; r.is_unsigned = false; r.s_val = is_zero ? 1 : 0;
            return r;
        }
        if (op == OP_COMPL) {
            if (v.is_unsigned) v.u_val = ~v.u_val;
            else v.s_val = ~v.s_val;
            return v;
        }
        throw logic_error("unknown unary op");
    }
};

struct BinaryExpr : Expr {
    ETokenType op;
    unique_ptr<Expr> left, right;
    BinaryExpr(ETokenType o, unique_ptr<Expr> l, unique_ptr<Expr> r) : op(o), left(move(l)), right(move(r)) {}
    
    bool is_unsigned_type() const override {
        if (op == OP_LT || op == OP_GT || op == OP_LE || op == OP_GE || op == OP_EQ || op == OP_NE || op == OP_LAND || op == OP_LOR) return false;
        if (op == OP_LSHIFT || op == OP_RSHIFT) return left->is_unsigned_type();
        return left->is_unsigned_type() || right->is_unsigned_type();
    }
    
    Value eval() override {
        if (op == OP_LAND) {
            Value l = left->eval();
            bool lt = l.is_unsigned ? (l.u_val != 0) : (l.s_val != 0);
            if (!lt) { Value r; r.is_unsigned = false; r.s_val = 0; return r; }
            Value r = right->eval();
            bool rt = r.is_unsigned ? (r.u_val != 0) : (r.s_val != 0);
            Value res; res.is_unsigned = false; res.s_val = rt ? 1 : 0; return res;
        }
        if (op == OP_LOR) {
            Value l = left->eval();
            bool lt = l.is_unsigned ? (l.u_val != 0) : (l.s_val != 0);
            if (lt) { Value r; r.is_unsigned = false; r.s_val = 1; return r; }
            Value r = right->eval();
            bool rt = r.is_unsigned ? (r.u_val != 0) : (r.s_val != 0);
            Value res; res.is_unsigned = false; res.s_val = rt ? 1 : 0; return res;
        }
        
        Value a = left->eval();
        Value b = right->eval();
        if (op == OP_LSHIFT || op == OP_RSHIFT) {
            int64_t shift_amount = b.is_unsigned ? b.u_val : b.s_val;
            if (shift_amount < 0 || shift_amount >= 64) throw logic_error("invalid shift");
            if (op == OP_LSHIFT) {
                if (a.is_unsigned) a.u_val = a.u_val << shift_amount;
                else a.s_val = a.s_val << shift_amount;
            } else {
                if (a.is_unsigned) a.u_val = a.u_val >> shift_amount;
                else a.s_val = a.s_val >> shift_amount;
            }
            return a;
        }
        
        bool is_u = a.is_unsigned || b.is_unsigned;
        uint64_t au = a.is_unsigned ? a.u_val : (uint64_t)a.s_val;
        uint64_t bu = b.is_unsigned ? b.u_val : (uint64_t)b.s_val;
        
        if (is_u) {
            uint64_t res = 0;
            switch(op) {
                case OP_STAR: res = au * bu; break;
                case OP_DIV: if (bu == 0) throw logic_error("div 0"); res = au / bu; break;
                case OP_MOD: if (bu == 0) throw logic_error("mod 0"); res = au % bu; break;
                case OP_PLUS: res = au + bu; break;
                case OP_MINUS: res = au - bu; break;
                case OP_AMP: res = au & bu; break;
                case OP_XOR: res = au ^ bu; break;
                case OP_BOR: res = au | bu; break;
                case OP_LT: { Value v; v.is_unsigned = false; v.s_val = au < bu ? 1 : 0; return v; }
                case OP_GT: { Value v; v.is_unsigned = false; v.s_val = au > bu ? 1 : 0; return v; }
                case OP_LE: { Value v; v.is_unsigned = false; v.s_val = au <= bu ? 1 : 0; return v; }
                case OP_GE: { Value v; v.is_unsigned = false; v.s_val = au >= bu ? 1 : 0; return v; }
                case OP_EQ: { Value v; v.is_unsigned = false; v.s_val = au == bu ? 1 : 0; return v; }
                case OP_NE: { Value v; v.is_unsigned = false; v.s_val = au != bu ? 1 : 0; return v; }
                default: throw logic_error("unknown binary op");
            }
            Value v; v.is_unsigned = true; v.u_val = res; return v;
        } else {
            int64_t as = a.s_val, bs = b.s_val;
            int64_t res = 0;
            switch(op) {
                case OP_STAR: res = as * bs; break;
                case OP_DIV: 
                    if (bs == 0) throw logic_error("div 0");
                    if (as == INT64_MIN && bs == -1) throw logic_error("overflow");
                    else res = as / bs; 
                    break;
                case OP_MOD: 
                    if (bs == 0) throw logic_error("mod 0");
                    if (as == INT64_MIN && bs == -1) throw logic_error("overflow");
                    else res = as % bs; 
                    break;
                case OP_PLUS: res = as + bs; break;
                case OP_MINUS: res = as - bs; break;
                case OP_AMP: res = as & bs; break;
                case OP_XOR: res = as ^ bs; break;
                case OP_BOR: res = as | bs; break;
                case OP_LT: { Value v; v.is_unsigned = false; v.s_val = as < bs ? 1 : 0; return v; }
                case OP_GT: { Value v; v.is_unsigned = false; v.s_val = as > bs ? 1 : 0; return v; }
                case OP_LE: { Value v; v.is_unsigned = false; v.s_val = as <= bs ? 1 : 0; return v; }
                case OP_GE: { Value v; v.is_unsigned = false; v.s_val = as >= bs ? 1 : 0; return v; }
                case OP_EQ: { Value v; v.is_unsigned = false; v.s_val = as == bs ? 1 : 0; return v; }
                case OP_NE: { Value v; v.is_unsigned = false; v.s_val = as != bs ? 1 : 0; return v; }
                default: throw logic_error("unknown binary op");
            }
            Value v; v.is_unsigned = false; v.s_val = res; return v;
        }
    }
};

struct TernaryExpr : Expr {
    unique_ptr<Expr> cond, left, right;
    TernaryExpr(unique_ptr<Expr> c, unique_ptr<Expr> l, unique_ptr<Expr> r) : cond(move(c)), left(move(l)), right(move(r)) {}
    
    bool is_unsigned_type() const override {
        return left->is_unsigned_type() || right->is_unsigned_type();
    }
    
    Value eval() override {
        Value c = cond->eval();
        bool ct = c.is_unsigned ? (c.u_val != 0) : (c.s_val != 0);
        Value res = ct ? left->eval() : right->eval();
        
        bool is_u = is_unsigned_type();
        if (is_u && !res.is_unsigned) {
            res.is_unsigned = true;
            res.u_val = (uint64_t)res.s_val;
        }
        return res;
    }
};

class Parser {
    const vector<PA3Token>& tokens;
    size_t pos;

public:
    Parser(const vector<PA3Token>& t) : tokens(t), pos(0) {}

    bool match(ETokenType op) {
        if (pos < tokens.size() && tokens[pos].type == TOK_OP && tokens[pos].op == op) {
            pos++;
            return true;
        }
        return false;
    }
    
    unique_ptr<Expr> parse_primary() {
        if (pos >= tokens.size()) throw logic_error("unexpected eof");
        PA3Token t = tokens[pos++];
        if (t.type == TOK_LITERAL) {
            return unique_ptr<Expr>(new LiteralExpr(t.lit_val));
        } else if (t.type == TOK_IDENTIFIER) {
            if (t.str_val == "defined") {
                bool has_paren = match(OP_LPAREN);
                if (pos >= tokens.size() || tokens[pos].type != TOK_IDENTIFIER) throw logic_error("expected identifier after defined");
                string id = tokens[pos++].str_val;
                if (has_paren) {
                    if (!match(OP_RPAREN)) throw logic_error("expected )");
                }
                Value v; v.is_unsigned = false; v.s_val = PA3Mock_IsDefinedIdentifier(id) ? 1 : 0;
                return unique_ptr<Expr>(new LiteralExpr(v));
            } else if (t.str_val == "true") {
                Value v; v.is_unsigned = false; v.s_val = 1;
                return unique_ptr<Expr>(new LiteralExpr(v));
            } else {
                Value v; v.is_unsigned = false; v.s_val = 0;
                return unique_ptr<Expr>(new LiteralExpr(v));
            }
        } else if (t.type == TOK_OP && t.op == OP_LPAREN) {
            auto e = parse_expr();
            if (!match(OP_RPAREN)) throw logic_error("expected )");
            return e;
        }
        throw logic_error("expected primary");
    }

    unique_ptr<Expr> parse_unary() {
        if (match(OP_PLUS) || match(OP_MINUS) || match(OP_LNOT) || match(OP_COMPL)) {
            ETokenType op = tokens[pos-1].op;
            return unique_ptr<Expr>(new UnaryExpr(op, parse_unary()));
        }
        return parse_primary();
    }

    unique_ptr<Expr> parse_multiplicative() {
        auto a = parse_unary();
        while (match(OP_STAR) || match(OP_DIV) || match(OP_MOD)) {
            ETokenType op = tokens[pos-1].op;
            a = unique_ptr<Expr>(new BinaryExpr(op, move(a), parse_unary()));
        }
        return a;
    }

    unique_ptr<Expr> parse_additive() {
        auto a = parse_multiplicative();
        while (match(OP_PLUS) || match(OP_MINUS)) {
            ETokenType op = tokens[pos-1].op;
            a = unique_ptr<Expr>(new BinaryExpr(op, move(a), parse_multiplicative()));
        }
        return a;
    }

    unique_ptr<Expr> parse_shift() {
        auto a = parse_additive();
        while (match(OP_LSHIFT) || match(OP_RSHIFT)) {
            ETokenType op = tokens[pos-1].op;
            a = unique_ptr<Expr>(new BinaryExpr(op, move(a), parse_additive()));
        }
        return a;
    }

    unique_ptr<Expr> parse_relational() {
        auto a = parse_shift();
        while (match(OP_LT) || match(OP_GT) || match(OP_LE) || match(OP_GE)) {
            ETokenType op = tokens[pos-1].op;
            a = unique_ptr<Expr>(new BinaryExpr(op, move(a), parse_shift()));
        }
        return a;
    }

    unique_ptr<Expr> parse_equality() {
        auto a = parse_relational();
        while (match(OP_EQ) || match(OP_NE)) {
            ETokenType op = tokens[pos-1].op;
            a = unique_ptr<Expr>(new BinaryExpr(op, move(a), parse_relational()));
        }
        return a;
    }

    unique_ptr<Expr> parse_and() {
        auto a = parse_equality();
        while (match(OP_AMP)) {
            a = unique_ptr<Expr>(new BinaryExpr(OP_AMP, move(a), parse_equality()));
        }
        return a;
    }

    unique_ptr<Expr> parse_xor() {
        auto a = parse_and();
        while (match(OP_XOR)) {
            a = unique_ptr<Expr>(new BinaryExpr(OP_XOR, move(a), parse_and()));
        }
        return a;
    }

    unique_ptr<Expr> parse_or() {
        auto a = parse_xor();
        while (match(OP_BOR)) {
            a = unique_ptr<Expr>(new BinaryExpr(OP_BOR, move(a), parse_xor()));
        }
        return a;
    }

    unique_ptr<Expr> parse_logical_and() {
        auto a = parse_or();
        while (match(OP_LAND)) {
            a = unique_ptr<Expr>(new BinaryExpr(OP_LAND, move(a), parse_or()));
        }
        return a;
    }

    unique_ptr<Expr> parse_logical_or() {
        auto a = parse_logical_and();
        while (match(OP_LOR)) {
            a = unique_ptr<Expr>(new BinaryExpr(OP_LOR, move(a), parse_logical_and()));
        }
        return a;
    }

    unique_ptr<Expr> parse_expr() {
        auto a = parse_logical_or();
        if (match(OP_QMARK)) {
            auto b = parse_expr();
            if (!match(OP_COLON)) throw logic_error("expected :");
            auto c = parse_expr();
            return unique_ptr<Expr>(new TernaryExpr(move(a), move(b), move(c)));
        }
        return a;
    }
    
    unique_ptr<Expr> parse() {
        if (pos >= tokens.size()) return nullptr;
        auto expr = parse_expr();
        if (pos < tokens.size()) throw logic_error("extra tokens");
        return expr;
    }
};

void process_line(const vector<PPToken>& line) {
    if (line.empty()) return;
    try {
        vector<PA3Token> pa3_tokens;
        for (const PPToken& t : line) {
            if (t.type == "non-whitespace-character" || t.type == "header-name" || t.type == "string-literal" || t.type == "user-defined-string-literal") {
                throw logic_error("invalid token");
            } else if (t.type == "preprocessing-op-or-punc") {
                if (t.data == "#" || t.data == "##" || t.data == "%:" || t.data == "%:%:") throw logic_error("invalid op");
                if (StringToTokenTypeMap.count(t.data)) {
                    PA3Token tok; tok.type = TOK_OP; tok.op = StringToTokenTypeMap.at(t.data);
                    pa3_tokens.push_back(tok);
                } else throw logic_error("unknown op");
            } else if (t.type == "identifier") {
                PA3Token tok; tok.type = TOK_IDENTIFIER; tok.str_val = t.data;
                pa3_tokens.push_back(tok);
            } else if (t.type == "character-literal" || t.type == "user-defined-character-literal") {
                LiteralInfo info = parse_literal(t.data, true);
                if (!info.suffix.empty()) throw logic_error("ud_suffix not allowed");
                vector<int> cps = decode_escapes(info.content_utf8, false);
                if (cps.size() != 1) throw logic_error("char literal must have 1 code point");
                int cp = cps[0];
                if (cp < 0 || (cp >= 0xD800 && cp < 0xE000) || cp >= 0x110000) throw logic_error("char out of range");
                
                EFundamentalType ftype;
                if (info.prefix == "") {
                    if (cp <= 127) ftype = FT_CHAR;
                    else ftype = FT_INT;
                } else if (info.prefix == "u") { if (cp > 0xFFFF) throw logic_error("char16 out of range"); ftype = FT_CHAR16_T; }
                else if (info.prefix == "U") ftype = FT_CHAR32_T;
                else if (info.prefix == "L") ftype = FT_WCHAR_T;
                else throw logic_error("invalid prefix");
                
                PA3Token tok; tok.type = TOK_LITERAL; tok.lit_val.is_unsigned = is_integral_unsigned(ftype);
                tok.lit_val.u_val = cp;
                pa3_tokens.push_back(tok);
            } else if (t.type == "pp-number") {
                static const std::regex float_re(R"(^(([0-9]+\.[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)(.*)$)");
                std::smatch match;
                if (std::regex_match(t.data, match, float_re)) throw logic_error("float not allowed");
                
                string prefix, suffix; int base;
                if (parse_integer(t.data, prefix, suffix, base)) {
                    bool is_u, is_l, is_ll;
                    if (is_standard_integer_suffix(suffix, is_u, is_l, is_ll)) {
                        uint64_t val = string_to_uint64(prefix, base);
                        EFundamentalType ftype = determine_int_type(base, val, is_u, is_l, is_ll);
                        PA3Token tok; tok.type = TOK_LITERAL; tok.lit_val.is_unsigned = is_integral_unsigned(ftype);
                        tok.lit_val.u_val = val;
                        pa3_tokens.push_back(tok);
                    } else throw logic_error("invalid suffix");
                } else throw logic_error("malformed number");
            } else {
                throw logic_error("unknown token type");
            }
        }
        
        if (pa3_tokens.empty()) return;
        Parser parser(pa3_tokens);
        auto expr = parser.parse();
        if (!expr) throw logic_error("empty expr");
        Value res = expr->eval();
        if (res.is_unsigned) cout << res.u_val << "u\n";
        else cout << res.s_val << "\n";
    } catch (...) {
        cout << "error\n";
    }
}

int main() {
    try {
        ostringstream oss;
        oss << cin.rdbuf();
        string input = oss.str();
        VectorPPTokenStream pp_output;
        PPTokenizer tokenizer(pp_output);
        for (char c : input) tokenizer.process((unsigned char)c);
        tokenizer.process(EndOfFile);
        cout << "eof\n";
    } catch (...) {
        return EXIT_FAILURE;
    }
}
