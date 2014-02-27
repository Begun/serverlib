#ifndef __FILES_FILES_BASE_H
#define __FILES_FILES_BASE_H


#include "clientserver/clientserver_base.h"
#include "error/error.h"

#include <boost/shared_ptr.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace files {

// Прокинем, для удобства.
typedef clientserver::eof_exception eof_exception;


// "Базовый класс" для поточной буфферизации над файлами файловой системы.

class file_ {
public:
    int fd;

    file_(int f) : fd(f) {}
    
    ~file_() {
	::close(fd);
    }

    void send(const void* data, size_t len) {
	ssize_t tmp = ::write(fd, data, len);

	if (tmp < 0 || tmp != (ssize_t)len)
	    throw error::system_error("could not write() : ");
    }

    size_t recv(void* buff, size_t len) {
	int tmp = 0;
	tmp = ::read(fd, buff, len);

	if (tmp < 0)
	    throw error::system_error("could not read() : ");

	if (tmp == 0) 
	    throw eof_exception();
	
	return tmp;
    }

    off_t lseek(off_t off, int whence) {
        int i = ::lseek(fd, off, whence);
        if (i < 0) {
            throw std::runtime_error("Could not lseek()!");
        }
        return i;
    }
};


// Для удобства: то, что отдается пользователю.
typedef boost::shared_ptr<clientserver::buffer<file_> > file;

inline file open(const std::string& f, bool onlynew = false, bool readonly = false) {

    int fd = ::open(f.c_str(), 
                    (readonly ? (O_RDONLY /*| O_NOATIME*/) : 
                     ((O_RDWR | O_APPEND | O_CREAT | O_NOATIME) | (onlynew ? O_EXCL : 0))),
                    0660);

    if (fd < 0) 
	throw error::system_error(("could not open() : " + f + " : ").c_str());

    boost::shared_ptr<file_> s(new file_(fd));
    return file(new clientserver::buffer<file_>(s));
}

 inline file create(const std::string& f, bool onlynew = false) {

    int fd = ::open(f.c_str(), (O_RDWR | O_TRUNC | O_CREAT | O_NOATIME | (onlynew ? O_EXCL : 0)), 0660);

    if (fd < 0)
        throw error::system_error(("could not open() : " + f + " : ").c_str());

    boost::shared_ptr<file_> s(new file_(fd));
    return file(new clientserver::buffer<file_>(s));
}

inline file connect(int fd) {
    boost::shared_ptr<file_> s(new file_(fd));
    return file(new clientserver::buffer<file_>(s));
}


inline file stdin() {
    static file __stdin = connect(0);
    return __stdin;
}

inline file stdout() {
    static file __stdout = connect(1);
    return __stdout;
}

inline file stderr() {
    static file __stderr = connect(2);
    return __stderr;
}


}


#endif
