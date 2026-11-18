// Copyright (c) 2023 Denis Mikhailov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SERDE_BOOST_PFR_DETAIL_STDARRAY_HPP
#define SERDE_BOOST_PFR_DETAIL_STDARRAY_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

#include <third_party/boost_pfr/include/boost/pfr/detail/sequence_tuple.hpp>

#if !defined(SERDE_BOOST_PFR_INTERFACE_UNIT)
#include <array>
#if SERDE_BOOST_PFR_CORE_NAME_ENABLED
#   include <string_view>
#endif
#include <utility> // metaprogramming stuff
#endif

namespace serde_boost_pfr { namespace pfr { namespace detail {

#if SERDE_BOOST_PFR_CORE_NAME_ENABLED
template <class T, std::size_t... I>
constexpr auto make_stdarray_from_tietuple(const T& t, std::index_sequence<I...>) noexcept {
    return std::array<std::string_view, sizeof...(I)>{
        serde_boost_pfr::pfr::detail::sequence_tuple::get<I>(t)...
    };
}
#else
template <class T, std::size_t... I>
constexpr auto make_stdarray_from_tietuple(const T&, std::index_sequence<I...>) noexcept {
    return nullptr;
}
#endif

}}} // namespace serde_boost_pfr::pfr::detail

#endif // SERDE_BOOST_PFR_DETAIL_STDARRAY_HPP
