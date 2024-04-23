// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 Robin Jarry

#include "ip.h"

#include <br_api.h>
#include <br_cli.h>
#include <br_ip4.h>
#include <br_net_types.h>

#include <ecoli.h>

#include <errno.h>
#include <stdint.h>

static cmd_status_t nh4_add(const struct br_api_client *c, const struct ec_pnode *p) {
	struct br_ip4_nh_add_req req = {0};

	if (inet_pton(AF_INET, arg_str(p, "IP"), &req.nh.host) != 1) {
		errno = EINVAL;
		return CMD_ERROR;
	}
	if (br_eth_addr_parse(arg_str(p, "MAC"), &req.nh.mac) < 0)
		return CMD_ERROR;
	if (arg_u16(p, "PORT_ID", &req.nh.port_id) < 0)
		return CMD_ERROR;

	if (br_api_client_send_recv(c, BR_IP4_NH_ADD, sizeof(req), &req, NULL) < 0)
		return CMD_ERROR;

	return CMD_SUCCESS;
}

static cmd_status_t nh4_del(const struct br_api_client *c, const struct ec_pnode *p) {
	struct br_ip4_nh_del_req req = {.missing_ok = true};

	if (inet_pton(AF_INET, arg_str(p, "IP"), &req.host) != 1) {
		errno = EINVAL;
		return CMD_ERROR;
	}

	if (br_api_client_send_recv(c, BR_IP4_NH_DEL, sizeof(req), &req, NULL) < 0)
		return CMD_ERROR;

	return CMD_SUCCESS;
}

static cmd_status_t nh4_list(const struct br_api_client *c, const struct ec_pnode *p) {
	const struct br_ip4_nh_list_resp *resp;
	char ip[BUFSIZ], state[BUFSIZ];
	void *resp_ptr = NULL;
	ssize_t n;

	(void)p;

	if (br_api_client_send_recv(c, BR_IP4_NH_LIST, 0, NULL, &resp_ptr) < 0)
		return CMD_ERROR;

	resp = resp_ptr;

	printf("%-16s  %-20s  %-8s  %-8s  %s\n", "IP", "MAC", "PORT", "AGE", "STATE");
	for (size_t i = 0; i < resp->n_nhs; i++) {
		const struct br_ip4_nh *nh = &resp->nhs[i];

		n = 0;
		state[0] = '\0';
		for (uint8_t i = 0; i < 16; i++) {
			br_ip4_nh_flags_t f = 1 << i;
			if (f & nh->flags) {
				n += snprintf(
					state + n, sizeof(state) - n, "%s ", br_ip4_nh_f_name(f)
				);
			}
		}
		if (n > 0)
			state[n - 1] = '\0';

		inet_ntop(AF_INET, &nh->host, ip, sizeof(ip));

		if (nh->flags & BR_IP4_NH_F_REACHABLE) {
			printf("%-16s  " ETH_ADDR_FMT "     %-8u  %-8u  %s\n",
			       ip,
			       ETH_BYTES_SPLIT(nh->mac.bytes),
			       nh->port_id,
			       nh->age,
			       state);
		} else {
			printf("%-16s  ??:??:??:??:??:??     ?         ?         %s\n", ip, state);
		}
	}

	free(resp_ptr);

	return CMD_SUCCESS;
}

static int ctx_init(struct ec_node *root) {
	int ret;

	ret = CLI_COMMAND(
		IP_ADD_CTX(root),
		"nexthop IP mac MAC port PORT_ID",
		nh4_add,
		"Add a new next hop.",
		with_help("IPv4 address.", ec_node_re("IP", IPV4_RE)),
		with_help("Ethernet address.", ec_node_re("MAC", ETH_ADDR_RE)),
		with_help("Output port ID.", ec_node_uint("PORT_ID", 0, UINT16_MAX - 1, 10))
	);
	if (ret < 0)
		return ret;
	ret = CLI_COMMAND(
		IP_DEL_CTX(root),
		"nexthop IP",
		nh4_del,
		"Delete a next hop.",
		with_help("IPv4 address.", ec_node_re("IP", IPV4_RE))
	);
	if (ret < 0)
		return ret;
	ret = CLI_COMMAND(IP_SHOW_CTX(root), "nexthop", nh4_list, "List all next hops.");
	if (ret < 0)
		return ret;

	return 0;
}

static struct br_cli_context ctx = {
	.name = "ipv4 nexthop",
	.init = ctx_init,
};

static void __attribute__((constructor, used)) init(void) {
	register_context(&ctx);
}
