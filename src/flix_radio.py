#!/usr/bin/env python3
"""
Fly the Flix drone with a BETAFPV LiteRadio 3 (USB joystick mode) using pyflix.
Smooth, Betaflight-style stick feel: expo, throttle mid/expo, stick smoothing.

Usage:
    python3 flix_radio.py --test   # only show stick values (no drone needed)
    python3 flix_radio.py          # connect to Flix and fly

Before flying: connect the computer to the Flix Wi-Fi and CLOSE QGroundControl.
"""
import sys
import time
import pygame

# ---------------- Axis numbers (check with --test) ----------------
AXIS_ROLL = 0
AXIS_PITCH = 1
AXIS_THROTTLE = 2
AXIS_YAW = 3

# ---------------- Set True if a stick moves the wrong way ----------------
INVERT_ROLL = False
INVERT_PITCH = False
INVERT_YAW = False
INVERT_THROTTLE = False

# ---------------- Feel settings (Betaflight-style) ----------------
# Expo: 0.0 = linear, higher = softer around center (Betaflight "RC Expo")
ROLL_EXPO = 0.30
PITCH_EXPO = 0.30
YAW_EXPO = 0.30

# Max stick output (1.0 = full). Lower = gentler drone (like lower rates)
ROLL_SCALE = 0.70
PITCH_SCALE = 0.70
YAW_SCALE = 0.80

# Throttle curve (Betaflight "Throttle MID" and "Throttle EXPO")
THR_MID = 0.50     # stick point where the curve is flattest (near hover)
THR_EXPO = 0.30    # 0 = linear, higher = finer control around THR_MID

# Smoothing time in seconds (like Betaflight RC smoothing). 0 = off
SMOOTH_RPY = 0.06
SMOOTH_THROTTLE = 0.08

DEADBAND = 0.03        # dead zone around stick center (roll/pitch/yaw)
DISARM_BUTTON = None   # e.g. 7 to disarm with a switch; None = not used
RATE_HZ = 50           # how often controls are sent


def read_axis(js, axis, invert, deadband=0.0):
    v = js.get_axis(axis)
    if invert:
        v = -v
    if abs(v) < deadband:
        v = 0.0
    elif deadband > 0:
        # rescale so output still starts smoothly from 0 after the deadband
        v = (abs(v) - deadband) / (1 - deadband) * (1 if v > 0 else -1)
    return max(-1.0, min(1.0, v))


def expo(x, e):
    """Betaflight RC expo: soft center, full range at the ends."""
    return x * (1 - e) + (x ** 3) * e


def throttle_curve(t, mid, e):
    """Betaflight throttle mid/expo curve, t in 0..1."""
    tmp = t - mid
    span = (1 - mid) if tmp > 0 else mid
    if span <= 0:
        return t
    return mid + tmp * (1 - e + e * (tmp * tmp) / (span * span))


class Smoother:
    """First-order low-pass filter."""
    def __init__(self, tau, dt):
        self.alpha = 1.0 if tau <= 0 else dt / (tau + dt)
        self.value = None

    def __call__(self, x):
        if self.value is None:
            self.value = x
        self.value += self.alpha * (x - self.value)
        return self.value


def main():
    test = "--test" in sys.argv
    dt = 1.0 / RATE_HZ

    pygame.init()
    pygame.joystick.init()
    if pygame.joystick.get_count() == 0:
        print("No joystick found. Plug in the radio in USB joystick mode.")
        return
    js = pygame.joystick.Joystick(0)
    js.init()
    print(f"Using: {js.get_name()}  ({js.get_numaxes()} axes, {js.get_numbuttons()} buttons)")

    s_roll, s_pitch, s_yaw = (Smoother(SMOOTH_RPY, dt) for _ in range(3))
    s_thr = Smoother(SMOOTH_THROTTLE, dt)

    flix = None
    if not test:
        from pyflix import Flix
        print("Connecting to Flix (join its Wi-Fi first)...")
        flix = Flix()
        print("Connected! Arm: left stick bottom-right. Disarm: bottom-left. Ctrl+C to stop.")

    try:
        while True:
            pygame.event.pump()

            roll = expo(read_axis(js, AXIS_ROLL, INVERT_ROLL, DEADBAND), ROLL_EXPO) * ROLL_SCALE
            pitch = expo(read_axis(js, AXIS_PITCH, INVERT_PITCH, DEADBAND), PITCH_EXPO) * PITCH_SCALE
            yaw = expo(read_axis(js, AXIS_YAW, INVERT_YAW, DEADBAND), YAW_EXPO) * YAW_SCALE

            t = (read_axis(js, AXIS_THROTTLE, INVERT_THROTTLE) + 1.0) / 2.0
            throttle = throttle_curve(t, THR_MID, THR_EXPO)

            roll, pitch, yaw = s_roll(roll), s_pitch(pitch), s_yaw(yaw)
            throttle = max(0.0, min(1.0, s_thr(throttle)))
            if t < 0.02:          # stick at bottom = exactly zero (needed to arm/disarm)
                throttle = 0.0
                s_thr.value = 0.0
                # full, unscaled yaw so the arm/disarm stick gesture always works
                yaw = read_axis(js, AXIS_YAW, INVERT_YAW, DEADBAND)

            if test:
                print(f"\rroll {roll:+.2f}  pitch {pitch:+.2f}  yaw {yaw:+.2f}  "
                      f"thr {throttle:.2f}   ", end="", flush=True)
            else:
                flix.set_controls(roll=roll, pitch=pitch, yaw=yaw, throttle=throttle)
                if DISARM_BUTTON is not None and js.get_button(DISARM_BUTTON):
                    flix.set_armed(False)

            time.sleep(dt)

    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        if flix is not None:
            flix.set_controls(roll=0, pitch=0, yaw=0, throttle=0)
            flix.set_armed(False)
            print("Disarmed.")
        pygame.quit()


if __name__ == "__main__":
    main()