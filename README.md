# Gesture-Controlled 6-DOF Robotic Arm

A 6-DOF robotic arm controlled by hand gestures. A wearable glove reads your hand movements and sends them wirelessly to the arm, which mirrors them in real time. The arm can also be controlled manually from a browser-based console.

## How It Works

- **Glove (master):** An **MPU6050** motion sensor reads hand orientation and tap gestures. Taps cycle between control modes — Shoulder, Elbow, Wrist, Gripper, and Base — so one glove can drive all the joints.
- **Wireless link:** The glove talks to the arm over **HC-05 Bluetooth**.
- **Arm (slave):** An Arduino receives the gesture data and drives:
  - 5x **MG995 servos** (via a **PCA9685** PWM driver) for the shoulder, elbow, wrist, and gripper joints
  - 1x **NEMA17 stepper motor** for base rotation
- **Manual control console:** A browser-based interface (`trigger-cycle-console.html`) uses the **Web Serial API** to let you manually control each joint with sliders, trigger preset demos, and view live telemetry — no app installation needed.

## Tech Stack

| Component | Role |
|---|---|
| MPU6050 | Motion/gesture sensing (glove) |
| HC-05 Bluetooth (x2) | Wireless link between glove and arm |
| PCA9685 | PWM driver for servo control |
| MG995 servos (x5) | Shoulder, elbow, wrist, gripper joints |
| NEMA17 stepper | Base rotation |
| Arduino | Master (glove) and slave (arm) controllers |
| Web Serial API | Browser-based manual control console |

## Repository Structure

```
├── glove-master/           # Arduino sketch for the MPU6050 glove (master)
├── arm-slave/               # Arduino sketch for the arm controller (slave)
├── trigger-cycle-console.html   # Web Serial browser console (sliders, demos, telemetry)
└── README.md
```

## Getting Started

1. Flash `glove-master` to the glove's Arduino and `arm-slave` to the arm's Arduino.
2. Pair the two HC-05 modules (ensure TX/RX are cross-wired correctly).
3. Power on the glove and the arm — gesture control should sync automatically.
4. To use manual control instead, open `trigger-cycle-console.html` in a Web Serial-compatible browser (e.g. Chrome) and connect to the arm's serial port.

## Status

Core gesture control and the Bluetooth link are fully working, including tap-based mode switching and live telemetry over the web console.

**Planned next:**
- LLM-based control via MCP (natural language commands to the arm)
- Vision-based pick-and-place using a phone camera
- Adaptive gripping (currently uses fixed min/max calibration per object)

## Background

This project was built as a minor project and presented to the Department of Science and Technology (DST) and JNTU's incubator (JTBI).

## License

MIT
