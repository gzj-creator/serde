// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#ifndef SERDE_BOOST_PFR_TUPLE_SIZE_HPP
#define SERDE_BOOST_PFR_TUPLE_SIZE_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

#if !defined(SERDE_BOOST_PFR_USE_MODULES) || defined(SERDE_BOOST_PFR_INTERFACE_UNIT)

#include <third_party/boost_pfr/include/boost/pfr/detail/sequence_tuple.hpp>
#include <third_party/boost_pfr/include/boost/pfr/detail/fields_count.hpp>

#if !defined(SERDE_BOOST_PFR_INTERFACE_UNIT)
#include <type_traits>
#include <utility>      // metaprogramming stuff
#endif

/// \file boost/pfr/tuple_size.hpp
/// Contains tuple-like interfaces to get fields count \forcedlink{tuple_size}, \forcedlink{tuple_size_v}.
///
/// \b Synopsis:
namespace serde_boost_pfr { namespace pfr {

SERDE_BOOST_PFR_BEGIN_MODULE_EXPORT

/// Has a static const member variable `value` that contains fields count in a T.
/// Works for any T that satisfies \aggregate.
///
/// \b Example:
/// \code
///     std::array<int, serde_boost_pfr::pfr::tuple_size<my_structure>::value > a;
/// \endcode
template <class T>
using tuple_size = detail::size_t_< serde_boost_pfr::pfr::detail::fields_count<T>() >;


/// `tuple_size_v` is a template variable that contains fields count in a T and
/// works for any T that satisfies \aggregate.
///
/// \b Example:
/// \code
///     std::array<int, serde_boost_pfr::pfr::tuple_size_v<my_structure> > a;
/// \endcode
template <class T>
constexpr std::size_t tuple_size_v = tuple_size<T>::value;

SERDE_BOOST_PFR_END_MODULE_EXPORT

}} // namespace serde_boost_pfr::pfr

#endif  // #if defined(SERDE_BOOST_PFR_USE_MODULES) && !defined(SERDE_BOOST_PFR_INTERFACE_UNIT)

#endif // SERDE_BOOST_PFR_TUPLE_SIZE_HPP
