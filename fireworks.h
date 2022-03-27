

#define NUM_LAUNCH_SPARKS   5
#define NUM_SPARKS LED_COUNT/2

static int nSparks;
static float sparkPos[NUM_SPARKS];
static float sparkVel[NUM_SPARKS];
static float sparkCol[NUM_SPARKS];

static float flarePos;
static float brightness;
static float flareVel;

static float gravity = -.002; // m/s/s
static float dying_gravity;

#define WAIT_STAGE      0
#define LAUNCH_STAGE    1
#define FLARE_STAGE     2
#define EXPLODE_STAGE   3
#define FADE_STAGE      4
static uint8_t stage = WAIT_STAGE;

void fireworksWait(Strip *s) {
    fill_solid(s->leds, s->count, CRGB::Black);

    EVERY_X_MILLIS(s->t2, random16(500, 5000))
        stage = LAUNCH_STAGE;
    }
}

void fireworksLaunch(Strip *s) {
    flarePos = 0;
    flareVel = float(random16(35, 45)) / 100; // trial and error to get reasonable range
    brightness = 1;

    // initialize launch sparks
    for (int i = 0; i < NUM_LAUNCH_SPARKS; i++) {
        sparkPos[i] = 0;
        sparkVel[i] = (float(random8()) / 255) * (flareVel / 5); // random around 20% of flare velocity
        sparkCol[i] = sparkVel[i] * 2000;
        sparkCol[i] = constrain(sparkCol[i], 32, 255);
    } // launch

    fill_solid(s->leds, s->count, CRGB::Black);

    stage = FLARE_STAGE;
}

void fireworksFlare(Strip *s) {
    if (flareVel < -0.1) {
        stage = EXPLODE_STAGE;
        return;
    }

    fill_solid(s->leds, s->count, CRGB::Black);

    // sparks
    for (int i = 0; i < NUM_LAUNCH_SPARKS; i++) {
        sparkPos[i] += sparkVel[i];
        sparkPos[i] = constrain(sparkPos[i], 0, s->count);
        sparkVel[i] += gravity;
        sparkCol[i] += -.98;
        sparkCol[i] = constrain(sparkCol[i], 32, 255);
        s->leds[int(sparkPos[i])] = HeatColor(sparkCol[i]);
        s->leds[int(sparkPos[i])] %= 50; // reduce brightness to 50/255
    }

    // flare
    s->leds[int(flarePos)] = CHSV(0, 0, int(brightness * 255));

    flarePos += flareVel;
    flarePos = constrain(flarePos, 0, s->count);
    flareVel += gravity;
    brightness *= .985;
}

void fireworksExplode(Strip *s) {
    nSparks = flarePos / 3; // works out to look about right

    // initialize sparks
    for (int i = 0; i < nSparks; i++) {
        sparkPos[i] = flarePos;
        sparkVel[i] = (float(random16(0, 20000)) / 10000.0) - 1.0;
        sparkCol[i] = abs(sparkVel[i]) * 300; // set colors before scaling velocity to keep them bright
        sparkCol[i] = constrain(sparkCol[i], 128, 255);
        sparkVel[i] *= (flarePos / 8) / s->count; // proportional to height
    }

    sparkCol[0] = 255; // this will be our known spark
    dying_gravity = gravity;

    fill_solid(s->leds, s->count, CRGB::Black);

    stage = FADE_STAGE;
}

void fireworksFade(Strip *s) {
    float c1 = 96;
    float c2 = 48;

    // as long as our known spark is lit, work with all the sparks
    if (sparkCol[0] <= c2 / 128) {
        stage = WAIT_STAGE;
        return;
    }

    fill_solid(s->leds, s->count, CRGB::Black);

    for (int i = 0; i < nSparks; i++) {
        sparkPos[i] += sparkVel[i];
        sparkPos[i] = constrain(sparkPos[i], 0, s->count);
        sparkVel[i] += dying_gravity;
        sparkCol[i] *= .99;
        sparkCol[i] = constrain(sparkCol[i], 0, 255); // red cross dissolve

        CRGB c;
        if (sparkCol[i] > c1) { // fade white to yellow
            c = CRGB(255, 255, (255 * (sparkCol[i] - c1)) / (255 - c1));
        } else if (sparkCol[i] < c2) { // fade from red to black
            c = CRGB((255 * sparkCol[i]) / c2, 0, 0);
        } else { // fade from yellow to red
            c = CRGB(255, (255 * (sparkCol[i] - c2)) / (c1 - c2), 0);
        }

        s->leds[int(sparkPos[i])] = c;
    }
    dying_gravity *= .70; // as sparks burn out they fall slower

    s->leds[0] = CRGB::Black;
}

void fireworks(Strip *s) {
    switch (stage) {
        case LAUNCH_STAGE:
            fireworksLaunch(s);
            break;
            case FLARE_STAGE:
                fireworksFlare(s);
                break;
                case EXPLODE_STAGE:
                    fireworksExplode(s);
                    break;
                    case FADE_STAGE:
                        fireworksFade(s);
                        break;
                        default:
                            fireworksWait(s);
                            break;
    }
}
