#include <ESPGizmoDefault.h>
#include <WiFiUDP.h>
#include <FS.h>

#include <FastLED.h>

#include <WebSocketsServer.h>

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2020.03.02.007"

#define STATE      "/cfg/state"

#define FRONT_PIN       4
#define BACK_PIN        5

#define MIC_PIN         A0

#define LED_COUNT               60
#define LED_TYPE                WS2812B
#define COLOR_ORDER             GRB
#define BRIGHTNESS              96
#define FRAMES_PER_SECOND       60

#define GROUP_PORT  7001
#define BUDDY_PORT  7002

WiFiUDP udp;
WiFiUDP buddy;

IPAddress buddyIp(224,0,42,69);

CRGB frontLeds[LED_COUNT];
CRGB backLeds[LED_COUNT];

typedef struct StripRec Strip;

// Pattern renderer function type.
typedef void (*Renderer)(Strip *);

// Pattern record
typedef struct Pattern {
    const char *name;
    Renderer renderer;
    uint32_t huePause;
    uint32_t renderPause;
    boolean  soundReactive;
} Pattern;

typedef enum {
    NOT_RANDOM,
    ALL,
    SOUND_REACTIVE,
    NOT_SOUND_REACTIVE
} RandomMode;

struct StripRec {
    const char *name;
    bool on;
    CRGB color;
    uint8_t brightness;
    CRGB *leds;
    Pattern *pattern;
    uint8_t hue;
    CLEDController *ctl;
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;
    TBlendType currentBlending;
    RandomMode randomMode;
    byte *data;
};

byte frontData[LED_COUNT];
byte backData[LED_COUNT];

Strip front = {
        .name = "front", .on = true, .color = CRGB::Red, .brightness = BRIGHTNESS,
        .leds = &frontLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND, .randomMode = NOT_RANDOM, .data = frontData
};
Strip back = {
        .name = "back", .on = true, .color = CRGB::Green, .brightness = BRIGHTNESS,
        .leds = &backLeds[0], .pattern = NULL, .hue = 0, .ctl = NULL,
        .currentPalette = CRGBPalette16(PartyColors_p), .targetPalette = CRGBPalette16(PartyColors_p),
        .currentBlending = LINEARBLEND, .randomMode = NOT_RANDOM, .data = backData
};

static WebSocketsServer wsServer(81);

typedef struct {
    uint16_t sampleavg;
    uint16_t samplepeak;
    uint16_t oldsample;
} MicSample;

#define SAMPLE  100

typedef struct {
    uint16_t op;
    uint16_t param;
    uint32_t src;
} Command;

// Sample average, max and peak detection
uint16_t sampleavg = 0;
uint16_t samplepeak = 0;
uint16_t oldsample = 0;

#define SAMPLE_TIMEOUT  1000
uint32_t lastSample = 0;

#define OFFLINE_TIMEOUT     15000
uint32_t offlineTime;

void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, "gizmo123");
    gizmo.setUpdateURL(SW_UPDATE_URL);

    gizmo.setCallback(mqttCallback);
    gizmo.addTopic("%s/all");
    gizmo.addTopic("%s/all/rgb");
    gizmo.addTopic("%s/all/brightness");
    gizmo.addTopic("%s/all/effect");

    gizmo.addTopic("%s/front");
    gizmo.addTopic("%s/front/rgb");
    gizmo.addTopic("%s/front/brightness");
    gizmo.addTopic("%s/front/effect");

    gizmo.addTopic("%s/back");
    gizmo.addTopic("%s/back/rgb");
    gizmo.addTopic("%s/back/brightness");
    gizmo.addTopic("%s/back/effect");

    gizmo.httpServer()->on("/", handleRoot);
    gizmo.httpServer()->on("/cmd", handleCommand);
    gizmo.httpServer()->serveStatic("/", SPIFFS, "/", "max-age=86400");

    setupWebSocket();

    setupLED();
    gizmo.endSetup();

    offlineTime = strlen(gizmo.getSSID()) ? millis() + OFFLINE_TIMEOUT : millis();
}

void setupLED() {
    front.ctl = &FastLED.addLeds<LED_TYPE, FRONT_PIN, COLOR_ORDER>(frontLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(frontLeds, LED_COUNT, 255);
    front.ctl->showLeds(front.brightness);
    front.pattern = findPattern("glitter");

    back.ctl = &FastLED.addLeds<LED_TYPE, BACK_PIN, COLOR_ORDER>(backLeds, LED_COUNT).setCorrection(TypicalLEDStrip);
    fadeToBlackBy(backLeds, LED_COUNT, 255);
    back.ctl->showLeds(back.brightness);
    back.pattern = findPattern("glitter");
}

void setupWebSocket() {
    wsServer.begin();
    wsServer.onEvent(webSocketEvent);
    Serial.println("WebSocket server setup.");
}

// Command processors
void processOnOff(char *value, Strip *strip) {
    strip->on = !strcmp(value, "on");
}

CRGB colorFromCSV(char *csv) {
    char tmp[32];
    strncpy(tmp, csv, 32);
    uint8_t r, g, b;
    char *p = strtok(tmp, ",");
    r = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    g = p ? atoi(p) : 0;
    p = strtok(NULL, ",");
    b = p ? atoi(p) : 0;
    return CRGB(r, g, b);
}

void processColor(char *value, Strip *strip) {
    strip->on = true;
    strip->color = colorFromCSV(value);
    strip->pattern = findPattern("solid");
}

void processBrightness(char *value, Strip *strip) {
    strip->brightness = atoi(value);
    strip->on = strip->brightness != 0;
}

RandomMode randomMode(const char *name) {
    return !strcmp(name, "random") ? ALL :
                !strcmp(name, "soundReactive") ? SOUND_REACTIVE :
                    !strcmp(name, "notSoundReactive") ? NOT_SOUND_REACTIVE : NOT_RANDOM;
}

void processEffect(char *value, Strip *strip) {
    strip->on = true;
    strip->randomMode = randomMode(value);
    if (strip->randomMode == NOT_RANDOM) {
        strip->pattern = findPattern(value);
    } else {
        strip->pattern = randomPattern(strip);
    }
}

void processCallback(char *topic, char *value, Strip *strip) {
    if (strstr(topic, "/rgb")) {
        processColor(value, strip);
    } else if (strstr(topic, "/brightness")) {
        processBrightness(value, strip);
    } else if (strstr(topic, "/effect")) {
        processEffect(value, strip);
    } else {
        processOnOff(value, strip);
    }
    messageBuddy();
    saveState();
}


// MQTT Callback
void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = '\0';
    strncat(value, (char *) payload, length);

    Serial.printf("%s: %s\n", topic, value);

    if (strstr(topic, "/all")) {
        processCallback(topic, value, &front);
        processCallback(topic, value, &back);
    } else if (strstr(topic, "/front")) {
        processCallback(topic, value, &front);
    } else if (strstr(topic, "/back")) {
        processCallback(topic, value, &back);
    } else {
        gizmo.handleMQTTMessage(topic, value);
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected.\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Connected.\n", num);
            break;
        case WStype_TEXT:
            char cmd[128];
            cmd[0] = '\0';
            strncat(cmd, (char *) payload, length);
            handleWsCommand(cmd);
            break;

        default:
            Serial.printf("Unhandled message type\n");
            break;
    }
}

void handleWsCommand(char *cmd) {
    char *t = strtok(cmd, "&");
    char *m = strtok(NULL, "&");

    if (strcmp(t, "get")) {
        mqttCallback(t, (uint8_t *) m, strlen(m));
    }
    broadcastState();
}

void broadcastState() {
    char state[512], f[128], b[128];
    state[0] = '\0';
    snprintf(state, 511, "{%s,%s,\"version\":\"" SW_VERSION "\"}", stripStatus(f, &front), stripStatus(b, &back));
    wsServer.broadcastTXT(state);
}

void messageBuddy() {
    Command command;
    command.src = (uint32_t) WiFi.localIP();
    command.op = SAMPLE;
    command.param = (back.on && back.pattern && back.pattern->soundReactive) ||
                        (front.on && front.pattern && front.pattern->soundReactive);
    buddy.beginPacketMulticast(buddyIp, BUDDY_PORT, WiFi.localIP());
    buddy.write((char *) &command, sizeof(command));
    buddy.endPacket();
    buddy.flush();
}

void handleLEDs(Strip *strip) {
    if (strip->on && strip->pattern) {
        EVERY_N_MILLISECONDS(20) {
            uint8_t maxChanges = 24;
            nblendPaletteTowardPalette(strip->currentPalette, strip->targetPalette, maxChanges);
            // TODO: potential broadcast point for a synchronized setup
        }

        // Sets initial timing only. Changes here don't do anything
        EVERY_N_MILLIS_I(hueTimer, 20) {
            strip->hue++; // slowly cycle the "base color" through the rainbow
            hueTimer.setPeriod(strip->pattern->huePause);
            // TODO: potential broadcast point for a synchronized setup
        }

        EVERY_N_MILLIS_I(renderTimer, 20) {
            renderTimer.setPeriod(strip->pattern->renderPause);
            strip->pattern->renderer(strip);
            strip->ctl->showLeds(strip->brightness);
        }

        // Change the target palette to a 'related colours' palette every 5 seconds.
        EVERY_N_SECONDS(5) {
            // This is the base colour. Other colours are within 16 hues of this. One color is 128 + baseclr.
            uint8_t baseclr = random8();
            strip->targetPalette = CRGBPalette16(
                    CHSV(baseclr + random8(64), 255, random8(128,255)),
                    CHSV(baseclr + random8(64), 255, random8(128,255)),
                    CHSV(baseclr + random8(64), 192, random8(128,255)),
                    CHSV(baseclr + random8(64), 255, random8(128,255)));
            // TODO: potential broadcast point for a synchronized setup
        }

        EVERY_N_SECONDS(30) {
            if (strip->randomMode != NOT_RANDOM) {
                strip->pattern = randomPattern(strip);
                messageBuddy();
                // TODO: potential broadcast point for a synchronized setup
            }
        }

    } else {
        fill_solid(strip->leds, LED_COUNT, strip->color);
        strip->ctl->showLeds(strip->on ? strip->brightness : 0);
    }
}

void handleRemoteMicData() {
    EVERY_N_SECONDS(3) {
        messageBuddy();
    }

    if (lastSample && lastSample < millis()) {
        lastSample = 0;
        samplepeak = 0;
        sampleavg = 0;
        oldsample = 0;
    }

    MicSample sample;
    while (udp.parsePacket()) {
        udp.read((char *) &sample, sizeof(sample));
        samplepeak = sample.samplepeak;
        sampleavg = sample.sampleavg;
        oldsample = sample.oldsample;
        lastSample = millis() + SAMPLE_TIMEOUT;
//        Serial.printf("-50, 100, %d, %d\n", sampleavg, samplepeak * -20);
    }
}

void loop() {
    if (gizmo.isNetworkAvailable(finishWiFiConnect)) {
        wsServer.loop();
        handleRemoteMicData();
    } else {
        handleNetworkFailover();
    }
    handleLEDs(&front);
    handleLEDs(&back);
}

void finishWiFiConnect() {
    offlineTime = 0;
    loadState();

    udp.begin(GROUP_PORT);
    buddy.beginMulticast(WiFi.localIP(), buddyIp, BUDDY_PORT);

    messageBuddy();
    Serial.printf("%s is ready\n", LED_LIGHTS);
}

void handleNetworkFailover() {
    if (offlineTime && millis() > offlineTime) {
        gizmo.setNoNetworkConfig();
        finishWiFiConnect();
    }
}


void handleRoot() {
    File f = SPIFFS.open("/index.html", "r");
    gizmo.httpServer()->streamFile(f, "text/html");
    f.close();
}

#define STRIP_STATUS "\"%s\": {\"on\": %s,\"rgb\": \"%d,%d,%d\",\"brightness\": %d,\"effect\": \"%s\"}"

char *stripStatus(char *html, Strip *s) {
    snprintf(html, 127, STRIP_STATUS, s->name, s->on ? "true" : "false",
             s->color.red, s->color.green, s->color.blue, s->brightness,
             s->pattern ? s->pattern->name : "solid");
    return html;
}

void handleCommand() {
    char t[64], m[64];
    strncpy(t, gizmo.httpServer()->arg("t").c_str(), 63);
    strncpy(m, gizmo.httpServer()->arg("m").c_str(), 63);
    if (strcmp(t, "get")) {
        mqttCallback(t, (uint8_t *) m, strlen(m));
    }

    static char f[128];
    static char b[128];

    gizmo.httpServer()->setContentLength(CONTENT_LENGTH_UNKNOWN);
    gizmo.httpServer()->send(200, "application/json", "{");
    gizmo.httpServer()->sendContent(stripStatus(f, &front));
    gizmo.httpServer()->sendContent(",");
    gizmo.httpServer()->sendContent(stripStatus(b, &back));
    gizmo.httpServer()->sendContent(",\"version\":\"" SW_VERSION "\"");
    gizmo.httpServer()->sendContent("}");
}

void loadState() {
    File f = SPIFFS.open(STATE, "r");
    if (f) {
        loadStripState(f, &front);
        loadStripState(f, &back);
        f.close();
    } else {
        front.pattern = findPattern("confetti");
        back.pattern = findPattern("cycle");
    }
}

void loadStripState(File f, Strip *s) {
    char field[32];
    int l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    s->on = !strcmp(field, "on");

    l = f.readBytesUntil('|', field, 32);
    field[l] = '\0';
    processColor(field, s);

    l = f.readBytesUntil('|', field, 7);
    field[l] = '\0';
    s->brightness = atoi(field);

    l = f.readBytesUntil('\n', field, 32);
    field[l] = '\0';
    s->pattern = NULL;
    if (strcmp(field, "none")) {
        processEffect(field, s);
    }
}

const char *effect(Strip *s) {
    return s->randomMode == ALL ? "random" :
            s->randomMode == SOUND_REACTIVE ? "soundReactive" :
                s->randomMode == NOT_SOUND_REACTIVE ? "notSoundReactive" :
                    s->pattern ? s->pattern->name : "solid";
}

void saveState() {
    File f = SPIFFS.open(STATE, "w");
    if (f) {
        f.printf("%s|%d,%d,%d|%d|%s\n", front.on ? "on" : "off",
                 front.color.red, front.color.green, front.color.blue, front.brightness, effect(&front));
        f.printf("%s|%d,%d,%d|%d|%s\n", back.on ? "on" : "off",
                 back.color.red, back.color.green, back.color.blue, back.brightness, effect(&back));
        f.close();
    }
}

// LED Patterns
#include "simple.h"
#include "fire.h"
#include "noise.h"
#include "plasma.h"
#include "blendwave.h"
#include "dotBeat.h"
#include "pixels.h"
#include "ripple.h"
#include "matrix.h"
#include "pixel.h"
#include "onesine.h"
#include "noisefire.h"
#include "rainbowg.h"
#include "noisepal.h"
#include "besin.h"
#include "fillnoise.h"
#include "plasmasr.h"

// Setup a catalog of the different patterns.
Pattern patterns[] = {
        Pattern{.name = "glitter", .renderer = glitter, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "confetti", .renderer = confetti, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "cycle", .renderer = cycle, .huePause = 200, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "rainbow", .renderer = rainbow, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "rainbowWithGlitter", .renderer = rainbowWithGlitter, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "sinelon", .renderer = sinelon, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "juggle", .renderer = juggle, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "bpm", .renderer = bpm, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "fire", .renderer = fire, .huePause = 20, .renderPause = 10, .soundReactive = false },
        Pattern{.name = "noise", .renderer = noise, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "blendwave", .renderer = blendwave, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "dotbeat", .renderer = dotBeat, .huePause = 20, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "plasma", .renderer = plasma, .huePause = 20, .renderPause = 20, .soundReactive = false },

        Pattern{.name = "pixel", .renderer = pixel, .huePause = 2000, .renderPause = 0, .soundReactive = true },
        Pattern{.name = "pixels", .renderer = pixels, .huePause = 2000, .renderPause = 30, .soundReactive = true },
        Pattern{.name = "ripple", .renderer = ripple, .huePause = 2000, .renderPause = 20, .soundReactive = true },
        Pattern{.name = "matrix", .renderer = matrix, .huePause = 2000, .renderPause = 40, .soundReactive = true },
        Pattern{.name = "onesine", .renderer = onesine, .huePause = 2000, .renderPause = 30, .soundReactive = true },
        Pattern{.name = "noisefire", .renderer = noisefire, .huePause = 2000, .renderPause = 0, .soundReactive = true },
        Pattern{.name = "noisepal", .renderer = noisepal, .huePause = 2000, .renderPause = 0, .soundReactive = true },
        Pattern{.name = "rainbowg", .renderer = rainbowg, .huePause = 2000, .renderPause = 10, .soundReactive = true },
        Pattern{.name = "besin", .renderer = besin, .huePause = 2000, .renderPause = 20, .soundReactive = true },
        Pattern{.name = "fillnoise", .renderer = fillnoise, .huePause = 2000, .renderPause = 20, .soundReactive = true },
        Pattern{.name = "plasmasr", .renderer = plasmasr, .huePause = 2000, .renderPause = 10, .soundReactive = true },

        Pattern{.name = "solid", .renderer = solid, .huePause = 2000, .renderPause = 20, .soundReactive = false },
        Pattern{.name = "test", .renderer = test, .huePause = 20, .renderPause = 20, .soundReactive = false }
};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

Pattern *findPattern(const char *name) {
    int i = 0;
    while (strcmp(patterns[i].name, name) && strcmp(patterns[i].name, "test")) {
        i++;
    }
    return &patterns[i];
}

Pattern *randomPattern(Strip *s) {
    Pattern *old = s->pattern;
    Pattern *p;
    do {
        p = &patterns[random8(ARRAY_SIZE(patterns) - 1)];
    } while (p == old ||
            (s->randomMode == SOUND_REACTIVE && !p->soundReactive) ||
            (s->randomMode == NOT_SOUND_REACTIVE && p->soundReactive));
    return p;
}
