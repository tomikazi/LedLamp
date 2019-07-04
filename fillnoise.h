
void fillnoise(Strip *s) {

#define xscale 160
#define yscale 160

    // A random number for our noise generator.
    static int16_t xdist;
    static int16_t ydist;

    // Clip the sampleavg to maximize at LED_COUNT.
    if (sampleavg > LED_COUNT) {
        sampleavg = LED_COUNT;
    }

    // The louder the sound, the wider the soundbar.
    for (int i = (LED_COUNT - sampleavg / 2) / 2; i < (LED_COUNT + sampleavg / 2) / 2; i++) {
        // Get a value from the noise function. I'm using both x and y axis.
        uint8_t index = inoise8(i * sampleavg + xdist, ydist + i * sampleavg);
        // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        // Effect is a NOISE bar the width of sampleavg. Very fun. By Andrew Tuline.
        s->leds[i] = ColorFromPalette(s->currentPalette, index, sampleavg, LINEARBLEND);
    }

    // Moving forward in the NOISE field, but with a sine motion.
    xdist = xdist + beatsin8(5, 0, 3);
    // Moving sideways in the NOISE field, but with a sine motion.
    ydist = ydist + beatsin8(4, 0, 3);

    // Add glitter based on sample and not peaks.
    addGlitter(s, sampleavg / 2);

    // Move the pixels to the left/right, but not too fast.
    waveit(s);

    // Fade the center, while waveit moves everything out to the edges.
    fadeToBlackBy(s->leds + LED_COUNT / 2 - 1, 2, 128);
}
