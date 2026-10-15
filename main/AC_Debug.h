/*
 *  AC_Debug.h
 *
 *  debug but also really just build configuration
 */

#ifndef MAIN_AC_DEBUG_H_
#define MAIN_AC_DEBUG_H_

#define DEBUG_BUILD             // increased log levels and debug information
#define DEVELOPER_BUILD         // additional dev features, menus, etc.

/*
 *  we need more logging levels than E|W|I|D provide.
 *  in a debug build we include extra informational messages that don't need
 *  to be in production.
 *  - making them Debug and defaulting to Debug level gives too much noise
 *  - setting the default log level greater than Info to turn them off in prod
 *    suppresses too much potentially useful stuff.
 *  so, conditional Info and Debug
 */

#ifdef DEBUG_BUILD
#define ESP_LOGI_DEBUG(tag, fmt, args...) ESP_LOGI(tag, fmt, ##args)
#define ESP_LOGD_DEBUG(tag, fmt, args...) ESP_LOGD(tag, fmt, ##args)
#else
#define ESP_LOGI_DEBUG(tag, fmt, args...) do { } while (0)
#define ESP_LOGD_DEBUG(tag, fmt, args...) do { } while (0)
#endif

#endif /* MAIN_AC_DEBUG_H_ */
