// tlssh/src/util.cc
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include<wordexp.h>

#include"util.h"
#include"xgetpwnam.h"

std::string
xwordexp(const std::string &in)
{
	wordexp_t p;
	char **w;
	int i;

	if (wordexp(in.c_str(), &p, 0)) {
		throw "FIXME: wordexp()";
	}

	if (p.we_wordc != 1) {
		throw "FIXME: wordexp() nmatch != 1";
	}

	std::string ret(p.we_wordv[0]);
	wordfree(&p);
	return ret;
}

/**
 * FIXME: handle doublequotes
 */
std::vector<std::string>
tokenize(const std::string &s)
{
	std::vector<std::string> ret;
	int end;
	int start = 0;

	for (;;) {
		// find beginning of word
		start = s.find_first_not_of(" \t", start);
		if (std::string::npos == start) {
			return ret;
		}

		// find end of word
		end = s.find_first_of(" \t", start);
		if (std::string::npos == end) {
			ret.push_back(s.substr(start));
			break;
		}
		ret.push_back(trim(s.substr(start, end-start)));
		start = end;
	}
	return ret;
}

std::string
trim(const std::string &str)
{
	size_t startpos = str.find_first_not_of(" \t");
	if (std::string::npos == startpos) {
		return "";
	}

	size_t endpos = str.find_last_not_of(" \t");

	return str.substr(startpos, endpos-startpos+1);
}

/**
 *
 */
struct passwd
xgetpwnam(const std::string &name, std::vector<char> &buffer)
{
	buffer.reserve(1024);
	struct passwd pw;
	struct passwd *ppw = 0;
	if (xgetpwnam_r(name.c_str(), &pw, &buffer[0], buffer.capacity(), &ppw)
	    || !ppw) {
		throw "FIXME";
	}

	return pw;
}


/* ---- Emacs Variables ----
 * Local Variables:
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
