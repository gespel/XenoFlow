#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <microhttpd.h>

/**
 * @brief HTTP Server context structure
 */
struct http_server_ctx {
	struct MHD_Daemon *daemon;
	int port;
	void *config;  /* pointer to XenoFlowConfig */
};

/**
 * @brief Global HTTP server context
 */
extern struct http_server_ctx *http_server_ctx;

/**
 * @brief Start the HTTP server on the specified port
 * @param port Port number to listen on
 * @param config Pointer to XenoFlowConfig
 * @return 0 on success, -1 on failure
 */
int http_server_start(int port, void *config);

/**
 * @brief Stop the HTTP server
 */
void http_server_stop(void);

#endif /* HTTP_SERVER_H */
