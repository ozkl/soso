/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#ifndef FPU_H
#define FPU_H

struct Thread;

void fpu_initialize();
void fpu_initialize_thread(struct Thread* thread);
void fpu_clone_thread(struct Thread* destination, struct Thread* source);
void fpu_save(struct Thread* thread);
void fpu_restore(struct Thread* thread);

#endif //FPU_H
