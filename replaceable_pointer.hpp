#include <memory>
#include <exception>

namespace utility{
    template<class T>
    class replaceable_pointer{
      struct sign_post{
        std::variant<std::shared_ptr<sign_post>, std::shared_ptr<T> > data;
        std::shared_ptr<T> read(){
          if(std::holds_alternative<std::shared_ptr<sign_post> >(data)){
            auto ret = std::get<std::shared_ptr<sign_post> >(data)->read();
            data = ret;
            return ret;
          }else{
            return std::get<std::shared_ptr<T> >(data);
          }
        }
      };
      std::shared_ptr<sign_post> stored;
    public:
      replaceable_pointer() = default;
      replaceable_pointer(std::nullptr_t){}
      explicit replaceable_pointer(std::shared_ptr<T> value):stored(std::make_shared<sign_post>(std::move(value))){}
      explicit replaceable_pointer(T* value):replaceable_pointer(std::shared_ptr<T>(value)){}
      operator bool() const{ return stored != nullptr; }
      T& operator*() const{ return *stored->read(); }
      T* get() const{ return stored->read().get(); }
      T* operator->() const{ return get(); }
      void forward_to(replaceable_pointer const& target){
        std::shared_ptr<sign_post>* ptr = &target.stored;
        if(!*ptr) throw std::runtime_error("tried to forward to nullptr");
        while(auto next_ptr = std::get_if<std::shared_ptr<sign_post> >(&(*ptr)->data)) ptr = next_ptr;

        stored->data = target.stored;
        stored = target.stored;
      }
      struct strict_hasher{
        std::size_t operator()(replaceable_pointer const& p) const{
          return std::hash<std::shared_ptr<T> >()(p.stored->read());
        }
      };
      struct strict_equality{
        bool operator()(replaceable_pointer const& a, replaceable_pointer const& b) const{
          return a.get() == b.get();
        }
      };
    };
    template<class T, class... Args>
    replaceable_pointer<T> make_replaceable(Args&&... args){
        return replaceable_pointer<T>{std::make_shared<T>(std::forward<Args>(args)...)};
    }
}


//x^2 = -1
//(i + x)(i - x) = 0
//-> find common factor of i + x and p
