#include "ice_effects.h"

// Animation Constants
#define RAINBOW_SPEED 10
#define GLITTER_CHANCE 20
#define PULSE_BPM 15

// Define the "Frost" Palette
const TProgmemPalette16 FrostColors_P PROGMEM = {
  CRGB::DarkBlue, CRGB::Blue, CRGB::DeepSkyBlue, CRGB::Azure,
  CRGB::White, CRGB::LightCyan, CRGB::Aqua, CRGB::Blue,
  CRGB::DarkBlue, CRGB::Navy, CRGB::Black, CRGB::Black,
  CRGB::DarkBlue, CRGB::Blue, CRGB::Azure, CRGB::White
};

const IceEffects::Command _commandTable[] = {
    {"/RAINBOW", "Rainbow Pulse", "rainbow", "RAINBOW", RAINBOW_PULSE},
    {"/TRAIL",   "Ice Trail",     "trail",   "TRAIL",   TRAIL_SPARK},
    {"/WAVE",    "Frost Wave",    "wave",    "WAVE",    PALETTE_WAVE},
    {"/CONFETTI","Confetti",      "confetti","CONFETTI",CONFETTI},
    {"/JUGGLE",  "Juggle",        "juggle",  "JUGGLE",  JUGGLE},
    {"/JITTER",  "Jitter",        "jitter",  "JITTER",  JITTER},
    {"/CRACKLE", "Crackle",       "crackle", "CRACKLE", CRACKLE},
    {"/FIREWORK","Firework",      "firework","FIREWORK",FIREWORK},
    {"/BREATHE", "Breathe",       "breathe", "BREATHE", BREATHE},
    {"/SCANNER", "Scanner",       "scanner", "SCANNER", SCANNER},
    {"/TWINKLE", "Twinkle",       "twinkle", "TWINKLE", TWINKLE},
    {"/METEOR",  "Meteor",        "meteor",  "METEOR",  METEOR},
    {"/OFF",     "Power Off",     "off",     "None",    OFF}
};

IceEffects::IceEffects(CRGB* leds, int numLeds) 
    : _leds(leds),
      _numLeds(numLeds),
      _currentMode(PALETTE_WAVE), 
      _manualColor(CRGB(255, 200, 130)), // Warm White Default
      _gHue(0), 
      _startIndex(0),
      _currentBlending(LINEARBLEND),
      _fwState(0)
{
}

void IceEffects::begin() {
    // Safety
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 400); 
    
    FastLED.setBrightness(120);

    // Initial Palette
    _currentPalette = FrostColors_P;
}

void IceEffects::run() {
    // 60 FPS Animation Tick
    EVERY_N_MILLISECONDS(16) { 
        _gHue++; 
        
        switch (_currentMode) {
            case RAINBOW_PULSE: pulseGlitter(); break;
            case TRAIL_SPARK:   trailSpark();   break;
            case PALETTE_WAVE:  palletShift();  break;
            case CONFETTI:      confetti();     break;
            case JUGGLE:        juggle();       break;
            case JITTER:        jitter();       break;
            case CRACKLE:       crackle();      break;
            case FIREWORK:      firework();     break;
            case BREATHE:       breathe();      break;
            case SCANNER:       scanner();      break;
            case TWINKLE:       twinkle();      break;
            case METEOR:        meteor();       break;
            case OFF:           FastLED.clear(); break;
            case MANUAL_SOLID:  fill_solid(_leds, _numLeds, _manualColor); break;
        }
        
        FastLED.show();
    }
}

// --- Setters / Getters ---

void IceEffects::setMode(DisplayMode mode) {
    _currentMode = mode;
    _fwState = 0; // Reset firework state on mode change
    if (mode == OFF) {
        FastLED.clear();
        FastLED.show();
    }
}

// Add a new method to handle MQTT effect strings
bool IceEffects::setModeByMqttName(const char* name) {
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(name, _commandTable[i].mqttName) == 0) {
            setMode(_commandTable[i].mode);
            return true;
        }
    }
    return false;
}

DisplayMode IceEffects::getMode() {
    return _currentMode;
}

const char* IceEffects::getModeName() {
    switch (_currentMode) {
        case RAINBOW_PULSE: return "RAINBOW";
        case TRAIL_SPARK:   return "TRAIL";
        case PALETTE_WAVE:  return "WAVE";
        case CONFETTI:      return "CONFETTI";
        case JUGGLE:        return "JUGGLE";
        case JITTER:        return "JITTER";
        case CRACKLE:       return "CRACKLE";
        case FIREWORK:      return "FIREWORK";
        case BREATHE:       return "BREATHE";
        case SCANNER:       return "SCANNER";
        case TWINKLE:       return "TWINKLE";
        case METEOR:        return "METEOR";
        default:            return "None";
    }
}

const char* IceEffects::getEffectList() {
    // Returning a hardcoded string from the class that owns the modes
    return "[\"RAINBOW\",\"TRAIL\",\"WAVE\",\"CONFETTI\",\"JUGGLE\",\"JITTER\",\"CRACKLE\",\"FIREWORK\",\"BREATHE\",\"SCANNER\",\"TWINKLE\",\"METEOR\"]";
}

void IceEffects::getCapabilitiesJSON(char* buffer, size_t maxLen) {
    // Manual JSON construction to save flash/ram on Nano
    // {"leds":N,"bri":B,"mode":"M","modes":["A","B",...]}
    
    size_t offset = 0;
    offset += snprintf(buffer + offset, maxLen - offset, "{\"leds\":%d,\"bri\":%d,\"mode\":\"%s\",\"modes\":[", 
        _numLeds, getBrightness(), getModeName());
        
    for(int i=0; i<COMMAND_COUNT; i++) {
        if(i > 0) offset += snprintf(buffer + offset, maxLen - offset, ",");
        offset += snprintf(buffer + offset, maxLen - offset, "\"%s\"", _commandTable[i].mqttName);
    }
    
    snprintf(buffer + offset, maxLen - offset, "]}");
}

IceEffects::Command IceEffects::getCommand(int index) {
    if (index >= 0 && index < COMMAND_COUNT) return _commandTable[index];
    return {"", "", "", "", OFF};
}

bool IceEffects::parseCommand(const char* request) {
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (strstr(request, _commandTable[i].url)) {
            setMode(_commandTable[i].mode);
            return true;
        }
    }
    return false;
}

void IceEffects::setManualColor(uint8_t r, uint8_t g, uint8_t b) {
    _manualColor = CRGB(r, g, b);
    _currentMode = MANUAL_SOLID; // Auto-switch to manual when color is set
}

CRGB IceEffects::getManualColor() {
    return _manualColor;
}

void IceEffects::setBrightness(uint8_t scale) {
    FastLED.setBrightness(scale);
    FastLED.show(); // Immediate update
}

uint8_t IceEffects::getBrightness() {
    return FastLED.getBrightness();
}

// --- Animation Logic ---

void IceEffects::pulseGlitter() {
    fill_rainbow(_leds, _numLeds, _gHue, 7);
    addGlitter(GLITTER_CHANCE);
    
    uint8_t pulseBrightness = beatsin8(PULSE_BPM, 0, 120);
    FastLED.setBrightness(pulseBrightness);
}

void IceEffects::addGlitter(fract8 chanceOfGlitter) {
    if (random8() < chanceOfGlitter) {
        _leds[random16(_numLeds)] += CRGB::White;
    }
}

void IceEffects::trailSpark() {
    fadeToBlackBy(_leds, _numLeds, 20);
    int pos = beatsin16(15, 0, _numLeds - 1);
    _leds[pos] += CHSV(_gHue, 255, 192);
}

void IceEffects::palletShift() {
    EVERY_N_MILLISECONDS(20) {
        _startIndex++; 
        fill_palette_strip();
    }
}

void IceEffects::fill_palette_strip() {
    uint8_t colorIndex = _startIndex;
    for( int i = 0; i < _numLeds; i++) {
        _leds[i] = ColorFromPalette(_currentPalette, colorIndex, 255, _currentBlending);
        colorIndex += 3; 
    }
}

void IceEffects::confetti() {
    fadeToBlackBy(_leds, _numLeds, 10);
    int pos = random16(_numLeds);
    _leds[pos] += CHSV(_gHue + random8(64), 200, 255);
}

void IceEffects::juggle() {
    fadeToBlackBy(_leds, _numLeds, 20);
    uint8_t dothue = 0;
    for (int i = 0; i < 8; i++) {
        _leds[beatsin16(i + 7, 0, _numLeds - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

void IceEffects::jitter() {
    fill_solid(_leds, _numLeds, CRGB::Black);
    // Toggle even/odd pixels every 128ms approx
    bool odd = (millis() >> 7) & 1;
    for (int i = 0; i < _numLeds; i++) {
        if ((i % 2) == odd) {
            _leds[i] = _manualColor;
        }
    }
}

void IceEffects::crackle() {
    // Fill with dimmed manual color
    CRGB dimColor = _manualColor;
    dimColor.nscale8(50); 
    fill_solid(_leds, _numLeds, dimColor);
    // Add intense white sparkles
    addGlitter(60); 
}

void IceEffects::firework() {
    fadeToBlackBy(_leds, _numLeds, 30); // Leave a trail

    if (_fwState == 0) {
        if (random8() < 15) { // Chance to spawn (Increased from 5)
            _fwState = 1;
            _fwPos = random16(2, _numLeds - 3);
            _fwRadius = 0;
            _fwHue = random8();
            _leds[_fwPos] = CHSV(_fwHue, 255, 255);
        }
    } else {
        // Expand the explosion
        _fwRadius++;
        int left = _fwPos - _fwRadius;
        int right = _fwPos + _fwRadius;
        
        if (left >= 0) _leds[left] = CHSV(_fwHue, 255, 255);
        if (right < _numLeds) _leds[right] = CHSV(_fwHue, 255, 255);
        
        if (_fwRadius >= 15) _fwState = 0; // Reset after expanding
    }
}

void IceEffects::breathe() {
    uint8_t bri = beatsin8(12, 60, 255); // 12 BPM, min 60, max 255
    CRGB c = _manualColor;
    c.nscale8(bri);
    fill_solid(_leds, _numLeds, c);
}

void IceEffects::scanner() {
    fadeToBlackBy(_leds, _numLeds, 32);
    int pos = beatsin16(30, 0, _numLeds - 1);
    _leds[pos] = _manualColor;
}

void IceEffects::twinkle() {
    fadeToBlackBy(_leds, _numLeds, 20);
    if (random8() < 40) { 
        _leds[random16(_numLeds)] = _manualColor;
    }
}

void IceEffects::meteor() {
    fadeToBlackBy(_leds, _numLeds, 64);
    // beat8 generates a sawtooth wave (0-255) for one-way motion
    int pos = map(beat8(40), 0, 255, 0, _numLeds - 1);
    _leds[pos] = _manualColor;
}