#pragma once

// A gray (absolute four-tone) image is on the glass right now — the viewer's last paint. A sheet opening over it (the
// control center) paints its first frame by driving only its own rect, so the image stays and no full-frame clean
// flashes. Cleared when the screen under it is repainted in B/W.
namespace grayglass {
extern bool imageOnGlass;
}  // namespace grayglass
