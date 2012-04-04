#ifndef __CLIENTSERVER_CLIENTSERVER_H
#define __CLIENTSERVER_CLIENTSERVER_H



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <stropts.h>
#include <net/if.h>
#include <netinet/tcp.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

#include <string.h>
#include <time.h>
#include <errno.h>

#include <iostream>

#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/thread.hpp>


#include "error/error.h"

#include "clientserver_base.h"

#include "lockfree/aux_.h"


namespace clientserver {

struct connection_error: public std::runtime_error {
    connection_error(const std::string &s):
        std::runtime_error(s)
    {}
};

struct recv_error: public std::runtime_error {
    recv_error(const std::string &s):
        std::runtime_error(s)
    {}
};

struct send_error: public std::runtime_error {
    send_error(const std::string &s):
        std::runtime_error(s)
    {}
};

/*
 * Теперь, собственно, сокеты.
 *
 */


// Просто любой файловый дескриптор который понимает send и recv.

class unknown_socket {

protected:
    void teardown(const std::string& msg) {
        error::system_error err(msg);
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
        fd = -1;
        throw err;
    }

public:
    int fd;

    unknown_socket(int f) : fd(f) {}

    ~unknown_socket() {
        if (fd < 0) return;
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }

    void send(const void* data, size_t len) {
        ssize_t tmp = ::send(fd, data, len, MSG_NOSIGNAL);

        if (tmp < 0 || tmp != (ssize_t)len)
            throw send_error("could not send() : " + error::strerror());
    }

    size_t recv(void* buff, size_t len) {
        int tmp = 0;
        tmp = ::recv(fd, buff, len, 0);

        if (tmp < 0)
            throw recv_error("could not recv() : " + error::strerror());

        if (tmp == 0)
            throw eof_exception();

        return tmp;
    }



    std::pair<std::string,int> getname(bool peer) {

        struct sockaddr_in addr;
        socklen_t alen = sizeof(addr);
        char buff[INET_ADDRSTRLEN];

        if (peer) {
            if (::getpeername(fd, (struct sockaddr*)&addr, &alen) < 0)
                throw error::system_error("could not getpeername()");

        } else {
            if (::getsockname(fd, (struct sockaddr*)&addr, &alen) < 0)
                throw error::system_error("could not getsockname()");
        }

        if (::inet_ntop(AF_INET, (void*)(&addr.sin_addr), buff, INET_ADDRSTRLEN) == NULL)
            throw error::system_error("could not inet_ntop()");

        return std::make_pair(buff, ntohs(addr.sin_port));
    }

    std::pair<std::string,int> getsockname() {
        return getname(false);
    }

    std::pair<std::string,int> getpeername() {
        return getname(true);
    }

    void check_peer_state() {

        tcp_info inf;
        socklen_t tmp = sizeof(tcp_info);

        if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, (void*)&inf, &tmp) == 0 &&
            inf.tcpi_state != TCP_ESTABLISHED) {

            throw std::runtime_error("check_peer_state(): connection closed by peer.");
        }
    }

};



// Сокет, который пришел из accept().

class service_socket : public unknown_socket {


public:
    unsigned int thread_number;

    service_socket(int f, unsigned int tn) : unknown_socket(f), thread_number(tn) {}
};



// Серверный сокет: делает bind и listen в конструкторе;
// Метод serve крутится в бесконечном цикле, вызывая accept и создавая
// соотв. буфферизируемый сокет. Этот сокет потом передается пользовательской функции.

class server_socket : public unknown_socket {

protected:


    int thread_count;

    const unsigned int rcv_timeout;
    const unsigned int snd_timeout;

    // Ненавижу наследование.
    bool do_setopts;

    struct scoped_counter {
        server_socket& self;
        int count;

        scoped_counter(server_socket& s) : self(s) {
            count = lockfree::atomic_add(&(self.thread_count), 1);
        }

        ~scoped_counter() {
            lockfree::atomic_add(&(self.thread_count), -1);
        }
    };

    void setopts(int fd) {

        // Опционально, но должно улучшить диагностику.
        int is_true = 1;
        if (::setsockopt(fd, SOL_TCP, TCP_NODELAY, &is_true, sizeof(is_true)) < 0)
            std::cerr << "WARNING: could not setsockopt(TCP_NODELAY) : " << error::strerror() << std::endl;
            //throw error::system_error("could not setsockopt(TCP_NODELAY) : ");

        if (rcv_timeout) {

            struct timeval tv;
            tv.tv_sec = rcv_timeout / 1000;
            tv.tv_usec = (rcv_timeout % 1000) * 1000;
            if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_RCVTIMEO) failed. (" << rcv_timeout << ")" << std::endl;
                //throw error::system_error("could not setsockopt(SO_RCVTIMEO) : ");

            if (tv.tv_sec) {

                if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &is_true, sizeof(is_true)) < 0)
                    std::cerr << "WARNING: setsockopt(SO_KEEPALIVE) failed." << std::endl;

                if (::setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &(tv.tv_sec), sizeof(tv.tv_sec)) < 0)
                    std::cerr << "WARNING: setsockopt(TCP_KEEPIDLE) failed. " << rcv_timeout << ")" << std::endl;

            }
        }

        if (snd_timeout) {
            struct timeval tv;
            tv.tv_sec = snd_timeout / 1000;
            tv.tv_usec = (snd_timeout % 1000) * 1000;

            if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_SNDTIMEO) failed. (" << snd_timeout << ")" << std::endl;
        }
    }


    template <typename F>
    void run(F& f, int client, bool priority) {

        if (do_setopts)
            setopts(client);

        if (priority) {

            struct sched_param param;
            param.sched_priority = 1;

            int tmp = ::pthread_setschedparam(::pthread_self(), SCHED_RR, &param);

            if (tmp != 0) {
                std::cerr << "Could not pthread_setschedparam() : " << tmp << std::endl;
                abort();
            }
        }

        scoped_counter sc(*this);

        boost::shared_ptr<service_socket> s(new service_socket(client, sc.count));
        boost::shared_ptr<buffer<service_socket> > b(new buffer<service_socket>(s));

        // Exception-safe

        f(b);
    }


    server_socket(unsigned int rtimeout, unsigned int stimeout, bool setop) :
        unknown_socket(-1), thread_count(0), rcv_timeout(rtimeout), snd_timeout(stimeout), do_setopts(setop)
        {}


public:

    server_socket(const std::string& host, int port, unsigned int rtimeout = 0, unsigned int stimeout = 0) :
        unknown_socket(-1), thread_count(0), rcv_timeout(rtimeout), snd_timeout(stimeout), do_setopts(true) {

        fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0) throw error::system_error("could not socket() : ");

        int is_true = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &is_true, sizeof(is_true)) < 0)
            teardown("could not setsockopt(SO_REUSEADDR) : ");

        if (::setsockopt(fd, SOL_TCP, TCP_NODELAY, &is_true, sizeof(is_true)) < 0)
            teardown("could not setsockopt(TCP_NODELAY) : ");

        struct sockaddr_in addr;
        ::memset(&addr, 0, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        //addr.sin_addr.s_addr = INADDR_ANY;

        /* TODO: select the address properly, based on interface names. */
        if (::inet_pton(AF_INET, host.c_str(), (void*)&addr.sin_addr) <= 0)
            teardown("could not inet_pton() : ");

        if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
            teardown("could not bind() : ");

        if (::listen(fd, 1024) < 0)
            teardown("could not listen() : ");
    }


    // Серверная часть.

    template <typename F>
    void serve(F f, size_t maxconns, bool priority) {

        while (1) {

            try {

                int client = ::accept(fd, NULL, NULL);

                if (client < 0) {
                    break;
                }

                if (maxconns > 0 &&
                    (size_t)lockfree::atomic_add(&thread_count, 0) >= maxconns) {

                    ::shutdown(client, SHUT_RDWR);
                    ::close(client);
                    continue;
                }

                boost::thread(boost::bind<void>(&server_socket::run<F>, this,
                                                boost::ref(f), client, priority));


            } catch (std::exception& e) {
                std::cerr << "ERROR in serving : " << e.what() << std::endl;

            } catch (...) {
                std::cerr << "UNKNOWN ERROR in serving" << std::endl;
            }
        }
    }
};


// Для удобства: то, что сервер отдает пользователю.
typedef boost::shared_ptr<buffer<service_socket> > service_buffer;



// Клиентский сокет: делает connect на данный хост и порт.

class client_socket : public unknown_socket {
public:
    client_socket(const std::string& host, int port,
                  unsigned int rcv_timeout, unsigned int snd_timeout) : unknown_socket(-1) {

        fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (fd < 0) throw error::system_error("could not socket() : ");

        int is_true = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &is_true, sizeof(is_true)) < 0)
            teardown("could not setsockopt(SO_REUSEADDR) : ");

        if (::setsockopt(fd, SOL_TCP, TCP_NODELAY, &is_true, sizeof(is_true)) < 0)
            teardown("could not setsockopt(TCP_NODELAY) : ");

        if (snd_timeout) {

            struct timeval tv;
            tv.tv_sec = snd_timeout / 1000;
            tv.tv_usec = (snd_timeout % 1000) * 1000;

            if (::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_SNDTIMEO) failed. (" << snd_timeout << ")" << std::endl;
        }

        if (rcv_timeout) {

            struct timeval tv;
            tv.tv_sec = rcv_timeout / 1000;
            tv.tv_usec = (rcv_timeout % 1000) * 1000;

            if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0)
                std::cerr << "WARNING: setsockopt(SO_RCVTIMEO) failed. (" << rcv_timeout << ")" << std::endl;
            //throw error::system_error("could not setsockopt(SO_RCVTIMEO) : ");

            if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &is_true, sizeof(is_true)) < 0)
                std::cerr << "WARNING: setsockopt(SO_KEEPALIVE) failed." << std::endl;

            if (tv.tv_sec) {

                if (::setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &(tv.tv_sec), sizeof(tv.tv_sec)) < 0)
                    std::cerr << "WARNING: setsockopt(TCP_KEEPIDLE) failed. " << rcv_timeout << ")" << std::endl;

            }
        }

        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), (void*)&addr.sin_addr) <= 0)
            teardown("could not inet_pton() : ");

        if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)))
            teardown("could not connect() : ");

    }
};


// Для удобства: то, что отдается пользователю.
typedef boost::shared_ptr<buffer<client_socket> > client_buffer;


// Создает буфферизуемый пользовательский сокет.

inline client_buffer connect(const std::string& host, int port,
                             unsigned int rcv_timeout = 0, unsigned int snd_timeout = 0)
try {
    boost::shared_ptr<client_socket> s(new client_socket(host, port, rcv_timeout, snd_timeout));
    return client_buffer(new buffer<client_socket>(s));
} catch(const error::system_error &e) {
    throw connection_error( e.what() );
}



template <typename C>
inline std::pair<std::string,int> get_server_address(boost::shared_ptr<buffer<C> > sock) {
    return sock->m_obj->getsockname();
}

template <typename C>
inline std::pair<std::string,int> get_client_address(boost::shared_ptr<buffer<C> > sock) {
    return sock->m_obj->getpeername();
}

inline void check_peer_state(service_buffer sock) {
    sock->m_obj->check_peer_state();
}




// Для удобства: запуск сервера.
// Замечание: если server или service потом уничтожатся, то все сломается!

template <typename F>
inline void serve(server_socket& server, F service, size_t maxconns = 0, bool priority = false) {

    // boost::protect нужен для того, чтобы вложенные boost::bind
    // не конфликтовали и честно выполняли композицию функций.

    boost::thread th(boost::bind<void>(&server_socket::serve<F>,
                                       &server, service,
                                       maxconns, priority));
}

template <typename F>
inline void serve_blocking(server_socket& server, F service, size_t maxconns = 0, bool priority = false) {

    server.serve(service, maxconns, priority);
}

}

/*

   Пример использования:

   На сервере:

     struct service_thread {
        void operator()(service_buffer s) {
           s >> ...;
           s << ...;
        }
     };

     server_socket server("192.168.0.2", 11099);
     service_thread service;
     serve(server, service);

   На клиенте:

     client_buffer c = connect("192.168.0.2", 11099);
     c << ...;
     c >> ...;


 */




#endif
