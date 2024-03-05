/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMSPACE_PREDEFINED_INTERNAL_H
#define UMF_MEMSPACE_PREDEFINED_INTERNAL_H 1

#ifdef __cplusplus
extern "C" {
#endif

///
/// \brief Destroys the predefined host all memspace.
///
void umfMemspaceHostAllDestroy(void);

///
/// \brief Destroys the predefined high bandwidth memspace.
///
void umfMemspaceHBWDestroy(void);

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMSPACE_PREDEFINED_INTERNAL_H */
