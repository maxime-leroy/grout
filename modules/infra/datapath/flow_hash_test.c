// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 Maxime Leroy

#include "_cmocka.h"
#include "flow_hash.h"

#include <netinet/in.h>
#include <string.h>

// Large enough for an IPv6 header followed by an L4 header.
struct fake_mbuf {
	uint8_t buf[128];
	struct rte_mbuf mbuf;
};

static void fm_init(struct fake_mbuf *fm, uint16_t len) {
	memset(fm, 0, sizeof(*fm));
	fm->mbuf.buf_addr = fm->buf;
	fm->mbuf.data_len = len;
	fm->mbuf.pkt_len = len;
	fm->mbuf.nb_segs = 1;
}

// IPv4 header with a TCP or UDP header right after it.
static void fm_init_ip4_l4(struct fake_mbuf *fm, uint8_t proto, rte_be16_t frag_offset) {
	struct rte_ipv4_hdr *ip;
	struct rte_tcp_hdr *l4;

	fm_init(fm, sizeof(*ip) + sizeof(*l4));

	ip = (struct rte_ipv4_hdr *)fm->buf;
	ip->version = 4;
	ip->ihl = sizeof(*ip) / 4;
	ip->total_length = rte_cpu_to_be_16(sizeof(*ip) + sizeof(*l4));
	ip->time_to_live = 64;
	ip->next_proto_id = proto;
	ip->fragment_offset = frag_offset;
	ip->src_addr = RTE_IPV4(192, 168, 0, 1);
	ip->dst_addr = RTE_IPV4(192, 168, 0, 2);

	// Both rte_tcp_hdr and rte_udp_hdr start with the two ports.
	l4 = (struct rte_tcp_hdr *)(fm->buf + sizeof(*ip));
	l4->src_port = rte_cpu_to_be_16(45678);
	l4->dst_port = rte_cpu_to_be_16(179);
}

static uint32_t hash_of(struct fake_mbuf *fm) {
	uint32_t hash = 0;

	assert_true(flow_hash_l3l4(&fm->mbuf, 0, RTE_BE16(RTE_ETHER_TYPE_IPV4), &hash));

	return hash;
}

// The don't fragment flag shares the field with the fragment offset. Setting it
// must not make the packet look like a fragment and cost it its L4 ports.
static void df_hashes_like_no_df(uint8_t proto) {
	struct fake_mbuf fm;
	uint32_t plain, df;

	fm_init_ip4_l4(&fm, proto, 0);
	plain = hash_of(&fm);

	fm_init_ip4_l4(&fm, proto, RTE_BE16(RTE_IPV4_HDR_DF_FLAG));
	df = hash_of(&fm);

	assert_int_equal(plain, df);
}

static void flow_hash_tcp_df(void **) {
	df_hashes_like_no_df(IPPROTO_TCP);
}

static void flow_hash_udp_df(void **) {
	df_hashes_like_no_df(IPPROTO_UDP);
}

// A real fragment still has its ports ignored, so its hash differs from the
// same packet sent unfragmented.
static void fragment_ignores_ports(rte_be16_t frag_offset) {
	struct fake_mbuf fm;
	uint32_t plain, frag;

	fm_init_ip4_l4(&fm, IPPROTO_TCP, 0);
	plain = hash_of(&fm);

	fm_init_ip4_l4(&fm, IPPROTO_TCP, frag_offset);
	frag = hash_of(&fm);

	assert_int_not_equal(plain, frag);
}

static void flow_hash_more_fragments(void **) {
	fragment_ignores_ports(RTE_BE16(RTE_IPV4_HDR_MF_FLAG));
}

static void flow_hash_fragment_offset(void **) {
	fragment_ignores_ports(rte_cpu_to_be_16(1480 / 8));
}

// A fragment with the don't fragment flag also set is still a fragment.
static void flow_hash_fragment_with_df(void **) {
	fragment_ignores_ports(RTE_BE16(RTE_IPV4_HDR_DF_FLAG | RTE_IPV4_HDR_MF_FLAG));
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(flow_hash_tcp_df),
		cmocka_unit_test(flow_hash_udp_df),
		cmocka_unit_test(flow_hash_more_fragments),
		cmocka_unit_test(flow_hash_fragment_offset),
		cmocka_unit_test(flow_hash_fragment_with_df),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
