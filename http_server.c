#include <string.h>
#include <stdlib.h>
#include <microhttpd.h>
#include <cjson/cJSON.h>
#include <doca_log.h>

#include "http_server.h"
#include "core.h"

DOCA_LOG_REGISTER(HTTP_SERVER);

struct http_server_ctx *http_server_ctx = NULL;

struct post_data {
	char *data;
	size_t size;
	int received_data;
};

static enum MHD_Result http_request_handler(void *cls, struct MHD_Connection *connection,
					     const char *url, const char *method,
					     const char *version, const char *upload_data,
					     size_t *upload_data_size, void **con_cls)
{
	struct MHD_Response *response;
	enum MHD_Result ret;

	if (strcmp(url, "/api") == 0 && strcmp(method, "GET") == 0) {		
		char *json_str = handle_base_path_request();
		
		response = MHD_create_response_from_buffer(strlen(json_str), (void *)json_str, MHD_RESPMEM_MUST_FREE);
		MHD_add_response_header(response, "Content-Type", "application/json");
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
		return ret;
	}
	if (strcmp(url, "/api") == 0 && strcmp(method, "POST") == 0) {
		struct post_data *post = (struct post_data *)*con_cls;
		
		if (post == NULL) {
			post = malloc(sizeof(struct post_data));
			post->data = NULL;
			post->size = 0;
			post->received_data = 0;
			*con_cls = post;
			return MHD_YES;
		}
		
		if (*upload_data_size > 0) {
			post->data = realloc(post->data, post->size + *upload_data_size + 1);
			memcpy(post->data + post->size, upload_data, *upload_data_size);
			post->size += *upload_data_size;
			post->data[post->size] = '\0';
			post->received_data = 1;
			*upload_data_size = 0;
			return MHD_YES;
		}
		
		DOCA_LOG_INFO("Received POST: %s", post->data ? post->data : "(empty)");
		char *str = malloc(64);
		snprintf(str, 64, "{\"status\": \"Ok\"}");
		response = MHD_create_response_from_buffer(strlen(str), (void*)str, MHD_RESPMEM_MUST_FREE);
		MHD_add_response_header(response, "Content-Type", "application/json");
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
		
		if (post->data) free(post->data);
		free(post);
		*con_cls = NULL;
		
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

char* handle_base_path_request() {
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "message", "XenoFlow REST API is running.");
	cJSON_AddStringToObject(root, "status", "ok");
	cJSON_AddNumberToObject(root, "version", 1.0);

	cJSON *backends = cJSON_CreateArray();
	XenoFlowConfig* config = http_server_ctx->config;
	
	for (int i = 0; i < config->numBackends; i++) {
		XenoFlowBackend *backend = config->backends[i];
		cJSON *backend_info = cJSON_CreateObject();
		cJSON_AddStringToObject(backend_info, "name", backend->name);
		
		/* Format MAC address as string */
		char mac_str[18];
		snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
				backend->mac_address[0], backend->mac_address[1], 
				backend->mac_address[2], backend->mac_address[3],
				backend->mac_address[4], backend->mac_address[5]);
		cJSON_AddStringToObject(backend_info, "mac_address", mac_str);

		char entry_pps[128];
		snprintf(entry_pps, 128, "%d", entry_processed_packages(i, config));
		cJSON_AddStringToObject(backend_info, "packetsProcessed", entry_pps);
		
		cJSON_AddItemToArray(backends, backend_info);
	}
	cJSON_AddItemToObject(root, "backends", backends);

	cJSON_AddNumberToObject(root, "backendNumber", config->numBackends);
	char *json_str = cJSON_Print(root);
	cJSON_Delete(root);

	return json_str;
}

/**
 * @brief Start the HTTP server on the specified port
 * @param port Port number to listen on
 * @param config Pointer to XenoFlowConfig
 * @return 0 on success, -1 on failure
 */
int http_server_start(int port, XenoFlowConfig *config)
{
	http_server_ctx = malloc(sizeof(struct http_server_ctx));
	if (!http_server_ctx) {
		DOCA_LOG_ERR("Failed to allocate HTTP server context");
		return -1;
	}

	http_server_ctx->port = port;
	http_server_ctx->config = config;
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

int entry_processed_packages(int entryId, XenoFlowConfig *config) {
	struct doca_flow_resource_query query_stats;
	doca_error_t query_result;
	
	query_result = doca_flow_resource_query_entry(config->backends[entryId]->entry, &query_stats);
	
	uint64_t packets = 0;
	if (query_result == DOCA_SUCCESS) {
		packets = query_stats.counter.total_pkts;
	}
	
	return packets;
}