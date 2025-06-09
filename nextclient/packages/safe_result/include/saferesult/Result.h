#pragma once
#include <optional>
#include <stdexcept>
#include <variant>
#include <string>

namespace saferesult
{
    class ResultErrorInterface
    {
    public:
        virtual ~ResultErrorInterface() = default;
        virtual std::string to_string() const = 0;
    };

    class ResultErrorException : public std::runtime_error
    {
    public:
        ResultErrorException(std::string what_message) :
            std::runtime_error(what_message)
        { }
    };

    class ResultError : public ResultErrorInterface
    {
        std::string error_;

    public:
        explicit ResultError(std::string error) noexcept :
            error_(std::move(error))
        { }

        [[nodiscard]] const std::string& description() const
        {
            return error_;
        }

        std::string to_string() const override
        {
            return error_;
        }
    };

    template<class TError = ResultError>
        requires std::derived_from<TError, ResultErrorInterface>
    class Result
    {
        std::optional<TError> error_;

    public:
        Result() = default;

        Result(TError&& error) :
            error_(std::move(error))
        { }

        [[nodiscard]] bool has_error() const
        {
            return error_.has_value();
        }

        [[nodiscard]] const TError& error() const
        {
            return error_.value();
        }

        [[nodiscard]] std::string error_str() const
        {
            return error_.value().to_string();
        }

        Result& throw_if_error()
        {
            if (has_error())
            {
                throw ResultErrorException(error_->to_string());
            }

            return *this;
        }
    };

    template<class T, class TError = ResultError>
        requires std::derived_from<TError, ResultErrorInterface>
    class ResultT
    {
        std::variant<T, TError> value_;

    public:
        ResultT(T&& value) :
            value_(std::forward<T>(value))
        { }

        ResultT(TError&& error) :
            value_(std::move(error))
        { }

        [[nodiscard]] bool has_error() const
        {
            return std::holds_alternative<TError>(value_);
        }

        [[nodiscard]] const TError& error() const
        {
            return std::get<TError>(value_);
        }

        [[nodiscard]] std::string error_str() const
        {
            return error().to_string();
        }

        ResultT& throw_if_error()
        {
            if (has_error())
            {
                throw ResultErrorException(error().to_string());
            }

            return *this;
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
}
