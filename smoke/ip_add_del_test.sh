#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Christophe Fontaine

. $(dirname $0)/_init.sh

p0=${run_id}0

grcli add interface port $p0 devargs net_tap0,iface=$p0 mac f0:0d:ac:dc:00:00

grcli add ip address 172.16.0.1/24 iface $p0
grcli show ip address
grcli del ip address 172.16.0.1/24 iface $p0
grcli show ip address
grcli add ip address 172.16.0.1/24 iface $p0
grcli show ip address
