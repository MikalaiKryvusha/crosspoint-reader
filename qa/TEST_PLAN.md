# Krinik build — test plan (simulator + device)

Simulator: `pio run -e simulator_x4_pro` (PATH=/usr/bin first: Apple `ar`), scripted input + BMP screenshots,
`CROSSPOINT_SIM_SD` = a throwaway copy of the card. The simulator does not model LUT waveforms, ghosting or timing:
rows marked **device** are checked by Nikolai on the reader.

| # | Feature | Case | Where | Expected |
|---|---|---|---|---|
| 1 | Sleep clean | page text never shows through the sleep image; no harsh B/W draft before grays | device | white clean → grays |
| 2a | Sleep order | 3 sleeps in a row | sim | images 1, 2, 3 (path order) |
| 2b | Sleep order | after the last image | sim | wraps to the first |
| 2c | Sleep order | image added / removed between sleeps | sim | new one joins in order; removed one skipped |
| 2d | Sleep order | folders empty, /sleep.bmp present | sim | /sleep.bmp |
| 2e | Sleep order | /.sleep and /sleep both hold images | sim | one sequence across both |
| 3 | Note links | «Четверояк. корень» notes 12, 14 | sim + device | page starts with the note text «[012] …» |
| 8a | Panel curve | flat images + curve vs baked images, no curve | sim | identical screenshots |
| 8b | Panel curve | malformed / missing curve file | sim | renders as without curve, no crash |
| 11 | About | labels in Russian, SoC Flash, SD rows; list scrolls to the last row | sim + device | values present, last row reachable |
| 12a | Viewer | BMP / PNG / JPG open in gray, small ones enlarged, centered | sim ✅ 04.11 (`qa_runs/v_flow`) | gray image, theme-colored fields |
| 12b | Viewer | next from the last → first, back from the first → last | sim ✅ 04.11 | wraps both ways |
| 12c | Viewer | long tap rotates 90°, side tap flips in landscape, Back restores portrait | sim ✅ 04.11 | rotated, exit in portrait |
| 12d | Viewer | dark theme | sim ✅ 04.11 (`qa_runs/v_dark`) | positive image, black fields, dark «Загрузка» |
| 12e | Viewer | flip: «Загрузка» on a clean sheet, no harsh B/W flash, then deep clean → grays | device | no harsh frame |
| 13 | Rails before gray | SSD1677 host test: powered-down panel → 0xC0 (BUSY) then 0xCC; powered → 0xCC only; mutation (no power-on) fails | host ✅ 04.11 (`run_pro.py`) | as described |
| 13a | Rails before gray | gray sleep / viewer images, 20+ flips and 5+ sleeps after menus: band ~1 cm from the top, «Загрузка» ghost | device | none on any paint |
| 13b | Rails before gray | B/W after gray: menus, book pages, file browser after the viewer and after wake | device | clean, no gray residue, no stuck refresh |
| 13c | Rails before gray | battery after a night in sleep (sleep image paint powers the rails) | device | no extra drain vs before |
| 12f | Viewer | long tap rotates clockwise; four taps back to portrait | sim (orientation math) + device | clockwise |
| 14 | /.firmware | SD update opens in /.firmware; Back shows root with .firmware; file browser hides it | sim ✅ 04.11 (`qa_runs/v_fw`) | as described |
| R | Regression | boot → Home → Settings → open book → turn pages → sleep → wake | sim | no crash, screens as before |

## Device run 04.11.2026 (Nikolai, X4 Pro with the **UC8279** panel)
✅ viewer: edge swipe pages back (no exit) · touch off → buttons only · reading-orientation change applies after closing the
control center, picture keeps its long-tap turn · dark theme: positive picture, black fields · JPG/PNG gray, small enlarged
· About shows «Загрузка» first · B/W screens clean after gray and after wake · control center over a gray photo opens in its
theme (UC8279 displayDrivingRect) · orientation tile icon · Home cover after theme switch · photo-only long-tap rotation.
⏳ battery after a night of sleep (05.11). ⏳ trial: HALF (UC8279 scrub) before gray vs ghosts — build a880d886.
