#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <utility>

template<typename T>
class Optional {
    bool has_value;
    union {
        T value;
    };
public:
    Optional() : has_value(false) {}
    Optional(const T& val) : has_value(true), value(val) {}
    Optional(T&& val) : has_value(true), value(std::move(val)) {}
    ~Optional() { if (has_value) value.~T(); }
    
    bool hasValue() const { return has_value; }
    T& get() { return value; }
    const T& get() const { return value; }
};

#endif