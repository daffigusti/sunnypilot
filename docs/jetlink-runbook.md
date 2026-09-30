# Jetlink di Chery Omoda E5: runbook

Catatan hasil pengujian 22, 23, dan 30 September 2026. Comma 3X (tizi), MacBook
Pro 16 inci M3 Max, branch `chery-jetlink-v072` (jetlink v0.7.2, server Swift),
model besar Cinque Terre V3.

## Urutan pakai di mobil

1. Harness ke comma. Biarkan comma boot penuh sampai UI muncul.
2. Mac di charger mobil USB-C PD, minimal 65 W. High Power Mode on
   (System Settings > Battery > Energy Mode, untuk power adapter dan battery).
3. Buka Jetlink.app. Settings > Backend: **CoreML on the GPU**, bukan Neural
   Engine (lihat Gejala). Tutup browser, Termius, dan aplikasi Electron lain.
4. Colok kabel USB-C ke C dari comma ke port Mac yang sudah teruji. Pakai kabel
   dan port yang sama terus. C-to-C memberi USB 3; A-to-C yang dicoba hanya USB 2.
5. Ignition on. Settings > Models > Accelerator Link harus **USB**, model besar
   Cinque Terre V3. Jetlink.app > Status: Comma connected, Connected over USB 3.

Batas arus USB 500 mA (lihat bawah) dipasang owner jetlink sekitar 40 detik
setelah comma boot. Boot dengan kabel tercolok sudah lolos, tapi kalau comma
mati saat boot, colok kabel Mac setelah UI muncul.

## Batas arus USB 500 mA

Lewat kabel C-to-C, Mac menawarkan 3 A lewat PD, dan charger comma lalu
mengambil hampir semua dayanya dari Mac, bukan dari harness: arus harness turun
dari 1.3 A ke sekitar 0.05 A, VBUS turun ke 4.6 V, dan PMIC mematikan comma
(`Power-off reason: Triggered from UVLO`) 25 sampai 84 detik setelah model
besar join, berulang selama kabel tercolok. Tegangan 12 V mobil tetap 12.9
sampai 13.7 V, jadi aki dan harness tidak bermasalah.

Obatnya jetlink `0e3b0ef`: `jetlink-root.sh vm apply` memaksa voter `USB_ICL`
charger ke 500 mA selama Accelerator Link aktif, `vm restore` melepasnya saat
link dimatikan, reboot mengembalikan nilai PD sampai owner memasangnya lagi.
Link tetap super-speed, harness yang menyuplai comma. Kabel A-to-C dari port
USB-A hanya menawarkan maksimal 900 mA, karena itu upstream tidak kena.

## Cek cepat

Di comma (ssh):

```bash
cat /sys/class/udc/a600000.dwc3/current_speed                         # harus super-speed
sudo cat /sys/kernel/debug/pmic-votable/USB_ICL/status | grep effective # harus v=500000
cat /sys/class/power_supply/battery/current_now                       # arus harness, ratusan ribu uA ke atas
free -m | head -2                                                     # pemakaian di bawah 90%
tail -20 /data/log/jetlink-owner.log
```

Di Mac:

```bash
tail -f ~/Library/Logs/Jetlink/server.log        # slow frame harus jarang
grep -c "slow frame" ~/Library/Logs/Jetlink/server.log
```

Jetlink.app > Status: rate 20 per detik, frame sekitar 20 ms di GPU, slow 0.

## Angka acuan

| kondisi | p50 | p99 | lewat 50 ms |
| --- | ---: | ---: | ---: |
| v0.7.2 bench, Neural Engine | 23.9 ms | 28.8 ms | 1/1201 (pemanasan) |
| v0.7.2 bench, CoreML GPU | 19.0 ms | 24.9 ms | 1/1201 |
| v0.7.2 client Python ke server Swift, TCP lokal | 24.3 ms | 27.9 ms | 0/1190 |
| v0.7.2 di mobil diam, Neural Engine, dengan comma | 24.8 ms | 25.8 ms | 3 menit bersih, lalu 14 slow dalam 5 menit |
| v0.7.2 drive 30 Sep, CoreML GPU, dengan comma | ~20 ms | - | 3 slow dan 1 lepas dalam 10 menit, 5 menit terakhir bersih |

Protocol v0.7 mengirim 393 KB per frame ke Mac dan menerima 8 KB. Lewat USB 2
itu sekitar 10 ms per frame, lewat USB 3 1 sampai 2 ms.

## Gejala dan obatnya

| gejala | sebab | obat |
| --- | --- | --- |
| comma reboot 25 sampai 84 detik setelah model besar join, `dmesg` menulis UVLO | comma menarik daya dari Mac lewat PD 3 A | pastikan `USB_ICL` 500 mA aktif (Cek cepat); kalau tidak, cek jetlink `0e3b0ef` ada di comma |
| link putus-sambung tiap 20 sampai 40 detik, `hello from modeld` baru tiap kali, slow frame gpu 60 sampai 210 ms | frame Mac di atas 100 ms, atau dua di atas 75 ms dalam 10 detik: comma melepas model besar ke model kecil (v0.7.1) dan join lagi. Neural Engine dipakai bersama fitur macOS | backend CoreML on the GPU; tutup Chrome dan app berat |
| TAKE CONTROL 5 detik saat engaged, lalu tetap nyetir | model besar dilepas, seperti baris di atas | sama; engage ulang untuk kembali ke model besar |
| log Mac: `peer speaks protocol v3, we speak v2` | app Jetlink lama (v0.3) yang jalan | buka `/Applications/Jetlink.app` (v0.7.2) |
| log Mac: `the comma's gadget is on the bus, but nothing on the comma is serving it yet` | mobil offroad, provisioning tidak ada kerjaan | normal; frame baru mengalir saat ignition on |
| `current_speed = high-speed`, send sekitar 10 ms per frame | kabel atau port USB 2 | kabel C-to-C USB 3, atau port Mac lain |
| alert Low Memory | pemakaian comma di atas 90% (30 Sep: 83% onroad) | matikan `OsmLocal` kalau peta offline tidak dipakai |
| mobil mepet satu sisi lajur | kalibrasi lama | reset kalibrasi (sudah menyelesaikan kasus 30 Sep) |
| link putus saat lid ditutup atau Mac di baterai | clamshell sleep atau idle sleep macOS | charger tetap tercolok; lid tertutup butuh `sudo pmset -a disablesleep 1` |
| Mac hanya melihat `panda` (3801:ddcc), bukan `jetlink` (1209:0001) | kabel masuk jalur panda, bukan port data comma | pindah ke port yang benar |

## Kalau "waiting for comma" muncul lagi

Jangan toggle atau cabut dulu: keduanya menghapus jejaknya. Ambil ini dulu.

Di comma:

```bash
cat /sys/class/udc/*/state                       # configured / not attached / default
cat /sys/class/power_supply/usb/typec_mode       # Mac tercolok: "high current"
cat /data/jetlink-server.json                    # hello server terakhir, sleep_after harus 0.0
ls /dev/shm/jetlink-dormant 2>/dev/null          # ada = owner sengaja melepas gadget
tail -20 /data/log/jetlink-owner.log
dmesg | grep -E "usbpd|USB_STATE|configfs|UVLO" | tail
```

Di Mac:

```bash
system_profiler SPUSBHostDataType | grep -A6 -iE "jetlink|panda"
tail -20 ~/Library/Logs/Jetlink/server.log
```

Cara membaca:

- `waiting for a jetlink gadget` di Mac artinya macOS tidak melihat comma;
  `could not open the gadget` artinya terlihat tapi gagal dibuka.
- Owner log `releasing the gadget so the jetson can sleep` sementara Mac
  menyala: `sleep_after` di `/data/jetlink-server.json` salah.
- `typec_cc_orientation` dan `pd_active` di tizi mengikuti jalur daya
  harness, bukan kabel Mac. Jangan dipakai untuk menilai kabel.
- Mencabut ujung Mac: comma tidak melihat disconnect, UDC tetap `configured`.
  Jetlink membaca turunnya iklan arus Type-C (3 A ke default) sebagai Mac
  dicabut, jadi ikon hilang setelah sekitar 5 detik.
- Baris owner log bertanggal `2026-07-28 22:05` adalah boot baru, sebelum jam
  tersinkron. Beberapa berurutan dalam hitungan menit berarti comma reboot.

Setelah update kode di comma, reboot saat offroad.

## Apa yang ada di mana

- sunnypilot fork, branch `chery-jetlink-v072`: merge zoompilot `jetson-trt`
  (jetlink v0.7.2) ke branch Chery. Kode jetlink sisi comma sekarang di
  submodule (`jetlink/openpilot`, `jetlink/comma`); fork hanya menyimpan
  `openpilot/sunnypilot/jetlink_adapter`. Warp dibangun SConscript ke
  `jetlink_adapter/models/`. Branch lama `feature/chery-jetlink` utuh untuk
  rollback.
- opendbc fork: tetap fork Chery (`a16e7d4`). opendbc zoompilot sejak merge
  sebelumnya hanya berubah di Mazda.
- jetlink fork, branch `hf-first`: v0.7.2 plus Hugging Face sebelum GitLab,
  pool compile tinygrad dimatikan (244 MB), deteksi Mac dicabut lewat iklan
  arus Type-C, hello server disimpan di `/data`, dan batas arus USB 500 mA.
  Model stateful, Metal keep-alive, dan fix sesi hantu sudah ada di upstream.
- Katalog model besar: chestnut v26 dari upstream (v25 plus Cinque Terre V3
  di selector 19, terbaca juga oleh sunnylink). Patch selector 20 di
  `models/helpers.py` masih ada tapi tidak dipakai. Setelah pindah dari v27,
  pilih ulang Cinque Terre V3 sekali.
- Mac: `/Applications/Jetlink.app` build `hf-first` (`make -C macos app`, versi
  `0.7.2-4-g54fd5bf` saat dipasang). Cadangan: `Jetlink-stateful.app` (v0.3,
  protocol lama, pasangan `feature/chery-jetlink`), `Jetlink-prev.app`,
  `Jetlink-keeper.app`. Kalau build membawa sisa app lama: `make -C macos clean`.

Update ke depan: jetlink, fetch `origin/main` zoompilot lalu rebase atau merge
`hf-first`; sunnypilot, merge `zoompilot/jetson-trt` dan pin `jetlink_repo` ke
`hf-first`. Protocol comma dan Mac harus sama: update keduanya bersamaan. Di
comma updater mati (`DisableUpdates=1`), update manual:

```bash
cd /data/openpilot
git fetch --no-recurse-submodules origin chery-jetlink-v072 && git merge --ff-only FETCH_HEAD
git submodule update jetlink_repo
sudo reboot
```

Rollback: `git checkout feature/chery-jetlink && git submodule update jetlink_repo`,
reboot, dan di Mac buka `Jetlink-stateful.app`.

## Belum tervalidasi

- Drive panjang dengan v0.7.2: baru satu drive sekitar 10 menit (30 Sep),
  tanpa reboot, satu kali model besar dilepas.
- Lonjakan frame GPU 80 sampai 92 ms di Mac (sekali tiap beberapa menit, satu
  memicu pelepasan). Dugaan: app lain di Mac. Belum dites dengan Mac sepi.
- Kenapa 23 September VBUS 5.2 sampai 5.4 V dan tidak UVLO, sedangkan 30
  September 4.6 V. Batas 500 mA membuatnya tidak penting lagi.
- 40 detik pertama boot, sebelum owner memasang batas, comma masih bisa menarik
  daya dari Mac. Aman di satu boot yang dites.
- Memori comma 83% onroad, dekat batas alert 90% di drive panjang.
