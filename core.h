#ifndef CORE_H
#define CORE_H

#include <doca_flow.h>

/**
 * @brief Main XenoFlow function - initializes and runs the flow load balancer
 * @param nb_queues Number of queues to use
 * @return DOCA_SUCCESS on success, error code otherwise
 */
doca_error_t xeno_flow(int nb_queues);

#endif /* CORE_H */
