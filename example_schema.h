#include "roloman.h"

typedef struct {
	uint32_t a;
	uint32_t b;
} foo_t;

typedef struct {
	uint32_t c;
	uint32_t d;
} bar_t;

typedef enum {
	FOO = ROLO_NEXT_ELEMENT_TYPE,
	BAR
} custom_types;

rolo_register(foo_t, FOO);
rolo_register(bar_t, BAR);

