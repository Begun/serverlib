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



inline std::string resolve(const std::string& hostname) {
    struct hostent result;
    struct hostent* tmpret;
    char buffer[128];
    int h_errnop;
    struct in_addr in;

    if (::gethostbyname_r(hostname.c_str(), &result, buffer, sizeof(buffer), &tmpret, &h_errnop) != 0 ||
	tmpret == NULL) {

	errno = h_errnop;
	throw error::system_error("Could not gethostbyname_r() : ");
    }

    if (result.h_addr_list[0] == NULL) {
	throw std::runtime_error("Could not gethostbyname_r() : empty result");
    }

    in.s_addr = *(u_long*)result.h_addr_list[0];

    if (::inet_ntop(AF_INET, (void*)(&in), buffer, sizeof(buffer)) == NULL) 
	throw error::system_error("could not inet_ntop()");

    return buffer;
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
