

void onesine(Strip *s) {
    // Phase change value gets calculated.
    static int thisphase = 0;

    uint8_t allfreq = 32;
    // You can change the cutoff value to display this wave. Lower value = longer wave.
    uint8_t thiscutoff = 192;

    // Brightness of background colour.
    // uint8_t bgbright = 10;
    uint8_t colorIndex;

    thiscutoff = 255 - sampleavg;

    // Move the sine waves along as a function of sound.
    thisphase += sampleavg / 2;

    thisphase = beatsin16(20, -600, 600);

    colorIndex = millis() >> 4;

    for (int k = 0; k < s->count; k++) {
        // For each of the LED's in the strand, set a brightness based on a wave as follows:
        // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
        int thisbright = qsuba(cubicwave8((k * allfreq) + thisphase), thiscutoff);
        s->leds[k] = ColorFromPalette(s->currentPalette, colorIndex, thisbright, s->currentBlending);
        colorIndex += 3;
    }

    addGlitter(s, sampleavg / 2);
}
