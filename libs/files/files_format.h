#ifndef __FILES_FILES_FORMAT_H
#define __FILES_FILES_FORMAT_H

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

template <typename T>
inline std::string& format_real(std::string& out, T t, const char* /*fmt_*/, int round = 5) {

    static const char* base10 = "0123456789";

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

    int ro0 = 0;
    char buff[65];

    while (ro0 <= round) {
        t0 *= 10;

        long tmp = (long)t0;

        buff[ro0] = base10[tmp];
        t0 -= tmp;
        ro0++;
    }

    bool carry = false;
    for (int i = round; i >= 0; i--) {
        if (buff[i] < '9') {
            if (buff[i+1] >= '5' || carry) {
                buff[i]++;
            }
            carry = false;
            break;
        } else {
            buff[i] = '0';
            carry = true;
        }
    }    

    if (carry) 
        l++;

    format_numeric(out, l);
    out += '.';

    for (int i = 0; i < round; i++) {
        out += buff[i];
    }

    return out;
}


// Тут порядок struct fmt и format() важен почему-то


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


inline std::string& format(std::string& out, short t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, unsigned short t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, int t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, unsigned int t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, long t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, unsigned long t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, long long t) { return format_numeric(out, t); }
inline std::string& format(std::string& out, unsigned long long t) { return format_numeric(out, t); }

inline std::string& format(std::string& out, const std::string& t) { out += t; return out; }
inline std::string& format(std::string& out, const char* t) { out += t; return out; }
inline std::string& format(std::string& out, char* t) { out += t; return out; }
inline std::string& format(std::string& out, char t) { out += t; return out; }
inline std::string& format(std::string& out, unsigned char t) { out += t; return out; }

inline std::string& format(std::string& out, bool t) { out += (t ? 'T' : 'F'); return out; }

inline std::string& format(std::string& out, float t) { return format_real(out, t, "%.*g"); }
inline std::string& format(std::string& out, double t) { return format_real(out, t, "%.*g"); }
inline std::string& format(std::string& out, long double t) { return format_real(out, t, "%.*lg"); }

template <typename T>
inline std::string format (T const &t) {std::string tmp; format (tmp, t); return tmp;}


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
