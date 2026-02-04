#pragma once

#include <string>
#include <optional>
#include <cmath>
#include "rational.h"
#include "pow.h"

using Error = std::string;

template<typename Number>
class Calculator {
public:

    bool GetHasMem() {
        return mem_.has_value();
    }

    void Save() {
        mem_ = current_number_;
    }

    void Load() {
        if (mem_) {
            current_number_ = mem_.value();
        }
    }

    void Set(Number n) {
        current_number_ = n;
    }

    Number GetNumber() const {
        return current_number_;
    }

    std::optional<Error> Add(Number n) {
        current_number_ += n;
        return std::nullopt;
    }

    std::optional<Error>Sub(Number n) {
        current_number_ -= n;
        return std::nullopt;
    }

    std::optional<Error> Div(Number n) {
        if constexpr (std::is_integral_v<Number> || std::is_same_v<Number, Rational>) {
            if (n == 0) {
                return "Division by zero";
            }
        }
        current_number_ /= n;
        return std::nullopt;
    }

    std::optional<Error> Mul(Number n) {
        current_number_ *= n;
        return std::nullopt;
    }    

    std::optional<Error> Pow(Number n) {
        if constexpr (std::is_integral_v<Number> || std::is_same_v<Number, Rational>) {
            if (current_number_ == 0 && n == 0) {
                return "Zero power to zero";
            }
            if constexpr (std::is_same_v<Number, Rational>) {
                if (n.GetDenominator() !=  1) {
                    return "Fractional power is not supported";
                }
                current_number_ = ::Pow(current_number_, n);
                return std::nullopt;
            }
            if constexpr (std::is_integral_v<Number>) {
                if (n < 0) {
                    return "Integer negative power";
                }
                current_number_ = ::IntegerPow(current_number_, n);
                return std::nullopt;
            }
        } else {
            current_number_ = std::pow(current_number_, n);
            return std::nullopt;
        }
    }
    
private:
    Number current_number_ = 0;
    std::optional<Number> mem_;
};


