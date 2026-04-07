#include "MockAudioManager.h"

// Musical notes frequencies
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880

// A recognizable melody snippet (Star Wars Cantina Band style)
int melody[] = {
    NOTE_A4, NOTE_D5, NOTE_A4, NOTE_D5, NOTE_A4, NOTE_D5, NOTE_A4, NOTE_AS4,
    NOTE_A4, NOTE_AS4, NOTE_A4, NOTE_G4, NOTE_F4, NOTE_G4, NOTE_D4
};

// Durations: 4 = quarter note, 8 = eighth note, etc
int durations[] = {
    8, 8, 8, 8, 8, 8, 8, 4,
    8, 8, 8, 8, 8, 8, 4
};

MockAudioManager::MockAudioManager(int pinL, int pinR) 
    : _pinL(pinL), _pinR(pinR), _volume(0.5f), _playing(false), _mode(MODE_SD_CARD), _lastUpdate(0) {}

void MockAudioManager::begin() {
    pinMode(_pinL, OUTPUT);
    pinMode(_pinR, OUTPUT);
    Serial.println(F("--- Mock Audio Manager Initialized (UNO Quality Test) ---"));
}

void MockAudioManager::update() {
    static int noteIndex = 0;
    
    if (_playing) {
        int noteDuration = 1000 / durations[noteIndex];
        
        // Only move to next note if the previous one is finished + 30% pause
        if (millis() - _lastUpdate > (noteDuration * 1.30)) {
            int currentNote = melody[noteIndex];
            
            // Alternate speakers to test stereo separation
            if (noteIndex % 2 == 0) {
                tone(_pinL, currentNote, noteDuration);
            } else {
                tone(_pinR, currentNote, noteDuration);
            }
            
            _lastUpdate = millis();
            noteIndex++;
            
            // Loop melody
            if (noteIndex >= (sizeof(melody) / sizeof(int))) {
                noteIndex = 0;
            }
        }
    }
}

void MockAudioManager::play() {
    _playing = true;
    Serial.println(F("Audio: Play (Melody Test)"));
}

void MockAudioManager::pause() {
    _playing = false;
    noTone(_pinL);
    noTone(_pinR);
    Serial.println(F("Audio: Pause"));
}

void MockAudioManager::next() {
    Serial.println(F("Audio: Next Track (Sweep Test)"));
    // A quick frequency sweep to test range
    for (int i = 200; i < 1000; i += 50) {
        tone(_pinL, i, 20);
        tone(_pinR, i, 20);
        delay(25);
    }
}

void MockAudioManager::setVolume(float volume) {
    _volume = constrain(volume, 0.0f, 1.0f);
    Serial.print(F("Audio: Volume set to "));
    Serial.println(_volume);
    // Note: tone() doesn't support amplitude/volume on Uno, 
    // but we log it to maintain the interface logic.
}

float MockAudioManager::getVolume() { 
    return _volume; 
}

int MockAudioManager::getAudioLevel() {
    // Return a value based on the current frequency being played for LED sync
    return _playing ? random(1000, 3000) : 0;
}

void MockAudioManager::setMode(AudioMode mode) {
    _mode = mode;
    Serial.print(F("Audio: Mode changed to "));
    Serial.println(mode == MODE_SD_CARD ? F("SD_CARD") : F("BLUETOOTH"));
}

AudioMode MockAudioManager::getMode() { 
    return _mode; 
}

bool MockAudioManager::isPlaying() { 
    return _playing; 
}