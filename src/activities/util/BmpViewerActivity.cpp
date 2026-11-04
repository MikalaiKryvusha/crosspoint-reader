#include "BmpViewerActivity.h"

#include <Bitmap.h>
#include <BmpToBmpConverter.h>
#include <Epub/converters/JpegToFramebufferConverter.h>
#include <Epub/converters/PngToFramebufferConverter.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/CacheLimit.h"
#include "util/DeepClean.h"
#include "util/GrayGlass.h"
#include "util/PanelCurve.h"

namespace {
constexpr char CUSTOM_SLEEP_ROOT_BMP[] = "/sleep.bmp";
constexpr char CUSTOM_SLEEP_TMP_BMP[] = "/sleep.bmp.tmp";
constexpr char JPEG_SLEEP_TMP_PXC[] = "/sleep.bmp.pxc.tmp";
constexpr char VIEW_TMP_PXC[] = "/.crosspoint/view.pxc.tmp";
constexpr char TRANSPARENT_SLEEP_ROOT_BMP[] = "/sleep-overlay.bmp";
constexpr char TRANSPARENT_SLEEP_ROOT_PNG[] = "/sleep-overlay.png";
constexpr size_t COPY_BUFFER_SIZE = 2048;
constexpr uint8_t GRAYSCALE_PALETTE[] = {0, 0, 0, 0, 85, 85, 85, 0, 170, 170, 170, 0, 255, 255, 255, 0};
// Bump when the cached rendering changes, so stale caches are not reused.
constexpr char VIEW_CACHE_VERSION[] = "v2";

// Fits the image into the screen. The viewer also enlarges small images (as BMPs are), the sleep cover does not.
RenderConfig fitImage(const ImageDimensions& dimensions, const GfxRenderer& renderer, const bool enlarge = false) {
  const float scale =
      std::min({static_cast<float>(renderer.getScreenWidth()) / dimensions.width,
                static_cast<float>(renderer.getScreenHeight()) / dimensions.height, enlarge ? 1e6f : 1.0f});
  const int width = std::max(1, std::min(renderer.getScreenWidth(), static_cast<int>(dimensions.width * scale)));
  const int height = std::max(1, std::min(renderer.getScreenHeight(), static_cast<int>(dimensions.height * scale)));
  RenderConfig config{(renderer.getScreenWidth() - width) / 2, (renderer.getScreenHeight() - height) / 2, width,
                      height};
  config.useExactDimensions = true;
  return config;
}

bool overlaySleepMode() {
  const uint8_t mode = SETTINGS.sleepScreen;
  return mode == CrossPointSettings::SLEEP_SCREEN_MODE::TRANSPARENT_CUSTOM ||
         mode == CrossPointSettings::SLEEP_SCREEN_MODE::COVER_TRANSPARENT;
}

// Writes a decoder pixel cache (.pxc: uint16 width, uint16 height, then 2-bit rows) as a top-down 2-bit BMP.
bool pixelCacheToBmp(const char* pxcPath, const char* bmpPath, const int expectedWidth, const int expectedHeight) {
  HalFile input, output;
  if (!Storage.openFileForRead("BMP", pxcPath, input)) return false;
  uint16_t width, height;
  if (input.read(&width, sizeof(width)) != sizeof(width) || input.read(&height, sizeof(height)) != sizeof(height) ||
      width != expectedWidth || height != expectedHeight) {
    LOG_ERR("BMP", "Invalid image pixels");
    return false;
  }
  const size_t pixelRowBytes = (width + 3) / 4;
  const size_t bmpRowBytes = (width + 15) / 16 * 4;
  if (input.size() != 4 + pixelRowBytes * height) {
    LOG_ERR("BMP", "Incomplete image pixels");
    return false;
  }
  auto row = makeUniqueNoThrow<uint8_t[]>(bmpRowBytes);
  if (!row) {
    LOG_ERR("BMP", "OOM: image row");
    return false;
  }
  if (!Storage.openFileForWrite("BMP", bmpPath, output)) return false;
  BmpHeader header;
  createBmpHeader(&header, width, height, BmpRowOrder::TopDown);
  header.infoHeader.biBitCount = 2;
  header.infoHeader.biClrUsed = header.infoHeader.biClrImportant = 4;
  header.infoHeader.biSizeImage = bmpRowBytes * height;
  header.fileHeader.bfOffBits = sizeof(header) - sizeof(header.colors) + sizeof(GRAYSCALE_PALETTE);
  header.fileHeader.bfSize = header.fileHeader.bfOffBits + header.infoHeader.biSizeImage;
  if (output.write(&header, sizeof(header) - sizeof(header.colors)) != sizeof(header) - sizeof(header.colors) ||
      output.write(GRAYSCALE_PALETTE, sizeof(GRAYSCALE_PALETTE)) != sizeof(GRAYSCALE_PALETTE)) {
    LOG_ERR("BMP", "Failed to write image header");
    return false;
  }
  memset(row.get(), 0, bmpRowBytes);
  for (int y = 0; y < height; ++y) {
    if (input.read(row.get(), pixelRowBytes) != pixelRowBytes || output.write(row.get(), bmpRowBytes) != bmpRowBytes) {
      LOG_ERR("BMP", "Failed to write image row %d", y);
      return false;
    }
  }
  return output.close();
}

// A long tap turns the picture clockwise: portrait → landscape (content turned right) → inverted → the other landscape.
constexpr GfxRenderer::Orientation ROTATION_ORDER[] = {
    GfxRenderer::Orientation::Portrait, GfxRenderer::Orientation::LandscapeCounterClockwise,
    GfxRenderer::Orientation::PortraitInverted, GfxRenderer::Orientation::LandscapeClockwise};
// Night mode with an image on the glass: the image was painted with the output polarity off (the driver offers no
// gray while inverted). Re-express the framebuffer for the inverted polarity and switch it back on without forcing a
// clean refresh — the controller already holds the frame on the panel — so the control center slides in dark over the
// untouched gray image instead of a light full-screen repaint of the B/W draft.
void restNightPolarity(GfxRenderer& renderer) {
  if (SETTINGS.screenInverted == 0) return;
  renderer.invertScreen();
#ifdef SIMULATOR
  display.setInverted(true);
#else
  display.setInvertedKeepingPanel(true);
#endif
}
}  // namespace

BmpViewerActivity::BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
    : Activity("BmpViewer", renderer, mappedInput), filePath(std::move(path)) {}

void BmpViewerActivity::loadSiblingImages() {
  siblingImages.clear();
  currentImageIndex = -1;

  if (filePath.empty()) return;

  std::string dirPath = FsHelpers::extractFolderPath(filePath);
  size_t lastSlash = filePath.find_last_of('/');
  std::string fileName = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

  auto dir = Storage.open(dirPath.c_str());
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }

  const auto name = makeUniqueNoThrow<char[]>(500);
  if (!name) {
    LOG_ERR("BMP", "OOM: sibling filename buffer");
    return;
  }
  size_t imageCount = 0;
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(name.get(), 500);
    if (name[0] != '.' && FsHelpers::hasImageExtension(std::string_view{name.get()})) ++imageCount;
  }
  dir.rewindDirectory();
  siblingImages.reserve(imageCount);

  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (!file.isDirectory()) {
      file.getName(name.get(), 500);
      if (name[0] != '.') {
        std::string fname(name.get());
        if (FsHelpers::hasImageExtension(fname)) {
          siblingImages.push_back(fname);
        }
      }
    }
    file.close();
  }
  dir.close();

  FsHelpers::sortFileList(siblingImages);

  const auto image = std::find(siblingImages.begin(), siblingImages.end(), fileName);
  if (image != siblingImages.end()) {
    currentImageIndex = static_cast<int>(image - siblingImages.begin());
  }
}

bool BmpViewerActivity::canSetSleepCover() const {
  return FsHelpers::hasBmpExtension(filePath) || FsHelpers::hasJpgExtension(filePath) ||
         (overlaySleepMode() && FsHelpers::hasPngExtension(filePath));
}

// Prepares the current image as a screen-ready 2-bit BMP through the panel tone curve, once per image, size and
// rotation; later visits reuse the cache. A BMP that is already ≤ 2 bits and fits the screen is drawn as is.
bool BmpViewerActivity::prepareCache(std::string& outPath) {
  HalFile source;
  if (!Storage.openFileForRead("BMP", filePath, source)) return false;
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const std::string key = filePath + '|' + std::to_string(source.size()) + '|' +
                          std::to_string(source.modificationTime()) + '|' + std::to_string(screenW) + 'x' +
                          std::to_string(screenH) + "|k" + std::to_string(curveHash) + '|' + VIEW_CACHE_VERSION;
  const bool bmp = FsHelpers::hasBmpExtension(filePath);
  if (bmp) {
    Bitmap probe(source);
    if (probe.parseHeaders() != BmpReaderError::Ok) return false;
    if (probe.getBpp() <= 2 && probe.getWidth() <= screenW && probe.getHeight() <= screenH) {
      outPath = filePath;
      return true;
    }
  }

  outPath = "/.crosspoint/view_" + std::to_string(std::hash<std::string>{}(key)) + ".bmp";
  if (Storage.exists(outPath.c_str())) return true;
  const std::string tmpPath = outPath + ".tmp";
  bool ok = false;
  if (bmp) {
    source.close();
    if (!Storage.openFileForRead("BMP", filePath, source)) return false;
    HalFile out;
    ok = Storage.openFileForWrite("BMP", tmpPath, out) &&
         BmpToBmpConverter::bmpFileToBmpStream(source, out, /*crop=*/false, /*originalThresholds=*/false, curve,
                                               screenW, screenH);
    if (out.isOpen()) ok = out.close() && ok;
  } else {
    source.close();
    const bool jpeg = FsHelpers::hasJpgExtension(filePath);
    JpegToFramebufferConverter jpegDecoder;
    PngToFramebufferConverter pngDecoder;
    ImageToFramebufferDecoder& decoder = jpeg ? static_cast<ImageToFramebufferDecoder&>(jpegDecoder)
                                              : static_cast<ImageToFramebufferDecoder&>(pngDecoder);
    ImageDimensions dimensions;
    if (!decoder.getDimensions(filePath, dimensions) || dimensions.width <= 0 || dimensions.height <= 0) return false;
    auto config = fitImage(dimensions, renderer, /*enlarge=*/true);
    config.cachePath = VIEW_TMP_PXC;
    config.toneCurve = curve;
    Storage.remove(VIEW_TMP_PXC);
    // The decoder also draws into the framebuffer; the popup on screen is untouched until the next refresh.
    ok = decoder.decodeToFramebuffer(filePath, renderer, config) &&
         pixelCacheToBmp(VIEW_TMP_PXC, tmpPath.c_str(), config.maxWidth, config.maxHeight);
    Storage.remove(VIEW_TMP_PXC);
  }
  ok = ok && Storage.rename(tmpPath.c_str(), outPath.c_str());
  if (ok) cachelimit::remember("view_", outPath.c_str(), cachelimit::VIEWER_CACHE_FILES);
  if (!ok) {
    Storage.remove(tmpPath.c_str());
    LOG_ERR("BMP", "Viewer cache failed for %s", filePath.c_str());
  }
  return ok;
}

// Paints a ≤ 2-bit bitmap centered: the absolute gray image, whose base pass cleans to white first (one HALF of the
// wiped framebuffer); the B/W framebuffer is rebuilt after for popups and differential updates.
bool BmpViewerActivity::paintBitmap(Bitmap& bitmap) {
  grayglass::imageOnGlass = false;
  // Gray needs normal output polarity (the driver offers no grayscale while inverted), as the sleep screen does.
  // Switch it without repainting the panel: a plain setInverted() in night mode made the next gray pass first repaint
  // the framebuffer — the B/W draft of the photo on screen — a harsh high-contrast flash of it (04.11.2026).
#ifdef SIMULATOR
  display.setInverted(false);
#else
  display.setInvertedKeepingPanel(false);
#endif
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int x = std::max(0, (pageWidth - bitmap.getWidth()) / 2);
  const int y = std::max(0, (pageHeight - bitmap.getHeight()) / 2);
  const bool several = siblingImages.size() > 1 && currentImageIndex != -1;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), canSetSleepCover() ? tr(STR_SET_SLEEP_COVER) : "",
                                            several ? "<" : "", several ? ">" : "");
  // Night mode: the image stays positive, the fields around it («ears») take the theme color. Level 0 (black)
  // writes black into both absolute planes and the B/W frame; the overlay planes leave it to the B/W base.
  const bool dark = SETTINGS.screenInverted != 0;
  const int imageW = std::min(bitmap.getWidth(), pageWidth - x);
  const int imageH = std::min(bitmap.getHeight(), pageHeight - y);
  const bool absolute = renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported();
  const auto draw = [&]() {
    if (bitmap.rewindToData() != BmpReaderError::Ok) return false;
    if (dark && (renderer.getRenderMode() == GfxRenderer::BW || absolute)) {
      renderer.fillRect(0, 0, pageWidth, y, true);
      renderer.fillRect(0, y + imageH, pageWidth, pageHeight - y - imageH, true);
      renderer.fillRect(0, y, x, imageH, true);
      renderer.fillRect(x + imageW, y, pageWidth - x - imageW, imageH, true);
    }
    if (!renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0)) return false;
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    return true;
  };

  if (!bitmap.hasGreyscale()) {
    renderer.clearScreen();
    if (!draw()) return false;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    restNightPolarity(renderer);
    return true;
  }

  if (absolute) {
    // Wipe the framebuffer: on UC8279 the absolute gray pass first paints it with one HALF (the driver's base pass,
    // a «Separate» base), and it still holds the B/W draft of the photo on screen — that HALF flashed a harsh copy of
    // the old photo (04.11.2026). A driver with a «Combined» base (SSD1677) has no base pass: give it the white sheet.
    renderer.clearScreen(0xFF);
    if (renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).base ==
        HalDisplay::GrayscaleBase::Combined) {
      deepclean::shortClean(renderer);
    }
    if (!renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute)) return false;
  } else {
    renderer.clearScreen();
    if (!draw()) return false;
    renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  }
  bool ready = true;
  for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
    renderer.clearScreen(absolute ? 0xFF : 0x00);
    renderer.setRenderMode(mode);
    if (!draw()) {
      ready = false;
      break;
    }
    if (mode == GfxRenderer::GRAYSCALE_LSB) {
      renderer.copyGrayscaleLsbBuffers();
    } else {
      renderer.copyGrayscaleMsbBuffers();
    }
  }
  if (ready) renderer.displayGrayBuffer();

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  if (!draw()) ready = false;
  renderer.cleanupGrayscaleWithFrameBuffer();
  if (ready) restNightPolarity(renderer);
  grayglass::imageOnGlass = ready;
  return ready;
}

void BmpViewerActivity::showMessage(const char* message) {
  renderer.setOrientation(holdOrientation());
  display.setInverted(SETTINGS.screenInverted != 0);
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, message);
  const bool several = siblingImages.size() > 1 && currentImageIndex != -1;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", several ? "<" : "", several ? ">" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void BmpViewerActivity::showCurrent() {
  // Paints from the input loop (onEnter, page turns, rotate, the settled redraw — none holds the lock): hold the render
  // lock so a queued render (its polarity switch would cancel a gray pass) cannot interleave.
  RenderLock lock;
  LOG_DBG("BMP", "Show %d/%d: %s", currentImageIndex + 1, static_cast<int>(siblingImages.size()), filePath.c_str());
  paintedVisit = activityManager.visit();
  paintedAtMs = millis();
  settledRedrawPending = true;
  renderer.setOrientation(drawOrientation());
  const char* failure = paintCurrent();
  renderer.setOrientation(holdOrientation());
  if (failure) showMessage(failure);
}

// Paints the current image in the draw orientation; returns the message to show on failure.
const char* BmpViewerActivity::paintCurrent() {
  // No «Загрузка» popup: the current image stays on screen while the next one is prepared, then the gray paint.
  // A popup left a ghost through the gray image, and drawing it over the framebuffer (the B/W draft
  // of the gray image on screen) flashed a harsh black-and-white frame.
  renderer.setRenderMode(GfxRenderer::BW);

  std::string path;
  if (!prepareCache(path)) {
    return FsHelpers::hasBmpExtension(filePath) ? tr(STR_INVALID_BMP_FILE) : tr(STR_FILE_OPEN_FAILED);
  }
  HalFile file;
  if (!Storage.openFileForRead("BMP", path, file)) {
    return tr(STR_FILE_OPEN_FAILED);
  }
  Bitmap bitmap(file);
  const bool cached = path != filePath;
  if (bitmap.parseHeaders() != BmpReaderError::Ok || !paintBitmap(bitmap)) {
    file.close();
    if (cached) Storage.remove(path.c_str());  // a damaged cache is rebuilt on the next visit
    return tr(STR_INVALID_BMP_FILE);
  }
  file.close();
  return nullptr;
}

void BmpViewerActivity::onEnter() {
  Activity::onEnter();

  if (!orientationSaved) {
    savedOrientation = renderer.getOrientation();
    orientationSaved = true;
  }
  if (!curveReady) {
    // Hot in RAM for the whole viewer session: no SD read per image.
    panelcurve::load(curve, curveHash);
    curveReady = true;
  }
  grayglass::viewerUnderneath = true;
  if (siblingImages.empty() && !filePath.empty()) {
    loadSiblingImages();
  }
  showCurrent();
}

void BmpViewerActivity::onExit() {
  Activity::onExit();
  grayglass::trace("viewer onExit: imageOnGlass=%d sleep=%d", grayglass::imageOnGlass ? 1 : 0, goingToSleep ? 1 : 0);
  grayglass::viewerUnderneath = false;
  renderer.setOrientation(savedOrientation);
  display.setInverted(SETTINGS.screenInverted != 0);
  if (goingToSleep) {
    // The sleep screen erases the gray photo with its own clean before the sleep image: one transition, not two
    // (driving the photo to white here first flashed a harsh copy of it).
    return;
  }
  grayglass::imageOnGlass = false;
  // Settings → Display → «Перерисовка меню» (on by default): a short clean before the menus, so the photo leaves no
  // trace on them.
  if (SETTINGS.menuExitClean) deepclean::shortClean(renderer);
}

void BmpViewerActivity::openSibling(const int delta) {
  const int count = static_cast<int>(siblingImages.size());
  if (currentImageIndex < 0 || count <= 1) return;
  // Wrap around: next from the last goes to the first, back from the first to the last.
  currentImageIndex = ((currentImageIndex + delta) % count + count) % count;
  std::string dirPath = FsHelpers::extractFolderPath(filePath);
  if (dirPath.empty() || dirPath.back() != '/') dirPath += "/";
  filePath = dirPath + siblingImages[currentImageIndex];
  showCurrent();
}

void BmpViewerActivity::rotate() {
  viewTurns = (viewTurns + 1) % 4;
  showCurrent();
}

// Long taps turn only the picture: it is drawn in the reading orientation turned by them, clockwise. When the global
// reading orientation changes (control center), the picture follows it and keeps its own turn.
GfxRenderer::Orientation BmpViewerActivity::drawOrientation() const {
  const auto reading = holdOrientation();
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (ROTATION_ORDER[i] == reading) index = i;
  }
  return ROTATION_ORDER[(index + viewTurns) % 4];
}

// Touches, swipes, the control center and messages: the global reading orientation, never the long-tap turns.
GfxRenderer::Orientation BmpViewerActivity::holdOrientation() const {
  switch (SETTINGS.orientation) {
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      return GfxRenderer::Orientation::LandscapeClockwise;
    case CrossPointSettings::ORIENTATION::INVERTED:
      return GfxRenderer::Orientation::PortraitInverted;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      return GfxRenderer::Orientation::LandscapeCounterClockwise;
    default:
      return GfxRenderer::Orientation::Portrait;
  }
}

bool BmpViewerActivity::saveJpegSleepCover() {
  if (Storage.exists(JPEG_SLEEP_TMP_PXC) && !Storage.remove(JPEG_SLEEP_TMP_PXC)) {
    LOG_ERR("BMP", "Failed to remove previous sleep image pixels");
    return false;
  }
  bool keepBmpTemp = false;
  const ScopedCleanup cleanup{[&keepBmpTemp]() {
    Storage.remove(JPEG_SLEEP_TMP_PXC);
    if (!keepBmpTemp) Storage.remove(CUSTOM_SLEEP_TMP_BMP);
  }};

  JpegToFramebufferConverter decoder;
  ImageDimensions dimensions;
  if (!decoder.getDimensions(filePath, dimensions)) return false;
  auto config = fitImage(dimensions, renderer);
  if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP &&
      (dimensions.width > renderer.getScreenWidth() || dimensions.height > renderer.getScreenHeight())) {
    const float ratio = static_cast<float>(dimensions.width) / dimensions.height;
    const float screenRatio = static_cast<float>(renderer.getScreenWidth()) / renderer.getScreenHeight();
    config.x = config.y = 0;
    config.maxWidth = renderer.getScreenWidth();
    config.maxHeight = renderer.getScreenHeight();
    if (ratio > screenRatio) {
      config.sourceCropX = 1.0f - screenRatio / ratio;
    } else {
      config.sourceCropY = 1.0f - ratio / screenRatio;
    }
  }
  config.cachePath = JPEG_SLEEP_TMP_PXC;
  renderer.clearScreen();
  bool cacheWritten;
  if (!decoder.decodeToFramebuffer(filePath, renderer, config, cacheWritten) || !cacheWritten) {
    LOG_ERR("BMP", "Failed to decode sleep image pixels");
    return false;
  }
  if (!pixelCacheToBmp(JPEG_SLEEP_TMP_PXC, CUSTOM_SLEEP_TMP_BMP, config.maxWidth, config.maxHeight)) return false;
  if (!Storage.replaceFile(CUSTOM_SLEEP_TMP_BMP, CUSTOM_SLEEP_ROOT_BMP)) {
    // ponytail: FAT replacement is not atomic; keep the complete temp for recovery.
    keepBmpTemp = true;
    LOG_ERR("BMP", "Failed to publish sleep image");
    return false;
  }
  return true;
}

void BmpViewerActivity::doSetSleepCover() {
  display.setInverted(SETTINGS.screenInverted != 0);
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen(0xFF);
  GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));

  const bool jpeg = FsHelpers::hasJpgExtension(filePath);
  const bool transparentMode = !jpeg && overlaySleepMode();
  if (!canSetSleepCover()) return;

  const char* destination =
      transparentMode ? (FsHelpers::hasPngExtension(filePath) ? TRANSPARENT_SLEEP_ROOT_PNG : TRANSPARENT_SLEEP_ROOT_BMP)
                      : CUSTOM_SLEEP_ROOT_BMP;
  bool success = filePath == destination;

  if (jpeg) {
    // Own sleep images draw in the reading orientation (SleepActivity::renderCustomSleepScreen): prepare the cover in
    // it, not in portrait, so it lands 1:1.
    const auto viewOrientation = renderer.getOrientation();
    ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
    success = saveJpegSleepCover();
    renderer.setOrientation(viewOrientation);
  } else if (!success) {
    auto buffer = makeUniqueNoThrow<uint8_t[]>(COPY_BUFFER_SIZE);
    if (!buffer) {
      LOG_ERR("BMP", "OOM: sleep cover copy buffer");
    } else {
      // Copy beside the target and swap in only a complete image, so a failed
      // copy keeps the previous sleep cover instead of a truncated one.
      const std::string tmp = std::string(destination) + ".tmp";
      HalFile inFile, outFile;
      if (Storage.openFileForRead("BMP", filePath, inFile) && Storage.openFileForWrite("BMP", tmp, outFile)) {
        int bytesRead;
        success = true;
        while ((bytesRead = inFile.read(buffer.get(), COPY_BUFFER_SIZE)) > 0) {
          if (outFile.write(buffer.get(), static_cast<size_t>(bytesRead)) != static_cast<size_t>(bytesRead)) {
            success = false;
            break;
          }
        }
        if (bytesRead < 0) success = false;
        outFile.close();
        success = success && Storage.replaceFile(tmp.c_str(), destination);
        if (!success) Storage.remove(tmp.c_str());
      }
    }
  }

  if (success) {
    if (!transparentMode) {
      SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM;
      // The sleep folders take priority over /sleep.bmp: «One Image» makes the cover show; In Order / Random in
      // Settings → Display bring the folder rotation back (the folders are not touched).
      SETTINGS.sleepImageOrder = CrossPointSettings::SLEEP_ORDER_SINGLE;
    }
    SETTINGS.saveToFile();
    renderer.clearScreen(0xFF);
    GUI.drawPopup(renderer, transparentMode ? tr(STR_DONE) : tr(STR_SLEEP_COVER_SET_SINGLE));
  } else {
    renderer.clearScreen(0xFF);
    GUI.drawPopup(renderer, tr(STR_FAILED_LOWER));
  }

  delay(1000);
  onEnter();
}

void BmpViewerActivity::loop() {
  // Keep CPU awake/polling so 1st click works
  Activity::loop();

  // Back from the control center (or any sheet over the image): paint again — its settings (reading orientation,
  // night mode, touch) may have changed.
  if (activityManager.visit() != paintedVisit) {
    showCurrent();
    return;
  }

#if FREEINK_DEVICE_X4PRO
  // Settled redraw (X4 Pro, Nikolai 04.11.2026): whatever was drawn seconds before the gray image (the file
  // browser's header line) prints into it and drains in minutes. Two minutes after the paint, repaint it once — its
  // own path cleans to white first (the driver's base pass) — and the image comes back band-free.
  if (SETTINGS.imageSettledRedraw && settledRedrawPending && grayglass::imageOnGlass &&
      millis() - paintedAtMs >= 120000UL) {
    LOG_DBG("BMP", "Settled redraw after 2 min");
    showCurrent();
    settledRedrawPending = false;
    return;
  }
#endif

  // As on the book reading surface: no left-edge swipe-to-exit here — in swipe mode a right swipe pages back. The
  // physical Back button exits; the Home gesture/key stays available.
  if (!mappedInput.wasBackGesture() && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    renderer.setOrientation(savedOrientation);
    activityManager.goToFileBrowser(filePath);
    return;
  }

  // Touch follows the book reader's settings: the touch master switch (also in the quick panel), and per direction
  // tap / swipe / both / inverted tap / off. Touch off — physical buttons only. A long tap (touch on) rotates.
  if (SETTINGS.touchReaderControls && mappedInput.hasTouch()) {
    int touchX = 0;
    int touchY = 0;
    if (mappedInput.wasScreenLongPress(touchX, touchY)) {
      LOG_DBG("BMP", "Input: long tap -> rotate");
      rotate();
      return;
    }
    const auto touch = ReaderUtils::detectTouchPageTurn(renderer, mappedInput);
    if (touch.next || touch.prev) {
      LOG_DBG("BMP", "Input: touch %s", touch.next ? "next" : "prev");
      openSibling(touch.next ? 1 : -1);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!canSetSleepCover()) return;

    auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_SET_SLEEP_COVER_CONFIRM),
                                                                filePath.substr(filePath.find_last_of('/') + 1));
    if (!confirmation) {
      LOG_ERR("BMP", "OOM: sleep cover confirmation (%zu bytes)", sizeof(ConfirmationActivity));
      return;
    }
    startActivityForResult(std::move(confirmation), [this](const ActivityResult& result) {
      if (result.isCancelled) {
        onEnter();
        return;
      }
      doSetSleepCover();
    });
    return;
  }

  // Buttons as in a book: front Left/Right (swap setting), side page buttons (their layout), power-button and tilt
  // page turns when enabled.
  const auto turn = ReaderUtils::detectPageTurn(mappedInput);
  if (turn.next || turn.prev) {
    LOG_DBG("BMP", "Input: button %s", turn.next ? "next" : "prev");
    openSibling(turn.next ? 1 : -1);
    return;
  }
}
