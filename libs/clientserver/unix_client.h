#ifndef __CLIENTSETVER_UNIX_CLIENT_H
#define __CLIENTSETVER_UNIX_CLIENT_H


#include "error/error.h"

#include "clientserver/clientserver.h"

#include <sys/socket.h>
#include <sys/un.h>


namespace clientserver {


class unix_client_socket : public unknown_socket {
public:

    unix_client_socket(const std::string& path, int rcv_timeout, int snd_timeout) : 
        unknown_socket(-1) {

	fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
 
	if (fd < 0) throw error::system_error("could not socket() : ");

	struct sockaddr_un addr;
	bzero(&addr, sizeof(addr));
 
	addr.sun_family = AF_UNIX;
        ::strncpy(addr.sun_path, path.data(), path.size());

	if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr))) 
            teardown("could not connect() : ");


	if (rcv_timeout) {

	    struct timeval tv;
	    tv.tv_sec = rcv_timeout / 1000;
	    tv.tv_usec = (rcv_timeout % 1000) * 1000;

	    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_RCVTIMEO) failed. (" << rcv_timeout << ")" << std::endl;
	}

        if (snd_timeout) {
            struct timeval tv;
            tv.tv_sec = snd_timeout / 1000;
            tv.tv_usec = (snd_timeout % 1000) * 1000;

            if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_SNDTIMEO) failed. (" << snd_timeout << ")" << std::endl;
        }

    }
};


// Для удобства: то, что отдается пользователю.
typedef boost::shared_ptr<buffer<unix_client_socket> > unix_client_buffer;

inline unix_client_buffer unix_connect(const std::string& path, int rtimeout = 0, int stimeout = 0) {
    boost::shared_ptr<unix_client_socket> s(new unix_client_socket(path, rtimeout, stimeout));
    return unix_client_buffer(new buffer<unix_client_socket>(s));
}


}

#endif
