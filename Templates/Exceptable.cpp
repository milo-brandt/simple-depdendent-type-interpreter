#include "Exceptable.h"
#include <sstream>
const char* tuple_exception::what() const noexcept{
    try{
        std::stringstream stream;
        stream << "Multiple errors: ";
        bool first = true;
        for(auto const& ptr : components){
            if(first) first = false;
            else stream << " and ";
            stream << "(";
            try{
                std::rethrow_exception(ptr);
            }catch(std::exception& except){
                stream << except.what();
            }catch(...){
                stream << "unknown exception";
            }
            stream << ")";
        }
        *str = stream.str();
        return str->c_str();
    }catch(...){
        return "Multiple errors. Concatenation failed.";
    }
}
