/**
 * @file expected.hpp
 * @brief Lightweight expected<T, E> implementation for error handling
 *
 * This is a minimal implementation of std::expected (C++23) for use
 * in embedded systems where exceptions are disabled.
 *
 * For production, consider using tl::expected from:
 * https://github.com/TartanLlama/expected
 */

#pragma once

#include <utility>
#include <type_traits>

namespace tl {

/**
 * @brief Tag type for unexpected value construction
 */
template<typename E>
class unexpected {
public:
    constexpr explicit unexpected(const E& e) : error_(e) {}
    constexpr explicit unexpected(E&& e) : error_(std::move(e)) {}

    constexpr const E& error() const& { return error_; }
    constexpr E& error() & { return error_; }
    constexpr E&& error() && { return std::move(error_); }

private:
    E error_;
};

// Deduction guide
template<typename E>
unexpected(E) -> unexpected<E>;

/**
 * @brief Expected value or error type
 *
 * @tparam T Expected value type
 * @tparam E Error type
 */
template<typename T, typename E>
class expected {
public:
    using value_type = T;
    using error_type = E;

    // Constructors for value
    constexpr expected() : hasValue_(true), value_{} {}

    constexpr expected(const T& val) : hasValue_(true), value_(val) {}

    constexpr expected(T&& val) : hasValue_(true), value_(std::move(val)) {}

    // Constructor for error
    constexpr expected(const unexpected<E>& unex)
        : hasValue_(false), error_(unex.error()) {}

    constexpr expected(unexpected<E>&& unex)
        : hasValue_(false), error_(std::move(unex.error())) {}

    // Copy constructor
    constexpr expected(const expected& other) : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&value_) T(other.value_);
        } else {
            new (&error_) E(other.error_);
        }
    }

    // Move constructor
    constexpr expected(expected&& other) noexcept : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&value_) T(std::move(other.value_));
        } else {
            new (&error_) E(std::move(other.error_));
        }
    }

    // Destructor
    ~expected() {
        if (hasValue_) {
            value_.~T();
        } else {
            error_.~E();
        }
    }

    // Copy assignment
    expected& operator=(const expected& other) {
        if (this != &other) {
            this->~expected();
            new (this) expected(other);
        }
        return *this;
    }

    // Move assignment
    expected& operator=(expected&& other) noexcept {
        if (this != &other) {
            this->~expected();
            new (this) expected(std::move(other));
        }
        return *this;
    }

    // Observers
    constexpr bool has_value() const noexcept { return hasValue_; }
    constexpr explicit operator bool() const noexcept { return hasValue_; }

    // Value accessors
    constexpr T& value() & { return value_; }
    constexpr const T& value() const& { return value_; }
    constexpr T&& value() && { return std::move(value_); }

    constexpr T* operator->() { return &value_; }
    constexpr const T* operator->() const { return &value_; }

    constexpr T& operator*() & { return value_; }
    constexpr const T& operator*() const& { return value_; }
    constexpr T&& operator*() && { return std::move(value_); }

    // Error accessors
    constexpr E& error() & { return error_; }
    constexpr const E& error() const& { return error_; }
    constexpr E&& error() && { return std::move(error_); }

    // value_or
    template<typename U>
    constexpr T value_or(U&& default_value) const& {
        return hasValue_ ? value_ : static_cast<T>(std::forward<U>(default_value));
    }

    template<typename U>
    constexpr T value_or(U&& default_value) && {
        return hasValue_ ? std::move(value_) : static_cast<T>(std::forward<U>(default_value));
    }

private:
    bool hasValue_;
    union {
        T value_;
        E error_;
    };
};

/**
 * @brief Specialization for void value type
 */
template<typename E>
class expected<void, E> {
public:
    using value_type = void;
    using error_type = E;

    constexpr expected() : hasValue_(true) {}

    constexpr expected(const unexpected<E>& unex)
        : hasValue_(false), error_(unex.error()) {}

    constexpr expected(unexpected<E>&& unex)
        : hasValue_(false), error_(std::move(unex.error())) {}

    constexpr bool has_value() const noexcept { return hasValue_; }
    constexpr explicit operator bool() const noexcept { return hasValue_; }

    constexpr E& error() & { return error_; }
    constexpr const E& error() const& { return error_; }

private:
    bool hasValue_;
    E error_;
};

}  // namespace tl
