#include "any.hpp"

namespace utility{
    printable_any::printable_any(printable_any const& o):data(o.data?nullptr:o.data->copy()){}
    std::size_t printable_any::__impl_base::get_next_type_index(){
        static std::size_t counter = 0;
        return counter++;
    }
    bool operator==(printable_any const& a, printable_any const& b){
        if(a.data == nullptr) return b.data == nullptr;
        if(b.data == nullptr) return false;
        if(a.data->get_type_index() != b.data->get_type_index()) return false;
        return a.data->compare_to(b.data.get());
    }
    std::ostream& operator<<(std::ostream& o, printable_any const& b){
        if(!b.data) return o << "(no value)";
        else{
            b.data->print_to(o);
            return o;
        }
    }
}
