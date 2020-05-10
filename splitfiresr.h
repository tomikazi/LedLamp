
//static uint16_t cooling = 0;
//static uint16_t sparking = 0;

void splitfiresr(Strip *s) {
    uint8_t half = LED_COUNT/2;

    EVERY_X_MILLIS(s->t2, 10)
        sparking = map(sampleavg, 0, 255, 0, 100);
        cooling = map(255 - sampleavg, 0, 255, 20, 200);

        if (samplepeak) {
            sparking = sparking * 1.6;
            cooling = cooling * 2.7;
        }

        // Step 1.  Cool down every cell a little
        for (int i = 0; i < half; i++) {
            s->data[i] = qsub8(s->data[i], random8(0, ((cooling * 10) / LED_COUNT) + 2));
        }

        // Step 2.  Heat from each cell drifts 'up' and diffuses a little
        for (int k = half - 1; k >= 2; k--) {
            s->data[k] = (s->data[k - 1] + s->data[k - 2] + s->data[k - 2]) / 3;
        }

        // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
        if (random8() < sparking) {
            int y = random8(7);
            s->data[y] = qadd8(s->data[y], random8(160, 255));
        }

        // Step 4.  Map from heat cells to LED colors
        for (int j = 0; j < half; j++) {
            s->leds[half-j-1] = HeatColor(s->data[j]);
            s->leds[half+j] = HeatColor(s->data[j]);
        }
    }
}
