#ifndef __ERROR_ERROR_H
#define __ERROR_ERROR_H

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <string>
#include <stdexcept>

#include <boost/thread/mutex.hpp>

namespace error {


inline std::string strerror_old() {
    static boost::mutex _lock;

    boost::mutex::scoped_lock l(_lock);

    char err[256];
    return std::string(::strerror_r(errno, err, 255));
}

inline std::string strerror() {
    char err[256];
    ::snprintf(err, 255, "errno = %d", errno);
    return std::string(err);
}


class system_error : public std::runtime_error {
public:
    system_error(const std::string& s) : std::runtime_error(s + strerror()) {}
};

}


#endif
