/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "exec_parser.h"


static so_exec_t *exec;

static int fd;

static int pageSize = 4096;

static void segfault_handler(int signo, siginfo_t *info, void *ucontext)
{
	void *address = info->si_addr;
	void *lower = info->si_lower;
	void *upper = info->si_upper;
	int difference;
	int pageNo;
	int crtOffset;
	int code = info->si_code;
	int i, j;
	int *rc;
	int rez;

	if (code == SEGV_ACCERR) {
		signal(SIGSEGV, SIG_DFL);
		raise(SIGSEGV);
		return;
	}
	if (code == SEGV_MAPERR) {
		for (i = 0; i < exec->segments_no; i++) {
			if (address >= (void *)(exec->segments[i].vaddr) &&
				(void *)(exec->segments[i].vaddr +
					exec->segments[i].mem_size) > address) {
				if (!exec->segments[i].data) {
					exec->segments[i].data =
					(char *)malloc(
						(exec->segments[i].mem_size
					 / pageSize) * sizeof(char));
					if (!exec->segments[i].data)
						return;
					for (j = 0; j < exec->segments[i].mem_size
					/ pageSize; j++)
						((char *)
							(exec->segments[i].data))
					[j] = 0;
						//page marked as not mapped
				}
				difference = (int)(address -
					exec->segments[i].vaddr);
				pageNo = difference / pageSize;
				if (((char *)(exec->segments[i].data))
					[pageNo] == 1) {
					signal(SIGSEGV, SIG_DFL);
					raise(SIGSEGV);
					return;
				}
				rc = mmap((void *)(exec->segments[i].vaddr
					+ pageNo * pageSize), pageSize,
				 exec->segments[i].perm, MAP_PRIVATE|MAP_FIXED,
				 fd, exec->segments[i].offset +
				 pageNo * pageSize);
				if (rc == MAP_FAILED) {
					signal(SIGSEGV, SIG_DFL);
					raise(SIGSEGV);
					return;
				}
				((char *)(exec->segments[i].data))
				[pageNo] = 1; //page marked as mapped
				return;
			}
		}
		signal(SIGSEGV, SIG_DFL);
		raise(SIGSEGV);
	}
}

int so_init_loader(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = segfault_handler;
	sigaction(SIGSEGV, &sa, NULL);
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	fd = open(path, O_RDONLY);
	pageSize = getpagesize();
	int i, j;

	for (i = 0; i < exec->segments_no; i++) {
		int aux = exec->segments[i].mem_size -
		exec->segments[i].file_size;

		for (j = 0; j < aux; j++) {
			((char *)(exec->segments[i].vaddr +
				exec->segments[i].file_size))[j] = 0;
		}
	}
	if (fd == -1)
		return -1;
	if (!exec)
		return -1;
	so_start_exec(exec, argv);
	close(fd);
	return 0;
}
