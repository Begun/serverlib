#ifndef __FILES_SERIALIZATION_SAVE_H
#define __FILES_SERIALIZATION_SAVE_H



// Теперь сериализация структур в текстовый буффер.

// Заметим, что сериализация ВЕЗДЕ где можно явно проверяет и пишет номер версии.



#include <boost/noncopyable.hpp>

#include "files/files_format.h"

#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <utility>
#include <string>
#include <vector>
#include <queue>
#include <list>
#include <map>
#include <set>


namespace serialization {


template <typename T> struct type_version;
template <typename T,typename B> struct saver;
template <typename T,typename B> struct save_;


/******  Теперь пользовательский интерфейс. ***/

// Буферизация сериализованного.
// Имеет параметр -- размер буфера.
// Если он 0 -- то каждый атомарный элемент попадет сразу на диск (в сеть);
// Если он MAX_INT -- то попадет вся структура целиком.
// Если N -- то попадут частями размерами не более N.

template <typename B>
struct saver_policy : public boost::noncopyable {

public:

    B m_output;
    std::string data;
    const size_t max_buf_size;

    saver_policy(B b, size_t m, bool profile = false) : m_output(b), max_buf_size(m) {}

    inline void check() {
	if (data.size() >= max_buf_size) {
            flush();
	}
    }

    ~saver_policy() {
        try {
            flush();
        } catch (...) {
            // Nothing
        }
    }

    inline void flush() {
        if (data.size() > 0) {
            m_output << data;
        }

        data.clear();
    }
};


//
// Сериализация только в строку.
//

struct saver_string_policy {

    std::string data;

    inline void check() {}
    inline void flush() {}
};


//
// Сериализация в существующую строку
//

struct saver_string_out_policy {

    std::string& data;

    saver_string_out_policy(std::string& b) : data(b) {}

    inline void check() {}
    inline void flush() {}
};



template <typename T, typename B>
inline void save(B& buf, const T& t) {
    save_<T,B>()(buf, t);
}


#define TYPE_VERSION(T) template <> struct type_version<T > { inline unsigned int operator()

#define TYPE_VERSION1(T,T1) template <typename T1> struct type_version<T<T1> > { inline unsigned int operator()

#define TYPE_VERSION2(T,T1,T2) template <typename T1,typename T2> \
    struct type_version<T<T1,T2> > { inline unsigned int operator()



#define DEFINE_SAVER(B,T) template <typename B> struct saver<T,B > { inline void operator()

#define DEFINE_SAVER1(B,T,T1) template <typename B, typename T1> struct saver<T<T1>,B > { inline void operator()

#define DEFINE_SAVER2(B,T,T1,T2) template <typename B, typename T1,typename T2> \
    struct saver<T<T1,T2>,B > { inline void operator()





/****** Однозначные типы: без версионирования. **/

#define SAVE_NUMERIC(T) \
    template <typename B> struct save_<T,B> { \
	inline void operator()(B& buf, const T t) { \
            files::format(buf.data, t); files::format(buf.data, '\n'); buf.check(); } }

SAVE_NUMERIC(short);
SAVE_NUMERIC(unsigned short);
SAVE_NUMERIC(int);
SAVE_NUMERIC(unsigned int);
SAVE_NUMERIC(long);
SAVE_NUMERIC(unsigned long);
SAVE_NUMERIC(long long);
SAVE_NUMERIC(unsigned long long);
SAVE_NUMERIC(char);
SAVE_NUMERIC(unsigned char);
SAVE_NUMERIC(bool);
SAVE_NUMERIC(double);
SAVE_NUMERIC(float);
SAVE_NUMERIC(long double);

#undef SAVE_NUMERIC

template <typename BU,typename A,typename B>
struct save_<std::pair<A,B>, BU> {
    inline void operator()(BU& buf, const std::pair<A,B>& t) {
	save(buf, t.first);
	save(buf, t.second);
    }
};



/***** Версионируемые типы. **/


// Сохранение строк.


template <> struct type_version<std::string> {
    inline unsigned int operator()(const std::string& t) {
	return 0;
    }
};

template <typename B> struct saver<std::string, B> {
    inline void operator()(B& buf, const std::string& t) {
	save(buf, t.size());
	buf.data += t;
	buf.check();
    }
};

template <> struct type_version<char*> {
    inline unsigned int operator()(const char* t) { return 0; }
};

template <typename B> struct saver<char*,B> {
    inline void operator()(B& buf, const char* t) {
	size_t len = ::strlen(t);
	save(buf, len);
	buf.data += t;
	buf.check();
    }
};


// Сохранение некоторых стандартных структур.

template <typename T,typename B>
inline void save_stl_(B& buf, const T& t) {
    save(buf, t.size());
    for (typename T::const_iterator i = t.begin(); i != t.end(); ++i) {
	save(buf, *i);
    }
}


template <typename T, size_t N> struct type_version<T [N]> {
    inline unsigned int operator()(const T (&t)[N]) { return 0; } };

template <typename T, size_t N> struct type_version<std::array<T,N> > {
     inline unsigned int operator()(const std::array<T,N>& ) { return 0; } };

template <typename T, typename A> struct type_version<std::vector<T,A> > {
    inline unsigned int operator()(const std::vector<T>& t) { return 0; } };

template <typename T, typename L, typename A> struct type_version<std::set<T,L,A> > {
    inline unsigned int operator()(const std::set<T,L,A>& t) { return 0; } };

template <typename T, typename L, typename A> struct type_version<std::unordered_set<T,L,A> > {
    inline unsigned int operator()(const std::unordered_set<T,L,A>& t) { return 0; } };

template <typename T, typename L, typename A> struct type_version<std::multiset<T,L,A> > {
    inline unsigned int operator()(const std::multiset<T,L,A>& t) { return 0; } };

template <typename K,typename V, typename L, typename A> struct type_version<std::multimap<K,V,L,A> > {
    inline unsigned int operator()(const std::multimap<K,V,L,A>& t) { return 0; } };

template <typename T, typename A> struct type_version<std::list<T,A> > {
    inline unsigned int operator()(const std::list<T,A>& t) { return 0; } };

template <typename T, typename A> struct type_version<std::deque<T,A> > {
    inline unsigned int operator()(const std::deque<T,A>& t) { return 0; } };

template <typename K,typename V, typename L, typename A> struct type_version<std::map<K,V,L,A> > {
    inline unsigned int operator()(const std::map<K,V,L,A>& t) { return 0; } };

template <typename K,typename V, typename L, typename A> struct type_version<std::unordered_map<K,V,L,A> > {
    inline unsigned int operator()(const std::unordered_map<K,V,L,A>& t) { return 0; } };


template <typename T, typename B, size_t N> struct saver<T [N], B> {
    inline void operator()(B& buf, const T (&t)[N]) {
        for(auto i = 0; i < N; ++i) {
            save(buf, t[i]);
        }
    }
};

template <typename T, size_t N, typename B> struct saver<std::array<T,N>,B > {
    inline void operator()(B& buf, const std::array<T,N>& t) {
        for(auto& v : t) {
            save(buf, v);
        }
    }
};

template <typename T, typename A, typename B> struct saver<std::vector<T,A>,B > {
    inline void operator()(B& buf, const std::vector<T,A>& t) { save_stl_(buf, t); } };

template <typename T,typename L, typename A,typename B> struct saver<std::set<T,L,A>,B > {
    inline void operator()(B& buf, const std::set<T,L,A>& t) { save_stl_(buf, t); } };

template <typename T,typename L, typename A,typename B> struct saver<std::unordered_set<T,L,A>,B > {
    inline void operator()(B& buf, const std::unordered_set<T,L,A>& t) { save_stl_(buf, t); } };

template <typename T,typename L, typename A, typename B> struct saver<std::multiset<T,L,A>,B > {
    inline void operator()(B& buf, const std::multiset<T,L,A>& t) { save_stl_(buf, t); } };

template <typename K,typename V,typename L, typename A,typename B> struct saver<std::multimap<K,V,L,A>,B > {
    inline void operator()(B& buf, const std::multimap<K,V,L,A>& t) { save_stl_(buf, t); } };

template <typename T,typename A,typename B> struct saver<std::list<T,A>,B > {
    inline void operator()(B& buf, const std::list<T,A>& t) { save_stl_(buf, t); } };

template <typename T,typename A,typename B> struct saver<std::deque<T,A>,B > {
    inline void operator()(B& buf, const std::deque<T,A>& t) { save_stl_(buf, t); } };

template <typename K,typename V,typename L, typename A,typename B> struct saver<std::map<K,V,L,A>,B > {
    inline void operator()(B& buf, const std::map<K,V,L,A>& t) { save_stl_(buf, t); } };

template <typename K,typename V,typename E, typename L, typename A,typename B> struct saver<std::tr1::unordered_map<K,V,E,L,A>,B > {
    inline void operator()(B& buf, const std::tr1::unordered_map<K,V,E,L,A>& t) { save_stl_(buf, t); } };

template <typename K,typename V,typename E, typename L, typename A,typename B> struct saver<std::unordered_map<K,V,E,L,A>,B > {
    inline void operator()(B& buf, const std::unordered_map<K,V,E,L,A>& t) { save_stl_(buf, t); } };


// Обертка...

template <typename T,typename B>
struct save_ {

    inline void operator()(B& buf, const T& t) {
	save(buf, type_version<T>()(t));
	saver<T,B>()(buf, t);
    }
};



}


#endif
