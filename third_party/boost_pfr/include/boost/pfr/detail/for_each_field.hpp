// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SERDE_BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
#define SERDE_BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

#include <third_party/boost_pfr/include/boost/pfr/detail/core.hpp>
#include <third_party/boost_pfr/include/boost/pfr/detail/fields_count.hpp>
#include <third_party/boost_pfr/include/boost/pfr/detail/for_each_field_impl.hpp>
#include <third_party/boost_pfr/include/boost/pfr/detail/make_integer_sequence.hpp>

#if !defined(SERDE_BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>      // metaprogramming stuff
#endif

namespace serde_boost_pfr { namespace pfr { namespace detail {

template <class T, class F>
constexpr void for_each_field(T&& value, F&& func) {
    constexpr std::size_t fields_count_val = serde_boost_pfr::pfr::detail::fields_count<std::remove_reference_t<T>>();

    ::serde_boost_pfr::pfr::detail::for_each_field_dispatcher(
        value,
        [f = std::forward<F>(func)](auto&& t) mutable {
            // MSVC related workaround. Its lambdas do not capture constexprs.
            constexpr std::size_t fields_count_val_in_lambda
                = serde_boost_pfr::pfr::detail::fields_count<std::remove_reference_t<T>>();

            ::serde_boost_pfr::pfr::detail::for_each_field_impl(
                t,
                std::forward<F>(f),
                detail::make_index_sequence<fields_count_val_in_lambda>{},
                std::is_rvalue_reference<T&&>{}
            );
        },
        detail::make_index_sequence<fields_count_val>{}
    );
}

}}} // namespace serde_boost_pfr::pfr::detail


#endif // SERDE_BOOST_PFR_DETAIL_FOR_EACH_FIELD_HPP
