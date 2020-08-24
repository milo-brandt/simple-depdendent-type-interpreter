#ifndef stream_coro_h
#define stream_coro_h

#include "Templates/coro_base.h"
#include <optional>
#include <variant>
#include <functional>
#include <memory>

namespace coro{
  struct read_from_stream_t{};
  constexpr read_from_stream_t read_from_stream;

  class generator_sentinel_t{};
  constexpr generator_sentinel_t generator_sentinel;

  template<class output_type>
  class generator{
  public:
    struct promise_type;
    bool done() const{
      return !impl.promise().out.has_value();
    }
    output_type take_value(){
      auto ret = std::move(*impl.promise().out);
      impl.resume_keep_ownership();
      return ret;
    }
  private:
    coro::unique_coro_handle<promise_type> impl;
    generator(std::coroutine_handle<promise_type> handle):impl(handle){}
  public:
    struct promise_type{
      std::optional<output_type> out;
      generator get_return_object(){
        return std::coroutine_handle<promise_type>::from_promise(*this);
      }
      template<class T>
      void await_transform(T) = delete;
      auto yield_value(output_type v){
        out = std::move(v);
        return std::suspend_always();
      }
      auto initial_suspend(){
          return std::suspend_never();
      }
      auto final_suspend(){
          return std::suspend_always();
      }
      void return_void(){}
      void unhandled_exception(){
        throw; //pass exception outwards
      }
    };
  };
  template<class output_type>
  bool operator==(generator<output_type> const& gen, generator_sentinel_t){ return gen.done(); }
  template<class output_type>
  bool operator!=(generator<output_type> const& gen, generator_sentinel_t){ return gen.done(); }

  template<class input_type, class return_type>
  class stream_function{
  public:
    struct promise_type;
    void advance(input_type t){
      impl.promise().advance(std::move(t));
    }
    bool done() const{
      return impl.promise().passed.index() == 2;
    }
    return_type take_value(){
      auto ret = std::move(std::get<2>(impl.promise().passed));
      impl = nullptr;
      return ret;
    }
    template<class iterator, class sentinel>
    std::optional<return_type> advance_range(iterator& it, sentinel last){
      while(it != last){
        advance(*it);
        ++it;
      }
      if(done()) return take_value();
      else return std::nullopt;
    }
    std::optional<return_type> advance_generator(generator<input_type>& g){
      while(!g.done()){
        advance(g.take_value());
      }
      if(done()) return take_value();
      else return std::nullopt;
    }
  private:
    coro::unique_coro_handle<promise_type> impl;
    stream_function(std::coroutine_handle<promise_type> handle):impl(handle){}
  public:
    struct promise_type{
      using passing_type = std::variant<std::monostate, input_type, return_type>;
      passing_type passed;
      std::function<bool(input_type)> accept_override;
      auto get_coro_handle(){
        return std::coroutine_handle<promise_type>::from_promise(*this);
      }
      stream_function get_return_object(){
          return get_coro_handle();
      }
      auto initial_suspend(){
          return std::suspend_never();
      }
      auto final_suspend(){
          return std::suspend_always();
      }
      template<class inner_return_type>
      auto await_transform(stream_function<input_type, inner_return_type> func){
        struct awaiter{
          std::shared_ptr<stream_function<input_type, inner_return_type> > func;
          bool await_ready(){ return func->done(); }
          void await_suspend(std::coroutine_handle<>){}
          inner_return_type await_resume(){
            return func->take_value();
          }
        };
        auto func_ptr = std::make_shared<stream_function<input_type, inner_return_type> >(std::move(func));
        awaiter ret{func_ptr};
        accept_override = [this, func = std::move(func_ptr)](input_type t){
          func->advance(std::move(t));
          return func->done();
        };
        return ret;
      }
      auto await_transform(read_from_stream_t){
        struct awaiter{
          promise_type& me;
          bool await_ready(){ return false; }
          void await_suspend(std::coroutine_handle<>){}
          input_type await_resume(){
            return std::move(std::get<1>(me.passed));
          }
        };
        return awaiter{*this};
      }
      void advance(input_type t){
        if(accept_override){
          if(accept_override(std::move(t))){
            accept_override = nullptr;
            get_coro_handle().resume();
          }
        }else{
          passed = passing_type{std::in_place_index<1>, std::move(t)};
          get_coro_handle().resume();
        }
      }
      void return_value(return_type v){
        passed = passing_type{std::in_place_index<2>, std::move(v)};
      }
      void unhandled_exception(){
        throw; //pass exception outwards
      }
    };
  };

};

#endif
