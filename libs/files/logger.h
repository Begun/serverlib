#ifndef __FILES_LOGGER_H
#define __FILES_LOGGER_H

#include <time.h>
#include <pthread.h>
#include <stdio.h>

#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>

#include "files/files.h"
#include "util/glob.h"

// необходимо для backtrace
#include <execinfo.h>

// Логирование.
// Принцип логирования прост:
// Все записи в лог -- это обычные записи в стандартные ввод и вывод.
// Отличие от обычной записи только в специальном форматировании!
// 
// Иными словами, для записи в файл необходимо, чтобы программа сама 
// перенаправила ввод/вывод в нужные файлы. (Как часть процесса демонизации.)
// Ротация проводится по такой же схеме, через открытие файла и вызов dup2.

namespace logger {

inline std::string format_time(time_t t, const char* fmtstr = "%F-%T") {

    if (t == 0 || t == 0xFFFFFFFF) return "never";

    char buff[256];
    struct tm tm_;
    ::localtime_r(&t, &tm_);
    size_t ret = ::strftime(buff, 255, fmtstr, &tm_);

    if (ret == 0 || ret >= 255) return "(invalid format)";
    return buff;
}





namespace detail {



struct log_spec_ {

    files::file info;
    files::file error;

    log_spec_() : info(files::stdout()), error(files::stderr()), num_lines(0) {}

    log_spec_(const std::string& i, const std::string& e) : info(files::open(i)), error(files::open(e)), 
                                                            num_lines(0), is_fresh(true) {} 
  
    void retarget(const std::string& i, const std::string& e) {

        replace_logfile_fd_name_(i, info->m_obj->fd);
        replace_logfile_fd_name_(e, error->m_obj->fd);
    }

    static void reset_stdin() {
        replace_logfile_fd_name_("/dev/null", 0, true);
    }

    inline void try_to_rotate(const unsigned int u_limit_count, const unsigned int num_files, const size_t logfile_size) {
    
      {
        static boost::mutex lock;
        boost::mutex::scoped_try_lock l(lock);

        if (!l) return;

        if (is_fresh) {
            is_fresh = false;
            num_lines = u_limit_count;
        }

        num_lines++; 
      }

      if (num_lines > u_limit_count) {
          rotate_logfiles(num_files, logfile_size);
          num_lines = 0;
      }
    }

    inline void rotate_logfiles(const unsigned int num_files, const size_t logfile_size) {

        rotate_logfile_(info->m_obj->fd, num_files, logfile_size);
        rotate_logfile_(error->m_obj->fd, num_files, logfile_size);
    }

private:

    unsigned int num_lines;
    bool is_fresh;


    inline void clean_old_logfiles_(const std::string& name, const unsigned int num_logfiles) {

        std::vector<std::string> files;

        util::glob(name + ".2*", files, false);

        if (files.size() <= num_logfiles) {
            return;
        }

        int n = files.size() - num_logfiles;

        for (std::vector<std::string>::const_iterator i = files.begin();
             i != files.end(); ++i, n--) {

            if (n <= 0) break;

            ::unlink(i->c_str());
        }
    }


    static inline void replace_logfile_fd_name_(const std::string& name, int fd, bool read = false) {

        int fd1 = ::open(name.c_str(), (read ? O_RDONLY : O_WRONLY | O_APPEND | O_CREAT | O_NOATIME), 0660);

        if (fd1 < 0)
            throw error::system_error(("could not open() in logger: " + name + " : ").c_str());

        ::dup2(fd1, fd);
    
        if (fd1 != fd)
            ::close(fd1);
    }

    // Внимание: платформо-зависимость. (/proc) Только для линукса.

    inline void rotate_logfile_(int fd, const unsigned int num_logfiles, const size_t logfile_size) {

        static boost::mutex lock;
        boost::mutex::scoped_lock l(lock);

        struct stat buf1;

        if (::fstat(fd, &buf1) < 0) return;

        if (!S_ISREG(buf1.st_mode)) return;

        if ((size_t)buf1.st_size >= logfile_size) {

            files::fmt ff;
            ff << "/proc/self/fd/" << fd;

            char buf[1024];
            int i = ::readlink(ff.data.c_str(), buf, 1023);

            if (i < 0 || i >= 1023) {
                throw error::system_error("Could not readlink() : " + ff.data);
            }

            buf[i] = '\0';

            ::rename(buf, (std::string(buf) + "." + format_time(::time(NULL))).c_str());

            replace_logfile_fd_name_(buf, fd);
            clean_old_logfiles_(buf, num_logfiles);
        }

    }

};


inline log_spec_& default_log() {

    static log_spec_ ret;
    return ret;
}

inline log_spec_& thread_specific_log(const log_spec_* f = NULL) {

    static boost::thread_specific_ptr<log_spec_> ret;

    if (f != NULL) {
        ret.reset(new log_spec_(*f));
    }

    log_spec_* r = ret.get();

    if (r == NULL) {
        return default_log();
    }

    return *r;
}

}






inline void set_logfiles(const std::string& info, const std::string& error, bool set_stdin = true) {

    // Убираем stdin от греха подальше.
    if (set_stdin) {
        detail::log_spec_::reset_stdin();
    }

    detail::default_log().retarget(info, error);
}



inline void set_thread_local_logfiles(const std::string& info, const std::string& error) {

    detail::log_spec_ tmp(info, error);
    detail::thread_specific_log(&tmp);
}





//enum level_ {
const unsigned int TRASH  = 0;   // Мусор
const unsigned int DEBUG  = 1;   // Дебаговый вывод
const unsigned int DBMS   = 2;   // Логирование запросов к БД
const unsigned int INFO   = 3;   // Информация для разработчика
const unsigned int IN     = 4;   // Входные данные к демону
const unsigned int OUT    = 5;   // Результаты работы демона.
const unsigned int ERROR  = 6;   // Нефатальная ошибка.
const unsigned int FATAL  = 7;    // Фатальная ошибка.
//};



struct loglevels {
        
    struct loglevel {
        boost::mutex lock;
        unsigned int level;
        std::string name;
        bool enabled;
        bool print_time;
        bool is_error;

        loglevel() : level(0), name(""), enabled(false), print_time(false), is_error(false) {}

        loglevel(unsigned int l, const std::string& n, bool pt = false, bool e = true, bool er = false) : 
            level(l), name(n), enabled(e), print_time(pt), is_error(er) {}

        void init(unsigned int l, const std::string& n, bool pt = false, bool e = true, bool er = false) {
            boost::mutex::scoped_lock lo(lock);
            level = l;
            name = n;
            print_time = pt;
            enabled = e;
            is_error = er;
        }

        void enable() {
            boost::mutex::scoped_lock l(lock);
            enabled = true;
        }

        void disable() {
            boost::mutex::scoped_lock l(lock);
            enabled = false;
        }
    };
  
private:
    struct loglevelp : public boost::shared_ptr<loglevel> {
        loglevelp() : boost::shared_ptr<loglevel>(new loglevel) {}
    };


public:

    static loglevel& get(unsigned int l) {

        init_defaults();

        static boost::mutex lock;
        static std::vector<loglevelp> levels;

        if (l >= levels.size()) {
            boost::mutex::scoped_lock lo(lock);
            levels.resize(l+1);
        }

        return *(levels[l]);
    }

    static void set_level(unsigned int l, const std::string& n, bool pt = false, bool e = true, bool er = false) {
        get(l).init(l, n, pt, e, er);
    }

    static void enable(unsigned int l) {
        get(l).enable();
    }

    static void disable(unsigned int l) {
        get(l).disable();
    }

    static void enable_gt(unsigned int l) {

        while (l <= FATAL) {
            get(l).enable();
            ++l;
        }
    }

    static void disable_lt(unsigned int l_) {

        unsigned int l = l_;

        while (l >= TRASH) {
            get(l).disable();
            if (l == 0) break;
            --l;
        }
    }

    static unsigned int which_level(const std::string& s) {

        if (s == "TRASH") {
            return TRASH;
        } else if (s == "DEBUG") {
            return DEBUG;
        } else if (s == "DBMS") {
            return DBMS;
        } else if (s == "INFO") {
            return INFO;
        } else if (s == "IN") {
            return IN;
        } else if (s == "OUT") {
            return OUT;
        } else if (s == "ERROR") {
            return ERROR;
        } else if (s == "FATAL") {
            return FATAL;

        } else {
            throw std::runtime_error("Invalid log level name: " + s);
        }
    }


    static unsigned int& get_rotation_freq() {
        static unsigned int __rotate_freq = 32000;
        return __rotate_freq;
    }

    static unsigned int& get_num_logfiles() {
        static unsigned int __num_logfiles = 10;
        return __num_logfiles;
    }

    static off_t& get_logfile_size() {
        static off_t __logfile_size = 10*1024*1024;
        return __logfile_size;
    }

    static void init_defaults() {
        static bool initted = false;

        if (!initted) {
            initted = true;
            
            set_level(TRASH, "TRASH");
            set_level(DEBUG, "DEBUG");
            set_level(DBMS,  "DBMS ");
            set_level(INFO,  "INFO ");
            set_level(IN,    "IN   ");
            set_level(OUT,   "OUT  ");
            set_level(ERROR, "ERROR", true, true, true);
            set_level(FATAL, "FATAL", true, true, true);
        }
    }
};



/*
 * Выдача стек-трейсов.
 * 
 */

inline std::string stacktrace() {

    static const unsigned int BACKTRACE_MESSAGE_LIMIT = 200;

    void* p_trace_array[BACKTRACE_MESSAGE_LIMIT];

    size_t len;
    char** msg;

    len = ::backtrace(p_trace_array, BACKTRACE_MESSAGE_LIMIT);
    msg = ::backtrace_symbols(p_trace_array, len);
        
    std::string ret = "{\n";

    for (size_t i = 0; i < len; ++i) {
        ret += msg[i];
        ret += "\n";
    }

    ret += "}\n";

    free(msg);

    return ret;
}



/*
 * 
 * Теперь сама пользовательская обертка.
 */


struct log : public files::fmt {

private:
    const loglevels::loglevel& level;


public:

    log(unsigned int lev = INFO, bool showtime = false) : level(loglevels::get(lev)) {

        if (!level.enabled) return;

        if (showtime || level.print_time) {
            files::format(data, format_time(::time(NULL)));
        } else {
            files::format(data, ::time(NULL));
        }

        files::format(data, "  ");
        files::format(data, level.name);
        files::format(data, " [");
        files::format(data, ::pthread_self());
        files::format(data, "]>  ");
    }

    template <typename T>
    log& operator<<(const T& t) {

        if (data.empty())
            return *this;

        files::fmt::operator<<(t);
        return *this;
    }

    ~log() { 
      
      if (!level.enabled) return;

      detail::log_spec_& lsp = detail::thread_specific_log();

      files::format(data, "\n");

      lsp.info << data;

      if (level.is_error) {
          lsp.error << data;
      }

      lsp.try_to_rotate(loglevels::get_rotation_freq(), loglevels::get_num_logfiles(), loglevels::get_logfile_size());
    }

    
};


}


#endif
