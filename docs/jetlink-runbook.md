# Jetlink di Chery Omoda E5: runbook

Catatan hasil pengujian 22 September 2026. Comma 3X (tizi), MacBook Pro 16
inci M3 Max, branch `feature/chery-jetlink`, model besar default (BMRLNAP v4).

## Urutan pakai di mobil

1. Harness ke comma. Biarkan comma boot penuh sampai UI muncul.
2. Mac di charger mobil USB-C PD, minimal 65 W. High Power Mode on
   (System Settings > Battery > Energy Mode, untuk power adapter dan battery).
3. Buka Jetlink.app. Keeper GPU hidup sendiri (`gpu clock keeper running` di
   Logs). Tutup browser, Termius, dan aplikasi Electron lain. Terminal.app
   boleh.
4. Baru colok kabel USB-C ke C dari comma ke port Mac yang sudah teruji.
   Pakai kabel dan port yang sama terus.
5. Ignition on. Settings > Models harus menampilkan nama model besar dan status
   jalan. Indikator GPU di home hijau.

Kenapa urutannya begitu: dengan kabel Mac tercolok sejak boot, comma tiga kali
reboot saat model besar join. PMIC mencatat UVLO (tegangan drop). Dengan kabel
dicolok setelah boot, tegangan `usb` stabil 5.2 sampai 5.4 V.

## Cek cepat

Di comma (ssh):

```bash
cat /sys/class/udc/a600000.dwc3/current_speed   # harus super-speed
free -m | head -2                               # pemakaian di bawah 90%
pgrep -fc multiprocessing.spawn                 # 1 (hanya ssh ini), bukan 9
```

Di Mac:

```bash
tail -f ~/Library/Logs/Jetlink/server.log        # slow frame harus jarang
grep -c "slow frame" ~/Library/Logs/Jetlink/server.log
```

Jetlink.app > Status: fps 20, frame time sekitar 23 ms, slow 0.

## Angka acuan

| kondisi | p50 | p99 | lewat 50 ms |
| --- | ---: | ---: | ---: |
| awal, tanpa apa-apa | 74 ms | 105 ms | 42% |
| keeper GPU + AC + High Power, loopback | 18 ms | 22 ms | 0% |
| dengan comma, USB 3, di meja | 27 ms | 32 ms | 0.05% |
| di mobil, 30 menit, sisi Mac | 23 ms | 23 ms | 0 |

Penyebab angka awal: governor macOS menurunkan clock GPU dari 1345 ke 340 MHz
setelah 20 detik beban 40% duty. Keeper (`--keep-gpu-busy`, otomatis di Mac)
menjaga antrian GPU tidak kosong. Di baterai tetap 8.8% lewat budget, jadi
charger wajib.

## Gejala dan obatnya

| gejala | sebab | obat |
| --- | --- | --- |
| `current_speed = high-speed`, `send 12 ms` per frame | kabel atau port USB 2 | ganti kabel USB 3 atau port Mac lain, cek lagi |
| join macet di `joining`, modeld tulis `no reader for 15 s` | sesi hantu di server (sudah dipatch di build sendiri) | Restart server di Jetlink.app |
| slow frame melonjak, gpu 60 sampai 110 ms, link tidak putus | aplikasi lain rebutan GPU Mac | tutup Chrome, Termius, tab Logs |
| alert Low Memory | pemakaian comma di atas 90% | sudah turun ke 87% setelah pool tinygrad dimatikan; matikan `OsmLocal` kalau peta offline tidak dipakai |
| comma reboot saat model besar join | tegangan drop (UVLO) | colok kabel Mac setelah comma boot |
| `link closed: LIBUSB_ERROR_IO` lalu `waiting for a jetlink gadget` saat offroad | comma lepas gadget karena idle, normal | tidak perlu apa-apa |

## Apa yang ada di mana

- sunnypilot fork, branch `feature/chery-jetlink`: merge zoompilot `jetson-trt`
  (jetlink sisi comma) ke branch Chery, plus commit `5ff003c23` yang mematikan
  pool compile tinygrad (244 MB).
- opendbc fork, branch `feature/chery-jetlink`: merge opendbc zoompilot, wajib
  karena kode car dan selfdrived zoompilot mengimpornya saat boot.
- jetlink fork, branch `keep-gpu-busy`: keeper clock GPU dan fix sesi hantu.
  Jetlink.app di Mac adalah build sendiri dari branch ini (`make -C macos app`),
  app resmi tersimpan di `/Applications/Jetlink-prev.app`.

Update ke depan: merge `origin/master` sunnypilot seperti biasa; untuk jetlink,
fetch `zoompilot/jetson-trt` dan merge, opendbc sama. Di comma updater mati
(`DisableUpdates=1`), update manual dengan
`git fetch --no-recurse-submodules origin feature/chery-jetlink && git reset --hard origin/feature/chery-jetlink`.
Tanpa `--no-recurse-submodules` fetch gagal di submodule lama zoompilot.

## Belum tervalidasi

- Perjalanan panjang: panas kabin ke Mac, getaran ke konektor USB-C. Sempat
  terlihat `send 13 sampai 19 ms` di beberapa frame saat mobil jalan.
- Reboot UVLO belum direkam saat kejadian; hipotesis VBUS saat boot belum
  dibuktikan, hanya dihindari.
- Fix sesi hantu belum terpicu di kejadian nyata sejak dipasang.
- Engage rutin: tunggu dua atau tiga perjalanan 20 sampai 30 menit bersih.
