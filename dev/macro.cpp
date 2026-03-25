// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <iostream>
#include <sstream>
#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#define main posttoken_embedded_main
#include "posttoken.cpp"
#undef main

struct MacroToken
{
	PPToken pp;
	set<string> hidden;
	bool unavailable = false;
	bool placemarker = false;
	bool op_hashhash = false;

	MacroToken() {}
	explicit MacroToken(const PPToken& pp) : pp(pp) {}
};

MacroToken MakeWs() { return MacroToken(PPToken{PPKind::Whitespace, ""}); }
MacroToken MakePunc(const string& s) { return MacroToken(PPToken{PPKind::PreprocessingOpOrPunc, s}); }
MacroToken MakeStringLit(const string& s) { return MacroToken(PPToken{PPKind::StringLiteral, s}); }
MacroToken MakePlacemarker() { MacroToken t; t.placemarker = true; return t; }

struct MacroDef
{
	bool function_like = false;
	bool variadic = false;
	vector<string> params;
	vector<MacroToken> replacement;
};

bool IsWs(const PPToken& pp) { return pp.kind == PPKind::Whitespace || pp.kind == PPKind::NewLine; }
bool IsWs(const MacroToken& mt) { return IsWs(mt.pp); }
bool IsId(const MacroToken& mt, const string& s) { return !mt.placemarker && mt.pp.kind == PPKind::Identifier && mt.pp.source == s; }

vector<MacroToken> ToMacroTokens(const vector<PPToken>& in, size_t a, size_t b)
{
	vector<MacroToken> out;
	for (size_t i = a; i < b; ++i) out.push_back(MacroToken(in[i]));
	return out;
}

vector<MacroToken> CollapseWhitespace(const vector<MacroToken>& in, bool newlines_to_space)
{
	vector<MacroToken> out;
	bool pending = false;
	for (const MacroToken& tok : in)
	{
		if (tok.placemarker) { out.push_back(tok); continue; }
		if (tok.pp.kind == PPKind::Whitespace || (newlines_to_space && tok.pp.kind == PPKind::NewLine))
		{
			pending = true;
			continue;
		}
		if (pending && !out.empty())
			out.push_back(MakeWs());
		pending = false;
		out.push_back(tok);
	}
	if (!out.empty() && IsWs(out.back()))
		out.pop_back();
	return out;
}

bool SameTokensForRedef(const vector<MacroToken>& a, const vector<MacroToken>& b)
{
	vector<MacroToken> ca = CollapseWhitespace(a, false);
	vector<MacroToken> cb = CollapseWhitespace(b, false);
	if (ca.size() != cb.size()) return false;
	for (size_t i = 0; i < ca.size(); ++i)
	{
		if (ca[i].placemarker != cb[i].placemarker) return false;
		if (ca[i].placemarker) continue;
		if (ca[i].pp.kind != cb[i].pp.kind || ca[i].pp.source != cb[i].pp.source) return false;
	}
	return true;
}

bool HasParam(const MacroDef& def, const string& name)
{
	return find(def.params.begin(), def.params.end(), name) != def.params.end() || (def.variadic && name == "__VA_ARGS__");
}

void ValidateReplacement(const MacroDef& def)
{
	vector<MacroToken> toks;
	for (const MacroToken& tok : def.replacement)
		if (!IsWs(tok))
			toks.push_back(tok);
	for (size_t i = 0; i < toks.size(); ++i)
	{
		if (!toks[i].placemarker && toks[i].pp.kind == PPKind::PreprocessingOpOrPunc && toks[i].pp.source == "#")
		{
			if (def.function_like && (i + 1 >= toks.size() || toks[i + 1].pp.kind != PPKind::Identifier || !HasParam(def, toks[i + 1].pp.source)))
				throw runtime_error("bad # in replacement");
		}
		if (!toks[i].placemarker && toks[i].pp.kind == PPKind::PreprocessingOpOrPunc && toks[i].pp.source == "##")
		{
			if (i == 0 || i + 1 == toks.size())
				throw runtime_error("bad ## in replacement");
		}
	}
}

struct Arg
{
	vector<MacroToken> raw;
	vector<MacroToken> expanded;
	bool expanded_ready = false;

	Arg() {}
	explicit Arg(const vector<MacroToken>& raw) : raw(raw) {}
};

string Stringize(const vector<MacroToken>& toks)
{
	string body;
	bool pending_space = false;
	bool emitted = false;
	for (size_t i = 0; i < toks.size(); ++i)
	{
		const MacroToken& tok = toks[i];
		if (tok.placemarker) continue;
		if (IsWs(tok))
		{
			if (emitted) pending_space = true;
			continue;
		}
		if (pending_space) body += ' ';
		pending_space = false;
		emitted = true;
		bool escape_all = tok.pp.kind == PPKind::StringLiteral || tok.pp.kind == PPKind::CharacterLiteral ||
			tok.pp.kind == PPKind::UserDefinedStringLiteral || tok.pp.kind == PPKind::UserDefinedCharacterLiteral;
		for (char ch : tok.pp.source)
		{
			if (ch == '"' || (escape_all && ch == '\\')) body += '\\';
			body += ch;
		}
	}
	return "\"" + body + "\"";
}

vector<MacroToken> RetokenizeSingle(const string& src)
{
	PPTokenizer tok;
	vector<PPToken> pp = tok.run(src);
	vector<MacroToken> out;
	for (const PPToken& t : pp)
		if (t.kind != PPKind::Whitespace && t.kind != PPKind::NewLine)
			out.push_back(MacroToken(t));
	return out;
}

vector<MacroToken> PasteTokens(const MacroToken& a, const MacroToken& b)
{
	if (a.placemarker && b.placemarker) return {};
	if (a.placemarker) return {b};
	if (b.placemarker) return {a};
	vector<MacroToken> joined = RetokenizeSingle(a.pp.source + b.pp.source);
	if (joined.empty()) return {};
	joined[0].hidden = a.hidden;
	joined[0].hidden.insert(b.hidden.begin(), b.hidden.end());
	joined[0].unavailable = false;
	return joined;
}

vector<MacroToken> EmitPostTokens(const vector<PPToken>& pp)
{
	vector<MacroToken> out;
	for (const PPToken& t : pp)
		out.push_back(MacroToken(t));
	return out;
}

void EmitFromPP(const vector<PPToken>& pp, DebugPostTokenOutputStream& output)
{
	for (size_t i = 0; i < pp.size(); ++i)
	{
		const PPToken& tok = pp[i];
		if (tok.kind == PPKind::Whitespace || tok.kind == PPKind::NewLine) continue;
		if (tok.kind == PPKind::HeaderName || tok.kind == PPKind::NonWhitespaceCharacter) { output.emit_invalid(tok.source); continue; }
		if (tok.kind == PPKind::Identifier)
		{
			auto it = StringToTokenTypeMap.find(tok.source);
			if (it != StringToTokenTypeMap.end()) output.emit_simple(tok.source, it->second);
			else output.emit_identifier(tok.source);
			continue;
		}
		if (tok.kind == PPKind::PreprocessingOpOrPunc)
		{
			if (tok.source == "#" || tok.source == "##" || tok.source == "%:" || tok.source == "%:%:") output.emit_invalid(tok.source);
			else output.emit_simple(tok.source, StringToTokenTypeMap.at(tok.source));
			continue;
		}
		if (tok.kind == PPKind::PPNumber)
		{
			string prefix, uds; EFundamentalType type; vector<unsigned char> bytes;
			if (looks_like_float(tok.source))
			{
				if (!parse_float_literal(tok.source, prefix, uds, type, bytes)) output.emit_invalid(tok.source);
				else if (!uds.empty()) output.emit_user_defined_literal_floating(tok.source, uds, prefix);
				else output.emit_literal(tok.source, type, bytes.data(), bytes.size());
			}
			else
			{
				if (!parse_integer_literal(tok.source, prefix, uds, type, bytes)) output.emit_invalid(tok.source);
				else if (!uds.empty()) output.emit_user_defined_literal_integer(tok.source, uds, prefix);
				else output.emit_literal(tok.source, type, bytes.data(), bytes.size());
			}
			continue;
		}
		if (tok.kind == PPKind::CharacterLiteral || tok.kind == PPKind::UserDefinedCharacterLiteral)
		{
			StringData data; string uds; bool is_char = false;
			if (!decode_string_or_char(tok.source, tok.kind == PPKind::UserDefinedCharacterLiteral, data, uds, is_char) || !is_char) output.emit_invalid(tok.source);
			else if (!uds.empty())
			{
				if (uds[0] != '_') output.emit_invalid(tok.source);
				else output.emit_user_defined_literal_character(tok.source, uds, data.type, data.bytes.data(), data.bytes.size());
			}
			else output.emit_literal(tok.source, data.type, data.bytes.data(), data.bytes.size());
			continue;
		}
		if (tok.kind == PPKind::StringLiteral || tok.kind == PPKind::UserDefinedStringLiteral)
		{
			vector<PPToken> group(1, tok);
			while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::Whitespace || pp[i + 1].kind == PPKind::NewLine)) ++i;
			while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::StringLiteral || pp[i + 1].kind == PPKind::UserDefinedStringLiteral))
			{
				group.push_back(pp[++i]);
				while (i + 1 < pp.size() && (pp[i + 1].kind == PPKind::Whitespace || pp[i + 1].kind == PPKind::NewLine)) ++i;
			}
			string joined;
			StringData combined;
			string final_ud;
			bool ok = true;
			bool saw_u8 = false;
			int target_kind = 0;
			vector<int> combined_cps;
			for (size_t gi = 0; gi < group.size(); ++gi)
			{
				if (gi) joined += " ";
				joined += group[gi].source;
				StringData part; string uds; bool is_char = false;
				if (!decode_string_or_char(group[gi].source, group[gi].kind == PPKind::UserDefinedStringLiteral, part, uds, is_char) || is_char) { ok = false; continue; }
				int kind = string_prefix_kind(group[gi].source);
				if (kind == 1) saw_u8 = true;
				if (kind >= 2)
				{
					if (target_kind == 0) target_kind = kind;
					else if (target_kind != kind) ok = false;
				}
				if (target_kind >= 2 && saw_u8) ok = false;
				if (gi == 0) combined.type = part.type;
				if (part.type != FT_CHAR && combined.type == FT_CHAR) combined.type = part.type;
				else if (part.type != FT_CHAR && combined.type != FT_CHAR && part.type != combined.type) { ok = false; continue; }
				if (!uds.empty())
				{
					if (uds[0] != '_') ok = false;
					else if (final_ud.empty()) final_ud = uds;
					else if (final_ud != uds) ok = false;
				}
				part.bytes.resize(part.bytes.size() - (part.type == FT_CHAR ? 1 : part.type == FT_CHAR16_T ? 2 : 4));
				vector<int> cps = bytes_to_codepoints(part.type, part.bytes);
				combined_cps.insert(combined_cps.end(), cps.begin(), cps.end());
				combined.elements += part.elements - 1;
			}
			if (!ok) { output.emit_invalid(joined); continue; }
			combined.bytes.clear();
			append_codepoints_as(combined.type, combined_cps, combined.bytes);
			if (combined.type == FT_CHAR) combined.bytes.push_back(0);
			else if (combined.type == FT_CHAR16_T) { char16_t z = 0; append_scalar(combined.bytes, z); }
			else { char32_t z = 0; append_scalar(combined.bytes, z); }
			combined.elements = combined.type == FT_CHAR ? combined.bytes.size() : combined.bytes.size() / (combined.type == FT_CHAR16_T ? 2 : 4);
			if (!final_ud.empty()) output.emit_user_defined_literal_string_array(joined, final_ud, combined.elements, combined.type, combined.bytes.data(), combined.bytes.size());
			else output.emit_literal_array(joined, combined.elements, combined.type, combined.bytes.data(), combined.bytes.size());
			continue;
		}
	}
}

struct MacroProcessor
{
	unordered_map<string, MacroDef> macros;

	size_t prev_non_ws(const vector<MacroToken>& v, size_t i) const
	{
		while (i > 0)
		{
			--i;
			if (!IsWs(v[i])) return i;
		}
		return v.size();
	}

	size_t next_non_ws(const vector<MacroToken>& v, size_t i) const
	{
		for (++i; i < v.size(); ++i)
			if (!IsWs(v[i])) return i;
		return v.size();
	}

	vector<MacroToken> expand_text(const vector<MacroToken>& input)
	{
		deque<MacroToken> pending;
		for (MacroToken t : CollapseWhitespace(input, true))
			pending.push_back(t);
		vector<MacroToken> out;
		while (!pending.empty())
		{
			MacroToken head = pending.front();
			pending.pop_front();
			if (!head.placemarker && head.pp.kind == PPKind::Identifier && head.pp.source == "__VA_ARGS__")
				throw runtime_error("__VA_ARGS__ outside variadic macro");
			if (head.placemarker || head.pp.kind != PPKind::Identifier || head.unavailable || (head.hidden.count(head.pp.source) == 0 && macros.find(head.pp.source) == macros.end()))
			{
				out.push_back(head);
				continue;
			}
			if (head.hidden.count(head.pp.source))
			{
				head.unavailable = true;
				out.push_back(head);
				continue;
			}
			const MacroDef& def = macros.at(head.pp.source);
			if (def.function_like)
			{
				size_t ws = 0;
				while (ws < pending.size() && IsWs(pending[ws])) ++ws;
				if (ws >= pending.size() || pending[ws].placemarker || pending[ws].pp.kind != PPKind::PreprocessingOpOrPunc || pending[ws].pp.source != "(")
				{
					out.push_back(head);
					continue;
				}
				while (ws--) pending.pop_front();
				pending.pop_front();
				vector<Arg> args;
				vector<MacroToken> cur;
				int depth = 0;
				bool saw_any = false;
				bool closed = false;
				while (!pending.empty())
				{
					MacroToken t = pending.front();
					pending.pop_front();
					if (!t.placemarker && t.pp.kind == PPKind::PreprocessingOpOrPunc && t.pp.source == "(") { ++depth; cur.push_back(t); saw_any = true; continue; }
					if (!t.placemarker && t.pp.kind == PPKind::PreprocessingOpOrPunc && t.pp.source == ")" && depth == 0)
					{
						closed = true;
						vector<MacroToken> collapsed = CollapseWhitespace(cur, true);
						if (!collapsed.empty() || !args.empty() || !def.params.empty() || def.variadic)
							args.push_back(Arg(collapsed));
						break;
					}
					if (!t.placemarker && t.pp.kind == PPKind::PreprocessingOpOrPunc && t.pp.source == ")" && depth > 0) { --depth; cur.push_back(t); saw_any = true; continue; }
					if (!t.placemarker && t.pp.kind == PPKind::PreprocessingOpOrPunc && t.pp.source == "," && depth == 0)
					{
						args.push_back(Arg(CollapseWhitespace(cur, true)));
						cur.clear();
						saw_any = true;
						continue;
					}
					cur.push_back(t);
					saw_any = true;
				}
				if (!closed) throw runtime_error("unterminated macro invocation");
				if (!def.variadic)
				{
					if (args.size() != def.params.size()) throw runtime_error("wrong number of macro arguments");
				}
				else
				{
					if (args.size() < def.params.size()) throw runtime_error("wrong number of macro arguments");
				}
				vector<MacroToken> repl = instantiate(def, head, args);
				for (auto it = repl.rbegin(); it != repl.rend(); ++it) pending.push_front(*it);
			}
			else
			{
				vector<Arg> none;
				vector<MacroToken> repl = instantiate(def, head, none);
				for (auto it = repl.rbegin(); it != repl.rend(); ++it) pending.push_front(*it);
			}
		}
		return out;
	}

	vector<MacroToken> get_arg_tokens(const MacroDef& def, vector<Arg>& args, const string& name, bool expanded)
	{
		for (size_t i = 0; i < def.params.size(); ++i)
			if (def.params[i] == name)
			{
				if (expanded && !args[i].expanded_ready)
				{
					args[i].expanded = expand_text(args[i].raw);
					args[i].expanded_ready = true;
				}
				return expanded ? args[i].expanded : args[i].raw;
			}
		if (def.variadic && name == "__VA_ARGS__")
		{
			vector<MacroToken> out;
			for (size_t i = def.params.size(); i < args.size(); ++i)
			{
				if (expanded && !args[i].expanded_ready)
				{
					args[i].expanded = expand_text(args[i].raw);
					args[i].expanded_ready = true;
				}
				if (!out.empty())
				{
					out.push_back(MakePunc(","));
					out.push_back(MakeWs());
				}
				const vector<MacroToken>& src = expanded ? args[i].expanded : args[i].raw;
				out.insert(out.end(), src.begin(), src.end());
			}
			return out;
		}
		return {};
	}

	vector<MacroToken> instantiate(const MacroDef& def, const MacroToken& head, vector<Arg>& args)
	{
		vector<MacroToken> sub;
		for (size_t i = 0; i < def.replacement.size(); ++i)
		{
			const MacroToken& tok = def.replacement[i];
			if (IsWs(tok)) { sub.push_back(tok); continue; }
			if (def.function_like && !tok.placemarker && tok.pp.kind == PPKind::PreprocessingOpOrPunc && tok.pp.source == "#")
			{
				size_t ni = next_non_ws(def.replacement, i);
				if (ni >= def.replacement.size() || def.replacement[ni].pp.kind != PPKind::Identifier) throw runtime_error("bad # in replacement");
				string name = def.replacement[ni].pp.source;
				i = ni;
				vector<MacroToken> raw = get_arg_tokens(def, args, name, false);
				sub.push_back(MakeStringLit(Stringize(raw)));
				continue;
			}
			if (!tok.placemarker && tok.pp.kind == PPKind::Identifier)
			{
				size_t pi = prev_non_ws(def.replacement, i);
				size_t ni = next_non_ws(def.replacement, i);
				bool left_hashhash = pi < def.replacement.size() && !def.replacement[pi].placemarker && def.replacement[pi].pp.kind == PPKind::PreprocessingOpOrPunc && def.replacement[pi].pp.source == "##";
				bool right_hashhash = ni < def.replacement.size() && !def.replacement[ni].placemarker && def.replacement[ni].pp.kind == PPKind::PreprocessingOpOrPunc && def.replacement[ni].pp.source == "##";
				vector<MacroToken> rep = get_arg_tokens(def, args, tok.pp.source, !(left_hashhash || right_hashhash));
				if (!rep.empty() || tok.pp.source == "__VA_ARGS__" || find(def.params.begin(), def.params.end(), tok.pp.source) != def.params.end())
				{
					if (rep.empty()) sub.push_back(MakePlacemarker());
					else sub.insert(sub.end(), rep.begin(), rep.end());
					continue;
				}
			}
			MacroToken copy = tok;
			copy.op_hashhash = (!copy.placemarker && copy.pp.kind == PPKind::PreprocessingOpOrPunc && copy.pp.source == "##");
			sub.push_back(copy);
		}
		vector<MacroToken> pasted;
		for (size_t i = 0; i < sub.size(); ++i)
		{
			if (IsWs(sub[i]))
			{
				size_t pi = prev_non_ws(sub, i);
				size_t ni = next_non_ws(sub, i);
				bool around_hashhash = (pi < sub.size() && sub[pi].op_hashhash) || (ni < sub.size() && sub[ni].op_hashhash);
				if (!around_hashhash) pasted.push_back(sub[i]);
				continue;
			}
			if (sub[i].op_hashhash)
			{
				if (i + 1 >= sub.size()) throw runtime_error("bad ## in replacement");
				while (!pasted.empty() && IsWs(pasted.back())) pasted.pop_back();
				MacroToken left = pasted.empty() ? MakePlacemarker() : pasted.back();
				if (!pasted.empty()) pasted.pop_back();
				size_t ni = next_non_ws(sub, i);
				if (ni >= sub.size()) throw runtime_error("bad ## in replacement");
				i = ni;
				vector<MacroToken> tmp = PasteTokens(left, sub[i]);
				pasted.insert(pasted.end(), tmp.begin(), tmp.end());
			}
			else pasted.push_back(sub[i]);
		}
		for (MacroToken& tok : pasted)
		{
			tok.hidden = head.hidden;
			tok.hidden.insert(head.pp.source);
			if (!tok.placemarker && tok.pp.kind == PPKind::Identifier && tok.hidden.count(tok.pp.source))
				tok.unavailable = true;
		}
		vector<MacroToken> out;
		for (MacroToken& tok : pasted) if (!tok.placemarker) out.push_back(tok);
		return out;
	}

	void define_macro(const string& name, const MacroDef& def)
	{
		auto it = macros.find(name);
		if (it != macros.end())
		{
			const MacroDef& old = it->second;
			if (old.function_like != def.function_like || old.variadic != def.variadic || old.params != def.params || !SameTokensForRedef(old.replacement, def.replacement))
				throw runtime_error("macro redefined");
			return;
		}
		macros[name] = def;
	}
};

#ifndef CPPGM_MACRO_EMBED
int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		PPTokenizer tokenizer;
		vector<PPToken> toks = tokenizer.run(oss.str());
		MacroProcessor proc;
		vector<PPToken> final_pp;
		size_t i = 0;
		while (i < toks.size())
		{
			size_t line_start = i;
			bool at_directive = (i == 0 || toks[i - 1].kind == PPKind::NewLine);
			if (at_directive)
			{
				size_t j = i;
				while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
				if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && (toks[j].source == "#" || toks[j].source == "%:"))
				{
					++j;
					while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
					if (j >= toks.size() || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
					string directive = toks[j++].source;
					if (directive == "define")
					{
						while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
						if (j >= toks.size() || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
						string name = toks[j++].source;
						if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
						MacroDef def;
						if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "(")
						{
							def.function_like = true;
							++j;
							bool need_param = true;
							while (j < toks.size())
							{
								while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
								if (j >= toks.size()) throw runtime_error("unterminated define");
								if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
								if (toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
								{
									def.variadic = true;
									++j;
									while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
									if (j >= toks.size() || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
									++j;
									break;
								}
								if (toks[j].kind != PPKind::Identifier) throw runtime_error("bad parameter");
								string param = toks[j++].source;
								if (param == "__VA_ARGS__") throw runtime_error("bad variadic macro");
								if (find(def.params.begin(), def.params.end(), param) != def.params.end()) throw runtime_error("duplicate parameter " + param + " in macro definition");
								def.params.push_back(param);
								while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
								if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ",")
								{
									++j;
									while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
									if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == "...")
									{
										def.variadic = true;
										++j;
										while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
										if (j >= toks.size() || toks[j].kind != PPKind::PreprocessingOpOrPunc || toks[j].source != ")") throw runtime_error("bad variadic macro");
										++j;
										break;
									}
									continue;
								}
								if (j < toks.size() && toks[j].kind == PPKind::PreprocessingOpOrPunc && toks[j].source == ")") { ++j; break; }
								throw runtime_error("bad parameter list");
							}
						}
						else if (j < toks.size() && toks[j].kind == PPKind::Whitespace)
						{
							while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
						}
						size_t end = j;
						while (end < toks.size() && toks[end].kind != PPKind::NewLine) ++end;
						def.replacement = ToMacroTokens(toks, j, end);
						if (!def.variadic)
						{
							for (const MacroToken& tok : def.replacement)
								if (!tok.placemarker && tok.pp.kind == PPKind::Identifier && tok.pp.source == "__VA_ARGS__")
									throw runtime_error("__VA_ARGS__ outside variadic macro");
						}
						ValidateReplacement(def);
						proc.define_macro(name, def);
						i = end + (end < toks.size());
						continue;
					}
					if (directive == "undef")
					{
						while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
						if (j >= toks.size() || toks[j].kind != PPKind::Identifier) throw runtime_error("identifier missing");
						string name = toks[j++].source;
						if (name == "__VA_ARGS__") throw runtime_error("bad variadic macro");
						while (j < toks.size() && toks[j].kind == PPKind::Whitespace) ++j;
						if (j < toks.size() && toks[j].kind != PPKind::NewLine) throw runtime_error("extra tokens after undef");
						proc.macros.erase(name);
						i = j + (j < toks.size());
						continue;
					}
				}
			}
			size_t end = i;
			while (end < toks.size())
			{
				if (end == i ? false : toks[end - 1].kind == PPKind::NewLine)
				{
					size_t k = end;
					while (k < toks.size() && toks[k].kind == PPKind::Whitespace) ++k;
					if (k < toks.size() && toks[k].kind == PPKind::PreprocessingOpOrPunc && (toks[k].source == "#" || toks[k].source == "%:"))
						break;
				}
				++end;
			}
			vector<MacroToken> text = ToMacroTokens(toks, i, end);
			vector<MacroToken> expanded = proc.expand_text(text);
			for (const MacroToken& t : expanded)
				final_pp.push_back(t.pp);
			i = end;
		}

		DebugPostTokenOutputStream output;
		EmitFromPP(final_pp, output);
		output.emit_eof();
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
#endif
