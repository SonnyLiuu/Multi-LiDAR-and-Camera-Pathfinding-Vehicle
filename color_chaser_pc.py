"""
ESP32-CAM hsv tracker + udp command sender
manual driving uses pynput for true hold-to-drive
"""

import cv2
import numpy as np
import datetime as dt
import threading
import time
import socket
from pynput import keyboard as pyn_kb

# stream and tracking settings
STREAM_URL   = "http://192.168.1.78/stream"
WRAP_RED     = True
CENTER_TOL   = 30
MIN_PIXELS   = 80
GOAL_PIXELS  = 2000
TARGET_FPS   = 30.0
EMA_ALPHA    = 0.1

# udp destination (esp32 bridge)
CTRL_IP      = "192.168.1.79"
CTRL_PORT    = 5005
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# default hsv values
DEFAULTS = {
    "H_min": 138, "H_max":   7,
    "S_min":  54, "S_max": 180,
    "V_min":  50, "V_max": 255,
}

# shared camera state
latest_frame = None
latest_ts    = 0.0
lock         = threading.Lock()
stopped      = False

# manual control state
manual_mode = False
pressed = set()
pressed_lock = threading.Lock()

# ui actions triggered by keyboard
ui = {
    "toggle_mode": False,
    "print_hsv":   False,
    "snapshot":    False,
    "quit":        False,
    "stop_now":    False,
}
ui_lock = threading.Lock()

# send udp command only when it changes
last_cmd = None
def send_cmd(cmd: str):
    global last_cmd
    if cmd != last_cmd:
        sock.sendto(cmd.encode(), (CTRL_IP, CTRL_PORT))
        last_cmd = cmd

# grab mjpeg frames in a background thread
def frame_grabber():
    global latest_frame, latest_ts, stopped
    cap = cv2.VideoCapture(STREAM_URL, cv2.CAP_FFMPEG)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    if not cap.isOpened():
        print("stream open failed:", STREAM_URL)
        stopped = True
        return

    while not stopped:
        ret, frm = cap.read()
        if ret:
            with lock:
                latest_frame = frm
                latest_ts    = time.time()
        else:
            time.sleep(0.01)

    cap.release()

threading.Thread(target=frame_grabber, daemon=True).start()

# pynput helpers
def _set_ui(flag):
    with ui_lock:
        ui[flag] = True

def _to_char(k):
    try:
        return k.char.lower()
    except Exception:
        return None

# keyboard press handler
def on_press(k):
    ch = _to_char(k)

    if k == pyn_kb.Key.esc or ch == 'q':
        _set_ui("quit")
        return

    if ch == 'm':
        _set_ui("toggle_mode")
        return

    if ch == 'h':
        _set_ui("print_hsv")
        return

    if ch == 'c':
        _set_ui("snapshot")
        return

    if k == pyn_kb.Key.space:
        _set_ui("stop_now")
        with pressed_lock:
            pressed.clear()
            pressed.add('space')
        return

    if ch in ('w','a','s','d','e','z','x','c','1','3'):
        with pressed_lock:
            pressed.add(ch)

# keyboard release handler
def on_release(k):
    ch = _to_char(k)
    if k == pyn_kb.Key.space:
        with pressed_lock:
            pressed.discard('space')
    elif ch:
        with pressed_lock:
            pressed.discard(ch)

listener = pyn_kb.Listener(on_press=on_press, on_release=on_release)
listener.daemon = True
listener.start()

# resolve pressed keys into a single drive command
def resolve_manual_cmd():
    with pressed_lock:
        keys = set(pressed)

    if 'space' in keys:
        return 'S'
    if '1' in keys:
        return '1'
    if '3' in keys:
        return '3'
    if 'w' in keys and 'a' in keys:
        return 'Q'
    if 'w' in keys and 'd' in keys:
        return 'E'
    if 's' in keys and 'a' in keys:
        return 'Z'
    if 's' in keys and 'd' in keys:
        return 'C'
    if 'w' in keys:
        return 'W'
    if 's' in keys:
        return 'X'
    if 'a' in keys:
        return 'A'
    if 'd' in keys:
        return 'D'
    return 'S'

# opencv ui
cv2.namedWindow("frame", cv2.WINDOW_NORMAL)
cv2.namedWindow("mask", cv2.WINDOW_NORMAL)
cv2.namedWindow("controls", cv2.WINDOW_NORMAL)
cv2.namedWindow("help", cv2.WINDOW_NORMAL)

def make_help_image():
    h, w = 320, 520
    img = np.zeros((h, w, 3), dtype=np.uint8)

    lines = [
        "driving (manual mode, hold keys):",
        "",
        "w = forward",
        "a = left",
        "s = back",
        "d = right",
        "",
        "w+a = q (forward-left)",
        "w+d = e (forward-right)",
        "s+a = z (back-left)",
        "s+d = c (back-right)",
        "",
        "1 = rotate left",
        "3 = rotate right",
        "space = stop",
        "",
        "m = toggle manual/auto",
        "h = print hsv   c = snapshot",
        "q / esc = quit",
    ]

    y = 28
    for i, t in enumerate(lines):
        scale = 0.6 if i == 0 else 0.55
        thick = 2 if i == 0 else 1
        cv2.putText(img, t, (12, y), cv2.FONT_HERSHEY_SIMPLEX, scale, (255, 255, 255), thick)
        y += 20

    return img

help_img = make_help_image()

def _noop(x): pass

sliders = [
    ("H_min", DEFAULTS["H_min"], 179),
    ("H_max", DEFAULTS["H_max"], 179),
    ("S_min", DEFAULTS["S_min"], 255),
    ("S_max", DEFAULTS["S_max"], 255),
    ("V_min", DEFAULTS["V_min"], 255),
    ("V_max", DEFAULTS["V_max"], 255),
]
for name, ini, maxi in sliders:
    cv2.createTrackbar(name, "controls", ini, maxi, _noop)

print("m toggle manual/auto")
print("manual: hold wasd, diagonals via combos, space stops")
print("h print hsv, c snapshot, q or esc quit")

# main loop
prev_ts   = None
ema_fps   = TARGET_FPS
last_vals = {n: None for n,_,_ in sliders}
lower1 = upper1 = lower2 = upper2 = None
ds = 2

hmin,hmax = DEFAULTS["H_min"],DEFAULTS["H_max"]
smin,smax = DEFAULTS["S_min"],DEFAULTS["S_max"]
vmin,vmax = DEFAULTS["V_min"],DEFAULTS["V_max"]

while True:
    with ui_lock:
        toggle = ui["toggle_mode"]; ui["toggle_mode"] = False
        phsv   = ui["print_hsv"];   ui["print_hsv"]   = False
        snap   = ui["snapshot"];    ui["snapshot"]    = False
        quit_r = ui["quit"];        ui["quit"]        = False
        stop_n = ui["stop_now"];    ui["stop_now"]    = False

    if quit_r:
        send_cmd('S')
        break

    if toggle:
        manual_mode = not manual_mode
        with pressed_lock:
            pressed.clear()
        send_cmd('S')
        print("mode:", "manual" if manual_mode else "auto")

    if stop_n:
        send_cmd('S')

    with lock:
        frame = None if latest_frame is None else latest_frame.copy()
        ts    = latest_ts

    if frame is None:
        time.sleep(0.01)
        continue

    if prev_ts is not None:
        dtf = ts - prev_ts
        if dtf > 0:
            ema_fps = ema_fps*(1-EMA_ALPHA) + (1.0/dtf)*EMA_ALPHA
    prev_ts = ts

    h,w = frame.shape[:2]

    vals = {n: cv2.getTrackbarPos(n, "controls") for n,_,_ in sliders}
    if any(vals[n] != last_vals[n] for n in vals):
        hmin,hmax = vals["H_min"],vals["H_max"]
        smin,smax = vals["S_min"],vals["S_max"]
        vmin,vmax = vals["V_min"],vals["V_max"]
        lower1 = np.array([hmin,smin,vmin],np.uint8)
        upper1 = np.array([179,smax,vmax],np.uint8)
        lower2 = np.array([0,smin,vmin],np.uint8)
        upper2 = np.array([hmax,smax,vmax],np.uint8)
    last_vals.update(vals)

    if phsv:
        print(f"[{dt.datetime.now().isoformat(timespec='seconds')}] "
              f"H({hmin},{hmax}) S({smin},{smax}) V({vmin},{vmax})")

    small = frame[::ds,::ds]
    hsv = cv2.cvtColor(small, cv2.COLOR_BGR2HSV)

    m1 = cv2.inRange(hsv, lower1, upper1)
    if WRAP_RED and hmin > hmax:
        m2 = cv2.inRange(hsv, lower2, upper2)
        mask = cv2.bitwise_or(m1,m2)
    else:
        mask = m1

    n, labels, stats, _ = cv2.connectedComponentsWithStats(mask, 8)
    if n > 1:
        idx = 1 + np.argmax(stats[1:,cv2.CC_STAT_AREA])
        mask = np.where(labels==idx,255,0).astype(np.uint8)

    M = cv2.moments(mask)
    pixels = int(M["m00"]/255) if M["m00"] else 0
    auto_cmd = 'S'

    if pixels >= MIN_PIXELS:
        cx = int(M["m10"]/M["m00"])*ds
        cy = int(M["m01"]/M["m00"])*ds
        if pixels > GOAL_PIXELS: auto_cmd = 'S'
        elif cx < w//2-CENTER_TOL: auto_cmd = 'A'
        elif cx > w//2+CENTER_TOL: auto_cmd = 'D'
        else: auto_cmd = 'W'
        cv2.circle(frame,(cx,cy),6,(255,255,255),-1)

    cmd = resolve_manual_cmd() if manual_mode else auto_cmd
    send_cmd(cmd)

    cv2.line(frame,(w//2,0),(w//2,h),(200,200,200),1)
    mask_full = np.repeat(np.repeat(mask>0,ds,0),ds,1)
    masked = frame.copy()
    masked[~mask_full] = 0

    cv2.putText(frame,
        f"{'manual' if manual_mode else 'auto'} cmd:{cmd} pix:{pixels} fps:{ema_fps:.1f}",
        (10,30),cv2.FONT_HERSHEY_SIMPLEX,0.7,(0,0,255),2)

    cv2.imshow("frame",frame)
    cv2.imshow("mask",masked)
    cv2.imshow("help", help_img)

    if snap:
        tsn = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        cv2.imwrite(f"frame_{tsn}.png",frame)
        cv2.imwrite(f"mask_{tsn}.png",masked)
        print("saved snapshot")

    cv2.waitKey(1)

stopped = True
send_cmd('S')
sock.close()
cv2.destroyAllWindows()
