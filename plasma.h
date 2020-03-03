
// Use qsuba for smooth pixel colouring and qsubd for non-smooth pixel colouring
#define qsubd(x, b)  ((x>b)?b:0)                              // Digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.
#define qsuba(x, b)  ((x>b)?x-b:0)                            // Analog Unsigned subtraction macro. if result <0, then => 0

void plasma(Strip *s) {
    EVERY_X_MILLIS(s->t2, 50)
        doPlasma(s);
    }

    EVERY_X_MILLIS(s->t3, 100)
        uint8_t maxChanges = 24;
        nblendPaletteTowardPalette(s->currentPalette, s->targetPalette, maxChanges);   // AWESOME palette blending capability.
    }

    // Change the target palette to a random one every 5 seconds.
    EVERY_X_MILLIS(s->t4, 5000)
        // You can use this as a baseline colour if you want similar hues in the next line.
        uint8_t baseC = random8();
        s->targetPalette = CRGBPalette16(CHSV(baseC+random8(32), 192, random8(128,255)),
                                         CHSV(baseC+random8(32), 255, random8(128,255)),
                                         CHSV(baseC+random8(32), 192, random8(128,255)),
                                         CHSV(baseC+random8(32), 255, random8(128,255)));
    }
}

void doPlasma(Strip *s) {
    // Setting phase change for a couple of waves.
    int thisPhase = beatsin8(6, -64, 64);
    int thatPhase = beatsin8(7, -64, 64);

    // For each of the LED's in the strand, set a brightness based on a wave as follows:
    for (int k = 0; k < LED_COUNT; k++) {
        // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish.
        int colorIndex = cubicwave8((k * 23) + thisPhase) / 2 + cos8((k * 15) + thatPhase) / 2;
        // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..
        int thisBright = qsuba(colorIndex, beatsin8(7, 0, 96));

        // Let's now add the foreground colour.
        s->leds[k] = ColorFromPalette(s->currentPalette, colorIndex, thisBright, LINEARBLEND);
    }
}
