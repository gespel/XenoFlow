#ifndef CORE_H
#define CORE_H

#include <doca_flow.h>
#include <stdint.h>

/**
 * @brief Backend structure
 */
typedef struct {
	char name[64];
	uint8_t mac_address[6];
} XenoFlowBackend;

/**
 * @brief XenoFlow configuration structure
 */
typedef struct {
	XenoFlowBackend *backends;
	int numBackends;
	int nextBackend;
} XenoFlowConfig;

/**
 * @brief Main XenoFlow function - initializes and runs the flow load balancer
 * @param nb_queues Number of queues to use
 * @return DOCA_SUCCESS on success, error code otherwise
 */
doca_error_t xeno_flow(int nb_queues);

#endif /* CORE_H */
