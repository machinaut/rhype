#ifndef OS_ARGS_H
#define OS_ARGS_H

#define VDEV_DESCRIPTION_SIZE	(2 * 1024)
struct os_args {
	char commandlineargs[0x100];
	char vdevargs[VDEV_DESCRIPTION_SIZE];
};

#endif
