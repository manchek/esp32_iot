#ifndef PORTAL_H
#define PORTAL_H

/*
 *  portal.c - embedded mini server for configuration/status web portal.
 */

#include <stdbool.h>
#include <esp_err.h>

/**
 *  must be called once before any other portal functions.
 */
esp_err_t portal_init();

/**
 *  can be called to refresh the login token.
 *  anyone using the old token will be logged out.
 */
void portal_generate_login_token();

/**
 *  enable or disable the web portal.
 *  portal is accessible on any netif that is currently up.
 */
esp_err_t portal_enable( bool onoff );

#endif /*PORTAL_H*/
