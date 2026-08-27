/*!
 * Copyright 2025 NXP
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * \file fwk_errno.h
 * \brief this header aims to map std and posix errno to all toolchains used with the connectivity framework.
 *
 */

#ifndef _FWK_ERRNO_H_
#define _FWK_ERRNO_H_

#ifndef NOERROR
#define NOERROR 0
#endif

/* C99 only requires to define EDOM, EILSEQ and ERANGE, so only those 3 are guaranteed to be defined by any toolchain
 * GCC defines the POSIX codes, so no need to define them here for this toolchain */
#include <errno.h>

#if defined(__IAR_SYSTEMS_ICC__) || defined(__CC_ARM) || defined(__ARMCC_VERSION)

/* IAR and MDK toolchains don't define POSIX error codes, so we re-define those we need */

#ifndef ENOMEM
#define ENOMEM 12 /* Not enough space */
#endif

#ifndef ENODEV
#define ENODEV 19 /* No such device */
#endif

#ifndef EINVAL
#define EINVAL 22 /* Invalid argument */
#endif

#endif /* defined(__IAR_SYSTEMS_ICC__) || defined(__CC_ARM) || defined(__ARMCC_VERSION) */

#ifndef EPERM
#define EPERM 1 /* Operation not permitted */
#endif
#ifndef ENOENT
#define ENOENT 2 /* No such file or directory */
#endif
#ifndef ENOFILE
#define ENOFILE ENOENT /* Alias for ENOENT */
#endif
#ifndef ESRCH
#define ESRCH 3 /* No such process */
#endif
#ifndef EINTR
#define EINTR 4 /* Interrupted system call */
#endif
#ifndef EIO
#define EIO 5 /* I/O error */
#endif
#ifndef ENXIO
#define ENXIO 6 /* No such device or address */
#endif
#ifndef E2BIG
#define E2BIG 7 /* Argument list too long */
#endif
#ifndef ENOEXEC
#define ENOEXEC 8 /* Executable format error */
#endif
#ifndef EBADF
#define EBADF 9 /* Bad file descriptor */
#endif
#ifndef ECHILD
#define ECHILD 10 /* No child processes */
#endif
#ifndef EAGAIN
#define EAGAIN 11 /* Resource temporarily unavailable */
#endif
#ifndef ENOMEM
#define ENOMEM 12 /* Not enough memory */
#endif
#ifndef EACCES
#define EACCES 13 /* Permission denied */
#endif
#ifndef EFAULT
#define EFAULT 14 /* Bad address */
#endif
#ifndef EBUSY
#define EBUSY 16 /* Device or resource busy */
#endif
#ifndef EEXIST
#define EEXIST 17 /* File exists */
#endif
#ifndef EXDEV
#define EXDEV 18 /* Cross-device link */
#endif
#ifndef ENODEV
#define ENODEV 19 /* No such device */
#endif
#ifndef ENOTDIR
#define ENOTDIR 20 /* Not a directory */
#endif
#ifndef EISDIR
#define EISDIR 21 /* Is a directory */
#endif
#ifndef ENFILE
#define ENFILE 23 /* Too many open files in system */
#endif
#ifndef ENOTTY
#define ENOTTY 25 /* Inappropriate ioctl for device */
#endif
#ifndef EFBIG
#define EFBIG 27 /* File too large */
#endif
#ifndef ENOSPC
#define ENOSPC 28 /* No space left on device */
#endif
#ifndef ESPIPE
#define ESPIPE 29 /* Illegal seek */
#endif
#ifndef EROFS
#define EROFS 30 /* Read-only file system */
#endif
#ifndef EMLINK
#define EMLINK 31 /* Too many links */
#endif
#ifndef EPIPE
#define EPIPE 32 /* Broken pipe */
#endif
#ifndef EDOM
#define EDOM 33 /* Numerical argument out of domain */
#endif
#ifndef EDEADLK
#define EDEADLK 36 /* Resource deadlock would occur */
#endif

#ifndef ENAMETOOLONG
#define ENAMETOOLONG 38 /* Filename or pathname too long */
#endif

#ifndef ENOLCK
#define ENOLCK 39 /* No locks available */
#endif

#ifndef ENOSYS
#define ENOSYS 40 /* Function not implemented */
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY 41 /* Directory not empty */
#endif

#ifndef EDEADLOCK
#define EDEADLOCK EDEADLK
#endif
/* Posix thread extensions.  */

#ifndef ENOTSUP
#define ENOTSUP 129 /* Operation not supported */
#endif

#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL 101 /* Cannot assign requested address */
#endif

#ifndef ENOBUFS
#define ENOBUFS 119 /* No buffer space available */
#endif

#ifndef EALREADY
#define EALREADY 103 /* Operation already in progress */
#endif

#ifndef EMSGSIZE
#define EMSGSIZE 115 /* Message too long */
#endif

#ifndef ECANCELED
#define ECANCELED 105 /* Operation canceled */
#endif

#ifndef EINPROGRESS
#define EINPROGRESS 112 /* Operation in progress */
#endif

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 130 /* Operation not supported */
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK 140 /* Operation would block */
#endif

#ifndef EOWNERDEAD
#define EOWNERDEAD 133
#endif

#ifndef EPROTO
#define EPROTO 134 /* Protocol error */
#endif

#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT 135 /* Protocol not supported */
#endif

#ifndef EBADMSG
#define EBADMSG 104 /* Bad message */
#endif

#ifndef EIDRM
#define EIDRM 111 /* Identifier removed */
#endif

#ifndef ENODATA
#define ENODATA 120
#endif

#ifndef ENOLINK
#define ENOLINK 121
#endif

#ifndef ENOMSG
#define ENOMSG 122
#endif

#ifndef ENOTRECOVERABLE
#define ENOTRECOVERABLE 127 /* State not recoverable */
#endif

#ifndef ETIME
#define ETIME 137 /* Timer expired */
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 138 /* Connection timed out */
#endif

#ifndef EOVERFLOW
#define EOVERFLOW 132 /* Value too large to store */
#endif

#endif /* _FWK_ERRNO_H_ */
