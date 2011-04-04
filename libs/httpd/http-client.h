#ifndef __HTTP_CLIENT_H
#define __HTTP_CLIENT_H

#include <sys/epoll.h>
#include <errno.h>

#include "files/logger.h"
#include "files/serialization.h"
#include "lockfree/aux_.h"

#include "addresses.h"
#include "httpd.h"



namespace httpd {

struct response_code_error: public std::runtime_error {
    response_code_error(const std::string& head, const std::string &code="200")
        : std::runtime_error("Error in HTTP response. Expected code is \"" + code + "\", got head: \"" + head + "\"")
    {}
};

class client {

protected:

    clientserver::client_buffer m_sock;


public:

    std::string m_host;
    int m_port;

    client(const std::string& host, int port, int rtimeout = 0, int stimeout = 0) : 
        m_sock(clientserver::connect(host, port, rtimeout, stimeout)),
        m_host(host),
        m_port(port) {}

    typedef std::map<std::string,std::vector<std::string> > fields_t;

protected:

    client(bool) : m_port(0) {}

    void parse_response(std::string& head, fields_t& fields) {

	head.clear();

	while (1) {
	    unsigned char c;
	    m_sock >> c;

	    if (c == '\r') continue;
	    if (c == '\n') break;

	    head += c;
	}

	request tmp;
	parse_request_fields(m_sock, tmp);
	fields.swap(tmp.fields);
    }

    void parse_response(std::string& head, fields_t& fields, std::string& body) {
	// TODO: content-size, chunked !
	try {

	    parse_response(head, fields);

	    int len = -1;

	    fields_t::const_iterator i = fields.find("content-length");

	    if (i != fields.end() && i->second.size() > 0) {
		len = ::atoi(i->second.front().c_str());
	    }

	    while (len != 0) {
		unsigned char c;
		m_sock >> c;
		body += c;

		if (len > 0) len--;
	    }

	} catch (clientserver::eof_exception& e) {
	    return;
	}
    }

public:

    std::string send(const request& req, std::string& head, fields_t& fields, std::string& body) {

	std::string req_str;
	unparse_request(req, req_str);

        send(req_str, head, fields, body);
        return req_str;
    }


    void send(const std::string& req_str, std::string& head, fields_t& fields, std::string& body) {

	m_sock << req_str;

        parse_response(head, fields, body);
    }
    
    static void check_reply_head(const std::string& head, const std::string& code = "200") {
        if (head.compare(0, 5, "HTTP/", 5) != 0 ||
            head.compare(9, 3, code.c_str(), 3) != 0) {
            throw response_code_error(head, code);
        }
    }
};


/*****************************/


template <typename CLIENT>
struct persistent_client_ : public CLIENT {

public:

    using CLIENT::m_host;
    using CLIENT::m_port;

    using CLIENT::m_sock;

    persistent_client_() : CLIENT(false) {}

    persistent_client_(const std::string& h, int p, int rtimeout = 0, int stimeout = 0) : 
        CLIENT(h, p, rtimeout, stimeout) {}


    operator bool() const { return (bool)m_sock; }
    bool operator!() const { return !m_sock; }
};

typedef persistent_client_<client> persistent_client;



/*****************************/

class post_client : protected client {

public:

    using client::m_host;
    using client::m_port;

    post_client(const std::string& host, int port, const request& req, int rtimeout=0, int stimeout=0) : 
        client(host, port, rtimeout, stimeout) {

        std::string request_head;
	unparse_request(req, request_head);
        m_sock << request_head;
    }

    post_client(const std::string& host, int port, const std::string& req, int rtimeout=0, int stimeout=0) : 
        client(host, port, rtimeout, stimeout) {

        m_sock << req;
    }

    post_client& operator<<(const std::string& s) {
        m_sock << s;
        return (*this);
    }

    void send(std::string& head, fields_t& fields, std::string& body) {
        parse_response(head, fields, body);
    }
};


class chunked_post_client : protected client {

    void write_header(std::string& s) {
        if (s.size() > 2) {
            s.erase(s.end()-1);
            s.erase(s.end()-1);
        }

        s += "Transfer-Encoding: chunked\r\n\r\n";
        m_sock << s;
    }


public:

    using client::m_host;
    using client::m_port;

    chunked_post_client(const std::string& host, int port, const request& req, int rtimeout=0, int stimeout=0) : 
        client(host, port, rtimeout, stimeout) {
        std::string request_head;
	unparse_request(req, request_head);
        write_header(request_head);
    }


    chunked_post_client(const std::string& host, int port, const std::string& req, int rtimeout=0, int stimeout=0) : 
        client(host, port, rtimeout, stimeout) {

        std::string request_head(req);
        write_header(request_head);
    }

    chunked_post_client& operator<<(const std::string& s) {
        std::string s0;
        files::format_numeric(s0, s.size(), 16);
        s0 += "\r\n";
        s0 += s;
        s0 += "\r\n";
        m_sock << s0;
        return (*this);
    }

    void send(std::string& head, fields_t& fields, std::string& body) {
        std::string s;
        s += "\r\n0\r\n\r\n";
        m_sock << s;
        parse_response(head, fields, body);
    }
};



/*****************************/


class serialized_client : protected client {

protected:

    void send_get_serialized_head_1_(const request& req, const std::string& poison = "") {

	std::string req_str;

        // HACK.

        if (m_true_http) {

            // ���������� _��������_ ������, ��� ����.
            unparse_request(req, req_str, poison);

        } else {

            // ���������� ����������� http/1.1

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

            req_str += " HTTP/1.1\r\nHost: begun.ru\r\n\r\n";
        }

	m_sock << req_str;
    }

    void send_get_serialized_head_2_(fields_t& fields) {

        std::string head;

        parse_response(head, fields);
        check_reply_head(head);
    }

    void send_get_serialized_head_(const request& req, fields_t& fields, const std::string& poison = "") {
        send_get_serialized_head_1_(req, poison);
        send_get_serialized_head_2_(fields);
    }

public:

    using client::m_host;
    using client::m_port;
    using client::fields_t;
    using client::send;

    bool m_true_http;

    serialized_client(const std::string& h, int p, int rtimeout = 0, int stimeout = 0, bool truehttp = false) : 
        client(h, p, rtimeout, stimeout), 
        m_true_http(truehttp) {}

    template <typename T>
    void send_get_serialized(const request& req, T& out) {

        fields_t fields;
        send_get_serialized_head_(req, fields);
	serialization::load(m_sock, out);
    }

    template <typename T>
    void send_get_serialized(const request& req, T& out, fields_t& fields, const std::string& poison = "") {

        send_get_serialized_head_(req, fields, poison);
	serialization::load(m_sock, out);
    }

    template <typename T>
    void send_get_serialized_n(const request& req, std::vector<T>& out, fields_t& fields) {

        send_get_serialized_head_(req, fields);

        size_t n = 0;
        serialization::load(m_sock, n);

        out.resize(n);

        for (size_t i = 0; i < n; i++) {
            serialization::load(m_sock, out[i]);
        }
    }

    template <typename T>
    void send_get_serialized_n(const request& req, std::vector<T>& out) {
        fields_t fields;
        send_get_serialized_n(req, out, fields);
    }
};


struct raw_serialized_client : protected serialized_client {

public:

    using client::m_host;
    using client::m_port;

    using client::m_sock;
    
    using client::fields_t;

    using client::send;

    raw_serialized_client(const std::string& h, int p, int rtimeout = 0, int stimeout = 0, bool truehttp = false) : 
        serialized_client(h, p, rtimeout, stimeout, truehttp) {}

    void send_header(const request& req) {
        fields_t fields;
        send_get_serialized_head_(req, fields);
    }

    void send_header(const std::string& req) {
        m_sock << req;

        fields_t fields;
        send_get_serialized_head_2_(fields);
    }
    
    void send_header(const request& req, fields_t& fields) {
        send_get_serialized_head_(req, fields);
    }
    
};


/*****************************/


class serialized_post_client : protected raw_serialized_client {


public:

    using client::m_host;
    using client::m_port;

    serialized_post_client(const std::string& h, int p, int rtimeout = 0, int stimeout = 0, bool truehttp = false) : 
        raw_serialized_client(h, p, rtimeout, stimeout, truehttp) {}

    template <typename T>
    void send_serialized(request& req, const T& out) {

        serialization::saver_string_policy buf;
        serialization::save(buf, out);

        req.set_field("content-length", files::format(buf.data.size()));

        send_get_serialized_head_1_(req);

        m_sock << buf.data;

        fields_t fields;
        send_get_serialized_head_2_(fields);
    }

    template <typename T>
    void send_serialized_n(request& req, const std::vector<T>& out) {

        serialization::saver_string_policy buf;
        serialization::save(buf, out.size());

        req.set_field("content-length", files::format(buf.data.size()));

        for (typename std::vector<T>::const_iterator i = out.begin(); i != out.end(); ++i) {
            serialization::save(buf, *i);
        }

        send_get_serialized_head_1_(req);

        m_sock << buf.data;

        fields_t fields;
        send_get_serialized_head_2_(fields);
    }

};

class full_serialization_client : protected serialized_post_client {

public:

    using client::m_host;
    using client::m_port;
    using client::m_sock;

    using raw_serialized_client::send_header;
    using serialized_client::send_get_serialized;
    using serialized_client::send_get_serialized_n;
    using serialized_post_client::send_serialized;
    using serialized_post_client::send_serialized_n;

    full_serialization_client(const std::string& h, int p, int rtimeout = 0, int stimeout = 0, bool truehttp = false) : 
        serialized_post_client(h, p, rtimeout, stimeout, truehttp) {}

    template <typename T, typename F>
    void send_serialized_f(request& req, const T& out, F f) {

        serialization::saver_string_policy buf;
        serialization::save(buf, out);

        req.set_field("content-length", files::format(buf.data.size()));

        send_get_serialized_head_1_(req);

        m_sock << buf.data;

        f(m_sock);

        fields_t fields;
        send_get_serialized_head_2_(fields);
    }


};


/*

  ------

*/


template <typename CLIENT>
struct http1_1 {

    typedef persistent_client_<CLIENT> client;

private:
    std::map<configure::address, std::list<client> > connects;
    boost::mutex lock;

public:

    unsigned int rcv_timeout;
    unsigned int snd_timeout;

    unsigned int stat_size;

    http1_1(unsigned int r = 0, unsigned int s = 0) : rcv_timeout(r), snd_timeout(s),
                                                      stat_size(0) {}

    client get(const configure::address& a, unsigned int& sc, unsigned int& ss, 
               int rtimeout, int stimeout) {

        boost::mutex::scoped_lock l(lock);

        typename std::map<configure::address, std::list<client> >::iterator i = connects.find(a);

        if (i == connects.end() || i->second.empty()) {
            l.unlock();

            client ret(a.host, a.port, rtimeout, stimeout);

            lf::atomic_inc( &sc );

            lf::cas( &ss, ss, stat_size );
            return ret;

        } else {
            client ret = i->second.front();
            i->second.pop_front();
            stat_size--;
            lf::cas( &ss, ss, stat_size );

            try {
                ret.m_sock->m_obj->check_peer_state();
                return ret;

            } catch (const std::runtime_error& e) {

                logger::log(logger::DEBUG) << "bad client in pool found: " << e.what();

                l.unlock();
                client ret(a.host, a.port, rtimeout, stimeout);
                lf::atomic_inc( &sc );

                return ret;
            }
        }
    }

    client get(const configure::address& a, unsigned int& sc, unsigned int& ss) {
        return get(a, sc, ss, rcv_timeout, snd_timeout);
    }

    void put(const configure::address& a, client c, const typename CLIENT::fields_t& fields) {
        if (c) {

            typename CLIENT::fields_t::const_iterator i = fields.find("connection");

            if (i != fields.end() && 
                i->second.size() == 1 && 
                ::strcasecmp(i->second.front().c_str(), "close") == 0) {

                logger::log() << "Keepalive connection to " << a.host << ":" << a.port 
                              << " prematurely closed by response header field.";
                return;
            }

            boost::mutex::scoped_lock l(lock);
            connects[a].push_back(c);
            stat_size++;
        }
    }
};


}


#endif

