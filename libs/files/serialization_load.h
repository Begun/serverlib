#ifndef __FILES_SERIALIZATION_LOAD_H
#define __FILES_SERIALIZATION_LOAD_H

#include "files/serialization_save.h"
#include "files/files_scan.h"

#include "util/type_to_typename.h"

#include <unordered_set>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <vector>
#include <queue>
#include <list>
#include <map>
#include <set>



namespace serialization {


template <typename T, typename B> struct loader;

template <typename T, typename B> struct load_;


/******  Теперь пользовательский интерфейс. ***/

template <typename T, typename B>
inline void load(B buf, T& t) {
    load_<T,B>()(buf, t);
}


#define DEFINE_LOADER(B,T) template <typename B> struct loader<T,B> { inline void operator()

#define DEFINE_LOADER1(B,T,T1) template <typename B, typename T1> struct loader<T<T1>,B> { inline void operator()

#define DEFINE_LOADER2(B,T,T1,T2) template <typename B, typename T1, typename T2> \
    struct loader<T<T1,T2>,B> { inline void operator()

#define DONE }


/************* Типы без версий. ***/


#define LOAD_NUMERIC(T) \
    template <typename B> struct load_<T,B> { \
	inline void operator()(B buf, T& t) { unsigned char la; buf >> la; files::scan_numeric(buf, t, la, "\n", 1); } }

LOAD_NUMERIC(short);
LOAD_NUMERIC(unsigned short);
LOAD_NUMERIC(int);
LOAD_NUMERIC(unsigned int);
LOAD_NUMERIC(long);
LOAD_NUMERIC(unsigned long);
LOAD_NUMERIC(long long);
LOAD_NUMERIC(unsigned long long);

#undef LOAD_NUMERIC

namespace {
inline void __check(unsigned char la, unsigned char terminator) {
    if (la != terminator) {
	throw std::runtime_error((files::fmt() << "error while scanning: got " 
				  << (int)la << ", wanted " << (int)terminator).data);
    }
}
}


template <typename B> struct load_<unsigned char,B> { 
    inline void operator()(B buf, unsigned char& t) { buf >> t; unsigned char tmp; buf >> tmp; __check(tmp, '\n'); } };

template <typename B> struct load_<char,B> { 
    inline void operator()(B buf, char& t) { unsigned char tmp; buf >> tmp; t = (char)tmp; buf >> tmp; 
	__check(tmp, '\n'); } };

template <typename B> struct load_<bool,B> { 
    inline void operator()(B buf, bool& t) { 
	unsigned char tmp; buf >> tmp; t = (tmp == 'T' ? true : false); buf >> tmp; __check(tmp, '\n'); }
};

template <typename B> struct load_<double,B> { 
    inline void operator()(B buf, double& t) { unsigned char la; buf >> la; 
	files::scan_real(buf, t, la, "\n", 1, strtod); } };

template <typename B> struct load_<float,B> { 
    inline void operator()(B buf, float& t) { unsigned char la; buf >> la; 
	files::scan_real(buf, t, la, "\n", 1, strtof); } };

template <typename B> struct load_<long double,B> { 
    inline void operator()(B buf, long double& t) { 
	unsigned char la; buf >> la; files::scan_real(buf, t, la, "\n", 1, strtold); } };



template <typename B, typename A, typename C> struct load_<std::pair<A,C>,B> {
    inline void operator()(B buf, std::pair<A,C>& t) {
	load(buf, t.first);
	load(buf, t.second);
    }
};


/***************** Типы с версиями. ***/


// Эта функция нужна сугубо для удобства -- когда хочется только проверить, что 
// версия подгруженого типа равна текущей известной версии.

// TODO: Подключить c++ abi.

template <typename T>
inline void check_version(T& t, unsigned int version) {
    if (version != type_version<T>()(t)) {
	std::string err = "serialization::load : ";
	err += util::type_to_typename<T>();
	err += " : got wrong type: ";
	files::format(err, version);
	err += " wanted ";
	files::format(err, type_version<T>()(t));

	throw std::runtime_error(err);
    }
}


template <typename B> struct loader<std::string,B> {
    inline void operator()(B buf, std::string& t, unsigned int version) { 
	check_version(t, version);

	size_t sz; 
	load(buf, sz);
        files::scan_string(buf, t, sz);
    }
};


template <typename B,typename T>
inline void load_stl_(B buf, T& t, unsigned int version) {
    check_version(t, version);

    size_t sz;
    load(buf, sz);
    t.clear();

    while (sz != 0) {
	typename T::value_type tmp;
	load(buf, tmp);
	t.insert(t.end(), tmp);
	sz--;
    }
}


// У map и hash_map особенность -- их значения имеют тип pair<const K,V>.
// Поэтому нужен особый код обработки вот этого const.

template <typename B,typename T>
inline void load_stl_map_(B buf, T& t, unsigned int version) {
    check_version(t, version);

    size_t sz = 0;
    load(buf, sz);
    t.clear();

    while (sz != 0) {
	std::pair<typename T::key_type, typename T::mapped_type> tmp;
	load(buf, tmp);
	t.insert(tmp);
	sz--;
    }
}


template <typename B, typename T, size_t  N> struct loader<T [N],B> {
    inline void operator()(B buf, T (&t)[N], unsigned int version) {
        for(auto i = 0; i < N; ++i) {
            load(buf, t[i]);
        }
    }
};

template <typename B, typename T, size_t N> struct loader<std::array<T,N>,B> {
    inline void operator()(B& buf, std::array<T,N>& t, unsigned int version) {
        for(auto& v : t) {
            load(buf, v);
        }
    }
};

template <typename B, typename T, typename A> struct loader<std::vector<T,A>,B> {
    inline void operator()(B buf, std::vector<T,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename T, typename L, typename A> struct loader<std::set<T,L,A>,B> {
    inline void operator()(B buf, std::set<T,L,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename T, typename L, typename A> struct loader<std::unordered_set<T,L,A>,B> {
    inline void operator()(B buf, std::unordered_set<T,L,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename T, typename L, typename A> struct loader<std::multiset<T,L,A>,B> {
    inline void operator()(B buf, std::multiset<T,L,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename K, typename V, typename L, typename A> struct loader<std::multimap<K,V,L,A>,B> {
    inline void operator()(B buf, std::multimap<K,V,L,A>& t, unsigned int version) { load_stl_map_(buf, t, version); }
};

template <typename B, typename T, typename A> struct loader<std::list<T,A>,B> {
    inline void operator()(B buf, std::list<T,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename T, typename A> struct loader<std::deque<T,A>,B> {
    inline void operator()(B buf, std::deque<T,A>& t, unsigned int version) { load_stl_(buf, t, version); }
};

template <typename B, typename K, typename V, typename L, typename A> struct loader<std::map<K,V,L,A>,B> {
    inline void operator()(B buf, std::map<K,V,L,A>& t, unsigned int version) { load_stl_map_(buf, t, version); }
};

template <typename B, typename K, typename V, typename E, typename L, typename A> struct loader<std::unordered_map<K,V,E,L,A>,B> {
    inline void operator()(B buf, std::unordered_map<K,V,E,L,A>& t, unsigned int version) { load_stl_map_(buf, t, version); }
};



// Обертка...

template <typename T,typename B>
struct load_ {
    inline void operator()(B buf, T& t) {
	unsigned int version;
	load(buf, version);
	loader<T,B>()(buf, t, version);
    }
};


}


#endif
