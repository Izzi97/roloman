#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define ROLO_MAJOR 0
#define ROLO_MINOR 0
#define ROLO_PATCH 0

typedef struct {
	uint8_t 	magic[4];
	uint16_t 	version[3];
	uint8_t		schema_hash[68];
	uint32_t	compression;
	uint32_t	body_size;
} rolo_head_t;

typedef struct {
	int fd;
	size_t limit;
	size_t count;
} rolo_reader_t;

typedef struct {
	void *buffer;
	size_t limit;
	size_t count;
	int fd;
} rolo_writer_t;

typedef enum {
	ROLO_NO_CONTAINER,
	ROLO_ARRAY
} rolo_container_type_t;

typedef enum {
	ROLO_UINT8,
	ROLO_UINT16,
	ROLO_UINT32,
	ROLO_UINT64,
	ROLO_INT8,
	ROLO_INT16,
	ROLO_INT32,
	ROLO_INT64,
	ROLO_FLOAT,
	ROLO_DOUBLE,
	ROLO_STRING
} rolo_element_type_t;

#define ROLO_NEXT_ELEMENT_TYPE ROLO_STRING + 1

typedef struct {
	uint32_t 	length;
	uint32_t 	container_type;
	uint32_t 	element_type;
	void*		data;
} rolo_entity_t;

bool rolo_init_read(rolo_reader_t *reader, int fd);
bool rolo_read(rolo_reader_t *reader, void *dest, size_t count);
bool rolo_read_complete(rolo_reader_t *reader);

bool rolo_init_write(rolo_writer_t *rolo, int fd);
bool rolo_write(rolo_writer_t *writer, void *src, size_t count);
bool rolo_flush(rolo_writer_t *writer);

#define rolo_head_read(reader, dest) rolo_read(reader, dest, sizeof(rolo_head_t))
#define rolo_head_write(writer, src) rolo_write(writer, src, sizeof(rolo_head_t))
bool rolo_entity_read(rolo_reader_t *reader, rolo_entity_t *dest);
bool rolo_entity_write(rolo_writer_t *writer, rolo_entity_t *src);

bool rolo_read(rolo_reader_t *reader, void *dest, size_t count) {
	if (!reader || !dest) return false;

	ssize_t ret;
	do {
		ret = read(reader->fd, dest, count);
		if (ret < 0) return false;

		dest += ret;
		count -= ret;
	} while(ret > 0);

	return true;
}

bool rolo_read_complete(rolo_reader_t *reader) {
	return reader->count >= reader->limit;
}

bool rolo_write(rolo_writer_t *writer, void *src, size_t count) {
	if (!writer || !src) return false;

	size_t free = writer->limit - writer->count;
	if (free < count) {
		writer->limit *= 2;
		writer->buffer = realloc(writer->buffer, writer->limit);
		if (!writer->buffer) return false;
	}
	memcpy(writer->buffer + writer->count, src, count);
	writer->count += count;

	return true;
}

// TODO: test this
bool rolo_flush(rolo_writer_t *writer) {
	if (!writer || !writer->buffer) return false;

	ssize_t ret;
	size_t written = 0;
	size_t count = writer->count;
	do {
		ret = write(writer->fd, writer->buffer + written, count);
		if (ret < 0) return false;
		
		written += ret;
		count -= ret;
	} while (ret > 0);

	return true;
}

bool rolo_init_read(rolo_reader_t *reader, int fd) {
	rolo_head_t *head = NULL;

	if (!reader) goto fail;

	reader->fd = fd;
	reader->limit = 0;
	reader->count = 0;

	head = malloc(sizeof(rolo_head_t));
	if (!head) goto fail;

	if (!rolo_head_read(reader, head)) goto fail;

	unsigned char *magic = head->magic;
	if (!(magic[0] == 'R' && magic[1] == 'O' && magic[2] == 'L' && magic[3] == 'O'))
		goto fail;

	uint16_t *version = head->version;
	if (!(version[0] == ROLO_MAJOR && version[1] == ROLO_MINOR && version[2] == ROLO_PATCH))
		goto fail;

	// ignore schema hash for now...
	// ignore compression for now...
	
	reader->limit = head->body_size;
	return true;
fail:
	free(head);
	return false;
}

bool rolo_init_write(rolo_writer_t *writer, int fd) {
	if (!writer) return false;

	writer->fd = fd;
	writer->limit = 128;
	writer->count = 0;
	writer->buffer = malloc(writer->limit);
	if (!writer->buffer) return false;

	rolo_head_t head = (rolo_head_t){
		.magic = "ROLO",
                .version = { ROLO_MAJOR, ROLO_MINOR, ROLO_PATCH },
                .schema_hash = {0}, // ignore this for now ...
                .compression = 0, // ignore this for now ...
                .body_size = 0
	};
	rolo_head_write(writer, &head);
	return true;
}

