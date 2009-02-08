/* $Id: osdep-freebsd.c,v 1.11 2009-02-08 13:03:43 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __FreeBSD__

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char	*get_argv0(int, char *);
char	*get_proc_argv0(pid_t);

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))

#define is_runnable(p) \
	((p)->ki_stat == SRUN || (p)->ki_stat == SIDL)
#define is_stopped(p) \
	((p)->ki_stat == SSTOP || (p)->ki_stat == SZOMB)

char *
get_argv0(int fd, char *tty)
{
	int		 mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PGRP, 0 };
	struct stat	 sb;
	size_t		 len;
	struct kinfo_proc *buf, *newbuf, *p, *bestp;
	char		*procname;
	u_int		 i;

	buf = NULL;

	if (stat(tty, &sb) == -1)
		return (NULL);
	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

retry:
	if (sysctl(mib, nitems(mib), NULL, &len, NULL, 0) == -1)
		return (NULL);
	len = (len * 5) / 4;

	if ((newbuf = realloc(buf, len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf = newbuf;

	if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		free(buf);
		return (NULL);
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (struct kinfo_proc); i++) {
		if (buf[i].ki_tdev != sb.st_rdev)
			continue;
		p = &buf[i];
		if (bestp == NULL) {
			bestp = p;
			continue;
		}

		if (is_runnable(p) && !is_runnable(bestp))
			bestp = p;
		else if (!is_runnable(p) && is_runnable(bestp))
			continue;

		if (!is_stopped(p) && is_stopped(bestp))
			bestp = p;
		else if (is_stopped(p) && !is_stopped(bestp))
			continue;

		if (p->ki_estcpu > bestp->ki_estcpu)
			bestp = p;
		else if (p->ki_estcpu < bestp->ki_estcpu)
			continue;

		if (p->ki_slptime < bestp->ki_slptime)
			bestp = p;
		else if (p->ki_slptime > bestp->ki_slptime)
			continue;

		if (strcmp(p->ki_comm, p->ki_comm) < 0)
			bestp = p;
		else if (strcmp(p->ki_comm, p->ki_comm) > 0)
			continue;

		if (p->ki_pid > bestp->ki_pid)
			bestp = p;
	}
	if (bestp != NULL) {
		procname = get_proc_argv0(bestp->ki_pid);
		if (procname == NULL || *procname == '\0') {
			free(procname);
			procname = strdup(bestp->ki_comm);
		}
	} else
		procname = NULL;


	free(buf);
	return (procname);
}

char *
get_proc_argv0(pid_t pid)
{
	int	mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ARGS, 0 };
        size_t	size, size2;
	char   *args, *args2, *procname;

	mib[3] = pid;
	procname = NULL;

	args = NULL;
	size = 128;
	while (size < SIZE_MAX / 2) {
		size *= 2;
		if ((args2 = realloc(args, size)) == NULL)
			break;
		args = args2;
		size2 = size;
		if (sysctl(mib, 4, args, &size2, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			break;
		}
		if (size2 > 0 && *args != '\0')
			procname = strdup(args);
		break;
	}
	free(args);

	return (procname);
}

#endif
