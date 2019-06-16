#include <ESPGizmoDefault.h>
#include <NeoPixelFader.h>
#include <FS.h>

#include "LedLampHTML.h"

#define LED_LIGHTS      "LedLamp"
#define SW_UPDATE_URL   "http://iot.vachuska.com/LedLamp.ino.bin"
#define SW_VERSION      "2019.06.15.001"

#define LED_DATA   "cfg/led"

#define TRANSITION_STEPS    64
#define TRANSITION_DELAY     2

static Adafruit_NeoPixel frontBase = Adafruit_NeoPixel();
static NeoPixelFader frontFader = NeoPixelFader();

static Adafruit_NeoPixel backBase = Adafruit_NeoPixel();
static NeoPixelFader backFader = NeoPixelFader();

typedef struct {
    char *name;
    Adafruit_NeoPixel *base;
    NeoPixelFader *fader;
    int ledPin, ledOffset, ledCount;
    boolean changed;
    uint32_t color;
    uint8_t brightness;
    boolean on;
    boolean testOn;
} Strip;

static Strip frontStrip = { .name = "front", .base = &frontBase, .fader = &frontFader };
static Strip backStrip = { .name = "back", .base = &backBase, .fader = &backFader };

void setup() {
    gizmo.beginSetup(LED_LIGHTS, SW_VERSION, PASSKEY);
    gizmo.setUpdateURL(SW_UPDATE_URL);

    gizmo.setCallback(mqttCallback);
    gizmo.addTopic("%s/front");
    gizmo.addTopic("%s/front/rgb");
    gizmo.addTopic("%s/front/brightness");
    gizmo.addTopic("%s/front/test");

    gizmo.addTopic("%s/back");
    gizmo.addTopic("%s/back/rgb");
    gizmo.addTopic("%s/back/brightness");
    gizmo.addTopic("%s/back/test");

    gizmo.httpServer()->on("/", handleRoot);
    gizmo.httpServer()->on("/led", handleLEDPage);
    gizmo.httpServer()->on("/ledcfg", handleLEDConfig);

    setupLED();
    gizmo.endSetup();
}

void setupLED() {
    loadLEDConfig(LED_DATA "front", &frontStrip);
    loadLEDConfig(LED_DATA "back", &backStrip);
    setupBase();
    setupStrip(&frontStrip);
    setupStrip(&backStrip);

    Serial.printf("front: %d %d %d\n", frontStrip.ledPin, frontStrip.ledOffset, frontStrip.ledCount);
    Serial.printf("back: %d %d %d\n", backStrip.ledPin, backStrip.ledOffset, backStrip.ledCount);
}

void setupBase() {
    if (frontStrip.ledPin) {
        frontBase.setPin(frontStrip.ledPin);
        frontBase.updateType(NEO_GRB + NEO_KHZ800);
        frontBase.updateLength(frontStrip.ledCount);
    }
    if (backStrip.ledPin) {
        backBase.setPin(backStrip.ledPin);
        backBase.updateType(NEO_GRB + NEO_KHZ800);
        backBase.updateLength(backStrip.ledCount);
    }

    if (frontStrip.ledPin && !backStrip.ledPin) {
        backStrip.base = &frontBase;
        frontBase.updateLength(frontStrip.ledCount + backStrip.ledCount);
    }
}

void setupStrip(Strip *strip) {
    if (strip->ledCount) {
        strip->fader->map(*strip->base, strip->ledOffset, strip->ledOffset + strip->ledCount - 1);
        strip->fader->begin();
        strip->fader->clear();
        strip->changed = true;
        strip->color = strip->fader->ColorFromCSV("255,255,255", false);
        strip->brightness = 16;
        strip->on = true;
    }
}

// TODO: use ESPGizmo to do this
void publishState(const char *topic, const char *value, Strip *strip) {
    char stateTopic[64];
    snprintf(stateTopic, 64, "%s/%s%s", gizmo.getTopicPrefix(), strip->name, topic);
    gizmo.publish(stateTopic, (char *) value);
}

void processOnOff(char *value, Strip *strip) {
    boolean wasOn = strip->on;
    strip->on = !strcmp(value, "on");
    if (strip->on && strip->brightness == 0) {
        strip->brightness = 64;
    }
    strip->changed = wasOn != strip->on;
}

void processColor(char *value, Strip *strip) {
    uint32_t oldColor = strip->color;
    strip->color = strip->fader->ColorFromCSV(value, false);
    strip->changed = oldColor != strip->color;
}

void processBrightness(char *value, Strip *strip) {
    uint8_t oldBrightness = strip->brightness;
    strip->brightness = atoi(value);
    strip->on = strip->brightness != 0;
    strip->changed = oldBrightness != strip->brightness;
}

void processTest(char *value, Strip *strip) {
    boolean wasTestOn = strip->testOn;
    strip->testOn = !strcmp(value, "on");
    strip->changed = wasTestOn != strip->testOn;
}

void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = NULL;
    strncat(value, (char *) payload, length);

    Serial.printf("%s: %s\n", topic, value);

    if (strstr(topic, "/front")) {
        processCallback(topic, value, &frontStrip);
    } else if (strstr(topic, "/back")) {
        processCallback(topic, value, &backStrip);
    } else {
        gizmo.handleMQTTMessage(topic, value);
    }
}

void processCallback(char *topic, char *value, Strip *strip) {
    if (strstr(topic, "/rgb")) {
        processColor(value, strip);
    } else if (strstr(topic, "/brightness")) {
        processBrightness(value, strip);
    } else if (strstr(topic, "/test")) {
        processTest(value, strip);
    } else {
        processOnOff(value, strip);
    }
}

uint32_t color(uint8_t r, uint8_t g, uint8_t b) {
    return frontStrip.fader->Color(r, g, b);
}

void handleLEDs(Strip *strip) {
    if (strip->changed) {
        if (strip->testOn) {
            // Test pattern
            strip->fader->fill(color(32, 32, 32));
            strip->fader->setPixelColor(0, color(0, 255, 0));
            strip->fader->setPixelColor(strip->ledCount - 1, color(255, 0, 0));
            strip->fader->setBrightness(255);
            publishState("", "on", strip);

        } else if (strip->on) {
            strip->fader->setBrightness(strip->brightness);
            strip->fader->fill(strip->color);

            char num[16];
            publishState("/state", "on", strip);
            publishState("/rgb/state", strip->fader->ColorToCSV(strip->color, num), strip);

            snprintf(num, 4, "%d", strip->brightness);
            publishState("/brightness/state", num, strip);
        } else {
            strip->fader->setBrightness(0);
            publishState("/state", "off", strip);
        }
        strip->fader->show();
        strip->changed = false;
    }
}

void finishWiFiConnect() {
    Serial.printf("%s is ready\n", LED_LIGHTS);
}

void loop() {
    gizmo.isNetworkAvailable(finishWiFiConnect);
    handleLEDs(&frontStrip);
    handleLEDs(&backStrip);
}

void handleRoot() {
    snprintf(html, MAX_HTML, INDEX_HTML, gizmo.getHostname(),
             gizmo.getHostname(),
             frontStrip.on ? "true" : "false", frontStrip.color, frontStrip.brightness, frontStrip.ledPin, frontStrip.ledOffset, frontStrip.ledCount,
             gizmo.getHostname(),
             backStrip.on ? "true" : "false", backStrip.color, backStrip.brightness, backStrip.ledPin, backStrip.ledOffset, backStrip.ledCount,
             gizmo.getTopicPrefix(), SW_VERSION);
    gizmo.httpServer()->send(200, "text/html", html);
}

void handleLEDPage() {
    char nets[MAX_HTML/2];
    snprintf(html, MAX_HTML, LED_HTML,
             frontStrip.ledPin, frontStrip.ledOffset, frontStrip.ledCount,
             backStrip.ledPin, backStrip.ledOffset, backStrip.ledCount);
    gizmo.httpServer()->send(200, "text/html", html);
}

void handleLEDConfig() {
    char num[8];
    strncpy(num, gizmo.httpServer()->arg("upin").c_str(), 7);
    frontStrip.ledPin = atoi(num);
    strncpy(num, gizmo.httpServer()->arg("uoffset").c_str(), 7);
    frontStrip.ledOffset = atoi(num);
    strncpy(num, gizmo.httpServer()->arg("ucount").c_str(), 7);
    frontStrip.ledCount = atoi(num);

    strncpy(num, gizmo.httpServer()->arg("lpin").c_str(), 7);
    backStrip.ledPin = atoi(num);
    strncpy(num, gizmo.httpServer()->arg("loffset").c_str(), 7);
    backStrip.ledOffset = atoi(num);
    strncpy(num, gizmo.httpServer()->arg("lcount").c_str(), 7);
    backStrip.ledCount = atoi(num);

    Serial.println("Reconfiguring LEDS");
    Serial.printf("front: %d %d %d\n", frontStrip.ledPin, frontStrip.ledOffset, frontStrip.ledCount);
    Serial.printf("back: %d %d %d\n", backStrip.ledPin, backStrip.ledOffset, backStrip.ledCount);
    snprintf(html, MAX_HTML, LEDCFG_HTML);

    gizmo.httpServer()->send(200, "text/html", html);
    saveLEDConfig(LED_DATA "front", &frontStrip);
    saveLEDConfig(LED_DATA "back", &backStrip);
    gizmo.scheduleRestart();
}

void loadLEDConfig(char *fileName, Strip *strip) {
    File f = SPIFFS.open(fileName, "r");
    if (f) {
        char num[8];
        int l = f.readBytesUntil('|', num, 7);
        num[l] = NULL;
        strip->ledPin = atoi(num);
        l = f.readBytesUntil('|', num, 7);
        num[l] = NULL;
        strip->ledOffset = atoi(num);
        l = f.readBytesUntil('|', num, 7);
        num[l] = NULL;
        strip->ledCount = atoi(num);
        f.close();
    }
}

void saveLEDConfig(char *fileName, Strip *strip) {
    File f = SPIFFS.open(fileName, "w");
    if (f) {
        f.printf("%d|%d|%d\n", strip->ledPin, strip->ledOffset, strip->ledCount);
        f.close();
    }
}

void switchOn(boolean o, Strip *strip) {
    strip->changed = strip->on != o;
    strip->on = o;
}

void setBrightness(uint8_t b, Strip *strip) {
    strip->changed = b != strip->brightness;
    strip->brightness = b;
}