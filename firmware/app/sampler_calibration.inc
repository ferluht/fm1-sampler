/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * USB MIDI calibration commands. Included by the Sampler control task. */

enum {
    CMD_VELOCITY = 0x0c,
    EV_VELOCITY = 0x43,
    velocityRecordBytes = 5,
    velocityProfileBytes = 1 + SAMPLER_ANALOG_KEYS * velocityRecordBytes,
};

/* Capability and profile replies each have their own pending slot. Profile
 * bytes are encoded as soon as SAVE freezes the accepted landings. */
static uint8_t velocityCapabilitiesPending;
static uint8_t velocitySnapshotPending;
static uint8_t velocitySnapshot[1 + velocityProfileBytes];

static void velocity_encode_snapshot(void)
{
    struct sampler_velocity_profile profile;
    sampler_velocity_capture(&profile);
    velocitySnapshot[0] = EV_VELOCITY;
    uint8_t *p = velocitySnapshot + 1;
    *p++ = profile.curve;
    for (unsigned key = 0; key < SAMPLER_ANALOG_KEYS; ++key) {
        *p++ = (uint8_t)(profile.hard[key] >> 7);
        *p++ = (uint8_t)(profile.hard[key] & 127u);
        *p++ = (uint8_t)(profile.soft[key] >> 7);
        *p++ = (uint8_t)(profile.soft[key] & 127u);
        *p++ = profile.landings[key];
    }
    velocitySnapshotPending = 1;
}

static void velocity_decode_profile(const uint8_t *p)
{
    struct sampler_velocity_profile profile;
    for (unsigned i = 0; i < velocityProfileBytes; ++i) {
        if (p[i] > 127u)
            return;
    }
    profile.curve = *p++;
    for (unsigned key = 0; key < SAMPLER_ANALOG_KEYS; ++key) {
        profile.hard[key] = (uint16_t)n14(p);
        profile.soft[key] = (uint16_t)n14(p + 2);
        profile.landings[key] = p[4];
        p += velocityRecordBytes;
    }
    (void)sampler_velocity_apply(&profile);
}

static void velocity_command(const uint8_t *p, unsigned len)
{
    if (!len)
        return;
    switch (p[0]) {
    case 0:
        if (len == 3u && p[1] <= SAMPLER_CURVE_MAX && p[2] <= 1u)
            sampler_velocity_preview(p[1], p[2]);
        break;
    case 1:
        if (len == 1u)
            velocity_encode_snapshot();
        break;
    case 2:
        if (len == 1u)
            sampler_velocity_reset();
        break;
    case 3:
        if (len == 1u + velocityProfileBytes)
            velocity_decode_profile(p + 1);
        break;
    case 4:
        if (len == 1u)
            velocityCapabilitiesPending = 1;
        break;
    }
}

static void velocity_send_pending(void)
{
    if (velocityCapabilitiesPending) {
        const uint8_t ready[] = {EV_VELOCITY, 0};
        if (sysex_reply(ready, sizeof ready) != 0)
            return;
        velocityCapabilitiesPending = 0;
    }
    if (velocitySnapshotPending && sysex_reply(velocitySnapshot, sizeof velocitySnapshot) == 0)
        velocitySnapshotPending = 0;
}
