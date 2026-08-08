# Gesture-Controlled 6-DOF Robotic Arm — Trigger-Cycle

You wear a glove. You tilt your hand. A robotic arm mirrors you live, over Bluetooth — no wires between you and it. When you're not wearing the glove, a browser-based console lets you drive every joint with sliders, run pre-built choreographed demos, and watch live telemetry.

This repo has the CAD files, full firmware, the console, the parts list with prices, and an honest account of every problem we ran into and how we fixed it — so you can build one yourself.

Built by Pranav (https://github.com/Pranavkrishna481) and Jahnavi (https://github.com/jahnavikuruvalli) , presented as a minor project to DST (Department of Science and Technology) and JNTUH's incubator (JTBI).

## Table of Contents

- [What This Project Does](#what-this-project-does)
- [Demo](#demo)
- [How It Works](#how-it-works)
  - [Mode Switching: Tap to Choose a Joint](#mode-switching-tap-to-choose-a-joint)
- [The Console](#the-console)
- [CAD Files](#cad-files)
- [Bill of Materials](#bill-of-materials)
- [Power Requirements](#power-requirements)
- [Getting Started](#getting-started)
  - [1. 3D Print & Source the Parts](#1-3d-print--source-the-parts)
  - [2. Calibrate Servo Neutrality (Read This Before Assembling)](#2-calibrate-servo-neutrality-read-this-before-assembling)
  - [3. Assemble the Arm](#3-assemble-the-arm)
  - [4. Wire Everything Up](#4-wire-everything-up)
  - [5. Upload the Code](#5-upload-the-code)
  - [6. Open the Console and Move It](#6-open-the-console-and-move-it)
- [Challenges We Faced](#challenges-we-faced)
- [Technical Deep Dive](#technical-deep-dive)
  - [System Architecture](#system-architecture)
  - [Communication Protocol](#communication-protocol)
  - [Control Logic](#control-logic)
  - [Choreography / Demos](#choreography--demos)
  - [Bugs We Hit and Fixed](#bugs-we-hit-and-fixed)
- [Status & Roadmap](#status--roadmap)
- [Known Open Items](#known-open-items)
- [Credits](#credits)
- [License](#license)

## What This Project Does

A wearable glove reads how your hand tilts and twists using an accelerometer. That motion streams live over Bluetooth to a 6-DOF robotic arm, which mirrors it joint by joint. Tapping a button on the glove a set number of times switches which joint you're currently controlling — so one glove can drive the entire arm: shoulder, elbow, wrist, gripper, and base rotation.

You don't need the glove to use it, either. A browser-based console can take over control at any moment — drag a slider and it instantly claims control from the glove. From there you get manual joint sliders, one-click choreographed demos, pose capture, and a live readout of exactly what the arm (and the glove) are doing.

## Demo




![Arm base position](media/arm1.jfjf)
![Arm tall position](media/arm2.jfjf)
![console](media/console.png)




## How It Works

**In plain terms:** put the glove on, tap the button to pick a joint, then tilt your hand — the arm moves the same way. Tap again to switch to a different joint. At any time, someone can open the console in a browser and take over with sliders instead.

**Under the hood:** two Arduino boards talk to each other and to a browser app, over two independent links:

```
[Glove/Master Arduino]  --HC-05 Bluetooth-->  [Arm/Slave Arduino]  <--USB Serial-->  [Browser Console]
   MPU6050 (tilt sensing)                        PCA9685 -> 5x MG995 servos
   trigger button (mode select)                   Stepper driver -> NEMA17 base
```

- **Glove → Arm:** one-way, live gesture data, sent 20 times per second.
- **Console ↔ Arm:** two-way over USB — manual joint control, demo triggering, live telemetry.
- The arm decides who's "in control" at any moment — the glove drives by default, but moving a console slider instantly takes over.

### Mode Switching: Tap to Choose a Joint

**In plain terms:** tap the glove's button a certain number of times in a row, and that picks which joint your hand movement will control next. One tap = shoulder, two taps = elbow, and so on. A light on the glove blinks back the number of taps so you know it registered correctly.

| Taps | Joint | What tilting/twisting your hand does |
|---|---|---|
| 1 | Shoulder | Tilt controls shoulder pitch |
| 2 | Elbow | Tilt controls elbow pitch |
| 3 | Wrist | Tilt controls wrist pitch **and** roll at the same time |
| 4 | Gripper | Twist your hand to open/close the gripper |
| 5 | Base | Twist your hand to rotate the arm's base |
| 6+ | Wraps back to Mode 1 | Overshooting your tap count isn't a problem — it just wraps around |

**Technically:** taps are counted in a burst and the mode "commits" about 450ms after your last tap (needed to detect the burst has ended). Whenever you switch modes, the glove recenters its baseline tilt reading right then — so switching never causes the arm to jump, motion is always relative to wherever your hand already is at that moment. Choreographed demos can only be triggered from the console, never from the glove — the glove's only job is sending live gesture data.

## The Console

The browser-based console is a full second way to drive the arm, and it's just as central to this project as the glove.

![Console UI](media/console.png)


What it can do:
- **Manual joint sliders** — drag any joint to a target angle directly
- **Full pose control** — set all five servo joints at once
- **One-click choreographed demos** — 8 built-in demos, from a calibration sweep to a "nod" gesture to drawing a square
- **Pose capture** — record the arm's current position as a reusable pose
- **Live telemetry** — see exactly what the arm's joints are doing, and what the glove is currently sending, in real time
- **Instant control handoff** — touching any slider automatically takes control away from the glove, no manual switch needed

It talks to the arm's Arduino over USB using the **Web Serial API**, directly in the browser (Chrome or another Web Serial-compatible browser) — no app or driver install required.

## CAD Files
[Gripper_STL](https://www.thingiverse.com/thing:1748596)
[base_STL](https://www.thingiverse.com/thing:1750025)
[arm_joints_stl](https://www.thingiverse.com/thing:1838120)
print all the stl files provided!


## Bill of Materials

| Part | Quantity | Price(INR) |

| MG995 servo | 5 | 300-each |
| PCA9685 PWM driver | 1 | 250 |
| MPU6050 | 1 | 200 |
| HC-05 Bluetooth module | 2 | 200 |
| NEMA17 stepper motor | 1 | 550 |
| A4988 / DRV8825 stepper driver | 1 | 250 |
| Arduino (master + slave) | 2 | 500 |
| 3d printing will cost you around 2,000 to 3,000. | | |

## Power Requirements

- **Voltage:** 4.8V unloaded, up to 7V under full load
- **Current:** at least 4A once the full arm is assembled and running

Undersizing the power supply shows up as servo jitter, brownouts, or the arm resetting mid-motion — size accordingly.

## Getting Started

This walks you through the entire build, start to finish — not just the code.

### 1. 3D Print & Source the Parts

Print the arm's structural parts from the [CAD files](#cad-files), and gather the electronics from the [Bill of Materials](#bill-of-materials). Don't skip ahead to wiring before you've dry-fit the printed parts — check that servos physically seat correctly in their mounts before anything is glued, screwed, or wired.

### 2. Calibrate Servo Neutrality (Read This Before Assembling)

This step is easy to skip and caused us real problems — do this before mounting any servo into the arm.

**The problem:** every servo has an *electrical neutral* — the position it holds when sent a specific signal (usually the position corresponding to 90°). Separately, every joint on the arm has a *mechanical neutral* — the resting, aligned position that joint is designed around (e.g. the forearm sitting straight in line with its bracket).

If you bolt a servo horn onto a joint **without first lining these two up**, the servo's electrical range won't match the joint's real range of motion. In practice this means the joint hits its safe travel limit before it visually looks centered, or moves in a direction that doesn't match what you'd expect.

**How to avoid it:**
1. Power each servo individually through the PCA9685, *before* it's mounted into the arm.
2. Send it a signal corresponding to 90° (its electrical center) and let it hold there.
3. **Only now** attach the servo horn to the joint, positioned at that joint's own mechanical center (e.g. arm segment straight/aligned, gripper half-open).
4. Screw it down. The servo's electrical center and the joint's mechanical center are now aligned.
5. Repeat for every servo before final assembly.

Do this for all 5 servo joints before moving to the next step.

### 3. Assemble the Arm

With every servo pre-centered as above, assemble the arm structure joint by joint. Expect this to take a few passes — small misalignments in mounting show up as binding or uneven range of motion once everything's connected. Test each joint's free movement by hand (unpowered) before powering it.

### 4. Wire Everything Up

Wire the PCA9685 and NEMA17 stepper driver to the slave Arduino, and the MPU6050 and trigger button to the master (glove) Arduino, per the pin mapping in the [Technical Deep Dive](#system-architecture). Double-check the two HC-05 modules' TX/RX lines are **crossed** between the boards (TX→RX, not TX→TX) — this exact mistake cost us a lot of debugging time, see [Challenges](#challenges-we-faced).

### 5. Upload the Code

Flash the master sketch to the glove's Arduino and the slave sketch to the arm's Arduino (see [Repository Structure](#technical-deep-dive) for filenames).

### 6. Open the Console and Move It

Connect the arm's Arduino to your computer over USB, open `trigger-cycle-console.html` in Chrome (or another Web Serial-compatible browser), and connect to the arm's serial port. From here you can test every joint manually with sliders before ever putting the glove on — this is also a good way to re-verify your safe joint limits on your own hardware before trusting gesture control.

Once you're confident the console can move every joint safely, put the glove on, tap to select a mode, and start moving it live.

## Challenges We Faced

Building this wasn't a straight line — here's what actually slowed us down:

- **Working out the joint angles:** finding the correct, safe range of motion for each joint took real trial and error, not just reading a servo's datasheet.
- **Testing and mounting:** getting each joint mounted securely while still moving freely took several passes — small mechanical misalignments caused problems downstream.
- **General assembly issues:** fitting and wiring problems kept surfacing as the arm came together as a whole.
- **Setting safe movement limits:** we manually jogged each servo to find its real minimum and maximum safe angles, so the arm can't over-rotate and damage its own wiring or structure.
- **The Bluetooth link going silently stale:** the HC-05 modules would show as paired (LEDs synced) but zero data would reach the arm. Pairing state is just the radio link between the two modules — it says nothing about whether each module's UART pins are correctly wired to its own Arduino. Root cause: the TX/RX lines were wired straight across instead of crossed.
- **Moving from the Arduino Serial Monitor to a real web console:** everything was originally debugged through the basic Serial Monitor. Building a proper Web Serial-based browser console — sliders, live telemetry, demo triggers, pose capture — was its own project on top of the arm itself.

## Technical Deep Dive

The sections below are for anyone building on this code directly, or curious how it works internally.

### System Architecture

**Glove (master) board:**
- MPU6050 accelerometer (I2C, `0x68`) — pitch/roll only, no gyro fusion
- Trigger button on pin 15 (`A1`), `INPUT_PULLUP`
- Status LED on pin 2 — blinks N times to confirm the active mode
- HC-05 Bluetooth via SoftwareSerial, pins 4 (RX) / 5 (TX), 9600 baud

**Arm (slave) board:**
- PCA9685 PWM driver (I2C, `0x40`), driving 6 channels: Shoulder (dual, mirrored, Ch0/1), Elbow (Ch2), Wrist1 (Ch3), Wrist2 (Ch4), Gripper (Ch5)
- NEMA17 base stepper via A4988/DRV8825-style driver: STEP=pin8, DIR=pin9, EN=pin7 (active LOW), currently quarter-step microstepping
- HC-05 Bluetooth via SoftwareSerial, pins 10 (RX) / 11 (TX), 9600 baud
- USB serial to the console, 115200 baud

**Why the base joint has a limited rotation range:** the arm's servo wiring and the Bluetooth antenna physically ride on top of this joint, so unlimited rotation would wind them up. It's soft-limited to roughly ±90° in microsteps by default — widening this would require adding a slip ring.

### Communication Protocol

**Glove → Arm** (comma-separated, ~20Hz):
```
mode,deltaPitch,deltaRoll
```

**Console → Arm** (USB line protocol, v8+):
```
J <idx> <deg>              set one joint target (0=Shoulder 1=Elbow 2=Wrist1 3=Wrist2 4=Gripper)
P <sh> <el> <w1> <w2> <g>  set a full 5-joint pose at once
D <id>                     run demo 0-7
M <0|1>                    control owner: 0=glove drives, 1=app drives
H                          stop/hold immediately
T <0|1>                    telemetry stream off/on
L                          print joint limits
Z <ms>                     set the pre-demo "get ready" delay (default 2000ms)
```

**Arm → Console telemetry** (every 100ms when `T 1` is on):
```
# owner sh el w1 w2 gr base mode dPitch dRoll
```

### Control Logic

- **Velocity-style motion:** incoming deltas ramp the joint's target with a 5° deadzone and three speed zones, so small unintentional tilts don't move the arm, and larger tilts move faster.
- **Per-joint direction inversion:** each joint has an `INVERT_*` flag to flip gesture direction if it feels backwards for how its servo horn is physically mounted.
- **Link-loss freeze:** if no glove packet arrives for 500ms while the glove owns control, the arm freezes rather than drifting on stale data. This only applies while the glove owns control — it doesn't affect console-commanded motion.
- **Control arbitration:** the arm tracks who currently owns control (glove or console). Sending any `J` or `P` command from the console automatically claims ownership, so dragging a slider "just works."
- **Demo interruption:** a real gesture from the glove immediately interrupts any running demo and hands control back to live input.

### Choreography / Demos

Every demo waits 2 seconds by default (adjustable) after being triggered before it starts moving — a "get ready" beat.

| ID | Key | Name | Notes |
|---|---|---|---|
| 0 | c | Calibration Sweep | Each joint in turn: center → min → max → center → pause |
| 1 | w | Wave Cascade | Joints move out-and-back in a randomized staggered order each run |
| 2 | b | Synchronized Breathe | — |
| 3 | p | Stand Tall ↔ Full Down | User-capturable poses via the console's pose-capture tool |
| 4 | a | Nod | Shoulder/elbow ease into position, then the wrist does 3 quick dips — tuned to read as "looking at someone" |
| 5 | e | Square | Draws a square using shoulder/elbow/wrist, gripper acts as the "pen" |
| 6 | s | Coordinate Preset 1 | Multi-joint sequence ending in a gripper-cycle finale |
| 7 | f | Coordinate Preset 2 | Same structure as Preset 1, slightly faster, gripper cycles once |

### Bugs We Hit and Fixed

1. **Multi-second control lag:** a blocking serial read call could stall the entire main loop for up to a full second on stray bytes. Rewritten as non-blocking buffer accumulators for both serial lines.
2. **Console showed no pitch/roll data, ever:** the debug line that printed this data was gated to only run when telemetry was *off* — but the console turns telemetry on immediately on connect, so the data never actually left the arm. Fixed by appending the fields onto the end of the telemetry frame instead.
3. **HC-05 modules paired but zero data reaching the arm:** pairing (the radio link) and correct UART wiring are two separate things — the LEDs syncing only confirmed the former. Root cause was swapped TX/RX wiring.
4. **Wrist gesture direction felt backwards:** fixed via the `INVERT_WRIST1` flag.

## Status & Roadmap

Fully working end-to-end: glove gesture control, the Bluetooth link, and the full browser console (sliders, ownership handoff, live telemetry, choreographed demos, pose capture) are all confirmed working together.

**Planned next:**
- LLM-based control via MCP (natural language commands to the arm)
- Vision-based pick-and-place using a phone camera
- Adaptive gripping (currently uses fixed min/max calibration per object, which can crush objects that don't match)

## Known Open Items

- The Square demo's corner poses are still placeholders — the arm will sit still until real angles are filled in.
- The base joint hasn't been jog-confirmed on real hardware yet — its soft limits are a conservative default, not a measured one.
- The Stand Tall / Full Down poses for the Pose Cycle demo aren't captured with real angles yet.
- Wrist2 direction may be inverted from what feels intuitive — flagged in code, not yet resolved like Wrist1 was.

## Credits

Built by Pranav, Jahnavi. 4th year ECE, JNTUH.

## License

MIT
