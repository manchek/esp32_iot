
#include "MediaFiles.h"
#include "MediaFilesData.h"

bool
find_media_len( const char *path, int pathlen, const char **mimetype, int *size, const uint8_t **data )
{
	if (pathlen < 0) pathlen = strlen(path);
	for (int i = 0; i < sizeof(media)/sizeof(media[0]); i++) {
		if (pathlen == media[i].pathlen && !strncmp(path, media[i].path, pathlen)) {
			*mimetype = media[i].mimetype;
			*size = media[i].size;
			*data = media[i].data;
			return true;
		}
	}
	return false;
}

bool
find_media( const char *path, const char **mimetype, int *size, const uint8_t **data )
{
	return find_media_len(path, strlen(path), mimetype, size, data);
}

