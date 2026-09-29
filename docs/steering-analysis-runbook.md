# Analisis steering Chery Omoda E5: runbook

Cara membedah kontrol setir dari log: apa yang diminta model, apa yang dikirim
carcontroller, dan apa yang benar-benar dicapai EPS. Dibuat 28 dan 29
September 2026 di branch `feature/chery-jetlink`, data dari Konik
(`391b5ce35cb77d89`).

Tiga alat, dipakai sesuai kebutuhan:

| Alat | Untuk apa |
|---|---|
| `tools/car_porting/steer_jiggle.py` | Angka jiggle, lag, dan tracking; video atau jendela live dengan tiga setir |
| jotpluggler, layout `steer-jiggle` | Menelusuri sinyal mentah dengan timeline dan video |
| op-replay-clipper `ui-alt` | Klip UI openpilot asli dengan panel telemetri, untuk dibagikan |

Semua perintah dijalankan dari root repo, kecuali disebut lain.

## Sinyal yang dibandingkan

| Nama di alat | Sumber | Arti |
|---|---|---|
| Model / TARGET | `carControl.actuators.steeringAngleDeg` | Sudut yang diminta controls, sebelum carcontroller |
| carcontroller / APPLIED | `carOutput.actuatorsOutput.steeringAngleDeg` | Sudut yang dikirim ke CAN, setelah filter dan limit |
| EPS actual / ACTUAL | `carState.steeringAngleDeg` | Sudut setir sebenarnya |

Sudut positif berarti ke kiri. Di layar, setir berputar berlawanan arah jarum
jam untuk nilai positif.

## steer_jiggle.py

```bash
export API_HOST=https://api.konik.ai      # untuk route Konik; direktori lokal tidak perlu
T=tools/car_porting/steer_jiggle.py
P=.venv/bin/python

$P $T 391b5ce35cb77d89/000004dd--f4d940509f --live          # satu route, interaktif
$P $T 391b5ce35cb77d89/000004df--29c0050d5a/3 --stats-only  # satu segment, tabel saja
$P $T <seg> --start 14 --duration 30 --out ~/Downloads/x.mp4     # render video
$P $T <segA> <segB> --label sebelum --label sesudah --live       # dua segment ditumpuk
$P $T --selftest
```

Format input:

- `dongle/route` berarti semua segment digabung jadi satu timeline.
- `dongle/route/N` atau `dongle|route--N` untuk satu segment.
- Direktori lokal yang berisi `rlog.zst` atau `qlog.zst`, plus `qcamera.ts` kalau ada. Bisa diambil dari
  device: `rsync -av comma:/data/media/0/realdata/<route>--<N>/ ~/segN/`.

Tombol di `--live`:

| Tombol | Aksi |
|---|---|
| Space | Play atau pause |
| Kiri, kanan | Mundur atau maju 1 detik |
| `,` `.` | Mundur atau maju 1 frame (0.05 detik) |
| `[` `]` | Kecepatan putar, 0.125x sampai 4x |
| Slider bawah | Lompat ke detik mana saja |

Jendela live menggambar sekitar 7 sampai 10 fps. Waktu putar tetap mengikuti
jam, jadi frame dilewati, bukan melambat. Untuk melihat per frame, pause lalu
pakai `.`. Render mp4 butuh sekitar 3 menit per baris untuk 60 detik.

### Membaca tabel

```
segment      Hz  model  output  actual  filt lag  EPS lag  track  engaged
seg 3       100  0.89°   0.18°   0.22°    250 ms   120 ms  0.52°      18%
```

- **model, output, actual (jiggle):** RMS sinyal dikurangi rata-rata bergerak
  0.5 detiknya sendiri. Kalau filter bekerja, output jauh di bawah model.
- **filt lag:** delay model ke output, harga yang dibayar filter.
- **EPS lag:** delay output ke actual, seberapa cepat EPS merespons.
- **track:** RMS selisih actual dan output, seberapa dekat EPS mengikuti perintah.
- **engaged:** porsi waktu openpilot memegang lateral.
- **—** berarti datanya tidak cukup, bukan error.

Semua angka hanya menghitung saat engaged dan setir tidak dipegang driver.
Saat driver memegang setir, carcontroller me-relay perintah stok, jadi output
mengikuti driver dan tidak mencerminkan filter. `steeringPressed` diperpanjang
0.5 detik (`STEER_RETURN_TIME` Chery), karena carcontroller belum langsung
mengambil alih lagi setelah setir dilepas. Di plot, abu-abu berarti tidak
engaged dan oranye berarti driver override.

### Jebakan

- Konik biasanya hanya punya qlog (10 Hz). Enak untuk ditonton, tapi jiggle
  terbaca rendah dan resolusi lag 100 ms. Untuk angka tuning, minta upload
  rlog di dashboard stable.konik.ai atau ambil langsung dari device.
- Filter lag tidak stabil kalau data bersihnya di bawah sekitar 30 detik.
  Pernah keluar 870, 580, lalu 100 ms untuk segment yang sama.
- Memuat banyak rlog sekaligus dalam satu proses pernah kehabisan memori
  (exit 137). Route qlog aman, sekitar 260 MB. Kalau perlu statistik banyak
  segment rlog, jalankan satu segment per perintah.
- `ffmpeg` di `.venv` hanya bisa file dan pipe. Pakai `pipe:0`/`pipe:1`, jangan
  `-` atau URL https. Video remote diunduh dulu ke `/tmp` dan tidak dihapus.
- Kalau `--live` diam lama tanpa jendela, lihat output terminalnya dulu. Lalu
  cek apakah jendelanya tertutup VS Code (Cmd+Tab, ikon Python di Dock).

## jotpluggler

Plot sinyal mentah, video, dan timeline. Tanpa setir dan tanpa statistik.

```bash
source .venv/bin/activate
API_HOST=https://api.konik.ai openpilot/tools/jotpluggler/jotpluggler \
  --layout steer-jiggle "391b5ce35cb77d89/000004df--29c0050d5a"
```

Layout ada di `openpilot/tools/jotpluggler/layouts/steer-jiggle.json`. Isinya
kamera `qroad` (Konik tidak punya fcamera), torsi driver, tiga sudut setir,
`latActive`/`steeringPressed`, dan `vEgo`.

Kalau muncul `decompressZST error: content is corrupt`, binary-nya basi, bukan
file log yang rusak. Build ulang:

```bash
scons -j$(sysctl -n hw.ncpu) openpilot/tools/jotpluggler/jotpluggler
```

`--output x.png` butuh ImageMagick `convert`. Tanpa itu, file `.ppm` yang
tertinggal bisa dibuka dengan Pillow.

## Klip ui-alt (op-replay-clipper)

Klip UI openpilot asli dengan panel TELEMETRY: TARGET, APPLIED, DELTA, ACTUAL,
HANDS, gas/brake driver dan openpilot, serta aEgo/aTarget/cmd/out. Alatnya dari
codeberg.org/nelsonjchen/op-replay-clipper. Versi hosted-nya di Replicate hanya
kenal comma Connect, jadi kita jalankan lokal dengan patch.

### Pasang dari nol

```bash
cd ~/Code/openpilot
git clone https://codeberg.org/nelsonjchen/op-replay-clipper.git
cd op-replay-clipper
git checkout 9070cd9                     # versi tempat patch dibuat
git apply ../sunnypilot/tools/car_porting/op-replay-clipper-konik.patch
uv sync

# openpilot upstream dipin ke 19 Agustus 2026; master sudah terlalu jauh berubah
git clone --filter=blob:none https://github.com/commaai/openpilot.git .cache/openpilot-local
cd .cache/openpilot-local
git checkout 555f48c
git submodule update --init --recursive
git lfs install --local && git lfs pull  # tanpa ini font masih pointer 131 byte
cd ../..
```

Bootstrap openpilot (`uv sync` plus scons) jalan otomatis di render pertama.

### Render

```bash
cd ~/Code/openpilot/op-replay-clipper
TOKEN=$(python3 -c "import json;print(json.load(open('$HOME/.comma/auth.konik.json'))['access_token'])")
env -u VIRTUAL_ENV SCALE=0.5 API_HOST=https://api.konik.ai uv run python clip.py ui-alt \
  "391b5ce35cb77d89|000004df--29c0050d5a" -s 228 -l 15 \
  --qcam --ui-alt-variant device --windowed --skip-openpilot-update \
  -j "$TOKEN" -o ./shared/out.mp4
```

- `-s` dihitung dalam detik dari awal route (segment N mulai di 60 x N);
  `-l` maksimal 300.
- `--skip-openpilot-update` wajib. Tanpa flag ini, clipper menarik master dan
  pin 555f48c tertimpa.
- `--qcam` wajib, karena Konik tidak punya fcamera. Karena itu variant
  `stacked_*` yang butuh wide juga tidak bisa.
- `--windowed` wajib di Mac, karena mode headless clipper hanya untuk Linux
  (Xorg). Jendela UI akan muncul selama render.
- `SCALE` mengatur jendela sekaligus resolusi video: tanpa SCALE 2160x1080,
  0.75 menjadi 1620x810, 0.5 menjadi 1080x540. qcamera sendiri 526x330, jadi
  0.5 tidak kehilangan detail kamera.
- `-m` target ukuran file dalam MB (default 9). Kecilkan untuk WhatsApp atau
  Discord.
- Untuk render ulang, tambahkan `--skip-openpilot-bootstrap --skip-download`.
  Render 15 detik makan sekitar 1 menit.
- Token Konik ada di `~/.comma/auth.konik.json`. Jangan tempel token ke log
  atau chat.

### Isi patch

File `tools/car_porting/op-replay-clipper-konik.patch`, di atas clipper 9070cd9:

| File | Perubahan | Kenapa |
|---|---|---|
| `core/route_downloader.py`, `core/route_inputs.py` | URL API membaca `API_HOST` | Aslinya di-hardcode ke api.commadotai.com |
| `core/openpilot_bootstrap.py` | Target scons untuk struktur `openpilot/` | Upstream #38219 (Juni) memindah semua ke `openpilot/`; #38411 (Juli) membuat params jadi ctypes `libparams_c` |
| `renderers/ui_renderer.py` | Deteksi `openpilot/tools/clip/run.py`, lewati langkah `.fnt` | UI baru memuat `.ttf` langsung |
| `renderers/big_ui_engine.py` | `VisionStreamType` dari `openpilot.cereal.visionipc`; ROAD menjadi NARROW_ROAD | API msgq pindah ke cereal |
| `renderers/big_ui_engine.py` | `roadEncodeIdx` fallback ke `narrowRoadEncodeIdx` | Nama service kamera berubah di log baru |

Kalau clipper atau pin openpilot diperbarui dan ada yang patah, urutan
kegagalan yang dulu ditemui: scons target, clip tool tidak ditemukan, font,
headless Xorg, `VisionStreamType`, lalu `roadEncodeIdx`. Log tiap run ada di
`shared/*.log`; cari `Traceback` paling dalam (dari `big_ui_engine.py`).

## Temuan route 000004df (28 September 2026)

Data rlog 100 Hz. Engaged hanya di segment 2 sampai 4, semua di bawah 30 km/h.
Data bersih (engaged, driver lepas lebih dari 2 detik) sekitar 18 detik.

- Low-pass filter meredam jiggle model 40 sampai 80%. Di seg 4 (merayap)
  jiggle turun dari 3.18 ke 1.30 derajat. EPS mengikuti output dengan lag 30
  sampai 130 ms.
- Seg 3 (16 km/h) hampir ideal: selisih actual dan output median 0.34, p95
  0.9 derajat, tanpa saturasi.
- Seg 4 (merayap) p95 2.8 derajat. Seg 2 p95 3.7 derajat, saturasi 6.7%.
- Di seg 2 driver melawan model 11 dari 26 detik engaged. Model minta 6 sampai
  20 derajat lebih ke kanan di jalan sempit, torsi driver +80 sampai +130.
  angleOffset (sekitar 2.2 derajat) dan steerRatio 16.75 stabil, jadi itu
  perilaku model, bukan kalibrasi.
- Di awal seg 0 (mobil baru hidup, diam) `carOutput` bernilai 0 derajat
  sementara setir +11 derajat, karena carcontroller belum jalan.

Belum ada data di atas 30 km/h. Filter (tau 0.5 detik) sudah mati di atas
40 km/h, jadi kecepatan tinggi menguji carcontroller tanpa filter. Sebelum
menyebut lateral bagus, ambil minimal 5 menit engaged, tangan ringan, untuk
masing-masing:

1. Kota, 20 sampai 40 km/h.
2. Arteri, 50 sampai 80 km/h, ada tikungan.
3. Tol, di atas 80 km/h.

Patokan kasar:

- Tracking p95 di bawah sekitar 1 derajat, kecuali saat merayap.
- Jiggle output di bawah sekitar 0.3 derajat di kecepatan tinggi.
- Saturasi mendekati 0%.
- Override driver jarang, dan kalau terjadi alasannya jelas di video.

## Catatan lain

- Di cereal ini `liveParameters` bernama `vehicleParameters`.
- Kamera jalan bernama `narrowRoad`: `narrowRoadEncodeIdx` untuk fcamera dan
  `qNarrowRoadEncodeIdx` untuk qcamera. Keduanya memakai frame id yang sama.
- Auto mode Claude Code kadang gagal memverifikasi perintah jaringan (Bash,
  web). Coba lagi atau jalankan sendiri dengan `!`.
