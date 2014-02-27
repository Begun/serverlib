#ifndef __HTTPD_PARSE_H
#define __HTTPD_PARSE_H

#include <string>
#include <strings.h>

#include "httpd/request.h"

namespace httpd {





inline void split_comma_string(const std::string& s, std::vector<std::string>& out) {

    std::string::const_iterator b = s.begin();
    std::string::const_iterator e = s.end();
    std::string::const_iterator b_prev = s.begin();

    while (b != e) {

	if (*b == ',') {
	    std::string::const_iterator b1 = b;

	    while (b_prev != b1 && *b_prev == ' ') ++b_prev;
	    while (b_prev != b1 && *(b1-1) == ' ') --b1;
	    
	    out.push_back(std::string(b_prev, b1));

	    ++b;
	    b_prev = b;

	    if (b == e) break;
	}

	++b;
    }

    while (b_prev != e && *b_prev == ' ') ++b_prev;
    std::string::const_iterator b1 = e;
    while (b_prev != b1 && *(b1-1) == ' ') --b1;

    out.push_back(std::string(b_prev, b1));
}


inline unsigned char unquote_quoted_printable_char(unsigned char c1, unsigned char c2) {
    c1 = ::tolower(c1);
    c2 = ::tolower(c2);

    if (c1 >= '0' && c1 <= '9') c1 = c1 - '0';
    else if (c1 >= 'a' && c1 <= 'f') c1 = (c1- 'a') + 10;
    else c1 = 0;

    if (c2 >= '0' && c2 <= '9') c2 = c2 - '0';
    else if (c2 >= 'a' && c2 <= 'f') c2 = (c2 - 'a') + 10;
    else c2 = 0;

    return (c1 << 4) | c2;
}



inline void unquote(const std::string& s, std::string& out) {

    for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
	unsigned char c = *i;

	if (c == '%') {
	    ++i;
	    if (i == s.end()) return;
	    unsigned char c1 = *i;

	    ++i;
	    if (i == s.end()) return;
	    unsigned char c2 = *i;

	    out += unquote_quoted_printable_char(c1, c2);

	} else if (c == '+') {
	    out += ' ';

	} else {
	    out += c;
	}
    }
}

inline std::string unquote(const std::string& s) {
    std::string ret;
    unquote(s, ret);
    return ret;
}


inline std::string get_domain_from_url(const std::string& url) {

    size_t i = 0;


    if (url.size() > 7 && ::strncasecmp(url.c_str(), "http://", 7) == 0) {
        i = 7;
    } else if (url.size() > 8 && ::strncasecmp(url.c_str(), "https://", 8) == 0) {
        i = 8;
    }

    size_t l = 0;
    for (size_t j = i; url[j] != '\0'; ++j, ++l) {
        if (url[j] == '/' || url[j] == '?') break;
    }

    return url.substr(i, l);
}

template <typename BUF>
inline void parse_query(BUF sock, request& out, int nlen = -1) {

    int n = 0;

    out.queries.clear();

    unsigned char c;
    enum state_ { KEY, VAL } state = KEY;

    std::string key;
    std::string val;

    while (1) {

        if (nlen > 0 && n >= nlen) break;

	sock >> c;
        n++;

	if (c == ' ') break;

	out.query_raw += c;

	if (c == '=') {
	    state = VAL;
	    continue;
	}

	if (c == '&') {
            if (key.size() > 0) {
		out.queries[key].push_back(val);
	    }
	    key.clear();
	    val.clear();
	    state = KEY;
	    continue;
	}

	if (c == '%') {
	    unsigned char c1;
	    unsigned char c2;

	    sock >> c1;
            n++;
            if (c1 == ' ' || (nlen > 0 && n >= nlen)) break;

	    sock >> c2;
            n++;
            if (c2 == ' ') break;

	    c = unquote_quoted_printable_char(c1, c2);

	    out.query_raw += c1;
	    out.query_raw += c2;

	} else if (c == '+') {
	    c = ' ';
	}

	if (state == KEY) key += c;
	else if (state == VAL) val += c;
    }

    if (key.size() > 0 && state == VAL) {
	out.queries[key].push_back(val);
    }
}


template <typename BUF>
inline void parse_request_line(BUF sock, request& out) {
    out.version.clear();
    out.method.clear();
    out.path.clear();

    unsigned char c;
    enum state_ { MET, URI, VER } state = MET;

    // Прощаем некорректно сформированные запросы.
    // Если есть мусорные буквы в несущественных местах, то будем их пропускать.

    while (1) {
	sock >> c;

	if (c == ' ') {
	    if (state == MET) state = URI;
	    else if (state == URI) state = VER;
	    continue;
	}

	if (c == '\r') continue;
	if (c == '\n') return;

	if (state == MET) out.method += ::toupper(c);
	else if (state == VER) out.version += ::toupper(c);
	else if (state == URI) {

	    if (c == '?') {
		parse_query<BUF>(sock, out);
		state = VER;
	    } else {
		out.path += c;
	    }
	}
    }
}
    

/* Эта функция используется и для парсинга на клиентской стороне тоже; поэтому она шаблонная. */

template <typename BUF>
inline void parse_request_fields(BUF sock, request& out) {
    out.fields.clear();

    unsigned char c;

    std::string key_prev;

    while (1) {
	std::string key;
	std::string value;
	size_t num = 0;

	enum state_ { INIT, KEY, VAL, VAL_SP, COLON_SP } state = INIT;

	while (1) {
	    sock >> c;

	    if (state == INIT) {
		if (c == ' ') {
		    state = VAL_SP;
		} else {
		    state = KEY;
		}
	    }

	    out.fields_raw += c;

	    if (c == '\r') continue;
	    if (c == '\n') break;

	    num++;

	    if (c == ':' && state == KEY) {
		state = COLON_SP;
		continue;
	    }

	    if (c == ' ' && state == VAL) {
		state = VAL_SP;
		continue;
	    }

	    if (state == KEY) {
		key += ::tolower(c);

	    } else if (state == VAL) {
		value += c;

	    } else if (state == VAL_SP) {
		if (c != ' ') { 
		    value += ' ';
		    value += c;
		    state = VAL;
		}

	    } else if (state == COLON_SP) {
		if (c != ' ') {
		    value += c;
		    state = VAL;
		}
	    }
	}

	// Пустая строка.
	if (num == 0) break;

	if (key.size() == 0) {
            std::vector<std::string>& f = out.fields[key_prev];

            if (f.size() == 0)
                throw std::runtime_error("malformed http request fields");

	    f.back() += value;

	} else {
	    out.fields[key].push_back(value);
	    key_prev = key;
	}
    }

    // Распарсим то, что перечислено через запятую.

    for (std::map<std::string,std::vector<std::string> >::iterator i = out.fields.begin();
	 i != out.fields.end(); ++i) {

        // Убогий формат дат конфликтует со стандартом в части поддержки списков через запятую
        if (i->first == "date" || 
            i->first == "if-modified-since" ||
            i->first == "if-unmodified-since" ||
            i->first == "if-range" ||
            i->first == "set-cookie" ||
            i->first == "expires" ||
            i->first == "last-modified" ||
            i->first == "user-agent") { // В некоторых user-agent бывают запятые

            continue;
        }

	std::vector<std::string> tmp;

	tmp.swap(i->second);

	for (std::vector<std::string>::const_iterator j = tmp.begin(); j != tmp.end(); ++j) {
	    std::vector<std::string> tmp2;

	    split_comma_string(*j, tmp2);

	    i->second.insert(i->second.end(), tmp2.begin(), tmp2.end());
	}
    }
    
}
    

template <typename BUF>
inline void parse_request(BUF sock, request& out) {

    parse_request_line<BUF>(sock, out);
    parse_request_fields<BUF>(sock, out);
}


}


#endif
