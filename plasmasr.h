
void plasmasr(Strip *s) {

    // Phase of a cubicwave8.
    static int16_t thisphase = 0;
    // Phase of the cos8.
    static int16_t thatphase = 0;

    uint16_t thisbright;
    uint16_t colorIndex;

    // You can change direction and speed individually.
    thisphase += beatsin8(6, -4, 4);
    // Two phase values to make a complex pattern. By Andrew Tuline.
    thatphase += beatsin8(7, -4, 4);

    // For each of the LED's in the strand, set a brightness based on a wave as follows.
    for (int k = 0; k < LED_COUNT; k++) {
        thisbright = cubicwave8((k * 8) + thisphase) / 2;
        // Let's munge the brightness a bit and animate it all with the phases.
        thisbright += cos8((k * 10) + thatphase) / 2;
        colorIndex = thisbright;
        // qsuba chops off values below a threshold defined by sampleavg. Gives a cool effect.
        thisbright = qsuba(thisbright, 255 - sampleavg);

        // Let's now add the foreground colour.
        s->leds[k] = ColorFromPalette(s->currentPalette, colorIndex, thisbright, s->currentBlending);
    }

    // Add glitter based on sampleavg.
    addGlitter(s, sampleavg / 2);

}
