
// Add a Perlin noise soundbar. This looks really cool.
void besin(Strip *s) {
    s->leds[s->count / 2] = ColorFromPalette(s->currentPalette, millis(), sampleavg, NOBLEND);
    s->leds[s->count / 2 - 1] = ColorFromPalette(s->currentPalette, millis(), sampleavg, NOBLEND);

    // Move the pixels to the left/right, but not too fast.
    waveit(s);

    fadeToBlackBy(s->leds, s->count, 2);
}
