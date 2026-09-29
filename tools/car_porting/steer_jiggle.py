#!/usr/bin/env python3
"""Render a video of what happens to the steering angle between the model and the rack.

Three wheels per segment: the angle controls asked for (carControl), the angle carcontroller.py put
on the bus (carOutput) and the angle the EPS actually reached (carState), over the road camera and a
rolling plot. Jiggle RMS is each signal minus its own 0.5 s moving average, engaged samples only.

  # Konik / comma API (uses ~/.comma/auth.json)
  API_HOST=https://api.konik.ai tools/car_porting/steer_jiggle.py "391b5ce35cb77d89/0000049e--e72cfa487c/57"

  # local segment dir, e.g. rsync'd from /data/media/0/realdata; stack several to compare tunes
  tools/car_porting/steer_jiggle.py seg_a/ seg_b/ --label before --label after --start 10 --duration 30
"""
import argparse
import math
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle
from matplotlib.transforms import Affine2D

BG, PANEL, FG, DIM = "#12151a", "#1b1f26", "#e6e8eb", "#8a929c"
SIGNALS = (  # key, title, subtitle, color
  ("model", "Model", "carControl", "#4aa3ff"),
  ("output", "carcontroller", "carOutput", "#ffa53a"),
  ("actual", "EPS actual", "carState", "#5fd38d"),
)
LOG_NAMES = ("rlog.zst", "rlog.bz2", "rlog", "qlog.zst", "qlog.bz2", "qlog")
VIDEO_NAMES = ("qcamera.ts", "fcamera.hevc")


@dataclass
class Seg:
  label: str
  rate: float
  t: np.ndarray       # s, uniform grid from log start
  sig: dict           # key -> deg on t
  v: np.ndarray       # m/s
  lat: np.ndarray     # bool, openpilot commanding lateral
  pressed: np.ndarray  # bool, driver on the wheel
  videos: list        # (file or None, s per frame on the same clock as t), one per segment


def jiggle_rms(x, mask, rate, win_s=0.5):
  w = max(3, int(round(win_s * rate)) | 1)
  resid = x - np.convolve(x, np.ones(w) / w, mode="same")
  # only windows fully inside engagement, so a handover step does not count as jiggle
  valid = np.convolve(mask.astype(float), np.ones(w), mode="same") > w - 0.5
  return float(np.sqrt(np.mean(resid[valid] ** 2))) if valid.any() else math.nan


def lag_s(ref, y, mask, rate, max_s=1.0):
  """Delay of y behind ref that best lines the two up; resolution is one sample."""
  n, errs = len(ref), []
  for k in range(int(max_s * rate) + 1):
    m = mask[k:] & mask[:n - k]
    errs.append(np.mean((y[k:][m] - ref[:n - k][m]) ** 2) if m.sum() > rate else np.inf)
  best = int(np.argmin(errs))
  # a best fit on the search edge means the signals never lined up, not a real delay
  return best / rate if np.isfinite(errs[best]) and best < len(errs) - 1 else math.nan


def ms(x):
  return "—" if math.isnan(x) else f"{x * 1e3:.0f} ms"


def deg(x):
  return "—" if math.isnan(x) else f"{x:.2f}°"


def held(pressed, rate, hold_s=0.5):
  """steeringPressed, stretched by the time carcontroller keeps yielding after the driver lets go."""
  # ponytail: Chery's STEER_RETURN_TIME; other ports hand back at their own pace
  n = int(hold_s * rate)
  return np.convolve(pressed.astype(float), np.ones(n + 1))[:len(pressed)] > 0


def resolve(src):
  """(segment number or None, log, video) per segment; a route name without a segment number means all of them."""
  if os.path.isdir(src):
    log = next((os.path.join(src, f) for f in LOG_NAMES if os.path.exists(os.path.join(src, f))), None)
    video = next((os.path.join(src, f) for f in VIDEO_NAMES if os.path.exists(os.path.join(src, f))), None)
    if log is None:
      raise SystemExit(f"no rlog/qlog in {src}")
    return [(None, log, video)], os.path.basename(os.path.normpath(src))

  from concurrent.futures import ThreadPoolExecutor
  from openpilot.tools.lib.filereader import FileReader
  from openpilot.tools.lib.route import Route, RouteName, SegmentName
  # SegmentName reads "dongle/route" as route "dongle" segment "route", so a bare route is spotted here
  bits = src.replace("|", "/").split("/")
  whole = len(bits) == 2 and bits[1].count("--") == 1
  sn = RouteName(src) if whole else SegmentName(src)
  segs = [x for x in Route(f"{sn.dongle_id}|{sn.time_str}").segments
          if (whole or x.name.segment_num == sn.segment_num) and (x.log_path or x.qlog_path)]
  if not segs:
    raise SystemExit(f"{src}: no logs uploaded for this {'route' if whole else 'segment'}")

  def fetch(seg):
    video = seg.qcamera_path or seg.camera_path
    if not video:
      return None
    # openpilot's bundled ffmpeg only reads files and pipes, not https
    with FileReader(video) as f, tempfile.NamedTemporaryFile(suffix=".ts" if seg.qcamera_path else ".hevc", delete=False) as tmp:
      tmp.write(f.read())
    return tmp.name

  with ThreadPoolExecutor(8) as pool:
    videos = list(pool.map(fetch, segs))
  parts = [(x.name.segment_num, x.log_path or x.qlog_path, v) for x, v in zip(segs, videos, strict=True)]
  return parts, sn.time_str if whole else f"{sn.time_str}/{sn.segment_num}"


def load(src, label):
  from openpilot.tools.lib.logreader import LogReader
  parts, name = resolve(src)
  kinds = sorted({os.path.basename(log.split("?")[0]) for _, log, _ in parts})
  print(f"{name}: reading {len(parts)} segment(s), {', '.join(kinds)}")

  cc, co, cs = [], [], []
  enc: dict[str, list] = {"qNarrowRoadEncodeIdx": [], "narrowRoadEncodeIdx": []}
  for m in LogReader([log for _, log, _ in parts]):
    w = m.which()
    if w == "carControl":
      cc.append((m.logMonoTime, m.carControl.actuators.steeringAngleDeg, m.carControl.latActive))
    elif w == "carOutput":
      co.append((m.logMonoTime, m.carOutput.actuatorsOutput.steeringAngleDeg))
    elif w == "carState":
      cs.append((m.logMonoTime, m.carState.steeringAngleDeg, m.carState.vEgo, m.carState.steeringPressed))
    elif w in enc:
      e = getattr(m, w)
      enc[w].append((e.segmentNum, e.segmentId, e.timestampEof))
  if not (cc and co and cs):
    raise SystemExit(f"{name}: log has no carControl/carOutput/carState")

  # several logs come back in whatever order they finished loading
  cc, co, cs = (np.array(sorted(x), dtype=np.float64) for x in (cc, co, cs))
  t0 = min(cc[0, 0], co[0, 0], cs[0, 0])
  rate = round(1e9 / np.median(np.diff(cc[:, 0])))
  if rate < 50:
    print(f"{name}: {rate} Hz qlog, jiggle reads low; upload the rlog for the real number")
  t = np.arange(max(cc[0, 0], co[0, 0], cs[0, 0]), min(cc[-1, 0], co[-1, 0], cs[-1, 0]), 1e9 / rate)

  def on_grid(a, col):
    return np.interp(t, a[:, 0], a[:, col])

  videos = []
  for num, _, video in parts:
    # qcamera frames carry qNarrowRoadEncodeIdx, fcamera narrowRoadEncodeIdx; same frame clock
    idx = enc["qNarrowRoadEncodeIdx"] if video and video.endswith(".ts") else enc["narrowRoadEncodeIdx"]
    idx = [(i, ts) for n, i, ts in (idx or enc["narrowRoadEncodeIdx"] or enc["qNarrowRoadEncodeIdx"]) if num is None or n == num]
    # ponytail: no encode index means guess 20 fps from where the segment should start, good to a frame or two
    guess = 60. * (num or 0) + np.arange(1200) / 20.
    frame_t = (np.array([ts for _, ts in sorted(idx)], dtype=np.float64) - t0) / 1e9 if idx else guess
    videos.append((video, frame_t))

  return Seg(
    label=label or name, rate=rate, t=(t - t0) / 1e9,
    sig={"model": on_grid(cc, 1), "output": on_grid(co, 1), "actual": on_grid(cs, 1)},
    v=on_grid(cs, 2), lat=on_grid(cc, 2) > 0.5, pressed=held(on_grid(cs, 3) > 0.5, rate),
    videos=videos,
  )


def stats(s):
  # with the driver on the wheel carcontroller relays the stock command and the rack follows the driver,
  # so none of output, actual or their lag says anything about the filter there
  m = s.lat & ~s.pressed
  st = {k: jiggle_rms(s.sig[k], m, s.rate) for k, *_ in SIGNALS}
  st["lag_filter"] = lag_s(s.sig["model"], s.sig["output"], m, s.rate)
  st["lag_eps"] = lag_s(s.sig["output"], s.sig["actual"], m, s.rate)
  st["track"] = float(np.sqrt(np.mean((s.sig["actual"] - s.sig["output"])[m] ** 2))) if m.any() else math.nan
  return st


class VideoIn:
  """Frames by time across one file per segment, decoding forward and reopening ffmpeg only to seek."""
  FPS = 20  # qcamera and fcamera both

  def __init__(self, parts, w, h):
    self.parts, self.w, self.h, self.frame = parts, w, h, np.zeros((h, w, 3), np.uint8)
    self.starts = np.array([ft[0] if len(ft) else np.inf for _, ft in parts])
    self.fn, self.part, self.proc, self.idx = None, -1, None, -1

  def at(self, tau):
    k = int(np.clip(np.searchsorted(self.starts, tau, side="right") - 1, 0, len(self.parts) - 1))
    fn, frame_t = self.parts[k]
    i = int(np.searchsorted(frame_t, tau))
    if k != self.part:
      self.fn, self.part = fn, k
      self._open(i)
    return self.get(i)

  def _open(self, i):
    self.close()
    self.proc, self.idx = None, i - 1
    if not self.fn:
      self.frame = np.zeros((self.h, self.w, 3), np.uint8)
    else:
      self.proc = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-ss", f"{i / self.FPS:.3f}", "-i", self.fn,
                                    "-vf", f"scale={self.w}:{self.h}", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"],
                                   stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

  def get(self, i):
    if i == self.idx:
      return self.frame
    if i < self.idx or i > self.idx + self.FPS * 2:
      # scrubbing: restarting ffmpeg at the frame beats decoding everything in between
      self._open(i)
    while self.proc and self.idx < i:
      raw = self.proc.stdout.read(self.w * self.h * 3)
      if len(raw) < self.w * self.h * 3:
        self.proc = None
        break
      self.frame, self.idx = np.frombuffer(raw, np.uint8).reshape(self.h, self.w, 3), self.idx + 1
    return self.frame

  def close(self):
    if self.proc:
      self.proc.kill()


def wheel(ax, color):
  ax.set_xlim(-1.35, 1.35)
  ax.set_ylim(-1.35, 1.35)
  ax.set_aspect("equal")
  ax.axis("off")
  parts = [
    Circle((0, 0), 1.0, fill=False, lw=9, ec=color),
    Circle((0, 0), 0.22, fc="#3a3f47", ec="none"),
    *[ax.plot(*zip((0, 0), p, strict=True), lw=5, c="#c9ccd1", solid_capstyle="round")[0] for p in ((-0.95, 0.05), (0.95, 0.05), (0, -0.95))],
    ax.plot([0, 0], [0.9, 1.1], lw=9, c="#ff5a52", solid_capstyle="butt")[0],
  ]
  for p in parts[:2]:
    ax.add_patch(p)
  return parts, ax.text(0, -1.3, "", ha="center", va="top", color=FG, fontsize=15, family="monospace", weight="bold")


def build(segs, window, row_h=6.4, bottom_in=0.25):
  """Lay out one row per segment; returns the figure and a function that moves every row to time tau."""
  sts = [stats(s) for s in segs]
  vw, vh = 640, 400
  height = row_h * len(segs)
  fig = plt.figure(figsize=(16, height), dpi=100, facecolor=BG)
  # margins in inches, so the titles keep their room however short the rows are
  rows = fig.add_gridspec(len(segs), 1, hspace=0.18, left=0.02, right=0.98, top=1 - 0.5 / height, bottom=bottom_in / height)
  views = []
  for r, (s, st) in enumerate(zip(segs, sts, strict=True)):
    g = rows[r].subgridspec(2, 4, height_ratios=[2.4, 1], width_ratios=[2.1, 1, 1, 1], hspace=0.28, wspace=0.05)
    cam = fig.add_subplot(g[0, 0])
    cam.axis("off")
    img = cam.imshow(np.zeros((vh, vw, 3), np.uint8))
    hud = cam.text(0.02, 0.96, "", transform=cam.transAxes, va="top", color=FG, fontsize=13, family="monospace",
                   bbox={"fc": "#000000b0", "ec": "none", "pad": 5})
    badge = cam.text(0.98, 0.96, "", transform=cam.transAxes, va="top", ha="right", fontsize=12, weight="bold",
                     bbox={"fc": "#444", "ec": "none", "boxstyle": "round,pad=0.4"})
    cam.set_title(s.label, loc="left", color=FG, fontsize=17, weight="bold")

    wheels = []
    for c, (key, title, sub, color) in enumerate(SIGNALS):
      ax = fig.add_subplot(g[0, c + 1])
      ax.set_title(f"{title}\n({sub})", color=color, fontsize=11)
      wheels.append((key, ax, *wheel(ax, color)))

    ax = fig.add_subplot(g[1, :])
    ax.set_facecolor(PANEL)
    for side in ax.spines.values():
      side.set_color("#333a44")
    ax.tick_params(colors=DIM, labelsize=9)
    ax.set_ylabel("deg", color=DIM)
    for key, title, _, color in SIGNALS:
      ax.plot(s.t, s.sig[key], color=color, lw=1.6 if key != "actual" else 1.2,
              label=f"{title}  jiggle {deg(st[key])}")
    lo, hi = min(s.sig[k].min() for k, *_ in SIGNALS), max(s.sig[k].max() for k, *_ in SIGNALS)
    ax.fill_between(s.t, lo - 1e3, hi + 1e3, where=~s.lat, color=DIM, alpha=0.18, lw=0, label="not engaged")
    ax.fill_between(s.t, lo - 1e3, hi + 1e3, where=s.lat & s.pressed, color="#e0a030", alpha=0.15, lw=0,
                    label="driver override")
    ax.legend(loc="lower left", bbox_to_anchor=(0, 1.0), ncol=5, fontsize=9, frameon=False, labelcolor=FG,
              borderaxespad=0.2)
    ax.text(1.0, 1.02, f"filter lag {ms(st['lag_filter'])}   EPS lag {ms(st['lag_eps'])}   tracking RMS {deg(st['track'])}",
            transform=ax.transAxes, ha="right", va="bottom", color=DIM, fontsize=9)
    now = ax.axvline(0, color="#ffffff55", lw=1)
    views.append((s, VideoIn(s.videos, vw, vh), img, hud, badge, wheels, ax, now))

  def draw_at(tau):
    for s, vid, img, hud, badge, wheels, ax, now in views:
      i = int(np.clip(np.searchsorted(s.t, tau), 0, len(s.t) - 1))
      img.set_data(vid.at(tau))
      hud.set_text(f"{s.v[i] * 3.6:5.1f} km/h  t={tau:5.1f}s")
      state = ("DRIVER", "#e0a030") if s.lat[i] and s.pressed[i] else ("ENGAGED", "#2e9e5b") if s.lat[i] else ("OFF", "#555b63")
      badge.set_text(state[0])
      badge.get_bbox_patch().set_facecolor(state[1])
      badge.set_color("#ffffff")
      for key, wax, parts, label in wheels:
        a = s.sig[key][i]
        # openpilot angles are positive to the left, which is counter-clockwise on screen
        tr = Affine2D().rotate_deg(a) + wax.transData
        for p in parts:
          p.set_transform(tr)
        label.set_text(f"{a:+.1f}°")
      sl = slice(np.searchsorted(s.t, tau - window), i + 1)
      lo = min(s.sig[k][sl].min() for k, *_ in SIGNALS)
      hi = max(s.sig[k][sl].max() for k, *_ in SIGNALS)
      mid, half = (lo + hi) / 2, max(5., (hi - lo) * 0.6)
      ax.set_xlim(tau - window, tau + 0.2)
      ax.set_ylim(mid - half, mid + half)
      now.set_xdata([tau, tau])

  def close():
    for v in views:
      v[1].close()

  return fig, draw_at, close


def check_start(segs, start):
  end = min(s.t[-1] for s in segs)
  if not 0 <= start < end:
    raise SystemExit(f"--start {start:g}s is outside the segment, which runs 0 to {end:.0f}s")
  return end


def render(segs, out, start, duration, window, fps):
  end = check_start(segs, start)
  duration = min(duration or end, end - start)
  fig, draw_at, close = build(segs, window)
  w, h = fig.canvas.get_width_height()
  enc = subprocess.Popen(["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", f"{w}x{h}",
                          "-r", str(fps), "-i", "pipe:0", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", out],
                         stdin=subprocess.PIPE)
  n = int(duration * fps)
  for k in range(n):
    draw_at(start + k / fps)
    fig.canvas.draw()
    enc.stdin.write(fig.canvas.buffer_rgba())
    if k % (fps * 5) == 0:
      print(f"\r{k}/{n} frames", end="", flush=True)
  enc.stdin.close()
  enc.wait()
  close()
  print(f"\rwrote {out} ({n} frames)")


def live(segs, start, window):
  """Interactive window: space play/pause, left/right 1 s, comma/period 1 frame, [ ] playback speed."""
  from matplotlib.widgets import Button, Slider
  plt.switch_backend("macosx" if sys.platform == "darwin" else "TkAgg")
  end = check_start(segs, start)
  fig, draw_at, close = build(segs, window, row_h=5.2, bottom_in=0.6)
  fig.canvas.manager.set_window_title("steer_jiggle")

  sax = fig.add_axes((0.06, 0.012, 0.76, 0.025), facecolor=PANEL)
  slider = Slider(sax, "t", 0., end, valinit=start, valfmt="%5.2f s", color="#4aa3ff", initcolor="none")
  slider.label.set_color(FG)
  slider.valtext.set_color(FG)
  bax = fig.add_axes((0.87, 0.006, 0.05, 0.037))
  button = Button(bax, "Play", color=PANEL, hovercolor="#333a44")
  button.label.set_color(FG)
  speed_text = fig.text(0.935, 0.024, "1x", color=DIM, va="center", fontsize=10)
  state = {"playing": False, "last": 0., "speed": 1.}

  def on_change(tau):
    draw_at(tau)
    fig.canvas.draw_idle()

  def set_playing(on):
    state["playing"], state["last"] = on, time.monotonic()
    button.label.set_text("Pause" if on else "Play")
    fig.canvas.draw_idle()

  def tick():
    if not state["playing"]:
      return
    now = time.monotonic()
    # follow the wall clock, so a slow draw drops frames instead of turning into slow motion
    tau = slider.val + (now - state["last"]) * state["speed"]
    state["last"] = now
    if tau >= end:
      tau = end
      set_playing(False)
    slider.set_val(tau)

  def on_key(e):
    step = {"left": -1., "right": 1., ",": -1 / VideoIn.FPS, ".": 1 / VideoIn.FPS}.get(e.key)
    if e.key == " ":
      set_playing(not state["playing"])
    elif step is not None:
      slider.set_val(float(np.clip(slider.val + step, 0., end)))
    elif e.key in ("[", "]"):
      state["speed"] = float(np.clip(state["speed"] * (0.5 if e.key == "[" else 2.), 0.125, 4.))
      speed_text.set_text(f"{state['speed']:g}x")
      fig.canvas.draw_idle()

  slider.on_changed(on_change)
  button.on_clicked(lambda _: set_playing(not state["playing"]))
  fig.canvas.mpl_connect("key_press_event", on_key)
  timer = fig.canvas.new_timer(interval=40)
  timer.add_callback(tick)
  timer.start()
  on_change(start)
  print("space play/pause   left/right 1 s   , . 1 frame   [ ] speed   drag the slider to scrub")
  plt.show()
  close()


def summary(segs):
  print(f"\n{'segment':<28}{'Hz':>5}{'model':>9}{'output':>9}{'actual':>9}{'filt lag':>10}{'EPS lag':>9}{'track':>8}{'engaged':>9}")
  for s in segs:
    st = stats(s)
    print(f"{s.label[:27]:<28}{s.rate:>5.0f}{deg(st['model']):>9}{deg(st['output']):>9}{deg(st['actual']):>9}" +
          f"{ms(st['lag_filter']):>10}{ms(st['lag_eps']):>9}{deg(st['track']):>8}{s.lat.mean() * 100:>8.0f}%")


def selftest():
  rng = np.random.default_rng(0)
  rate, t = 100, np.arange(0, 60, 0.01)
  smooth = 20 * np.sin(2 * np.pi * t / 8)
  noisy = smooth + rng.normal(0, 1.5, t.size)
  mask = np.ones(t.size, bool)
  mask[3000:3200] = False
  assert abs(jiggle_rms(noisy, mask, rate) - 1.5) < 0.1, jiggle_rms(noisy, mask, rate)
  assert jiggle_rms(smooth, mask, rate) < 0.15  # a 40 deg, 8 s weave leaves ~0.1 deg through the 0.5 s average
  assert list(held(np.array([0, 1, 0, 0, 0, 0], bool), rate=2, hold_s=1.0)) == [0, 1, 1, 1, 0, 0]
  assert lag_s(noisy, np.roll(noisy, 25), mask, rate) == 0.25
  print("selftest ok")


if __name__ == "__main__":
  p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  p.add_argument("segments", nargs="*", help="local segment dir, dongle/route/seg, or dongle/route for all of it")
  p.add_argument("--label", action="append", default=[], help="row title, once per segment")
  p.add_argument("--start", type=float, default=0., help="seconds into the segment")
  p.add_argument("--duration", type=float, default=0., help="seconds, default the whole segment")
  p.add_argument("--window", type=float, default=6., help="plot history in seconds")
  p.add_argument("--fps", type=int, default=20)
  p.add_argument("--out", default="steer_jiggle.mp4")
  p.add_argument("--stats-only", action="store_true", help="print the table, skip the video")
  p.add_argument("--live", action="store_true", help="open an interactive window instead of writing a video")
  p.add_argument("--selftest", action="store_true")
  args = p.parse_args()

  if args.selftest:
    selftest()
  elif not args.segments:
    p.error("give at least one segment")
  else:
    segs = [load(src, args.label[i] if i < len(args.label) else None) for i, src in enumerate(args.segments)]
    summary(segs)
    if args.live:
      live(segs, args.start, args.window)
    elif not args.stats_only:
      render(segs, args.out, args.start, args.duration, args.window, args.fps)
