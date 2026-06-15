/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#if defined(__clang__)
    #define CIMBAR_ALWAYS_INLINE __attribute__((always_inline))
    #define CIMBAR_FLATTEN       __attribute__((flatten))
#else
    #define CIMBAR_ALWAYS_INLINE
    #define CIMBAR_FLATTEN
#endif

