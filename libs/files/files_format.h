#ifndef __FILES_FILES_FORMAT_H
#define __FILES_FILES_FORMAT_H

#include <type_traits>

#include <math.h>
#include <sstream>

namespace files {


/*
 * Эффективный, безопасный способ форматирования.
 * Очень полезен в качестве сериализации.
 */


// Эта тупость нужна для того, чтобы победить ворнинг gcc. :)

template <typename T> inline void _numeric_sign(std::string& out, T& t) { 
    if (t < 0) { 
	out += '-'; 
	t = -t; 
    } 
}

template <> inline void _numeric_sign<unsigned short>(std::string& /*out*/, unsigned short& /*t*/) { }
template <> inline void _numeric_sign<unsigned int>(std::string& /*out*/, unsigned int& /*t*/) { }
template <> inline void _numeric_sign<unsigned long>(std::string& /*out*/, unsigned long& /*t*/) { }
template <> inline void _numeric_sign<unsigned long long>(std::string& /*out*/, unsigned long long& /*t*/) { }


template <typename T>
inline std::string& format_numeric(std::string& out, T t, int base = 10, int pad = 0) {
    const char* base_ = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    char buff[65];
    int i = 0;

    if (base < 2 || base > 36) base = 10;

    if (out.capacity() - out.size() < 12) {
	out.reserve( out.capacity() + 12 );
    }
   
    _numeric_sign(out, t);

    while (i < 64) {
	int p = (t % base);

	buff[i] = base_[p];
	t /= base;
	if (t == 0) break;
	i++;
    }

    while (pad > i+1) {
        out += '0';
        pad--;
    }

    while (i >= 0) {
	out += buff[i];
	i--;
    }

    return out;
}


// HACK.

template <typename T> inline T __round(T);

template <> inline float       __round(float       x) { return roundf(x); }
template <> inline double      __round(double      x) { return round(x);  }
template <> inline long double __round(long double x) { return roundl(x); }

template <typename T>
inline std::string& format_real(std::string& out, T t, const char* /*fmt_*/, int round = 5) {

    if (isnan(t)) {
        out += "nan";
        return out;
    }

    if (isinf(t)) {
        if (isinf(t) < 0) out += '-';
        out += "inf";
        return out;
    }

    long l = (long)t;

    T t0 = t - l;

    if (t0 < 0) {
        t0 = -t0;
        l = -l;
        out += '-';
    }

    // HACK!
    if (t0 > 1.0) {
        out += "nan";
        return out;
    }

    if (round > 63) round = 63;

    long whole = 1;

    for (int i = 0; i < round; ++i) {
        t0 *= 10;
        whole *= 10;
    }

    long f = (long)__round(t0);

    if (f >= whole) {
        l++;
       f -= whole;
    }

    format_numeric(out, l);
    out += '.';
    format_numeric(out, f, 10, round);

    return out;
}


// Тут порядок struct fmt и format() важен почему-то

template <typename T> struct format_;

template <> struct format_<short> 
{ std::string& operator()(std::string& out, short t) { return format_numeric(out, t); } };

template <> struct format_<unsigned short> 
{ std::string& operator()(std::string& out, unsigned short t) { return format_numeric(out, t); } };

template <> struct format_<int> 
{ std::string& operator()(std::string& out, int t) { return format_numeric(out, t); } };

template <> struct format_<unsigned int> 
{ std::string& operator()(std::string& out, unsigned int t) { return format_numeric(out, t); } };

template <> struct format_<long> 
{ std::string& operator()(std::string& out, long t) { return format_numeric(out, t); } };

template <> struct format_<unsigned long> 
{ std::string& operator()(std::string& out, unsigned long t) { return format_numeric(out, t); } };

template <> struct format_<long long> 
{ std::string& operator()(std::string& out, long long t) { return format_numeric(out, t); } };

template <> struct format_<unsigned long long> 
{ std::string& operator()(std::string& out, unsigned long long t) { return format_numeric(out, t); } };

template <> struct format_<std::string> 
{ std::string& operator()(std::string& out, const std::string& t) { out += t; return out; } };

template <> struct format_<char*> 
{ std::string& operator()(std::string& out, const char* t) { out += t; return out; } };

template <> struct format_<const char*> 
{ std::string& operator()(std::string& out, const char* t) { out += t; return out; } };

template <> struct format_<char> 
{ std::string& operator()(std::string& out, char t) { out += t; return out; } };

template <> struct format_<unsigned char> 
{ std::string& operator()(std::string& out, unsigned char t) { out += t; return out; } };

template <> struct format_<bool> 
{ std::string& operator()(std::string& out, bool t) { out += (t ? 'T' : 'F'); return out; } };

template <> struct format_<float> 
{ std::string& operator()(std::string& out, float t) { return format_real(out, t, "%.*g"); } };

template <> struct format_<double> 
{ std::string& operator()(std::string& out, double t) { return format_real(out, t, "%.*g"); } };

template <> struct format_<long double> 
{ std::string& operator()(std::string& out, long double t) { return format_real(out, t, "%.*lg"); } };


template <typename T>
inline std::string format (T const &t) {
    std::string tmp; 
    format_<typename std::decay<T>::type>()(tmp, t);
    return tmp;
}

template <typename T>
inline std::string& format (std::string& out, T const &t) {
    return format_<typename std::decay<T>::type>()(out, t);
}

struct fmt {
    std::string data;

    template <typename T> 
    fmt& operator<<(const T& t) { 
	format(data, t); 
	return *this; 
    }
};


struct fmt_out {
    std::string& data;

    fmt_out(std::string& out) : data(out) {}

    template <typename T> 
    fmt_out& operator<<(const T& t) { 
	format(data, t); 
	return *this; 
    }
};

template <typename T>
inline std::string& format_legacy_cpp(std::string& out, const T& t) {
    std::stringstream s;
    s << t;
    out += s.str();
    return out;
}


template <typename T>
inline std::string pad(const T& t, size_t len) {
    std::string buf;
    format(buf, t);

    if (buf.size() < len) {
	buf = std::string(len-buf.size(), ' ') + buf;
    }

    return buf;
}


}

#endif
