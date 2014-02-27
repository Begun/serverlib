#ifndef __HTTPD_RESPONSE_H
#define __HTTPD_RESPONSE_H


#include "util/webtime.h"
#include "httpd/unparse.h"
#include "httpd/request.h"
#include "files/files_format.h"
#include "files/serialization_save.h"
#include "clientserver/clientserver.h"

#include <string>
#include <vector>
#include <map>


namespace httpd {


namespace response {



struct headers {

    headers(const std::string& c = "200 OK") : 
        version("HTTP/1.1"), 
        code(c)
        {}

    headers(const request& r, const std::string& c = "200 OK") : 
        version(r.version),
        code(c) 
        {}

    void set_field(const std::string& k, const std::string& v) {
	std::vector<std::string>& tmp = fields[k];

	if (tmp.size() > 0) {
	    tmp[0] = v;
	} else {
	    tmp.push_back(v);
	}
    }

    void set_server(const std::string& s) { 
	set_field("server", s);
    }

    void set_date(time_t t) { 
	set_field("date", util::webtime(t));
    }

    void set_content_type(const std::string& ct, const std::string& charset = "Windows-1251") {
	if (charset.size() > 0) {
	    set_field("content-type", ct + "; charset=" + charset);
	} else {
	    set_field("content-type", ct);
	}
    }

    void set_keep_alive(bool ka) {
	set_field("connection", (ka ? "Keep-Alive" : "close"));
    }

    void set_expires(time_t t) {
	set_field("expires", util::webtime(t));
    }

    void set_last_modified(time_t t) {
	set_field("last-modified", util::webtime(t));
    }

    template <typename T>
    void set_cookie(const std::string& k, const T& v, 
		    time_t expires,
		    const std::string& domain=".example.com",
		    const std::string& path="/") {

	files::fmt ff;
	ff << k << "=" << v << "; domain=" << domain << "; path=" << path << "; expires=" << util::webtime(expires);
	fields["set-cookie"].push_back(ff.data);
    }

    void set_uncached() {
	fields["cache-control"].push_back("no-store, no-cache, must-revalidate");
	fields["pragma"].push_back("no-cache");
    }

    void set_content_length(size_t s) {
	files::fmt ff;
	ff << s;
	set_field("content-length", ff.data);
    }


    void headers_string(std::string& out) const {
        out = version;
        out += ' ';
        out += code;
        out += "\r\n";
	unparse_fields(fields, out);
    }



    std::string version;
    std::string code;
    std::map<std::string,std::vector<std::string> > fields;
};


}


struct responder : public response::headers, public files::fmt {

    clientserver::service_buffer sock;

    responder(clientserver::service_buffer s) : 
        sock(s), 
        should_close(false),
        sent(false)
        {}

    responder(clientserver::service_buffer s, const request& r) : 
        response::headers(r),
        sock(s),
        sent(false)
        {

            if (r.version == "HTTP/1.0" || r.get_field("connection") == "close") {
                should_close = true;
            } else {
                should_close = false;
            }
        }

    void send() {
	if (!sent) {
	    set_content_length(data.size());

	    std::string tmp;
	    headers_string(tmp);

	    sock << tmp;
	    sock << data;
            data.clear ();
            sent = true;
	}
    }

    void set_body(const std::string& b) {
	data = b;
    }

    ~responder() {
        try {
        	send();

        // Игноририем ошибки отправки
        } catch (...) { }
    }

    bool should_close;
    bool sent;
};




//
// Нестандартные способы ответа.
// Для внутреннего общения 

namespace response {


template <typename T>
void response_serialized(const T& payload, std::string& out) {

    serialization::saver_string_policy buf0;
    serialization::save(buf0, payload);

    out = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/plain\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: ";

    files::format(out, buf0.data.size());
    out += "\r\n\r\n";
    out += buf0.data;
}

inline void response_text(const std::string& t, std::string& out) {

    out = 
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/plain\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: Keep-Alive\r\n"
	"Content-Length: ";

    files::format(out, t.size());
    out += "\r\n\r\n";
    out += t;
}

template <typename T>
void response_error(const T& err, std::string& out, const std::string& error_code = "404 Not Found") {
    out = std::string("HTTP/1.1 ") + error_code + "\r\n"
	"Content-Type: text/plain\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: close\r\n"
	"Content-Length: ";

    files::format(out, ::strlen(err.what()));
    out += "\r\n\r\n";
    out += err.what();
}


inline void response_error_text(const std::string& err, std::string& out, const std::string& error_code = "404 Not Found") {
    out = std::string("HTTP/1.1 ") + error_code + "\r\n"
	"Content-Type: text/plain\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: close\r\n"
	"Content-Length: ";

    files::format(out, err.size());
    out += "\r\n\r\n";
    out += err;
}

}


}


#endif
