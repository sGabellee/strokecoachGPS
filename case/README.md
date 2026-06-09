# 📦 Case & Mounting

3D-printable enclosure files for the RowComputer live here.

## Mounting tips

A few placement rules that directly affect how well the device works on the
water (see [../HARDWARE.md](../HARDWARE.md) for the components themselves):

- **GPS antenna faces the sky.** Mount the GPS module (or its external active
  antenna) flat on the **top** of the case with a clear, unobstructed view
  upward. Don't bury it under the battery pack or behind the screen — the
  ceramic patch must point at the sky for a fast lock and a steady speed/split.
- **Keep the antenna away from the display ribbon and the ESP32.** A few
  centimetres of separation reduces electrical noise on the GPS.
- **Angle the display toward the rower.** With a reflective/sunlight-readable
  panel (recommended in HARDWARE.md), tilt it so ambient light falls on it and
  it faces your eyes — reflective LCDs look best lit from the front, not backlit.
- **Mount the device rigidly to the boat**, not loosely, so the MPU-6050 reads
  the hull's acceleration cleanly. The stroke algorithm removes gravity at any
  angle, so the orientation itself does not matter — only that it doesn't wobble
  independently of the boat.
- **Weatherproofing.** Rowing means spray. Add a gasket and route the USB-C /
  battery opening downward so water can't pool in it.
