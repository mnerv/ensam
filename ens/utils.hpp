/**
 * Commonly used utilties.
 *
 * Copyright (c) 2024 porter@nrz.se
 */
#ifndef ENS_UTILS_HPP
#define ENS_UTILS_HPP

#include <type_traits>
#include <cstddef>
#include <limits>
#include <numbers>
#include <memory>

namespace ens {

template <typename T>
concept Integral = std::is_integral<T>::value;

template <typename T, std::size_t N>
inline constexpr auto len(T (&)[N]) -> std::size_t {
    return N;
}

inline constexpr auto bit_set(Integral auto const& position) {
    return 1 << position;
}

inline constexpr auto bit_level(Integral auto const& reg, Integral auto const& mask, Integral auto const& data) {
    return (reg & ~mask) | (data & mask);
}

// Aliases
namespace nums = std::numbers;

template <typename T>
using lim = std::numeric_limits<T>;

inline constexpr auto pi = std::numbers::pi;

template <typename T>
inline constexpr auto pi_v = std::numbers::pi_v<T>;

template <typename T>
using ref = std::shared_ptr<T>;
template <typename T>
using local = std::unique_ptr<T>;
template <typename T>
using weak = std::weak_ptr<T>;

template <typename T, typename... Args>
constexpr auto make_ref(Args&&... args) -> ref<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
}
template <typename T, typename... Args>
constexpr auto make_local(Args&&... args) -> local<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
}
}

#endif  // !ENS_UTILS_HPP

