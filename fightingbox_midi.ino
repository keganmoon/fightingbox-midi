/*
 * FightingBox -> USB-MIDI firmware
 * Target: RP2040 board originally running GP2040-CE
 *
 * ============================ CONTROLS ============================
 *
 * MODE BUTTONS
 *   Start (GPIO 6)  -> CYCLE melodic modes: Chromatic -> Scale -> Chord
 *                      -> back to Chromatic. All send on channel 1.
 *                      From a drum mode, Start returns to the melodic
 *                      mode you were last in.
 *   L3    (GPIO 21) -> Drum-note-numbers mode, channel 1
 *                      (IN CHORD MODE: becomes the "add 6th" extension)
 *   R3    (GPIO 22) -> Real drum mode, channel 10, GM percussion
 *                      (IN CHORD MODE: becomes the "add min 7th" extension)
 *   Select (GPIO 5) -> MIDI PANIC: all-notes-off on all 16 channels, reset
 *                      octave/transpose, and clear all chord latches.
 *   Turbo  (GPIO 20)-> RECORD: save a button's exact current sound (every
 *                      note of a chord + its channel) onto another button,
 *                      permanently, surviving mode switches.
 *                      Tap Turbo -> press SOURCE -> press TARGET.
 *                      Tap Turbo again mid-flow to cancel.
 *                      (IN CHORD MODE: becomes the "add maj 7th" extension)
 *   Home   (GPIO 4) -> TAP: cycle the variant for the current mode
 *                        (Scale mode: which scale. Chord mode: n/a.)
 *                      HOLD (>=600ms): wipe all recorded overrides.
 *                      (IN CHORD MODE: becomes the "add 9th" extension)
 *
 * Because Chord mode borrows L3/R3/Turbo/Home for extensions, to reach the
 * drum modes from Chord mode press Start (back to Chromatic) then L3/R3.
 *
 * ---------------------------- MELODIC MODES ----------------------------
 *   Chromatic : 8 buttons = 8 consecutive semitones from C4.
 *   Scale     : 8 buttons = 8 degrees of the selected scale, so there are
 *               no wrong notes. Home taps through 8 scales: Major, Minor,
 *               MajPentatonic, MinPentatonic, Blues, Dorian, Mixolydian,
 *               HarmonicMinor. Scales shorter than 8 notes wrap upward.
 *   Chord     : Orchid-style. The 8 buttons choose the ROOT (as a scale
 *               degree); modifier buttons decide the chord's quality.
 *
 * ------------------------ CHORD MODE MODIFIERS -------------------------
 *   Chord TYPE (d-pad), pick the shape:
 *       Up    = Major       (root, +4, +7)
 *       Down  = Minor       (root, +3, +7)
 *       Left  = Suspended   (root, +5, +7)
 *       Right = Diminished  (root, +3, +6)
 *     With NO type active you get the diatonic triad for that scale degree
 *     - i.e. the chord that belongs in the key (Orchid's "Key mode").
 *     If several types are active at once, priority is Maj > Min > Sus > Dim.
 *
 *   Chord EXTENSIONS (stackable, combine freely with each other and any type):
 *       L3    = 6th        (+9)
 *       R3    = minor 7th  (+10)
 *       Turbo = Major 7th  (+11)
 *       Home  = 9th        (+14)
 *
 *   Every modifier is TAP TO LATCH, HOLD FOR MOMENTARY:
 *     - Hold it and it applies while held, releasing when you let go.
 *     - Tap it (under 150ms) and it latches on; tap again to latch off.
 *     So you can lock "Minor" on and play a progression, or just grab
 *     "Major 7th" for a single chord. Select clears every latch at once.
 *
 *   Chord mode has no octave/transpose on the d-pad (it is all modifiers
 *   there). Octave/transpose are global: set them in Chromatic or Scale
 *   mode and they carry into Chord mode.
 *
 * ------------------------------- D-PAD ---------------------------------
 *   Chromatic / Scale / L3 drum-notes:
 *     - TAP Up/Down    -> permanent octave shift +/-1
 *     - TAP Left/Right -> permanent semitone shift +/-1 (root key in Scale)
 *     - HOLD (>=150ms) -> TEMPORARY shift, snaps back on release.
 *       Holding two at once (e.g. Left+Down) stacks both offsets.
 *   Chord: 4 chord-type modifiers (see above).
 *   R3 real drums:
 *     - TAP Up/Down    -> cycle note bank (Core / Alt / Latin)
 *     - TAP Left/Right -> cycle GM kit via Program Change on ch10
 *
 * RECORDED OVERRIDES beat everything: once recorded, a button always plays
 * its saved notes+channel in every mode until Home-hold clears it. The
 * channel is saved too, so a real ch10 drum hit can live on one button
 * while the rest of the board plays melody, or vice versa.
 *
 * DISPLAY: disabled. The board's OLED (SDA=26/SCL=27/0x3C per the GP2040-CE
 * backup) never ACKed on I2C - likely an SH1106 controller rather than the
 * SSD1306 assumed, or a dead panel. Display code was removed rather than
 * left half-wired. Revisit with an I2C scan when convenient.
 *
 * Requires (arduino-cli):
 *   - Core: rp2040:rp2040 (Earle Philhower), FQBN suffix :usbstack=tinyusb
 *   - Libraries: Adafruit TinyUSB Library, MIDI Library
 *
 * Flash: hold BOOTSEL, plug in USB, board mounts as RPI-RP2, then
 *   arduino-cli upload --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb -p UF2_Board .
 *
 * Restore GP2040-CE: hold BOOTSEL, drag the original GP2040-CE .uf2 onto
 * RPI-RP2, then re-import gp2040ce_backup_*.gp2040 via the web configurator.
 */

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>   // RP2040: a 4 KB flash sector emulating EEPROM

// Triad (3) + up to 4 stacked extensions + headroom.
// Must be a #define up here: Arduino auto-inserts function prototypes
// directly below the includes, and they reference this in array params.
#define MAX_CHORD_NOTES 8

// ---- OLED --------------------------------------------------------------
// CONFIRMED by an I2C probe: an SSD1306 answers at 0x3C on I2C BLOCK 1.
// GPIO 26/27 can only belong to block 1, which the Arduino-Pico core calls
// Wire1 - NOT Wire (block 0). Driving this with `Wire` fails silently and
// looks exactly like a missing panel; that cost a lot of debugging.
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_SDA 26
#define OLED_SCL 27
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire1, -1);
bool displayOk = false;
unsigned long lastDraw = 0;
const unsigned long DRAW_INTERVAL_MS = 80;  // ~12fps, keeps MIDI latency low

// ---- USB MIDI device ----
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

// ---- Pin mapping (from GP2040-CE backup, active-low w/ internal pullups) ----
// Physical order: 1P 2P 3P 4P 1K 2K 3K 4K
// Verified empirically against the hardware (the monitor's key-press feed
// showed the old Hit-Box-convention guess was scrambled). This board wires
// the eight main keys sequentially: GPIO 7..14 = 1P 2P 3P 4P 1K 2K 3K 4K.
const uint8_t BTN_PINS[8]  = {7, 8, 9, 10, 11, 12, 13, 14};
const char*   BTN_NAMES[8] = {"1P","2P","3P","4P","1K","2K","3K","4K"};

const uint8_t PIN_UP     = 1;
const uint8_t PIN_DOWN   = 2;
const uint8_t PIN_LEFT   = 0;
const uint8_t PIN_RIGHT  = 3;
// Verified empirically: the monitor's press feed showed every function
// button reporting as its neighbour, i.e. the whole row was shifted one
// slot. The physical top row, left to right, is TURBO HOME SELECT START
// L3 R3 and maps to these GPIOs. (The GP2040-CE labels S1/S2/A1/A2 do NOT
// correspond to the silkscreened button names on this board.)
// MEASURED, then corrected against observed behaviour. The prober sends
// the GPIO number as a note number, but 18 presses only ever produce 17
// notes, so a positional read of that capture is ambiguous about WHICH
// key is missing. Live behaviour settled it: pressing HOME acted as
// Turbo, SELECT acted as Home, START acted as Select - i.e. the TURBO key
// is the dead one and everything after it sits one slot earlier.
//   TURBO = dead (no GPIO responds)   HOME  = 4
//   SELECT = 5                        START = 6
//   L3 = 21                           R3    = 22
// So this board has FIVE working function keys. Turbo's record job moved
// to a HOME hold.
const uint8_t PIN_HOME   = 4;   // tap: variant · hold: record / chord ext: 9th
const uint8_t PIN_SELECT = 5;   // panic + shift layer
const uint8_t PIN_START  = 6;   // cycle melodic modes / hold: latch
const uint8_t PIN_L3     = 21;  // looper transport  / chord ext: 6th
const uint8_t PIN_R3     = 22;  // GM drum mode      / chord ext: m7
const uint8_t PIN_TURBO  = 20;  // DEAD key - no GPIO responds to it

// ---- Modes ----
enum Mode { MODE_CHROMATIC, MODE_SCALE, MODE_CHORD, MODE_CUSTOM, MODE_DRUM_GM };
Mode currentMode = MODE_CHROMATIC;

const uint8_t NUM_MELODIC = 4;
const Mode MELODIC_CYCLE[NUM_MELODIC] =
  { MODE_CHROMATIC, MODE_SCALE, MODE_CHORD, MODE_CUSTOM };
uint8_t melodicIndex = 0;

// ---- Chromatic ----
const int8_t CHROMATIC_OFFSETS[8] = {0, 1, 2, 3, 4, 5, 6, 7};
const uint8_t BASE_NOTE = 60; // C4

// ---- Scales (semitone offsets from root; wraps by octave past the end) ----
const uint8_t NUM_SCALES = 8;
const uint8_t SCALE_LEN[NUM_SCALES] = { 7, 7, 5, 5, 6, 7, 7, 7 };
const int8_t  SCALES[NUM_SCALES][7] = {
  {0, 2, 4, 5, 7, 9, 11}, // Major
  {0, 2, 3, 5, 7, 8, 10}, // Natural Minor
  {0, 2, 4, 7, 9, 0,  0}, // Major Pentatonic (len 5)
  {0, 3, 5, 7, 10, 0, 0}, // Minor Pentatonic (len 5)
  {0, 3, 5, 6, 7, 10, 0}, // Blues (len 6)
  {0, 2, 3, 5, 7, 9, 10}, // Dorian
  {0, 2, 4, 5, 7, 9, 10}, // Mixolydian
  {0, 2, 3, 5, 7, 8, 11}, // Harmonic Minor
};
const char* SCALE_NAMES[NUM_SCALES] = {
  "Major","Minor","MajPent","MinPent","Blues","Dorian","Mixolydian","HarmMinor"
};
uint8_t scaleIndex = 0;

// ---- Chord modifiers (Orchid-style) ----
// Types, in priority order when several are active at once.
enum ChordTypeIdx { CT_MAJ = 0, CT_MIN = 1, CT_SUS = 2, CT_DIM = 3 };
const uint8_t NUM_CHORD_TYPES = 4;
const char* CHORD_TYPE_NAMES[NUM_CHORD_TYPES] = {"Maj","Min","Sus","Dim"};

// Extensions: semitones added above the root.
const uint8_t NUM_EXTENSIONS = 4;
const int8_t EXT_INTERVALS[NUM_EXTENSIONS] = { 9, 10, 11, 14 }; // 6th, m7, M7, 9th
const char* EXT_NAMES[NUM_EXTENSIONS] = {"6th","m7","M7","9th"};

// Latched vs momentary state for each modifier.
bool typeLatched[NUM_CHORD_TYPES] = {false};
bool typeHeld[NUM_CHORD_TYPES]    = {false};
bool extLatched[NUM_EXTENSIONS]   = {false};
bool extHeld[NUM_EXTENSIONS]      = {false};

bool typeActive(uint8_t i) { return typeLatched[i] || typeHeld[i]; }
bool extActive(uint8_t i)  { return extLatched[i]  || extHeld[i];  }

void clearAllLatches() {
  for (uint8_t i = 0; i < NUM_CHORD_TYPES; i++) typeLatched[i] = false;
  for (uint8_t i = 0; i < NUM_EXTENSIONS;  i++) extLatched[i]  = false;
}

// ---- Drums ----
const uint8_t NUM_BANKS = 3;
const uint8_t DRUM_BANKS[NUM_BANKS][8] = {
  {36, 38, 42, 46, 45, 47, 49, 51}, // Core
  {37, 39, 44, 43, 50, 52, 55, 53}, // Alt
  {56, 58, 60, 61, 62, 63, 64, 70}, // Latin
};
const char* BANK_NAMES[NUM_BANKS] = {"Core", "Alt", "Latin"};
uint8_t currentBank = 0;

const uint8_t NUM_KITS = 9;
const uint8_t KIT_PROGRAMS[NUM_KITS] = {0, 8, 16, 24, 25, 32, 40, 48, 56};
const char* KIT_NAMES[NUM_KITS] = {
  "Standard","Room","Power","Electronic","TR-808","Jazz","Brush","Orchestra","SFX"
};
uint8_t kitIndex = 0;

// ---- Custom banks -----------------------------------------------------
// Four banks of eight slots. Each slot holds a small chord (1..4 notes)
// plus its own channel, so a single bank can mix real ch10 drums with
// pitched ch1 material - which is what makes it useful for building a
// groove on one screen of keys.
//
// The defaults are picked to be immediately playable: a beat kit, a bass
// line, chord stabs, and hand percussion. Every slot can be overwritten
// per-bank with the record flow (hold Home), so the banks double as four
// independent user kits.
const uint8_t NUM_CUSTOM_BANKS = 4;
const char* CUSTOM_BANK_NAMES[NUM_CUSTOM_BANKS] = {"Beat","Bass","Stabs","Perc"};

struct Slot { uint8_t n[4]; uint8_t count; uint8_t ch; };

Slot customBank[NUM_CUSTOM_BANKS][8] = {
  // --- 0: Beat - core kit on the GM percussion channel ---
  { {{36},1,10}, {{38},1,10}, {{42},1,10}, {{46},1,10},
    {{39},1,10}, {{37},1,10}, {{51},1,10}, {{49},1,10} },
  // --- 1: Bass - C minor pentatonic walk, low register ---
  { {{36},1,1},  {{39},1,1},  {{41},1,1},  {{43},1,1},
    {{46},1,1},  {{48},1,1},  {{51},1,1},  {{53},1,1}  },
  // --- 2: Stabs - triads that comp together in C minor ---
  { {{48,51,55},3,1}, {{50,53,57},3,1}, {{51,55,58},3,1}, {{53,56,60},3,1},
    {{55,58,62},3,1}, {{56,60,63},3,1}, {{58,62,65},3,1}, {{60,63,67},3,1} },
  // --- 3: Perc - shakers, congas, cowbell, all ch10 ---
  { {{54},1,10}, {{56},1,10}, {{70},1,10}, {{69},1,10},
    {{63},1,10}, {{64},1,10}, {{62},1,10}, {{75},1,10} },
};
uint8_t customBankIndex = 0;

// ---- Shared pitch state ----
int8_t octaveShift    = 0;
int8_t transposeShift = 0;

const uint8_t CHANNEL_MELODY   = 1;
const uint8_t CHANNEL_DRUM_ALT = 1;
const uint8_t CHANNEL_DRUM_GM  = 10;
uint8_t velocity = 100;          // adjustable: hold Select + Up/Down

// ---- Latch (sustain) mode ----
// When on, a key press toggles its note instead of gating it: press to
// start, press again to stop. Turns 8 momentary keys into 8 sustaining
// voices you can stack by hand. Toggled by holding Start.
bool latchMode = false;

// ---- Looper ----
// Free-timing single-layer looper with overdub, in the classic pedal
// idiom. Recording starts on the FIRST note you play (not on the button
// press) so there is no dead air at the top of the loop.
enum LoopState { LOOP_IDLE, LOOP_REC, LOOP_PLAY, LOOP_DUB };
LoopState loopState = LOOP_IDLE;

struct LoopEvent { uint32_t t; uint8_t note; uint8_t vel; uint8_t ch; bool on; };
const uint16_t MAX_LOOP_EVENTS = 320;
LoopEvent loopBuf[MAX_LOOP_EVENTS];
uint16_t loopCount  = 0;   // events stored, kept sorted by t
uint32_t loopLen    = 0;   // loop length in ms (0 until the loop is closed)
uint32_t loopOrigin = 0;   // millis at t=0 of the current pass
uint16_t playHead   = 0;   // next event index to fire this pass
bool loopMuted = false;

// Notes the LOOP is currently sounding, so we can silence them cleanly.
uint8_t loopOnNote[MAX_CHORD_NOTES * 4];
uint8_t loopOnCh[MAX_CHORD_NOTES * 4];
uint8_t loopOnCount = 0;

const unsigned long DEBOUNCE_MS        = 8;
const unsigned long HOLD_THRESHOLD_MS  = 150; // tap vs hold, everywhere
const unsigned long HOME_RESET_HOLD_MS = 600; // Home hold = wipe overrides

uint8_t channelForMode(Mode m) {
  switch (m) {
    case MODE_CHROMATIC:
    case MODE_SCALE:
    case MODE_CHORD:     return CHANNEL_MELODY;
    case MODE_CUSTOM:    return CHANNEL_MELODY; // per-slot, see effectiveNotes
    case MODE_DRUM_GM:   return CHANNEL_DRUM_GM;
  }
  return CHANNEL_MELODY;
}

// ---- State broadcast (for the on-screen monitor) ----
// Every time anything user-visible changes, we publish the whole state as
// CCs on channel 16. Channel 16 is a deliberate choice: nothing musical is
// sent there, so a DAW track listening to ch1/ch10 never sees this traffic,
// but a monitor app can subscribe and mirror the controller's state.
const uint8_t CHANNEL_STATE = 16;
const uint8_t CC_MODE      = 20; // 0=Chromatic 1=Scale 2=Chord 3=DrumCh1 4=DrumGM
const uint8_t CC_SCALE     = 21; // scale index
const uint8_t CC_TYPES     = 22; // bitmask of active chord types  (Maj/Min/Sus/Dim)
const uint8_t CC_EXTS      = 23; // bitmask of active extensions   (6/m7/M7/9)
const uint8_t CC_OCTAVE    = 24; // octave shift + 64
const uint8_t CC_TRANSPOSE = 25; // semitone shift + 64
const uint8_t CC_KIT       = 26; // GM kit index
const uint8_t CC_BANK      = 27; // drum note bank index
const uint8_t CC_RECORD    = 28; // 0=idle 1=await source 2=await target
// CC values are 7-bit (0..127), so any 8-wide bitmask has to be split
// across two CCs - an 8-bit mask would overflow and corrupt the high bit.
const uint8_t CC_OVERRIDE_LO = 29; // recorded-sound mask, buttons 1P..4P
const uint8_t CC_OVERRIDE_HI = 36; // recorded-sound mask, buttons 1K..4K
const uint8_t CC_TYPELATCH = 30; // bitmask: which types are LATCHED (vs held)
const uint8_t CC_EXTLATCH  = 31; // bitmask: which exts are LATCHED (vs held)

// Live physical key state, for lighting up keys on the monitor.
const uint8_t CC_PRESS_LO  = 32; // pressed mask, buttons 1P 2P 3P 4P
const uint8_t CC_PRESS_HI  = 33; // pressed mask, buttons 1K 2K 3K 4K
const uint8_t CC_PRESS_DPAD= 34; // pressed mask, Up Down Left Right
const uint8_t CC_PRESS_FN  = 35; // pressed mask, L3 R3 Turbo Home Start Select
const uint8_t CC_LOOP      = 37; // 0=idle 1=rec 2=play 3=dub
const uint8_t CC_LOOPMUTE  = 38; // 0/1
const uint8_t CC_LATCH     = 39; // 0/1 latch (sustain) mode
const uint8_t CC_VELOCITY  = 40; // current velocity
const uint8_t CC_SHIFT     = 41; // 0/1 Select held (shift layer live)
const uint8_t CC_CUSTBANK  = 42; // active custom bank index
const uint8_t CC_SAVED     = 43; // 1 = settings committed to flash

void markSettingsDirty();   // defined with the persistence code below
bool stateDirty = true;
unsigned long lastStateSend = 0;
const unsigned long STATE_HEARTBEAT_MS = 1000; // resend periodically so a
                                               // monitor opened late syncs up

void markStateDirty() { stateDirty = true; markSettingsDirty(); }

bool inChordMode()   { return currentMode == MODE_CHORD; }
bool drumGM()        { return currentMode == MODE_DRUM_GM; }
bool inCustom()      { return currentMode == MODE_CUSTOM; }
// Must list every mode in MELODIC_CYCLE. Omitting one traps you in it:
// Start only advances melodicIndex when this returns true, so a missing
// mode makes switchMode() re-select the mode you are already in.
bool inMelodicMode() { return currentMode == MODE_CHROMATIC ||
                              currentMode == MODE_SCALE ||
                              currentMode == MODE_CHORD ||
                              currentMode == MODE_CUSTOM; }

// ---- Recorded overrides (store the whole chord + channel) ----
bool    overrideActive[8]                 = {false};
int     overrideNotes[8][MAX_CHORD_NOTES] = {{0}};
uint8_t overrideCount[8]                  = {0};
uint8_t overrideChannel[8]                = {0};

enum RecordState { RECORD_IDLE, RECORD_WAIT_SOURCE, RECORD_WAIT_TARGET };
RecordState recordState = RECORD_IDLE;
int     recordNotes[MAX_CHORD_NOTES] = {0};
uint8_t recordCount = 0;
uint8_t recordChannel = 0;

// ---- Currently sounding notes per button ----
int     activeNotes[8][MAX_CHORD_NOTES];
uint8_t activeCount[8]   = {0};
uint8_t activeChannel[8] = {0};

// ---- Debounce: main 8 buttons ----
bool btnState[8]   = {false};
bool btnLastRaw[8] = {false};
unsigned long btnLastChange[8] = {0};
bool btnSuppressed[8] = {false}; // press consumed by the record flow

// ---- Debounce: Start / Select ----
bool startState=false,  startLastRaw=false;  unsigned long startLastChange=0;
bool selectState=false, selectLastRaw=false; unsigned long selectLastChange=0;

// ---- Debounce + press timing: the four dual-purpose function buttons ----
// index 0=L3, 1=R3, 2=Turbo, 3=Home. In Chord mode these are extensions;
// elsewhere they are drum modes / record / variant+reset.
const uint8_t FN_PINS[4] = {PIN_L3, PIN_R3, PIN_TURBO, PIN_HOME};
bool fnState[4]   = {false, false, false, false};
bool fnLastRaw[4] = {false, false, false, false};
unsigned long fnLastChange[4]  = {0, 0, 0, 0};
unsigned long fnPressStart[4]  = {0, 0, 0, 0};
bool fnHomeResetFired = false; // Home-hold wipe already fired this press
unsigned long startPressStart = 0;  // Start: tap vs hold
bool startLongFired = false;
unsigned long selectPressStart = 0; // Select: tap (panic) vs hold (shift)
bool shiftUsed = false;             // a shift action fired, so don't panic
bool fnLongFired[4] = {false, false, false, false}; // long-hold escape fired
                                                    // (chord mode: Turbo/Home)

// ---- Debounce + hold: d-pad (0=Up,1=Down,2=Left,3=Right) ----
const uint8_t DPAD_PINS[4] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT};
bool dpadState[4]   = {false, false, false, false};
bool dpadLastRaw[4] = {false, false, false, false};
unsigned long dpadLastChange[4] = {0, 0, 0, 0};
unsigned long dpadPressStart[4] = {0, 0, 0, 0};
bool   dpadTempApplied[4] = {false, false, false, false};
int8_t dpadTempValue[4]   = {0, 0, 0, 0};

// Map d-pad index -> chord type index. Up=Maj, Down=Min, Left=Sus, Right=Dim.
const uint8_t DPAD_TO_TYPE[4] = { CT_MAJ, CT_MIN, CT_SUS, CT_DIM };

bool readDebounced(uint8_t pin, bool &state, bool &lastRaw, unsigned long &lastChange) {
  bool raw = (digitalRead(pin) == LOW); // active-low
  if (raw != lastRaw) {
    lastChange = millis();
    lastRaw = raw;
  }
  if ((millis() - lastChange) > DEBOUNCE_MS) {
    if (state != raw) {
      state = raw;
      return true;
    }
  }
  return false;
}

int8_t applyStep(uint8_t i) {
  int8_t before;
  switch (i) {
    case 0: before = octaveShift;    if (octaveShift    <   4) octaveShift++;    return octaveShift - before;
    case 1: before = octaveShift;    if (octaveShift    >  -4) octaveShift--;    return octaveShift - before;
    case 2: before = transposeShift; if (transposeShift > -12) transposeShift--; return transposeShift - before;
    case 3: before = transposeShift; if (transposeShift <  12) transposeShift++; return transposeShift - before;
  }
  return 0;
}

void revertStep(uint8_t i, int8_t delta) {
  if (i == 0 || i == 1) octaveShift -= delta;
  else                  transposeShift -= delta;
}

int clampNote(int n) {
  if (n < 0) return 0;
  if (n > 127) return 127;
  return n;
}

// Semitone offset of the Nth degree of the current scale, wrapping into
// higher octaves past the end so pentatonics simply reach further up.
int scaleDegreeOffset(int degree) {
  uint8_t len = SCALE_LEN[scaleIndex];
  int oct = degree / len;
  int idx = degree % len;
  return SCALES[scaleIndex][idx] + 12 * oct;
}

// Append a note if it is not already in the list (avoids duplicate Note On
// for the same pitch, which some synths reference-count badly).
void addNote(int out[MAX_CHORD_NOTES], uint8_t &count, int note) {
  if (count >= MAX_CHORD_NOTES) return;
  int n = clampNote(note);
  for (uint8_t i = 0; i < count; i++) if (out[i] == n) return;
  out[count++] = n;
}

// Everything button idx should play right now, ignoring recorded overrides.
uint8_t buildNotes(uint8_t idx, int out[MAX_CHORD_NOTES]) {
  int base = BASE_NOTE + (octaveShift * 12) + transposeShift;
  uint8_t count = 0;

  switch (currentMode) {
    case MODE_CHROMATIC:
      addNote(out, count, base + CHROMATIC_OFFSETS[idx]);
      return count;

    case MODE_SCALE:
      addNote(out, count, base + scaleDegreeOffset(idx));
      return count;

    case MODE_CHORD: {
      int root = base + scaleDegreeOffset(idx);

      // Pick the highest-priority active chord type, if any.
      int8_t chosen = -1;
      for (uint8_t t = 0; t < NUM_CHORD_TYPES; t++) {
        if (typeActive(t)) { chosen = (int8_t)t; break; }
      }

      if (chosen < 0) {
        // No type held: diatonic triad for this scale degree (Key mode).
        addNote(out, count, base + scaleDegreeOffset(idx));
        addNote(out, count, base + scaleDegreeOffset(idx + 2));
        addNote(out, count, base + scaleDegreeOffset(idx + 4));
      } else {
        addNote(out, count, root);
        switch ((ChordTypeIdx)chosen) {
          case CT_MAJ: addNote(out, count, root + 4); addNote(out, count, root + 7); break;
          case CT_MIN: addNote(out, count, root + 3); addNote(out, count, root + 7); break;
          case CT_SUS: addNote(out, count, root + 5); addNote(out, count, root + 7); break;
          case CT_DIM: addNote(out, count, root + 3); addNote(out, count, root + 6); break;
        }
      }

      // Stack any active extensions on top of the root.
      for (uint8_t e = 0; e < NUM_EXTENSIONS; e++) {
        if (extActive(e)) addNote(out, count, root + EXT_INTERVALS[e]);
      }
      return count;
    }

    case MODE_CUSTOM: {
      const Slot &sl = customBank[customBankIndex][idx];
      for (uint8_t k = 0; k < sl.count; k++) {
        // Pitched slots follow octave/transpose; ch10 percussion never does.
        int n = sl.n[k];
        if (sl.ch != CHANNEL_DRUM_GM) n += (octaveShift * 12) + transposeShift;
        addNote(out, count, n);
      }
      return count;
    }

    case MODE_DRUM_GM:
      // Percussion note numbers are fixed - never transpose them.
      if (count < MAX_CHORD_NOTES) out[count++] = DRUM_BANKS[currentBank][idx];
      return count;
  }

  addNote(out, count, BASE_NOTE);
  return count;
}

// What the button actually plays: a recorded override wins over everything.
uint8_t effectiveNotes(uint8_t idx, int out[MAX_CHORD_NOTES], uint8_t &chOut) {
  if (overrideActive[idx]) {
    for (uint8_t i = 0; i < overrideCount[idx]; i++) out[i] = overrideNotes[idx][i];
    chOut = overrideChannel[idx];
    return overrideCount[idx];
  }
  if (currentMode == MODE_CUSTOM) chOut = customBank[customBankIndex][idx].ch;
  else                            chOut = channelForMode(currentMode);
  return buildNotes(idx, out);
}

// ---------------------------- LOOPER ----------------------------------

void loopSilence() {                 // kill anything the loop is holding
  for (uint8_t i = 0; i < loopOnCount; i++) {
    MIDI.sendNoteOff(loopOnNote[i], 0, loopOnCh[i]);
  }
  loopOnCount = 0;
}

void loopClear() {
  loopSilence();
  loopState = LOOP_IDLE;
  loopCount = 0; loopLen = 0; playHead = 0; loopMuted = false;
  markStateDirty();
}

// Record one event. Insertion-sorted by timestamp so overdubbed notes
// interleave correctly with the original pass on the next lap.
void loopRecord(uint8_t note, uint8_t vel, uint8_t ch, bool on) {
  if (loopState != LOOP_REC && loopState != LOOP_DUB) return;
  if (loopCount >= MAX_LOOP_EVENTS) return;

  uint32_t t = millis() - loopOrigin;
  if (loopLen) t %= loopLen;        // overdub wraps into the existing loop

  uint16_t pos = loopCount;
  while (pos > 0 && loopBuf[pos - 1].t > t) {
    loopBuf[pos] = loopBuf[pos - 1];
    pos--;
  }
  loopBuf[pos] = { t, note, vel, ch, on };
  loopCount++;
}

// L3 tap cycles the transport, exactly like a looper pedal:
//   idle -> arm (recording starts on your first note)
//   rec  -> close the loop and start playing
//   play -> overdub
//   dub  -> back to play
void loopAdvance() {
  switch (loopState) {
    case LOOP_IDLE:
      loopCount = 0; loopLen = 0; playHead = 0;
      loopState = LOOP_REC;
      loopOrigin = 0;               // 0 = still waiting for the first note
      break;
    case LOOP_REC:
      if (!loopCount) { loopState = LOOP_IDLE; break; }  // nothing played
      loopLen = millis() - loopOrigin;
      if (loopLen < 200) loopLen = 200;
      loopOrigin = millis();
      playHead = 0;
      loopState = LOOP_PLAY;
      break;
    case LOOP_PLAY: loopState = LOOP_DUB;  break;
    case LOOP_DUB:  loopState = LOOP_PLAY; break;
  }
  markStateDirty();
}

void loopTick() {
  if (loopState != LOOP_PLAY && loopState != LOOP_DUB) return;
  if (!loopLen) return;

  uint32_t pos = millis() - loopOrigin;
  if (pos >= loopLen) {             // wrap to the top of the loop
    loopOrigin += loopLen;
    pos -= loopLen;
    playHead = 0;
  }
  while (playHead < loopCount && loopBuf[playHead].t <= pos) {
    const LoopEvent &e = loopBuf[playHead];
    if (!loopMuted) {
      if (e.on) {
        MIDI.sendNoteOn(e.note, e.vel, e.ch);
        if (loopOnCount < sizeof(loopOnNote)) {
          loopOnNote[loopOnCount] = e.note;
          loopOnCh[loopOnCount]   = e.ch;
          loopOnCount++;
        }
      } else {
        MIDI.sendNoteOff(e.note, 0, e.ch);
        for (uint8_t i = 0; i < loopOnCount; i++) {
          if (loopOnNote[i] == e.note && loopOnCh[i] == e.ch) {
            loopOnNote[i] = loopOnNote[loopOnCount - 1];
            loopOnCh[i]   = loopOnCh[loopOnCount - 1];
            loopOnCount--;
            break;
          }
        }
      }
    }
    playHead++;
  }
}

void stopButton(uint8_t idx) {
  for (uint8_t n = 0; n < activeCount[idx]; n++) {
    MIDI.sendNoteOff(activeNotes[idx][n], 0, activeChannel[idx]);
    loopRecord(activeNotes[idx][n], 0, activeChannel[idx], false);
  }
  activeCount[idx] = 0;
}

void allNotesOff() {
  for (uint8_t i = 0; i < 8; i++) stopButton(i);
}

void switchMode(Mode m) {
  if (currentMode == m) return;
  allNotesOff();
  // Leaving chord mode: drop momentary modifier state so a held button
  // can't stick "on" after the mode changes underneath it.
  for (uint8_t i = 0; i < NUM_CHORD_TYPES; i++) typeHeld[i] = false;
  for (uint8_t i = 0; i < NUM_EXTENSIONS;  i++) extHeld[i]  = false;
  currentMode = m;
  markStateDirty();
}

// Home tap outside chord mode: cycle the variant for the current mode.
void cycleVariant() {
  if (currentMode == MODE_SCALE) {
    scaleIndex = (scaleIndex + 1) % NUM_SCALES;
    markStateDirty();
  }
}

// Publish the full controller state as CCs on channel 16. Cheap enough to
// resend wholesale rather than tracking per-field deltas.
void sendState() {
  uint8_t typeMask = 0, extMask = 0, typeLatchMask = 0, extLatchMask = 0;
  for (uint8_t i = 0; i < NUM_CHORD_TYPES; i++) {
    if (typeActive(i))  typeMask      |= (1 << i);
    if (typeLatched[i]) typeLatchMask |= (1 << i);
  }
  for (uint8_t i = 0; i < NUM_EXTENSIONS; i++) {
    if (extActive(i))  extMask      |= (1 << i);
    if (extLatched[i]) extLatchMask |= (1 << i);
  }
  uint8_t ovLo = 0, ovHi = 0;
  for (uint8_t i = 0; i < 4; i++) if (overrideActive[i])     ovLo |= (1 << i);
  for (uint8_t i = 0; i < 4; i++) if (overrideActive[i + 4]) ovHi |= (1 << i);

  // Which physical keys are held down right now.
  uint8_t pressLo = 0, pressHi = 0, pressDpad = 0, pressFn = 0;
  for (uint8_t i = 0; i < 4; i++) if (btnState[i])     pressLo   |= (1 << i);
  for (uint8_t i = 0; i < 4; i++) if (btnState[i + 4]) pressHi   |= (1 << i);
  for (uint8_t i = 0; i < 4; i++) if (dpadState[i])    pressDpad |= (1 << i);
  for (uint8_t i = 0; i < 4; i++) if (fnState[i])      pressFn   |= (1 << i);
  if (startState)  pressFn |= (1 << 4);
  if (selectState) pressFn |= (1 << 5);

  MIDI.sendControlChange(CC_MODE,      (uint8_t)currentMode,        CHANNEL_STATE);
  MIDI.sendControlChange(CC_SCALE,     scaleIndex,                  CHANNEL_STATE);
  MIDI.sendControlChange(CC_TYPES,     typeMask,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_EXTS,      extMask,                     CHANNEL_STATE);
  MIDI.sendControlChange(CC_OCTAVE,    (uint8_t)(octaveShift + 64),    CHANNEL_STATE);
  MIDI.sendControlChange(CC_TRANSPOSE, (uint8_t)(transposeShift + 64), CHANNEL_STATE);
  MIDI.sendControlChange(CC_KIT,       kitIndex,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_BANK,      currentBank,                 CHANNEL_STATE);
  MIDI.sendControlChange(CC_RECORD,    (uint8_t)recordState,        CHANNEL_STATE);
  MIDI.sendControlChange(CC_OVERRIDE_LO, ovLo,                     CHANNEL_STATE);
  MIDI.sendControlChange(CC_OVERRIDE_HI, ovHi,                     CHANNEL_STATE);
  MIDI.sendControlChange(CC_TYPELATCH, typeLatchMask,               CHANNEL_STATE);
  MIDI.sendControlChange(CC_EXTLATCH,  extLatchMask,                CHANNEL_STATE);
  MIDI.sendControlChange(CC_PRESS_LO,   pressLo,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_PRESS_HI,   pressHi,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_PRESS_DPAD, pressDpad,                  CHANNEL_STATE);
  MIDI.sendControlChange(CC_PRESS_FN,   pressFn,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_LOOP,      (uint8_t)loopState,          CHANNEL_STATE);
  MIDI.sendControlChange(CC_LOOPMUTE,  loopMuted ? 1 : 0,           CHANNEL_STATE);
  MIDI.sendControlChange(CC_LATCH,     latchMode ? 1 : 0,           CHANNEL_STATE);
  MIDI.sendControlChange(CC_VELOCITY,  velocity,                    CHANNEL_STATE);
  MIDI.sendControlChange(CC_SHIFT,     selectState ? 1 : 0,         CHANNEL_STATE);
  MIDI.sendControlChange(CC_CUSTBANK,  customBankIndex,             CHANNEL_STATE);

  stateDirty = false;
  lastStateSend = millis();
}

// ---- Persistence -------------------------------------------------------
// The RP2040 has no real EEPROM; the core emulates one in a flash sector.
// Writing flash stalls the CPU for milliseconds, so we never write from the
// hot path: changes set a dirty flag and are committed once the player has
// been idle for a moment. That keeps note timing clean and avoids burning
// flash cycles on every knob nudge.
const uint32_t SAVE_MAGIC   = 0x46424D34;  // "FBM4"
const uint16_t SAVE_ADDR    = 0;
const unsigned long SAVE_DEBOUNCE_MS = 2500;

struct Persist {
  uint32_t magic;
  uint8_t  scaleIndex, kitIndex, currentBank, customBankIndex;
  int8_t   octaveShift, transposeShift;
  uint8_t  velocity, mode;
  Slot     banks[NUM_CUSTOM_BANKS][8];
  bool     ovActive[8];
  uint8_t  ovNotes[8][MAX_CHORD_NOTES];
  uint8_t  ovCount[8], ovChannel[8];
};

bool settingsDirty = false;
unsigned long lastChangeAt = 0;
unsigned long savedFlashAt = 0;   // brief on-screen 'saved' confirmation

void markSettingsDirty() { settingsDirty = true; lastChangeAt = millis(); }

void saveSettings() {
  Persist p;
  p.magic = SAVE_MAGIC;
  p.scaleIndex = scaleIndex;   p.kitIndex = kitIndex;
  p.currentBank = currentBank; p.customBankIndex = customBankIndex;
  p.octaveShift = octaveShift; p.transposeShift = transposeShift;
  p.velocity = velocity;
  p.mode = (uint8_t)currentMode;
  memcpy(p.banks, customBank, sizeof(customBank));
  for (uint8_t i = 0; i < 8; i++) {
    p.ovActive[i]  = overrideActive[i];
    p.ovCount[i]   = overrideCount[i];
    p.ovChannel[i] = overrideChannel[i];
    for (uint8_t n = 0; n < MAX_CHORD_NOTES; n++)
      p.ovNotes[i][n] = (uint8_t)overrideNotes[i][n];
  }
  EEPROM.put(SAVE_ADDR, p);
  EEPROM.commit();
  settingsDirty = false;
  savedFlashAt = millis();
  MIDI.sendControlChange(CC_SAVED, 1, CHANNEL_STATE);
}

void loadSettings() {
  Persist p;
  EEPROM.get(SAVE_ADDR, p);
  if (p.magic != SAVE_MAGIC) return;   // never saved, or layout changed
  scaleIndex      = p.scaleIndex % NUM_SCALES;
  kitIndex        = p.kitIndex % NUM_KITS;
  currentBank     = p.currentBank % NUM_BANKS;
  customBankIndex = p.customBankIndex % NUM_CUSTOM_BANKS;
  octaveShift     = constrain(p.octaveShift, -4, 4);
  transposeShift  = constrain(p.transposeShift, -12, 12);
  velocity        = constrain(p.velocity, 1, 127);
  if (p.mode <= (uint8_t)MODE_DRUM_GM) {
    currentMode = (Mode)p.mode;
    for (uint8_t i = 0; i < NUM_MELODIC; i++)
      if (MELODIC_CYCLE[i] == currentMode) melodicIndex = i;
  }
  memcpy(customBank, p.banks, sizeof(customBank));
  for (uint8_t i = 0; i < 8; i++) {
    overrideActive[i]  = p.ovActive[i];
    overrideCount[i]   = (p.ovCount[i] > MAX_CHORD_NOTES) ? 0 : p.ovCount[i];
    overrideChannel[i] = p.ovChannel[i] ? p.ovChannel[i] : 1;
    for (uint8_t n = 0; n < MAX_CHORD_NOTES; n++)
      overrideNotes[i][n] = p.ovNotes[i][n];
  }
}

// Reset every stored bank and setting back to the compiled-in defaults.
void factoryReset() {
  Persist blank; memset(&blank, 0, sizeof(blank));
  EEPROM.put(SAVE_ADDR, blank);   // wipe the magic so load() ignores it
  EEPROM.commit();
}

// The screen mirrors the monitor page: what mode you are in, the setting
// that matters for that mode, and any modifier that is currently engaged.
// Held-SELECT overlay: draws the actual board layout with each key
// labelled by what it does in the current mode, the way GP2040-CE drew its
// button map. Keys fill solid while physically held, so it doubles as an
// input tester.
//
// 128x64 is tight: labels are at most 3 characters (6px per char at text
// size 1), boxes are 15x13 for the main grid and the d-pad.

void keyBox(int16_t x, int16_t y, int16_t w, int16_t h,
            const char *label, bool pressed) {
  if (pressed) {
    display.fillRoundRect(x, y, w, h, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.drawRoundRect(x, y, w, h, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
  }
  // centre the label in the box
  int16_t len = strlen(label);
  int16_t tx = x + (w - len * 6) / 2 + 1;
  int16_t ty = y + (h - 7) / 2;
  display.setCursor(tx, ty);
  display.print(label);
  display.setTextColor(SSD1306_WHITE);
}

// Short label for main key idx in the current mode.
void mainKeyLabel(uint8_t idx, char *out) {
  if (overrideActive[idx]) { strcpy(out, "SAV"); return; }

  switch (currentMode) {
    case MODE_CHORD: {
      // roman-ish degree number is the most useful thing here
      out[0] = '1' + idx; out[1] = 0;
      return;
    }
    case MODE_CUSTOM: {
      static const char *BEAT[8] = {"BD","SD","CH","OH","CP","RS","RD","CR"};
      static const char *PERC[8] = {"TM","CB","MR","GR","CG","LC","MC","CL"};
      if (customBankIndex == 0)      { strcpy(out, BEAT[idx]); return; }
      else if (customBankIndex == 3) { strcpy(out, PERC[idx]); return; }
      // pitched banks: show the note letter
      const Slot &sl = customBank[customBankIndex][idx];
      static const char *NN[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      strcpy(out, NN[sl.n[0] % 12]);
      return;
    }
    case MODE_DRUM_GM: {
      static const char *D0[8] = {"BD","SD","CH","OH","LT","MT","CR","RD"};
      static const char *D1[8] = {"RS","CP","PH","FT","HT","CN","SP","BL"};
      static const char *D2[8] = {"CB","VS","HB","LB","MC","OC","LC","MR"};
      const char **t = (currentBank == 0) ? D0 : (currentBank == 1) ? D1 : D2;
      strcpy(out, t[idx]);
      return;
    }
    default: {
      // Chromatic / Scale: the note name
      int notes[MAX_CHORD_NOTES]; uint8_t ch;
      uint8_t n = effectiveNotes(idx, notes, ch);
      static const char *NN[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
      if (n) strcpy(out, NN[notes[0] % 12]); else strcpy(out, "-");
      return;
    }
  }
}

void drawHelp() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // --- header: mode + the setting that matters ---
  display.setCursor(0, 0);
  switch (currentMode) {
    case MODE_CHROMATIC: display.print("CHROMA"); break;
    case MODE_SCALE:     display.print("SCALE ");
                         display.print(SCALE_NAMES[scaleIndex]); break;
    case MODE_CHORD: {
      display.print("CHORD ");
      bool any = false;
      for (uint8_t t = 0; t < NUM_CHORD_TYPES; t++)
        if (typeActive(t)) { display.print(CHORD_TYPE_NAMES[t]); any = true; break; }
      if (!any) display.print("diat");
      for (uint8_t e = 0; e < NUM_EXTENSIONS; e++)
        if (extActive(e)) { display.print("+"); display.print(EXT_NAMES[e]); }
      break;
    }
    case MODE_CUSTOM:    display.print("CUSTOM ");
                         display.print(CUSTOM_BANK_NAMES[customBankIndex]); break;
    case MODE_DRUM_GM:   display.print("DRUM ");
                         display.print(KIT_NAMES[kitIndex]); break;
  }

  // --- d-pad cluster, lower left, staggered like the real board ---
  const char *dl[4];   // Up, Down, Left, Right
  // Two characters max: a 16px box fits 2 glyphs (6px each) with padding.
  if (inChordMode())      { dl[0]="Mj"; dl[1]="Mn"; dl[2]="Su"; dl[3]="Dm"; }
  else if (inCustom())    { dl[0]="O+"; dl[1]="O-"; dl[2]="B-"; dl[3]="B+"; }
  else if (drumGM())      { dl[0]="B+"; dl[1]="B-"; dl[2]="K-"; dl[3]="K+"; }
  else                    { dl[0]="O+"; dl[1]="O-"; dl[2]="S-"; dl[3]="S+"; }

  keyBox( 0, 14, 16, 13, dl[2], dpadState[2]);   // LEFT
  keyBox(17, 23, 16, 13, dl[1], dpadState[1]);   // DOWN
  keyBox(34, 32, 16, 13, dl[3], dpadState[3]);   // RIGHT
  keyBox( 0, 48, 33, 13, dl[0], dpadState[0]);   // UP (wide thumb key)

  // --- main 8 keys, two staggered rows on the right ---
  char lab[6];
  for (uint8_t i = 0; i < 4; i++) {              // punches
    mainKeyLabel(i, lab);
    keyBox(56 + i * 18, 14 + i * 2, 17, 13, lab, btnState[i]);
  }
  for (uint8_t i = 0; i < 4; i++) {              // kicks
    mainKeyLabel(i + 4, lab);
    keyBox(52 + i * 18, 31 + i * 2, 17, 13, lab, btnState[i + 4]);
  }

  // --- function row reminder along the bottom right ---
  display.setCursor(52, 55);
  // x=52 leaves room for 12 characters (52 + 12*6 = 124 < 128).
  if (inChordMode())   display.print("L3+6 R3m7 H9");
  else if (inCustom()) display.print("hold HM:rec");
  else if (drumGM())   display.print("R3 to melody");
  else                 display.print("L3 lp R3 drm");

  display.display();
}

void drawScreen() {
  if (!displayOk) return;
  if (millis() - lastDraw < DRAW_INTERVAL_MS) return;
  lastDraw = millis();

  // Holding Select shows the cheat sheet instead of the status readout.
  if (selectState) { drawHelp(); return; }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // --- line 1: mode, big ---
  display.setTextSize(2);
  display.setCursor(0, 0);
  switch (currentMode) {
    case MODE_CHROMATIC: display.print("CHROMA"); break;
    case MODE_SCALE:     display.print("SCALE");  break;
    case MODE_CHORD:     display.print("CHORD");  break;
    case MODE_CUSTOM:    display.print("CUSTOM"); break;
    case MODE_DRUM_GM:   display.print("DRUMS");  break;
  }

  // --- line 2: the detail that matters for this mode ---
  display.setTextSize(1);
  display.setCursor(0, 20);
  if (currentMode == MODE_SCALE) {
    display.print(SCALE_NAMES[scaleIndex]);
  } else if (currentMode == MODE_CHORD) {
    bool any = false;
    for (uint8_t t = 0; t < NUM_CHORD_TYPES; t++) {
      if (typeActive(t)) { display.print(CHORD_TYPE_NAMES[t]); any = true; break; }
    }
    if (!any) display.print("diatonic");
    for (uint8_t e = 0; e < NUM_EXTENSIONS; e++) {
      if (extActive(e)) { display.print(" +"); display.print(EXT_NAMES[e]); }
    }
  } else if (currentMode == MODE_CUSTOM) {
    display.print("bank ");
    display.print(CUSTOM_BANK_NAMES[customBankIndex]);
  } else if (currentMode == MODE_DRUM_GM) {
    display.print(KIT_NAMES[kitIndex]);
    display.print(" / ");
    display.print(BANK_NAMES[currentBank]);
  } else {
    display.print("C4 + semitones");
  }

  // --- line 3: octave / transpose / velocity ---
  display.setCursor(0, 32);
  display.print("oct ");
  if (octaveShift >= 0) display.print("+");
  display.print(octaveShift);
  display.print("  semi ");
  if (transposeShift >= 0) display.print("+");
  display.print(transposeShift);
  display.print("  v");
  display.print(velocity);

  // --- line 4: whatever is engaged right now ---
  display.setCursor(0, 44);
  if (recordState == RECORD_WAIT_SOURCE)      display.print("REC: pick source");
  else if (recordState == RECORD_WAIT_TARGET) display.print("REC: pick target");
  else {
    if (loopState == LOOP_REC)       display.print("LOOP rec ");
    else if (loopState == LOOP_PLAY) display.print(loopMuted ? "LOOP mute " : "LOOP play ");
    else if (loopState == LOOP_DUB)  display.print("LOOP dub ");
    if (latchMode)   display.print("LATCH ");
    if (selectState) display.print("SHIFT");
  }

  // --- brief save confirmation, top right ---
  if (millis() - savedFlashAt < 1200) {
    display.setTextSize(1);
    display.setCursor(98, 0);
    display.print("SAVE");
  }

  // --- line 5: which keys hold a recorded sound ---
  display.setCursor(0, 56);
  bool anySaved = false;
  for (uint8_t i = 0; i < 8; i++) {
    if (overrideActive[i]) {
      if (!anySaved) { display.print("saved:"); anySaved = true; }
      display.print(" ");
      display.print(BTN_NAMES[i]);
    }
  }

  display.display();
}

void setup() {
  for (uint8_t i = 0; i < 8; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);
  pinMode(PIN_UP,     INPUT_PULLUP);
  pinMode(PIN_DOWN,   INPUT_PULLUP);
  pinMode(PIN_LEFT,   INPUT_PULLUP);
  pinMode(PIN_RIGHT,  INPUT_PULLUP);
  pinMode(PIN_START,  INPUT_PULLUP);
  pinMode(PIN_SELECT, INPUT_PULLUP);
  pinMode(PIN_L3,     INPUT_PULLUP);
  pinMode(PIN_R3,     INPUT_PULLUP);
  pinMode(PIN_TURBO,  INPUT_PULLUP);
  pinMode(PIN_HOME,   INPUT_PULLUP);

  usb_midi.setStringDescriptor("FightingBox MIDI");
  MIDI.begin(MIDI_CHANNEL_OMNI);
  while (!TinyUSBDevice.mounted()) delay(1);

  EEPROM.begin(4096);
  loadSettings();   // restore banks, overrides and settings from flash

  // Bring the OLED up only AFTER USB is enumerated: a blocking display
  // init before enumeration stalls the host handshake and makes the whole
  // device undetectable.
  Wire1.setSDA(OLED_SDA);
  Wire1.setSCL(OLED_SCL);
  Wire1.setClock(400000);
  Wire1.begin();

  // Probe for an ACK first - begin() can block much longer on a dead bus.
  Wire1.beginTransmission(OLED_ADDR);
  if (Wire1.endTransmission() == 0) {
    displayOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  }
  if (displayOk) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 12);
    display.println("FIGHTING");
    display.println("BOX MIDI");
    display.display();
    delay(700);
  }

  sendState();
}

void loop() {
  MIDI.read();

  // ---- Start: cycle melodic modes ----
  // Start: TAP cycles melodic mode, HOLD toggles latch (sustain) mode.
  // SELECT + START = factory reset: wipe saved settings and restore the
  // compiled-in banks. Deliberately a two-key chord so it cannot be hit
  // by accident.
  if (readDebounced(PIN_START, startState, startLastRaw, startLastChange)) {
    if (startState && selectState) {
      factoryReset();
      shiftUsed = true;
      allNotesOff();
      loopClear();
      for (uint8_t b = 0; b < 8; b++) overrideActive[b] = false;
      octaveShift = 0; transposeShift = 0; velocity = 100;
      scaleIndex = 0; customBankIndex = 0; kitIndex = 0; currentBank = 0;
      latchMode = false;
      settingsDirty = false;   // don't immediately re-save what we wiped
      markStateDirty();
      return;                  // skip the normal Start handling this pass
    }
    if (startState) {
      startPressStart = millis();
      startLongFired = false;
    } else if (!startLongFired) {
      if (inMelodicMode()) melodicIndex = (melodicIndex + 1) % NUM_MELODIC;
      switchMode(MELODIC_CYCLE[melodicIndex]);
    }
    markStateDirty(); // light the key up on press AND release
  }
  if (startState && !startLongFired &&
      (millis() - startPressStart >= HOME_RESET_HOLD_MS)) {
    latchMode = !latchMode;
    if (!latchMode) allNotesOff();   // dropping latch releases held notes
    startLongFired = true;
    markStateDirty();
  }

  // ---- Select: MIDI panic ----
  // Select: HOLD = shift layer (see d-pad). A quick TAP with no shift
  // action used = panic. Holding it never panics, so the shift layer is
  // safe to explore.
  if (readDebounced(PIN_SELECT, selectState, selectLastRaw, selectLastChange)) {
   markStateDirty();
   if (selectState) {
     selectPressStart = millis();
     shiftUsed = false;
   } else if (!shiftUsed && (millis() - selectPressStart < HOME_RESET_HOLD_MS)) {
    for (uint8_t ch = 1; ch <= 16; ch++) MIDI.sendControlChange(123, 0, ch);
    allNotesOff();
    loopSilence();
    octaveShift = 0;
    transposeShift = 0;
    for (uint8_t i = 0; i < 4; i++) dpadTempApplied[i] = false;
    clearAllLatches();
    latchMode = false;
    markStateDirty();
   }
  }

  // ---- L3 / R3 / Turbo / Home: mode buttons, or chord extensions ----
  for (uint8_t i = 0; i < 4; i++) {
    bool edge = readDebounced(FN_PINS[i], fnState[i], fnLastRaw[i], fnLastChange[i]);
    bool pressed = fnState[i];

    if (inChordMode()) {
      // Extension: active while held; a short tap toggles its latch.
      //
      // Turbo and Home are still needed for record / wipe while in chord
      // mode, otherwise you can never record a chord (the bug this fixes).
      // So on those two, a LONG hold escapes to their global function and
      // cancels the extension it would otherwise have applied.
      if (edge) {
        if (pressed) {
          fnPressStart[i] = millis();
          fnLongFired[i] = false;
          extHeld[i] = true;
          markStateDirty();
        } else {
          extHeld[i] = false;
          // A long hold already did its global job - don't also latch.
          if (!fnLongFired[i] && millis() - fnPressStart[i] < HOLD_THRESHOLD_MS) {
            extLatched[i] = !extLatched[i];
          }
          fnLongFired[i] = false;
          markStateDirty();
        }
      }

      if (pressed && !fnLongFired[i] &&
          (millis() - fnPressStart[i] >= HOME_RESET_HOLD_MS)) {
        if (i == 3) {          // Home held -> start/cancel record flow
          extHeld[i] = false;  // this press is not an extension
          recordState = (recordState == RECORD_IDLE) ? RECORD_WAIT_SOURCE : RECORD_IDLE;
          fnLongFired[i] = true;
          markStateDirty();
        }
      }
      continue;
    }

    // Normal (non-chord) behavior.
    if (edge) markStateDirty(); // key light on press and release
    switch (i) {
      case 0: // L3 -> looper transport: tap advances, hold clears
        if (edge) {
          if (pressed) {
            fnPressStart[i] = millis();
            fnLongFired[i] = false;
          } else if (!fnLongFired[i]) {
            loopAdvance();
          }
        }
        if (pressed && !fnLongFired[i] &&
            (millis() - fnPressStart[i] >= HOME_RESET_HOLD_MS)) {
          loopClear();
          fnLongFired[i] = true;
        }
        break;

      case 1: // R3 -> toggle real GM drums / back to the last melodic mode
        if (edge && pressed) {
          switchMode(drumGM() ? MELODIC_CYCLE[melodicIndex] : MODE_DRUM_GM);
        }
        break;

      case 2: // Turbo -> dead key on this board, nothing to do.
        break;

      case 3: // Home -> tap: cycle variant · hold: RECORD (Turbo is dead)
              //         Select+Home: wipe all recorded overrides
        if (edge && pressed && selectState) {
          for (uint8_t b = 0; b < 8; b++) overrideActive[b] = false;
          recordState = RECORD_IDLE;
          shiftUsed = true;
          fnHomeResetFired = true;   // don't also cycle the variant
          markStateDirty();
          break;
        }
        if (edge) {
          if (pressed) {
            fnPressStart[i] = millis();
            fnHomeResetFired = false;
          } else {
            if (!fnHomeResetFired) cycleVariant();
            fnHomeResetFired = false;
          }
        }
        if (pressed && !fnHomeResetFired &&
            (millis() - fnPressStart[i] >= HOME_RESET_HOLD_MS)) {
          recordState = (recordState == RECORD_IDLE) ? RECORD_WAIT_SOURCE : RECORD_IDLE;
          fnHomeResetFired = true;
          markStateDirty();
        }
        break;
    }
  }

  // ---- D-pad: pitch, chord types, or drum banks depending on mode ----
  bool pitchMode = (currentMode == MODE_CHROMATIC ||
                    currentMode == MODE_SCALE);

  for (uint8_t i = 0; i < 4; i++) {
    bool edge = readDebounced(DPAD_PINS[i], dpadState[i], dpadLastRaw[i], dpadLastChange[i]);
    bool pressed = dpadState[i];

    if (inChordMode()) {
      // Chord type: active while held; a short tap toggles its latch.
      uint8_t t = DPAD_TO_TYPE[i];
      if (edge) {
        if (pressed) {
          dpadPressStart[i] = millis();
          typeHeld[t] = true;
          markStateDirty();
        } else {
          typeHeld[t] = false;
          if (millis() - dpadPressStart[i] < HOLD_THRESHOLD_MS) {
            typeLatched[t] = !typeLatched[t];
          }
          markStateDirty();
        }
      }

    } else if (selectState) {
      // ---- SHIFT LAYER (Select held) ----
      // Up/Down nudge velocity, Left mutes the loop, Right clears it.
      if (edge && pressed) {
        shiftUsed = true;
        switch (i) {
          case 0: velocity = (velocity <= 117) ? velocity + 10 : 127; break;
          case 1: velocity = (velocity >=  11) ? velocity - 10 : 1;   break;
          case 2: loopMuted = !loopMuted; if (loopMuted) loopSilence(); break;
          case 3: loopClear(); break;
        }
        markStateDirty();
      }
      if (edge) markStateDirty();

    } else if (inCustom()) {
      // Left/Right swap banks; Up/Down still shift octave for pitched slots.
      if (edge) markStateDirty();
      if (edge && pressed) {
        if (i == 2) {
          allNotesOff();
          customBankIndex = (customBankIndex + NUM_CUSTOM_BANKS - 1) % NUM_CUSTOM_BANKS;
        } else if (i == 3) {
          allNotesOff();
          customBankIndex = (customBankIndex + 1) % NUM_CUSTOM_BANKS;
        } else {
          applyStep(i);   // Up/Down = octave
        }
        markStateDirty();
      }

    } else if (pitchMode) {
      if (edge) markStateDirty();
      if (edge) {
        if (pressed) {
          dpadPressStart[i] = millis();
          dpadTempApplied[i] = false;
        } else {
          if (dpadTempApplied[i]) {
            revertStep(i, dpadTempValue[i]);
            dpadTempApplied[i] = false;
          } else {
            applyStep(i); // tap -> permanent
          }
          markStateDirty();
        }
      }
      if (pressed && !dpadTempApplied[i] &&
          (millis() - dpadPressStart[i] >= HOLD_THRESHOLD_MS)) {
        dpadTempValue[i] = applyStep(i); // hold -> temporary
        dpadTempApplied[i] = true;
        markStateDirty();
      }

    } else if (currentMode == MODE_DRUM_GM) {
      if (edge) markStateDirty();
      if (edge && pressed) {
        switch (i) {
          case 0: currentBank = (currentBank + 1) % NUM_BANKS; break;
          case 1: currentBank = (currentBank + NUM_BANKS - 1) % NUM_BANKS; break;
          case 2:
            kitIndex = (kitIndex + NUM_KITS - 1) % NUM_KITS;
            MIDI.sendProgramChange(KIT_PROGRAMS[kitIndex], CHANNEL_DRUM_GM);
            break;
          case 3:
            kitIndex = (kitIndex + 1) % NUM_KITS;
            MIDI.sendProgramChange(KIT_PROGRAMS[kitIndex], CHANNEL_DRUM_GM);
            break;
        }
        markStateDirty();
      }
    }
  }

  // ---- Main 8 buttons ----
  for (uint8_t i = 0; i < 8; i++) {
    if (readDebounced(BTN_PINS[i], btnState[i], btnLastRaw[i], btnLastChange[i])) {
      markStateDirty(); // key light on press and release
      if (btnState[i]) {
        // --- pressed ---
        if (recordState == RECORD_WAIT_SOURCE) {
          uint8_t ch;
          recordCount = effectiveNotes(i, recordNotes, ch);
          recordChannel = ch;
          recordState = RECORD_WAIT_TARGET;
          btnSuppressed[i] = true;
          markStateDirty();

        } else if (recordState == RECORD_WAIT_TARGET) {
          if (inCustom()) {
            // Custom mode: burn the captured sound into THIS bank's slot,
            // so each of the four banks keeps its own edits and cycling
            // banks swaps the whole set.
            Slot &sl = customBank[customBankIndex][i];
            sl.count = (recordCount > 4) ? 4 : recordCount;
            sl.ch    = recordChannel;
            for (uint8_t n = 0; n < sl.count; n++) sl.n[n] = (uint8_t)recordNotes[n];
          } else {
            overrideActive[i]  = true;
            overrideCount[i]   = recordCount;
            overrideChannel[i] = recordChannel;
            for (uint8_t n = 0; n < recordCount; n++) overrideNotes[i][n] = recordNotes[n];
          }
          recordState = RECORD_IDLE;
          btnSuppressed[i] = true;
          markStateDirty();

        } else if (latchMode && activeCount[i]) {
          // Latch mode: this key is already sounding, so the press stops it.
          stopButton(i);
          btnSuppressed[i] = true;

        } else {
          int notes[MAX_CHORD_NOTES];
          uint8_t ch;
          uint8_t count = effectiveNotes(i, notes, ch);
          stopButton(i); // safety: never leave a previous voice hanging
          activeChannel[i] = ch;
          activeCount[i] = count;
          for (uint8_t n = 0; n < count; n++) {
            activeNotes[i][n] = notes[n];
            MIDI.sendNoteOn(notes[n], velocity, ch);
            // First note of an armed loop defines t=0 - no dead air.
            if (loopState == LOOP_REC && !loopOrigin) loopOrigin = millis();
            loopRecord(notes[n], velocity, ch, true);
          }
          btnSuppressed[i] = false;
        }
      } else {
        // --- released --- (in latch mode the note keeps sounding)
        if (!btnSuppressed[i] && !latchMode) stopButton(i);
        btnSuppressed[i] = false;
      }
    }
  }

  loopTick();
  drawScreen();

  // Autosave: commit only after things have settled, and never mid-note.
  if (settingsDirty && (millis() - lastChangeAt > SAVE_DEBOUNCE_MS)) {
    bool anyHeld = false;
    for (uint8_t i = 0; i < 8; i++) if (btnState[i]) anyHeld = true;
    if (!anyHeld && loopState != LOOP_REC) saveSettings();
  }

  // Publish state when it changed, plus a slow heartbeat so a monitor
  // opened after the fact still syncs without touching the controller.
  if (stateDirty || (millis() - lastStateSend >= STATE_HEARTBEAT_MS)) {
    sendState();
  }

#ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
#endif
}
