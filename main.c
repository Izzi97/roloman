#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>

#include "example_schema.h"

int main() {
	int fd = open("foo.rolo", O_WRONLY|O_CREAT);
	if (fd < 0) exit(1);
	if (fchmod(fd, 0777) < 0) exit(1);

	rolo_writer_t w;
	if (!rolo_init_write(&w, fd)) exit(1);

	foo_t foo = (foo_t) { 42, 69 };
	bar_t bar = (bar_t) { 420, 1337 };

	if (!write_foo_t(&w, foo)) exit(1);
	if (!write_bar_t(&w, bar)) exit(1);
	if (!rolo_flush(&w)) exit(1);

	close(fd);

	fd = open("foo.rolo", O_RDONLY);
	if (fd < 0) exit(1);

	rolo_reader_t r;
	if (!rolo_init_read(&r, fd)) exit(1);

	rolo_entity_t e = {0};
	while (!rolo_read_complete(&r)) {
		if (!rolo_entity_read(&r, &e)) exit(1);
		printf("read entity %d\n", e.meta.element_type);
	}
}

