#ifndef ALURE_MAIN_H
#define ALURE_MAIN_H

#include "alure2.h"

#include <system_error>
#if __cplusplus >= 201703L
#include <variant>
#else
#include "mpark/variant.hpp"

namespace std {
using mpark::variant;
using mpark::monostate;
using mpark::get;
using mpark::get_if;
using mpark::holds_alternative;
} // namespace std
#endif

#ifdef __GNUC__
#define LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#define UNLIKELY(x) __builtin_expect(static_cast<bool>(x), false)
#else
#define LIKELY(x) static_cast<bool>(x)
#define UNLIKELY(x) static_cast<bool>(x)
#endif

#define DECL_THUNK0(ret, C, Name, cv)                                         \
ret C::Name() cv { return pImpl->Name(); }
#define DECL_THUNK1(ret, C, Name, cv, T1)                                     \
ret C::Name(T1 a) cv { return pImpl->Name(std::forward<T1>(a)); }
#define DECL_THUNK2(ret, C, Name, cv, T1, T2)                                 \
ret C::Name(T1 a, T2 b) cv                                                    \
{ return pImpl->Name(std::forward<T1>(a), std::forward<T2>(b)); }
#define DECL_THUNK3(ret, C, Name, cv, T1, T2, T3)                             \
ret C::Name(T1 a, T2 b, T3 c) cv                                              \
{                                                                             \
    return pImpl->Name(std::forward<T1>(a), std::forward<T2>(b),              \
                       std::forward<T3>(c));                                  \
}


namespace alure {

// Need to use these to avoid extraneous commas in macro parameter lists
using Vector3Pair = std::pair<Vector3,Vector3>;
using UInt64NSecPair = std::pair<uint64_t,std::chrono::nanoseconds>;
using SecondsPair = std::pair<Seconds,Seconds>;
using ALfloatPair = std::pair<ALfloat,ALfloat>;
using ALuintPair = std::pair<ALuint,ALuint>;
using BoolTriple = std::tuple<bool,bool,bool>;


template<typename T>
inline std::future_status GetFutureState(const SharedFuture<T> &future)
{ return future.wait_for(std::chrono::seconds::zero()); }

// This variant is a poor man's optional
std::variant<std::monostate,uint64_t> ParseTimeval(StringView strval, double srate) noexcept;

template<size_t N>
struct Bitfield {
private:
    Array<uint8_t,(N+7)/8> mElems;

public:
    Bitfield() { std::fill(mElems.begin(), mElems.end(), 0); }

    bool operator[](size_t i) const noexcept
    { return static_cast<bool>(mElems[i/8] & (1<<(i%8))); }

    void set(size_t i) noexcept { mElems[i/8] |= 1<<(i%8); }
};


class alc_category : public std::error_category {
    alc_category() noexcept { }

public:
    static alc_category sSingleton;

    const char *name() const noexcept override final { return "alc_category"; }
    std::error_condition default_error_condition(int code) const noexcept override final
    { return std::error_condition(code, *this); }

    bool equivalent(int code, const std::error_condition &condition) const noexcept override final
    { return default_error_condition(code) == condition; }
    bool equivalent(const std::error_code &code, int condition) const noexcept override final
    { return *this == code.category() && code.value() == condition; }

    std::string message(int condition) const override final;
};
template<typename T>
inline std::system_error alc_error(int code, T&& what)
{ return std::system_error(code, alc_category::sSingleton, std::forward<T>(what)); }
inline std::system_error alc_error(int code)
{ return std::system_error(code, alc_category::sSingleton); }

class al_category : public std::error_category {
    al_category() noexcept { }

public:
    static al_category sSingleton;

    const char *name() const noexcept override final { return "al_category"; }
    std::error_condition default_error_condition(int code) const noexcept override final
    { return std::error_condition(code, *this); }

    bool equivalent(int code, const std::error_condition &condition) const noexcept override final
    { return default_error_condition(code) == condition; }
    bool equivalent(const std::error_code &code, int condition) const noexcept override final
    { return *this == code.category() && code.value() == condition; }

    std::string message(int condition) const override final;
};
template<typename T>
inline std::system_error al_error(int code, T&& what)
{ return std::system_error(code, al_category::sSingleton, std::forward<T>(what)); }
inline std::system_error al_error(int code)
{ return std::system_error(code, al_category::sSingleton); }

inline void throw_al_error(const char *str)
{
    ALenum err = alGetError();
    if(UNLIKELY(err != AL_NO_ERROR))
        throw al_error(err, str);
}

} // namespace alure

#endif /* ALURE_MAIN_H */
