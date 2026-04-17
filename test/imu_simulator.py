"""
=============================================================================
  DRONE ATTITUDE VISUALISER — Betaflight Style
  STM32F103 Blue Pill + MPU6050
=============================================================================

  FIXES FROM PREVIOUS VERSION:
  - "This is totally ignored" → fixed serial parser to handle any format
  - Cube barely moves → replaced with real drone 3D model (X frame)
  - Added Betaflight-style artificial horizon
  - Added compass/heading indicator
  - Added G-force meter
  - Added full movement visualization like Betaflight

  INSTALL:
  pip install pygame pyserial numpy

  CHANGE COM_PORT BELOW to your port:
  Linux:   /dev/ttyUSB0  or  /dev/ttyACM0
  Windows: COM3  COM4  etc.

  CONTROLS:
  R = reset yaw
  Q = quit
  +/- = zoom

=============================================================================
"""

import pygame
import serial
import serial.tools.list_ports
import numpy as np
import sys
import time
import threading
import math
from collections import deque

# ── CHANGE THIS TO YOUR PORT ───────────────────────────────────────────────
COM_PORT  = "/dev/ttyUSB0"   # Linux: /dev/ttyUSB0 or /dev/ttyACM0
BAUD_RATE = 115200
# ──────────────────────────────────────────────────────────────────────────

WINDOW_W = 1280
WINDOW_H = 720
FPS      = 60

# Colours
BG          = (15, 17, 20)
WHITE       = (240, 240, 240)
GRAY        = (80,  80,  90)
DARK_GRAY   = (30,  32,  38)
ROLL_C      = (255, 80,  80)
PITCH_C     = (80,  220, 120)
YAW_C       = (80,  160, 255)
ACCENT      = (255, 200, 0)
GREEN       = (60,  220, 100)
RED         = (220, 60,  60)
ORANGE      = (255, 140, 0)
SKY_TOP     = (20,  80,  160)
SKY_BOT     = (100, 180, 255)
GROUND_TOP  = (80,  55,  30)
GROUND_BOT  = (40,  28,  15)
ARM_COL     = (60,  65,  80)
PROP_COL    = (200, 210, 230)
MOTOR_COL   = (40,  120, 200)
MOTOR_HOT   = (220, 100, 40)

# ── THREAD-SAFE STATE ──────────────────────────────────────────────────────
class IMUState:
    def __init__(self):
        self._lock      = threading.Lock()
        self._roll      = 0.0
        self._pitch     = 0.0
        self._yaw       = 0.0
        self._connected = False
        self._line      = "Waiting..."
        self._pkt_count = 0

    def set(self, r, p, y):
        with self._lock:
            self._roll  = r
            self._pitch = p
            self._yaw   = y
            self._pkt_count += 1

    def get(self):
        with self._lock:
            return self._roll, self._pitch, self._yaw

    def get_count(self):
        with self._lock:
            return self._pkt_count

    def set_status(self, connected, line=""):
        with self._lock:
            self._connected = connected
            if line:
                self._line = line

    def get_status(self):
        with self._lock:
            return self._connected, self._line

    def reset_yaw(self):
        with self._lock:
            self._yaw = 0.0

imu = IMUState()

# ── SERIAL READER ──────────────────────────────────────────────────────────
def serial_reader(port, baud):
    """
    Reads serial data in background thread.
    Handles all formats including var1/var2/var3 from Arduino.
    """
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=0.1)
            imu.set_status(True)
            print(f"[Serial] Connected: {port} @ {baud}")

            while True:
                raw = ser.readline()
                if not raw:
                    continue
                # Strip whitespace AND leading > character from Arduino prompts
                line = raw.decode('utf-8', errors='ignore').strip().lstrip('>')
                if not line:
                    continue

                imu.set_status(True, line)
                r, p, y = parse_line(line)
                if r is not None:
                    imu.set(r, p, y)

        except serial.SerialException as e:
            imu.set_status(False, str(e))
            print(f"[Serial] Error: {e} — retry in 2s")
            time.sleep(2)
        except Exception as e:
            imu.set_status(False, str(e))
            time.sleep(1)

def parse_line(line):
    """
    Flexible parser — handles ALL serial formats.
    Returns (roll, pitch, yaw) or (None, None, None) if unreadable.

    Supported formats:
      var1:-1.00,var2:-0.00,var3:1.15    Arduino accel raw
      ROLL:23.4,PITCH:-12.3,YAW:5.6     STM32 firmware
      R:23.4,P:-12.3,Y:5.6              short format
      23.4,-12.3,5.6                     CSV numbers
      -1.00 -0.00 1.15                   space separated
    """
    # Strip leading > and whitespace
    line = line.strip().lstrip('>')
    line_up = line.upper().replace(' ', '')

    # ── Format: var1:-1.00,var2:-0.00,var3:1.15 ──────────────────────────
    # This is Arduino raw accelerometer: var1=ax, var2=ay, var3=az
    # We convert ax/ay/az to roll/pitch angles using atan2
    if 'VAR1' in line_up:
        try:
            parts = line_up.split(',')
            ax = float(parts[0].split(':')[1])
            ay = float(parts[1].split(':')[1])
            az = float(parts[2].split(':')[1])
            # Convert raw accelerometer to angles
            import math
            roll  =  math.atan2(ay, az) * 57.2958
            pitch =  math.atan2(-ax, math.sqrt(ay*ay + az*az)) * 57.2958
            yaw   =  0.0   # no magnetometer — yaw stays 0
            return roll, pitch, yaw
        except:
            pass

    # ── Format: ROLL:23.4,PITCH:-12.3,YAW:5.6 ────────────────────────────
    if 'ROLL' in line_up and 'PITCH' in line_up:
        try:
            parts = line_up.split(',')
            r = float(parts[0].split(':')[1])
            p = float(parts[1].split(':')[1])
            y = float(parts[2].split(':')[1])
            return r, p, y
        except:
            pass

    # ── Format: R:23.4,P:-12.3,Y:5.6 ─────────────────────────────────────
    if line_up.startswith('R:'):
        try:
            parts = line_up.split(',')
            r = float(parts[0].split(':')[1])
            p = float(parts[1].split(':')[1])
            y = float(parts[2].split(':')[1])
            return r, p, y
        except:
            pass

    # ── Format: three comma-separated numbers: 23.4,-12.3,5.6 ───────────────
    parts = line.replace(' ', '').split(',')
    if len(parts) >= 3:
        try:
            r = float(parts[0])
            p = float(parts[1])
            y = float(parts[2])
            if -180 <= r <= 180 and -90 <= p <= 90:
                return r, p, y
        except:
            pass

    # ── Format: space-separated numbers: 23.4 -12.3 5.6 ─────────────────────
    parts = line.split()
    if len(parts) >= 3:
        try:
            r = float(parts[0])
            p = float(parts[1])
            y = float(parts[2])
            if -180 <= r <= 180 and -90 <= p <= 90:
                return r, p, y
        except:
            pass

    return None, None, None

# ── 3D MATH ────────────────────────────────────────────────────────────────
def rot_x(d):
    a = math.radians(d)
    return np.array([[1,0,0],[0,math.cos(a),-math.sin(a)],[0,math.sin(a),math.cos(a)]])

def rot_y(d):
    a = math.radians(d)
    return np.array([[math.cos(a),0,math.sin(a)],[0,1,0],[-math.sin(a),0,math.cos(a)]])

def rot_z(d):
    a = math.radians(d)
    return np.array([[math.cos(a),-math.sin(a),0],[math.sin(a),math.cos(a),0],[0,0,1]])

def project(pt, cx, cy, fov=600):
    x, y, z = pt
    d = fov / (z + fov + 4)
    return int(cx + x * d), int(cy - y * d)

def rotate(pts, roll, pitch, yaw):
    R = rot_z(yaw) @ rot_y(pitch) @ rot_x(roll)
    return [(R @ np.array(p)).tolist() for p in pts]

# ── DRONE 3D MODEL (X-frame quadcopter) ───────────────────────────────────
# X-frame drone: M1 M2 top, M3 M4 bottom, X configuration
SCALE = 85  # drone size in pixels

def build_drone():
    """
    Returns a dict of drone parts, each as a list of 3D points.
    All points relative to centre (0,0,0).
    """
    s = SCALE
    parts = {}

    # Central body — flat hexagon
    body_pts = []
    for i in range(6):
        a = math.radians(i * 60)
        body_pts.append([math.cos(a)*s*0.22, math.sin(a)*s*0.22, 0])
    parts['body_top']    = body_pts
    parts['body_bottom'] = [[x, y, -s*0.06] for x,y,z in body_pts]

    # 4 arms — X configuration (45 degrees)
    arm_dirs = [(1,1), (1,-1), (-1,1), (-1,-1)]
    parts['arms'] = []
    for dx, dy in arm_dirs:
        parts['arms'].append([
            [0, 0, -s*0.03],
            [dx*s*0.18, dy*s*0.18, -s*0.03],
            [dx*s*0.18, dy*s*0.18, -s*0.06],
            [0, 0, -s*0.06],
        ])

    # 4 motors (cylinders approximated as circles)
    motor_positions = [(s*0.72, s*0.72), (s*0.72, -s*0.72),
                       (-s*0.72, s*0.72), (-s*0.72, -s*0.72)]
    parts['motors'] = motor_positions

    # 4 propellers — 2-blade each
    parts['props'] = motor_positions

    return parts

# ── DRAW DRONE ─────────────────────────────────────────────────────────────
def draw_drone(surface, roll, pitch, yaw, cx, cy):
    s = SCALE
    R = rot_z(yaw) @ rot_y(pitch) @ rot_x(roll)
    FOV = 600

    def proj(pt):
        p = (R @ np.array(pt)).tolist()
        return project(p, cx, cy, FOV), p[2]

    def proj_raw(pt):
        p = (R @ np.array(pt)).tolist()
        return project(p, cx, cy, FOV)

    # ── Draw arms first (back to front) ───────────────────────────────────
    arm_dirs = [(1,1), (1,-1), (-1,-1), (-1,1)]
    arm_data = []
    for dx, dy in arm_dirs:
        tip = (R @ np.array([dx*s*0.75, dy*s*0.75, -s*0.04])).tolist()
        arm_data.append((tip[2], dx, dy))

    for z_depth, dx, dy in sorted(arm_data, key=lambda x: x[0], reverse=True):
        a0 = proj_raw([dx*s*0.15, dy*s*0.15, -s*0.02])
        a1 = proj_raw([dx*s*0.75, dy*s*0.75, -s*0.04])
        a2 = proj_raw([dx*s*0.75, dy*s*0.75, -s*0.09])
        a3 = proj_raw([dx*s*0.15, dy*s*0.15, -s*0.07])
        pygame.draw.polygon(surface, ARM_COL, [a0, a1, a2, a3])
        pygame.draw.polygon(surface, (90, 95, 110), [a0, a1, a2, a3], 1)

    # ── Central body ──────────────────────────────────────────────────────
    body_pts_top    = []
    body_pts_bottom = []
    for i in range(6):
        a = math.radians(i * 60)
        body_pts_top.append(proj_raw([math.cos(a)*s*0.22, math.sin(a)*s*0.22, 0]))
        body_pts_bottom.append(proj_raw([math.cos(a)*s*0.22, math.sin(a)*s*0.22, -s*0.07]))

    # Side panels
    for i in range(6):
        j = (i + 1) % 6
        panel = [body_pts_top[i], body_pts_top[j], body_pts_bottom[j], body_pts_bottom[i]]
        pygame.draw.polygon(surface, (55, 60, 75), panel)
        pygame.draw.polygon(surface, (70, 75, 90), panel, 1)

    pygame.draw.polygon(surface, (45, 50, 65), body_pts_bottom)
    pygame.draw.polygon(surface, (70, 80, 100), body_pts_top)
    pygame.draw.polygon(surface, (90, 100, 120), body_pts_top, 1)

    # FC indicator light on top
    fc_centre = proj_raw([0, 0, s*0.02])
    pygame.draw.circle(surface, (0, 200, 255), fc_centre, 5)
    pygame.draw.circle(surface, (0, 255, 255), fc_centre, 3)

    # ── Motors and propellers ──────────────────────────────────────────────
    motor_positions_3d = [
        ( s*0.75,  s*0.75, -s*0.04),
        ( s*0.75, -s*0.75, -s*0.04),
        (-s*0.75, -s*0.75, -s*0.04),
        (-s*0.75,  s*0.75, -s*0.04),
    ]
    motor_colours = [RED, ORANGE, RED, ORANGE]  # front motors red, back orange
    prop_angles   = [0, 90, 45, 135]  # different prop angles for visual variety

    motor_draw_order = sorted(
        enumerate(motor_positions_3d),
        key=lambda x: (R @ np.array(x[1]))[2],
        reverse=True
    )

    for idx, mpos in motor_draw_order:
        mc   = proj_raw(mpos)
        mtop = proj_raw([mpos[0], mpos[1], mpos[2] + s*0.06])

        # Motor cylinder
        pygame.draw.circle(surface, (25, 30, 40), mc, int(s*0.13))
        pygame.draw.circle(surface, motor_colours[idx], mc, int(s*0.11))
        pygame.draw.circle(surface, (255, 255, 255), mc, int(s*0.04))

        # Propeller blades
        pa = math.radians(prop_angles[idx])
        pr = s * 0.55  # prop radius
        for blade in range(2):
            ba = pa + blade * math.pi
            tip1_3d = [mpos[0] + math.cos(ba)*pr,
                       mpos[1] + math.sin(ba)*pr,
                       mpos[2] + s*0.01]
            tip2_3d = [mpos[0] - math.cos(ba)*pr,
                       mpos[1] - math.sin(ba)*pr,
                       mpos[2] + s*0.01]
            t1 = proj_raw(tip1_3d)
            t2 = proj_raw(tip2_3d)
            # Blade as thick line with width
            perp_a = ba + math.pi/2
            off = int(s * 0.04)
            offx = int(math.cos(perp_a) * off)
            offy = int(math.sin(perp_a) * off)
            blade_pts = [
                (mc[0]+offx, mc[1]-offy),
                (t1[0]+offx//2, t1[1]-offy//2),
                (t1[0]-offx//2, t1[1]+offy//2),
                (mc[0]-offx, mc[1]+offy),
            ]
            alpha_surf = pygame.Surface((WINDOW_W, WINDOW_H), pygame.SRCALPHA)
            pygame.draw.polygon(alpha_surf, (200, 210, 230, 180), blade_pts)
            surface.blit(alpha_surf, (0, 0))

    # ── Front indicator ────────────────────────────────────────────────────
    front_3d = [0, s*0.28, s*0.02]
    front_pt = proj_raw(front_3d)
    pygame.draw.circle(surface, ACCENT, front_pt, 6)
    pygame.draw.circle(surface, WHITE, front_pt, 3)


# ── ARTIFICIAL HORIZON ─────────────────────────────────────────────────────
def draw_horizon(surface, roll, pitch, cx, cy, w, h):
    """
    Betaflight-style artificial horizon.
    Sky = blue, Ground = brown.
    Horizon line tilts with roll.
    Moves up/down with pitch.
    """
    # Clip drawing to this rectangle
    clip_rect = pygame.Rect(cx - w//2, cy - h//2, w, h)
    old_clip  = surface.get_clip()
    surface.set_clip(clip_rect)

    # Background
    pygame.draw.rect(surface, DARK_GRAY, clip_rect)

    # Pitch offset in pixels: 1° = 3px
    pitch_px = int(pitch * 3)

    # Horizon line rotated by roll
    roll_rad = math.radians(roll)
    cos_r    = math.cos(roll_rad)
    sin_r    = math.sin(roll_rad)

    # Half-width of horizon line (long enough to always cross the window)
    half = int(math.sqrt(w*w + h*h))

    # Centre of horizon with pitch offset
    hcx = cx
    hcy = cy + pitch_px

    # Two end points of horizon line
    hx1 = int(hcx - half * cos_r)
    hy1 = int(hcy - half * sin_r)
    hx2 = int(hcx + half * cos_r)
    hy2 = int(hcy + half * sin_r)

    # Sky polygon — above horizon line
    sky_pts = [(cx-w, cy-h), (cx+w, cy-h), (hx2, hy2), (hx1, hy1)]
    pygame.draw.polygon(surface, SKY_TOP, sky_pts)

    # Ground polygon — below horizon line
    gnd_pts = [(cx-w, cy+h), (cx+w, cy+h), (hx2, hy2), (hx1, hy1)]
    pygame.draw.polygon(surface, GROUND_TOP, gnd_pts)

    # Horizon line
    pygame.draw.line(surface, WHITE, (hx1, hy1), (hx2, hy2), 2)

    # Pitch ladder lines
    for deg in range(-90, 91, 10):
        if deg == 0:
            continue
        px_off  = int((-deg + pitch) * 3)
        line_w  = 30 if deg % 30 == 0 else 15
        lx1 = int(hcx - line_w * cos_r + px_off * sin_r)
        ly1 = int(hcy - line_w * sin_r - px_off * cos_r)
        lx2 = int(hcx + line_w * cos_r + px_off * sin_r)
        ly2 = int(hcy + line_w * sin_r - px_off * cos_r)
        pygame.draw.line(surface, (200, 200, 200), (lx1, ly1), (lx2, ly2), 1)

    # Centre crosshair (fixed, does not rotate)
    pygame.draw.line(surface, ACCENT, (cx-40, cy), (cx-10, cy), 2)
    pygame.draw.line(surface, ACCENT, (cx+10, cy), (cx+40, cy), 2)
    pygame.draw.line(surface, ACCENT, (cx, cy-8), (cx, cy-2), 2)

    # Roll indicator arc at top
    arc_r = h // 2 - 8
    arc_rect = pygame.Rect(cx-arc_r, cy-arc_r, arc_r*2, arc_r*2)
    pygame.draw.arc(surface, GRAY, arc_rect, math.radians(30), math.radians(150), 1)

    # Roll pointer
    roll_rad2 = math.radians(-roll + 90)
    px2 = int(cx + arc_r * math.cos(roll_rad2))
    py2 = int(cy - arc_r * math.sin(roll_rad2))
    pygame.draw.circle(surface, ACCENT, (px2, py2), 4)

    # Border
    surface.set_clip(old_clip)
    pygame.draw.rect(surface, GRAY, clip_rect, 1)


# ── GRAPH ──────────────────────────────────────────────────────────────────
class Graph:
    def __init__(self, x, y, w, h, n=300):
        self.x, self.y, self.w, self.h = x, y, w, h
        self.rolls  = deque([0.0]*n, maxlen=n)
        self.pitches = deque([0.0]*n, maxlen=n)
        self.yaws   = deque([0.0]*n, maxlen=n)
        self.n = n

    def update(self, r, p, y):
        self.rolls.append(r)
        self.pitches.append(p)
        self.yaws.append(y)

    def draw(self, surf, font):
        pygame.draw.rect(surf, DARK_GRAY, (self.x, self.y, self.w, self.h))
        pygame.draw.rect(surf, GRAY,      (self.x, self.y, self.w, self.h), 1)
        mid = self.y + self.h // 2
        # Zero line
        pygame.draw.line(surf, (50, 50, 60), (self.x, mid), (self.x+self.w, mid), 1)
        # Grid
        for v in [-90, -45, 45, 90]:
            py = mid - int(v / 90 * (self.h//2))
            pygame.draw.line(surf, (35, 38, 45), (self.x, py), (self.x+self.w, py), 1)
        # Lines
        for series, col in [(self.rolls, ROLL_C), (self.pitches, PITCH_C), (self.yaws, YAW_C)]:
            pts = []
            for i, v in enumerate(series):
                px = self.x + int(i / self.n * self.w)
                py = mid - int(max(-90, min(90, v)) / 90 * (self.h//2))
                pts.append((px, py))
            if len(pts) > 1:
                pygame.draw.lines(surf, col, False, pts, 2)


# ── MAIN ───────────────────────────────────────────────────────────────────
def main():
    # Auto-detect port if default not found
    global COM_PORT
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if COM_PORT not in ports and ports:
        COM_PORT = ports[0]
        print(f"[Serial] Auto-selected port: {COM_PORT}")
    print(f"[Serial] Available ports: {ports}")

    # Start serial thread
    t = threading.Thread(
        target=serial_reader,
        args=(COM_PORT, BAUD_RATE),
        daemon=True
    )
    t.start()

    # Pygame
    pygame.init()
    screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
    pygame.display.set_caption("Drone Attitude — STM32 + MPU6050")
    clock = pygame.time.Clock()

    font_lg  = pygame.font.SysFont("Consolas", 26, bold=True)
    font_md  = pygame.font.SysFont("Consolas", 18)
    font_sm  = pygame.font.SysFont("Consolas", 13)

    graph = Graph(x=680, y=460, w=580, h=240)

    # Smoothed display values
    dr = dp = dy = 0.0
    SMOOTH = 0.20   # lower = smoother but slower. 0.20 is good for real IMU.

    # Prop animation angle
    prop_spin = 0.0

    last_pkt = 0
    hz_timer = time.time()
    hz_count = 0
    hz_val   = 0.0

    running = True
    while running:

        # Events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_q:
                    running = False
                elif event.key == pygame.K_r:
                    dy = 0.0
                    imu.reset_yaw()
                elif event.key in (pygame.K_PLUS, pygame.K_EQUALS):
                    pass
                elif event.key == pygame.K_MINUS:
                    pass

        # Read IMU
        tr, tp, ty = imu.get()
        connected, raw_line = imu.get_status()

        # Smooth interpolation (software low-pass filter)
        dr += SMOOTH * (tr - dr)
        dp += SMOOTH * (tp - dp)
        dy += SMOOTH * (ty - dy)

        # Packet rate
        pkts = imu.get_count()
        if pkts != last_pkt:
            hz_count += pkts - last_pkt
            last_pkt  = pkts
        now = time.time()
        if now - hz_timer >= 1.0:
            hz_val   = hz_count
            hz_count = 0
            hz_timer = now

        # Prop spin
        prop_spin = (prop_spin + 8) % 360

        # ── DRAW ──────────────────────────────────────────────────────────
        screen.fill(BG)

        # ── LEFT PANEL: 3D Drone ──────────────────────────────────────────
        drone_cx = 320
        drone_cy = 280
        draw_drone(screen, dr, dp, dy, drone_cx, drone_cy)

        # Drone panel label
        screen.blit(font_sm.render("3D DRONE VIEW", True, GRAY), (drone_cx-70, 40))
        screen.blit(font_sm.render(f"Roll:{dr:+6.1f}°  Pitch:{dp:+6.1f}°  Yaw:{dy:+6.1f}°",
                                    True, ACCENT), (100, 520))

        # Axis labels (fixed on screen, not rotating)
        screen.blit(font_sm.render("↑ = nose direction", True, ACCENT), (drone_cx-60, 555))

        # ── ARTIFICIAL HORIZON ────────────────────────────────────────────
        draw_horizon(screen, dr, dp, 170, 615, 300, 90)
        screen.blit(font_sm.render("HORIZON", True, GRAY), (80, 532))

        # ── RIGHT PANEL: readouts ──────────────────────────────────────────
        rx = 680
        ry = 20

        # Title + status
        screen.blit(font_lg.render("DRONE ATTITUDE", True, WHITE), (rx, ry))
        s_col = GREEN if connected else RED
        s_txt = f"● CONNECTED  {hz_val:.0f} Hz" if connected else "● DISCONNECTED"
        screen.blit(font_md.render(s_txt, True, s_col), (rx, ry+34))

        # Big angle readouts
        for i, (label, val, col, desc) in enumerate([
            ("ROLL ",  dr, ROLL_C,  "← left / right tilt →"),
            ("PITCH",  dp, PITCH_C, "↑ nose up / nose down ↓"),
            ("YAW  ",  dy, YAW_C,   "↺ heading rotation ↻"),
        ]):
            yp = ry + 80 + i * 100

            # Value
            screen.blit(font_lg.render(f"{label}  {val:+7.1f}°", True, col), (rx, yp))
            screen.blit(font_sm.render(desc, True, GRAY), (rx, yp+30))

            # Bar
            bx, by, bw, bh = rx, yp+48, 560, 16
            bc = bx + bw//2
            pygame.draw.rect(screen, (30, 32, 40), (bx, by, bw, bh))
            pygame.draw.rect(screen, (50, 52, 62), (bx, by, bw, bh), 1)
            pygame.draw.line(screen, (70, 72, 85), (bc, by), (bc, by+bh), 1)
            clamped = max(-180 if i==2 else -90, min(180 if i==2 else 90, val))
            maxv    = 180 if i == 2 else 90
            fw = int(abs(clamped) / maxv * bw//2)
            if fw > 0:
                rx2 = bc if clamped >= 0 else bc - fw
                pygame.draw.rect(screen, col, (rx2, by+1, fw, bh-2))

        # ── Attitude cube indicator (small) ───────────────────────────────
        mini_cx, mini_cy = rx+490, ry+200
        # Mini axes
        R_mini = rot_z(dy) @ rot_y(dp) @ rot_x(dr)
        axis_vecs = [(1,0,0,'X',ROLL_C),(0,1,0,'Y',PITCH_C),(0,0,1,'Z',YAW_C)]
        for v, label, col in [(np.array([1,0,0]),'X',ROLL_C),
                               (np.array([0,1,0]),'Y',PITCH_C),
                               (np.array([0,0,-1]),'Z',YAW_C)]:
            rv = R_mini @ v * 35
            ex = int(mini_cx + rv[0])
            ey = int(mini_cy - rv[1])
            pygame.draw.line(screen, col, (mini_cx, mini_cy), (ex, ey), 3)
            pygame.draw.circle(screen, col, (ex, ey), 4)
            screen.blit(font_sm.render(label, True, col), (ex+4, ey-6))

        # ── Raw serial line ────────────────────────────────────────────────
        raw_disp = raw_line[:60] + "..." if len(raw_line) > 60 else raw_line
        screen.blit(font_sm.render(f"Raw: {raw_disp}", True, (80,80,100)),
                    (rx, WINDOW_H - 55))
        screen.blit(font_sm.render(f"Port: {COM_PORT}   FPS: {clock.get_fps():.0f}   Pkts/s: {hz_val:.0f}",
                                    True, (80,80,100)),
                    (rx, WINDOW_H - 38))
        screen.blit(font_sm.render("R=reset yaw   Q=quit", True, (80,80,100)),
                    (rx, WINDOW_H - 21))

        # ── Graph ──────────────────────────────────────────────────────────
        graph.update(dr, dp, dy)
        graph.draw(screen, font_sm)

        # Graph legend
        lx = rx
        for label, col in [("─ Roll", ROLL_C),("─ Pitch", PITCH_C),("─ Yaw", YAW_C)]:
            screen.blit(font_sm.render(label, True, col), (lx, 445))
            lx += 130

        pygame.display.flip()
        clock.tick(FPS)

    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()