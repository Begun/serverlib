#ifndef __FILES_FILES_SCAN_H
#define __FILES_FILES_SCAN_H

#include "files/files_base.h"

#include <boost/shared_ptr.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace files {

/**
 * Обратная операция от files::format.
 * Работает над буферами.
 * Тащит за собой 1 байт в качестве look-ahead буфера.
 */


/**
 * Псевдо-буфер для чтения из строки. 
 * (Запись в строку не поддерживается -- это тупость и тавтология. Не поддерживается для того, чтобы строка была const.)
 * Внимание: _время жизни такое же, как и сама строка_.
 */


struct string_as_buffer {

private:
    
    const std::string& s;
    boost::shared_ptr<size_t> pos;
    int terminator;

public:

    string_as_buffer(const std::string& s_, int term = -1) : s(s_), pos(new size_t(0)), terminator(term) {}

    string_as_buffer& operator>>(unsigned char& out) {

        if (terminator >= 0 && *pos == s.size()) {
            out = (unsigned char)terminator;
            (*pos)++;
            return *this;
        }

        if (*pos >= s.size()) {
            throw eof_exception();
        }

        out = s[*pos];

        (*pos)++;
        return *this;
    }

    string_as_buffer& operator>>(std::vector<unsigned char>& out) {

        if (terminator >= 0 && s.size() - *pos == out.size()) {
            std::copy(s.begin() + *pos, s.begin() + *pos + out.size() - 1, out.begin());
            out.back() = (unsigned char)terminator;
            (*pos) += out.size() + 1;
            return *this;
        }

        if (s.size() - *pos < out.size()) {
            throw eof_exception();
        }

        std::copy(s.begin() + *pos, s.begin() + *pos + out.size(), out.begin());
        (*pos) += out.size();
        return *this;
    }

    bool optional_read(unsigned char match) {

        if (terminator >= 0 && *pos == s.size()) {
            if (match == terminator) {
                (*pos)++;
                return true;
            } else {
                return false;
            }
        }

        if (*pos >= s.size()) {
            throw eof_exception();
        }

        if (s[*pos] == match) {
            (*pos)++;
            return true;
        }

        return false;
    }

    // Not supported!
    //string_as_buffer& operator<<(const std::string& s);

    //inline void reset() { *pos = 0; }

    size_t bytes_scanned() const { 
        return *pos;
    }

    string_as_buffer& operator*()
    {
        return *this;
    }
    string_as_buffer* operator->()
    {
        return this;
    }
};


inline string_as_buffer string_to_buffer(const std::string& s, int term = -1) {
    return string_as_buffer(s, term);
}


/// /// /// ///


struct range_as_buffer {

private:
    
    std::string::const_iterator b;
    std::string::const_iterator e;

public:

    range_as_buffer(const std::string::const_iterator& _b, const std::string::const_iterator& _e) : 
        b(_b), e(_e) {}

    range_as_buffer& operator>>(unsigned char& out) {

        if (b == e) {
            throw eof_exception();
        }

        out = *b;

        ++b;
        return *this;
    }

    range_as_buffer& operator>>(std::vector<unsigned char>& out) {
        if ((size_t)(e - b) >= out.size()) {
            throw eof_exception();
        }

        std::copy(b, b + out.size(), out.begin());
        b += out.size();
    }
};


inline range_as_buffer range_to_buffer(const std::string::const_iterator& _b, const std::string::const_iterator& _e) {
    return range_as_buffer(_b, _e);
}


namespace {

bool _strchr(const char* s, int l, char c) {

    while (l > 0) {
        if (*s == c) return true;
        ++s;
        --l;
    }

    return false;
}

}



/**
 * Чтение целого числа.
 */

template <typename B,typename T> 
inline void scan_numeric(B buf, T& t, unsigned char& la, const char* terminator, int tlen, int base = 10) {

    /* Отрицательное число. */
    bool neg = false;

    if (la == '-') {
	buf >> la;
	neg = true;
    }

    t = 0;

    while (1) {
	int tmp;
	if (la >= '0' && la <= '9') {
	    tmp = la - '0';
	} else if (la >= 'A' && la <= 'Z') {
	    tmp = la - 'A';
	} else if (_strchr(terminator, tlen, la)) {
	    break;
	} else {
	    throw std::runtime_error((files::fmt() << "error while scanning for number: got " 
				      << (int)la << ", wanted " << terminator).data);
	}

	t *= base;
	t += tmp;

	buf >> la;
    }

    if (neg) t = -t;
}



// HACK.

template <typename B,typename T,typename F>
inline void scan_real(B buf, T& t, unsigned char& la, const char* terminator, int tlen, F f) {
    char buff[256];
    int i = 0;

    while (i < 255) {
	if ((la >= '0' && la <= '9') || (la >= 'a' && la <= 'z') || la == '-' || la == '+' || la == '.') {
	    buff[i] = la;
	    i++;

	    buf >> la;

	} else if (_strchr(terminator, tlen, la)) {
	    break;

	} else {
	    throw std::runtime_error((files::fmt() << "error while scanning for real: got " 
				      << (int)la << ", wanted " << terminator).data);
	}

    }

    buff[i] = '\0';

    t = f(buff, NULL);
}


template <typename B,typename T>
inline void scan_string(B buf, T& t, size_t size) {
    // Чуть-чуть неэффективно.
    t.clear();

    if (size > 0) {
        std::vector<unsigned char> tmp;
        tmp.resize(size);

        buf >> tmp;

        t.insert(t.end(), tmp.begin(), tmp.end());
    }
}


template <typename B,typename T>
inline void scan_string_2(B buf, T& t, size_t size) {

    unsigned char tmp;
    t.clear();

    for (size_t i = 0; i < size; ++i) {        
        buf >> tmp;
        t += tmp;
    }
}

template <typename B,typename T>
inline void scan_string_3(B buf, T& t, unsigned char& la, const char* terminator, int tlen) {

    t.clear();

    while (1) {

        if (_strchr(terminator, tlen, la)) break;

        t += la;
        buf >> la;
    }
}



/*
 *
 * А теперь набор удобных сканнеров для повседневного использования.
 *
 */


template <typename BUF>
struct scn {
    BUF b;
    unsigned char la;
    bool valid;

    scn(BUF b_) : b(b_), valid(false) {}

private:

    void _get() { 
        if (!valid) { 
            b >> la;
        }
    }

public:

    void check(const unsigned char t) { 

        _get();

        if (t != la) {
            throw std::runtime_error("Scanning error: got '" + std::string(1, la) + 
                                     "', wanted '" + std::string(1, t) + "'");
        }

        valid = false;
    }

    void check(const char* t) {
        while (*t != '\0') {
            check(*t);
            ++t;
        }
    }

    unsigned char check_any(const char* t, int tlen) {

        _get();

        if (_strchr(t, tlen, la)) {
            throw std::runtime_error("Scanning error: got '" + std::string(1, la) + "', wanted one of '" + std::string(t) + "'");
        }

        valid = false;
        return la;
    }

    unsigned char scan(std::string& t, const char* te, int tlen) {
        _get();
        scan_string_3(b, t, la, te, tlen); 
        valid = false;
        return la;
    }

    template <typename T>
    unsigned char scan(T& t, const char* te, int tlen) {
        _get();
        scan_numeric(b, t, la, te, tlen); 
        valid = false;
        return la;
    }

    unsigned char scan(double& t, const char* te, int tlen) {
        _get();
        scan_real(b, t, la, te, tlen, ::strtod);
        valid = false;
        return la;
    }

    unsigned char skip(const char* c, int tlen) {
        _get();

        while (1) {
            if (_strchr(c, tlen, la)) {
                b >> la;
            } else {
                break;
            }
        }

        valid = true;
        return la;
    }

    unsigned char skip_until(const char* c, int tlen) {
        _get();

        while (1) {
            if (_strchr(c, tlen, la)) {
                break;

            } else {
                b >> la;
            }
        }

        valid = false;
        return la;
    }

};



}




#endif
