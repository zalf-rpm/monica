#+build linux, darwin, openbsd, freebsd, netbsd
package zeromq

import "core:sys/posix"

/***************************************************************************** */
/* 0MQ errors. */
/***************************************************************************** */
ENOTSUP :: posix.ENOTSUP
EPROTONOSUPPORT :: posix.EPROTONOSUPPORT
ENOBUFS :: posix.ENOBUFS
ENETDOWN :: posix.ENETDOWN
EADDRINUSE :: posix.EADDRINUSE
EADDRNOTAVAIL :: posix.EADDRNOTAVAIL
ECONNREFUSED :: posix.ECONNREFUSED
EINPROGRESS :: posix.EINPROGRESS
ENOTSOCK :: posix.ENOTSOCK
EMSGSIZE :: posix.EMSGSIZE
EAFNOSUPPORT :: posix.EAFNOSUPPORT
ENETUNREACH :: posix.ENETUNREACH
ECONNABORTED :: posix.ECONNABORTED
ECONNRESET :: posix.ECONNRESET
ENOTCONN :: posix.ENOTCONN
ETIMEDOUT :: posix.ETIMEDOUT
EHOSTUNREACH :: posix.EHOSTUNREACH
ENETRESET :: posix.ENETRESET
