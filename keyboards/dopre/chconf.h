// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// QMK's default ChibiOS configuration runs the system tick off a free-running hardware timer, and
// on STM32G0 that timer is TIM2. The key sensing needs TIM2 (and TIM3/TIM4) for input capture, so
// use the periodic SysTick instead. This leaves all general-purpose timers free.
#define CH_CFG_ST_FREQUENCY 1000
#define CH_CFG_ST_TIMEDELTA 0

#include_next <chconf.h>
