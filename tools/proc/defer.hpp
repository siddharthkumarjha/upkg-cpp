#pragma once

template <typename T> struct deferrer
{
    T f;
    deferrer(T f) : f(f) {};

    deferrer(const deferrer &) = delete;
    deferrer(deferrer &&)      = delete;

    ~deferrer() { f(); }
};

#define TOKEN_CONCAT_NX(a, b) a##b
#define TOKEN_CONCAT(a, b) TOKEN_CONCAT_NX(a, b)
#define DEFER deferrer TOKEN_CONCAT(__DEFERRED, __COUNTER__) =
