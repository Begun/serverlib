#ifndef __CONFIGURE_ADDRESSES_H
#define __CONFIGURE_ADDRESSES_H

#include "files/files.h"


namespace httpd {

struct address {
    std::string host;
    int port;
    std::string type;

    address() : host(""), port(-1), type("") {}

    address(const std::string& h, int p, const std::string& t = "") : host(h), port(p), type(t) {}

    bool operator<(const address& a) const {
        if (host < a.host) return true;
        if (host == a.host && port < a.port) return true;
        if (host == a.host && port == a.port && type < a.type) return true;
        return false;
    }
};


typedef std::vector<address> addresslist;


inline void parse(const std::string& s, address& a) {

    files::scn<files::string_as_buffer> ss(files::string_to_buffer(s));
    a = address ();
    try {

        ss.skip(" \t\n\r", 4);
        ss.scan(a.host, ":", 1);

        if (ss.scan(a.port, ":\0", 2) != ':') return;
        ss.scan(a.type, "\0", 1);
    } catch (files::eof_exception& e) {
    } catch (...) {
        throw std::runtime_error("Invalid address format: " + s);
    }
}


inline void parse(const std::string& s0, addresslist& v) {

    files::scn<files::string_as_buffer> ss(files::string_to_buffer(s, ','));

    v.clear();

    while (1) {

        std::string host;
        unsigned int port;
        std::string type;

        try {

            ss.skip(" \t\n\r", 4);
            ss.scan(host, ":", 1);

            if (ss.scan(port, ",:", 2) == ':') {
                ss.scan(type, ",", 1);
            }

        } catch (files::eof_exception& e) {
            break;

        } catch (...) {
            throw std::runtime_error("Invalid addresslist format: " + s);
        }

        v.push_back(address(host, port, type));
    }
}
        
}


namespace files {

inline std::string& format(std::string& out, const configure::address& a) {

    format(out, a.host);
    format(out, ":");
    format(out, a.port);
    if (a.type.size() > 0) {
        format(out, ":");
        format(out, a.type);
    }

    return out;
}

inline std::string& format(std::string& out, const configure::addresslist& al) { 
    
    for (configure::addresslist::const_iterator i = al.begin(); i != al.end(); ++i) {

        if (i != al.begin()) {
            format(out, ", ");
        }

        format(out, *i);
    }

    return out;
}

}



#endif
