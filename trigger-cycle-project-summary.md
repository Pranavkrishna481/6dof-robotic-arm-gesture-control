# Trigger-Cycle: Gesture-Controlled 6-DOF Robotic Arm — Project Summary

Built by Pranav (BTech ECE, JNTUH) with a collaborator. Presented as a minor project to DST (Department of Science and Technology) and JTBI (JNTU incubator). Code hosted at: github.com/Pranavkrishna481/6dof-robotic-arm-gesture-control

## One-line pitch
A wearable glove reads hand tilt/twist via an accelerometer and drives a 6-DOF robotic arm live over Bluetooth, with a separate browser-based control console (sliders, pre-built choreographed demos, live telemetry) that can take over control at any time.

## System architecture
Two Arduino boards + one browser app, talking over two independent serial links:

```
[Glove/Master Arduino]  --HC-05 Bluetooth-->  [Arm/Slave Arduino]  <--USB Serial-->  [Browser Console]
  MPU6050 (I2C)                                 PCA9685 (I2C) -> 5x MG995 servo
  trigger button                                A4988/DRV8825 -> NEMA17 base stepper
  status LED
```

- **Glove → Arm**: one-way, live gesture data, 20Hz.
- **Console ↔ Arm**: two-way over USB, a full line protocol (manual joint control, demo triggering, ownership arbitration, telemetry).
- The arm arbitrates between the two inputs — glove drives by default, but the console can claim control at any time (e.g., dragging a slider auto-claims it).

## Hardware

**Glove (master) board:**
- MPU6050 accelerometer, I2C address 0x68, used for pitch/roll only (accelerometer-derived, no gyro fusion)
- Momentary trigger button, digital pin 15 (=A1), `INPUT_PULLUP`
- Status LED, pin 2 — blinks N times to confirm which mode (1–5) is now active
- HC-05 Bluetooth, SoftwareSerial on pins 4 (RX) / 5 (TX), 9600 baud
- (Unused fallback path exists in code for a flex sensor on A0 instead of the button, threshold 600 — not currently wired)

**Arm (slave) board:**
- PCA9685 PWM driver, I2C address 0x40, driving 6 channels:
  - Ch0/1: dual-servo Shoulder (L/R, mirrored)
  - Ch2: Elbow
  - Ch3: Wrist1 (pitch-style joint)
  - Ch4: Wrist2 (roll-style joint)
  - Ch5: Gripper
- NEMA17 stepper for Base rotation, via A4988/DRV8825-style driver: STEP=pin8, DIR=pin9, EN=pin7 (active LOW), microstepping configurable (currently set to quarter-step, 4x)
- HC-05 Bluetooth, SoftwareSerial on pins 10 (RX) / 11 (TX), 9600 baud — receives from glove
- USB serial to the host PC/console, 115200 baud

**Base joint constraint:** deliberately limited to soft limits (~±90° by default, in microsteps) rather than full rotation, because the arm's servo wiring and the BT antenna physically ride on top of this joint and would wind up with unlimited rotation. Widening this requires adding a slip ring.

## Glove → Arm packet format
Comma-separated, newline-terminated, sent at ~20Hz:
```
mode,deltaPitch,deltaRoll\n
```
- `deltaPitch`/`deltaRoll` are relative to a recentered baseline captured fresh every time the mode changes — so switching modes never causes a jump, motion is always relative to "wherever your hand already is" when you switch.

## Mode switching (glove side): tap-count burst addressing
Modes are selected by tapping the trigger button N times in quick succession, not by cycling one-at-a-time:
- 1 tap → Mode 1: Shoulder (pitch)
- 2 taps → Mode 2: Elbow (pitch)
- 3 taps → Mode 3: Wrist (pitch = Wrist1, roll = Wrist2, simultaneously)
- 4 taps → Mode 4: Gripper (roll only — mimics twisting to grab)
- 5 taps → Mode 5: Base (roll only — same gesture language as Gripper)
- 6+ taps wraps back around via modulo, so overshooting is forgiving

Taps are counted in a burst; the mode commits ~450ms after the last tap (needed to detect the burst has ended). LED blinks N times to confirm, non-blockingly, so gesture data keeps streaming at 20Hz throughout — an earlier version blocked on `delay()` during the blink, which caused the arm to falsely think the link had gone stale.

Choreographed demos are **not** triggerable from the glove at all, by design — the glove only ever sends live gesture packets for modes 1–5; demo triggering is console/serial-only, so the glove never has to juggle live control and choreography at once.

## Arm-side control logic
- **Velocity-style control**: `applyVelocity()` takes the incoming delta and ramps the joint target, with a 5° deadzone and three speed zones (slow/medium/fast depending on how far the hand has moved from center) — so small unintentional tilts don't cause motion, and bigger tilts move faster.
- **Direction inversion**: each joint has an `INVERT_*` boolean (Shoulder/Elbow/Wrist1/Wrist2/Gripper/Base) to flip gesture direction if it feels backwards for how the servo horn is physically mounted. `INVERT_WRIST1` is currently set `true` — hand-down was moving the wrist up, felt unintuitive, now flipped so hand-down moves the wrist down.
- **Link-loss freeze**: if no glove packet arrives for 500ms (`STALE_MS`) while the glove owns control, motion freezes rather than drifting on stale data. This freeze only applies when the glove owns control — it doesn't affect console-commanded motion.
- **Control arbitration** (`controlOwner`, 0=glove / 1=app): when the app owns control, glove packets are still parsed (to keep link-liveness bookkeeping honest) but their motion is ignored. Sending a `J` or `P` command from the console auto-claims app ownership, so dragging a slider "just works" without needing to explicitly hand off control first.
- **Demo interruption**: a genuine gesture from the glove immediately interrupts any running choreographed demo and hands control back to live gesture input.

## Console USB line protocol (v8+)
Alongside legacy single-char keys (still supported for backward compatibility with the old Serial Monitor workflow):
```
J <idx> <deg>            set one joint target (0=Shoulder 1=Elbow 2=Wrist1 3=Wrist2 4=Gripper)
P <sh> <el> <w1> <w2> <g>  set a full 5-joint pose at once
D <id>                    run demo 0-7 (see Choreography below)
M <0|1>                   control owner: 0=glove drives, 1=app drives
H                         stop/hold immediately
T <0|1>                   telemetry stream off/on
L                         print joint limits (machine-readable)
Z <ms>                    set the pre-demo "get ready" delay (default 2000ms)
```

## Telemetry
With `T 1`, the arm streams every 100ms:
```
# owner sh el w1 w2 gr base mode dPitch dRoll
```
(`#` prefix lets the console distinguish telemetry from log text.) The `mode dPitch dRoll` fields were added in v9 — previously the frame only had `owner sh el w1 w2 gr base`, so the console had no way to show what the glove was actually sending. They're appended at the end (not inserted) so anything still reading only the first 7 fields keeps working unchanged.

## Choreography (8 built-in demos, `D 0`–`D 7` or direct keys)
Every demo waits `DEMO_START_DELAY_MS` (2s by default, adjustable via `Z`) after being triggered before it actually starts moving — a "get ready" beat.

| ID | Key | Name | Notes |
|----|-----|------|-------|
| 0 | c | Calibration Sweep | Each joint in turn: center → min (hold) → max (hold) → center → pause, then next joint |
| 1 | w | Wave Cascade | Joints go out-and-back in a randomized (Fisher-Yates) staggered order each run |
| 2 | b | Synchronized Breathe | — |
| 3 | p | Stand Tall ↔ Full Down | User-capturable poses — via console's "Pose capture" tool or `t`/`d` keys |
| 4 | a | Nod | Shoulder/elbow ease into a pose, then Wrist1 does 3 quick back-and-forth dips — tuned to read as "looking at someone" |
| 5 | e | Square | Draws a square via Shoulder/Elbow/Wrist1/Wrist2, Gripper acts as the "pen" — **corner poses are currently placeholders, not yet filled in** |
| 6 | s | Coordinate Preset 1 | Shoulder/Elbow/Wrist1 ease to a held pose → Wrist2 solo sweep → Gripper cycles (x2) → finale: Wrist2 eases home while Gripper cycles once more |
| 7 | f | Coordinate Preset 2 | Same structure as Preset 1, but Gripper only cycles once, all legs slightly faster |

## Safety / measured joint limits
Pre-loaded, confirmed-safe values (no re-jogging needed for these five):
- Shoulder: −60 to 15 (delta)
- Elbow: 0 to 110°
- Wrist1: 10 to 145°
- Wrist2: 5 to 170°
- Gripper: 60 to 173°
- Base: soft limit ±90° (in microsteps) — **flagged as new/unverified hardware, not yet jog-confirmed**, unlike the five servo joints above

## Software architecture notes (why v8/v9 look the way they do)
- **Killed a serious lag bug**: earlier versions used a blocking `BT.readStringUntil()`, which could stall the entire main loop for up to 1000ms on a single stray byte (SoftwareSerial noise, a floating RX pin generating phantom bytes even with the glove off). Rewritten as non-blocking char-buffer accumulators for both the USB and Bluetooth serial lines — bytes are drained every loop iteration, parsing only happens once a full line is present, and `loop()` never blocks.
- `delay()`-based motion pacing was also removed in favor of `millis()`-based timing, so easing/slew no longer throttles command handling.
- USB baud bumped from 9600 → 115200 (Bluetooth link stays at 9600, HC-05 default, unchanged).

## Bugs hit and fixed during this build
1. **Multi-second control lag** → root cause was the blocking `readStringUntil()` call described above; fixed by the non-blocking rewrite.
2. **Console showed no roll/pitch data, ever** → the telemetry frame the console reads never included mode/pitch/roll in the first place, and the one line that did print them was gated to only print when telemetry was *off* — but the console turns telemetry on immediately on connect, so that debug line was permanently suppressed the moment the console connected. Not a Bluetooth issue at all — the data just never left the arm in the stream the console was reading. Fixed by appending mode/dPitch/dRoll onto the end of the telemetry frame.
3. **HC-05 modules paired (LEDs synced) but zero data reaching the arm** → root cause was swapped TX/RX wiring between the two HC-05 modules. Pairing state (radio-to-radio link) is independent of whether each module's UART pins are correctly wired to its own Arduino — the LEDs syncing only confirmed the former, not the latter.
4. **Wrist1 gesture direction felt backwards** (hand down moved the wrist up) → fixed via the existing `INVERT_WRIST1` flag, flipped to `true`.

## Current status
Fully working end-to-end: glove gesture control, Bluetooth link, browser console (manual sliders, ownership handoff, live telemetry readout, choreographed demos, pose capture), and firmware are all confirmed functioning together.

## Known open items / not yet finished
- Square demo (`e`/`D 5`) corner poses are still placeholders — arm will sit still until real angles are filled in; loop-vs-draw-once behavior also undecided.
- Base joint has not yet been jog-confirmed on real hardware (soft limits are a conservative default, not measured).
- Stand Tall / Full Down poses for the Pose Cycle demo are not yet captured with real angles.
- Wrist2 (`Mode 3` roll) direction is flagged in code comments as possibly inverted from what feels intuitive — not yet resolved/tested like Wrist1 was.
