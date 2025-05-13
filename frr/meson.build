# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (c) 2025 Maxime Leroy

if not get_option('frr').enabled()
  subdir_done()
endif

frr_dep = dependency('frr', version: '>= 10.3', fallback: ['frr', 'frr_dep'])
api_headers_dep = declare_dependency(
  sources: api_headers
)

frr_plugin_src = files(
  'zebra_dplane_grout.c',
  'rt_grout.c',
  'if_grout.c',
)

shared_module(
  'frr_dplane_grout',
  frr_plugin_src,
  name_prefix: '',
  dependencies: [frr_dep, api_headers_dep],
  include_directories: api_inc + include_directories('.'),
  install: true,
  install_dir: frr_dep.get_variable('moduledir'),
)
