#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h> 

#include <rte_byteorder.h>

#include <doca_log.h>
#include <doca_flow.h>
#include <doca_dev.h>

#include "flow_common.h"


DOCA_LOG_REGISTER(FLOW_SHARED_COUNTER);

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
#include <time.h>
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

  	//match.outer.l3_type = DOCA_FLOW_L3_TYPE_IP4;
	//match.outer.ip4.src_ip = BE_IPV4_ADDR(255, 255, 255, 255);
	//match_mask.outer.ip4.src_ip = BE_IPV4_ADDR(0, 0, 0, 1);
	//DOCA_LOG_INFO("%d", match_mask.outer.ip4.src_ip);


	SET_MAC_ADDR(actions0.outer.eth.dst_mac, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
	SET_MAC_ADDR(actions0.outer.eth.src_mac, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff);

	actions_arr[0] = &actions0;


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

static doca_error_t init_mac_change_entry(struct doca_flow_pipe *pipe,
						  enum doca_flow_l4_type_ext out_l4_type,
						  uint32_t shared_counter_id,
						  struct entries_status *status,
						  struct doca_flow_match *match,
						  struct doca_flow_actions *actions,
						  struct doca_flow_monitor *monitor,
						  struct doca_flow_pipe_entry *entry)
{

	monitor->shared_counter.shared_counter_id = shared_counter_id;

	match->outer.ip4.src_ip = BE_IPV4_ADDR(0, 0, 0, 1);	

	actions->action_idx = 0;

	SET_MAC_ADDR(actions->outer.eth.dst_mac, 0xa0, 0x88, 0xc2, 0xb5, 0xf4, 0x5a);
	SET_MAC_ADDR(actions->outer.eth.src_mac, 0xc4, 0x70, 0xbd, 0xa0, 0x56, 0xbd);

	/*result = doca_flow_pipe_add_entry(0, pipe, &match, &actions, &monitor, NULL, 0, status, &entry_mac);
	if (result != DOCA_SUCCESS) {
		DOCA_LOG_ERR("Failed to add entry: %s", doca_error_get_descr(result));
		return result;
	}*/
	return DOCA_SUCCESS;
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
			char ipout[100]; 
			DOCA_LOG_INFO("IPv4: %u.%u.%u.%u", ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
			err = doca_dev_open(list[i], &dev);
            break;
        }
    }

    doca_devinfo_destroy_list(list);
    return dev;
}

doca_error_t xeno_flow_entry_latency(int nb_queues)
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
	uint32_t action_mem[2];
	uint32_t actions_mem_size[nb_ports];
	//ARRAY_INIT(action_mem, ACTIONS_MEM_SIZE(4));

	nr_shared_resources[DOCA_FLOW_SHARED_RESOURCE_COUNTER] = 2;

	doca_try(init_doca_flow(nb_queues, "vnf,hws", &resource, nr_shared_resources),
			"Failed to init DOCA Flow", nb_ports, ports);
	struct doca_dev *dev1 = open_doca_dev_by_pci("0000:0d:00.0");
	struct doca_dev *dev2 = open_doca_dev_by_pci("0000:0d:00.1");

	if (!dev1 || !dev2) {
		DOCA_LOG_INFO("Device not found");
		return;
	}
	else {
		DOCA_LOG_INFO("Devices found!");
	}
	dev_arr[0] = dev1;
	dev_arr[1] = dev2;

	if(result != DOCA_SUCCESS) {
		DOCA_LOG_INFO("DOCA ports error");
	}
    
	

	doca_try(init_doca_flow_ports(2, ports, true, dev_arr), "Failed to init DOCA ports", nb_ports, ports);
	memset(&status, 0, sizeof(status));

	doca_try(create_root_pipe(ports[0], 0, DOCA_FLOW_L4_TYPE_EXT_UDP, &udp_pipe), "Failed to create pipe", nb_ports, ports);
	
	int statRefreshIntervall = 10000;

	struct doca_flow_match match;
	struct doca_flow_actions actions;
	struct doca_flow_monitor monitor;
	struct doca_flow_pipe_entry *entry;

	memset(&match, 0, sizeof(match));
	memset(&actions, 0, sizeof(actions));
	memset(&monitor, 0, sizeof(monitor));

	init_mac_change_entry(udp_pipe, DOCA_FLOW_L4_TYPE_EXT_UDP, shared_counter_ids[0], &status, &match, &actions, &monitor, entry);

	DOCA_LOG_INFO("Starting the load balancer loop");

	srand(time(NULL));

	int nr_entry = 0;

  	while(1) {
		if (nr_entry == 8200) {
			break;
		}
		struct timespec ts;
		int random_delay = (rand() % 1000) + 1;
		clock_gettime(CLOCK_REALTIME, &ts);

		struct tm tm;
		localtime_r(&ts.tv_sec, &tm);

		DOCA_LOG_INFO("Adding entry at: %02d:%02d:%02d.%09ld", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		result = doca_flow_pipe_add_entry(0, udp_pipe, &match, &actions, &monitor, DOCA_FLOW_NO_WAIT, 0, &status, &entry);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to add entry: %s", doca_error_get_descr(result));
			return result;
		}
		result = doca_flow_entries_process(ports[0], 0, DEFAULT_TIMEOUT_US, num_of_entries);
		if (result != DOCA_SUCCESS) {
			DOCA_LOG_ERR("Failed to process entries: %s", doca_error_get_descr(result));
			return result;
		}
		nr_entry += 1;
		DOCA_LOG_INFO("Added nr %i", nr_entry);
		//usleep(statRefreshIntervall + random_delay);
		clock_gettime(CLOCK_REALTIME, &ts);
		localtime_r(&ts.tv_sec, &tm);

		//usleep(statRefreshIntervall);
		#ifdef CLEAR
		system("clear");
		#endif
	}

	result = stop_doca_flow_ports(nb_ports, ports);
	//doca_flow_destroy();
	return result;
}
