#pragma once

#include <optional>
#include <variant>
#include <string>

class ResultError
{
    std::string error_;

public:
    explicit ResultError(std::string error) noexcept :
        error_(std::move(error))
    { }

    [[nodiscard]] const std::string& get_error() const
    {
        return error_;
    }
};

class Result
{
    std::optional<ResultError> error_;

public:
    Result() = default;

    Result(ResultError&& error) :
        error_(std::move(error))
    { }

    [[nodiscard]] bool has_error() const
    {
        return error_.has_value();
    }

    [[nodiscard]] const std::string& get_error() const
    {
        return error_.value().get_error();
    }
};

template<class T>
class ResultT
{
    std::variant<T, ResultError> value_;

public:
    ResultT(T&& value) :
        value_(std::forward<T>(value))
    { }

    ResultT(ResultError&& error) :
        value_(std::move(error))
    { }

    [[nodiscard]] bool has_error() const
    {
        return std::holds_alternative<ResultError>(value_);
    }

    [[nodiscard]] const std::string& get_error() const
    {
        return std::get<ResultError>(value_).get_error();
    }

    const T& operator*() const
    {
        return std::get<T>(value_);
    }

    const T* operator->() const
    {
        return &std::get<T>(value_);
    }
};
