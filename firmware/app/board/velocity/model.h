/* SPDX-License-Identifier: MPL-2.0
 * Default key velocity settings adapted from fm1-base lib/touch.c.
 * Copyright (C) 2026 Charles Vestal. */
#pragma once

#include "sampler_touch.h"
#include "sense.inc"

enum { samplerAnalogMask = (1u << SAMPLER_ANALOG_KEYS) - 1u };
static uint8_t sampler_curve = SAMPLER_CURVE_NEUTRAL;

static uint16_t sampler_curve_exponent(uint8_t curve)
{
    const int adjustment = (int)curve - SAMPLER_CURVE_NEUTRAL;
    return (uint16_t)(70.0f * fm_exp2f((float)adjustment * 0.2f) + 0.5f);
}

static void sampler_touch_preview(uint8_t curve, uint16_t exponent, uint8_t learn)
{
    sampler_curve = curve;
    sn_cfg.d_curve = exponent;
    sn_cfg.learn = learn;
}

static void sampler_touch_reset(void)
{
    sampler_touch_preview(SAMPLER_CURVE_NEUTRAL, 70, 0);
    sense_cal_reset();
}

static void sampler_touch_init(void)
{
    /* Values used by fm1-base/lib/touch.c at its neutral settings. */
    sn_cfg.wait = 2;
    sn_cfg.drive = 2;
    sn_cfg.d_floor = 530;
    sn_cfg.d_ceil = 896;
    sn_cfg.t_lo = 0;
    sn_cfg.t_hi = 43;
    sn_cfg.t_curve = 240;
    sense_init(samplerAnalogMask);
    sampler_touch_preview(SAMPLER_CURVE_NEUTRAL, 70, 0);
}

static void sampler_touch_capture(struct sampler_velocity_profile *profile)
{
    sn_cfg.learn = 0;
    profile->curve = sampler_curve;
    sense_cal_get(profile->hard, profile->soft, profile->landings);
}

static int sampler_touch_valid(const struct sampler_velocity_profile *profile)
{
    if (profile->curve > SAMPLER_CURVE_MAX)
        return 0;
    for (unsigned key = 0; key < SAMPLER_ANALOG_KEYS; ++key) {
        const unsigned hard = profile->hard[key];
        const unsigned soft = profile->soft[key];
        if (hard < SAMPLER_ADC_HARD_MIN || hard > SAMPLER_ADC_HARD_MAX ||
            soft < hard + SAMPLER_ADC_SPAN_MIN || soft > SAMPLER_ADC_SOFT_MAX ||
            profile->landings[key] > 127u)
            return 0;
    }
    return 1;
}

static void sampler_touch_apply(const struct sampler_velocity_profile *profile, uint16_t exponent)
{
    sampler_touch_preview(profile->curve, exponent, 0);
    sense_cal_set(profile->hard, profile->soft, profile->landings);
}

static uint8_t sampler_top_velocity(const sense_probe_t* probe)
{
    /* An unchanged, rail, or unmeasurable probe leaves the key digital. */
    const uint8_t velocity = probe->peak >= 4u && probe->v > 0u && probe->v < 1023u ?
        sampler_probe_velocity(probe) : 0;
    return velocity ? velocity : SAMPLER_DEFAULT_VELOCITY;
}
