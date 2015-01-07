// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "msg.h"

#ifdef __cplusplus
extern "C" {
#endif

void	casycom_init (void) noexcept;
void	casycom_framework_init (void) noexcept;
int	casycom_main (void) noexcept;
void	casycom_quit (int exitCode) noexcept;

#ifdef __cplusplus
} // extern "C"
#endif
