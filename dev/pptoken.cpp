#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

#include "IPPTokenStream.h"
#include "DebugPPTokenStream.h"

constexpr int EndOfFile = -1;

int HexCharToValue(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw logic_error("HexCharToValue of nonhex char");
}

const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted = {
    {0xA8,0xA8}, {0xAA,0xAA}, {0xAD,0xAD}, {0xAF,0xAF}, {0xB2,0xB5}, {0xB7,0xBA},
    {0xBC,0xBE}, {0xC0,0xD6}, {0xD8,0xF6}, {0xF8,0xFF}, {0x100,0x167F}, {0x1681,0x180D},
    {0x180F,0x1FFF}, {0x200B,0x200D}, {0x202A,0x202E}, {0x203F,0x2040}, {0x2054,0x2054},
    {0x2060,0x206F}, {0x2070,0x218F}, {0x2460,0x24FF}, {0x2776,0x2793}, {0x2C00,0x2DFF},
    {0x2E80,0x2FFF}, {0x3004,0x3007}, {0x3021,0x302F}, {0x3031,0x303F}, {0x3040,0xD7FF},
    {0xF900,0xFD3D}, {0xFD40,0xFDCF}, {0xFDF0,0xFE44}, {0xFE47,0xFFFD}, {0x10000,0x1FFFD},
    {0x20000,0x2FFFD}, {0x30000,0x3FFFD}, {0x40000,0x4FFFD}, {0x50000,0x5FFFD},
    {0x60000,0x6FFFD}, {0x70000,0x7FFFD}, {0x80000,0x8FFFD}, {0x90000,0x9FFFD},
    {0xA0000,0xAFFFD}, {0xB0000,0xBFFFD}, {0xC0000,0xCFFFD}, {0xD0000,0xDFFFD},
    {0xE0000,0xEFFFD}
};

const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted = {
    {0x300,0x36F}, {0x1DC0,0x1DFF}, {0x20D0,0x20FF}, {0xFE20,0xFE2F}
};

const unordered_set<string> Digraph_IdentifierLike_Operators = {
    "new", "delete", "and", "and_eq", "bitand",
    "bitor", "compl", "not", "not_eq", "or",
    "or_eq", "xor", "xor_eq"
};

const vector<string> ops = {
    "%:%:", "<<=", ">>=", "->*", "...", 
    "<%", "%>", "<:", ":>", "%:", "##", "::", ".*",
    "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=",
    "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "++", "--", "->",
    "{", "}", "[", "]", "#", "(", ")", ";", ":", "?",
    ".", "+", "-", "*", "/", "%", "^", "&", "|", "~",
    "!", "=", "<", ">", ","
};

bool is_identifier_nondigit_start(int c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
    for (auto& range : AnnexE2_DisallowedInitially_RangesSorted) {
        if (c >= range.first && c <= range.second) return false;
    }
    for (auto& range : AnnexE1_Allowed_RangesSorted) {
        if (c >= range.first && c <= range.second) return true;
    }
    return false;
}

bool is_identifier_continue(int c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
    if (c >= '0' && c <= '9') return true;
    for (auto& range : AnnexE1_Allowed_RangesSorted) {
        if (c >= range.first && c <= range.second) return true;
    }
    return false;
}

bool is_digit(int c) { return c >= '0' && c <= '9'; }
bool is_whitespace(int c) { return c == ' ' || c == '\t' || c == '\v' || c == '\f'; }
bool is_hex(int c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

string utf8_encode(const vector<int>& cp_seq) {
    string s;
    for (int cp : cp_seq) {
        if (cp <= 0x7F) {
            s += (char)cp;
        } else if (cp <= 0x7FF) {
            s += (char)(0xC0 | ((cp >> 6) & 0x1F));
            s += (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            s += (char)(0xE0 | ((cp >> 12) & 0x0F));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        } else {
            s += (char)(0xF0 | ((cp >> 18) & 0x07));
            s += (char)(0x80 | ((cp >> 12) & 0x3F));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        }
    }
    return s;
}

class IPullStream {
protected:
    vector<int> buffer;
public:
    virtual ~IPullStream() = default;
    virtual int read_next() = 0;
    
    int get() {
        if (!buffer.empty()) {
            int c = buffer.back();
            buffer.pop_back();
            return c;
        }
        return read_next();
    }
    
    void unget(int c) {
        if (c != EndOfFile) buffer.push_back(c);
    }
    
    int peek() {
        int c = get();
        unget(c);
        return c;
    }
};

class UTF8Stream : public IPullStream {
    const vector<unsigned char>& data;
    size_t pos = 0;
public:
    UTF8Stream(const vector<unsigned char>& d) : data(d) {}
    int read_next() override {
        if (pos >= data.size()) return EndOfFile;
        int c = data[pos++];
        if ((c & 0x80) == 0) return c;
        if ((c & 0xE0) == 0xC0) {
            if (pos >= data.size()) throw logic_error("invalid utf-8");
            int c2 = data[pos++];
            if ((c2 & 0xC0) != 0x80) throw logic_error("invalid utf-8");
            return ((c & 0x1F) << 6) | (c2 & 0x3F);
        }
        if ((c & 0xF0) == 0xE0) {
            if (pos + 1 >= data.size()) throw logic_error("invalid utf-8");
            int c2 = data[pos++];
            int c3 = data[pos++];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) throw logic_error("invalid utf-8");
            return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        if ((c & 0xF8) == 0xF0) {
            if (pos + 2 >= data.size()) throw logic_error("invalid utf-8");
            int c2 = data[pos++];
            int c3 = data[pos++];
            int c4 = data[pos++];
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80) throw logic_error("invalid utf-8");
            return ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
        }
        throw logic_error("invalid utf-8");
    }
};

class TrigraphStream : public IPullStream {
    IPullStream& in;
public:
    bool raw_mode = false;
    TrigraphStream(IPullStream& in) : in(in) {}
    int read_next() override {
        int c = in.get();
        if (raw_mode || c != '?') return c;
        
        int c2 = in.get();
        if (c2 != '?') {
            in.unget(c2);
            return c;
        }
        
        int c3 = in.get();
        int rep = -1;
        switch (c3) {
            case '=': rep = '#'; break;
            case '/': rep = '\\'; break;
            case '\'': rep = '^'; break;
            case '(': rep = '['; break;
            case ')': rep = ']'; break;
            case '!': rep = '|'; break;
            case '<': rep = '{'; break;
            case '>': rep = '}'; break;
            case '-': rep = '~'; break;
        }
        
        if (rep != -1) return rep;
        
        in.unget(c3);
        in.unget(c2);
        return c;
    }
};

class UCNStream : public IPullStream {
    TrigraphStream& in;
public:
    UCNStream(TrigraphStream& in) : in(in) {}
    int read_next() override {
        int c = in.get();
        if (in.raw_mode || c != '\\') return c;
        
        int c2 = in.get();
        if (c2 == 'u') {
            int h1 = in.get(); int h2 = in.get(); int h3 = in.get(); int h4 = in.get();
            if (is_hex(h1) && is_hex(h2) && is_hex(h3) && is_hex(h4)) {
                return (HexCharToValue(h1) << 12) | (HexCharToValue(h2) << 8) | (HexCharToValue(h3) << 4) | HexCharToValue(h4);
            }
            in.unget(h4); in.unget(h3); in.unget(h2); in.unget(h1);
        } else if (c2 == 'U') {
            int h[8]; bool valid = true;
            for (int i = 0; i < 8; ++i) { h[i] = in.get(); if (!is_hex(h[i])) valid = false; }
            if (valid) {
                int cp = 0;
                for (int i = 0; i < 8; ++i) cp = (cp << 4) | HexCharToValue(h[i]);
                return cp;
            }
            for (int i = 7; i >= 0; --i) in.unget(h[i]);
        }
        
        in.unget(c2);
        return c;
    }
};

class LineSplicerStream : public IPullStream {
    TrigraphStream& trigraph;
    UCNStream& in;
public:
    LineSplicerStream(TrigraphStream& trigraph, UCNStream& in) : trigraph(trigraph), in(in) {}
    int read_next() override {
        int c = in.get();
        if (trigraph.raw_mode || c != '\\') return c;
        
        int c2 = in.get();
        if (c2 == '\n') {
            return get(); // recursive splice
        }
        
        in.unget(c2);
        return c;
    }
};

class EOFNewlinerStream : public IPullStream {
    LineSplicerStream& in;
    int last_char = -1;
    bool eof_newline_emitted = false;
    bool is_empty = true;
public:
    EOFNewlinerStream(LineSplicerStream& in) : in(in) {}
    int read_next() override {
        int c = in.get();
        if (c == EndOfFile) {
            if (!is_empty && last_char != '\n' && !eof_newline_emitted) {
                eof_newline_emitted = true;
                return '\n';
            }
            return EndOfFile;
        }
        is_empty = false;
        last_char = c;
        return c;
    }
};

struct PPTokenizer {
    IPPTokenStream& output;
    vector<unsigned char> data;
    
    PPTokenizer(IPPTokenStream& output) : output(output) {}
    
    void process(int code_unit) {
        if (code_unit != EndOfFile) {
            data.push_back(code_unit);
            return;
        }
        
        UTF8Stream utf8(data);
        TrigraphStream trigraph(utf8);
        UCNStream ucn(trigraph);
        LineSplicerStream splicer(trigraph, ucn);
        EOFNewlinerStream in(splicer);
        
        int state = 0; // 0=start, 1=hash, 2=include, 3=normal
        auto on_token = [&](string type, const string& val) {
            if (type == "whitespace-sequence") return;
            if (type == "new-line") { state = 0; return; }
            if (state == 0 && type == "preprocessing-op-or-punc" && (val == "#" || val == "%:")) state = 1;
            else if (state == 1 && type == "identifier" && val == "include") state = 2;
            else state = 3;
        };
        
        auto emit = [&](string type, const vector<int>& seq) {
            string s = utf8_encode(seq);
            if (type == "identifier" && Digraph_IdentifierLike_Operators.count(s)) {
                output.emit_preprocessing_op_or_punc(s);
                on_token("preprocessing-op-or-punc", s);
                return;
            }
            if (type == "identifier") output.emit_identifier(s);
            else if (type == "pp-number") output.emit_pp_number(s);
            else if (type == "character-literal") output.emit_character_literal(s);
            else if (type == "user-defined-character-literal") output.emit_user_defined_character_literal(s);
            else if (type == "string-literal") output.emit_string_literal(s);
            else if (type == "user-defined-string-literal") output.emit_user_defined_string_literal(s);
            else if (type == "preprocessing-op-or-punc") output.emit_preprocessing_op_or_punc(s);
            else if (type == "header-name") output.emit_header_name(s);
            else if (type == "non-whitespace-character") output.emit_non_whitespace_char(s);
            else if (type == "whitespace-sequence") output.emit_whitespace_sequence();
            else if (type == "new-line") output.emit_new_line();
            on_token(type, s);
        };
        
        while (true) {
            int c = in.get();
            if (c == EndOfFile) break;
            
            if (c == '\n') {
                emit("new-line", {'\n'});
                continue;
            }
            
            if (is_whitespace(c) || c == '/') {
                bool is_ws = false;
                in.unget(c);
                while (true) {
                    int c2 = in.get();
                    if (c2 == EndOfFile) break;
                    if (is_whitespace(c2)) {
                        is_ws = true;
                    } else if (c2 == '/') {
                        int c3 = in.get();
                        if (c3 == '/') {
                            is_ws = true;
                            while (true) {
                                int c4 = in.get();
                                if (c4 == '\n' || c4 == EndOfFile) { in.unget(c4); break; }
                            }
                        } else if (c3 == '*') {
                            is_ws = true;
                            while (true) {
                                int c4 = in.get();
                                if (c4 == EndOfFile) throw logic_error("unterminated block comment");
                                if (c4 == '*') {
                                    int c5 = in.get();
                                    if (c5 == EndOfFile) throw logic_error("unterminated block comment");
                                    if (c5 == '/') break;
                                    in.unget(c5);
                                }
                            }
                        } else {
                            in.unget(c3);
                            in.unget(c2);
                            break;
                        }
                    } else {
                        in.unget(c2);
                        break;
                    }
                }
                if (is_ws) {
                    emit("whitespace-sequence", {});
                    continue;
                }
                c = in.get();
            }
            if (state == 2 && (c == '<' || c == '"')) {
                vector<int> seq = {c};
                bool closed = false;
                while (true) {
                    int c2 = in.get();
                    if (c2 == '\n' || c2 == EndOfFile) { in.unget(c2); break; }
                    seq.push_back(c2);
                    if ((c == '<' && c2 == '>') || (c == '"' && c2 == '"')) { closed = true; break; }
                }
                if (closed) { emit("header-name", seq); continue; }
                for (int i = (int)seq.size() - 1; i >= 1; --i) in.unget(seq[i]);
            }
            
            if (is_digit(c) || (c == '.' && is_digit(in.peek()))) {
                vector<int> seq = {c};
                if (c == '.') seq.push_back(in.get());
                while (true) {
                    int c2 = in.get();
                    if (c2 == 'e' || c2 == 'E' || c2 == 'p' || c2 == 'P') { seq.push_back(c2); int c3 = in.get(); if (c3 == '+' || c3 == '-') seq.push_back(c3); else in.unget(c3); } else if (is_digit(c2) || is_identifier_continue(c2) || c2 == '.') {
                        seq.push_back(c2);
                    } else if (false) {
                        seq.push_back(c2);
                        int c3 = in.get();
                        if (c3 == '+' || c3 == '-') seq.push_back(c3);
                        else in.unget(c3);
                    } else {
                        in.unget(c2);
                        break;
                    }
                }
                emit("pp-number", seq);
                continue;
            }
            
            if (is_identifier_nondigit_start(c)) {
                vector<int> seq = {c};
                while (true) {
                    int c2 = in.get();
                    if (is_identifier_continue(c2)) seq.push_back(c2);
                    else { in.unget(c2); break; }
                }
                
                int quote = in.get();
                bool is_literal = false, is_raw = false, is_char = false;
                string pref = utf8_encode(seq);
                
                if (quote == '"' || quote == '\'') {
                    if (quote == '\'') {
                        if (pref == "u" || pref == "U" || pref == "L") { is_literal = true; is_char = true; }
                    } else {
                        if (pref == "u8" || pref == "u" || pref == "U" || pref == "L") is_literal = true;
                        if (pref == "R" || pref == "u8R" || pref == "uR" || pref == "UR" || pref == "LR") {
                            is_literal = true; is_raw = true;
                        }
                    }
                }
                
                if (is_literal) {
                    seq.push_back(quote);
                    if (is_raw) {
                        trigraph.raw_mode = true;
                        vector<int> dchar;
                        while (true) {
                            int c2 = in.get();
                            if (c2 == EndOfFile) throw logic_error("unterminated raw string literal");
                            seq.push_back(c2);
                            if (c2 == '(') break;
                            dchar.push_back(c2);
                            if (dchar.size() > 16) throw logic_error("raw string delimiter too long");
                        }
                        while (true) {
                            int c2 = in.get();
                            if (c2 == EndOfFile) throw logic_error("unterminated raw string literal");
                            seq.push_back(c2);
                            if (c2 == ')') {
                                bool match = true;
                                vector<int> peeked;
                                for (int d : dchar) {
                                    int c3 = in.get();
                                    if (c3 == EndOfFile) { match = false; break; }
                                    peeked.push_back(c3);
                                    if (c3 != d) match = false;
                                }
                                if (match) {
                                    int c4 = in.get();
                                    if (c4 == EndOfFile) { match = false; }
                                    peeked.push_back(c4);
                                    if (c4 != '"') match = false;
                                }
                                
                                if (match) {
                                    for (int p : peeked) seq.push_back(p);
                                    break;
                                } else {
                                    for (int i = (int)peeked.size() - 1; i >= 0; --i) in.unget(peeked[i]);
                                }
                            }
                        }
                        trigraph.raw_mode = false;
                    } else {
                        bool closed = false;
                        while (true) {
                            int c2 = in.get();
                            if (c2 == EndOfFile || c2 == '\n') throw logic_error("unterminated string literal");
                            seq.push_back(c2);
                            if (c2 == '\\') {
                                int c3 = in.get();
                                                            if (c3 != EndOfFile) seq.push_back(c3);
                            bool valid = false;
                            if (c3 == '\'' || c3 == '"' || c3 == '?' || c3 == '\\' || c3 == 'a' || c3 == 'b' || c3 == 'f' || c3 == 'n' || c3 == 'r' || c3 == 't' || c3 == 'v') valid = true;
                            else if (c3 >= '0' && c3 <= '7') valid = true;
                            else if (c3 == 'x') {
                                int c4 = in.get();
                                if (c4 == EndOfFile) throw logic_error("unterminated string literal");
                                if ((c4 >= '0' && c4 <= '9') || (c4 >= 'A' && c4 <= 'F') || (c4 >= 'a' && c4 <= 'f')) valid = true;
                                in.unget(c4);
                            }
                            if (!valid) throw logic_error("invalid escape sequence");
                            } else if (c2 == quote) {
                                closed = true; break;
                            }
                        }
                        if (!closed) {
                            // rollback to identifier
                            in.unget(quote);
                            for (int i = (int)seq.size() - 2; i >= 0; --i) in.unget(seq[i]);
                            emit("identifier", vector<int>(seq.begin(), seq.begin() + pref.size()));
                            continue;
                        }
                    }
                    
                    while (true) {
                        int c2 = in.get();
                        if (is_identifier_continue(c2)) seq.push_back(c2);
                        else { in.unget(c2); break; }
                    }
                    
                    string type = is_char ? "character-literal" : "string-literal";
                    if (is_identifier_continue(seq.back())) type = "user-defined-" + type;
                    emit(type, seq);
                    continue;
                } else {
                    in.unget(quote);
                    emit("identifier", seq);
                    continue;
                }
            }
            
            if (c == '\'' || c == '"') {
                vector<int> seq = {c};
                bool closed = false;
                while (true) {
                    int c2 = in.get();
                    if (c2 == EndOfFile || c2 == '\n') throw logic_error("unterminated string literal");
                    seq.push_back(c2);
                    if (c2 == '\\') {
                        int c3 = in.get();
                                                    if (c3 != EndOfFile) seq.push_back(c3);
                            bool valid = false;
                            if (c3 == '\'' || c3 == '"' || c3 == '?' || c3 == '\\' || c3 == 'a' || c3 == 'b' || c3 == 'f' || c3 == 'n' || c3 == 'r' || c3 == 't' || c3 == 'v') valid = true;
                            else if (c3 >= '0' && c3 <= '7') valid = true;
                            else if (c3 == 'x') {
                                int c4 = in.get();
                                if (c4 == EndOfFile) throw logic_error("unterminated string literal");
                                if ((c4 >= '0' && c4 <= '9') || (c4 >= 'A' && c4 <= 'F') || (c4 >= 'a' && c4 <= 'f')) valid = true;
                                in.unget(c4);
                            }
                            if (!valid) throw logic_error("invalid escape sequence");
                    } else if (c2 == c) {
                        closed = true; break;
                    }
                }
                if (closed) {
                    while (true) {
                        int c2 = in.get();
                        if (is_identifier_continue(c2)) seq.push_back(c2);
                        else { in.unget(c2); break; }
                    }
                    string type = (c == '\'') ? "character-literal" : "string-literal";
                    if (is_identifier_continue(seq.back())) type = "user-defined-" + type;
                    emit(type, seq);
                    continue;
                } else {
                    for (int i = (int)seq.size() - 1; i >= 1; --i) in.unget(seq[i]);
                }
            }
            
            if (c == '<') {
                int c2 = in.get(); int c3 = in.get(); int c4 = in.get();
                if (c2 == ':' && c3 == ':' && c4 != ':' && c4 != '>') {
                    in.unget(c4); in.unget(c3); in.unget(c2);
                    emit("preprocessing-op-or-punc", {'<'});
                    continue;
                }
                in.unget(c4); in.unget(c3); in.unget(c2);
            }
            
            bool found_op = false;
            for (int len = 4; len >= 1; --len) {
                vector<int> tseq = {c};
                bool has_eof = false;
                for (int i = 1; i < len; ++i) {
                    int n = in.get();
                    if (n == EndOfFile) has_eof = true;
                    tseq.push_back(n);
                }
                if (!has_eof) {
                    string s = utf8_encode(tseq);
                    if (find(ops.begin(), ops.end(), s) != ops.end()) {
                        emit("preprocessing-op-or-punc", tseq);
                        found_op = true;
                        break;
                    }
                }
                for (int i = len - 1; i >= 1; --i) in.unget(tseq[i]);
            }
            if (found_op) continue;
            
            emit("non-whitespace-character", {c});
        }
        
        output.emit_eof();
    }
};

int main() {
    try {
        ostringstream oss;
        oss << cin.rdbuf();
        string input = oss.str();
        DebugPPTokenStream output;
        PPTokenizer tokenizer(output);
        for (char c : input) tokenizer.process((unsigned char)c);
        tokenizer.process(EndOfFile);
    } catch (exception& e) {
        cerr << "ERROR: " << e.what() << endl;
        return EXIT_FAILURE;
    }
}
