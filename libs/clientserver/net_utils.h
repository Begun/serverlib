#ifndef __NET_UTILS_H
#define __NET_UTILS_H



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <stropts.h>
#include <net/if.h>

#include <string.h>
#include <time.h>
#include <errno.h>

#include <string>
#include <vector>


#include "files/files.h"


namespace clientserver {



struct raii_addrinfo {
    struct addrinfo* a;
    raii_addrinfo(struct addrinfo* _a) : a(_a) {}

    ~raii_addrinfo() {
        if (a != NULL) ::freeaddrinfo(a);
    }
};




inline std::string resolve(const std::string& hostname) {

    struct addrinfo* result = NULL;

    int err = ::getaddrinfo(hostname.c_str(), NULL, NULL, &result);

    if (err) {
        throw std::runtime_error("getaddrinfo() failed for " + hostname + " : " + files::format(err));
    }

    raii_addrinfo _ai(result);

    for (struct addrinfo* i = result; i != NULL; i = i->ai_next) {

        if (i->ai_family != PF_INET || i->ai_socktype != SOCK_STREAM) {
            continue;
        }

        struct sockaddr_in* sin = (sockaddr_in*)(i->ai_addr);

        char buffer[128];

        if (::inet_ntop(AF_INET, (void*)(&(sin->sin_addr)), buffer, sizeof(buffer)) == NULL)
            throw error::system_error("could not inet_ntop() while resolving " + hostname);

        return buffer;
    }  
    
    throw std::runtime_error("getaddrinfo() for " + hostname + " : empty result");
}
   

// Линуксно-специфично, я думаю.

inline std::vector<int> get_ip_address_for_interface(const std::string& ifa = "eth0") {

    int s;
    struct ifreq ifr;
    struct sockaddr_in sa;

    ::strcpy(ifr.ifr_name, ifa.c_str());
    s = ::socket(AF_INET, SOCK_DGRAM, 0);

    if (s >= 0) {
        if (::ioctl(s, SIOCGIFADDR, &ifr)) {
            ::close(s);
            return std::vector<int>();
        }

        ::close(s);
        ::memcpy(&sa, &ifr.ifr_addr, 16);

        unsigned char* ipaddr = (unsigned char*)&(sa.sin_addr.s_addr);
        
        std::vector<int> ret;
        ret.push_back((int)ipaddr[0]);
        ret.push_back((int)ipaddr[1]);
        ret.push_back((int)ipaddr[2]);
        ret.push_back((int)ipaddr[3]);

        return ret;
    }

    return std::vector<int>();
}


inline void parse_ip_address(const std::string& ip, std::vector<unsigned int>& ip_numeric) {

    files::scn<files::string_as_buffer> buf(files::string_to_buffer(ip, '.'));

    for (int i = 0; i < 4; i++) {
        unsigned int n;
        buf.scan(n, ".", 1);
        ip_numeric.push_back((n > 255 ? 0 : n));
    }
}

}

#endif
