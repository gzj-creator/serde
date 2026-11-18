// Copyright (c) 2022 Denis Mikhailov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SERDE_BOOST_PFR_DETAIL_TRAITS_FWD_HPP
#define SERDE_BOOST_PFR_DETAIL_TRAITS_FWD_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

#if !defined(SERDE_BOOST_PFR_USE_MODULES) || defined(SERDE_BOOST_PFR_INTERFACE_UNIT)

namespace serde_boost_pfr { namespace pfr {

SERDE_BOOST_PFR_BEGIN_MODULE_EXPORT

template<class T, class WhatFor>
struct is_reflectable;

SERDE_BOOST_PFR_END_MODULE_EXPORT

}} // namespace serde_boost_pfr::pfr

#endif  // #if !defined(SERDE_BOOST_PFR_USE_MODULES) || defined(SERDE_BOOST_PFR_INTERFACE_UNIT)

#endif // SERDE_BOOST_PFR_DETAIL_TRAITS_FWD_HPP
