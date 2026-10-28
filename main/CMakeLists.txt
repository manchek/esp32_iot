
set(CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Config.yaml)
set(GENERATED_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/Config.c)

set(GENERATED_MEDIA ${CMAKE_CURRENT_SOURCE_DIR}/MediaFilesData.h)

idf_component_register(
	SRCS
		"iot.c"
		"network.c"
		"nowfun.c"
		"portal.c"
		"FormUploader.c"
		"MediaFiles.c"
		${GENERATED_MEDIA}
		${GENERATED_CONFIG}
		"AC_NVS.c"
	INCLUDE_DIRS
	"."
)

# must be after idf_component_register, or protected with NOT CMAKE_BUILD_EARLY_EXPANSION
add_custom_command(
	OUTPUT ${GENERATED_MEDIA}
	COMMAND ${CMAKE_SOURCE_DIR}/tools/media-compiler -o ${GENERATED_MEDIA} ${CMAKE_SOURCE_DIR}/media
	DEPENDS ${CMAKE_SOURCE_DIR}/tools/media-compiler
	COMMENT "Generating media from files"
)

# must be after idf_component_register, or protected with NOT CMAKE_BUILD_EARLY_EXPANSION
add_custom_command(
	OUTPUT ${GENERATED_CONFIG}
	COMMAND ${CMAKE_SOURCE_DIR}/tools/ac_config_compiler.py ${CONFIG_FILE}
	DEPENDS ${CONFIG_FILE} ${CMAKE_SOURCE_DIR}/tools/ac_config_compiler.py
	COMMENT "Generating C code from config"
)
