
set(GENERATED_C ${CMAKE_CURRENT_SOURCE_DIR}/MediaFiles.c)

idf_component_register(
	SRCS
		"iot.c"
		"network.c"
		"nowfun.c"
		"portal.c"
		${GENERATED_C}
	INCLUDE_DIRS
	"."
)

# must be after idf_component_register, or protected with NOT CMAKE_BUILD_EARLY_EXPANSION
add_custom_command(
	OUTPUT ${GENERATED_C}
	COMMAND ${CMAKE_SOURCE_DIR}/tools/media-compiler -o ${GENERATED_C} ${CMAKE_SOURCE_DIR}/media
	DEPENDS ${CMAKE_SOURCE_DIR}/tools/media-compiler
	COMMENT "Generating media from files"
)
