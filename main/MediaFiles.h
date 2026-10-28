#ifndef MEDIA_FILES_H
#define MEDIA_FILES_H

#include <stdbool.h>
#include <stdint.h>

bool find_media( const char *path, const char **mimetype, int *size, const uint8_t **data );

bool find_media_len( const char *path, int pathlen, const char **mimetype, int *size, const uint8_t **data );

#endif /*MEDIA_FILES_H*/
