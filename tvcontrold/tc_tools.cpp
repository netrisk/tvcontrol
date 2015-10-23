#include <tc_tools.h>
#include <unistd.h>

int tc_read_all(int fd, void *buf, int len)
{
	int i = 0;
	while (i < len) {
		int r = read(fd, ((char *)buf) + i, len - i);
		if (r < 1)
			return -1;
		i += r;
	}
	return 0;
}

int tc_write_all(int fd, const void *buf, int len)
{
	while (len) {
		int r = write(fd, buf, len);
		if (r < 1)
			return -1;
		buf = ((char *)buf) + r;
		len -= r;
	}
	return 0;
}
