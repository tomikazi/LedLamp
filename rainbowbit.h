
void rainbowbit(Strip *s) {
    // Starting hue.
    uint8_t beatA = beatsin8(17, 0, 255);

    // Trigger a rainbow with a peak.
    if (samplepeak == 1) {
        // Use FastLED's fill_rainbow routine.
        fill_rainbow(s->leds + random8(0,LED_COUNT/2), random8(0,LED_COUNT/2), beatA, 8);
    }

    // Fade everything. By Andrew Tuline.
    fadeToBlackBy(s->leds, LED_COUNT, 40);

    // Add glitter based on sampleavg.
    addGlitter(s, sampleavg);
}