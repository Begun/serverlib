#ifndef __HTTPD_UNPARSE_H
#define __HTTPD_UNPARSE_H

#include <vector>
#include <string>
#include <map>
#include "request.h"

namespace httpd {

inline void quote_char(unsigned char c, std::string& ret) {
    static const char* __table = "0123456789ABCDEF";

    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        (c == '-' || c == '_' || c == '.' || c == '~')) {

        ret += c;

    } else if (c == ' ') {
        ret += '+';

    } else {

        ret += '%';
        ret += __table[(c >> 4) & 0xF];
        ret += __table[c & 0xF];
    }
}


inline std::string quote(const std::string& s) {

    std::string ret;

    for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
	unsigned char c = *i;
        quote_char(c, ret);
    }

    return ret;
}


inline void unparse_query(const std::map<std::string,std::vector<std::string> >& q, std::string& out) {

    for (std::map<std::string,std::vector<std::string> >::const_iterator i = q.begin(); 
	 i != q.end(); ++i) {
		
	for (std::vector<std::string>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
		
	    if (i == q.begin()) {
		out += '?';
	    } else {
		out += '&';
	    }
	    
	    out += quote(i->first);
	    out += '=';
	    out += quote(*j);
	}
    }
}


inline void unparse_fields(const std::map<std::string,std::vector<std::string> >& q, std::string& out) {

    for (std::map<std::string,std::vector<std::string> >::const_iterator i = q.begin();
	 i != q.end(); ++i) {

	for (std::vector<std::string>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {

	    out += i->first;
	    out += ": ";
	    out += *j;
	    out += "\r\n";
	}
    }

    out += "\r\n";
}




inline void unparse_request(const request& req, std::string& req_str, const std::string& poison = "") {

    req_str += req.method;
    req_str += ' ';
    req_str += req.path;


    if (req.query_raw.size() > 0) {
	req_str += '?';
	req_str += req.query_raw;

    } else {

	unparse_query(req.queries, req_str);
    }

    req_str += poison;

    req_str += ' ';
    req_str += req.version;
    req_str += "\r\n";

    
    if (req.fields_raw.size() > 0) {
	req_str += req.fields_raw;

    } else {

	unparse_fields(req.fields, req_str);
    }
}


inline std::string unparse_request(const request& req) {
    std::string req_str;
    unparse_request(req, req_str);
    return req_str;
}


} // namespace httpd

#endif
