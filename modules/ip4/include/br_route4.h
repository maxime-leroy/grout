// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 Robin Jarry

#ifndef _BR_IP_ROUTE4
#define _BR_IP_ROUTE4

// XXX: why not 1337, eh?
#define BR_MAX_ROUTES (1 << 16)
#define BR_NO_ROUTE 0
#define BR_IP4_FIB_NAME "route4"

struct rte_rcu_qsbr *br_route4_rcu(void);

#endif
