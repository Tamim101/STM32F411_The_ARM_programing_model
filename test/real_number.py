import numpy as np
import matplotlib.pyplot as plt

# --- Drone parameters (typical Flix-sized quadcopter) ---
mass = 0.5          # kg  (500g drone)
g = 9.81            # m/s²
dt = 0.01           # time step (10ms, like Flix's control loop)
T = 5.0             # simulate 5 seconds

# thrustTarget sequence: hover → climb → descend → hover
def get_thrust_target(t):
    if t < 1.0:   return 0.5   # hover
    elif t < 2.5: return 0.65  # climb
    elif t < 3.5: return 0.35  # descend
    else:         return 0.5   # hover again

# Max total thrust = 2 * weight (so 0.5 = exactly hover)
max_thrust = 2 * mass * g

# --- Simulation ---
time = np.arange(0, T, dt)
z = np.zeros(len(time))      # altitude
vz = np.zeros(len(time))     # vertical velocity
az = np.zeros(len(time))     # acceleration
thrust_vals = np.zeros(len(time))

for i in range(1, len(time)):
    t = time[i]
    target = get_thrust_target(t)
    thrust = target * max_thrust        # total thrust (N)
    weight = mass * g                   # gravity (N)
    net_force = thrust - weight         # Newton's 2nd: ΣF
    az[i] = net_force / mass            # a = F/m
    vz[i] = vz[i-1] + az[i] * dt       # integrate: velocity
    z[i] = max(0, z[i-1] + vz[i] * dt) # integrate: position (can't go underground)
    thrust_vals[i] = target

# --- Plot ---
fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
fig.suptitle("Flix Quadcopter — Newton's Laws Simulation", fontsize=14, fontweight='bold')

axes[0].plot(time, thrust_vals, color='steelblue', linewidth=2, label='thrustTarget')
axes[0].axhline(0.5, color='green', linestyle='--', alpha=0.5, label='hover point')
axes[0].set_ylabel('thrustTarget (0–1)')
axes[0].legend(); axes[0].grid(alpha=0.3)

axes[1].plot(time, az, color='orange', linewidth=2, label='acceleration (m/s²)')
axes[1].axhline(0, color='gray', linestyle='--', alpha=0.5)
axes[1].set_ylabel('Acceleration (m/s²)')
axes[1].legend(); axes[1].grid(alpha=0.3)

axes[2].plot(time, z, color='crimson', linewidth=2, label='altitude (m)')
axes[2].set_ylabel('Altitude (m)')
axes[2].set_xlabel('Time (s)')
axes[2].legend(); axes[2].grid(alpha=0.3)

plt.tight_layout()
plt.savefig('flix_simulation.png', dpi=150, bbox_inches='tight')
plt.show()
print("Done! Check flix_simulation.png")





import pygame
import serial
import time

# ── CONFIG ────────────────────────────────────────────────
SERIAL_PORT = "COM3"      # Windows: COM3, COM4 etc.
                           # Linux/Mac: /dev/ttyUSB0 or /dev/cu.usbserial-...
BAUD_RATE   = 115200
DEADZONE    = 0.05         # ignore tiny stick wobble below this
# ──────────────────────────────────────────────────────────

def apply_deadzone(val, dz=DEADZONE):
    """If stick is barely moved, treat it as zero."""
    return val if abs(val) > dz else 0.0

def map_trigger(raw):
    """Triggers return -1 (released) to +1 (fully pressed). Map to 0..1."""
    return (raw + 1.0) / 2.0

pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("No gamepad found! Plug one in and retry.")
    exit()

joy = pygame.joystick.Joystick(0)
joy.init()
print(f"Connected: {joy.get_name()}")
print(f"Axes: {joy.get_numaxes()}, Buttons: {joy.get_numbuttons()}")

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)   # wait for ESP32 to boot
print(f"Serial open on {SERIAL_PORT}")
print("\n--- Controls ---")
print("LEFT  stick Y  (axis 1) : Thrust (up = more thrust)")
print("RIGHT stick X  (axis 3) : Roll   (right = roll right)")
print("RIGHT stick Y  (axis 4) : Pitch  (up = pitch forward)")
print("LEFT  stick X  (axis 0) : Yaw    (right = yaw right)")
print("Press START to arm, SELECT to disarm\n")

armed   = False
thrust  = 0.0
roll    = 0.0
pitch   = 0.0
yaw     = 0.0

try:
    while True:
        pygame.event.pump()   # process gamepad events

        # ── READ RAW AXES ─────────────────────────────────
        # Axis layout for most PS4/Xbox controllers:
        # Axis 0 = Left stick X   (-1 left,  +1 right)
        # Axis 1 = Left stick Y   (-1 up,    +1 down)   ← inverted!
        # Axis 2 = Left trigger   (-1=off, +1=full)
        # Axis 3 = Right stick X
        # Axis 4 = Right stick Y  (-1 up,    +1 down)   ← inverted!
        # Axis 5 = Right trigger

        raw_thrust = -joy.get_axis(1)   # invert: push up = positive
        raw_roll   =  joy.get_axis(3)
        raw_pitch  = -joy.get_axis(4)   # invert: push up = pitch forward
        raw_yaw    =  joy.get_axis(0)

        # ── APPLY DEADZONE ────────────────────────────────
        thrust = apply_deadzone(raw_thrust)
        roll   = apply_deadzone(raw_roll)
        pitch  = apply_deadzone(raw_pitch)
        yaw    = apply_deadzone(raw_yaw)

        # ── MAP TO DRONE RANGES ───────────────────────────
        # Real drones: thrust 0.0 to 1.0 (can't pull down)
        # Roll/Pitch/Yaw: -1.0 to +1.0 (bidirectional)
        # Stick gives -1 to +1, so:
        thrust = (thrust + 1.0) / 2.0   # remap -1..+1 → 0..1
        # Roll, pitch, yaw stay as -1..+1 but scaled down
        # so they don't saturate the mixer at full deflection
        roll  *= 0.3    # max 30% correction authority
        pitch *= 0.3
        yaw   *= 0.2

        # ── ARM / DISARM ──────────────────────────────────
        if joy.get_button(9):    # START button
            armed = True
            print("ARMED")
        if joy.get_button(8):    # SELECT button
            armed = False
            thrust = 0.0
            print("DISARMED")

        if not armed:
            thrust = 0.0

        # ── CALCULATE MIXER (same math as Flix!) ──────────
        m1 = thrust - roll + pitch + yaw   # front-left
        m2 = thrust + roll + pitch - yaw   # front-right
        m3 = thrust - roll - pitch - yaw   # back-left
        m4 = thrust + roll - pitch + yaw   # back-right

        # Constrain 0.0 to 1.0 (motor can't go negative)
        m1 = max(0.0, min(1.0, m1))
        m2 = max(0.0, min(1.0, m2))
        m3 = max(0.0, min(1.0, m3))
        m4 = max(0.0, min(1.0, m4))

        # ── SEND TO ESP32 ─────────────────────────────────
        packet = f"T:{thrust:.2f},R:{roll:.2f},P:{pitch:.2f},Y:{yaw:.2f}\n"
        ser.write(packet.encode())

        # ── LIVE DISPLAY ──────────────────────────────────
        print(f"\r  T={thrust:.2f} R={roll:+.2f} P={pitch:+.2f} Y={yaw:+.2f}"
              f"  |  M1={m1:.2f} M2={m2:.2f} M3={m3:.2f} M4={m4:.2f}"
              f"  {'[ARMED]' if armed else '[disarmed]'}", end="")

        time.sleep(0.02)   # 50Hz update rate (fast enough, not spammy)

except KeyboardInterrupt:
    # Safe shutdown — send zero thrust before quitting
    ser.write(b"T:0.00,R:0.00,P:0.00,Y:0.00\n")
    ser.close()
    pygame.quit()
    print("\nShutdown complete.")