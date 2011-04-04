#ifndef __SERIALIZATION_VARIANT_H
#define __SERIALIZATION_VARIANT_H


#include <boost/variant.hpp>

#include <boost/mpl/identity.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/empty.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/int.hpp>

#include "serialization.h"

#include <boost/preprocessor/repetition/repeat.hpp>



// Жесть. Некрасиво. (Но работать должно.)

// Вариант нужно обязательно объявлять через make_variant_over!!
//
// Вот так, например:
//
// typedef boost::make_variant_over<
//    boost::mpl::vector<long, double, std::string>
//    >::type value_t;



namespace serialization {


namespace {

template <typename B>
struct __variant_save_visitor : boost::static_visitor<> {
    B& buf;
    __variant_save_visitor(B& b) : buf(b) {}

    template <typename T>
    void operator()(const T& v) const {
        save(buf, v);
    }

    void operator()(const boost::blank& v) const {
    }

};



template <typename T>
struct __init {
    template <typename VAR, typename BUF>
    void operator()(BUF& buf, VAR& v) {

        v = T();

        load(buf, boost::get<T>(v));
    }
};

template <>
struct __init<mpl_::void_> {
    template <typename VAR, typename BUF>
    void operator()(BUF& buf, VAR& v) {}
};

template <>
struct __init<boost::blank> {
    template <typename VAR, typename BUF>
    void operator()(BUF& buf, VAR& v) {}
};


template <typename VEC, typename BUF, typename VAR>
void __dynint_to_statint(int n, BUF& buf, VAR& v) {

#define _N(Z,N,A)  case N: \
    __init<typename boost::mpl::at<VEC, boost::mpl::int_<N> >::type>()(buf, v); break;


    switch (n) {

        BOOST_PP_REPEAT(100, _N, 0);

    default:
        throw std::runtime_error("Large index is usupported in loading variant: " + (files::fmt() << n).data); 
        break;
    }

#undef _N

}

}



template <BOOST_VARIANT_ENUM_PARAMS(typename T)> 
struct type_version<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> > { 
    inline unsigned int operator()(const boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& t) {
        return t.which();
    }
};

template <typename B, BOOST_VARIANT_ENUM_PARAMS(typename T)> 
struct saver<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, B> { 
    inline void operator()(B& buf, const boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& t) {

        __variant_save_visitor<B> v(buf);
        t.apply_visitor(v);
    }
};


template <typename B, BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct loader<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, B> {
    inline void operator()(B buf, boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& t, unsigned int version) {

        typedef typename boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>::types types;

        if (version >= boost::mpl::size<types>::value) {
            throw std::runtime_error("Got unsupported boost::variant type index.");
        }

        __dynint_to_statint<types>(version, buf, t);

    }
};

}




#endif
