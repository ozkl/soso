/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef SERIAL_H
#define SERIAL_H

void serial_initialize();
void serial_initialize_file_device();
void serial_printf(const char *format, ...);

#endif // SERIAL_H
