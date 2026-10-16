# Copyright 2026 Vestigl Creations
# SPDX-License-Identifier: GPL-2.0-or-later

# The shared dopre scan system, used by every board under keyboards/dopre/. info.json (next to this
# file) selects the custom "lite" matrix and the custom debounce type; these are the sources behind
# them. dopre.c is not listed because QMK compiles <vendor>.c on its own.
SRC += matrix.c
SRC += dopre_scan.c
SRC += dopre_filter.c
SRC += lockout_debounce.c
