#include "stdafx.h"
#include "common.h"

#include "fiber/lib_fiber.h"
#include "event.h"
#include "fiber.h"

typedef int (*select_fn)(int, fd_set *, fd_set *, fd_set *, struct timeval *);

static select_fn __sys_select = NULL;

#ifdef SYS_UNIX

static void hook_api(void)
{
	__sys_select = (select_fn) dlsym(RTLD_NEXT, "select");
	assert(__sys_select);
}

static pthread_once_t __once_control = PTHREAD_ONCE_INIT;

static void hook_init(void)
{
	if (pthread_once(&__once_control, hook_api) != 0) {
		abort();
	}
}

/****************************************************************************/

int select(int nfds, fd_set *readfds, fd_set *writefds,
	fd_set *exceptfds, struct timeval *timeout)
{
	struct pollfd *fds;
	int fd, timo, n, nready = 0;

	if (__sys_select == NULL)
		hook_init();

	if (!var_hook_sys_api)
		return __sys_select ? __sys_select
			(nfds, readfds, writefds, exceptfds, timeout) : -1;

	fds = (struct pollfd *) calloc(nfds + 1, sizeof(struct pollfd));

	for (fd = 0; fd < nfds; fd++) {
		if (readfds && FD_ISSET(fd, readfds)) {
			fds[fd].fd = fd;
			fds[fd].events |= POLLIN;
		}

		if (writefds && FD_ISSET(fd, writefds)) {
			fds[fd].fd = fd;
			fds[fd].events |= POLLOUT;
		}

		if (exceptfds && FD_ISSET(fd, exceptfds)) {
			fds[fd].fd = fd;
			fds[fd].events |= POLLERR | POLLHUP;
		}
	}

	if (timeout != NULL)
		timo = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
	else
		timo = -1;

	n = poll(fds, nfds, timo);

	if (readfds)
		FD_ZERO(readfds);
	if (writefds)
		FD_ZERO(writefds);
	if (exceptfds)
		FD_ZERO(exceptfds);

	for (fd = 0; fd < nfds && nready < n; fd++) {
		if (fds[fd].fd < 0 || fds[fd].fd != fd)
			continue;

		if (readfds && (fds[fd].revents & POLLIN)) {
			FD_SET(fd, readfds);
			nready++;
		}

		if (writefds && (fds[fd].revents & POLLOUT)) {
			FD_SET(fd, writefds);
			nready++;
		}

		if (exceptfds && (fds[fd].revents & (POLLERR | POLLHUP))) {
			FD_SET(fd, exceptfds);
			nready++;
		}
	}

	free(fds);
	return nready;
}

#endif
