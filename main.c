#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include "roloman.h"

typedef struct {
	int bar;
	int baz;
} foo_t;

int main() {
	int fd = open("foo.rolo", O_WRONLY|O_CREAT);
	if (fd < 0) exit(1);
	if (fchmod(fd, 0777) < 0) exit(1);

	rolo_writer_t w;
	if (!rolo_init_write(&w, fd)) exit(1);

	foo_t foo = (foo_t) { 42, 69 };
	rolo_entity_t foo_ent;
	foo_ent.meta = (rolo_entity_meta_t){ sizeof(foo), 0, 1337 };
	foo_ent.data = &foo;

	if (!rolo_entity_write(&w, &foo_ent)) exit(1);
	if (!rolo_flush(&w)) exit(1);

	close(fd);

	fd = open("foo.rolo", O_RDONLY);
	if (fd < 0) exit(1);

	rolo_reader_t r;
	if (!rolo_init_read(&r, fd)) exit(1);

	rolo_entity_t e = {0};
	while (!rolo_read_complete(&r)) {
		if (!rolo_entity_read(&r, &e)) exit(1);
		if (e.meta.element_type == 1337)
			printf(
				"bar: %d\n"
				"baz: %d\n",
				((foo_t*)e.data)->bar,
				((foo_t*)e.data)->baz
			);
	}
}

