
void pixels(Strip *s) {
    uint16_t currLED = beatsin8(16, 0, 10);

    for (int i = 0; i < s->count; i++) {
        // Colour of the LED will be based on oldsample, while brightness is based on sampleavg.
        CRGB c = ColorFromPalette(s->currentPalette, oldsample + i * 8, sampleavg, s->currentBlending);

        // Blend the old value and the new value for a gradual transitioning.
        nblend(s->leds[(i + currLED) % s->count], c, 192);
    }
}
