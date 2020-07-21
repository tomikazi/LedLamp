
// Display ripples triggered by peaks.
void ripple(Strip *s) {

// Local definitions
#define maxsteps 16                                                           // Maximum number of steps.

    // Persistent local variables.
    static uint8_t colour;                                                        // Ripple colour is based on samples.
    static uint16_t center = 0;                                                   // Center of current ripple.
    static int8_t step = -1;                                                      // Phase of the ripple as it spreads out.

    // Trigger a new ripple if we have a peak.
    if (samplepeak == 1) {
        step = -1;
    }

    fadeToBlackBy(s->leds, s->count, 64);

    switch (step) {
        case -1:
            center = random(s->count);
            colour = (oldsample) % 255; // More peaks/s = higher the hue colour.
            step = 0;
            break;

        case 0:
            // Display the first pixel of the ripple.
            s->leds[center] += ColorFromPalette(s->currentPalette, colour, 255, s->currentBlending);
            step++;
            break;

        case maxsteps:                                                              // At the end of the ripples.
            break;

        default:                                                                    // Middle of the ripples.
            // A spreading and fading pattern up the strand.
            s->leds[(center + step + s->count) % s->count] +=
                    ColorFromPalette(s->currentPalette, colour, 255 / step * 2, s->currentBlending);
            // A spreading and fading pattern down the strand.
            s->leds[(center - step + s->count) % s->count] +=
                    ColorFromPalette(s->currentPalette, colour, 255 / step * 2, s->currentBlending);
            step++;
            break;

    } // switch step

    addGlitter(s, sampleavg);                                                        // Add glitter baesd on sampleavg.
}
