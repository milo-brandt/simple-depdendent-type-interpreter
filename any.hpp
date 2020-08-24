#ifndef any_h
#define any_h

#include <concepts>
#include <iostream>
#include <memory>
#include <exception>

namespace utility{
    template<class T>
    concept can_insert_to_ostream = requires(std::ostream& o, T const& a){
        o << a;
    };
    template<class T>
    concept printable_regular = std::regular<T> && can_insert_to_ostream<T>;

    class printable_any{
        struct __impl_base{
            static std::size_t get_next_type_index();
            template<printable_regular T>
            static std::size_t type_index_for(){
                static std::size_t index = get_next_type_index();
                return index;
            }
            virtual std::size_t get_type_index() const = 0;
            virtual bool compare_to(__impl_base*) const = 0; //only called after seeing that both types are equal
            virtual void print_to(std::ostream&) const = 0;
            virtual std::unique_ptr<__impl_base> copy() const = 0;
            virtual ~__impl_base() = default;
        };
        template<printable_regular T>
        struct __impl_specific final : __impl_base{
            T value;
            __impl_specific(T&& value):value(std::move(value)){}
            __impl_specific(T const& value):value(value){}
            std::size_t get_type_index() const override{
                return __impl_base::type_index_for<T>();
            }
            bool compare_to(__impl_base* other) const override{
                return ((__impl_specific<T>*)other)->value == value;
            }
            void print_to(std::ostream& o) const override{
                o << value;
            }
            std::unique_ptr<__impl_base> copy() const override{
                return std::make_unique<__impl_specific>(value);
            }
        };
        std::unique_ptr<__impl_base> data;
    public:
        printable_any(){}
        template<printable_regular T>
        explicit printable_any(T const& value):data(new __impl_specific<T>(value)){}
        template<printable_regular T>
        explicit printable_any(T&& value):data(new __impl_specific<T>(std::move(value))){}
        printable_any(printable_any const&);
        printable_any(printable_any&&) = default;
        printable_any(std::nullptr_t){}
        ~printable_any() = default;

        template<printable_regular T>
        bool contains_type() const{
            if(!data) return false;
            return __impl_base::type_index_for<T>() == data->get_type_index();
        }
        template<printable_regular T>
        T& as(){
            if(!contains_type<T>) throw std::runtime_error("bad any cast");
            return ((__impl_specific<T>*)data)->value;
        }
        template<printable_regular T>
        T const& as() const{
            if(!contains_type<T>) throw std::runtime_error("bad any cast");
            return ((__impl_specific<T>*)data)->value;
        }
        friend bool operator==(printable_any const& a, printable_any const& b);
        friend std::ostream& operator<<(std::ostream&, printable_any const& b);
    };
    bool operator==(printable_any const& a, printable_any const& b);
    std::ostream& operator<<(std::ostream&, printable_any const& b);
}
#endif
