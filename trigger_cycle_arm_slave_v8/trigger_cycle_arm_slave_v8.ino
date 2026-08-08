/******************************************************************************
  Trigger-Cycle Slave (arm side) v8 -- responsiveness overhaul + line
  protocol for the Web Serial console. The arm can now be driven by the
  browser app (sliders, demo buttons, pose commands) as well as the glove,
  with explicit arbitration over who owns it.

  WHAT CHANGED FROM v7:
    - KILLED THE LAG. The old BT read (BT.readStringUntil) blocked the whole
      loop for up to 1000ms whenever a byte arrived without a newline --
      which happened constantly (SoftwareSerial mangles bytes; a floating
      RX pin generates phantom bytes even with the glove off). Replaced
      with a non-blocking char-buffer accumulator: bytes are drained as
      they arrive, parsing only happens when a full line is present, and
      the loop NEVER waits. String usage in the hot path is gone too (heap
      churn on a 2KB Uno).
    - delay(SLEW_DELAY_MS) in the RUN easing removed -- slew pacing is now
      millis()-based, so easing no longer throttles command handling.
    - Serial.begin bumped 9600 -> 115200 (USB only; BT stays 9600 for the
      HC-05). Set your console/monitor to match.
    - NEW LINE PROTOCOL on USB serial, alongside the old single-char keys
      (both work; a 1-char line is treated as a legacy key):
        J <idx> <deg>          set one joint target (0=Sh 1=El 2=W1 3=W2 4=Gr)
        P <s> <e> <w1> <w2> <g>  set full 5-joint pose
        D <id>                 run demo 0..7 (c w b p a e s f order)
        M <0|1>                control owner: 0=glove drives, 1=app drives
        H                      stop/hold (same as 'h')
        T <0|1>                telemetry stream off/on
        L                      print joint limits (machine-readable)
        Z <ms>                 set the pre-demo "get ready" delay (default 2000)
    - CONTROL ARBITRATION: controlOwner decides who drives. When the app
      owns (M 1), glove packets are still parsed (so link status stays
      honest) but their motion is ignored -- no more glove overwriting
      slider targets 20x/sec. When the glove owns (M 0, default), J/P
      commands auto-claim ownership for the app so a slider drag Just Works.
    - TELEMETRY: with T 1, emits "# owner sh el w1 w2 gr base" every 100ms.
      Lines start with '#' so the console can split telemetry from logs.
      The human-readable 500ms status spam is suppressed while telemetry
      is on (it's redundant and floods the console log).
    - linkStale freeze now only applies when the GLOVE owns control --
      previously it would also have frozen app-commanded easing.

  WHAT CHANGED FROM v6:
    - Finished the Nod demo. The constants (nodShoulder/nodElbow/NOD_REPS/
      NOD_AMPLITUDE_DEG/NOD_SETTLE_MS/NOD_QUICK_MS) already existed in v6
      but nothing ever built the actual step sequence or wired it to a key.
      Added startNodShow() and bound it to 'a'. nodShoulder/nodElbow are now
      set to your measured values (-5 / 60) instead of the old placeholder
      (which just copied the generic start-center constants).
    - Added Square demo ('e'): eases through 4 corner poses (Shoulder/Elbow/
      Wrist1/Wrist2/Gripper each) in order, using the same smoothstep easing
      as every other show. Corner poses and per-edge travel time are TODO --
      see the placeholders below, fill in before this demo will look like
      anything other than sitting still.
    - Added Coordinate Preset 1 ('s') and Preset 2 ('f'): each first eases
      Shoulder/Elbow/Wrist1 into a held pose, then runs Wrist2 through a
      solo current->max->min sweep, then Gripper through N solo min->max->
      min cycles (2x for Preset 1, 1x for Preset 2, slightly faster than the
      Wrist2 sweep), then finishes with Wrist2 easing home from min while
      Gripper does one more min->max->min cycle at the same time.
    - Direct-select keys now: c/w/b/p (existing 4) + a/e/s/f (new 4). Key
      'd' was intentionally left alone (still "capture Full Down pose") and
      'w' was left alone (still Wave Cascade) -- both were already taken,
      so the new demos got free keys instead of colliding with them.
    - NUM_DEMOS-style cycling ('g') was NOT extended to the new demos --
      only the original 4 (Calib Sweep/Wave/Breathe/Pose Cycle) are in that
      rotation, matching the request that these 4 new ones be direct-key-only.

  WHAT CHANGED FROM v5 (v5 in old comments below, kept for history):
    - New joint: Base (stepper). Lives entirely alongside the 5 servo
      joints -- same live-control pattern (Mode 5 packets from the glove
      move targetPosBase), same JOG mode (key '6'), same "hold still
      unless my mode is active" behavior, same STALE_MS link-loss freeze.
    - NOT wired into the demo choreography (Calib Sweep / Wave / Breathe /
      Pose Cycle) yet on purpose -- see the note above queueDemo(). It
      just holds its last commanded position while a demo plays, same as
      it would if you simply weren't gesturing. Easy to add later; see
      the comment there for how.
    - Base uses SOFT LIMITS in steps (jointMinBase/jointMaxBase), not a
      full 360-degree spin -- because the servo wiring and BT antenna for
      the rest of the arm physically ride on top of this joint and would
      wind up on an unlimited rotation. Widen the limits only if you add
      a slip ring.

  WHAT CHANGED FROM v4 (older, kept for history):
    - All 5 joints are now pre-loaded with your measured-safe ranges --
      including Gripper (60 to 173 deg), which was still a placeholder in
      v4. Nothing needs to be jogged before demos will run anymore.
    - Pressing a demo key (c/w/b/p/g) no longer starts movement instantly.
      It waits DEMO_START_DELAY_MS (2s) doing nothing, then begins -- a
      "get ready" beat before the arm moves, same for every demo.
    - Calibration Sweep now holds still for 1s after each joint returns
      to center, before the next joint's motion starts.
    - Every motion duration in Calib Sweep / Breathe / Pose Cycle is
      roughly DOUBLED (half the old speed), and the Wave Cascade's travel
      + stagger timing scaled up to match -- slower and smoother overall,
      leaning on the existing smoothstep easing for a calm, deliberate
      feel rather than a snappy/robotic one.

  Everything else from v4 is unchanged: demos are still purely
  serial-controlled (glove never triggers them), direct-select keys
  still work, and a live gesture still interrupts a running demo
  immediately and hands control back to your hand.

  GESTURE-DIRECTION NOTES (as you described them -- see INVERT_* below):
  I didn't set these based on your notes because "left/right/up/down"
  depends on how your servo horns are physically mounted, and I can't
  verify that from text. Test live, and flip the specific INVERT_* to
  true if that one joint moves opposite to what you want.

  ============================================================================
  WORKFLOW:
  ============================================================================
    1. Flash this. Open Serial Monitor @ 9600.
    2. Jog the Gripper only: '5' to enter, '+'/'-'/'>'/'<' to move,
       'n'/'x' to mark min/max, 'q' to exit. The other four joints are
       already loaded with your safe values -- no need to touch them
       unless you want to re-measure.
    3. Press 'c', 'w', 'b', or 'p' any time to play a specific demo, or
       'g' to just step through them one at a time. 'h' stops instantly.
    4. Turn on the glove whenever you want live control back -- any
       gesture mode (1-4) takes over immediately, demo or not.
******************************************************************************/

#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_PWMServoDriver.h>
#include <AccelStepper.h> // install "AccelStepper" by Mike McCauley via Library Manager

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(0x40);
SoftwareSerial BT(10, 11); // RX, TX -- from glove master board

// ---- NEMA17 base stepper (via A4988/DRV8825-style driver board) ----
const int BASE_STEP_PIN = 8;
const int BASE_DIR_PIN  = 9;
const int BASE_EN_PIN   = 7;  // driver ENABLE, active LOW (LOW = enabled)
AccelStepper baseStepper(AccelStepper::DRIVER, BASE_STEP_PIN, BASE_DIR_PIN);

// Set this to match the driver's microstep jumpers (MS1/MS2/MS3).
// 1 = full step (200 steps/rev), 2 = half, 4 = quarter, 8 = eighth, 16 = sixteenth.
const int BASE_MICROSTEP_MULT = 4;
const float BASE_STEPS_PER_DEG = (200.0 * BASE_MICROSTEP_MULT) / 360.0;

#define SERVOMIN 125
#define SERVOMAX 575
#define SERVO_FREQ 50

// ---- Channel assignment (fixed wiring, don't change) ----
const int CH_SHOULDER_L = 0;
const int CH_SHOULDER_R = 1;
const int CH_ELBOW      = 2;
const int CH_WRIST1     = 3;
const int CH_WRIST2     = 4;
const int CH_GRIPPER    = 5;

const int L_CENTER = 125;
const int R_CENTER = 120;

// ---- Starting positions for the five joints under live control ----
const int shoulderStartCenter = 0;   // delta
const int elbowStartCenter    = 90;  // degrees
const int wrist1StartCenter   = 25;  // degrees
const int wrist2StartCenter   = 90;  // degrees
const int gripperStartCenter  = 122; // degrees
const int baseStartCenter     = 0;   // steps -- "straight ahead"

// ---- Nod demo: shoulder+elbow settle into a pose, then wrist1 does a
// quick, noticeable back-and-forth like a real head nod. Measured values --
// tune further if the pose doesn't read as "looking at someone" in person.
const int nodShoulder = -5;
const int nodElbow    = 60;
const int NOD_REPS = 3;                 // how many dips
const int NOD_AMPLITUDE_DEG = 20;       // how far wrist1 swings each way -- tune to taste
const unsigned long NOD_SETTLE_MS = 900;  // shoulder/elbow moving into pose
const unsigned long NOD_QUICK_MS  = 180;  // each half-nod -- keep this short, that's what makes it read as a "nod" not a "wave"

// ---- Square demo: 4 corner poses, eased through in order with the same
// step-queue engine as Calib Sweep/Breathe/Pose Cycle. Base can't rotate
// (parked per your call), so this square is drawn entirely with Shoulder/
// Elbow/Wrist1/Wrist2 motion, gripper held as the "pen".
// TODO -- PLACEHOLDERS: all 4 corners currently just copy start-center, so
// the arm will sit still. Fill in real corner angles before using 'e'.
int squareCorner1Shoulder = shoulderStartCenter, squareCorner1Elbow = elbowStartCenter,
    squareCorner1Wrist1 = wrist1StartCenter, squareCorner1Wrist2 = wrist2StartCenter, squareCorner1Gripper = gripperStartCenter;
int squareCorner2Shoulder = shoulderStartCenter, squareCorner2Elbow = elbowStartCenter,
    squareCorner2Wrist1 = wrist1StartCenter, squareCorner2Wrist2 = wrist2StartCenter, squareCorner2Gripper = gripperStartCenter;
int squareCorner3Shoulder = shoulderStartCenter, squareCorner3Elbow = elbowStartCenter,
    squareCorner3Wrist1 = wrist1StartCenter, squareCorner3Wrist2 = wrist2StartCenter, squareCorner3Gripper = gripperStartCenter;
int squareCorner4Shoulder = shoulderStartCenter, squareCorner4Elbow = elbowStartCenter,
    squareCorner4Wrist1 = wrist1StartCenter, squareCorner4Wrist2 = wrist2StartCenter, squareCorner4Gripper = gripperStartCenter;
const unsigned long SQUARE_EDGE_MS = 1500; // travel time between corners -- TODO confirm/replace
const bool SQUARE_LOOP = false;            // TODO: draw once and stop, or loop continuously? -- pending your answer

// ---- Coordinate Preset 1 & 2: Shoulder/Elbow/Wrist1 ease into a held
// pose, then Wrist2 sweeps solo (current->max->min), then Gripper cycles
// solo N times (min->max->min, a bit faster than the Wrist2 sweep), then
// a combined finale: Wrist2 eases home from min while Gripper does one
// more min->max->min cycle at the same time.
const int preset1Shoulder = -34, preset1Elbow = 3, preset1Wrist1 = 95;
const int preset2Shoulder = 1,   preset2Elbow = 90, preset2Wrist1 = 25;

const unsigned long PRESET_SETTLE_MS     = 1200; // easing into the held Shoulder/Elbow/Wrist1 pose (~1-1.5s)
const unsigned long PRESET_WRIST2_LEG_MS = 900;  // each leg of the Wrist2 solo sweep (current->max, max->min)
const unsigned long PRESET_GRIPPER_LEG_MS = 350; // each leg of a Gripper cycle -- "faster" than the Wrist2 sweep
const int PRESET1_GRIPPER_CYCLES = 2;
const int PRESET2_GRIPPER_CYCLES = 1;

// ---- Joint limits: your measured-safe values, pre-loaded (no re-jog needed) ----
int jointMinShoulder = -60, jointMaxShoulder = 15;   // delta -- confirmed safe
int jointMinElbow    = 0,   jointMaxElbow    = 110;  // deg   -- confirmed safe
int jointMinWrist1   = 10,  jointMaxWrist1   = 145;  // deg   -- confirmed safe
int jointMinWrist2   = 5,   jointMaxWrist2   = 170;  // deg   -- confirmed safe
int jointMinGripper  = 60,  jointMaxGripper  = 173; // deg   -- confirmed safe
bool jogHasRunShoulder = true, jogHasRunElbow = true, jogHasRunWrist1 = true, jogHasRunWrist2 = true, jogHasRunGripper = true;

// PLACEHOLDER -- base is new hardware, so unlike the servos above this one
// is NOT pre-confirmed. Jog it ('6' -> '<'/'>' -> 'n'/'x' -> 'q') before
// trusting these numbers. Default is a conservative +-90 deg soft limit.
int jointMinBase = (int)(-90 * BASE_STEPS_PER_DEG);
int jointMaxBase = (int)( 90 * BASE_STEPS_PER_DEG);
bool jogHasRunBase = false;

// ---- Named poses for the Stand Tall <-> Full Down demo ----
// TODO: replace with your measured angles, or capture live with 't' / 'd'
// (RUN mode only) -- they print back as CSV so you can paste them here later.
int standTallShoulder = shoulderStartCenter, standTallElbow = elbowStartCenter,
    standTallWrist1 = wrist1StartCenter, standTallWrist2 = wrist2StartCenter, standTallGripper = gripperStartCenter;
int fullDownShoulder = shoulderStartCenter, fullDownElbow = elbowStartCenter,
    fullDownWrist1 = wrist1StartCenter, fullDownWrist2 = wrist2StartCenter, fullDownGripper = gripperStartCenter;

// ---- Deadzone / speed ramp for LIVE control, tune to taste ----
const float DEADZONE_DEG  = 5.0;
const float ZONE_SLOW_MAX = 20.0;
const float ZONE_MED_MAX  = 40.0;
const int SPEED_SLOW = 1;
const int SPEED_MED  = 2;
const int SPEED_FAST = 4;

// Gesture-direction reference (as you described it -- not yet translated into
// flags; verify by testing, then flip the relevant one to true if reversed):
//   Elbow  (Mode 2, pitch): hand down -> "motion left"  | hand up -> "motion right"
//   Wrist1 (Mode 3, pitch): hand up   -> "motion up"    | hand down -> "motion down"
//   Wrist2 (Mode 3, roll):  hand up   -> "motion down"  (sounds inverted -- try INVERT_WRIST2=true if unwanted)
bool INVERT_SHOULDER = false, INVERT_ELBOW = false, INVERT_WRIST1 = false, INVERT_WRIST2 = false, INVERT_GRIPPER = false, INVERT_BASE = false;

enum Mode { RUN, JOG, SHOW, PENDING_DEMO };
Mode mode = RUN;

enum JogTarget { JOG_NONE, JOG_SHOULDER, JOG_ELBOW, JOG_WRIST1, JOG_WRIST2, JOG_GRIPPER, JOG_BASE };
JogTarget jogTarget = JOG_NONE;
int jogPos = 0;

const int SLEW_DELAY_MS = 15;
const unsigned long STALE_MS = 500;

int currentPosShoulder = shoulderStartCenter, targetPosShoulder = shoulderStartCenter;
int currentPosElbow    = elbowStartCenter,    targetPosElbow    = elbowStartCenter;
int currentPosWrist1   = wrist1StartCenter,   targetPosWrist1   = wrist1StartCenter;
int currentPosWrist2   = wrist2StartCenter,   targetPosWrist2   = wrist2StartCenter;
int currentPosGripper  = gripperStartCenter,  targetPosGripper  = gripperStartCenter;
int currentPosBase     = baseStartCenter,     targetPosBase     = baseStartCenter; // steps

unsigned long lastPacketTime = 0;
unsigned long lastPrint = 0;
int lastMode = 1;
float lastDeltaPitch = 0, lastDeltaRoll = 0;

// ============================================================================
// SHOW ENGINE -- generic step queue (Calib Sweep, Breathe, Pose Cycle) plus a
// dedicated Wave engine for staggered/overlapping timing.
// ============================================================================
const int SKIP_JOINT = 32000; // sentinel meaning "leave this joint where it is"

struct ShowStep {
  int shoulder, elbow, wrist1, wrist2, gripper; // target angle, or SKIP_JOINT
  unsigned long duration;                        // ms to ease into this step
};
const int MAX_SHOW_STEPS = 40; // Calib Sweep now needs ~31 steps (added inter-joint pauses)
ShowStep showSteps[MAX_SHOW_STEPS];
int showStepCount = 0, showStepIndex = 0;
unsigned long showStepStart = 0;
int stepFromShoulder, stepFromElbow, stepFromWrist1, stepFromWrist2, stepFromGripper;

bool usingWaveEngine = false;
unsigned long waveStart = 0;
bool waveOutPhase = true;
int waveOrder[5]; // permutation of {0..4} = {shoulder,elbow,wrist1,wrist2,gripper}, arrival order
const unsigned long WAVE_TRAVEL_MS  = 1800; // time for ONE joint's own out (or back) travel -- was 1000
const unsigned long WAVE_STAGGER_MS = 260;  // delay between each joint's start -- was 180

int demoIndex = 0;
const int NUM_DEMOS = 4;

// ---- 2-second "get ready" pre-roll before any demo actually starts moving ----
void (*pendingDemoFn)() = nullptr;
const __FlashStringHelper *pendingDemoLabel = nullptr;
unsigned long pendingDemoStartTime = 0;
unsigned long DEMO_START_DELAY_MS = 2000; // v8: variable, set live with "Z <ms>"

// ---- v8: line protocol / arbitration / telemetry state ----
// controlOwner: 0 = glove drives the joints (default, matches old behavior),
// 1 = app drives (glove packets parsed but their motion ignored).
uint8_t controlOwner = 0;
bool telemetryOn = false;
unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_MS = 100;

// USB serial line accumulator (non-blocking). A completed line of length 1
// is dispatched as a legacy single-char key, so the old serial-monitor
// workflow keeps working. If bytes sit in the buffer with no terminator
// for USB_LINE_TIMEOUT_MS (serial monitor set to "No line ending"), the
// buffer is treated as a complete line anyway.
char usbBuf[40];
uint8_t usbLen = 0;
unsigned long usbLastByte = 0;
const unsigned long USB_LINE_TIMEOUT_MS = 100;

// BT (glove) line accumulator -- replaces the blocking readStringUntil.
char btBuf[28];
uint8_t btLen = 0;

// Non-blocking slew pacing (replaces delay(SLEW_DELAY_MS) in RUN easing).
unsigned long lastSlewStep = 0;

float smoothstepf(float t) {
  t = constrain(t, 0.0, 1.0);
  return t * t * (3.0 - 2.0 * t);
}
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
int roundToInt(float v) { return (int)(v + (v >= 0 ? 0.5 : -0.5)); }

bool allJogged() {
  // Base intentionally excluded -- it's parked (unjogged, unverified hardware)
  // and none of the demos (old or new) move it, so it shouldn't block them.
  return jogHasRunShoulder && jogHasRunElbow && jogHasRunWrist1 && jogHasRunWrist2 && jogHasRunGripper;
}

void addStepRaw(int sh, int el, int w1, int w2, int gr, unsigned long dur) {
  if (showStepCount >= MAX_SHOW_STEPS) return;
  showSteps[showStepCount++] = { sh, el, w1, w2, gr, dur };
}
void addSingleJointStep(int jointIdx, int value, unsigned long dur) {
  int v[5] = { SKIP_JOINT, SKIP_JOINT, SKIP_JOINT, SKIP_JOINT, SKIP_JOINT };
  v[jointIdx] = value;
  addStepRaw(v[0], v[1], v[2], v[3], v[4], dur);
}
void addAllJointsStep(int v[5], unsigned long dur) {
  addStepRaw(v[0], v[1], v[2], v[3], v[4], dur);
}

void startShowStep(int idx) {
  showStepIndex = idx;
  showStepStart = millis();
  stepFromShoulder = currentPosShoulder;
  stepFromElbow    = currentPosElbow;
  stepFromWrist1   = currentPosWrist1;
  stepFromWrist2   = currentPosWrist2;
  stepFromGripper  = currentPosGripper;
}

void beginStepQueueShow() {
  mode = SHOW;
  usingWaveEngine = false;
  startShowStep(0);
}

void updateStepQueueShow() {
  if (showStepIndex >= showStepCount) { mode = RUN; return; }
  ShowStep &s = showSteps[showStepIndex];
  unsigned long elapsed = millis() - showStepStart;
  float t = (s.duration == 0) ? 1.0 : (float)elapsed / (float)s.duration;
  float e = smoothstepf(t);

  if (s.shoulder != SKIP_JOINT) currentPosShoulder = roundToInt(lerpf(stepFromShoulder, s.shoulder, e));
  if (s.elbow    != SKIP_JOINT) currentPosElbow    = roundToInt(lerpf(stepFromElbow,    s.elbow,    e));
  if (s.wrist1   != SKIP_JOINT) currentPosWrist1   = roundToInt(lerpf(stepFromWrist1,   s.wrist1,   e));
  if (s.wrist2   != SKIP_JOINT) currentPosWrist2   = roundToInt(lerpf(stepFromWrist2,   s.wrist2,   e));
  if (s.gripper  != SKIP_JOINT) currentPosGripper  = roundToInt(lerpf(stepFromGripper,  s.gripper,  e));

  writeShoulder(currentPosShoulder);
  writeElbow(currentPosElbow);
  writeWrist1(currentPosWrist1);
  writeWrist2(currentPosWrist2);
  writeGripper(currentPosGripper);

  if (t >= 1.0) {
    targetPosShoulder = currentPosShoulder; targetPosElbow = currentPosElbow;
    targetPosWrist1 = currentPosWrist1;     targetPosWrist2 = currentPosWrist2;
    targetPosGripper = currentPosGripper;
    showStepIndex++;
    if (showStepIndex < showStepCount) startShowStep(showStepIndex);
    else mode = RUN; // demo finished, hand control back
  }
}

void startWaveShow() {
  mode = SHOW;
  usingWaveEngine = true;
  waveOutPhase = true;
  waveStart = millis();
  for (int i = 0; i < 5; i++) waveOrder[i] = i;
  randomSeed(micros());
  for (int i = 4; i > 0; i--) { // Fisher-Yates -- fresh cascade order every run
    int j = random(0, i + 1);
    int tmp = waveOrder[i]; waveOrder[i] = waveOrder[j]; waveOrder[j] = tmp;
  }
}

void updateWaveShow() {
  int mins[5]   = { jointMinShoulder, jointMinElbow, jointMinWrist1, jointMinWrist2, jointMinGripper };
  int maxs[5]   = { jointMaxShoulder, jointMaxElbow, jointMaxWrist1, jointMaxWrist2, jointMaxGripper };
  int starts[5] = { shoulderStartCenter, elbowStartCenter, wrist1StartCenter, wrist2StartCenter, gripperStartCenter };
  int pos[5];
  unsigned long elapsed = millis() - waveStart;
  bool anyActive = false;

  for (int rank = 0; rank < 5; rank++) {
    int j = waveOrder[rank];
    long localElapsed = (long)elapsed - (long)(rank * WAVE_STAGGER_MS);
    float t = (localElapsed <= 0) ? 0.0 : (float)localElapsed / (float)WAVE_TRAVEL_MS;
    if (t < 1.0) anyActive = true;
    float e = smoothstepf(t);
    int fromV = waveOutPhase ? starts[j] : maxs[j];
    int toV   = waveOutPhase ? maxs[j]   : starts[j];
    pos[j] = roundToInt(lerpf(fromV, toV, e));
  }

  currentPosShoulder = pos[0]; currentPosElbow = pos[1]; currentPosWrist1 = pos[2];
  currentPosWrist2 = pos[3];   currentPosGripper = pos[4];
  writeShoulder(currentPosShoulder); writeElbow(currentPosElbow);
  writeWrist1(currentPosWrist1);     writeWrist2(currentPosWrist2); writeGripper(currentPosGripper);

  if (!anyActive) {
    if (waveOutPhase) {
      waveOutPhase = false;
      waveStart = millis();
    } else {
      targetPosShoulder = currentPosShoulder; targetPosElbow = currentPosElbow;
      targetPosWrist1 = currentPosWrist1;     targetPosWrist2 = currentPosWrist2;
      targetPosGripper = currentPosGripper;
      mode = RUN;
    }
  }
}

void startCalibSweepShow() {
  showStepCount = 0;
  int mins[5]   = { jointMinShoulder, jointMinElbow, jointMinWrist1, jointMinWrist2, jointMinGripper };
  int maxs[5]   = { jointMaxShoulder, jointMaxElbow, jointMaxWrist1, jointMaxWrist2, jointMaxGripper };
  int starts[5] = { shoulderStartCenter, elbowStartCenter, wrist1StartCenter, wrist2StartCenter, gripperStartCenter };

  addAllJointsStep(starts, 1600);
  for (int j = 0; j < 5; j++) {
    addSingleJointStep(j, mins[j],   2000); // to min
    addSingleJointStep(j, mins[j],   800);  // hold at min
    addSingleJointStep(j, maxs[j],   2800); // to max
    addSingleJointStep(j, maxs[j],   800);  // hold at max
    addSingleJointStep(j, starts[j], 1800); // back to center
    addSingleJointStep(j, starts[j], 1000); // pause before the next joint starts
  }
  beginStepQueueShow();
}

void startBreatheShow() {
  showStepCount = 0;
  int mins[5]   = { jointMinShoulder, jointMinElbow, jointMinWrist1, jointMinWrist2, jointMinGripper };
  int maxs[5]   = { jointMaxShoulder, jointMaxElbow, jointMaxWrist1, jointMaxWrist2, jointMaxGripper };
  int starts[5] = { shoulderStartCenter, elbowStartCenter, wrist1StartCenter, wrist2StartCenter, gripperStartCenter };

  addAllJointsStep(starts, 1200);
  for (int cycle = 0; cycle < 2; cycle++) {
    addAllJointsStep(maxs, 2400);
    addAllJointsStep(maxs, 600);
    addAllJointsStep(mins, 2800);
    addAllJointsStep(mins, 600);
  }
  addAllJointsStep(starts, 1800);
  beginStepQueueShow();
}

void startPoseCycleShow() {
  showStepCount = 0;
  int tall[5] = { standTallShoulder, standTallElbow, standTallWrist1, standTallWrist2, standTallGripper };
  int down[5] = { fullDownShoulder,  fullDownElbow,  fullDownWrist1,  fullDownWrist2,  fullDownGripper };

  addAllJointsStep(tall, 1400);
  for (int i = 0; i < 3; i++) {
    addAllJointsStep(down, 3200);
    addAllJointsStep(down, 1000);
    addAllJointsStep(tall, 3200);
    addAllJointsStep(tall, 1000);
  }
  beginStepQueueShow();
}

void startNodShow() {
  showStepCount = 0;
  // Settle: shoulder+elbow move into the nod pose, wrist1/wrist2/gripper
  // hold wherever they already are (SKIP_JOINT) -- addSingleJointStep()
  // can only touch one joint at a time, so settle shoulder and elbow with
  // two back-to-back steps of the same duration; visually simultaneous
  // since both start immediately after each other with no gap.
  addSingleJointStep(0 /*shoulder*/, nodShoulder, NOD_SETTLE_MS);
  addSingleJointStep(1 /*elbow*/,    nodElbow,    NOD_SETTLE_MS);

  // Nod: wrist1 swings +-NOD_AMPLITUDE_DEG around wherever it ACTUALLY is
  // right now (currentPosWrist1) -- not a hardcoded default -- since the
  // settle phase above doesn't touch wrist1, so it stays wherever the last
  // demo/gesture left it. Each half-nod is quick (NOD_QUICK_MS) to read as
  // a nod, not a slow wave.
  int base = currentPosWrist1;
  int up   = constrain(base + NOD_AMPLITUDE_DEG, jointMinWrist1, jointMaxWrist1);
  int down = constrain(base - NOD_AMPLITUDE_DEG, jointMinWrist1, jointMaxWrist1);
  for (int i = 0; i < NOD_REPS; i++) {
    addSingleJointStep(2 /*wrist1*/, down, NOD_QUICK_MS);
    addSingleJointStep(2 /*wrist1*/, up,   NOD_QUICK_MS);
  }
  addSingleJointStep(2 /*wrist1*/, base, NOD_QUICK_MS); // return to level
  beginStepQueueShow();
}

void startSquareShow() {
  showStepCount = 0;
  int c1[5] = { squareCorner1Shoulder, squareCorner1Elbow, squareCorner1Wrist1, squareCorner1Wrist2, squareCorner1Gripper };
  int c2[5] = { squareCorner2Shoulder, squareCorner2Elbow, squareCorner2Wrist1, squareCorner2Wrist2, squareCorner2Gripper };
  int c3[5] = { squareCorner3Shoulder, squareCorner3Elbow, squareCorner3Wrist1, squareCorner3Wrist2, squareCorner3Gripper };
  int c4[5] = { squareCorner4Shoulder, squareCorner4Elbow, squareCorner4Wrist1, squareCorner4Wrist2, squareCorner4Gripper };

  // Move to corner 1 first (from wherever the arm currently is), then trace
  // 1->2->3->4->1 so the square closes.
  addAllJointsStep(c1, SQUARE_EDGE_MS);
  addAllJointsStep(c2, SQUARE_EDGE_MS);
  addAllJointsStep(c3, SQUARE_EDGE_MS);
  addAllJointsStep(c4, SQUARE_EDGE_MS);
  addAllJointsStep(c1, SQUARE_EDGE_MS);
  beginStepQueueShow();
}

// Shared body for both Coordinate Presets -- same 4-phase structure, only
// the pose/cycle-count differ between preset 1 and 2.
void runPresetShow(int poseShoulder, int poseElbow, int poseWrist1, int gripperCycles) {
  showStepCount = 0;

  // Phase 1: settle shoulder/elbow/wrist1 into the held pose. Wrist2 and
  // gripper are untouched here (SKIP_JOINT via addSingleJointStep), so
  // they hold wherever they already are -- per your spec, gripper starts
  // this whole sequence at its normal center (gripperStartCenter).
  addSingleJointStep(0 /*shoulder*/, poseShoulder, PRESET_SETTLE_MS);
  addSingleJointStep(1 /*elbow*/,    poseElbow,    PRESET_SETTLE_MS);
  addSingleJointStep(2 /*wrist1*/,   poseWrist1,   PRESET_SETTLE_MS);

  // Phase 2: Wrist2 solo sweep, current -> max -> min.
  addSingleJointStep(3 /*wrist2*/, jointMaxWrist2, PRESET_WRIST2_LEG_MS);
  addSingleJointStep(3 /*wrist2*/, jointMinWrist2, PRESET_WRIST2_LEG_MS);

  // Phase 3: Gripper solo cycles, min -> max -> min, repeated, a bit
  // faster than the wrist2 sweep (PRESET_GRIPPER_LEG_MS < PRESET_WRIST2_LEG_MS).
  addSingleJointStep(4 /*gripper*/, jointMinGripper, PRESET_GRIPPER_LEG_MS);
  for (int i = 0; i < gripperCycles; i++) {
    addSingleJointStep(4 /*gripper*/, jointMaxGripper, PRESET_GRIPPER_LEG_MS);
    addSingleJointStep(4 /*gripper*/, jointMinGripper, PRESET_GRIPPER_LEG_MS);
  }

  // Phase 4 (finale): Wrist2 eases home from min WHILE Gripper does one
  // more min->max->min cycle -- both in the SAME step, via
  // addAllJointsStep() with SKIP_JOINT on shoulder/elbow/wrist1 so they
  // just hold the pose from phase 1.
  int homeWrist2 = wrist2StartCenter;
  int stepA[5] = { SKIP_JOINT, SKIP_JOINT, SKIP_JOINT, jointMaxWrist2, jointMaxGripper };
  int stepB[5] = { SKIP_JOINT, SKIP_JOINT, SKIP_JOINT, homeWrist2,     jointMinGripper };
  // Wrist2 needs 2 legs (min->max->home) to end up back home while gripper
  // only needs 1 more full cycle -- split unevenly so both land together:
  // first sub-step brings wrist2 up to max (halfway home) alongside
  // gripper's rise to max; second sub-step brings wrist2 the rest of the
  // way home alongside gripper's fall back to min.
  addAllJointsStep(stepA, PRESET_GRIPPER_LEG_MS);
  addAllJointsStep(stepB, PRESET_GRIPPER_LEG_MS);

  beginStepQueueShow();
}

void startPreset1Show() { runPresetShow(preset1Shoulder, preset1Elbow, preset1Wrist1, PRESET1_GRIPPER_CYCLES); }
void startPreset2Show() { runPresetShow(preset2Shoulder, preset2Elbow, preset2Wrist1, PRESET2_GRIPPER_CYCLES); }

// Queues a demo to start after a 2s "get ready" pause instead of moving
// instantly. loop() checks pendingDemoFn/pendingDemoStartTime and actually
// calls the start function once DEMO_START_DELAY_MS has elapsed.
void queueDemo(void (*startFn)(), const __FlashStringHelper *label) {
  if (mode == JOG) return;
  if (!allJogged()) {
    Serial.println(F("!! Can't run demos yet -- one or more joints haven't been jogged."));
    return;
  }
  mode = PENDING_DEMO;
  pendingDemoFn = startFn;
  pendingDemoLabel = label;
  pendingDemoStartTime = millis();
  Serial.print(F(">> Get ready -- ")); Serial.print(label); Serial.println(F(" starting in 2s..."));
}

// Direct-select entry point: press 'c'/'w'/'b'/'p' to jump straight to a
// specific demo, no cycling needed.
void tryStartDemo(void (*startFn)(), const __FlashStringHelper *label) {
  queueDemo(startFn, label);
}

// "Next" cycling entry point, kept for convenience -- steps through the
// library in order each time it's pressed.
void triggerNextDemo() {
  switch (demoIndex) {
    case 0: queueDemo(startCalibSweepShow, F("Calibration Sweep"));        break;
    case 1: queueDemo(startWaveShow,       F("Wave Cascade"));             break;
    case 2: queueDemo(startBreatheShow,    F("Synchronized Breathe"));     break;
    case 3: queueDemo(startPoseCycleShow,  F("Stand Tall <-> Full Down")); break;
  }
  demoIndex = (demoIndex + 1) % NUM_DEMOS;
}

void stopDemo() {
  mode = RUN;
  targetPosShoulder = currentPosShoulder; targetPosElbow = currentPosElbow;
  targetPosWrist1 = currentPosWrist1;     targetPosWrist2 = currentPosWrist2;
  targetPosGripper = currentPosGripper;
  Serial.println(F(">> Demo stopped, holding position."));
}

// ============================================================================
// Core joint I/O
// ============================================================================
int degToPulse(int deg) { return map(deg, 0, 180, SERVOMIN, SERVOMAX); }

void writeShoulder(int delta) {
  delta = constrain(delta, -90, 90);
  int lPos = constrain(L_CENTER + delta, 0, 180);
  int rPos = constrain(R_CENTER - delta, 0, 180);
  pca.setPWM(CH_SHOULDER_L, 0, degToPulse(lPos));
  pca.setPWM(CH_SHOULDER_R, 0, degToPulse(rPos));
}
void writeElbow(int deg)   { pca.setPWM(CH_ELBOW,   0, degToPulse(constrain(deg, 0, 180))); }
void writeWrist1(int deg)  { pca.setPWM(CH_WRIST1,  0, degToPulse(constrain(deg, 0, 180))); }
void writeWrist2(int deg)  { pca.setPWM(CH_WRIST2,  0, degToPulse(constrain(deg, 0, 180))); }
void writeGripper(int deg) { pca.setPWM(CH_GRIPPER, 0, degToPulse(constrain(deg, 0, 180))); }

// Unlike the servo writeX() functions, this doesn't move anything itself --
// AccelStepper needs frequent run() calls to actually pulse STEP, and that
// has to happen every loop() iteration (not gated behind SLEW_DELAY_MS or
// the show engine's timing), so it lives in loop() directly. This just
// hands over the new destination.
void writeBase(int steps) {
  baseStepper.moveTo(constrain(steps, jointMinBase, jointMaxBase));
}

// v8: zero-allocation packet parser. Format unchanged: "mode,dPitch,dRoll".
// Works directly on the char buffer -- no String, no heap.
bool parsePacket(char *line, int &m, float &dPitch, float &dRoll) {
  char *c1 = strchr(line, ',');
  if (!c1 || c1 == line) return false;
  char *c2 = strchr(c1 + 1, ',');
  if (!c2) return false;

  *c1 = '\0'; *c2 = '\0';
  m      = atoi(line);
  dPitch = atof(c1 + 1);
  dRoll  = atof(c2 + 1);
  return true;
}

int applyVelocity(float delta, int currentTarget, int jointMin, int jointMax, bool invert) {
  float ad = fabs(delta);
  if (ad < DEADZONE_DEG) return currentTarget;

  int dir = (delta < 0) ? -1 : 1;
  if (invert) dir = -dir;

  int speed;
  if (ad < ZONE_SLOW_MAX) speed = SPEED_SLOW;
  else if (ad < ZONE_MED_MAX) speed = SPEED_MED;
  else speed = SPEED_FAST;

  return constrain(currentTarget + dir * speed, jointMin, jointMax);
}

// Same shape as applyVelocity(), but the base moves in steps, not degrees --
// SPEED_SLOW/MED/FAST (1/2/4) would be imperceptible at 200-800 steps/rev,
// so this has its own, larger, step-scaled speed tiers. Tune the three
// BASE_SPEED_* constants to taste once it's jogged and spinning.
const int BASE_SPEED_SLOW = (int)(2  * BASE_STEPS_PER_DEG); // deg/packet-tick, converted to steps
const int BASE_SPEED_MED  = (int)(6  * BASE_STEPS_PER_DEG);
const int BASE_SPEED_FAST = (int)(15 * BASE_STEPS_PER_DEG);

int applyVelocityBase(float delta, int currentTarget, int jointMin, int jointMax, bool invert) {
  float ad = fabs(delta);
  if (ad < DEADZONE_DEG) return currentTarget;

  int dir = (delta < 0) ? -1 : 1;
  if (invert) dir = -dir;

  int speed;
  if (ad < ZONE_SLOW_MAX) speed = BASE_SPEED_SLOW;
  else if (ad < ZONE_MED_MAX) speed = BASE_SPEED_MED;
  else speed = BASE_SPEED_FAST;

  return constrain(currentTarget + dir * speed, jointMin, jointMax);
}

void printStatus() {
  Serial.println(F("---- STATUS ----"));
  Serial.print(F("Mode (last received): ")); Serial.println(lastMode);
  Serial.print(F("Next demo (via 'g'): ")); Serial.println(demoIndex);
  Serial.print(F("Shoulder: min=")); Serial.print(jointMinShoulder);
  Serial.print(F(" max=")); Serial.print(jointMaxShoulder);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunShoulder ? F("true") : F("false (PLACEHOLDER)"));
  Serial.print(F("Elbow:    min=")); Serial.print(jointMinElbow);
  Serial.print(F(" max=")); Serial.print(jointMaxElbow);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunElbow ? F("true") : F("false (PLACEHOLDER)"));
  Serial.print(F("Wrist1:   min=")); Serial.print(jointMinWrist1);
  Serial.print(F(" max=")); Serial.print(jointMaxWrist1);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunWrist1 ? F("true") : F("false (PLACEHOLDER)"));
  Serial.print(F("Wrist2:   min=")); Serial.print(jointMinWrist2);
  Serial.print(F(" max=")); Serial.print(jointMaxWrist2);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunWrist2 ? F("true") : F("false (PLACEHOLDER)"));
  Serial.print(F("Gripper:  min=")); Serial.print(jointMinGripper);
  Serial.print(F(" max=")); Serial.print(jointMaxGripper);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunGripper ? F("true") : F("false (PLACEHOLDER)"));
  Serial.print(F("Base:     min=")); Serial.print(jointMinBase);
  Serial.print(F(" max=")); Serial.print(jointMaxBase);
  Serial.print(F(" jogHasRun=")); Serial.println(jogHasRunBase ? F("true") : F("false (PLACEHOLDER -- jog this before trusting it)"));
  Serial.println(F("----------------"));
}

void printJogHelp() {
  Serial.print(F(">> JOG MODE ("));
  switch (jogTarget) {
    case JOG_SHOULDER: Serial.print(F("Shoulder")); break;
    case JOG_ELBOW:     Serial.print(F("Elbow")); break;
    case JOG_WRIST1:    Serial.print(F("Wrist1")); break;
    case JOG_WRIST2:    Serial.print(F("Wrist2")); break;
    case JOG_GRIPPER:   Serial.print(F("Gripper")); break;
    case JOG_BASE:      Serial.print(F("Base")); break;
    default: break;
  }
  Serial.println(F("). '+'/'-' = nudge 1, '>'/'<' = nudge 5, 'n' = mark MIN, 'x' = mark MAX, 'q' = exit"));
}

void enterJog(JogTarget t, int startPos) {
  mode = JOG;
  jogTarget = t;
  jogPos = startPos;
  printJogHelp();
}

// ============================ v8 INPUT HANDLING ============================
// Legacy single-char keys (the old serial-monitor interface, unchanged).
// Called when a completed USB line is exactly one lowercase/digit char.
void handleLegacyKey(char cmd) {
  if (cmd == 'h' && mode != JOG) {
    stopDemo();
  }
  else if (mode == RUN) {
    if (cmd == '1') enterJog(JOG_SHOULDER, currentPosShoulder);
    else if (cmd == '2') enterJog(JOG_ELBOW, currentPosElbow);
    else if (cmd == '3') enterJog(JOG_WRIST1, currentPosWrist1);
    else if (cmd == '4') enterJog(JOG_WRIST2, currentPosWrist2);
    else if (cmd == '5') enterJog(JOG_GRIPPER, currentPosGripper);
    else if (cmd == '6') enterJog(JOG_BASE, currentPosBase);
    else if (cmd == 'c') tryStartDemo(startCalibSweepShow, F("Calibration Sweep"));
    else if (cmd == 'w') tryStartDemo(startWaveShow,       F("Wave Cascade"));
    else if (cmd == 'b') tryStartDemo(startBreatheShow,    F("Synchronized Breathe"));
    else if (cmd == 'p') tryStartDemo(startPoseCycleShow,  F("Stand Tall <-> Full Down"));
    else if (cmd == 'a') tryStartDemo(startNodShow,        F("Nod"));
    else if (cmd == 'e') tryStartDemo(startSquareShow,     F("Square"));
    else if (cmd == 's') tryStartDemo(startPreset1Show,    F("Coordinate Preset 1"));
    else if (cmd == 'f') tryStartDemo(startPreset2Show,    F("Coordinate Preset 2"));
    else if (cmd == 'g') triggerNextDemo();
    else if (cmd == 't') {
      standTallShoulder = currentPosShoulder; standTallElbow = currentPosElbow;
      standTallWrist1 = currentPosWrist1;     standTallWrist2 = currentPosWrist2;
      standTallGripper = currentPosGripper;
      Serial.print(F(">> Captured STAND TALL: "));
      Serial.print(standTallShoulder); Serial.print(',');
      Serial.print(standTallElbow); Serial.print(',');
      Serial.print(standTallWrist1); Serial.print(',');
      Serial.print(standTallWrist2); Serial.print(',');
      Serial.println(standTallGripper);
    }
    else if (cmd == 'd') {
      fullDownShoulder = currentPosShoulder; fullDownElbow = currentPosElbow;
      fullDownWrist1 = currentPosWrist1;     fullDownWrist2 = currentPosWrist2;
      fullDownGripper = currentPosGripper;
      Serial.print(F(">> Captured FULL DOWN: "));
      Serial.print(fullDownShoulder); Serial.print(',');
      Serial.print(fullDownElbow); Serial.print(',');
      Serial.print(fullDownWrist1); Serial.print(',');
      Serial.print(fullDownWrist2); Serial.print(',');
      Serial.println(fullDownGripper);
    }
    else if (cmd == '?') printStatus();
  }
  else if (mode == JOG) {
    int *jMin, *jMax;
    switch (jogTarget) {
      case JOG_SHOULDER: jMin = &jointMinShoulder; jMax = &jointMaxShoulder; break;
      case JOG_ELBOW:     jMin = &jointMinElbow;     jMax = &jointMaxElbow;     break;
      case JOG_WRIST1:    jMin = &jointMinWrist1;    jMax = &jointMaxWrist1;    break;
      case JOG_WRIST2:    jMin = &jointMinWrist2;    jMax = &jointMaxWrist2;    break;
      case JOG_GRIPPER:   jMin = &jointMinGripper;   jMax = &jointMaxGripper;   break;
      default:            jMin = &jointMinBase;      jMax = &jointMaxBase;      break;
    }

    if (cmd == '+') jogPos++;
    else if (cmd == '-') jogPos--;
    else if (cmd == '>') jogPos += 5;
    else if (cmd == '<') jogPos -= 5;
    else if (cmd == 'n') { *jMin = jogPos; Serial.print(F(">> MIN set to ")); Serial.println(*jMin); }
    else if (cmd == 'x') { *jMax = jogPos; Serial.print(F(">> MAX set to ")); Serial.println(*jMax); }
    else if (cmd == 'q') {
      if (*jMin > *jMax) { int t = *jMin; *jMin = *jMax; *jMax = t; }
      switch (jogTarget) {
        case JOG_SHOULDER: currentPosShoulder = jogPos; targetPosShoulder = jogPos; jogHasRunShoulder = true; break;
        case JOG_ELBOW:     currentPosElbow = jogPos;     targetPosElbow = jogPos;     jogHasRunElbow = true;     break;
        case JOG_WRIST1:    currentPosWrist1 = jogPos;    targetPosWrist1 = jogPos;    jogHasRunWrist1 = true;    break;
        case JOG_WRIST2:    currentPosWrist2 = jogPos;    targetPosWrist2 = jogPos;    jogHasRunWrist2 = true;    break;
        case JOG_GRIPPER:   currentPosGripper = jogPos;   targetPosGripper = jogPos;   jogHasRunGripper = true;   break;
        default:
          currentPosBase = jogPos; targetPosBase = jogPos; jogHasRunBase = true;
          baseStepper.setCurrentPosition(jogPos); // re-zero AccelStepper's internal count to match
          break;
      }
      Serial.print(F(">> JOG DONE. MIN=")); Serial.print(*jMin);
      Serial.print(F(" MAX=")); Serial.println(*jMax);
      mode = RUN;
      jogTarget = JOG_NONE;
    }
    else if (cmd == '?') printStatus();

    if (cmd == '+' || cmd == '-' || cmd == '>' || cmd == '<') {
      switch (jogTarget) {
        case JOG_SHOULDER: writeShoulder(jogPos); break;
        case JOG_ELBOW:     writeElbow(jogPos);     break;
        case JOG_WRIST1:    writeWrist1(jogPos);    break;
        case JOG_WRIST2:    writeWrist2(jogPos);    break;
        case JOG_GRIPPER:   writeGripper(jogPos);   break;
        default:
          baseStepper.moveTo(jogPos);
          while (baseStepper.distanceToGo() != 0) baseStepper.run(); // jog nudges are tiny, brief blocking is fine here
          break;
      }
      Serial.print(F("pos=")); Serial.println(jogPos);
    }
  }
}

// Pull the next integer out of a command line; advances *p past it.
// Returns false if there is no number left.
bool nextInt(char **p, long &out) {
  while (**p == ' ') (*p)++;
  if (**p == '\0') return false;
  char *end;
  out = strtol(*p, &end, 10);
  if (end == *p) return false;
  *p = end;
  return true;
}

// If a demo is running (or pending), a direct pose/joint command from the
// app interrupts it -- same rule as a glove gesture interrupting SHOW.
void interruptDemoIfRunning() {
  if (mode == SHOW || mode == PENDING_DEMO) stopDemo();
}

// If the glove currently owns the arm, a J/P command from the app claims
// ownership implicitly -- a slider drag should Just Work without needing
// the user to remember to flip the ownership toggle first.
void appAutoClaim() {
  if (controlOwner == 0) {
    controlOwner = 1;
    Serial.println(F(">> APP claimed control (send M 0 to hand back to glove)"));
  }
}

void setJointTarget(uint8_t idx, int deg) {
  switch (idx) {
    case 0: targetPosShoulder = constrain(deg, jointMinShoulder, jointMaxShoulder); break;
    case 1: targetPosElbow    = constrain(deg, jointMinElbow,    jointMaxElbow);    break;
    case 2: targetPosWrist1   = constrain(deg, jointMinWrist1,   jointMaxWrist1);   break;
    case 3: targetPosWrist2   = constrain(deg, jointMinWrist2,   jointMaxWrist2);   break;
    case 4: targetPosGripper  = constrain(deg, jointMinGripper,  jointMaxGripper);  break;
  }
}

// Uppercase protocol lines from the console: J/P/D/M/H/T/L/Z.
void handleProtocolLine() {
  char op = usbBuf[0];
  char *args = usbBuf + 1;
  long a, b, c, d, e;

  if (mode == JOG && op != 'H') {
    Serial.println(F("?? in JOG mode -- finish with q first"));
    return;
  }

  switch (op) {
    case 'J':
      if (nextInt(&args, a) && nextInt(&args, b)) {
        if (a == 5) { Serial.println(F("?? base is parked -- jog it ('6') before driving it")); return; }
        if (a < 0 || a > 4) { Serial.println(F("?? J: joint index must be 0..4")); return; }
        interruptDemoIfRunning();
        appAutoClaim();
        setJointTarget((uint8_t)a, (int)b);
      } else Serial.println(F("?? usage: J <idx 0..4> <deg>"));
      break;

    case 'P':
      if (nextInt(&args, a) && nextInt(&args, b) && nextInt(&args, c) &&
          nextInt(&args, d) && nextInt(&args, e)) {
        interruptDemoIfRunning();
        appAutoClaim();
        setJointTarget(0, (int)a); setJointTarget(1, (int)b); setJointTarget(2, (int)c);
        setJointTarget(3, (int)d); setJointTarget(4, (int)e);
      } else Serial.println(F("?? usage: P <sh> <el> <w1> <w2> <gr>"));
      break;

    case 'D':
      if (nextInt(&args, a)) {
        switch (a) {
          case 0: tryStartDemo(startCalibSweepShow, F("Calibration Sweep"));        break;
          case 1: tryStartDemo(startWaveShow,       F("Wave Cascade"));             break;
          case 2: tryStartDemo(startBreatheShow,    F("Synchronized Breathe"));     break;
          case 3: tryStartDemo(startPoseCycleShow,  F("Stand Tall <-> Full Down")); break;
          case 4: tryStartDemo(startNodShow,        F("Nod"));                      break;
          case 5: tryStartDemo(startSquareShow,     F("Square"));                   break;
          case 6: tryStartDemo(startPreset1Show,    F("Coordinate Preset 1"));      break;
          case 7: tryStartDemo(startPreset2Show,    F("Coordinate Preset 2"));      break;
          default: Serial.println(F("?? D: demo id must be 0..7"));                 break;
        }
      } else Serial.println(F("?? usage: D <id 0..7>"));
      break;

    case 'M':
      if (nextInt(&args, a) && (a == 0 || a == 1)) {
        controlOwner = (uint8_t)a;
        Serial.println(a ? F(">> owner: APP") : F(">> owner: GLOVE"));
      } else Serial.println(F("?? usage: M <0|1>"));
      break;

    case 'H':
      stopDemo();
      break;

    case 'T':
      if (nextInt(&args, a) && (a == 0 || a == 1)) {
        telemetryOn = (a == 1);
        Serial.println(telemetryOn ? F(">> telemetry ON") : F(">> telemetry OFF"));
      } else Serial.println(F("?? usage: T <0|1>"));
      break;

    case 'L':
      Serial.print(F("L "));
      Serial.print(jointMinShoulder); Serial.print(' '); Serial.print(jointMaxShoulder); Serial.print(' ');
      Serial.print(jointMinElbow);    Serial.print(' '); Serial.print(jointMaxElbow);    Serial.print(' ');
      Serial.print(jointMinWrist1);   Serial.print(' '); Serial.print(jointMaxWrist1);   Serial.print(' ');
      Serial.print(jointMinWrist2);   Serial.print(' '); Serial.print(jointMaxWrist2);   Serial.print(' ');
      Serial.print(jointMinGripper);  Serial.print(' '); Serial.print(jointMaxGripper);  Serial.print(' ');
      Serial.print(jointMinBase);     Serial.print(' '); Serial.println(jointMaxBase);
      break;

    case 'Z':
      if (nextInt(&args, a) && a >= 0 && a <= 10000) {
        DEMO_START_DELAY_MS = (unsigned long)a;
        Serial.print(F(">> demo pre-roll = ")); Serial.print(DEMO_START_DELAY_MS); Serial.println(F("ms"));
      } else Serial.println(F("?? usage: Z <ms 0..10000>"));
      break;

    default:
      Serial.println(F("?? unknown command"));
      break;
  }
}

// A completed USB line: uppercase first char = protocol, single lowercase
// char / digit = legacy key, anything else = junk.
void handleUsbLine() {
  char op = usbBuf[0];
  if (op >= 'A' && op <= 'Z') { handleProtocolLine(); return; }
  if (usbLen == 1) { handleLegacyKey(op); return; }
  Serial.println(F("?? unrecognized line"));
}

// A completed glove packet line, parsed without String or blocking.
// Ownership gate lives here: when the app owns, the packet still refreshes
// link-liveness bookkeeping but its motion is ignored.
void handleGlovePacket() {
  int pMode; float dPitch, dRoll;
  if (!parsePacket(btBuf, pMode, dPitch, dRoll)) return;

  lastPacketTime = millis();
  lastMode = pMode;
  lastDeltaPitch = dPitch;
  lastDeltaRoll = dRoll;

  if (controlOwner != 0) return; // app drives; glove motion ignored

  if (pMode >= 1 && pMode <= 5 && mode == SHOW) {
    // A genuine gesture command interrupts any running demo and
    // gives control back to your hand immediately.
    mode = RUN;
  }

  if (mode == RUN) {
    if (pMode == 1) {
      targetPosShoulder = applyVelocity(dPitch, targetPosShoulder, jointMinShoulder, jointMaxShoulder, INVERT_SHOULDER);
    } else if (pMode == 2) {
      targetPosElbow  = applyVelocity(dPitch, targetPosElbow,  jointMinElbow,  jointMaxElbow,  INVERT_ELBOW);
    } else if (pMode == 3) {
      targetPosWrist1 = applyVelocity(dPitch, targetPosWrist1, jointMinWrist1, jointMaxWrist1, INVERT_WRIST1);
      targetPosWrist2 = applyVelocity(dRoll,  targetPosWrist2, jointMinWrist2, jointMaxWrist2, INVERT_WRIST2);
    } else if (pMode == 4) {
      targetPosGripper = applyVelocity(dRoll, targetPosGripper, jointMinGripper, jointMaxGripper, INVERT_GRIPPER);
    } else if (pMode == 5) {
      // Same gesture language as Gripper: roll (wrist twist) drives it, pitch unused.
      targetPosBase = applyVelocityBase(dRoll, targetPosBase, jointMinBase, jointMaxBase, INVERT_BASE);
      writeBase(targetPosBase);
    }
  }
}
// ========================== end v8 INPUT HANDLING ==========================

void setup() {
  Serial.begin(115200);   // v8: was 9600 -- match your console/monitor to this
  BT.begin(9600);         // HC-05 default, unchanged
  BT.setTimeout(5);       // belt-and-braces; v8 never does timeout reads anyway
  delay(500);

  Wire.begin();
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);
  delay(10);

  writeShoulder(currentPosShoulder);
  writeElbow(currentPosElbow);
  writeWrist1(currentPosWrist1);
  writeWrist2(currentPosWrist2);
  writeGripper(currentPosGripper);
  delay(300);

  pinMode(BASE_EN_PIN, OUTPUT);
  digitalWrite(BASE_EN_PIN, LOW); // enable the driver (active LOW)
  baseStepper.setMaxSpeed(BASE_STEPS_PER_DEG * 60);      // ~60 deg/sec ceiling -- raise once you trust the mechanics
  baseStepper.setAcceleration(BASE_STEPS_PER_DEG * 90);  // ~90 deg/sec^2
  baseStepper.setCurrentPosition(currentPosBase);

  Serial.println(F("=== Trigger-Cycle Arm Control v8 (115200 baud) ==="));
  Serial.println(F("Protocol: J <i> <deg> | P <5 angles> | D <0-7> | M <0|1> | H | T <0|1> | L | Z <ms>"));
  Serial.println(F("Jog: 1/2/3/4/5/6 = Shoulder/Elbow/Wrist1/Wrist2/Gripper/Base, ?=status"));
  Serial.println(F("!! Base is new hardware and NOT pre-confirmed -- jog it ('6') before trusting the default +-90deg limits."));
  Serial.println(F("Demo (direct): c=Calib Sweep, w=Wave, b=Breathe, p=Pose Cycle"));
  Serial.println(F("Demo (direct, new): a=Nod, e=Square, s=Coord Preset 1, f=Coord Preset 2"));
  Serial.println(F("!! Square ('e') corner poses are still placeholders -- fill in before it'll draw anything."));
  Serial.println(F("Demo (cycle):  g=next demo   |  h=stop/hold   |  t/d=capture Stand Tall/Full Down pose"));
  Serial.println(F("All 5 joints are pre-loaded with your measured-safe ranges -- demos are ready now."));
  Serial.println(F("Every demo waits 2s after you press its key before it actually starts moving."));
  printStatus();
}

void loop() {
  // ---- USB serial: non-blocking line accumulator (v8) ----
  while (Serial.available() > 0) {
    char ch = Serial.read();
    usbLastByte = millis();
    if (ch == '\n' || ch == '\r') {
      if (usbLen > 0) { usbBuf[usbLen] = '\0'; handleUsbLine(); usbLen = 0; }
    } else if (usbLen < sizeof(usbBuf) - 1) {
      usbBuf[usbLen++] = ch;
    } else {
      usbLen = 0; // overlong line: discard and resync
    }
  }
  // Serial monitor set to "No line ending" sends bare chars -- flush the
  // buffer as a completed line once it's sat quietly for a moment.
  if (usbLen > 0 && millis() - usbLastByte > USB_LINE_TIMEOUT_MS) {
    usbBuf[usbLen] = '\0'; handleUsbLine(); usbLen = 0;
  }

  // ---- Bluetooth (glove): non-blocking accumulator. LIVE CONTROL ONLY,
  // no choreography commands. v7 and earlier used a blocking
  // readStringUntil here with a 1000ms default timeout -- THE source of
  // the multi-second lag: any stray byte arriving without a newline
  // (mangled packet, floating RX pin noise) froze the entire loop for a
  // second per byte. This version never waits.
  while (BT.available()) {
    char ch = BT.read();
    if (ch == '\n' || ch == '\r') {
      if (btLen > 0) { btBuf[btLen] = '\0'; handleGlovePacket(); btLen = 0; }
    } else if (btLen < sizeof(btBuf) - 1) {
      btBuf[btLen++] = ch;
    } else {
      btLen = 0; // mangled/overlong packet: discard, resync on next newline
    }
  }

  // ---- Telemetry stream (v8): runs in EVERY mode, including SHOW, so the
  // console's actual-position ticks track demos too. '#' prefix lets the
  // console separate telemetry from human-readable log lines.
  if (telemetryOn && millis() - lastTelemetry >= TELEMETRY_MS) {
    lastTelemetry = millis();
    Serial.print(F("# "));
    Serial.print(controlOwner);       Serial.print(' ');
    Serial.print(currentPosShoulder); Serial.print(' ');
    Serial.print(currentPosElbow);    Serial.print(' ');
    Serial.print(currentPosWrist1);   Serial.print(' ');
    Serial.print(currentPosWrist2);   Serial.print(' ');
    Serial.print(currentPosGripper);  Serial.print(' ');
    Serial.println(currentPosBase);
  }

  // ---- Base stepper: always serviced, independent of servo mode/state.
  // moveTo() was already called (from the BT handler above, or from JOG/
  // demo capture below); run() is what actually issues STEP pulses, using
  // AccelStepper's own accel/decel profile -- it needs to be polled often,
  // so it does NOT wait for JOG/SHOW/PENDING_DEMO to finish first.
  baseStepper.run();

  // ---- JOG mode: skip everything else ----
  if (mode == JOG) {
    if (millis() - lastPrint > 300) {
      Serial.print(F("jog pos=")); Serial.println(jogPos);
      lastPrint = millis();
    }
    return;
  }

  // ---- PENDING_DEMO: holding still during the 2s "get ready" pre-roll ----
  if (mode == PENDING_DEMO) {
    if (millis() - pendingDemoStartTime >= DEMO_START_DELAY_MS) {
      Serial.print(F(">> DEMO: ")); Serial.println(pendingDemoLabel);
      pendingDemoFn(); // sets mode = SHOW internally
    }
    return; // arm holds its current position while waiting, no easing/writes needed
  }

  // ---- SHOW mode: run the sequencer, skip normal RUN easing ----
  if (mode == SHOW) {
    if (usingWaveEngine) updateWaveShow(); else updateStepQueueShow();
    if (!telemetryOn && millis() - lastPrint > 500) {
      Serial.print(F("[SHOW] Sh=")); Serial.print(currentPosShoulder);
      Serial.print(F(" El=")); Serial.print(currentPosElbow);
      Serial.print(F(" W1=")); Serial.print(currentPosWrist1);
      Serial.print(F(" W2=")); Serial.print(currentPosWrist2);
      Serial.print(F(" Gr=")); Serial.println(currentPosGripper);
      lastPrint = millis();
    }
    return;
  }

  // ---- RUN mode: ease all five joints toward target, 1 unit per slew tick.
  // v8: pacing is millis()-based (no delay() -- command handling never
  // stalls), and the link-stale freeze only applies while the GLOVE owns
  // control. App-commanded targets don't refresh lastPacketTime, so
  // without the owner gate every slider move would freeze after 500ms.
  bool linkStale = (controlOwner == 0) &&
                   (millis() - lastPacketTime > STALE_MS) && (lastPacketTime != 0);
  if (!linkStale && millis() - lastSlewStep >= (unsigned long)SLEW_DELAY_MS) {
    lastSlewStep = millis();
    bool moved = false;
    if (currentPosShoulder != targetPosShoulder) { currentPosShoulder += (currentPosShoulder < targetPosShoulder) ? 1 : -1; moved = true; }
    if (currentPosElbow    != targetPosElbow)    { currentPosElbow    += (currentPosElbow    < targetPosElbow)    ? 1 : -1; moved = true; }
    if (currentPosWrist1   != targetPosWrist1)   { currentPosWrist1   += (currentPosWrist1   < targetPosWrist1)   ? 1 : -1; moved = true; }
    if (currentPosWrist2   != targetPosWrist2)   { currentPosWrist2   += (currentPosWrist2   < targetPosWrist2)   ? 1 : -1; moved = true; }
    if (currentPosGripper  != targetPosGripper)  { currentPosGripper  += (currentPosGripper  < targetPosGripper)  ? 1 : -1; moved = true; }
    if (moved) {
      writeShoulder(currentPosShoulder);
      writeElbow(currentPosElbow);
      writeWrist1(currentPosWrist1);
      writeWrist2(currentPosWrist2);
      writeGripper(currentPosGripper);
    }
  }

  if (!telemetryOn && millis() - lastPrint > 500) {
    if (!allJogged()) {
      Serial.println(F("!! One or more joints haven't been jogged this session -- demos are blocked until they are."));
    }
    Serial.print(F("mode=")); Serial.print(lastMode);
    Serial.print(F(" dPitch=")); Serial.print(lastDeltaPitch, 1);
    Serial.print(F(" dRoll=")); Serial.print(lastDeltaRoll, 1);
    Serial.print(F("  Sh=")); Serial.print(currentPosShoulder);
    Serial.print(F(" El=")); Serial.print(currentPosElbow);
    Serial.print(F(" W1=")); Serial.print(currentPosWrist1);
    Serial.print(F(" W2=")); Serial.print(currentPosWrist2);
    Serial.print(F(" Gr=")); Serial.print(currentPosGripper);
    Serial.println(linkStale ? F("  [LINK STALE]") : F(""));
    lastPrint = millis();
  }
}
