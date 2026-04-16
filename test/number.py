"""
Step 1 Simulation: Scientific Notation & Engineering Prefixes
=============================================================
Run this with:  python electronics_prefixes_sim.py
 
What you will see:
  1. All engineering prefixes printed clearly
  2. Real component values as engineers write them vs raw numbers
  3. An RC circuit time constant — where prefixes matter CRITICALLY
  4. A unit-error disaster demo (what happens if you mix µF and F)
  5. Your practice answers auto-checked
"""
 
# ── 1. Engineering prefix table ───────────────────────────────────────────────
prefixes = [
    ("Giga",  "G",  1e9),
    ("Mega",  "M",  1e6),
    ("Kilo",  "k",  1e3),
    ("(none)","—",  1e0),
    ("Milli", "m",  1e-3),
    ("Micro", "µ",  1e-6),
    ("Nano",  "n",  1e-9),
    ("Pico",  "p",  1e-12),
]
 
def eng(value, unit=""):
    """Format a number using engineering prefixes."""
    for name, sym, mult in prefixes:
        if abs(value) >= mult * 0.999:
            num = value / mult
            sym_str = sym if sym != "—" else ""
            return f"{num:.4g} {sym_str}{unit}"
    return f"{value:.4g} {unit}"
 
print("=" * 60)
print("  ENGINEERING PREFIX TABLE")
print("=" * 60)
for name, sym, mult in prefixes:
    print(f"  {name:<8} {sym}   = {mult:.2e}   → 1 {sym}X = {mult:,.0f} X" if mult >= 1
          else f"  {name:<8} {sym}   = {mult:.2e}   → 1 {sym}X = {mult} X")
 
# ── 2. Real component values ──────────────────────────────────────────────────
print("\n" + "=" * 60)
print("  REAL COMPONENT VALUES  (what the label says  →  raw SI)")
print("=" * 60)
 
components = [
    ("Pull-up resistor",    4.7e3,  "Ω",  "4.7 kΩ"),
    ("Filter capacitor",    100e-6, "F",  "100 µF"),
    ("Arduino clock",       16e6,   "Hz", "16 MHz"),
    ("LED current limit",   20e-3,  "A",  "20 mA"),
    ("Decoupling cap",      10e-9,  "F",  "10 nF"),
    ("Crystal load cap",    22e-12, "F",  "22 pF"),
    ("High-Z input",        1e6,    "Ω",  "1 MΩ"),
    ("Raspberry Pi current",500e-3, "A",  "500 mA"),
]
 
for label, raw, unit, human in components:
    print(f"  {label:<22} {human:<12} = {raw:.6g} {unit}")
 
# ── 3. RC circuit time constant ───────────────────────────────────────────────
print("\n" + "=" * 60)
print("  RC CIRCUIT: Time Constant τ = R × C")
print("  (This is a real filter you will build in Phase 01)")
print("=" * 60)
 
import math
 
R = 10e3    # 10 kΩ
C = 100e-9  # 100 nF
 
tau = R * C   # time constant in seconds
 
print(f"\n  R = {eng(R, 'Ω')}  ({R:.0f} Ω raw)")
print(f"  C = {eng(C, 'F')}  ({C:.2e} F raw)")
print(f"\n  τ = R × C = {R:.0f} × {C:.2e} = {tau:.6f} seconds")
print(f"  τ = {eng(tau, 's')}  ← this is 1 millisecond")
print(f"\n  Cutoff frequency f = 1 / (2π × τ)")
f_c = 1 / (2 * math.pi * tau)
print(f"  f = 1 / (2π × {tau:.6f}) = {f_c:.2f} Hz ≈ {eng(f_c, 'Hz')}")
print(f"\n  This circuit passes signals BELOW ~{f_c:.0f} Hz and blocks above.")
print(f"  Used in audio (remove hiss), sensors (remove noise), power (smooth voltage).")
 
# Charge curve
print(f"\n  Capacitor voltage while charging (supply = 5V):")
print(f"  {'Time':<12}  {'Voltage':<10}  {'% charged'}")
print(f"  {'-'*40}")
V_supply = 5.0
for t_tau in [0, 0.5, 1, 2, 3, 5]:
    t_sec = t_tau * tau
    V = V_supply * (1 - math.exp(-t_sec / tau))
    print(f"  t={eng(t_sec,'s'):<10}  {V:.3f} V      {V/V_supply*100:.1f}%")
print(f"\n  After 5τ = {eng(5*tau,'s')}, cap is 99.3% charged. Engineers call this 'fully charged'.")
 
# ── 4. Unit error disaster demo ───────────────────────────────────────────────
print("\n" + "=" * 60)
print("  UNIT ERROR DISASTER DEMO")
print("  What happens when you confuse µF and F")
print("=" * 60)
 
R_osc = 10e3   # 10 kΩ
 
# Correct: C = 100 µF
C_correct = 100e-6
tau_correct = R_osc * C_correct
f_correct   = 1 / (2 * math.pi * tau_correct)
 
# Wrong: treats 100 µF as 100 F (forgot the µ)
C_wrong = 100.0
tau_wrong = R_osc * C_wrong
f_wrong   = 1 / (2 * math.pi * tau_wrong)
 
print(f"\n  CORRECT  C = 100 µF = {C_correct:.2e} F")
print(f"           τ = {eng(tau_correct,'s')},  f_cutoff = {eng(f_correct,'Hz')}")
print(f"           → Audio low-pass filter. Works perfectly.\n")
print(f"  WRONG    C = 100 F  (forgot the µ — a factor of 1,000,000 off)")
print(f"           τ = {eng(tau_wrong,'s')},  f_cutoff = {f_wrong:.8f} Hz")
print(f"           → Would take {tau_wrong/60:.0f} minutes to charge. Useless as a filter.")
print(f"\n  Error ratio: {C_wrong/C_correct:.0f}× — one missing prefix, circuit is BROKEN.")
 
# ── 5. Practice answer checker ────────────────────────────────────────────────
print("\n" + "=" * 60)
print("  PRACTICE ANSWERS (from the guide)")
print("=" * 60)
 
exercises = [
    ("0.1 µF → nF",       0.1e-6 / 1e-9,          "nF",  100.0),
    ("0.1 µF → pF",       0.1e-6 / 1e-12,          "pF",  100000.0),
    ("3.3 kΩ → Ω",        3.3e3,                    "Ω",   3300.0),
    ("450 mA → A",        450e-3,                   "A",   0.45),
    ("450 mA → µA",       450e-3 / 1e-6,            "µA",  450000.0),
    ("22.2 MHz → MHz",    22.2e6 / 1e6,             "MHz", 22.2),
    ("22.2 MHz → GHz",    22.2e6 / 1e9,             "GHz", 0.0222),
    ("0.25 mm → µm",      0.25e-3 / 1e-6,           "µm",  250.0),
]
 
print()
for question, calculated, unit, expected in exercises:
    match = abs(calculated - expected) < 0.001 * expected + 1e-20
    status = "✓" if match else "✗"
    print(f"  {status}  {question:<20} = {calculated:.6g} {unit}")
 
print(f"\n  P = 22.2V × 1.8A = {22.2 * 1.8:.2f} W = {22.2 * 1.8 * 1000:.0f} mW")
 
print("\n" + "=" * 60)
print("  WHERE YOU USE THIS IN YOUR ELECTRONICS WORK:")
print("=" * 60)
print("""
  Component datasheets:
    Resistors:   always kΩ or MΩ
    Capacitors:  µF (electrolytic) / nF or pF (ceramic)
    Inductors:   µH or mH
    Clocks:      MHz or GHz
 
  Calculations you will do every week:
    Ohm's law:      V = I × R   (watch mA vs A, kΩ vs Ω)
    Power:          P = V × I   (answer in mW or W)
    RC time const:  τ = R × C   (answer in µs or ms)
    Resonance:      f = 1/(2π√LC)
 
  A mistake at the prefix level makes your answer
  wrong by a factor of 1,000 or 1,000,000.
  A resistor that should be 4.7 kΩ but you calculated
  4.7 Ω will blow up your circuit or just not work.
""")
# print("Run complete. Every number above came from one formula.")
# print("The prefix is the most important part of the number.")
 

