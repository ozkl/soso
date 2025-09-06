/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef LOG_H
#define LOG_H

void log_initialize_serial();
void log_initialize(const char* file_name);
void log_printf(const char *format, ...);

#endif // LOG_H
