#include <string.h>
#include <unistd.h>
#include <sys/time.h> 

#include <rte_byteorder.h>

#include <doca_log.h>
#include <doca_flow.h>

#include "flow_common.h"

DOCA_LOG_REGISTER(FLOW_HASH_PIPE);

typedef struct {
	char name[64];
	uint8_t mac_address[6];
} XenoFlowBackend;

typedef struct {
	XenoFlowBackend *backends;
	int numBackends;
	int nextBackend;
} XenoFlowConfig;

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
	config->backends = malloc(sizeof(XenoFlowBackend) * 128);
	return config;
}

void configAddBackend(XenoFlowConfig* config, XenoFlowBackend* backend) {
	config->backends[config->numBackends] = *backend;
	config->numBackends += 1;
}

XenoFlowConfig* load_config() {
	XenoFlowConfig* c = createConfig();
	
	/* NOTE: Hash pipe requires power-of-2 number of entries (1, 2, 4, 8, 16, ...) */
	XenoFlowBackend* b1 = createBackend("backend1", "aa:bb:cc:dd:ee:00");
	XenoFlowBackend* b2 = createBackend("backend2", "aa:bb:cc:dd:ee:01");
	XenoFlowBackend* b3 = createBackend("backend3", "aa:bb:cc:dd:ee:02");
	XenoFlowBackend* b4 = createBackend("backend4", "aa:bb:cc:dd:ee:03");
	
	configAddBackend(c, b1);
	configAddBackend(c, b2);
	configAddBackend(c, b3);
	configAddBackend(c, b4);
	
	DOCA_LOG_INFO("Loaded %d backends", c->numBackends);
	for (int i = 0; i < c->numBackends; i++) {
		DOCA_LOG_INFO("  %s -> %02x:%02x:%02x:%02x:%02x:%02x", 
			c->backends[i].name,
			c->backends[i].mac_address[0], c->backends[i].mac_address[1],
			c->backends[i].mac_address[2], c->backends[i].mac_address[3],
			c->backends[i].mac_address[4], c->backends[i].mac_address[5]);
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
	struct doca_flow_fwd fwd;
	struct doca_flow_pipe_cfg *pipe_cfg;
	doca_error_t result;

	memset(&match_mask, 0, sizeof(match_mask));
	memset(&monitor, 0, sizeof(monitor));
	memset(&fwd, 0, sizeof(fwd));

	match_mask.outer.l3_type = DOCA_FLOW_L3_TYPE_IP4;
	match_mask.outer.ip4.src_ip = 0xffffffff;

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

doca_error_t xeno_flow_hash_pipe(int nb_queues)
{
	int nb_ports = 1;
	struct flow_resources resource = {0};
	uint32_t nr_shared_resources[SHARED_RESOURCE_NUM_VALUES] = {0};
	struct doca_flow_port *ports[2];
	struct doca_dev *dev_arr[nb_ports];
	struct doca_flow_pipe *hash_pipe;
	struct entries_status status;
	doca_error_t result;
	uint32_t action_mem[2] = {0};

	XenoFlowConfig *config = load_config();
	DOCA_LOG_INFO("Number of backends: %d", config->numBackends);

	resource.mode = DOCA_FLOW_RESOURCE_MODE_PORT;
	resource.nr_counters = config->numBackends;

	doca_try(init_doca_flow(nb_queues, "vnf,hws", &resource, nr_shared_resources), "Failed to init DOCA Flow", nb_ports, ports);

	struct doca_dev *dev1 = open_doca_dev_by_pci("0000:03:00.0");
	struct doca_dev *dev2 = open_doca_dev_by_pci("0000:03:00.1");

	if (!dev1 || !dev2) {
		DOCA_LOG_INFO("Device not found");
		return DOCA_ERROR_NOT_FOUND;
	}
	else {
		DOCA_LOG_INFO("Devices found!");
	}
	dev_arr[0] = dev1;
	dev_arr[1] = dev2;

	doca_try(init_doca_flow_ports(2, ports, true, dev_arr, action_mem, &resource), "Failed to init DOCA ports", nb_ports, ports);

	memset(&status, 0, sizeof(status));

	doca_try(create_hash_pipe(ports[0], 0, config->numBackends, &hash_pipe), "Failed to create hash pipe", nb_ports, ports);

	DOCA_LOG_INFO("Starting the load balancer with hash pipe");

	struct doca_flow_pipe_entry *hash_entries[config->numBackends];
	for (int i = 0; i < config->numBackends; i++) {
		struct doca_flow_fwd fwd;
		struct doca_flow_actions actions;
		enum doca_flow_flags_type flags = DOCA_FLOW_WAIT_FOR_BATCH;

		memset(&fwd, 0, sizeof(fwd));
		memset(&actions, 0, sizeof(actions));

		actions.outer.eth.dst_mac[0] = config->backends[i].mac_address[0];
		actions.outer.eth.dst_mac[1] = config->backends[i].mac_address[1];
		actions.outer.eth.dst_mac[2] = config->backends[i].mac_address[2];
		actions.outer.eth.dst_mac[3] = config->backends[i].mac_address[3];
		actions.outer.eth.dst_mac[4] = config->backends[i].mac_address[4];
		actions.outer.eth.dst_mac[5] = config->backends[i].mac_address[5];

		fwd.type = DOCA_FLOW_FWD_PORT;
		fwd.port_id = 1;

		if (i == config->numBackends - 1)
			flags = DOCA_FLOW_NO_WAIT;

		result = doca_flow_pipe_hash_add_entry(0,
						       hash_pipe,
						       i,
						       0,
						       NULL,
						       &actions,
						       &fwd,
						       flags,
						       &status,
						       &hash_entries[i]);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to add hash entry %d: %s", i, doca_error_get_descr(result));
			doca_try(result, "Failed to add hash entry", nb_ports, ports);
		}
	}

	result = doca_flow_entries_process(ports[0], 0, DEFAULT_TIMEOUT_US, config->numBackends);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to process hash entries: %s", doca_error_get_descr(result));
		doca_try(result, "Failed to process entries", nb_ports, ports);
	}

	if (status.nb_processed != config->numBackends) {
		DOCA_LOG_ERR("Failed to process all hash entries, nb_processed=%d, expected=%d", 
			status.nb_processed, config->numBackends);
		doca_try(DOCA_ERROR_BAD_STATE, "Failed to process all entries", nb_ports, ports);
	}

	if (status.failure) {
		DOCA_LOG_ERR("Hash entry processing failed");
		doca_try(DOCA_ERROR_BAD_STATE, "Hash entry processing failed", nb_ports, ports);
	}

	DOCA_LOG_INFO("Hash Pipe Load Balancer initialized with %d backends", config->numBackends);
	
	int statRefreshIntervall = 5000000;
	uint64_t last_packets[config->numBackends];
	memset(last_packets, 0, sizeof(last_packets));
	
  	while(1) {
		DOCA_LOG_INFO("Hash Pipe Load Balancer Status - %d backends", config->numBackends);
		
		for (int i = 0; i < config->numBackends; i++) {
			struct doca_flow_resource_query query_stats;
			doca_error_t query_result;
			
			query_result = doca_flow_resource_query_entry(hash_entries[i], &query_stats);
			
			uint64_t packets = 0;
			if (query_result == DOCA_SUCCESS) {
				packets = query_stats.counter.total_pkts;
			}
			
			DOCA_LOG_INFO("  Entry %d - %s: %lu packets (%lu new)",
				i, config->backends[i].name, packets, 
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
