#include "FileUtility.h"

namespace file_utility{
    RawBuffer readFile(std::string const& filename){
        std::ifstream file(filename);
        if(!file) return {};
        file.seekg(0, std::ios::end);
        auto end_p = file.tellg();
        file.seekg(0);
        auto start_p = file.tellg();
        auto len = end_p - start_p;
        RawBuffer ret(len);
        file.read((char*)ret.data, len);
        return ret;
    }
}
