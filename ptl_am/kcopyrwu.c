/*
 * Copyright (c) 2006-2010. QLogic Corporation. All rights reserved.
 * Copyright (c) 2003-2006, PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/types.h>
#include <unistd.h>

#include "kcopyrw.h"

#define KCOPY_GET_SYSCALL 1
#define KCOPY_PUT_SYSCALL 2
#define KCOPY_ABI_SYSCALL 3

struct kcopy_syscall {
	uint32_t tag;
	pid_t    pid;
	uint64_t n;
	uint64_t src;
	uint64_t dst;
};

int64_t kcopy_get(int fd, pid_t pid, const void *src, void *dst, int64_t n) {
	struct kcopy_syscall e = {
		.tag = KCOPY_GET_SYSCALL,
		.pid = pid,
		.n = n,
		.src = (uint64_t) (uintptr_t) src,
		.dst = (uint64_t) (uintptr_t) dst
	};
	int ret;

	ret = write(fd, &e, sizeof(e));
	if (ret == sizeof(e))
		ret = n;
	else if (ret > 0 && ret != sizeof(e))
		ret = 0;
	
	return ret;
}

int64_t kcopy_put(int fd, const void *src, pid_t pid, void *dst, int64_t n) {
	struct kcopy_syscall e = {
		.tag = KCOPY_PUT_SYSCALL,
		.pid = pid,
		.n = n,
		.src = (uint64_t) (uintptr_t) src,
		.dst = (uint64_t) (uintptr_t) dst
	};
	int ret;

	ret = write(fd, &e, sizeof(e));
	if (ret == sizeof(e))
		ret = n;
	else if (ret > 0 && ret != sizeof(e))
		ret = 0;
	
	return ret;
}

int kcopy_abi(int fd) {
	int32_t abi;
	struct kcopy_syscall e = {
		.tag = KCOPY_ABI_SYSCALL,
		.dst = (uint64_t) (uintptr_t) &abi
	};
	int ret;

	ret = write(fd, &e, sizeof(e));
	if (ret == sizeof(e))
		ret = abi;
	else if (ret > 0 && ret != sizeof(e))
		ret = 0;

	return ret;
}
