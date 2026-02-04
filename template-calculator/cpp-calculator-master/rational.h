#pragma once
#include <cstdlib>
#include <numeric>
#include <iostream>

class Rational {
public:
    Rational(int numerator = 0, int denominator = 1) :
        numerator_(numerator), denominator_(denominator) {
        if (denominator == 0) {
            std::abort();
        }
        Reduction();
    }

    Rational operator+(const Rational& r2) const {
        Rational other{*this};
        other += r2;
        return other;
    }

    Rational operator-(const Rational& r2) const {
        Rational other{*this};
        other -= r2;
        return other;
    }

    Rational operator*(const Rational& r2) const {
        Rational other{*this};
        other *= r2;
        return other;
    }

    Rational operator/(const Rational& r2) const {
        Rational other{*this};
        other /= r2;
        return other;
    }

    Rational& operator+=(const Rational &other) {
        int new_numerator = numerator_ * other.GetDenominator() + other.GetNumerator() * denominator_;
        int new_denominator = denominator_ * other.GetDenominator();
        *this = Rational(new_numerator, new_denominator);
        return *this;
    }

    Rational& operator-=(const Rational &other) {
        int new_numerator = numerator_ * other.GetDenominator() - other.GetNumerator() * denominator_;
        int new_denominator = denominator_ * other.GetDenominator();
        *this = Rational(new_numerator, new_denominator);
        return *this;
    }

    Rational& operator*=(const Rational &other) {
        int new_numerator = numerator_ * other.GetNumerator();
        int new_denominator = denominator_ * other.GetDenominator();
        *this = Rational(new_numerator, new_denominator);
        return *this;
    }

    Rational& operator/=(const Rational &other) {
        int new_numerator = numerator_ * other.GetDenominator();
        int new_denominator = denominator_ * other.GetNumerator();
        *this = Rational(new_numerator, new_denominator);
        return *this;
    }

    Rational& operator=(const Rational &other) = default;

    Rational operator+() const {
        return *this;
    }

    Rational operator-() const {
        return Rational(-numerator_, denominator_);
    }

    int GetNumerator() const {
        return numerator_;
    }

    int GetDenominator() const {
        return denominator_;
    }

    Rational Inv() const {
        return Rational(denominator_, numerator_);
    }

    auto operator<=>(const Rational& other) const = default;

    friend std::ostream& operator<<(std::ostream &os, const Rational &r);
    friend std::istream& operator>>(std::istream &is, Rational &r);

private:
    void Reduction() {
        if (denominator_ < 0) {
            numerator_ = -numerator_;
            denominator_ = -denominator_;
        }
        const int divisor = std::gcd(numerator_, denominator_);
        numerator_ /= divisor;
        denominator_ /= divisor;
    }

private:
    int numerator_;
    int denominator_;
};

inline std::istream& operator>>(std::istream& is, Rational& r) {
    int numerator;
    int denominator;
    char div;

    if (!(is >> numerator)) {
        return is;
    }

    if (!(is >> std::ws >> div)) {
        r = Rational(numerator, 1);
        is.clear();
        return is;
    }

    if (div != '/') {
        r = Rational(numerator, 1);
        is.unget();
        return is;
    }

    if (!(is >> denominator) || (denominator == 0)) {
        is.setstate(std::ios::failbit);
        return is;
    }

    r = Rational(numerator, denominator);

    return is;
}

inline std::ostream& operator<<(std::ostream& os, const Rational& r) {
    using namespace std::literals;
    if (r.denominator_ == 1) {
        os << r.numerator_;
    } else {
        os << r.numerator_ << " / "s << r.denominator_;
    }
    return os;
}

