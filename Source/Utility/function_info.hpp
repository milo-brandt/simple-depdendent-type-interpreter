#ifndef FUNCTION_INFO_HPP
#define FUNCTION_INFO_HPP

namespace mdb {
  template<class Ret>
  struct FunctionInfo{};
  template<class Ret, class... Args>
  struct FunctionInfo<Ret(Args...)> {};
  namespace detail {
    template<class T>
    struct MemberFunctionInfo;
    template<class Ret, class T, class... Args>
    struct MemberFunctionInfo<Ret(T::*)(Args...)> {
      using Type = FunctionInfo<Ret(Args...)>;
    };
    template<class Ret, class T, class... Args>
    struct MemberFunctionInfo<Ret(T::*)(Args...) const> : MemberFunctionInfo<Ret(T::*)(Args...)> {};
    template<class T>
    struct GetFunctionInfo {
      using Type = typename MemberFunctionInfo<decltype(&T::operator())>::Type;
    };
    template<class Ret, class... Args>
    struct GetFunctionInfo<Ret(*)(Args...)> {
      using Type = FunctionInfo<Ret(Args...)>;
    };
  }
  template<class T>
  using FunctionInfoFor = typename detail::GetFunctionInfo<T>::Type;
  template<class T>
  constexpr FunctionInfoFor<T> function_info_for = {};
}

#endif
