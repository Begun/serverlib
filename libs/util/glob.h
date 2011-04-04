#ifndef __UTIL_GLOB_H
#define __UTIL_GLOB_H

#include <sys/stat.h>
#include <glob.h>

namespace util {


namespace {

struct glob_raii {

    glob_t gl;

    ~glob_raii() {
        ::globfree(&gl);
    }
};

}

inline void glob(const std::string& path, std::vector<std::string>& out, bool do_error = true) {

    glob_raii gl;

    int tmp = ::glob(path.c_str(), GLOB_ERR, NULL, &(gl.gl));

    out.clear();

    if (tmp != 0 && tmp != GLOB_NOMATCH) {

        if (do_error) {
            throw std::runtime_error("util::glob: Failed to glob index files in " + path);
        } else {
            return;
        }
    }

    for (size_t i = 0; i < gl.gl.gl_pathc; ++i) {

        std::string file(gl.gl.gl_pathv[i]);
        out.push_back(file);
    }

    //std::sort(out.begin(), out.end());
}

}

#endif
