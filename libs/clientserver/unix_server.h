#ifndef __CLIENTSERVER_UNIX_SERVER_H
#define __CLIENTSERVER_UNIX_SERVER_H

#include "clientserver.h"
#include "unix_client.h"


namespace clientserver {


class unix_server_socket : public server_socket {

protected:

public:

    unix_server_socket(const std::string& path, unsigned int rtimeout = 0, unsigned int stimeout = 0) : 
        server_socket(rtimeout, stimeout, false) {


	fd = ::socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0) throw error::system_error("could not socket() : ");

        struct sockaddr_un addr;
        ::memset(&addr, 0, sizeof(addr));

        addr.sun_family = AF_UNIX;
        ::strncpy(addr.sun_path, path.data(), path.size());

        ::unlink(addr.sun_path);

	if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	    throw error::system_error("could not bind() to " + path + " : ");

	if (::listen(fd, 1024) < 0)
	    throw error::system_error("could not listen() : ");
    }
};


}

#endif
