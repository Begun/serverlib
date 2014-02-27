#ifndef __UTIL_UTIL_H
#define __UTIL_UTIL_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/filesystem/path.hpp>

#include "files/logger.h"

namespace util {

typedef unsigned int OID;


// Создать все папки в пути, "рекурсивно".
// Делается таким извращенным способом, чтобы не линковать boost_filesystem.

inline void mkdir_recursively(const boost::filesystem::path& p) {

    if (p.empty()) return;

    struct stat st;
    int tmp = ::stat(p.string().c_str(), &st);

    if (tmp != 0 && errno != ENOENT)
        throw std::runtime_error("mkdir_recursively: Could not stat: " + p.string());

    if (tmp == 0 && !S_ISDIR(st.st_mode))
        throw std::runtime_error("mkdir_recursively: File exists and is not a directory: " + p.string());

    if (tmp == 0) {
        return;

    } else {

        mkdir_recursively(p.branch_path());

        if (::mkdir(p.string().c_str(), 0770) != 0) {
            if (errno == EEXIST)
                logger::log() << "mkdir failed for " + p.string() << " with EEXIST";
            else
                throw std::runtime_error("mkdir_recursively: mkdir failed: " + p.string());
        }

        logger::log(logger::TRASH) << "  mkdir(" << p.string() << ", 0770)";
    }
}


inline void mkdir_recursively2(const boost::filesystem::path& p) {

    mkdir_recursively(p.branch_path());
}

inline void unlink_recursively(const std::string& path) {

    std::vector<std::string> files;

    util::glob(path + "/*", files, false);

    files.push_back(path);

    for (std::vector<std::string>::const_iterator i = files.begin(); i != files.end(); ++i) {

        struct stat st;
        int tmp = ::stat(i->c_str(), &st);

        if (tmp != 0) {
            throw error::system_error("unlink_recursively: could not stat: " + *i);
        }

        if (S_ISDIR(st.st_mode)) {

            if (*i != path) {
                unlink_recursively(*i);

            } else {

                if (::rmdir(i->c_str()) < 0) {
                    throw error::system_error("unlink_recursively: could not rmdir(" + *i + ")");
                }
            }

        } else {

            if (::unlink(i->c_str()) < 0) {
                throw error::system_error("unlink_recursively: could not unlink(" + *i + ")");
            }

            logger::log(logger::TRASH) << "  unlink(" << *i << ")";
        }
    }
}



// Демонизация процесса.

inline void daemonize(const std::string& pidfile, const std::string& info_log, const std::string& error_log, bool do_daemon = true) {
    pid_t pid, sid;

    struct stat tmp;

    if (::stat(pidfile.c_str(), &tmp) >= 0) {
        logger::log(logger::ERROR) << "Could not daemonize: pidfile '" + pidfile + "' already exists.";
        ::exit(1);
    }

    if (do_daemon) {

        if (::getppid() == 1) return;

        pid = ::fork();
        if (pid < 0) {
            logger::log(logger::ERROR) << "Could not fork() when daemonizing!";
            ::exit(1);
        }

        if (pid > 0) {
            ::exit(0);
        }

        sid = ::setsid();

        if (sid < 0) {
            logger::log(logger::ERROR) << "Could not setsid() when daemonizing!";
            ::exit(1);
        }
    }

    try {
        files::file pf = files::open(pidfile);
        std::string ptmp;
        files::format(ptmp, ::getpid());
        pf << ptmp;

    } catch (std::exception& e) {
        logger::log(logger::ERROR) << "Could not write to pidfile '" << pidfile << "': " << e.what();
        ::exit(1);

    } catch (...) {
        logger::log(logger::ERROR) << "Could not write to pidfile '" << pidfile << "'. Unknown error.";
        ::exit(1);
    }

    // Запускаем логгер.
    logger::set_logfiles(info_log, error_log);
}

}


#endif
