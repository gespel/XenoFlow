#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include <rte_byteorder.h>

#include <doca_log.h>
#include <doca_flow.h>

#include "flow_common.h"
#include "http_server.h"
#include "core.h"

DOCA_LOG_REGISTER(FLOW_HASH_PIPE);
#define NB_ACTION_DESC (1)

static uint32_t next_power_of_two(uint32_t value) {
	if (value <= 1) {
		return 1;
	}

	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	return value + 1;
}


void doca_try(doca_error_t result, char* message, int nb_ports, struct doca_flow_port** ports) {
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("%s: %s", message, doca_error_get_descr(result));
		stop_doca_flow_ports(nb_ports, ports);
		doca_flow_destroy();
		exit(-1);
	}
}

XenoFlowBackend* createBackend(const char* name, const char* mac_str) {
	XenoFlowBackend* b = malloc(sizeof(XenoFlowBackend));
	strcpy(b->name, name);
	sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
		   &b->mac_address[0], &b->mac_address[1], &b->mac_address[2],
		   &b->mac_address[3], &b->mac_address[4], &b->mac_address[5]);
	return b;
}

XenoFlowConfig* createConfig() {
	XenoFlowConfig* config = malloc(sizeof(XenoFlowConfig));
	config->numBackends = 0;
	config->nextBackend = 0;
	config->backends = calloc(MAX_BACKENDS, sizeof(XenoFlowBackend *));
	return config;
}

void configAddBackend(XenoFlowConfig* config, XenoFlowBackend* backend) {
	if (config->numBackends >= MAX_BACKENDS) {
		DOCA_LOG_ERR("Cannot add backend: maximum backends (%d) reached", MAX_BACKENDS);
		return;
	}
	config->backends[config->numBackends] = backend;
	config->numBackends += 1;
}

XenoFlowConfig* load_config() {
	XenoFlowConfig* c = createConfig();
	
	/* NOTE: Hash pipe requires power-of-2 number of entries (1, 2, 4, 8, 16, ...) */
	XenoFlowBackend* b1 = createBackend("fips2", "a0:88:c2:b5:f4:5a");
	XenoFlowBackend* b2 = createBackend("fips1", "e8:eb:d3:9c:71:ac");
	
	configAddBackend(c, b1);
	configAddBackend(c, b2);
	
	DOCA_LOG_INFO("Loaded %d backends", c->numBackends);
	for (int i = 0; i < c->numBackends; i++) {
		DOCA_LOG_INFO("  %s -> %02x:%02x:%02x:%02x:%02x:%02x", 
			c->backends[i]->name,
			c->backends[i]->mac_address[0], c->backends[i]->mac_address[1],
			c->backends[i]->mac_address[2], c->backends[i]->mac_address[3],
			c->backends[i]->mac_address[4], c->backends[i]->mac_address[5]);
	}
	
	return c;
}

static doca_error_t create_hash_pipe(struct doca_flow_port *port,
				       int port_id,
				       int num_backends,
				       struct doca_flow_pipe **pipe)
{
	struct doca_flow_match match_mask;
	struct doca_flow_monitor monitor;
	struct doca_flow_actions actions, *actions_arr[1];
	struct doca_flow_action_descs descs;
	struct doca_flow_action_descs *descs_arr[1];
	struct doca_flow_action_desc desc_array[NB_ACTION_DESC] = {0};
	struct doca_flow_fwd fwd;
	struct doca_flow_pipe_cfg *pipe_cfg;
	doca_error_t result;

	memset(&match_mask, 0, sizeof(match_mask));
	memset(&monitor, 0, sizeof(monitor));
	memset(&actions, 0, sizeof(actions));
	memset(&descs, 0, sizeof(descs));
	memset(&fwd, 0, sizeof(fwd));

	actions_arr[0] = &actions;
	descs.nb_action_desc = NB_ACTION_DESC;
	descs.desc_array = desc_array;
	descs_arr[0] = &descs;

	match_mask.outer.l3_type = DOCA_FLOW_L3_TYPE_IP4;
	match_mask.outer.ip4.src_ip = 0xffffffff;

	SET_MAC_ADDR(actions.outer.eth.dst_mac, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);


	monitor.counter_type = DOCA_FLOW_RESOURCE_TYPE_NON_SHARED;

	result = doca_flow_pipe_cfg_create(&pipe_cfg, port);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to create doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		return result;
	}

	result = set_flow_pipe_cfg(pipe_cfg, "HASH_PIPE", DOCA_FLOW_PIPE_HASH, true);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	result = doca_flow_pipe_cfg_set_nr_entries(pipe_cfg, num_backends);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg nr_entries: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	result = doca_flow_pipe_cfg_set_match(pipe_cfg, NULL, &match_mask);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg match: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	result = doca_flow_pipe_cfg_set_monitor(pipe_cfg, &monitor);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg monitor: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	result = doca_flow_pipe_cfg_set_actions(pipe_cfg, actions_arr, NULL, descs_arr, 1);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to set doca_flow_pipe_cfg actions: %s", doca_error_get_descr(result));
		goto destroy_pipe_cfg;
	}

	fwd.type = DOCA_FLOW_FWD_PORT;
	fwd.port_id = 0xffff;

	result = doca_flow_pipe_create(pipe_cfg, &fwd, NULL, pipe);

destroy_pipe_cfg:
	doca_flow_pipe_cfg_destroy(pipe_cfg);
	return result;
}

struct doca_dev *open_doca_dev_by_pci(const char *pci_bdf)
{
    struct doca_devinfo **list;
    uint32_t nb;
    doca_error_t err;

    err = doca_devinfo_create_list(&list, &nb);
    if (err != DOCA_SUCCESS) {
        printf("devinfo_create_list failed\n");
        return NULL;
    }

    struct doca_dev *dev = NULL;

    for (uint32_t i = 0; i < nb; i++) {
        char pci[DOCA_DEVINFO_PCI_ADDR_SIZE] = {0};
		uint8_t ipv4[4];

        if (doca_devinfo_get_pci_addr_str(list[i], pci) != DOCA_SUCCESS)
            continue;

        if (strcmp(pci, pci_bdf) == 0) {
			doca_devinfo_get_ipv4_addr(list[i], ipv4, DOCA_DEVINFO_IPV4_ADDR_SIZE);
			DOCA_LOG_INFO("IPv4: %u.%u.%u.%u", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
			err = doca_dev_open(list[i], &dev);
            break;
        }
    }

    doca_devinfo_destroy_list(list);
    return dev;
}



doca_error_t xeno_flow(int nb_queues)
{
	struct doca_flow_pipe *hash_pipe;
	struct doca_flow_pipe_entry *hash_entries[1024];
	int nb_ports = 2;
	struct flow_resources resource = {0};
	uint32_t nr_shared_resources[SHARED_RESOURCE_NUM_VALUES] = {0};
	struct doca_flow_port *ports[2];
	struct doca_dev *dev_arr[nb_ports];
	doca_error_t result;
	uint32_t action_mem[2] = {0};

	XenoFlow *xeno = calloc(1, sizeof(XenoFlow));
	XenoFlowConfig *config = load_config();
	uint32_t hash_pipe_entries = next_power_of_two(config->numBackends);
	DOCA_LOG_INFO("Number of backends: %d", config->numBackends);


	/* Start HTTP Server with config */
	if (http_server_start(8080, config) != 0) {
		DOCA_LOG_ERR("Failed to start HTTP server");
		return DOCA_ERROR_INITIALIZATION;
	}

	resource.mode = DOCA_FLOW_RESOURCE_MODE_PORT;
	resource.nr_counters = hash_pipe_entries;

	doca_try(init_doca_flow(nb_queues, "vnf,hws", &resource, nr_shared_resources), "Failed to init DOCA Flow", nb_ports, ports);

	struct doca_dev *dev1 = open_doca_dev_by_pci("0000:03:00.0");
	struct doca_dev *dev2 = open_doca_dev_by_pci("0000:03:00.1");

	if (!dev1 || !dev2) {
		DOCA_LOG_INFO("Device not found");
		return DOCA_ERROR_NOT_FOUND;
	} else {
		DOCA_LOG_INFO("Devices found!");
	}

	dev_arr[0] = dev1;
	dev_arr[1] = dev2;

	ARRAY_INIT(action_mem, ACTIONS_MEM_SIZE(2));

	doca_try(init_doca_flow_ports(2, ports, true, dev_arr, action_mem, &resource), "Failed to init DOCA ports", nb_ports, ports);

	doca_try(create_hash_pipe(ports[0], 0, hash_pipe_entries, &hash_pipe), "Failed to create hash pipe", nb_ports, ports);
	DOCA_LOG_INFO("Starting the load balancer with hash pipe");

	xeno->config = config;
	xeno->ports[0] = ports[0];
	xeno->ports[1] = ports[1];
	xeno->hash_pipe = hash_pipe;
	xeno->hash_pipe_entries = hash_pipe_entries;
	for (int i = 0; i < config->numBackends; i++) {
		xeno->hash_entries[i] = hash_entries[i];
	}

	int initial_backends = config->numBackends;
	DOCA_LOG_INFO("Adding %d backends via helper", initial_backends);
	config->numBackends = 0;
	for (int i = 0; i < initial_backends; i++) {
		char backend_name[64];
		char backend_mac[18];

		snprintf(backend_name, sizeof(backend_name), "%s", config->backends[i]->name);
		snprintf(backend_mac, sizeof(backend_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
				config->backends[i]->mac_address[0], config->backends[i]->mac_address[1],
				config->backends[i]->mac_address[2], config->backends[i]->mac_address[3],
				config->backends[i]->mac_address[4], config->backends[i]->mac_address[5]);

		free(config->backends[i]);
		config->backends[i] = NULL;

		if (i == 0) {
			DOCA_LOG_INFO("Replacing %s with a host-target entry", backend_name);
			doca_try(xenoflow_add_host_entry(xeno, 0, "to-host", backend_mac, "1.2.3.4"),
				 "Failed to add host entry", nb_ports, ports);
		} else {
			doca_try(xenoflow_add_backend(xeno, backend_name, backend_mac),
				 "Failed to add backend", nb_ports, ports);
		}
	}

	for (int i = 0; i < config->numBackends; i++) {
		hash_entries[i] = xeno->hash_entries[i];
	}

	DOCA_LOG_INFO("XenoFlow Load Balancer initialized with %d backends", config->numBackends);
	
	int statRefreshIntervall = 5000000;
	uint64_t last_packets[config->numBackends];
	memset(last_packets, 0, sizeof(last_packets));
	
  	while(1) {
		DOCA_LOG_INFO("XenoFlow Load Balancer Status - %d backends", config->numBackends);
		
		for (int i = 0; i < config->numBackends; i++) {
			struct doca_flow_resource_query query_stats;
			doca_error_t query_result;
			
			query_result = doca_flow_resource_query_entry(config->backends[i]->entry, &query_stats);
			
			uint64_t packets = 0;
			if (query_result == DOCA_SUCCESS) {
				packets = query_stats.counter.total_pkts;
			}
			
			DOCA_LOG_INFO("  Entry %d - %s: %lu packets (%lu new)",
				i, config->backends[i]->name, packets, 
				(packets > last_packets[i]) ? (packets - last_packets[i]) : 0);
			
			last_packets[i] = packets;
		}
		DOCA_LOG_INFO("============================================");
		usleep(statRefreshIntervall);
	}

	result = stop_doca_flow_ports(nb_ports, ports);
	doca_flow_destroy();
	return result;
}

doca_error_t xenoflow_add_backend(XenoFlow *xeno, char *name, char *mac) {
	if (xeno == NULL || xeno->config == NULL || xeno->hash_pipe == NULL || xeno->ports[0] == NULL) {
		DOCA_LOG_ERR("Cannot add backend: XenoFlow is not initialized");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (xeno->hash_pipe_entries == 0) {
		DOCA_LOG_ERR("Cannot add backend: hash pipe entries not initialized");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (xeno->config->numBackends >= MAX_BACKENDS) {
		DOCA_LOG_ERR("Cannot add backend: maximum backends (%d) reached", MAX_BACKENDS);
		return DOCA_ERROR_NO_MEMORY;
	}

	int entry_index = xeno->config->numBackends;
	if ((uint32_t)entry_index >= xeno->hash_pipe_entries) {
		DOCA_LOG_ERR("Cannot add backend: entry index %d exceeds hash pipe size %u", entry_index, xeno->hash_pipe_entries);
		return DOCA_ERROR_BAD_STATE;
	}
	XenoFlowBackend *new_backend = createBackend(name, mac);
	doca_error_t result;
	struct entries_status status = {0};
	struct doca_flow_fwd fwd;
	struct doca_flow_actions actions;
	struct doca_flow_pipe_entry *entry = NULL;

	memset(&fwd, 0, sizeof(fwd));
	memset(&actions, 0, sizeof(actions));

	SET_MAC_ADDR(actions.outer.eth.dst_mac,
		     new_backend->mac_address[0], new_backend->mac_address[1], new_backend->mac_address[2],
		     new_backend->mac_address[3], new_backend->mac_address[4], new_backend->mac_address[5]);

	fwd.type = DOCA_FLOW_FWD_PORT;
	fwd.port_id = 1;

	result = doca_flow_pipe_hash_add_entry(0,
							xeno->hash_pipe,
							entry_index,
							0,
							&actions,
							NULL,
							&fwd,
							0,
							&status,
							&entry);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to add hash entry %d: %s", entry_index, doca_error_get_descr(result));
		free(new_backend);
		return result;
	}

	result = doca_flow_entries_process(xeno->ports[0], 0, DEFAULT_TIMEOUT_US, 1);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to process hash entry %d: %s", entry_index, doca_error_get_descr(result));
		free(new_backend);
		return result;
	}

	if (status.failure || status.nb_processed != 1) {
		DOCA_LOG_ERR("Hash entry %d was not fully processed (processed=%d failure=%d)",
			     entry_index, status.nb_processed, status.failure);
		free(new_backend);
		return DOCA_ERROR_BAD_STATE;
	}

	new_backend->entry = entry;
	xeno->hash_entries[entry_index] = entry;
	configAddBackend(xeno->config, new_backend);
	DOCA_LOG_INFO("Added backend %s at hash entry %d", new_backend->name, entry_index);
	return DOCA_SUCCESS;
}

doca_error_t xenoflow_add_host_entry(XenoFlow *xeno, uint32_t entry_index, char *name, char *mac, char *src_ip) {
	if (xeno == NULL || xeno->config == NULL || xeno->hash_pipe == NULL || xeno->ports[0] == NULL) {
		DOCA_LOG_ERR("Cannot add host entry: XenoFlow is not initialized");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (xeno->hash_pipe_entries == 0) {
		DOCA_LOG_ERR("Cannot add host entry: hash pipe entries not initialized");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (src_ip == NULL || mac == NULL || name == NULL) {
		DOCA_LOG_ERR("Cannot add host entry: missing name, mac or src_ip");
		return DOCA_ERROR_INVALID_VALUE;
	}

	if (entry_index >= xeno->hash_pipe_entries) {
		DOCA_LOG_ERR("Cannot add host entry: entry index %u exceeds hash pipe size %u", entry_index, xeno->hash_pipe_entries);
		return DOCA_ERROR_BAD_STATE;
	}

	if (xeno->config->numBackends >= MAX_BACKENDS) {
		DOCA_LOG_ERR("Cannot add host entry: maximum backends (%d) reached", MAX_BACKENDS);
		return DOCA_ERROR_NO_MEMORY;
	}

	struct doca_flow_fwd fwd;
	struct doca_flow_actions actions;
	struct doca_flow_pipe_entry *entry = NULL;
	struct entries_status status = {0};
	struct doca_flow_target *kernel_target = NULL;
	doca_error_t result;
	XenoFlowBackend *new_backend = createBackend(name, mac);

	memset(&fwd, 0, sizeof(fwd));
	memset(&actions, 0, sizeof(actions));

	result = doca_flow_get_target(DOCA_FLOW_TARGET_KERNEL, &kernel_target);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to get kernel target for host forwarding: %s", doca_error_get_descr(result));
		free(new_backend);
		return result;
	}

	if (xeno->hash_entries[entry_index] != NULL) {
		DOCA_LOG_ERR("Hash entry %u already exists", entry_index);
		free(new_backend);
		return DOCA_ERROR_INVALID_VALUE;
	}

	SET_MAC_ADDR(actions.outer.eth.dst_mac,
		     new_backend->mac_address[0], new_backend->mac_address[1], new_backend->mac_address[2],
		     new_backend->mac_address[3], new_backend->mac_address[4], new_backend->mac_address[5]);

	fwd.type = DOCA_FLOW_FWD_TARGET;
	fwd.target = kernel_target;

	result = doca_flow_pipe_hash_add_entry(0,
							xeno->hash_pipe,
							entry_index,
							0,
							&actions,
							NULL,
							&fwd,
							0,
							&status,
							&entry);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to add host entry %u for %s: %s", entry_index, src_ip, doca_error_get_descr(result));
		free(new_backend);
		return result;
	}

	result = doca_flow_entries_process(xeno->ports[0], 0, DEFAULT_TIMEOUT_US, 1);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to process host entry %u: %s", entry_index, doca_error_get_descr(result));
		free(new_backend);
		return result;
	}

	if (status.failure || status.nb_processed != 1) {
		DOCA_LOG_ERR("Host entry %u was not fully processed (processed=%d failure=%d)",
			     entry_index, status.nb_processed, status.failure);
		free(new_backend);
		return DOCA_ERROR_BAD_STATE;
	}

	new_backend->entry = entry;
	xeno->hash_entries[entry_index] = entry;
	configAddBackend(xeno->config, new_backend);
	DOCA_LOG_INFO("Added host entry %u for %s -> kernel target", entry_index, src_ip);
	return DOCA_SUCCESS;
}
