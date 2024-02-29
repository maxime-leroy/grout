// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2024 Robin Jarry

#include "graph.h"
#include "port_config.h"
#include "worker.h"

#include <br_api.h>
#include <br_control.h>
#include <br_infra_msg.h>
#include <br_log.h>
#include <br_port.h>
#include <br_queue.h>
#include <br_stb_ds.h>
#include <br_worker.h>

#include <rte_ethdev.h>

#include <errno.h>
#include <unistd.h>

static struct api_out rxq_list(const void *request, void **response) {
	struct br_infra_rxq_list_resp *resp = NULL;
	struct queue_map *qmap;
	struct worker *worker;
	uint16_t n_rxqs = 0;
	size_t len;

	(void)request;

	LIST_FOREACH (worker, &workers, next)
		n_rxqs += arrlen(worker->rxqs);

	len = sizeof(*resp) + n_rxqs * sizeof(struct br_infra_rxq);
	if ((resp = malloc(len)) == NULL)
		return api_out(ENOMEM, 0);

	memset(resp, 0, len);

	n_rxqs = 0;
	LIST_FOREACH (worker, &workers, next) {
		arrforeach (qmap, worker->rxqs) {
			struct br_infra_rxq *q = &resp->rxqs[n_rxqs];
			q->port_id = qmap->port_id;
			q->rxq_id = qmap->queue_id;
			q->cpu_id = worker->cpu_id;
			q->enabled = qmap->enabled;
			n_rxqs++;
		}
	}
	resp->n_rxqs = n_rxqs;
	*response = resp;

	return api_out(0, len);
}

static struct api_out rxq_set(const void *request, void **response) {
	const struct br_infra_rxq_set_req *req = request;
	struct worker *src_worker, *dst_worker;
	struct queue_map *qmap;
	int ret;

	(void)response;

	LIST_FOREACH (src_worker, &workers, next) {
		arrforeach (qmap, src_worker->rxqs) {
			if (qmap->port_id != req->port_id)
				continue;
			if (qmap->queue_id != req->rxq_id)
				continue;
			if (src_worker->cpu_id == req->cpu_id) {
				// rxq already assigned to the correct worker
				return api_out(0, 0);
			}
			goto dest;
		}
	}
	return api_out(ENODEV, 0);
dest:
	dst_worker = worker_find(req->cpu_id);
	if (dst_worker == NULL) {
		// no worker assigned to this cpu id yet, create one
		if (worker_create(req->cpu_id) < 0)
			return api_out(errno, 0);
		dst_worker = worker_find(req->cpu_id);

		struct port *port;
		// one more worker, need to reconfigure all ports to update tx queues
		LIST_FOREACH (port, &ports, next) {
			if ((ret = port_unplug(port)) < 0)
				return api_out(-ret, 0);
			if ((ret = port_reconfig(port, 0)) < 0)
				return api_out(-ret, 0);
			if ((ret = port_plug(port)) < 0)
				return api_out(-ret, 0);
		}
	}

	// unassign from src_worker
	for (int i = 0; i < arrlen(src_worker->rxqs); i++) {
		struct queue_map *qmap = &src_worker->rxqs[i];
		if (qmap->port_id != req->port_id)
			continue;
		if (qmap->queue_id != req->rxq_id)
			continue;
		arrdelswap(src_worker->rxqs, i);
		break;
	}

	// assign to dst_worker
	struct queue_map rx_qmap = {
		.port_id = req->port_id,
		.queue_id = req->rxq_id,
		.enabled = true,
	};
	arrpush(dst_worker->rxqs, rx_qmap);

	ret = worker_graph_reload_all();
	return api_out(-ret, 0);
}

static struct br_api_handler rxq_list_handler = {
	.name = "rxq list",
	.request_type = BR_INFRA_RXQ_LIST,
	.callback = rxq_list,
};
static struct br_api_handler rxq_set_handler = {
	.name = "rxq set",
	.request_type = BR_INFRA_RXQ_SET,
	.callback = rxq_set,
};

RTE_INIT(rxq_init) {
	br_register_api_handler(&rxq_list_handler);
	br_register_api_handler(&rxq_set_handler);
}
