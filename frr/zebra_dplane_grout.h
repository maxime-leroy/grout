// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2025 Maxime Leroy, Free Mobile

#pragma once

#include <lib/ns.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GROUT_NS NS_DEFAULT

// Tag identifying the marker route the plugin injects to detect
// META_QUEUE_EARLY_ROUTE drain completion. Used by the polling logic
// in zebra_dplane_grout.c and to filter the DELETE round-trip back
// to grout in rt_grout.c.
#define GROUT_SYNC_MARKER_TAG 0x03011986U

// Tag identifying the drain sentinel ctx injected right after the
// rib_sweep_route walk. The ctx is built directly via dplane_route_delete
// (it never enters zebra's RIB tree, so rib_process never sees it and
// ZAPI never notifies anyone). It rides at the tail of the dplane queue,
// FIFO with all sweep-enqueued deletes. The grout provider recognises
// the tag and short-circuits before any round-trip to grout, so grout's
// FIB stays untouched. Observation = end of drain.
#define GROUT_DRAIN_SENTINEL_TAG 0x05DA1A1AU

// Called from the grout dplane provider's per-ctx handler. Returns
// true if the ctx is the drain sentinel: caller must short-circuit
// (no round-trip to grout). Idempotent.
struct zebra_dplane_ctx;
bool grout_drain_sentinel_consume(const struct zebra_dplane_ctx *ctx);

int grout_client_send_recv(uint32_t req_type, size_t tx_len, const void *tx_data, void **rx_data);

// Bump per-VRF route activity counters exported by "show dataplane grout
// stats". family is AF_INET or AF_INET6; add=true for ROUTE_ADD/UPDATE,
// false for ROUTE_DELETE; ok=false marks the op as failed (grout returned
// an error).
void grout_stats_route_op(uint32_t vrf_id, int family, bool add, bool ok);

// Sweep-instrumentation counters. *_inject is bumped from grout_route_change
// when a startup-dump SELFROUTE entry is handed to rib_add_multipath; the
// total is logged at end-of-sync. *_inject_failed is bumped on the same
// callsite when rib_add_multipath returns < 0, so the difference between
// scanned and failed = entries that actually landed in zebra's RIB.
// *_delete is bumped from grout_add_del_route for every SELFROUTE delete
// the provider sees; the total observed during the sweep-drain window is
// logged when the drain sentinel surfaces.
void grout_stats_selfroute_inject(int family);
void grout_stats_selfroute_inject_failed(int family);
void grout_stats_selfroute_delete(int family);
