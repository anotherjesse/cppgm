// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace std;

#define CPPGM_PPTOKEN_LIBRARY
#define main CPPGM_POSTTOKEN_MAIN
#include "posttoken.cpp"
#undef main

namespace
{
	struct Line
	{
		vector<string> tokens;
		bool is_directive = false;
	};

	bool is_ws_kind(Kind k)
	{
		return k == Kind::WS;
	}

	bool is_nl_kind(Kind k)
	{
		return k == Kind::NL || k == Kind::EOFK;
	}

	Line collect_line(const vector<Tok>& toks, size_t begin, size_t end)
	{
		Line line;
		size_t i = begin;
		while (i < end && is_ws_kind(toks[i].kind))
			++i;
		if (i < end && toks[i].kind == Kind::PUNC && toks[i].data == "#")
			line.is_directive = true;
		for (size_t j = begin; j < end; ++j)
		{
			if (!is_ws_kind(toks[j].kind))
				line.tokens.push_back(toks[j].data);
		}
		return line;
	}

	string serialize_source(const vector<Line>& lines)
	{
		string out;
		for (size_t i = 0; i < lines.size(); ++i)
		{
			if (lines[i].is_directive || lines[i].tokens.empty())
				continue;
			if (!out.empty())
				out.push_back('\n');
			for (size_t j = 0; j < lines[i].tokens.size(); ++j)
			{
				if (j)
					out.push_back(' ');
				out += lines[i].tokens[j];
			}
		}
		return out;
	}
}

int main()
{
	try
	{
		ostringstream oss;
		oss << cin.rdbuf();
		string input = oss.str();

		Sink sink;
		PPTokenizer tokenizer(sink);
		for (size_t i = 0; i < input.size(); ++i)
			tokenizer.process(static_cast<unsigned char>(input[i]));
		tokenizer.process(EndOfFile);

		vector<Line> lines;
		for (size_t i = 0; i < sink.toks.size(); )
		{
			size_t j = i;
			while (j < sink.toks.size() && !is_nl_kind(sink.toks[j].kind))
				++j;
			lines.push_back(collect_line(sink.toks, i, j));
			if (j < sink.toks.size() && sink.toks[j].kind == Kind::EOFK)
				break;
			i = j + 1;
		}

		string expanded_source = serialize_source(lines);
		istringstream iss(expanded_source);
		streambuf* old = cin.rdbuf(iss.rdbuf());
		int rc = CPPGM_POSTTOKEN_MAIN();
		cin.rdbuf(old);
		return rc;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
