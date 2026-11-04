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
| 12 | Viewer | prev/next by buttons and side taps, wrap-around, long tap rotates, clean between images | sim + device | — |
| R | Regression | boot → Home → Settings → open book → turn pages → sleep → wake | sim | no crash, screens as before |
