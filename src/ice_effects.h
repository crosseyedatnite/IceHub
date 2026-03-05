#ifndef ICE_EFFECTS_H
#define ICE_EFFECTS_H


#include <FastLED.h>
#ifdef RED
#undef RED
#endif


// Shared Enum (Used by Main for MQTT/Web, used by Effects for logic)
enum DisplayMode { RAINBOW_PULSE, TRAIL_SPARK, PALETTE_WAVE, CONFETTI, JUGGLE, JITTER, CRACKLE, FIREWORK, BREATHE, SCANNER, TWINKLE, METEOR, MANUAL_SOLID, OFF };

class IceEffects {
  public:
    struct Command {
        const char* url;
        const char* label;
        const char* cssClass;
        const char* mqttName; // e.g., "RAINBOW"
        DisplayMode mode;
    };

    IceEffects(CRGB* leds, int numLeds);
    void begin();
    void run(); // Call this in loop()

    // Control Interface
    void setMode(DisplayMode mode);
    bool setModeByMqttName(const char* name);
    DisplayMode getMode();
    const char* getModeName();
    const char* getEffectList(); // For Home Assistant Discovery
    void getCapabilitiesJSON(char* buffer, size_t maxLen);
    
    static const int COMMAND_COUNT = 13;
    
    // Interface
    Command getCommand(int index);
    bool parseCommand(const char* cmd);
    
    void setManualColor(uint8_t r, uint8_t g, uint8_t b);
    CRGB getManualColor();

    void setBrightness(uint8_t scale);
    uint8_t getBrightness();

  private:
    // Internal State
    CRGB* _leds;
    int   _numLeds;
    DisplayMode _currentMode;
    CRGB _manualColor;
    uint8_t _gHue;
    uint8_t _startIndex;
    
    // Palette State
    CRGBPalette16 _currentPalette;
    TBlendType    _currentBlending;

    // Animation Helpers
    void pulseGlitter();
    void addGlitter(fract8 chanceOfGlitter);
    void trailSpark();
    void palletShift();
    void fill_palette_strip();
    void confetti();
    void juggle();
    void jitter();
    void crackle();
    void firework();
    void breathe();
    void scanner();
    void twinkle();
    void meteor();

    // Firework State
    uint8_t _fwState;
    uint8_t _fwPos;
    uint8_t _fwRadius;
    uint8_t _fwHue;
};

#endif