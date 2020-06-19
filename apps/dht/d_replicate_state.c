#include <stdio.h>
#include <string.h>
#include <time.h>
#include "woofc.h"
#include "dht.h"
#include "dht_utils.h"
#include "raft_client.h"

int d_replicate_state(WOOF *wf, unsigned long seq_no, void *ptr) {
	DHT_REPLICATE_STATE_ARG *arg = (DHT_REPLICATE_STATE_ARG *)ptr;
	
	log_set_tag("replicate_state");
	log_set_level(DHT_LOG_DEBUG);
	// log_set_level(DHT_LOG_INFO);
	log_set_output(stdout);

	unsigned long now = get_milliseconds();

	DHT_NODE_INFO node = {0};
	if (get_latest_node_info(&node) < 0) {
		log_error("couldn't get latest node info: %s", dht_error_msg);
		exit(1);
	}
	raft_set_client_leader(node.addr);
	unsigned long index = raft_put_handler("r_set_node", &node, sizeof(DHT_NODE_INFO), 0);
	while (index == RAFT_REDIRECTED) {
		log_debug("redirected to %s", raft_client_leader);
		index = raft_put_handler("r_set_node", &node, sizeof(DHT_NODE_INFO), 0);
	}
	if (raft_is_error(index)) {
		log_error("failed to invoke r_set_node using raft: %s", raft_error_msg);
		exit(1);
	}
	log_debug("node info is replicated");

	DHT_PREDECESSOR_INFO predecessor = {0};
	if (get_latest_predecessor_info(&predecessor) < 0) {
		log_error("couldn't get latest predecessor info: %s", dht_error_msg);
		exit(1);
	}
	
	index = raft_put_handler("r_set_predecessor", &predecessor, sizeof(DHT_PREDECESSOR_INFO), 0);
	while (index == RAFT_REDIRECTED) {
		log_debug("redirected to %s", raft_client_leader);
		index = raft_put_handler("r_set_predecessor", &predecessor, sizeof(DHT_PREDECESSOR_INFO), 0);
	}
	if (raft_is_error(index)) {
		log_error("failed to invoke r_set_predecessor using raft: %s", raft_error_msg);
		exit(1);
	}
	log_debug("predecessor is replicated");

	DHT_SUCCESSOR_INFO successor = {0};
	if (get_latest_successor_info(&successor) < 0) {
		log_error("couldn't get latest successor info: %s", dht_error_msg);
		exit(1);
	}
	index = raft_put_handler("r_set_successor", &successor, sizeof(DHT_SUCCESSOR_INFO), 0);
	while (index == RAFT_REDIRECTED) {
		log_debug("redirected to %s", raft_client_leader);
		raft_put_handler("r_set_successor", &successor, sizeof(DHT_SUCCESSOR_INFO), 0);
	}
	if (raft_is_error(index)) {
		log_error("failed to invoke r_set_successor using raft: %s", raft_error_msg);
		exit(1);
	}
	log_debug("successor is replicated");
	return 1;
}