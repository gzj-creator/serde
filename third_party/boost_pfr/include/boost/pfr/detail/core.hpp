// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef SERDE_BOOST_PFR_DETAIL_CORE_HPP
#define SERDE_BOOST_PFR_DETAIL_CORE_HPP
#pragma once

#include <third_party/boost_pfr/include/boost/pfr/detail/config.hpp>

// Each core provides `serde_boost_pfr::pfr::detail::tie_as_tuple` and
// `serde_boost_pfr::pfr::detail::for_each_field_dispatcher` functions.
//
// The whole PFR library is build on top of those two functions.
#if SERDE_BOOST_PFR_USE_CPP17
#   include <third_party/boost_pfr/include/boost/pfr/detail/core17.hpp>
#elif SERDE_BOOST_PFR_USE_LOOPHOLE
#   include <third_party/boost_pfr/include/boost/pfr/detail/core14_loophole.hpp>
#else
#   include <third_party/boost_pfr/include/boost/pfr/detail/core14_classic.hpp>
#endif

#endif // SERDE_BOOST_PFR_DETAIL_CORE_HPP
