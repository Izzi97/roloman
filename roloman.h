#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "rolo_schema_hash.h"

#define ROLO_MAJOR 0
#define ROLO_MINOR 0
#define ROLO_PATCH 0
#define ROLO_NO_COMPRESSION 0

typedef struct {
	uint8_t 	magic[4];
	uint16_t 	version[3];
	uint16_t	alignment_padding; // for uint32 alignment
	uint8_t		schema_hash[16];
	uint32_t	compression;
	uint32_t	body_size;
} rolo_head_t;

typedef struct {
	int fd;
	size_t limit;
	size_t count;
} rolo_reader_t;

typedef struct {
	rolo_head_t head;
	void *buffer;
	size_t limit;
	size_t count;
	int fd;
} rolo_writer_t;

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
	uint32_t length;
	uint32_t element_type;
} rolo_entity_meta_t;

typedef struct {
	rolo_entity_meta_t 	meta;
	void*			data;
} rolo_entity_t;

bool rolo_init_read(rolo_reader_t *reader, int fd);
bool rolo_read(rolo_reader_t *reader, void *dest, size_t count);
#define rolo_head_read(reader, dest) rolo_read(reader, dest, sizeof(rolo_head_t))
bool rolo_entity_read(rolo_reader_t *reader, rolo_entity_t *dest);
bool rolo_read_complete(rolo_reader_t *reader);

bool rolo_init_write(rolo_writer_t *rolo, int fd);
bool rolo_write(rolo_writer_t *writer, void *src, size_t count);
bool rolo_entity_write(rolo_writer_t *writer, rolo_entity_t *src);
bool rolo_flush(rolo_writer_t *writer);

#define rolo_register(type, tag) \
	bool write_##type(rolo_writer_t *w, type t) { \
		if (!w) return false; \
		rolo_entity_t e = (rolo_entity_t){ \
			.meta = (rolo_entity_meta_t){ \
				.length = sizeof(type), \
				.element_type = tag \
			}, \
			.data = (void*)&t \
		}; \
		return rolo_entity_write(w, &e); \
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

	for (size_t i = 0; i < 16; i++) {
		if (head->schema_hash[i] != ROLO_SCHEMA_HASH[i]) goto fail;
	}

	// no compression support for now...
	if (head->compression != ROLO_NO_COMPRESSION) goto fail;
	
	reader->limit = sizeof(rolo_head_t) + head->body_size;
	return true;
fail:
	free(head);
	return false;
}

bool rolo_read(rolo_reader_t *reader, void *dest, size_t size) {
	if (!reader || !dest) return false;

	ssize_t ret;
	do {
		ret = read(reader->fd, dest, size);
		if (ret < 0) return false;

		dest += ret;
		reader->count += ret;
		size -= ret;
	} while(ret > 0);

	if (size != 0) return false;
	return true;
}

bool rolo_entity_read(rolo_reader_t *reader, rolo_entity_t *dest) {
	if (!reader || !dest) return false;

	rolo_entity_meta_t meta = {0};
	if (!rolo_read(reader, &meta, sizeof(meta))) return false;
	
	void *data = malloc(sizeof(meta.length));
	if (!data) return false;
	if (!rolo_read(reader, data, meta.length)) return false;

	dest->meta = meta;
	dest->data = data;
	return true;
}

bool rolo_read_complete(rolo_reader_t *reader) {
	return reader->count >= reader->limit;
}



bool rolo_init_write(rolo_writer_t *writer, int fd) {
	if (!writer) return false;

	writer->fd = fd;
	writer->limit = 128;
	writer->count = 0;
	writer->buffer = malloc(writer->limit);
	if (!writer->buffer) return false;

	writer->head = (rolo_head_t){
		.magic = "ROLO",
                .version = { ROLO_MAJOR, ROLO_MINOR, ROLO_PATCH },
		.alignment_padding = 0,
                .compression = 0, // ignore this for now ...
                .body_size = 0
	};
	for (size_t i = 0; i < 16; i++)
		writer->head.schema_hash[i] = ROLO_SCHEMA_HASH[i];

	return true;
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

bool rolo_entity_write(rolo_writer_t *writer, rolo_entity_t *src) {
	if (!writer || !src) return false;

	if (!rolo_write(writer, (void*)&(src->meta), sizeof(rolo_entity_meta_t))) return false;
	if (!rolo_write(writer, src->data, src->meta.length)) return false;
	writer->head.body_size += sizeof(rolo_entity_meta_t) + src->meta.length;
	return true;
}

bool rolo_flush(rolo_writer_t *writer) {
	if (!writer || !writer->buffer) return false;

	// flush head
	ssize_t ret;
	size_t written = 0;
	size_t count = sizeof(rolo_head_t);
	do {
		ret = write(writer->fd, &(writer->head) + written, count);
		if (ret < 0) return false;
		
		written += ret;
		count -= ret;
	} while (count > 0);

	// flush body
	written = 0;
	count = writer->count;
	do {
		ret = write(writer->fd, writer->buffer + written, count);
		if (ret < 0) return false;

		written += ret;
		count -= ret;
	} while (count > 0);

	return true;
}

