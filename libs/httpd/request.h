#ifndef __HTTPD_REQUEST_H
#define __HTTPD_REQUEST_H

#include <algorithm>

namespace httpd {


static void parse_fields_inline(const std::string& s, std::map<std::string,std::string>& out) {

    enum state_ { KEY, VAL } state = KEY;

    std::string key;
    std::string val;

    for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {

	if (state == KEY) {

	    if (*i == ' ' || *i == '\t' || *i == '\n') {
		// Nothing.

	    } else if (*i == '=') {
		state = VAL;

	    } else {
		key += *i;
	    }

	} else {

	    if (*i == ';') {
		state = KEY;

		if (key.size() != 0) {
		    out[key] = val;
		    key.clear();
		    val.clear();
		}

	    } else {
		val += *i;
	    }
	}
    }

    if (key.size() != 0) {
	out[key] = val;
    }
}



inline std::string unparse_fields_inline(const std::map<std::string,std::string>& f) {

    std::string ret;

    for (std::map<std::string,std::string>::const_iterator i = f.begin(); i != f.end(); ++i) {

	if (i != f.begin()) {
	    ret += "; ";
	}

	ret += i->first;
	ret += '=';
	ret += i->second;
    }

    return ret;
}



struct request {
    std::string method;
    std::string path;
    std::string version;

    std::string query_raw;
    std::string fields_raw;

    typedef std::map<std::string,std::vector<std::string> > queries_;
    queries_ queries;

    std::map<std::string,std::vector<std::string> > fields;

    std::string empty;

    request(const std::string& host = "example.com") : method("GET"), version("HTTP/1.1")
    {
        fields["host"].push_back(host);
    }

    const std::string& get_query(const std::string& f) const {
	std::map<std::string,std::vector<std::string> >::const_iterator i = queries.find(f);
	if (i != queries.end() && i->second.size() > 0) return i->second[0];
	return empty;
    }

    const size_t get_query(const std::string& f, std::vector<std::string>& values) const
    {
        queries_::const_iterator i = queries.find(f);
        if (i != queries.end() && i->second.size() > 0) {
            values = i->second;
            return values.size();
        }
        return 0;
    }

    void set_query(const std::string& k, const std::string& v) {
	query_raw.clear();
	queries[k].push_back(v);
    }

    void reset_query(const std::string& k, const std::string& v) {
	query_raw.clear();
	std::vector<std::string>& tmp = queries[k];

	if (tmp.size() == 0) {
	    tmp.push_back(v);
	} else if (tmp.size() == 1) {
	    tmp[0] = v;
	} else {
	    tmp.clear();
	    tmp.push_back(v);
	}
    }

    const std::string& get_field(const std::string& f) const {
	std::map<std::string,std::vector<std::string> >::const_iterator i = fields.find(f);
	if (i != fields.end() && i->second.size() > 0) return i->second[0];
	return empty;
    }

    void set_field(const std::string& k, const std::string& v) {
        std::string key;
        std::transform(k.begin(), k.end(), std::back_inserter(key), ::tolower );

	fields_raw.clear();
        fields[key].push_back(v);
    }

    void get_cookies(std::map<std::string,std::string>& out) const {

	out.clear();

	std::map<std::string,std::vector<std::string> >::const_iterator i = fields.find("cookie");

	if (i == fields.end()) return;

	for (std::vector<std::string>::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
	    parse_fields_inline(*j, out);
	}
    }

    void set_cookies(const std::map<std::string,std::string>& c) {
	set_field("cookie", unparse_fields_inline(c));
    }

};


}


#endif

