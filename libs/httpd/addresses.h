#ifndef __HTTPD_ADDRESSES_H
#define __HTTPD_ADDRESSES_H

#include "files/files.h"
#include "clientserver/net_utils.h"


namespace httpd {

struct address {
    std::string host;
    int port;
    //std::string type;

    address() : host(""), port(-1) {}

    address(const std::string& h, int p) : host(h), port(p) {}

    bool operator<(const address& a) const {
        if (host < a.host) return true;
        if (host == a.host && port < a.port) return true;
        return false;
    }

    bool operator==(const address& a) const {
        return host == a.host && port == a.port;
    }
};

struct hostname {
    std::string host;

    hostname() : host("") {}

    hostname(const std::string& h) : host(h) {}

    bool operator<(const hostname& a) const { return (host < a.host); }

    bool operator==(const hostname& a) const {
        return host == a.host;
    }
};


typedef std::vector<address> addresslist;

typedef std::vector<hostname> hostnamelist;

// Deprecated. Use more general round_robin.
struct roundrobin {
    size_t rr;
    boost::mutex lock;

    roundrobin() : rr(0) {}

    const address& get_next(const addresslist& al) {
        boost::mutex::scoped_lock l(lock);
        return al[(rr++) % al.size()];
    }
};

// General Round Robin algorithm for vectors.
// TODO: move it to some more suitable source file.
struct round_robin {
    size_t current;
    boost::mutex lock;

    round_robin() : current(0) {}

    template<typename VECTOR>
    const typename VECTOR::value_type& get_next(const VECTOR& vec) {
        boost::mutex::scoped_lock l(lock);
        return vec[(current++) % vec.size()];
    }
};


struct roundrobin_map {

    boost::mutex lock;
    std::map<std::string,boost::shared_ptr<roundrobin> > roundrobins;

    const address& get_next(const std::string& us, const addresslist& al) {
        
        boost::mutex::scoped_lock l(lock);

        std::map<std::string,boost::shared_ptr<roundrobin> >::iterator i = roundrobins.find(us);
        if (i == roundrobins.end()) {
            i = roundrobins.insert(i, std::make_pair(us, boost::shared_ptr<roundrobin>(new roundrobin)));
    }

        return i->second->get_next(al);
}
};


inline void parse_option(const std::string& s, hostname& a) {

    files::scn<files::string_as_buffer> ss(files::string_to_buffer(s, '\0'));

    ss.skip(" \t\n\r", 4);
    ss.scan(a.host, "\0 \t\n\r", 5);

    a.host = clientserver::resolve(a.host);
}

inline void parse_option(const std::string& s, address& a) {

    files::scn<files::string_as_buffer> ss(files::string_to_buffer(s, '\0'));
    a = address ();
        try {

            ss.skip(" \t\n\r", 4);
        ss.scan(a.host, ":", 1);

        // HACK #3204

        a.host = clientserver::resolve(a.host);

        ss.scan(a.port, " \t\n\r\0", 5);

        if (a.port <= 0 || a.port >= 65535) throw std::runtime_error("invalid port number: " + files::format(a.port));

    } catch (clientserver::eof_exception& e) {
        throw std::runtime_error("Invalid address format: " + s);
    } catch (...) {
        throw std::runtime_error("Invalid address format: " + s);
    }
}
        
       
}


namespace files {

template <> struct format_<httpd::address> {
    std::string& operator()(std::string& out, const httpd::address& a) {

    format(out, a.host);
    format(out, ":");
    format(out, a.port);

        return out;
    }
};

template <> struct format_<httpd::hostname> {
    std::string& operator()(std::string& out, const httpd::hostname& a) {
        format(out, a.host);
    return out;
}
};

template <> struct format_<httpd::addresslist> {
    std::string& operator()(std::string& out, const httpd::addresslist& al) { 
    
    for (httpd::addresslist::const_iterator i = al.begin(); i != al.end(); ++i) {

        if (i != al.begin()) {
            format(out, ", ");
        }

        format(out, *i);
    }

    return out;
}
};

}



#endif
