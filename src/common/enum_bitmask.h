// Copyright (C) 2024 Takayoshi Kochi
// See LICENSE.md for more information.

#pragma once

#include <type_traits>

// typed bitmask flags pattern - see
// https://walbourn.github.io/modern-c++-bitmask-types/
template <typename T>
constexpr T operator~(T a) {
  return static_cast<T>(~static_cast<std::underlying_type<T>::type>(a));
}
template <typename T>
constexpr T operator|(T a, T b) {
  return static_cast<T>(static_cast<std::underlying_type<T>::type>(a) |
                        static_cast<std::underlying_type<T>::type>(b));
}
template <typename T>
constexpr T operator&(T a, T b) {
  return static_cast<T>(static_cast<std::underlying_type<T>::type>(a) &
                        static_cast<std::underlying_type<T>::type>(b));
}
template <typename T>
constexpr T operator^(T a, T b) {
  return static_cast<T>(static_cast<std::underlying_type<T>::type>(a) ^
                        static_cast<std::underlying_type<T>::type>(b));
}
template <typename T>
constexpr T& operator|=(T& a, T b) {
  return reinterpret_cast<T&>(reinterpret_cast<std::underlying_type<T>::type&>(a) |=
                              static_cast<std::underlying_type<T>::type>(b));
}
template <typename T>
constexpr T& operator&=(T& a, T b) {
  return reinterpret_cast<T&>(reinterpret_cast<std::underlying_type<T>::type&>(a) &=
                              static_cast<std::underlying_type<T>::type>(b));
}
template <typename T>
constexpr T& operator^=(T& a, T b) {
  return reinterpret_cast<T&>(reinterpret_cast<std::underlying_type<T>::type&>(a) ^=
                              static_cast<std::underlying_type<T>::type>(b));
}

