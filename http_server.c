#include <string.h>
#include <stdlib.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <doca_log.h>

#include "http_server.h"

DOCA_LOG_REGISTER(HTTP_SERVER);

/* Global HTTP server context */
struct http_server_ctx *http_server_ctx = NULL;

/* HTTP Server Request Handler */
static enum MHD_Result http_request_handler(void *cls, struct MHD_Connection *connection,
                                 const char *url, const char *method,
                                 const char *version, const char *upload_data,
                                 size_t *upload_data_size, void **con_cls)
{
struct MHD_Response *response;
enum MHD_Result ret;

if (strcmp(url, "/api") == 0 && strcmp(method, "GET") == 0) {
/* Create JSON response */
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "message", "Hello World! XenoFlow REST API is running.");
cJSON_AddStringToObject(root, "status", "ok");
cJSON_AddNumberToObject(root, "version", 1.0);

char *json_str = cJSON_Print(root);
response = MHD_create_response_from_buffer(strlen(json_str),
(void *)json_str,
MHD_RESPMEM_MUST_FREE);
MHD_add_response_header(response, "Content-Type", "application/json");
ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
MHD_destroy_response(response);
cJSON_Delete(root);
return ret;
}

/* 404 Response */
cJSON *error = cJSON_CreateObject();
cJSON_AddStringToObject(error, "error", "Endpoint not found");
char *error_str = cJSON_Print(error);
response = MHD_create_response_from_buffer(strlen(error_str),
(void *)error_str,
MHD_RESPMEM_MUST_FREE);
MHD_add_response_header(response, "Content-Type", "application/json");
ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
MHD_destroy_response(response);
cJSON_Delete(error);
return ret;
}

/**
 * @brief Start the HTTP server on the specified port
 * @param port Port number to listen on
 * @return 0 on success, -1 on failure
 */
int http_server_start(int port)
{
http_server_ctx = malloc(sizeof(struct http_server_ctx));
if (!http_server_ctx) {
DOCA_LOG_ERR("Failed to allocate HTTP server context");
return -1;
}

http_server_ctx->port = port;
http_server_ctx->daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 
   http_server_ctx->port,
   NULL, NULL,
   &http_request_handler, NULL,
   MHD_OPTION_END);

if (http_server_ctx->daemon == NULL) {
DOCA_LOG_ERR("Failed to start HTTP server on port %d", http_server_ctx->port);
free(http_server_ctx);
http_server_ctx = NULL;
return -1;
}

DOCA_LOG_INFO("HTTP server started on port %d", http_server_ctx->port);
return 0;
}

/**
 * @brief Stop the HTTP server
 */
void http_server_stop(void)
{
if (http_server_ctx != NULL) {
if (http_server_ctx->daemon != NULL) {
MHD_stop_daemon(http_server_ctx->daemon);
DOCA_LOG_INFO("HTTP server stopped");
}
free(http_server_ctx);
http_server_ctx = NULL;
}
}
