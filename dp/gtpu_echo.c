/*
 * Copyright (c) 2019 Sprint
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <rte_log.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include "ipv4.h"
#include "gtpu.h"
//#include "gtpu_echo.h"
#include "util.h"
#include "clogger.h"

/* VS: TODO*/
//static uint8_t resp_cnt = 1;

/**
 * @brief  : Function to set echo request as echo response
 * @param  : echo_pkt rte_mbuf pointer
 * @return : Returns nothing
 */
static void reset_req_pkt_as_resp(struct rte_mbuf *echo_pkt) {
	/* Swap src and destination mac addresses */
	struct ether_hdr *eth_h = rte_pktmbuf_mtod(echo_pkt, struct ether_hdr *);
	struct ether_addr tmp_mac;
	ether_addr_copy(&eth_h->d_addr, &tmp_mac);
	ether_addr_copy(&eth_h->s_addr, &eth_h->d_addr);
	ether_addr_copy(&tmp_mac, &eth_h->s_addr);

	/* Swap src and dst IP addresses */
	struct ipv4_hdr *ip_hdr = get_mtoip(echo_pkt);
	uint32_t tmp_ip = ip_hdr->dst_addr;
	ip_hdr->dst_addr = ip_hdr->src_addr;
	ip_hdr->src_addr = tmp_ip;
	ip_hdr->total_length = htons(ntohs(ip_hdr->total_length)+
			sizeof(gtpu_recovery_ie));

	/* Swap src and dst UDP ports */
	struct udp_hdr *udphdr = get_mtoudp(echo_pkt);
	uint16_t tmp_port = udphdr->dst_port;
	udphdr->dst_port = udphdr->src_port;
	udphdr->src_port = tmp_port;
	udphdr->dgram_len = htons(ntohs(udphdr->dgram_len)+
			sizeof(gtpu_recovery_ie));
}

/**
 * @brief  : Function to set recovery IE
 * @param  : echo_pkt rte_mbuf pointer
 * @return : Returns 0 in case of success , -1 otherwise
 */
static int set_recovery(struct rte_mbuf *echo_pkt, uint8_t port_id) {
	//struct ipv4_hdr *ip_hdr = get_mtoip(echo_pkt);
	struct gtpu_hdr *gtpu_hdr = get_mtogtpu(echo_pkt);
	gtpu_recovery_ie *recovery_ie = NULL;

	/* VS: Fix the recovery IE issue for response */
	recovery_ie = (gtpu_recovery_ie *)rte_pktmbuf_append(echo_pkt,
			(sizeof(gtpu_recovery_ie)));

	//if (((app.spgw_cfg == SGWU) || (app.spgw_cfg == SAEGWU)) &&
	//		(port_id == S1U_PORT_ID)) {
	//	if (echo_pkt->pkt_len - (ETHER_HDR_LEN +(ip_hdr->total_length))) {
	//		recovery_ie = (gtpu_recovery_ie*)((char*)gtpu_hdr+
	//				GTPU_HDR_SIZE + ntohs(gtpu_hdr->msglen));
	//	} else if ((echo_pkt->pkt_len - (ETHER_HDR_LEN +(ip_hdr->total_length))) == 0) {
	//		recovery_ie = (gtpu_recovery_ie *)rte_pktmbuf_append(echo_pkt,
	//				(sizeof(gtpu_recovery_ie)));
	//	}
	//} else {
	//	recovery_ie = (gtpu_recovery_ie *)rte_pktmbuf_append(echo_pkt,
	//			(sizeof(gtpu_recovery_ie)));
	//}

	if (recovery_ie == NULL) {
		clLog(clSystemLog, eCLSeverityCritical, "Couldn't append %lu bytes to mbuf",
				sizeof(gtpu_recovery_ie));
		 return -1;
	}

	gtpu_hdr->msgtype = GTPU_ECHO_RESPONSE;
	gtpu_hdr->msglen = htons(ntohs(gtpu_hdr->msglen)+
		sizeof(gtpu_recovery_ie));
	recovery_ie->type = GTPU_ECHO_RECOVERY;
	//recovery_ie->restart_cntr = resp_cnt;
	recovery_ie->restart_cntr = 0;
	return 0;
}

/**
 * @brief  : Function to set checksum of IPv4 and UDP header
 * @param  : echo_pkt rte_mbuf pointer
 * @return : Returns nothing
 */
static void set_checksum(struct rte_mbuf *echo_pkt) {
	struct ipv4_hdr *ipv4hdr = get_mtoip(echo_pkt);
	ipv4hdr->hdr_checksum = 0;
	struct udp_hdr *udphdr = get_mtoudp(echo_pkt);
	udphdr->dgram_cksum = 0;
	udphdr->dgram_cksum = rte_ipv4_udptcp_cksum(ipv4hdr, udphdr);
	ipv4hdr->hdr_checksum = rte_ipv4_cksum(ipv4hdr);
}

void process_echo_request(struct rte_mbuf *echo_pkt, uint8_t port_id) {
	int ret;
	ret = set_recovery(echo_pkt, port_id);
	if (ret < 0) {
		clLog(clSystemLog, eCLSeverityCritical, "Failed to create echo response..\n");
		return;
	}

	reset_req_pkt_as_resp(echo_pkt);
	set_checksum(echo_pkt);
}
