#ifndef __SERIALIZATION_SHARED_PTR_H
#define __SERIALIZATION_SHARED_PTR_H


#include "files/serialization.h"


// Считаем, что shared_ptr всегда хранится в сериализуемых структурах в единственном экземпляре!
// Поддержим boost::shared_ptr<const T>, хотя это и не совсем правильно.

namespace serialization {


TYPE_VERSION1(boost::shared_ptr, T)(boost::shared_ptr<T> t) { 
    if (!t) {
        return 1;
    } else {
        return 0; 
    }
} 
DONE;

DEFINE_SAVER1(B, boost::shared_ptr, T)(B& buf, const boost::shared_ptr<T> t) {
    if (t) {
        save(buf, *t);
    }
}
DONE;

DEFINE_LOADER1(B, boost::shared_ptr, T)(B buf, boost::shared_ptr<T>& t, unsigned int version) {

    if (version > 1)
        throw std::runtime_error("error in deserializing shared_ptr: supported versions are 0 and 1.");

    if (version == 0) {
        t = boost::shared_ptr<T>(new T);
        load(buf, *t);
    } else {
        t.reset();
    }
}
DONE;



template <typename T> struct type_version<boost::shared_ptr<const T> > { 
    inline unsigned int operator()(boost::shared_ptr<const T> t) {
	return 0;
    }
};

template <typename B,typename T> struct saver<boost::shared_ptr<const T>,B> { 
    inline void operator()(B& s, boost::shared_ptr<const T> t) {
	save(s, *t);
    }
};

template <typename B, typename T> struct loader<boost::shared_ptr<const T>,B> { 
    inline void operator()(B buf, boost::shared_ptr<const T>& t, unsigned int version) {
	check_version(t, version);
	t = boost::shared_ptr<const T>(new T);
	load(buf, const_cast<T&>(*t));
    }
};


}

#endif

