// #define DEBUG

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

#include "woofc.h"
#include "woofc-host.h"
#include "dht.h"

#define ARGS "w:"
char *Usage = "join -w node_woof\n";

char node_woof[256];

char *woof_to_create[] = {DHT_TABLE_WOOF, DHT_FIND_SUCCESSOR_ARG_WOOF, DHT_FIND_SUCCESSOR_RESULT_WOOF,
	DHT_GET_PREDECESSOR_ARG_WOOF, DHT_GET_PREDECESSOR_RESULT_WOOF, DHT_NOTIFY_ARG_WOOF, DHT_NOTIFY_RESULT_WOOF,
	DHT_INIT_TOPIC_ARG_WOOF, DHT_SUBSCRIPTION_ARG_WOOF, DHT_TRIGGER_ARG_WOOF};
unsigned long woof_element_size[] = {sizeof(DHT_TABLE_EL), sizeof(FIND_SUCESSOR_ARG), sizeof(FIND_SUCESSOR_RESULT),
	sizeof(GET_PREDECESSOR_ARG), sizeof(GET_PREDECESSOR_RESULT), sizeof(NOTIFY_ARG), sizeof(NOTIFY_RESULT), 
	sizeof(INIT_TOPIC_ARG), sizeof(SUBSCRIPTION_ARG), sizeof(TRIGGER_ARG)};

int main(int argc, char **argv) {
	log_set_tag("join");
	// log_set_level(LOG_DEBUG);
	log_set_level(LOG_INFO);
	// FILE *f = fopen("log_join","w");
	// log_set_output(f);
	log_set_output(stdout);

	int c;
	while ((c = getopt(argc, argv, ARGS)) != EOF) {
		switch (c) {
			case 'w': {
				strncpy(node_woof, optarg, sizeof(node_woof));
				break;
			}
			default: {
				fprintf(stderr, "unrecognized command %c\n", (char)c);
				fprintf(stderr, "%s", Usage);
				exit(1);
			}
		}
	}

	char woof_name[DHT_NAME_LENGTH];
	if (node_woof_name(woof_name) < 0) {
		log_error("couldn't get local node's woof name");
		exit(1);
	}

	if (node_woof[0] == 0) {
		fprintf(stderr, "must specify a node woof to join\n");
		fprintf(stderr, "%s", Usage);
		exit(1);
	}
	
	WooFInit();

	int i;
	for (i = 0; i < 10; ++i) {
		if (WooFCreate(woof_to_create[i], woof_element_size[i], 10) < 0) {
			log_error("couldn't create woof %s", woof_to_create[i]);
			exit(1);
		}
		log_debug("created woof %s", woof_to_create[i]);
	}

	// compute the node hash with SHA1
	FIND_SUCESSOR_ARG arg;
	SHA1(woof_name, strlen(woof_name), arg.id_hash);
	log_info("woof_name: %s", woof_name);
	char msg[256];
	sprintf(msg, "hash: ");
	print_node_hash(msg + strlen(msg), arg.id_hash);
	log_info(msg);

	sprintf(arg.callback_woof, "%s/%s", woof_name, DHT_FIND_SUCCESSOR_RESULT_WOOF);
	sprintf(arg.callback_handler, "h_join_callback", sizeof(arg.callback_handler));
	if (node_woof[strlen(node_woof) - 1] == '/') {
		sprintf(node_woof, "%s%s", node_woof, DHT_FIND_SUCCESSOR_ARG_WOOF);
	} else {
		sprintf(node_woof, "%s/%s", node_woof, DHT_FIND_SUCCESSOR_ARG_WOOF);
	}

	unsigned long seq_no = WooFPut(node_woof, "h_find_successor", &arg);
	if (WooFInvalid(seq_no)) {
		log_error("couldn't call find_successor on woof %s", node_woof);
		exit(1);
	}
	log_info("called find_successor on %s", node_woof);

	return 0;
}
