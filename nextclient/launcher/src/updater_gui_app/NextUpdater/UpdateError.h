#pragma once
#include <string>
#include <magic_enum/magic_enum.hpp>
#include <ncl_utils/safe_result.h>

enum class UpdateErrorType
{
    ConnectionError,
    NetworkError,
    DeserializationError,
    FileOperationError,
    UnknownError
};

class UpdateError : public ncl_utils::ResultErrorInterface
{
    std::string message_;
    UpdateErrorType type_;

public:
    explicit UpdateError(UpdateErrorType type, std::string message) :
        type_(type),
        message_(message)
    {}

    [[nodiscard]] UpdateErrorType GetType() const { return type_; }
    [[nodiscard]] std::string to_string() const { return std::format("{} | {}", magic_enum::enum_name(GetType()), message_); }
};