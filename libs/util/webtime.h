#ifndef __UTIL_WEBTIME_H
#define __UTIL_WEBTIME_H

#include <time.h>

#include "files/files_format.h"

namespace util {

inline std::string webtime(time_t t) {

    struct tm lt;
    
    ::gmtime_r(&t, &lt);

    static const char* __days[7] = 
        { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* __mont[12] = 
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    files::fmt ff;

    ff << __days[lt.tm_wday] << ", " << lt.tm_mday << ' ' << __mont[lt.tm_mon] << ' ' << lt.tm_year+1900
       << ' ';

    ff << (lt.tm_hour < 10 ? "0" : "") << lt.tm_hour << ':';
    ff << (lt.tm_min < 10 ? "0" : "") << lt.tm_min << ':';
    ff << (lt.tm_sec < 10 ? "0" : "") << lt.tm_sec << " GMT";

    return ff.data;
}

inline std::string syslogtime(time_t t) {

    struct tm lt;

    ::localtime_r(&t, &lt);

    static const char* __mont[12] = 
        { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    files::fmt ff;

    ff << __mont[lt.tm_mon] << ' ' 
       << (lt.tm_mday < 10 ? " " : "") << lt.tm_mday << ' '
       << (lt.tm_hour < 10 ? "0" : "") << lt.tm_hour << ':' 
       << (lt.tm_min < 10 ? "0" : "") << lt.tm_min << ':' 
       << (lt.tm_sec < 10 ? "0" : "") << lt.tm_sec;

    return ff.data;
}

}

#endif
