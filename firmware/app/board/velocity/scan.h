/* SPDX-License-Identifier: MPL-2.0
 * ADC scheduling and pin drive adapted from felucca-bsp's boards/fm1/fm1_input.h.
 * Copyright (C) 2026 Leo Kuroshita, Hügelton Instruments and Charles Vestal. */
#pragma once

#include "fm1_adc.h"
#include "model.h"

/* Key indices follow the application's order. Separate from the 14 panel
 * buttons, which are still scanned digitally regardless of the row. */
enum {
    samplerRowTop = 2,
    samplerRowPA7 = 3,
    samplerRowPA8 = 4,
    samplerProbeChannel = 2,
    samplerBatteryInterval = 60,
    samplerAdcTimeoutTicks = 2400000, /* 100 ms at TIMER4's 24 MHz */
};
enum SamplerAdcInput { adcKey, adcTop, adcMaster, adcBattery };

#define SAMPLER_TIMER4_COUNT (*(volatile uint32_t *)0x10804u)

static struct {
    uint16_t master, battery;
    uint8_t phase, batteryClock, pending, drivenPin;
    uint8_t column, key;
    enum SamplerAdcInput input;
    volatile uint32_t lastAnalogSample;
    uint8_t haveAnalogSample;
    uint8_t positionToKey[FM1_NCOL][3];
} samplerAdc;

static void sampler_adc_init(const uint8_t* keyPins)
{
    unsigned key;
    fm1_adc_init();
    for (key = 0; key < SAMPLER_KEYS; ++key) {
        unsigned pin = keyPins[key];
        samplerAdc.positionToKey[pin >> 4][(pin & 15u) - samplerRowTop] = (uint8_t)(key + 1u);
    }
    sampler_touch_init();
}

static FM1_AUDIO_CODE void sampler_adc_drive(unsigned pin, int strong)
{
    const uint32_t mask = 1u << pin;
    if (strong)
        FM1_PR(FM1_PA, FM1_HD0) |= mask;
    else
        FM1_PR(FM1_PA, FM1_HD0) &= ~mask;
    FM1_PR(FM1_PA, FM1_HD) &= ~mask;
    FM1_PR(FM1_PA, FM1_OUT) |= mask;
    FM1_PR(FM1_PA, FM1_DIR) &= ~mask;
    samplerAdc.drivenPin = (uint8_t)pin;
}

static FM1_AUDIO_CODE void sampler_adc_probe(int on)
{
    const uint32_t led = 1u << 10;
    if (on) {
        FM1_PR(FM1_PA, FM1_OUT) &= ~led;
        FM1_PR(FM1_PA, FM1_PU) |= led;
        FM1_PR(FM1_PA, FM1_DIR) |= led;
    } else {
        FM1_PR(FM1_PA, FM1_PU) &= ~led;
        FM1_PR(FM1_PA, FM1_DIR) &= ~led;
    }
}

/* Complete before reading digital contacts or latching the next column.
 * A timeout restores the driven pins as well. */
static FM1_AUDIO_CODE void sampler_adc_finish(const uint8_t* rows)
{
    if (!samplerAdc.pending) return;
    samplerAdc.pending = 0;
    const unsigned drive = samplerAdc.drivenPin;
    if (drive) {
        const uint32_t mask = 1u << drive;
        FM1_PR(FM1_PA, FM1_DIR) |= mask;
        FM1_PR(FM1_PA, FM1_OUT) &= ~mask;
        FM1_PR(FM1_PA, FM1_HD0) &= ~mask;
        samplerAdc.drivenPin = 0;
    }
    if (samplerAdc.input == adcTop) sampler_adc_probe(0);
    if (!fm1_adc_done()) {
        fm1_adc_abort();
        return;
    }

    const unsigned sample = fm1_adc_take();
    switch (samplerAdc.input) {
    case adcKey:
        samplerAdc.lastAnalogSample = SAMPLER_TIMER4_COUNT;
        samplerAdc.haveAnalogSample = 1;
        sense_sample(samplerAdc.key, sample, samplerAdc.lastAnalogSample, drive != 0u);
        break;
    case adcTop:
        sense_probe(samplerAdc.key, sample, (rows[samplerAdc.column] >> samplerRowTop) & 1u);
        break;
    case adcMaster:
        samplerAdc.master = (uint16_t)sample;
        break;
    case adcBattery:
        samplerAdc.battery = (uint16_t)sample;
        break;
    }
}

static FM1_AUDIO_CODE unsigned sampler_adc_key(unsigned column, unsigned row)
{
    /* Zero marks a position without a note key. */
    return samplerAdc.positionToKey[column][row - samplerRowTop];
}

static FM1_AUDIO_CODE int sampler_adc_leds(unsigned column, int leds)
{
    if (samplerAdc.phase == 2u && sampler_adc_key(column, samplerRowTop))
        return leds & ~(1u << samplerRowTop);

    if (samplerAdc.phase < 2u) {
        const unsigned key = sampler_adc_key(column, samplerRowPA7 + samplerAdc.phase);
        if (key && sn_key[key - 1u].st != SN_OPEN) return 0;
    }
    return leds;
}

/* Alternate the two directly wired rows and the PA10 top-row probe. The
 * pot/battery take a slot where there is a panel button, not a note key. */
static FM1_AUDIO_CODE void sampler_adc_start(unsigned column)
{
    const unsigned row = samplerAdc.phase == 2u ? samplerRowTop : samplerRowPA7 + samplerAdc.phase;
    const unsigned key = sampler_adc_key(column, row);
    unsigned channel;
    if (samplerAdc.phase == 1u && column == 0u) {
        channel = FM1_ADC_MASTER;
        samplerAdc.input = adcMaster;
        if (++samplerAdc.batteryClock >= samplerBatteryInterval) {
            samplerAdc.batteryClock = 0;
            channel = FM1_ADC_BATT;
            samplerAdc.input = adcBattery;
        }
    } else if (key && row == samplerRowTop) {
        channel = samplerProbeChannel;
        samplerAdc.input = adcTop;
        samplerAdc.column = (uint8_t)column;
        samplerAdc.key = (uint8_t)(key - 1u);
        sampler_adc_probe(1);
        sampler_adc_drive(6u, 0);
    } else if (key) {
        channel = row == samplerRowPA7 ? FM1_ADC_ROW_PA7 : FM1_ADC_ROW_PA8;
        samplerAdc.input = adcKey;
        samplerAdc.key = (uint8_t)(key - 1u);
        if (sense_want_drive(samplerAdc.key)) sampler_adc_drive(row + 4u, 1);
    } else {
        return;
    }
    fm1_adc_start(channel, FM1_ADC_CON(7, 2));
    samplerAdc.pending = 1;
}

static FM1_AUDIO_CODE void sampler_adc_next_phase(void)
{
    if (++samplerAdc.phase == 3u) samplerAdc.phase = 0;
}
