// Copyright (c) 2018 Adam Butcher, Antony Polukhin
// Copyright (c) 2019-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SERDE_BOOST_PFR_DETAIL_TIE_FROM_STRUCTURE_TUPLE_HPP
#define SERDE_BOOST_PFR_DETAIL_TIE_FROM_STRUCTURE_TUPLE_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

#include <third_party/boost_pfr/include/boost/pfr/detail/core.hpp>

#include <third_party/boost_pfr/include/boost/pfr/detail/stdtuple.hpp>
#include <third_party/boost_pfr/include/boost/pfr/tuple_size.hpp>
#include <third_party/boost_pfr/include/boost/pfr/detail/make_integer_sequence.hpp>

#if !defined(SERDE_BOOST_PFR_INTERFACE_UNIT)
#include <tuple>
#endif

namespace serde_boost_pfr { namespace pfr { namespace detail {

/// \brief A `std::tuple` capable of de-structuring assignment used to support
/// a tie of multiple lvalue references to fields of an aggregate T.
///
/// \sa serde_boost_pfr::pfr::tie_from_structure
template <typename... Elements>
struct tie_from_structure_tuple : std::tuple<Elements&...> {
    using base = std::tuple<Elements&...>;
    using base::base;

    template <typename T>
    constexpr tie_from_structure_tuple& operator= (T const& t) {
        base::operator=(
            detail::make_stdtiedtuple_from_tietuple(
                detail::tie_as_tuple(t),
                detail::make_index_sequence<tuple_size_v<T>>()));
        return *this;
    }
};

}}} // namespace serde_boost_pfr::pfr::detail

#endif // SERDE_BOOST_PFR_DETAIL_TIE_FROM_STRUCTURE_TUPLE_HPP
