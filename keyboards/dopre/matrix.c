// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#include "matrix.h"
#include "dopre_scan.h"

// CUSTOM_MATRIX = lite: QMK owns the matrix arrays and the debounce call, we only fill in the raw
// matrix. Row = sense channel, column = mux state.

void matrix_init_custom(void) {
    dopre_init();
}

bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    return dopre_scan(current_matrix);
}
