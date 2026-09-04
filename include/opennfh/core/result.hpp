#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace opennfh {

enum class ErrorCode {
    InvalidArgument,
    Io,
    Format,
    Missing,
    Duplicate,
    Unsupported,
};

struct Error {
    ErrorCode code{ErrorCode::Format};
    std::string message;
    std::string source;
    std::size_t line{0};
    std::size_t column{0};
};

template <typename T>
class Result {
public:
    static Result success(T value) {
        Result result;
        result.value_ = std::move(value);
        return result;
    }

    static Result failure(Error error) {
        Result result;
        result.error_ = std::move(error);
        return result;
    }

    [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }
    [[nodiscard]] const T& value() const { return *value_; }
    [[nodiscard]] const Error& error() const { return *error_; }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};

}  // namespace opennfh
