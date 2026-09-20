#ifndef PORTAL_H
#define PORTAL_H

#include <stdbool.h>
#include <esp_err.h>

esp_err_t portal_init();
esp_err_t portal_enable( bool onoff );

#endif /*PORTAL_H*/
