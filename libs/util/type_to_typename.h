#ifndef __UTIL_TYPE_TO_TYPENAME_H
#define __UTIL_TYPE_TO_TYPENAME_H


#include <typeinfo>
#include <cxxabi.h>

#include <boost/algorithm/string/erase.hpp>


namespace util {


// ������������� ��� ���� �������� �������.
template <typename T>
std::string type_to_typename() {
    char const* mangled_name = typeid(T).name();
    int status;
    char* name = abi::__cxa_demangle(mangled_name, 0, 0, &status);
    std::string result(name ? name : mangled_name);
    std::free(name);
    return result;
}


// ���, ����� � ��������.
// ��������� ������ ������ ��������� ��������� �� ����� ����.
// ���� �� g++ ��� �����, �� � ������ ����� ������������� �������� ��������� ���������� �� ������������ ��.
// � ��� ���, �� ���������� ������� ��� ������������� �������� ������ ������������ �����������. 

template <typename T>
std::string type_to_typename_2() {
    std::string t0 = type_to_typename<T>();

    static const char* bad_types__[] = { 
        "boost::detail::variant::void_", 
        NULL };

    for (const char** c = bad_types__; *c != NULL; c++) {

        boost::algorithm::erase_all(t0, *c);
    }

    boost::algorithm::erase_all(t0, " , ");
    boost::algorithm::erase_all(t0, ",,");

    return t0;
}

inline std::string typeid_to_typename(const std::type_info& t) {
    char const* mangled_name = t.name();
    int status;
    char* name = abi::__cxa_demangle(mangled_name, 0, 0, &status);
    std::string result(name ? name : mangled_name);
    std::free(name);
    return result;
}    


}


#endif
