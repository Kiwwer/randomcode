#pragma once
#include <type_traits>
#include <memory>
#include <exception>
#include <stdexcept>
#include <cstring>

template <class T>
struct Try {
    T* object_ = nullptr;
    std::exception_ptr except_ = nullptr;
    Try() {
    }
    Try(const Try& other) {
        if (other.object_) {
            object_ = new T(*other.object_);
        }
        except_ = other.except_;
    }
    Try(Try&& other) {
        if (other.object_) {
            object_ = std::move(other.object_);
            other.object_ = nullptr;
        }
        except_ = other.except_;
    }
    ~Try() {
        if (object_) {
            delete object_;
            object_ = nullptr;
        }
    }
    Try(T obj) : object_(new T(obj)) {
    }
    template <class Ex>
    Try(const Ex& exc) : except_(std::make_exception_ptr(exc)) {
    }
    Try(const std::exception_ptr& exc) : except_(exc) {
    }
    void operator=(const Try& other) {
        if (object_) {
            delete object_;
            object_ = nullptr;
        }
        if (other.object_) {
            object_ = new T(*other.object_);
        }
        except_ = other.except_;
    }
    void operator=(Try&& other) {
        if (object_) {
            delete object_;
            object_ = nullptr;
        }
        if (other.object_) {
            object_ = std::move(other.object_);
            other.object_ = nullptr;
        }
        except_ = other.except_;
    }
    const T& Value() const {
        if (static_cast<bool>(except_)) {
            std::rethrow_exception(except_);
        }
        if (!object_) {
            throw std::logic_error("Object is empty");
        }
        return *object_;
    }
    void Throw() {
        if (static_cast<bool>(except_)) {
            std::rethrow_exception(except_);
        }
        throw std::logic_error("No exception");
    }
    bool IsFailed() const {
        return static_cast<bool>(except_);
    }
};

template <>
struct Try<void> {
    std::exception_ptr except_ = nullptr;
    Try() {
    }
    Try(const Try<void>& other) {
        except_ = other.except_;
    }
    Try(Try<void>&& other) {
        except_ = other.except_;
    }
    template <class Ex>
    Try(const Ex& exc) : except_(std::make_exception_ptr(exc)) {
    }
    Try(const std::exception_ptr& exc) : except_(exc) {
    }
    void operator=(const Try<void>& other) {
        except_ = other.except_;
    }
    void operator=(Try<void>&& other) {
        except_ = other.except_;
    }
    void Throw() {
        if (static_cast<bool>(except_)) {
            std::rethrow_exception(except_);
        }
        throw std::logic_error("No exception");
    }
    bool IsFailed() const {
        return static_cast<bool>(except_);
    }
};

template <typename T>
struct MakeTry {
    template <class Function, class... Args>
    Try<T> Guidance(Function func, Args... args) {
        return Try<T>(func(args...));
    }
};
template <>
struct MakeTry<void> {
    template <class Function, class... Args>
    Try<void> Guidance(Function func, Args... args) {
        func(args...);
        return Try<void>();
    }
};

template <class Function, class... Args>
auto TryRun(Function func, Args... args) {
    using ReturnType = decltype(func(args...));
    Try<ReturnType> res;
    try {
        MakeTry<ReturnType> guide;
        res = guide.Guidance(func, args...);
        func(args...);
    } catch (const std::exception& e) {
        Try<ReturnType> tryer = std::current_exception();
        res = tryer;
    } catch (const char* reason) {
        Try<ReturnType> tryer = std::logic_error(reason);
        res = tryer;
    } catch (int wtfint) {
        Try<ReturnType> tryer = std::logic_error(std::strerror(wtfint));
        res = tryer;
    } catch (...) {
        Try<ReturnType> tryer = std::logic_error("Unknown exception");
        res = tryer;
    }
    return res;
}

