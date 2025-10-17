#include <string.h>
#include <unistd.h>
#include <sys/time.h> 

#include <rte_byteorder.h>

#include <doca_log.h>
#include <doca_flow.h>

#include "flow_common.h"
#include <cjson/cJSON.h>

//#define CLEAR

DOCA_LOG_REGISTER(FLOW_SHARED_COUNTER);

/* Set match l4 port */
#define SET_L4_PORT(layer, port, value) \
	do { \
		if (match.layer.l4_type_ext == DOCA_FLOW_L4_TYPE_EXT_TCP) \
			match.layer.tcp.l4_port.port = (value); \
		else if (match.layer.l4_type_ext == DOCA_FLOW_L4_TYPE_EXT_UDP) \
			match.layer.udp.l4_port.port = (value); \
	} while (0)

void doca_try(doca_error_t result, char* message, int nb_ports, struct doca_flow_port** ports) {
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("%s: %s", message, doca_error_get_descr(result));
		stop_doca_flow_ports(nb_ports, ports);
		doca_flow_destroy();
		exit(-1);
	}
}

static doca_error_t create_egress_pipe(struct doca_flow_port *port,
	int port_id,
	enum doca_flow_l4_type_ext out_l4_type,
	struct doca_flow_pipe **pipe)
{
	struct doca_flow_match match;
	struct doca_flow_match match_mask;
	struct doca_flow_monitor monitor;
	struct doca_flow_actions actions0, actions1, actions2, *actions_arr[2];
	struct doca_flow_fwd fwd, fwd_miss;
	struct doca_flow_pipe_cfg *pipe_cfg;
	doca_error_t result;

	memset(&match, 0, sizeof(match));
	memset(&match_mask, 0, sizeof(match_mask));
	memset(&monitor, 0, sizeof(monitor));
	memset(&actions0, 0, sizeof(actions0));
	memset(&actions1, 0, sizeof(actions1));
	memset(&actions2, 0, sizeof(actions2));
	memset(&fwd, 0, sizeof(fwd));
	memset(&fwd_miss, 0, sizeof(fwd_miss));

	/* 5 tuple match */
  	match.outer.l3_type = DOCA_FLOW_L3_TYPE_IP4;
	match.outer.ip4.src_ip = BE_IPV4_ADDR(255, 255, 255, 255);

	result = doca_flow_pipe_cfg_create(&pipe_cfg, port);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		return result;
	}

	result = set_flow_pipe_cfg(pipe_cfg, "SHARED_COUNTER_PIPE", DOCA_FLOW_PIPE_BASIC, true);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_match(pipe_cfg, &match, &match_mask);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg match: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_actions(pipe_cfg, actions_arr, NULL, NULL, 1);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg actions: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_monitor(pipe_cfg, &monitor);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg monitor: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	fwd.type = DOCA_FLOW_FWD_PORT;
	fwd.port_id = 1;
	fwd_miss.type = DOCA_FLOW_FWD_DROP;

	result = doca_flow_pipe_create(pipe_cfg, &fwd, &fwd_miss, pipe);

destroy_pipe_cfg:
	doca_flow_pipe_cfg_destroy(pipe_cfg);
	return result;
}

static doca_error_t create_root_pipe(struct doca_flow_port *port,
					       int port_id,
					       enum doca_flow_l4_type_ext out_l4_type,
					       struct doca_flow_pipe **pipe)
{
	struct doca_flow_match match;
	struct doca_flow_match match_mask;
	struct doca_flow_monitor monitor;
	struct doca_flow_actions actions0, actions1, actions2, *actions_arr[2];
	struct doca_flow_fwd fwd, fwd_miss;
	struct doca_flow_pipe_cfg *pipe_cfg;
	doca_error_t result;

	memset(&match, 0, sizeof(match));
	memset(&match_mask, 0, sizeof(match_mask));
	memset(&monitor, 0, sizeof(monitor));
	memset(&actions0, 0, sizeof(actions0));
	memset(&actions1, 0, sizeof(actions1));
	memset(&actions2, 0, sizeof(actions2));
	memset(&fwd, 0, sizeof(fwd));
	memset(&fwd_miss, 0, sizeof(fwd_miss));

  	match.outer.l3_type = DOCA_FLOW_L3_TYPE_IP4;
	match.outer.ip4.src_ip = BE_IPV4_ADDR(255, 255, 255, 255);
	match_mask.outer.ip4.src_ip = BE_IPV4_ADDR(0, 0, 0, 1);
	DOCA_LOG_INFO("%d", match_mask.outer.ip4.src_ip);

	SET_MAC_ADDR(actions0.outer.eth.dst_mac, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
	SET_MAC_ADDR(actions0.outer.eth.src_mac, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

	actions_arr[0] = &actions0;

	monitor.counter_type = DOCA_FLOW_RESOURCE_TYPE_SHARED;
	monitor.shared_counter.shared_counter_id = 0xffffffff;

	result = doca_flow_pipe_cfg_create(&pipe_cfg, port);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		return result;
	}

	result = set_flow_pipe_cfg(pipe_cfg, "SHARED_COUNTER_PIPE", DOCA_FLOW_PIPE_BASIC, true);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_match(pipe_cfg, &match, &match_mask);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg match: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_actions(pipe_cfg, actions_arr, NULL, NULL, 1);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg actions: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}
	result = doca_flow_pipe_cfg_set_monitor(pipe_cfg, &monitor);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg monitor: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	fwd.type = DOCA_FLOW_FWD_PORT;
	fwd.port_id = 1;
	fwd_miss.type = DOCA_FLOW_FWD_DROP;

	result = doca_flow_pipe_create(pipe_cfg, &fwd, &fwd_miss, pipe);

destroy_pipe_cfg:
	doca_flow_pipe_cfg_destroy(pipe_cfg);
	return result;
}

static doca_error_t add_shared_counter_pipe_entry(struct doca_flow_pipe *pipe,
						  enum doca_flow_l4_type_ext out_l4_type,
						  uint32_t shared_counter_id,
						  struct entries_status *status)
{
	struct doca_flow_match match;
	struct doca_flow_actions actions;
	struct doca_flow_monitor monitor;
	struct doca_flow_pipe_entry *entry_mac;
	struct doca_flow_fwd fwd;

	doca_error_t result;

	memset(&match, 0, sizeof(match));
	memset(&actions, 0, sizeof(actions));
	memset(&monitor, 0, sizeof(monitor));
	memset(&fwd, 0, sizeof(fwd));

	monitor.shared_counter.shared_counter_id = shared_counter_id;

	match.outer.ip4.src_ip = BE_IPV4_ADDR(0, 0, 0, 1);	

	actions.action_idx = 0;

	SET_MAC_ADDR(actions.outer.eth.dst_mac, 0xe8, 0xeb, 0xd3, 0x9c, 0x71, 0xac);
	SET_MAC_ADDR(actions.outer.eth.src_mac, 0xc4, 0x70, 0xbd, 0xa0, 0x56, 0xbd);

	result = doca_flow_pipe_add_entry(0, pipe, &match, &actions, &monitor, NULL, 0, status, &entry_mac);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to add entry: %s", doca_error_get_descr(result));
		return result;
	}

	memset(&match, 0, sizeof(match));
	memset(&actions, 0, sizeof(actions));
	memset(&monitor, 0, sizeof(monitor));
	memset(&fwd, 0, sizeof(fwd));

	match.outer.ip4.src_ip = BE_IPV4_ADDR(0, 0, 0, 0);

	actions.action_idx = 0;

	SET_MAC_ADDR(actions.outer.eth.dst_mac, 0xa0, 0x88, 0xc2, 0xb5, 0xf4, 0x5a);	
	SET_MAC_ADDR(actions.outer.eth.src_mac, 0xc4, 0x70, 0xbd, 0xa0, 0x56, 0xbd);

	result = doca_flow_pipe_add_entry(0, pipe, &match, &actions, &monitor, DOCA_FLOW_NO_WAIT, 0, status, &entry_mac);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to add entry 2: %s", doca_error_get_descr(result));
		return result;
	}
	return DOCA_SUCCESS;
}

typedef struct xfb
{
	char* name;
	char* mac;

} XenoFlowBackend;

typedef struct xfc {
	int numBackends;
	XenoFlowBackend** backends;
} XenoFlowConfig;

XenoFlowBackend* createBackend(char* name, char* mac) {
	XenoFlowBackend* out = malloc(sizeof(XenoFlowBackend));
	
	out->name = name;
	out->mac = mac;
	
	return out; 
}

XenoFlowConfig* createConfig() {
	XenoFlowConfig* config = malloc(sizeof(XenoFlowConfig));
	
	config->numBackends = 0;
	config->backends = malloc(sizeof(XenoFlowBackend) * 128);
	
	return config;
}

void configAddBackend(XenoFlowConfig* config, XenoFlowBackend* backend) {
	config->backends[config->numBackends] = backend;
	config->numBackends += 1;
}


XenoFlowConfig* load_config() {
	FILE *file = fopen("backends.json", "rb");
    if (!file) {
        perror("Fehler beim Öffnen der Datei");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(length + 1);
    if (!content) {
        perror("Speicherzuweisung fehlgeschlagen");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

	cJSON *json = cJSON_Parse(content);
    if (!json) {
        printf("Fehler beim Parsen: %s\n", cJSON_GetErrorPtr());
        return 0;
    }
	printf("%s\n", cJSON_Print(json));

    cJSON *server = cJSON_GetObjectItem(json, "backends");
	if(cJSON_GetArraySize(server) == 0) {
		DOCA_LOG_ERR("No backends found in backends.json!");
		exit(-1);
	}
	cJSON* name = NULL;
  	cJSON* backend_mac = NULL;
	XenoFlowConfig* c = createConfig();
	for (int i = 0 ; i < cJSON_GetArraySize(server) ; i++) {
     	cJSON * subitem = cJSON_GetArrayItem(server, i);
     	name = cJSON_GetObjectItem(subitem, "name");
     	backend_mac = cJSON_GetObjectItem(subitem, "mac_address");
		DOCA_LOG_INFO("%s -> %s", name->valuestring, backend_mac->valuestring);
		XenoFlowBackend* b = createBackend(name->valuestring, backend_mac->valuestring);
		configAddBackend(c, b);
  	}
	usleep(3000000);
	return c;
}

doca_error_t xeno_flow(int nb_queues)
{
	int nb_ports = 1;
	struct flow_resources resource = {1};
	uint32_t nr_shared_resources[SHARED_RESOURCE_NUM_VALUES] = {0};
	struct doca_flow_port *ports[2];
	struct doca_dev *dev_arr[nb_ports];
	struct doca_flow_pipe *udp_pipe;
	int port_id = 0;
	uint32_t shared_counter_ids[] = {0, 1};
	struct doca_flow_resource_query query_results_array[nb_ports];
	struct doca_flow_shared_resource_cfg cfg = {.domain = DOCA_FLOW_PIPE_DOMAIN_DEFAULT};
	struct entries_status status;
	int num_of_entries = 4;
  	doca_error_t result;

	nr_shared_resources[DOCA_FLOW_SHARED_RESOURCE_COUNTER] = 2;

	XenoFlowConfig *config = load_config();
	DOCA_LOG_INFO("Number of backends: %d", config->numBackends);

	doca_try(init_doca_flow(nb_queues, "vnf,hws", &resource, nr_shared_resources), "Failed to init DOCA Flow", nb_ports, ports);

	memset(dev_arr, 0, sizeof(struct doca_dev *) * 1);
	
	doca_try(init_doca_flow_ports(2, ports, true, dev_arr), "Failed to init DOCA ports", nb_ports, ports);

	memset(&status, 0, sizeof(status));

	doca_try(doca_flow_shared_resource_set_cfg(DOCA_FLOW_SHARED_RESOURCE_COUNTER, 0, &cfg), "Failed to configure shared counter to port", nb_ports, ports);

	doca_try(doca_flow_shared_resources_bind(DOCA_FLOW_SHARED_RESOURCE_COUNTER, &shared_counter_ids[0], 1, ports[0]), "Failed to bind shared counter to pipe", nb_ports, ports);

	doca_try(create_root_pipe(ports[0], 0, DOCA_FLOW_L4_TYPE_EXT_UDP, &udp_pipe), "Failed to create pipe", nb_ports, ports);
	
	doca_try(add_shared_counter_pipe_entry(udp_pipe, DOCA_FLOW_L4_TYPE_EXT_UDP, shared_counter_ids[0], &status), "Failed to add entry", nb_ports, ports);

	doca_try(doca_flow_entries_process(ports[0], 0, DEFAULT_TIMEOUT_US, num_of_entries), "Failed to process entries", nb_ports, ports);

	DOCA_LOG_INFO("Starting the load balancer loop");

	int numPacketsOld = 0;
	int numPacketsNew = 0;
	int numBytesOld = 0;
	int numBytesNew = 0;
	struct timeval t1, t2;
    double elapsedTime;
	int statRefreshIntervall = 500000;
	gettimeofday(&t1, NULL);
	gettimeofday(&t2, NULL);
  	while(1) {
		result = doca_flow_shared_resources_query(DOCA_FLOW_SHARED_RESOURCE_COUNTER,
							shared_counter_ids,
							query_results_array,
							nb_ports);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to query entry: %s", doca_error_get_descr(result));
			stop_doca_flow_ports(nb_ports, ports);
			doca_flow_destroy();
			return result;
		}


		gettimeofday(&t2, NULL);
		numPacketsNew = query_results_array[port_id].counter.total_pkts;
		numBytesNew = query_results_array[port_id].counter.total_bytes;

		int diff = numPacketsNew - numPacketsOld;
		int diffBytes = numBytesNew - numBytesOld;;
		elapsedTime = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) / 1000000.0;

		t1 = t2;
		numPacketsOld = numPacketsNew;
		numBytesOld = numBytesNew;

		DOCA_LOG_INFO("Loadbalancer stats:");
		DOCA_LOG_INFO("\tpacket_count: \t%ld", query_results_array[port_id].counter.total_pkts);
		DOCA_LOG_INFO("\toverall_bytes: \t%ld", query_results_array[port_id].counter.total_bytes);
		DOCA_LOG_INFO("\tpackets/s: \t%.0f", ((double)diff/elapsedTime));
		DOCA_LOG_INFO("\tBytes/s: \t%.3f", ((double)(diffBytes)/elapsedTime));
		DOCA_LOG_INFO("============================================");
		usleep(statRefreshIntervall);
		#ifdef CLEAR
		system("clear");
		#endif
	}

	result = stop_doca_flow_ports(nb_ports, ports);
	doca_flow_destroy();
	return result;
}
