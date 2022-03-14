#pragma once

#include <tuple>
#include <functional>
#include <variant>
// #include <type_traits>

#include <plog/Log.h>

#include <ueye.h>


/////////////////////////////////////////////////////////////
// API CALL WRAPPER

// global or namespaced: UEYE_API_CALL_PROTO{ /* impl */ }
// member declaration: UEYE_API_CALL_PROTO;
#define UEYE_API_CALL_PROTO()                                                                                                                                                                                       \
    template <typename FunctionRet, typename... FunctionArgs>                                                                                                                                                       \
    void _api_wrapped(FunctionRet (*f)(FunctionArgs...), std::tuple<FunctionArgs...> f_args, const std::string f_name, const std::string caller_name, const int caller_line)                                        \
    {                                                                                                                                                                                                               \
        _api_wrapped(f, f_args, "", nullptr, f_name, caller_name, caller_line);                                                                                                                                     \
    }                                                                                                                                                                                                               \
    template <typename FunctionRet, typename... FunctionArgs>                                                                                                                                                       \
    void _api_wrapped(FunctionRet (*f)(FunctionArgs...), std::tuple<FunctionArgs...> f_args, const std::string msg, const std::string f_name, const std::string caller_name, const int caller_line)                 \
    {                                                                                                                                                                                                               \
        _api_wrapped(f, f_args, msg, nullptr, f_name, caller_name, caller_line);                                                                                                                                    \
    }                                                                                                                                                                                                               \
    template <typename FunctionRet, typename... FunctionArgs>                                                                                                                                                       \
    void _api_wrapped(FunctionRet (*f)(FunctionArgs...), std::tuple<FunctionArgs...> f_args, std::function<void()> cleanup_handler, const std::string f_name, const std::string caller_name, const int caller_line) \
    {                                                                                                                                                                                                               \
        _api_wrapped(f, f_args, "", cleanup_handler, f_name, caller_name, caller_line);                                                                                                                             \
    }                                                                                                                                                                                                               \
    template <typename FunctionRet, typename... FunctionArgs>                                                                                                                                                       \
    void _api_wrapped(FunctionRet (*f)(FunctionArgs...), std::tuple<FunctionArgs...> f_args, const std::string msg, std::function<void()> cleanup_handler, const std::string f_name, const std::string caller_name, const int caller_line)

// as class member: template<typename T> UEYE_API_CALL_MEMBER_DEF(uEyeHandle<M,D>){ /* impl */ }
#define UEYE_API_CALL_MEMBER_DEF(...)                         \
    template <typename FunctionRet, typename... FunctionArgs> \
    void __VA_ARGS__## ::_api_wrapped(FunctionRet (*f)(FunctionArgs...), std::tuple<FunctionArgs...> f_args, const std::string msg, std::function<void()> cleanup_handler, const std::string f_name, const std::string caller_name, const int caller_line)

// default wrapper implementation
UEYE_API_CALL_PROTO()
{
    int nret = std::apply(f, f_args);
    if (nret != IS_SUCCESS)
    {
        const std::string common_msg = fmt::format(
            "{}; {}() returned with code {}",
            msg,
            f_name,
            nret);
        PLOG_WARNING << fmt::format("[{}@{}] {}", caller_name, caller_line, common_msg);

        if (cleanup_handler)
        {
            PLOG_WARNING << fmt::format("[{}@{}] calling provided cleanup handler after failed call to {}()", caller_name, caller_line, f_name);
            cleanup_handler();
        }

        throw std::runtime_error(common_msg);
    }

    PLOG_DEBUG << fmt::format("[{}@{}] {}() returned with code {}", caller_name, caller_line, f_name, nret);
}

// call API method, log debug on success, log warning and throw std::runtime_error if fails
// override with classmember or namespaced method for custom logging
// UEYE_API_CALL(<apiFunction>, {<parameters>, <...>}, "error msg")
// UEYE_API_CALL(<apiFunction>, {<parameters>, <...>}, "error msg", [](){ /* cleanup function */ })
#define UEYE_API_CALL(f_name, ...) _api_wrapped(f_name, __VA_ARGS__, #f_name, PLOG_GET_FUNC(), __LINE__)

//
/////////////////////////////////////////////////////////////

namespace uEyeWrapper
{
    // loop repetition helper: repeat N times with nice syntax
    // for( auto _ : times(N)) {}
    typedef std::vector<std::monostate> times;

}