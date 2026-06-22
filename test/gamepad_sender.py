
import pygame, serial, time, sys, math

# ═══════════════════════════════════════════════════════════════
#  CONFIGURE HERE — only section you need to edit
# ═══════════════════════════════════════════════════════════════
SERIAL_PORT     = '/dev/ttyUSBO'   # Linux. Windows: 'COM3'
BAUD_RATE       = 115200

#  Axis numbers  ─  run with DRY_RUN=True first to find yours
AXIS_THROTTLE   = 1     # Left  stick UP/DOWN     →  throttle
AXIS_YAW        = 0     # Left  stick LEFT/RIGHT  →  yaw
AXIS_PITCH      = 3     # Right stick UP/DOWN     →  pitch
AXIS_ROLL       = 2     # Right stick LEFT/RIGHT  →  roll

#  Invert flags  ─  flip True/False if a channel is backwards
INVERT_THROTTLE = True  # True = push stick UP to increase throttle
INVERT_PITCH    = True  # True = push stick UP = pitch forward
INVERT_ROLL     = False
INVERT_YAW      = False

#  Limits
DEADZONE        = 0.04  # ignore tiny stick movements (drift)
MAX_CORRECTION  = 0.30  # max roll/pitch/yaw correction value ±0.30

LOOP_HZ         = 50    # send rate (50 times per second)
DRY_RUN         = False # True = no serial, just prints (for testing)
# ═══════════════════════════════════════════════════════════════


# ───────────────────────────────────────────────────────────────
#  HELPERS
# ───────────────────────────────────────────────────────────────
def deadzone(v, dz):
    """Remove small noise around center. Rescale remaining range."""
    if abs(v) < dz:
        return 0.0
    sign = 1.0 if v > 0 else -1.0
    return sign * (abs(v) - dz) / (1.0 - dz)

def bar(v, w=20):
    """Left-anchored bar: v is 0.0 to 1.0"""
    n = int(max(0.0, min(1.0, v)) * w)
    return "█" * n + "░" * (w - n)

def cbar(v, w=20):
    """Center-anchored bar: v is -1.0 to +1.0"""
    mid = w // 2
    n = int(abs(max(-1.0, min(1.0, v))) * mid)
    if v >= 0:
        return "░" * mid + "█" * n + "░" * (mid - n)
    else:
        return "░" * (mid - n) + "█" * n + "░" * mid

def clamp(v, lo, hi):
    return max(lo, min(hi, v))


# ───────────────────────────────────────────────────────────────
#  MOTOR MIXER  (identical math to Flix control.ino)
# ───────────────────────────────────────────────────────────────
def mix(T, R, P, Y):
    """
    Input:  T=throttle 0..1   R=roll ±MAX   P=pitch ±MAX   Y=yaw ±MAX
    Output: m1..m4  each 0..1  (clamped)

    Layout (top view, X = front):
          M1(FL)  ←front→  M2(FR)
          M3(BL)           M4(BR)

    M1 Front-Left  = T - R + P + Y   (CCW)
    M2 Front-Right = T + R + P - Y   (CW)
    M3 Back-Left   = T - R - P - Y   (CW)
    M4 Back-Right  = T + R - P + Y   (CCW)
    """
    m1 = clamp(T - R + P + Y, 0.0, 1.0)
    m2 = clamp(T + R + P - Y, 0.0, 1.0)
    m3 = clamp(T - R - P - Y, 0.0, 1.0)
    m4 = clamp(T + R - P + Y, 0.0, 1.0)
    return m1, m2, m3, m4


# ───────────────────────────────────────────────────────────────
#  AXIS DEBUG  ─  call once if unsure about axis mapping
# ───────────────────────────────────────────────────────────────
def print_all_axes(joy):
    print("\n── ALL AXES (move sticks to identify) ──")
    for i in range(joy.get_numaxes()):
        v = joy.get_axis(i)
        b = bar((v + 1) / 2, 24)
        print(f"  Axis {i}:  {v:+.3f}  [{b}]")
    print("  (Ctrl+C to stop, then set axis numbers above)")
    print()


# ───────────────────────────────────────────────────────────────
#  SERIAL  ─  open with retry
# ───────────────────────────────────────────────────────────────
def open_serial(port, baud):
    if DRY_RUN:
        print("  DRY RUN mode — no serial port used")
        return None
    try:
        s = serial.Serial(port, baud, timeout=0.02)
        time.sleep(2.0)          # wait for ESP32 to reboot
        return s
    except serial.SerialException as e:
        print(f"  Serial ERROR: {e}")
        print(f"  Check port. Linux: ls /dev/ttyUSB*  or  ls /dev/ttyACM*")
        print(f"  Continuing in DRY RUN mode...")
        return None


# ───────────────────────────────────────────────────────────────
#  MAIN
# ───────────────────────────────────────────────────────────────
def main():
    # ── Pygame / joystick init ──────────────────────────────
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("ERROR: No joystick found. Plug one in and retry.")
        sys.exit(1)

    joy = pygame.joystick.Joystick(0)
    joy.init()

    print("═" * 52)
    print("  DRONE MOTOR MIXER")
    print("═" * 52)
    print(f"  Joystick : {joy.get_name()}")
    print(f"  Axes     : {joy.get_numaxes()}")
    print(f"  Port     : {SERIAL_PORT}  ({BAUD_RATE} baud)")
    print(f"  Rate     : {LOOP_HZ} Hz")
    print(f"  Dry run  : {DRY_RUN}")
    print()

    # Show all axes once at startup so user can verify
    pygame.event.pump()
    print_all_axes(joy)
    print("  Starting in 2 seconds... (or set DRY_RUN=True to skip serial)")
    time.sleep(2)

    # ── Serial ──────────────────────────────────────────────
    ser = open_serial(SERIAL_PORT, BAUD_RATE)

    # Send zero packet first (safety)
    if ser:
        ser.write(b"0.000,0.000,0.000,0.000\n")
        print("  Zero packet sent.")

    print("  Running. Ctrl+C to stop.")
    print()

    interval   = 1.0 / LOOP_HZ
    last_esp32 = ""

    # ── Main loop ───────────────────────────────────────────
    try:
        while True:
            t0 = time.monotonic()
            pygame.event.pump()

            # ── Read joystick ──────────────────────────────
            raw_thr = joy.get_axis(AXIS_THROTTLE)
            raw_yaw = joy.get_axis(AXIS_YAW)
            raw_pit = joy.get_axis(AXIS_PITCH)
            raw_rol = joy.get_axis(AXIS_ROLL)

            # ── Convert to drone values ────────────────────
            # Throttle: raw -1..+1  →  0..1  (with invert)
            thr_raw  = -raw_thr if INVERT_THROTTLE else raw_thr
            T = clamp((thr_raw + 1.0) / 2.0, 0.0, 1.0)

            # Roll/Pitch/Yaw: raw -1..+1  →  ±MAX_CORRECTION
            R = deadzone(raw_rol, DEADZONE)
            P = deadzone(raw_pit, DEADZONE)
            Y = deadzone(raw_yaw, DEADZONE)

            if INVERT_ROLL:  R = -R
            if INVERT_PITCH: P = -P
            if INVERT_YAW:   Y = -Y

            R = clamp(R * MAX_CORRECTION, -MAX_CORRECTION, MAX_CORRECTION)
            P = clamp(P * MAX_CORRECTION, -MAX_CORRECTION, MAX_CORRECTION)
            Y = clamp(Y * MAX_CORRECTION, -MAX_CORRECTION, MAX_CORRECTION)

            # ── Motor mixer ────────────────────────────────
            m1, m2, m3, m4 = mix(T, R, P, Y)

            # ── Send to ESP32 ──────────────────────────────
            packet = f"{T:.3f},{R:.3f},{P:.3f},{Y:.3f}\n"
            if ser:
                ser.write(packet.encode())
                # Read any response from ESP32 (non-blocking)
                if ser.in_waiting:
                    line = ser.readline().decode(errors='ignore').strip()
                    if line:
                        last_esp32 = line

            # ── Terminal display ───────────────────────────
            print("\033[H", end="")   # jump cursor to top-left
            print("╔══════════════════════════════════════════════╗")
            print("║        DRONE MOTOR MIXER — LIVE              ║")
            print("╠══════════════════════════════════════════════╣")
            print(f"║  INPUTS                                      ║")
            print(f"║  Throttle  [{bar(T,22)}]  {T:.3f}  ║")
            print(f"║  Roll      [{cbar(R/MAX_CORRECTION,22)}]  {R:+.3f}  ║")
            print(f"║  Pitch     [{cbar(P/MAX_CORRECTION,22)}]  {P:+.3f}  ║")
            print(f"║  Yaw       [{cbar(Y/MAX_CORRECTION,22)}]  {Y:+.3f}  ║")
            print("╠══════════════════════════════════════════════╣")
            print(f"║  MOTOR OUTPUTS                               ║")
            print(f"║  M1 Front-Left  [{bar(m1,22)}]  {m1:.3f}  ║")
            print(f"║  M2 Front-Right [{bar(m2,22)}]  {m2:.3f}  ║")
            print(f"║  M3 Back-Left   [{bar(m3,22)}]  {m3:.3f}  ║")
            print(f"║  M4 Back-Right  [{bar(m4,22)}]  {m4:.3f}  ║")
            print("╠══════════════════════════════════════════════╣")
            sport = SERIAL_PORT if ser else "DRY RUN"
            print(f"║  Serial: {sport:<15}  {LOOP_HZ}Hz loop         ║")
            esp_line = last_esp32[:36] if last_esp32 else "waiting..."
            print(f"║  ESP32 : {esp_line:<36}  ║")
            print("╚══════════════════════════════════════════════╝")
            print()
            print("  Throttle up   → all 4 motors increase equally")
            print("  Pitch forward → M1,M2 up  M3,M4 down")
            print("  Roll right    → M2,M4 up  M1,M3 down")
            print("  Yaw right     → M1,M4 up  M2,M3 down")
            print()
            print("  Ctrl+C to stop safely")

            # ── Maintain loop rate ─────────────────────────
            elapsed = time.monotonic() - t0
            wait    = interval - elapsed
            if wait > 0:
                time.sleep(wait)

    except KeyboardInterrupt:
        print("\n\n  Stopping...")
        if ser:
            ser.write(b"0.000,0.000,0.000,0.000\n")
            time.sleep(0.1)
            ser.close()
        pygame.quit()
        print("  Motors zeroed. Serial closed. Done.")
        sys.exit(0)


if __name__ == "__main__":
    main()