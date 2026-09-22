#ifndef MEDIA_FILES_H
#define MEDIA_FILES_H

#include <stdbool.h>
#include <stdint.h>

bool find_media( const char *filename, const char **mimetype, int *size, const uint8_t **data );

#endif /*MEDIA_FILES_H*/
