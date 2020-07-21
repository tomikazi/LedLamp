
// Make a complex rainbow with glitter based on sampleavg.
void rainbowg(Strip *s) {

    uint8_t beatA = beatsin8(17, 0, 255);
    uint8_t beatB = beatsin8(13, 0, 255);
    uint8_t beatC = beatsin8(11, 0, 255);

    //  currentPalette = PartyColors_p;

    for (int i = 0; i < s->count; i++) {
        int colorIndex = (beatA + beatB + beatC) / 3 * i * 4 / s->count;
        // Variable brightness
        s->leds[i] = ColorFromPalette(s->currentPalette, colorIndex, sampleavg, s->currentBlending);
    }

    addGlitter(s, sampleavg);
}

