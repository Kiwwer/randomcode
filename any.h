#pragma once
#include <type_traits>
#include <memory>
#include <exception>
#include <stdexcept>

class Any {
private:
    struct InnerBase {
        using Pointer = std::unique_ptr<InnerBase>;
        virtual ~InnerBase() {
        }
        virtual InnerBase* Clone() const = 0;
    };

    template <typename T>
    struct Inner : InnerBase {
        Inner(T newval) : value_(std::move(newval)) {
        }
        virtual InnerBase* Clone() const override {
            return new Inner(value_);
        }
        T& operator*() {
            return value_;
        }
        const T& operator*() const {
            return value_;
        }

    private:
        T value_;
    };

    InnerBase::Pointer inner_ = nullptr;

public:
    Any() {
    }

    template <class T>
    Any(const T& value) : inner_(new Inner<T>(value)) {
    }

    template <class T>
    Any& operator=(const T& value) {
        inner_ = std::make_unique<Inner<T>>(value);
        return *this;
    }

    Any(const Any& rhs) : inner_(rhs.inner_->Clone()) {
    }

    Any& operator=(const Any& rhs) {
        Any tmp(rhs);
        std::swap(tmp.inner_, this->inner_);
        return *this;
    }
    ~Any() {
    }

    bool Empty() const {
        return inner_ == nullptr;
    }

    void Clear() {
        inner_ = nullptr;
    }
    void Swap(Any& rhs) {
        std::swap(rhs.inner_, this->inner_);
    }

    template <class T>
    const T& GetValue() const {
        try {
            return *dynamic_cast<Inner<T>&>(*inner_);
        } catch (...) {
            throw std::bad_cast();
        }
    }
};

