/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#ifndef CIMBAR_EXPORT_H
#define CIMBAR_EXPORT_H

#if defined(_WIN32) || defined(_WIN64)
#  if defined(CIMBAR_BUILD_SHARED)
#    define CIMBAR_EXPORT __declspec(dllexport)
#  elif defined(CIMBAR_USE_SHARED)
#    define CIMBAR_EXPORT __declspec(dllimport)
#  else
#    define CIMBAR_EXPORT
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  if defined(CIMBAR_BUILD_SHARED)
#    define CIMBAR_EXPORT __attribute__((visibility("default")))
#  else
#    define CIMBAR_EXPORT
#  endif
#else
#  define CIMBAR_EXPORT
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define CIMBAR_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#  define CIMBAR_DEPRECATED __declspec(deprecated)
#else
#  define CIMBAR_DEPRECATED
#endif

#endif
