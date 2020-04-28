#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "woofc.h"
#include "raft.h"
#include "monitor.h"

int h_count_vote(WOOF *wf, unsigned long seq_no, void *ptr) {
	RAFT_REQUEST_VOTE_RESULT *result = (RAFT_REQUEST_VOTE_RESULT *)monitor_cast(ptr);
	seq_no = monitor_seqno(ptr);

	log_set_tag("count_vote");
	// log_set_level(LOG_INFO);
	log_set_level(LOG_DEBUG);
	log_set_output(stdout);

	RAFT_SERVER_STATE server_state;
	if (get_server_state(&server_state) < 0) {
		log_error("failed to get the server state");
		free(result);
		exit(1);
	}

	// if result term is 0, it means the candidate is not in the leader config, shut it down
	if (result->term == 0) {
		server_state.role = RAFT_SHUTDOWN;
		unsigned long seq = WooFPut(RAFT_SERVER_STATE_WOOF, NULL, &server_state);
		if (WooFInvalid(seq)) {
			log_error("failed to shutdown");
			free(result);
			exit(1);
		}
		log_info("server not in the leader config anymore: SHUTDOWN");
		free(result);
		monitor_exit(ptr);
		return 1;
	}
	// server's term is higher than the vote's term, ignore it
	if (result->term < server_state.current_term) {
		log_debug("current term %lu is higher than vote's term %lu, ignore the election", server_state.current_term, result->term);
		free(result);
		monitor_exit(ptr);
		return 1;
	}
	// the server is already a leader at vote's term, ifnore the vote
	if (result->term == server_state.current_term && server_state.role == RAFT_LEADER) {
		log_debug("already a leader at term %lu, ignore the election", server_state.current_term);
		free(result);
		monitor_exit(ptr);
		return 1;
	}

	// start counting the votes
	int granted_votes = 0;
	if (result->granted == 1) {
		++granted_votes;
	}
	RAFT_REQUEST_VOTE_RESULT vote;
	unsigned long vote_seq;
	for (vote_seq = result->candidate_vote_pool_seqno + 1; vote_seq < seq_no; ++vote_seq) {
		if (WooFGet(RAFT_REQUEST_VOTE_RESULT_WOOF, &vote, vote_seq) < 0) {
			log_error("failed to get the vote result at seqno %lu", vote_seq);
			free(result);
			exit(1);
		}
		if (vote.granted == 1 && vote.term == result->term) {
			++granted_votes;
			if (granted_votes > server_state.members / 2) {
				break;
			}
		}
	}
	log_debug("counted %d granted votes for term %lu", granted_votes, result->term);

	// if the majority granted, promoted to leader
	if (granted_votes > server_state.members / 2) {
		unsigned long last_log_entry_seqno = WooFGetLatestSeqno(RAFT_LOG_ENTRIES_WOOF);
		if (WooFInvalid(last_log_entry_seqno)) {
			log_error("failed to get the latest seqno from %s", RAFT_LOG_ENTRIES_WOOF);
			free(result);
			exit(1);
		}
		server_state.current_term = result->term;
		server_state.role = RAFT_LEADER;
		memcpy(server_state.current_leader, server_state.woof_name, RAFT_WOOF_NAME_LENGTH);
		memcpy(server_state.voted_for, server_state.woof_name, RAFT_WOOF_NAME_LENGTH);
		int i;
		for (i = 0; i < RAFT_MAX_MEMBERS + RAFT_MAX_OBSERVERS; ++i) {
			server_state.next_index[i] = last_log_entry_seqno + 1;
			server_state.match_index[i] = 0;
			server_state.last_sent_index[i] = 0;
			server_state.last_sent_timestamp[i] = 0;
		}
		unsigned long seq = WooFPut(RAFT_SERVER_STATE_WOOF, NULL, &server_state);
		if (WooFInvalid(seq)) {
			log_error("failed to promote itself to leader");
			free(result);
			exit(1);
		}
		log_debug("promoted to leader for term %lu", result->term);
		log_info("state changed at term %lu: LEADER", server_state.current_term);

		// start replicate_entries handlers
		unsigned long last_append_result_seqno = WooFGetLatestSeqno(RAFT_APPEND_ENTRIES_RESULT_WOOF);
		if (WooFInvalid(last_append_result_seqno)) {
			log_error("failed to get the latest seqno from %s", RAFT_APPEND_ENTRIES_RESULT_WOOF);
			free(result);
			exit(1);
		}
		RAFT_REPLICATE_ENTRIES_ARG replicate_entries_arg;
		replicate_entries_arg.term = server_state.current_term;
		replicate_entries_arg.last_seen_result_seqno = last_append_result_seqno;
		seq = monitor_put(RAFT_MONITOR_NAME, RAFT_REPLICATE_ENTRIES_WOOF, "h_replicate_entries", &replicate_entries_arg);
		if (WooFInvalid(seq)) {
			log_error("failed to start h_replicate_entries handler");
			free(result);
			exit(1);
		}
	}

	free(result);
	monitor_exit(ptr);
	return 1;
}
