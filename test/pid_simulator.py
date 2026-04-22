"""
==============================================================================
DRONE PID SIMULATOR + LIVE STM32 TUNER
Stage 2 Learning Tool — by your firmware teacher
==============================================================================

TWO MODES:
  1. SIMULATOR MODE  — No hardware needed. Physics simulation in Python.
     See P, I, D effects live. Tilt drone with mouse or keyboard.

  2. LIVE STM32 MODE — Connect your Blue Pill + MPU6050 via USB-TTL.
     Real IMU data. Real PID. Tune gains from keyboard, they upload live.

CONTROLS (both modes):
  Q / A  →  Kp up / down      (×0.1 steps)
  W / S  →  Ki up / down      (×0.001 steps)
  E / D  →  Kd up / down      (×0.1 steps)
  ↑ / ↓  →  Setpoint up/down  (1° steps)
  R      →  Reset (zero angle, zero I term)
  M      →  Toggle mode (Simulator ↔ STM32)
  1/2/3  →  Load preset: 1=Underdamped, 2=Overdamped, 3=Tuned

  SIMULATOR ONLY:
  SPACE  →  Give the drone a kick (disturbance test)
  MOUSE  →  Click & drag to manually tilt drone
==============================================================================
"""

import pygame
import math
import time
import collections
import sys

# Try to import serial — only needed for STM32 mode
try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

# ==============================================================================
# CONFIGURATION
# ==============================================================================

SCREEN_W = 1200
SCREEN_H = 750
FPS      = 60

# Serial settings (change COM port to match your USB-TTL adapter)
SERIAL_PORT = "ttyUSB0"   # Windows: "COM3", Linux: "/dev/ttyUSB0", Mac: "/dev/cu.usbserial-..."
SERIAL_BAUD = 115200

# Graph settings
GRAPH_HISTORY = 300    # number of data points to show in scrolling graph
GRAPH_H       = 180    # height of graph panel in pixels

# ==============================================================================
# COLORS — Dark industrial theme
# ==============================================================================
BG          = (14,  17,  23)    # near-black background
PANEL       = (22,  27,  36)    # slightly lighter panel
BORDER      = (40,  50,  65)    # subtle border color
WHITE       = (230, 235, 245)
GREY        = (120, 130, 145)
DIM         = (60,  70,  85)

# Signal colors
C_ROLL      = (100, 210, 255)   # cyan-blue: roll angle line
C_SETPOINT  = (255,  80,  80)   # red:       setpoint line
C_PID       = (255, 200,  50)   # amber:     PID output line
C_P         = (80,  255, 120)   # green:     P term
C_I         = (255, 160,  60)   # orange:    I term
C_D         = (200,  80, 255)   # purple:    D term

# Drone colors
DRONE_BODY  = (50,  150, 255)
DRONE_ARM   = (70,  180, 255)
MOTOR_CW    = (255, 100,  50)   # clockwise motors
MOTOR_CCW   = (50,  255, 150)   # counter-clockwise motors
PROP_COLOR  = (200, 200, 200)

# UI colors
BTN_HOT     = (60,  120, 200)
BTN_NORMAL  = (35,  45,  60)
TEXT_BRIGHT = (240, 245, 255)
TEXT_DIM    = (100, 120, 140)

# ==============================================================================
# SIMULATOR PHYSICS
# ==============================================================================

class DroneSimulator:
    """
    Simple 1D rotational physics simulation.
    Simulates a drone trying to keep roll = setpoint.

    Physics:  τ = I × α
      τ = torque (our PID output)
      I = moment of inertia (resistance to rotation — heavier/bigger = harder to spin)
      α = angular acceleration (how fast rotation rate is changing)

    Each step:
      angular_accel  = torque / inertia
      angular_vel   += angular_accel × dt
      angle         += angular_vel × dt
      (+ damping: air resistance slows rotation slightly)
    """

    def __init__(self):
        self.angle        = 0.0   # current roll angle (degrees)
        self.angular_vel  = 0.0   # rotation rate (deg/s)
        self.inertia      = 0.08  # kg·m² — how hard it is to rotate
        self.damping      = 0.25  # air resistance coefficient

    def reset(self):
        self.angle       = 0.0
        self.angular_vel = 0.0

    def kick(self, impulse=80.0):
        """Apply a sudden angular velocity disturbance (like a wind gust)"""
        self.angular_vel += impulse

    def step(self, torque, dt):
        """
        Advance physics by dt seconds with given torque applied.
        torque: our PID output (positive = rotate right, negative = left)
        dt: time step in seconds
        """
        # τ = I × α  →  α = τ / I
        angular_accel = torque / self.inertia

        # Damping: proportional to current speed (like air friction)
        damping_torque = -self.damping * self.angular_vel
        angular_accel += damping_torque / self.inertia

        # Euler integration
        self.angular_vel += angular_accel * dt
        self.angle       += self.angular_vel * dt

        # Physical limits (drone would crash if tilted too much)
        self.angle = max(-89.0, min(89.0, self.angle))

        return self.angle


# ==============================================================================
# PID CONTROLLER (Python version — identical logic to STM32 C code)
# ==============================================================================

class PIDController:
    """
    This is the EXACT same logic as Section 13 in stm32_pid_imu.c
    Running in Python so you can experiment without hardware.
    """

    def __init__(self, Kp=1.2, Ki=0.01, Kd=0.4):
        self.Kp = Kp
        self.Ki = Ki
        self.Kd = Kd

        self.setpoint   = 0.0
        self.i_term     = 0.0
        self.last_error = 0.0

        # Individual terms (for display)
        self.p_term = 0.0
        self.i_term_val = 0.0
        self.d_term = 0.0
        self.output = 0.0

    def reset(self):
        self.i_term     = 0.0
        self.last_error = 0.0
        self.output     = 0.0

    def update(self, measured_angle, dt):
        error = self.setpoint - measured_angle

        # P
        self.p_term = self.Kp * error

        # I with anti-windup
        self.i_term += self.Ki * error * dt
        self.i_term  = max(-50.0, min(50.0, self.i_term))
        self.i_term_val = self.i_term

        # D
        self.d_term = self.Kd * (error - self.last_error) / dt if dt > 0 else 0.0

        self.output = self.p_term + self.i_term + self.d_term
        self.output = max(-100.0, min(100.0, self.output))

        self.last_error = error
        return self.output


# ==============================================================================
# SCROLLING GRAPH
# ==============================================================================

class ScrollingGraph:
    """
    Draws a scrolling time-series graph.
    Multiple signals, each with own color and scale.
    """

    def __init__(self, x, y, w, h, y_min=-180, y_max=180):
        self.rect  = pygame.Rect(x, y, w, h)
        self.y_min = y_min
        self.y_max = y_max
        self.signals = {}  # name → {values: deque, color: tuple}

    def add_signal(self, name, color, max_points=GRAPH_HISTORY):
        self.signals[name] = {
            'values': collections.deque(maxlen=max_points),
            'color' : color
        }

    def push(self, name, value):
        if name in self.signals:
            self.signals[name]['values'].append(value)

    def value_to_y(self, value):
        """Map a data value to a screen Y pixel"""
        ratio = (value - self.y_min) / (self.y_max - self.y_min)
        return self.rect.bottom - int(ratio * self.rect.height)

    def draw(self, surface):
        # Background
        pygame.draw.rect(surface, PANEL, self.rect)
        pygame.draw.rect(surface, BORDER, self.rect, 1)

        # Zero line
        zero_y = self.value_to_y(0)
        if self.rect.top < zero_y < self.rect.bottom:
            pygame.draw.line(surface, DIM,
                             (self.rect.left, zero_y),
                             (self.rect.right, zero_y), 1)

        # Grid lines at ±45°, ±90°
        for v in [-90, -45, 45, 90]:
            gy = self.value_to_y(v)
            if self.rect.top < gy < self.rect.bottom:
                pygame.draw.line(surface, (30, 38, 50),
                                 (self.rect.left, gy),
                                 (self.rect.right, gy), 1)

        # Draw each signal
        for name, sig in self.signals.items():
            vals = list(sig['values'])
            if len(vals) < 2:
                continue
            pts = []
            n = len(vals)
            for i, v in enumerate(vals):
                px = self.rect.left + int(i * self.rect.width / GRAPH_HISTORY)
                py = self.value_to_y(v)
                py = max(self.rect.top, min(self.rect.bottom, py))
                pts.append((px, py))
            pygame.draw.lines(surface, sig['color'], False, pts, 2)


# ==============================================================================
# DRONE DRAWING
# ==============================================================================

def draw_drone(surface, cx, cy, angle_deg, motor_speeds, size=120):
    """
    Draw a top-down view of an X-frame quadcopter tilted to angle_deg.
    motor_speeds: list of 4 floats [0,1] for M1,M2,M3,M4

    Frame layout (top view):
        M1(FL,CCW)    M3(FR,CW)
              \\       /
               [BODY]
              /       \\
        M2(RL,CW)     M4(RR,CCW)
    """

    # Arm positions relative to center (rotated by angle)
    arm_len  = size * 0.85
    motor_r  = int(size * 0.22)
    prop_r   = int(size * 0.32)
    body_r   = int(size * 0.22)

    # Four arm directions (45°, 135°, 225°, 315°)
    arm_dirs = [45, 135, 225, 315]
    motor_types = ['CCW', 'CW', 'CW', 'CCW']  # M1=FL,M2=RL,M3=FR,M4=RR

    rad = math.radians(angle_deg)

    # Helper: rotate a point around center
    def rot(x, y):
        rx = x * math.cos(rad) - y * math.sin(rad)
        ry = x * math.sin(rad) + y * math.cos(rad)
        return (int(cx + rx), int(cy + ry))

    # Draw shadow (gives 3D feel)
    shadow_offset = int(angle_deg * 0.3)
    for d in arm_dirs:
        r = math.radians(d)
        ex = int(cx + arm_len * math.cos(r)) + shadow_offset
        ey = int(cy + arm_len * math.sin(r)) + 4
        pygame.draw.line(surface, (20, 25, 32), (cx + shadow_offset, cy + 4), (ex, ey), 8)

    # Draw arms
    for i, (d, mtype) in enumerate(zip(arm_dirs, motor_types)):
        r = math.radians(d)
        end = rot(arm_len * math.cos(r), arm_len * math.sin(r))
        pygame.draw.line(surface, DRONE_ARM, (cx, cy), end, 7)

    # Draw propellers and motors
    for i, (d, mtype) in enumerate(zip(arm_dirs, motor_types)):
        r     = math.radians(d)
        mx    = arm_len * math.cos(r)
        my    = arm_len * math.sin(r)
        mpos  = rot(mx, my)
        spd   = motor_speeds[i] if i < len(motor_speeds) else 0.5
        col   = MOTOR_CW if mtype == 'CW' else MOTOR_CCW

        # Propeller disk (brightness based on speed)
        prop_alpha = int(40 + spd * 100)
        prop_surf  = pygame.Surface((prop_r*2, prop_r*2), pygame.SRCALPHA)
        pygame.draw.circle(prop_surf, (*col, prop_alpha), (prop_r, prop_r), prop_r)
        surface.blit(prop_surf, (mpos[0]-prop_r, mpos[1]-prop_r))

        # Motor hub
        pygame.draw.circle(surface, col,     mpos, motor_r)
        pygame.draw.circle(surface, (20,25,32), mpos, motor_r - 4)

        # Motor label
        font_sm = pygame.font.SysFont('Consolas', 13, bold=True)
        lbl = f"M{i+1}"
        txt = font_sm.render(lbl, True, col)
        surface.blit(txt, (mpos[0] - txt.get_width()//2, mpos[1] - txt.get_height()//2))

    # Draw body
    pygame.draw.circle(surface, (30, 40, 55),  (cx, cy), body_r + 4)
    pygame.draw.circle(surface, DRONE_BODY, (cx, cy), body_r)
    pygame.draw.circle(surface, (20, 30, 50),  (cx, cy), body_r - 8)

    # Front indicator (nose direction)
    nose = rot(0, -body_r + 5)
    pygame.draw.circle(surface, (255, 100, 50), nose, 5)


def draw_horizon(surface, cx, cy, angle_deg, w=300, h=80):
    """
    Draw an attitude indicator (artificial horizon).
    The horizon line tilts opposite to drone tilt (like a real flight instrument).
    """
    rect = pygame.Rect(cx - w//2, cy - h//2, w, h)
    pygame.draw.rect(surface, (20, 60, 100), rect)  # sky (blue)

    # Earth half — tilted by angle
    rad = math.radians(-angle_deg)
    half_w = w // 2 + 10
    # Four corners of a wide tilted rectangle for earth
    earth_pts = []
    for ex, ey in [(-half_w, 0), (half_w, 0), (half_w, h), (-half_w, h)]:
        rx = ex * math.cos(rad) - ey * math.sin(rad)
        ry = ex * math.sin(rad) + ey * math.cos(rad)
        earth_pts.append((int(cx + rx), int(cy + ry)))
    pygame.draw.polygon(surface, (120, 80, 30), earth_pts)  # earth (brown)

    # Horizon line
    hx1 = cx - int(half_w * math.cos(rad))
    hy1 = cy - int(half_w * math.sin(rad))
    hx2 = cx + int(half_w * math.cos(rad))
    hy2 = cy + int(half_w * math.sin(rad))
    pygame.draw.line(surface, WHITE, (hx1, hy1), (hx2, hy2), 2)

    # Fixed aircraft symbol
    pygame.draw.line(surface, (255, 220, 50), (cx-30, cy), (cx-10, cy), 3)
    pygame.draw.line(surface, (255, 220, 50), (cx+10, cy), (cx+30, cy), 3)
    pygame.draw.circle(surface, (255, 220, 50), (cx, cy), 4)

    pygame.draw.rect(surface, BORDER, rect, 2)


# ==============================================================================
# GAIN SLIDER WIDGET
# ==============================================================================

def draw_gain_slider(surface, x, y, label, value, min_v, max_v, color,
                     keys_up, keys_down, font, font_sm):
    """Draw a labeled parameter slider with keyboard hint"""
    bar_w = 200
    bar_h = 8

    # Label
    lbl = font.render(label, True, color)
    surface.blit(lbl, (x, y))

    # Value display
    val_str = f"{value:.3f}"
    val_txt = font.render(val_str, True, TEXT_BRIGHT)
    surface.blit(val_txt, (x + 60, y))

    # Bar background
    bar_y = y + 26
    pygame.draw.rect(surface, DIM, (x, bar_y, bar_w, bar_h), border_radius=4)

    # Bar fill
    fill = int((value - min_v) / (max_v - min_v) * bar_w)
    fill = max(0, min(bar_w, fill))
    if fill > 0:
        pygame.draw.rect(surface, color, (x, bar_y, fill, bar_h), border_radius=4)

    # Keyboard hint
    hint = font_sm.render(f"[{keys_up}]▲  [{keys_down}]▼", True, DIM)
    surface.blit(hint, (x + bar_w + 8, bar_y - 3))


# ==============================================================================
# MAIN APPLICATION
# ==============================================================================

def main():
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H))
    pygame.display.set_caption("Drone PID Simulator — STM32")
    clock = pygame.time.Clock()

    # Fonts
    font_big  = pygame.font.SysFont('Consolas', 22, bold=True)
    font_med  = pygame.font.SysFont('Consolas', 17, bold=True)
    font_sm   = pygame.font.SysFont('Consolas', 13)
    font_tiny = pygame.font.SysFont('Consolas', 11)

    # ---- PID and physics ----
    pid  = PIDController(Kp=1.2, Ki=0.01, Kd=0.4)
    sim  = DroneSimulator()

    # ---- Graph ----
    graph = ScrollingGraph(20, SCREEN_H - GRAPH_H - 20,
                           SCREEN_W - 40, GRAPH_H,
                           y_min=-120, y_max=120)
    graph.add_signal('roll',     C_ROLL)
    graph.add_signal('setpoint', C_SETPOINT)
    graph.add_signal('pid',      C_PID)
    graph.add_signal('p',        C_P)
    graph.add_signal('i',        C_I)
    graph.add_signal('d',        C_D)

    # ---- State ----
    mode         = 'sim'   # 'sim' or 'stm32'
    ser          = None    # serial port object
    stm32_roll   = 0.0
    stm32_pid    = 0.0
    stm32_p = stm32_i = stm32_d = 0.0
    stm32_ready  = False
    serial_error = ""

    # Display toggles
    show_pid_terms = True
    show_explanation = True

    # History for stats
    roll_history = collections.deque(maxlen=200)

    # Presets
    presets = {
        '1': (0.5,  0.0,  0.0,  "Underdamped — Only P. Watch it oscillate!"),
        '2': (0.3,  0.0,  2.5,  "Overdamped  — High D. Watch it crawl slowly."),
        '3': (1.2,  0.01, 0.4,  "Tuned       — P+I+D balanced. Stable flight."),
    }

    # Explanation text for each mode
    explanations = {
        'sim': [
            "SIMULATOR MODE  — No hardware needed",
            "Press SPACE for a disturbance kick",
            "Tune Kp/Ki/Kd and watch the response",
            "Press 1/2/3 to try tuning presets",
        ],
        'stm32': [
            "STM32 MODE  — Real IMU data from Blue Pill",
            "Tilt the MPU6050 with your hand",
            "Gains update live on STM32 via UART",
            f"Port: {SERIAL_PORT} @ {SERIAL_BAUD} baud",
        ],
    }

    # For motor speed animation
    motor_phase = [0.0, 0.2, 0.4, 0.6]  # phase offset per motor

    prev_time = time.time()

    # Try to open serial for STM32 mode
    def try_open_serial():
        nonlocal ser, stm32_ready, serial_error
        if not SERIAL_AVAILABLE:
            serial_error = "pyserial not installed. Run: pip install pyserial"
            return
        try:
            ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=0.01)
            ser.flushInput()
            stm32_ready = False
            serial_error = ""
        except Exception as e:
            serial_error = str(e)
            ser = None

    # ---- MAIN LOOP ----
    running = True
    while running:
        now      = time.time()
        dt       = min(now - prev_time, 0.05)  # cap at 50ms to prevent wild jumps
        prev_time = now

        # ================================================================
        # EVENTS
        # ================================================================
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

            elif event.type == pygame.KEYDOWN:
                k = event.key

                # Kp
                if k == pygame.K_q:
                    pid.Kp = round(min(5.0,  pid.Kp + 0.1), 3)
                    if ser: ser.write(f"P{int(pid.Kp*100)}\n".encode())
                if k == pygame.K_a:
                    pid.Kp = round(max(0.0,  pid.Kp - 0.1), 3)
                    if ser: ser.write(f"P{int(pid.Kp*100)}\n".encode())

                # Ki
                if k == pygame.K_w:
                    pid.Ki = round(min(0.5,  pid.Ki + 0.001), 4)
                    if ser: ser.write(f"I{int(pid.Ki*100)}\n".encode())
                if k == pygame.K_s:
                    pid.Ki = round(max(0.0,  pid.Ki - 0.001), 4)
                    if ser: ser.write(f"I{int(pid.Ki*100)}\n".encode())

                # Kd
                if k == pygame.K_e:
                    pid.Kd = round(min(5.0,  pid.Kd + 0.1), 3)
                    if ser: ser.write(f"D{int(pid.Kd*100)}\n".encode())
                if k == pygame.K_d:
                    pid.Kd = round(max(0.0,  pid.Kd - 0.1), 3)
                    if ser: ser.write(f"D{int(pid.Kd*100)}\n".encode())

                # Setpoint
                if k == pygame.K_UP:
                    pid.setpoint = min(45.0, pid.setpoint + 1.0)
                    if ser: ser.write(f"S{int(pid.setpoint*100)}\n".encode())
                if k == pygame.K_DOWN:
                    pid.setpoint = max(-45.0, pid.setpoint - 1.0)
                    if ser: ser.write(f"S{int(pid.setpoint*100)}\n".encode())

                # Reset
                if k == pygame.K_r:
                    pid.reset()
                    sim.reset()

                # Kick (disturbance)
                if k == pygame.K_SPACE and mode == 'sim':
                    sim.kick(impulse=120.0)

                # Mode toggle
                if k == pygame.K_m:
                    mode = 'stm32' if mode == 'sim' else 'sim'
                    if mode == 'stm32':
                        try_open_serial()
                    else:
                        if ser: ser.close(); ser = None

                # Presets
                for pk in ['1', '2', '3']:
                    if k == getattr(pygame, f'K_{pk}'):
                        kp, ki, kd, _ = presets[pk]
                        pid.Kp = kp; pid.Ki = ki; pid.Kd = kd
                        pid.reset(); sim.reset()

                if k == pygame.K_ESCAPE:
                    running = False

        # ================================================================
        # UPDATE — SIMULATOR MODE
        # ================================================================
        roll_angle = 0.0

        if mode == 'sim':
            roll_angle  = sim.angle
            pid_out     = pid.update(roll_angle, dt)
            # Apply PID output as torque to physics simulation
            sim.step(pid_out, dt)

        # ================================================================
        # UPDATE — STM32 MODE
        # ================================================================
        elif mode == 'stm32' and ser:
            try:
                # Read all available lines from serial
                while ser.in_waiting:
                    line = ser.readline().decode('ascii', errors='ignore').strip()
                    if line == "DRONE_PID_READY":
                        stm32_ready = True
                    elif line.startswith('R,'):
                        parts = line.split(',')
                        if len(parts) >= 10:
                            # Decode ×100 encoded values
                            stm32_roll = int(parts[1]) / 100.0
                            # setpoint from STM32 (parts[2]) — we override with our pid.setpoint
                            stm32_pid  = int(parts[3]) / 100.0
                            stm32_p    = int(parts[4]) / 100.0
                            stm32_i    = int(parts[5]) / 100.0
                            stm32_d    = int(parts[6]) / 100.0
                            # Update local PID display with STM32 values
                            pid.p_term    = stm32_p
                            pid.i_term_val = stm32_i
                            pid.d_term    = stm32_d
                            pid.output    = stm32_pid
                roll_angle = stm32_roll
            except Exception as e:
                serial_error = str(e)

        # Update graph
        graph.push('roll',     roll_angle)
        graph.push('setpoint', pid.setpoint)
        graph.push('pid',      pid.output if mode == 'sim' else stm32_pid)
        if show_pid_terms:
            graph.push('p', pid.p_term)
            graph.push('i', pid.i_term_val)
            graph.push('d', pid.d_term)

        roll_history.append(roll_angle)

        # Motor speeds (visual only — proportional to PID output direction)
        out_norm = (pid.output if mode == 'sim' else stm32_pid) / 100.0
        base_spd = 0.5
        motor_speeds = [
            base_spd + out_norm * 0.3,   # M1 FL
            base_spd - out_norm * 0.3,   # M2 RL
            base_spd - out_norm * 0.3,   # M3 FR
            base_spd + out_norm * 0.3,   # M4 RR
        ]
        for i in range(4):
            motor_speeds[i] = max(0.05, min(1.0, motor_speeds[i]))
            motor_phase[i] += motor_speeds[i] * dt * 15

        # ================================================================
        # DRAW
        # ================================================================
        screen.fill(BG)

        # ---- Left panel: drone visualization ----
        drone_cx, drone_cy = 200, 260

        # Horizon indicator
        draw_horizon(screen, drone_cx, 90, roll_angle)

        # 3D tilt shadow behind drone
        pygame.draw.ellipse(screen, (18, 22, 30),
                            (drone_cx - 110, drone_cy + 80, 220, 30))

        # Draw drone top view
        draw_drone(screen, drone_cx, drone_cy, roll_angle, motor_speeds, size=110)

        # Angle arc indicator
        arc_r = 130
        start_a = -math.pi/2
        end_a   = -math.pi/2 + math.radians(roll_angle)
        if abs(roll_angle) > 0.5:
            arc_col = C_ROLL if abs(roll_angle) < 30 else (255, 80, 80)
            steps = max(2, abs(int(roll_angle)))
            pts = []
            for i in range(steps + 1):
                frac = i / steps
                a = start_a + (end_a - start_a) * frac
                pts.append((int(drone_cx + arc_r * math.cos(a)),
                             int(drone_cy + arc_r * math.sin(a))))
            if len(pts) >= 2:
                pygame.draw.lines(screen, arc_col, False, pts, 3)

        # Angle text
        angle_txt = font_big.render(f"{roll_angle:+.1f}°", True,
                                    C_ROLL if abs(roll_angle) < 20 else (255, 80, 80))
        screen.blit(angle_txt, (drone_cx - angle_txt.get_width()//2, drone_cy + 130))

        sp_txt = font_sm.render(f"Setpoint: {pid.setpoint:+.1f}°  [↑↓]", True, C_SETPOINT)
        screen.blit(sp_txt, (drone_cx - sp_txt.get_width()//2, drone_cy + 158))

        # ---- Center panel: PID gains ----
        gx = 410
        gy = 30

        title = "PID CONTROLLER" if mode == 'sim' else "STM32 LIVE TUNER"
        title_col = (100, 210, 255) if mode == 'sim' else (50, 255, 150)
        t = font_big.render(title, True, title_col)
        screen.blit(t, (gx, gy))

        # Mode badge
        mbadge = "[ SIMULATOR ]" if mode == 'sim' else "[ STM32 LIVE ]"
        mb = font_sm.render(mbadge, True, title_col)
        screen.blit(mb, (gx + t.get_width() + 20, gy + 4))

        gy += 40
        draw_gain_slider(screen, gx, gy, "Kp", pid.Kp, 0, 5, C_P,
                         'Q', 'A', font_med, font_sm)
        gy += 50
        draw_gain_slider(screen, gx, gy, "Ki", pid.Ki, 0, 0.5, C_I,
                         'W', 'S', font_med, font_sm)
        gy += 50
        draw_gain_slider(screen, gx, gy, "Kd", pid.Kd, 0, 5, C_D,
                         'E', 'D', font_med, font_sm)

        gy += 60

        # ---- PID term breakdown ----
        pw  = 320
        ph  = 90
        pr  = pygame.Rect(gx, gy, pw, ph)
        pygame.draw.rect(screen, PANEL, pr, border_radius=6)
        pygame.draw.rect(screen, BORDER, pr, 1, border_radius=6)

        terms = [
            ("P", pid.p_term,     C_P,  "Kp × error"),
            ("I", pid.i_term_val, C_I,  "sum of errors × Ki × dt"),
            ("D", pid.d_term,     C_D,  "rate of error change × Kd"),
        ]
        bar_max = 100.0
        for ti, (name, val, col, desc) in enumerate(terms):
            ty = gy + 8 + ti * 27
            # Label
            lbl = font_sm.render(f"{name}:", True, col)
            screen.blit(lbl, (gx + 8, ty))
            # Bar
            bx, bw = gx + 35, 160
            bc = bw // 2
            pygame.draw.rect(screen, DIM, (bx, ty + 4, bw, 14), border_radius=3)
            fill_w = int(abs(val) / bar_max * bc)
            fill_w = min(fill_w, bc)
            if val >= 0:
                pygame.draw.rect(screen, col, (bx + bc, ty + 4, fill_w, 14), border_radius=3)
            else:
                pygame.draw.rect(screen, col, (bx + bc - fill_w, ty + 4, fill_w, 14), border_radius=3)
            # Zero line
            pygame.draw.line(screen, WHITE, (bx + bc, ty + 2), (bx + bc, ty + 18), 1)
            # Value text
            vt = font_sm.render(f"{val:+6.2f}", True, WHITE)
            screen.blit(vt, (bx + bw + 6, ty))

        gy += ph + 12

        # Total PID output
        out_val = pid.output if mode == 'sim' else stm32_pid
        out_col = (100, 210, 255) if abs(out_val) < 30 else \
                  (255, 200, 50) if abs(out_val) < 70 else (255, 80, 80)
        ot = font_big.render(f"OUTPUT: {out_val:+7.2f}", True, out_col)
        screen.blit(ot, (gx, gy))
        gy += 35

        # Error display
        error = pid.setpoint - roll_angle
        et = font_med.render(f"ERROR:  {error:+7.2f}°", True, GREY)
        screen.blit(et, (gx, gy))
        gy += 30

        # Stats: settle time, overshoot
        if len(roll_history) > 10:
            rh  = list(roll_history)
            rms = math.sqrt(sum(v*v for v in rh) / len(rh))
            st  = font_sm.render(f"RMS error: {rms:.2f}°   (lower = better)", True, DIM)
            screen.blit(st, (gx, gy))

        # ---- Right panel: explanation / presets ----
        rx = 820
        ry = 30

        # Explanation box
        exp_rect = pygame.Rect(rx, ry, SCREEN_W - rx - 20, 200)
        pygame.draw.rect(screen, PANEL, exp_rect, border_radius=6)
        pygame.draw.rect(screen, BORDER, exp_rect, 1, border_radius=6)

        ey = ry + 12
        hdr = font_med.render("HOW IT WORKS", True, (200, 200, 100))
        screen.blit(hdr, (rx + 10, ey)); ey += 28

        pid_lines = [
            ("P term", "Proportional: bigger error → bigger push",    C_P),
            ("I term", "Integral: fixes persistent steady-state err", C_I),
            ("D term", "Derivative: brakes overshoot/oscillation",    C_D),
        ]
        for name, desc, col in pid_lines:
            nt = font_sm.render(f"■ {name}", True, col)
            dt2= font_sm.render(f"  {desc}", True, TEXT_DIM)
            screen.blit(nt, (rx + 10, ey))
            ey += 18
            screen.blit(dt2, (rx + 10, ey))
            ey += 20

        # Explanation lines for current mode
        ey += 6
        for line in explanations[mode]:
            lt = font_sm.render(line, True, DIM)
            screen.blit(lt, (rx + 10, ey)); ey += 17

        # Presets
        ey = ry + 220
        ph_txt = font_med.render("TUNING PRESETS", True, (200, 200, 100))
        screen.blit(ph_txt, (rx, ey)); ey += 25

        for pk, (kp, ki, kd, desc) in presets.items():
            is_active = abs(pid.Kp - kp) < 0.01 and abs(pid.Kd - kd) < 0.01
            col = (100, 255, 180) if is_active else DIM
            pt = font_sm.render(f"[{pk}] {desc}", True, col)
            screen.blit(pt, (rx, ey))
            pt2= font_sm.render(f"     Kp={kp}  Ki={ki}  Kd={kd}", True,
                                  (80, 100, 80) if not is_active else (60, 180, 100))
            screen.blit(pt2, (rx, ey + 15))
            ey += 36

        # STM32 status
        if mode == 'stm32':
            sy = ry + 440
            if ser and stm32_ready:
                sc = font_med.render("● STM32 CONNECTED", True, (50, 255, 120))
            elif ser:
                sc = font_med.render("◌ Waiting for STM32...", True, (255, 200, 50))
            else:
                sc = font_med.render(f"✗ {serial_error[:35]}", True, (255, 80, 80))
            screen.blit(sc, (rx, sy))

        # Sim mode instructions
        if mode == 'sim':
            sy = ry + 440
            keys = [
                ("[SPACE] Disturbance kick", (200, 200, 100)),
                ("[R] Reset angle & I term",  DIM),
                ("[M] Switch to STM32 mode",  DIM),
                ("[ESC] Quit",                DIM),
            ]
            for ktxt, kcol in keys:
                kt = font_sm.render(ktxt, True, kcol)
                screen.blit(kt, (rx, sy)); sy += 18

        # ---- Scrolling graph ----
        graph.draw(screen)

        # Graph legend
        legend_items = [
            ("Roll",     C_ROLL),
            ("Setpoint", C_SETPOINT),
            ("PID out",  C_PID),
            ("P",        C_P),
            ("I",        C_I),
            ("D",        C_D),
        ]
        lx = graph.rect.left + 10
        ly = graph.rect.top + 8
        for lname, lcol in legend_items:
            pygame.draw.rect(screen, lcol, (lx, ly + 3, 14, 10))
            lt = font_tiny.render(lname, True, lcol)
            screen.blit(lt, (lx + 18, ly))
            lx += lt.get_width() + 36

        # Graph y-axis labels
        for v in [-90, -45, 0, 45, 90]:
            gy2 = graph.value_to_y(v)
            lbl = font_tiny.render(f"{v:+d}°", True, DIM)
            screen.blit(lbl, (graph.rect.right - 32, gy2 - 6))

        # ---- Bottom status bar ----
        bar_y = SCREEN_H - GRAPH_H - 52
        status = f"dt={dt*1000:.1f}ms  FPS={clock.get_fps():.0f}  " \
                 f"Kp={pid.Kp:.3f}  Ki={pid.Ki:.4f}  Kd={pid.Kd:.3f}  " \
                 f"Setpoint={pid.setpoint:.1f}°  Roll={roll_angle:.1f}°  " \
                 f"Mode={'SIMULATOR' if mode=='sim' else 'STM32'}"
        st = font_sm.render(status, True, DIM)
        screen.blit(st, (20, bar_y))

        pygame.display.flip()
        clock.tick(FPS)

    if ser:
        ser.close()
    pygame.quit()
    sys.exit()


if __name__ == '__main__':
    main()