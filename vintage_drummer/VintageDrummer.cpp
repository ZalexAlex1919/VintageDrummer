#include "VintageDrummer.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include "resources/resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace
{
constexpr int kNumDrumMappings = 10;
constexpr int kStereoSpread = 23;
constexpr int kClosedHatSampleIndex = 3;
constexpr int kOpenHatSampleIndex = 9;
constexpr float kGlobalSampleBoost = 4.0f;
constexpr int kCtrlTagDrumKnobStart = 1000;
constexpr int kMsgTagDrumKnobFlash = 1;
constexpr float kDrumKnobFlashTimeSeconds = 0.3f;
constexpr float kGrooveRowHeight = 30.0f;
constexpr const char* kGroovePanelGroup = "groove_panel_group";

constexpr std::array<int, kNumDrumMappings> kMidiNotes {
  24, // C1 - kick
  26, // D1 - snare
  27, // D#1 - side stick snare
  28, // E1 - closed hat
  29, // F1 - floor tom
  30, // F#1 - low tom
  31, // G1 - mid tom
  32, // G#1 - hi tom
  33, // A1 - crash
  34  // A#1 - open hat
};

constexpr std::array<const char*, kNumDrumMappings> kSampleFileNames {
  "kick.wav",
  "snare.wav",
  "side_stick.wav",
  "closed_hi_hat.wav",
  "floor_tom.wav",
  "low_tom.wav",
  "Mid_tom.wav",
  "hi_tom.wav",
  "crash.wav",
  "open_hi_hat.wav"
};

constexpr std::array<int, kNumDrumMappings> kSampleResourceIDs {
  IDR_WAV_KICK,
  IDR_WAV_SNARE,
  IDR_WAV_SIDE_STICK_SNARE,
  IDR_WAV_CLOSED_HAT,
  IDR_WAV_FLOOR_TOM,
  IDR_WAV_LOW_TOM,
  IDR_WAV_MID_TOM,
  IDR_WAV_HI_TOM,
  IDR_WAV_CRASH,
  IDR_WAV_OPEN_HAT
};

constexpr std::array<const char*, kNumDrumMappings> kDrumLabels {
  "Kick",
  "Snare",
  "Side Stick",
  "Closed Hat",
  "Floor Tom",
  "Low Tom",
  "Mid Tom",
  "Hi Tom",
  "Crash",
  "Open Hat"
};

// Per-drum trims: closed hat + all toms slightly quieter.
constexpr std::array<float, kNumDrumMappings> kSampleTrim {
  1.00f, // kick
  1.00f, // snare
  0.50f, // side stick
  0.90f, // closed hat
  0.88f, // floor tom
  0.88f, // low tom
  0.88f, // mid tom
  0.88f, // hi tom
  1.00f, // crash
  1.00f  // open hat
};

struct EmbeddedGrooveResource
{
  int resourceID;
  const char* fileName;
};

constexpr std::array<EmbeddedGrooveResource, 7> kEmbeddedGrooveResources {{
  {IDR_MID_GROOVE1, "groove1.mid"},
  {IDR_MID_GROOVE2, "groove2.mid"},
  {IDR_MID_GROOVE3, "groove3.mid"},
  {IDR_MID_GROOVE4, "groove4.mid"},
  {IDR_MID_GROOVE5, "groove5.mid"},
  {IDR_MID_GROOVE6, "groove6.mid"},
  {IDR_MID_GROOVE7, "groove7.mid"}
}};

std::string ToLowerASCII(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return str;
}

bool IsMidiFile(const std::filesystem::path& filePath)
{
  return ToLowerASCII(filePath.extension().string()) == ".mid";
}

#if IPLUG_EDITOR
class FlashingDrumKnobControl final : public IVKnobControl
{
public:
  FlashingDrumKnobControl(const IRECT& bounds, int paramIdx, const IVStyle& style)
  : IVKnobControl(bounds, paramIdx, "", style)
  {}

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (msgTag == kMsgTagDrumKnobFlash)
    {
      mFlashAmount = 1.0f;
      SetAnimation([this](IControl* pCaller) {
        const double progress = pCaller->GetAnimationProgress();
        if (progress >= 1.0)
        {
          mFlashAmount = 0.0f;
          pCaller->OnEndAnimation();
          return;
        }

        mFlashAmount = static_cast<float>(1.0 - progress);
        pCaller->SetDirty(false);
      }, static_cast<int>(kDrumKnobFlashTimeSeconds * 1000.0f));
      SetDirty(false);
      return;
    }

    IVKnobControl::OnMsgFromDelegate(msgTag, dataSize, pData);
  }

  void DrawWidget(IGraphics& g) override
  {
    IVKnobControl::DrawWidget(g);

    if (mFlashAmount <= 0.0f)
      return;

    const float widgetRadius = GetRadius();
    const float cx = mWidgetBounds.MW();
    const float cy = mWidgetBounds.MH();
    const IRECT knobHandleBounds = mWidgetBounds.GetCentredInside((widgetRadius - mTrackToHandleDistance) * 2.f);
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    const float pointerRadius = knobHandleBounds.W() * 0.5f;
    const int alpha = static_cast<int>(std::lround(std::clamp(mFlashAmount, 0.0f, 1.0f) * 255.0f));
    const IColor flashColor(alpha, 255, 255, 255);

    g.DrawArc(flashColor, cx, cy, widgetRadius, mAngle1, mAngle2, &mBlend, 2.0f);
    g.FillEllipse(flashColor, knobHandleBounds, &mBlend);
    g.DrawRadialLine(flashColor, cx, cy, angle, mInnerPointerFrac * pointerRadius, mOuterPointerFrac * pointerRadius, &mBlend, mPointerThickness);
  }

private:
  float mFlashAmount = 0.0f;
};

class GrooveFileButtonControl final : public IVButtonControl
{
public:
  GrooveFileButtonControl(const IRECT& bounds, const char* label, std::string filePath, const IVStyle& style)
  : IVButtonControl(bounds, EmptyClickActionFunc, label, style, true, false, EVShape::Rectangle)
  , mFilePath(std::move(filePath))
  {}

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mDragTriggered = false;
    mDragStartX = x;
    mDragStartY = y;
    IVButtonControl::OnMouseDown(x, y, mod);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    (void) dX;
    (void) dY;
    (void) mod;

    if (mDragTriggered || mFilePath.empty())
      return;

    const float dx = x - mDragStartX;
    const float dy = y - mDragStartY;
    const float dragDistance = std::sqrt((dx * dx) + (dy * dy));
    if (dragDistance < 4.0f)
      return;

    if (GetUI())
      mDragTriggered = GetUI()->InitiateExternalFileDragDrop(mFilePath.c_str(), GetRECT());

    SetValue(0.0);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    SetValue(0.0);
    SetDirty(false);
  }

private:
  std::string mFilePath;
  float mDragStartX = 0.0f;
  float mDragStartY = 0.0f;
  bool mDragTriggered = false;
};

class FittedBitmapControl final : public IControl
{
public:
  FittedBitmapControl(const IRECT& bounds, const IBitmap& bitmap)
  : IControl(bounds)
  , mBitmap(bitmap)
  {}

  void Draw(IGraphics& g) override
  {
    g.DrawFittedBitmap(mBitmap, mRECT, &mBlend);
  }

private:
  IBitmap mBitmap;
};
#endif

uint16_t ReadLE16(const uint8_t* p)
{
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t ReadLE32(const uint8_t* p)
{
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

float DecodeWaveSample(const uint8_t* pData, uint16_t format, uint16_t bitsPerSample)
{
  if (format == 1) // PCM
  {
    switch (bitsPerSample)
    {
      case 8:
      {
        const float v = static_cast<float>(*pData);
        return (v - 128.0f) / 128.0f;
      }
      case 16:
      {
        const int16_t v = static_cast<int16_t>(ReadLE16(pData));
        return static_cast<float>(v) / 32768.0f;
      }
      case 24:
      {
        int32_t v = (static_cast<int32_t>(pData[0])) |
                    (static_cast<int32_t>(pData[1]) << 8) |
                    (static_cast<int32_t>(pData[2]) << 16);
        if (v & 0x00800000)
          v |= 0xFF000000;
        return static_cast<float>(v) / 8388608.0f;
      }
      case 32:
      {
        const int32_t v = static_cast<int32_t>(ReadLE32(pData));
        return static_cast<float>(v / 2147483648.0);
      }
      default:
        break;
    }
  }
  else if (format == 3) // IEEE float
  {
    if (bitsPerSample == 32)
    {
      float v = 0.0f;
      std::memcpy(&v, pData, sizeof(float));
      return v;
    }

    if (bitsPerSample == 64)
    {
      double v = 0.0;
      std::memcpy(&v, pData, sizeof(double));
      return static_cast<float>(v);
    }
  }

  return 0.0f;
}

std::filesystem::path UTF8ToPath(const char* utf8)
{
#if defined(OS_WIN)
  if (!utf8 || !utf8[0])
    return {};

  const int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (wideLen <= 1)
    return {};

  std::wstring wide(static_cast<size_t>(wideLen), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wideLen);
  return std::filesystem::path(wide.c_str());
#else
  return std::filesystem::path(utf8 ? utf8 : "");
#endif
}

std::string PathToUTF8(const std::filesystem::path& path)
{
#if defined(OS_WIN)
  const std::wstring wide = path.wstring();
  if (wide.empty())
    return {};

  const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8Len <= 1)
    return {};

  std::string utf8(static_cast<size_t>(utf8Len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);
  utf8.resize(static_cast<size_t>(utf8Len - 1));
  return utf8;
#else
  return path.string();
#endif
}

bool FileExists(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

bool DirExists(const std::filesystem::path& path)
{
  std::error_code ec;
  return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

#if defined(OS_WIN)
bool GetWinResourceData(int resourceID, const wchar_t* resourceType, const void*& pData, int& sizeInBytes)
{
  pData = nullptr;
  sizeInBytes = 0;

  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&UTF8ToPath),
                          &module))
  {
    return false;
  }

  HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceID), resourceType);
  if (!resource)
    return false;

  const DWORD dataSize = SizeofResource(module, resource);
  if (dataSize == 0)
    return false;

  HGLOBAL loadedResource = LoadResource(module, resource);
  if (!loadedResource)
    return false;

  const void* resourceData = LockResource(loadedResource);
  if (!resourceData)
    return false;

  pData = resourceData;
  sizeInBytes = static_cast<int>(dataSize);
  return true;
}
#endif

} // namespace

struct VintageDrummer::ReverbState
{
  struct Comb
  {
    std::vector<float> buffer;
    size_t index = 0;
    float feedback = 0.0f;
    float filterStore = 0.0f;
    float damp1 = 0.0f;
    float damp2 = 1.0f;

    void Resize(size_t size)
    {
      buffer.assign(size, 0.0f);
      index = 0;
      filterStore = 0.0f;
    }

    void SetFeedback(float fb)
    {
      feedback = fb;
    }

    void SetDamp(float damp)
    {
      damp1 = damp;
      damp2 = 1.0f - damp;
    }

    float Process(float input)
    {
      if (buffer.empty())
        return 0.0f;

      const float output = buffer[index];
      filterStore = (output * damp2) + (filterStore * damp1);
      buffer[index] = input + (filterStore * feedback);

      ++index;
      if (index >= buffer.size())
        index = 0;

      return output;
    }
  };

  struct Allpass
  {
    std::vector<float> buffer;
    size_t index = 0;
    float feedback = 0.5f;

    void Resize(size_t size)
    {
      buffer.assign(size, 0.0f);
      index = 0;
    }

    float Process(float input)
    {
      if (buffer.empty())
        return 0.0f;

      const float bufOut = buffer[index];
      const float output = -input + bufOut;
      buffer[index] = input + (bufOut * feedback);

      ++index;
      if (index >= buffer.size())
        index = 0;

      return output;
    }
  };

  std::array<Comb, 8> combL;
  std::array<Comb, 8> combR;
  std::array<Allpass, 4> allpassL;
  std::array<Allpass, 4> allpassR;

  float roomSize = 0.5f;
  float damp = 0.2f;
  float wet = 0.0f;
  float width = 1.0f;

  float wet1 = 0.0f;
  float wet2 = 0.0f;

  void Init(double sampleRate)
  {
    static constexpr std::array<int, 8> kCombTuningL {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr std::array<int, 4> kAllpassTuningL {556, 441, 341, 225};

    const float scale = static_cast<float>(sampleRate / 44100.0);

    for (size_t i = 0; i < combL.size(); ++i)
    {
      const int lenL = std::max(1, static_cast<int>(std::lround(kCombTuningL[i] * scale)));
      const int lenR = std::max(1, static_cast<int>(std::lround((kCombTuningL[i] + kStereoSpread) * scale)));
      combL[i].Resize(static_cast<size_t>(lenL));
      combR[i].Resize(static_cast<size_t>(lenR));
    }

    for (size_t i = 0; i < allpassL.size(); ++i)
    {
      const int lenL = std::max(1, static_cast<int>(std::lround(kAllpassTuningL[i] * scale)));
      const int lenR = std::max(1, static_cast<int>(std::lround((kAllpassTuningL[i] + kStereoSpread) * scale)));
      allpassL[i].Resize(static_cast<size_t>(lenL));
      allpassR[i].Resize(static_cast<size_t>(lenR));
    }

    UpdateDamping();
    UpdateRoomSize();
    UpdateWet();
  }

  void SetRoomAmount(float amount)
  {
    amount = std::clamp(amount, 0.0f, 1.0f);
    roomSize = 0.2f + amount * 0.75f;
    damp = 0.1f + amount * 0.4f;
    wet = amount * 0.5f;

    UpdateDamping();
    UpdateRoomSize();
    UpdateWet();
  }

  void Process(float inL, float inR, float& outL, float& outR)
  {
    const float input = (inL + inR) * 0.015f;

    float accL = 0.0f;
    float accR = 0.0f;

    for (size_t i = 0; i < combL.size(); ++i)
    {
      accL += combL[i].Process(input);
      accR += combR[i].Process(input);
    }

    for (size_t i = 0; i < allpassL.size(); ++i)
    {
      accL = allpassL[i].Process(accL);
      accR = allpassR[i].Process(accR);
    }

    outL = accL * wet1 + accR * wet2;
    outR = accR * wet1 + accL * wet2;
  }

private:
  void UpdateDamping()
  {
    for (size_t i = 0; i < combL.size(); ++i)
    {
      combL[i].SetDamp(damp);
      combR[i].SetDamp(damp);
    }
  }

  void UpdateRoomSize()
  {
    for (size_t i = 0; i < combL.size(); ++i)
    {
      combL[i].SetFeedback(roomSize);
      combR[i].SetFeedback(roomSize);
    }
  }

  void UpdateWet()
  {
    wet1 = wet * (width * 0.5f + 0.5f);
    wet2 = wet * ((1.0f - width) * 0.5f);
  }
};

VintageDrummer::VintageDrummer(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
, mReverb(std::make_unique<ReverbState>())
{
  GetParam(kParamRoom)->InitDouble("Room", 0.0, 0.0, 100.0, 0.1, "%");
  GetParam(kParamMasterVolume)->InitDouble("Master Volume", 50.0, 0.0, 100.0, 0.1, "%");
  GetParam(kParamHiHatChoke)->InitBool("Hi-Hat Choke", true, "", 0, "", "Off", "On");
  for (int i = 0; i < kNumDrumMappings; ++i)
    GetParam(kParamDrumVolStart + i)->InitDouble(kDrumLabels[i], 100.0, 0.0, 200.0, 0.1, "%");

  ResolveAssetPaths();
  ScanMidiGrooves();
  LoadAllSamples();

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    constexpr int kUIWidth = 980;
    constexpr int kUIHeight = 620;
    return MakeGraphics(*this, kUIWidth, kUIHeight, 60, GetScaleForScreen(kUIWidth, kUIHeight));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    if (pGraphics->NControls())
      return;

    ScanMidiGrooves();

    pGraphics->EnableMouseOver(true);
    pGraphics->EnableMultiTouch(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    bool backgroundAttached = false;
#if defined(OS_WIN)
    const void* bgData = nullptr;
    int bgSize = 0;
    if (GetWinResourceData(IDR_PNG_BG, MAKEINTRESOURCEW(10), bgData, bgSize))
    {
      const IBitmap bgBitmap = pGraphics->LoadBitmap("bg_embedded.png", bgData, bgSize);
      if (bgBitmap.W() > 0 && bgBitmap.H() > 0)
      {
        pGraphics->AttachControl(new IBitmapControl(pGraphics->GetBounds(), bgBitmap));
        backgroundAttached = true;
      }
    }
#endif
    if (!backgroundAttached)
    {
      if (!mBackgroundImagePath.empty() && FileExists(mBackgroundImagePath))
      {
        const std::string bgPath = PathToUTF8(mBackgroundImagePath);
        if (!bgPath.empty())
        {
          pGraphics->AttachBackground(bgPath.c_str());
          backgroundAttached = true;
        }
      }
      if (!backgroundAttached)
        pGraphics->AttachBackground(BG_FN);
    }

    bool logoAttached = false;
#if defined(OS_WIN)
    const void* logoData = nullptr;
    int logoSize = 0;
    if (GetWinResourceData(IDR_PNG_LOGO, MAKEINTRESOURCEW(10), logoData, logoSize))
    {
      const IBitmap logoBitmap = pGraphics->LoadBitmap("logo_embedded.png", logoData, logoSize);
      if (logoBitmap.W() > 0 && logoBitmap.H() > 0)
      {
        const IRECT uiBounds = pGraphics->GetBounds();
        const float maxLogoW = 420.f;
        const float maxLogoH = 180.f;
        const float logoScale = std::min(maxLogoW / static_cast<float>(logoBitmap.W()),
                                         maxLogoH / static_cast<float>(logoBitmap.H()));
        const float drawW = static_cast<float>(logoBitmap.W()) * logoScale;
        const float drawH = static_cast<float>(logoBitmap.H()) * logoScale;
        const IRECT logoRect(uiBounds.L, uiBounds.T, uiBounds.L + drawW, uiBounds.T + drawH);
        pGraphics->AttachControl(new FittedBitmapControl(logoRect, logoBitmap));
        logoAttached = true;
      }
    }
#endif

    if (!logoAttached && !mLogoImagePath.empty() && FileExists(mLogoImagePath))
    {
      const std::string logoPath = PathToUTF8(mLogoImagePath);
      if (!logoPath.empty())
      {
        const IBitmap logoBitmap = pGraphics->LoadBitmap(logoPath.c_str());
        if (logoBitmap.W() > 0 && logoBitmap.H() > 0)
        {
          const IRECT uiBounds = pGraphics->GetBounds();
          const float maxLogoW = 420.f;
          const float maxLogoH = 180.f;
          const float logoScale = std::min(maxLogoW / static_cast<float>(logoBitmap.W()),
                                           maxLogoH / static_cast<float>(logoBitmap.H()));
          const float drawW = static_cast<float>(logoBitmap.W()) * logoScale;
          const float drawH = static_cast<float>(logoBitmap.H()) * logoScale;
          const IRECT logoRect(uiBounds.L, uiBounds.T, uiBounds.L + drawW, uiBounds.T + drawH);
          pGraphics->AttachControl(new FittedBitmapControl(logoRect, logoBitmap));
          logoAttached = true;
        }
      }
    }

    const IText sliderLabelText {16.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
    const IText sliderValueText {15.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
    const IText drumLabelText {14.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};

    const IVStyle sliderStyle = DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE)
                                             .WithColor(kPR, COLOR_WHITE)
                                             .WithColor(kHL, COLOR_WHITE)
                                             .WithColor(kBG, IColor(70, 0, 0, 0))
                                             .WithColor(kSH, IColor(120, 0, 0, 0))
                                             .WithRoundness(0.35f)
                                             .WithWidgetFrac(0.85f)
                                             .WithLabelText(sliderLabelText)
                                             .WithValueText(sliderValueText);

    const IText chokeValueText {16.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
    const IVStyle chokeStyle = DEFAULT_STYLE.WithColor(kFG, IColor(220, 50, 50, 50))
                                            .WithColor(kPR, IColor(240, 58, 150, 80))
                                            .WithColor(kHL, IColor(96, 255, 255, 255))
                                            .WithColor(kBG, COLOR_TRANSPARENT)
                                            .WithColor(kFR, IColor(200, 255, 255, 255))
                                            .WithColor(kSH, COLOR_TRANSPARENT)
                                            .WithRoundness(0.20f)
                                            .WithWidgetFrac(0.72f)
                                            .WithDrawShadows(false)
                                            .WithDrawFrame(true)
                                            .WithShowLabel(true)
                                            .WithShowValue(true)
                                            .WithLabelText(sliderLabelText)
                                            .WithValueText(chokeValueText);

    const IVStyle knobStyle = DEFAULT_STYLE.WithColor(kFG, COLOR_WHITE)
                                           .WithColor(kPR, IColor(255, 224, 224, 224))
                                           .WithColor(kHL, COLOR_WHITE)
                                           .WithColor(kBG, COLOR_TRANSPARENT)
                                           .WithColor(kSH, IColor(90, 0, 0, 0))
                                           .WithRoundness(0.25f)
                                           .WithWidgetFrac(0.96f)
                                           .WithShowLabel(false)
                                           .WithShowValue(false);

    const IText grooveTitleText {16.f, COLOR_WHITE, "Roboto-Regular", EAlign::Near, EVAlign::Middle};
    const IText grooveButtonText {15.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
    const IVStyle grooveButtonStyle = DEFAULT_STYLE.WithColor(kFG, IColor(240, 24, 24, 24))
                                                   .WithColor(kPR, IColor(220, 255, 255, 255))
                                                   .WithColor(kHL, IColor(96, 255, 255, 255))
                                                   .WithColor(kFR, IColor(220, 255, 255, 255))
                                                   .WithColor(kSH, IColor(120, 0, 0, 0))
                                                   .WithRoundness(0.20f)
                                                   .WithDrawFrame(true)
                                                   .WithLabelText(grooveButtonText)
                                                   .WithValueText(grooveButtonText);
    const IVStyle groovePanelStyle = DEFAULT_STYLE.WithColor(kFG, IColor(170, 28, 28, 28))
                                                  .WithColor(kSH, IColor(120, 0, 0, 0))
                                                  .WithColor(kFR, IColor(180, 255, 255, 255))
                                                  .WithRoundness(0.08f)
                                                  .WithDrawFrame(true)
                                                  .WithShowLabel(false)
                                                  .WithShowValue(false);
    const IVStyle grooveItemStyle = DEFAULT_STYLE.WithColor(kFG, IColor(220, 40, 40, 40))
                                                 .WithColor(kPR, IColor(230, 255, 255, 255))
                                                 .WithColor(kHL, IColor(96, 255, 255, 255))
                                                 .WithColor(kFR, IColor(170, 255, 255, 255))
                                                 .WithColor(kSH, IColor(100, 0, 0, 0))
                                                 .WithRoundness(0.16f)
                                                 .WithDrawFrame(true)
                                                 .WithLabelText(grooveButtonText)
                                                 .WithShowValue(false);

    const IRECT bounds = pGraphics->GetBounds().GetPadded(-24.f);
    const IRECT groovesButtonRect = bounds.GetFromTop(42.f).GetFromRight(146.f).GetPadded(-4.f);
    const float groovePanelTop = groovesButtonRect.B + 8.f;
    const float groovePanelBottom = std::min(bounds.B - 8.f, groovePanelTop + 300.f);
    const IRECT groovePanelRect(groovesButtonRect.R - 290.f, groovePanelTop, groovesButtonRect.R, groovePanelBottom);

    pGraphics->AttachControl(new IVButtonControl(groovesButtonRect, [this](IControl* pCaller) {
      mGroovesPanelVisible = !mGroovesPanelVisible;

      if (auto* pUI = pCaller->GetUI())
      {
        pUI->ForControlInGroup(kGroovePanelGroup, [this](IControl* pControl) {
          pControl->Hide(!mGroovesPanelVisible);
        });
        pUI->SetAllControlsDirty();
      }

      pCaller->SetAnimation(DefaultAnimationFunc, 120);
    }, "Grooves", grooveButtonStyle, true, false, EVShape::Rectangle));

    pGraphics->AttachControl(new IVPanelControl(groovePanelRect, "", groovePanelStyle), kNoTag, kGroovePanelGroup);

    const IRECT groovePanelContent = groovePanelRect.GetPadded(-10.f);
    const IRECT grooveTitleRect = groovePanelContent.GetFromTop(26.f);
    pGraphics->AttachControl(new ITextControl(grooveTitleRect, "MIDI Grooves", grooveTitleText, COLOR_TRANSPARENT),
                             kNoTag,
                             kGroovePanelGroup);

    const IRECT grooveListRect(groovePanelContent.L, grooveTitleRect.B + 6.f, groovePanelContent.R, groovePanelContent.B);
    const int maxVisibleGrooves = std::max(1, static_cast<int>(std::floor(grooveListRect.H() / kGrooveRowHeight)));
    const bool useEmbeddedGrooves = !mEmbeddedGrooves.empty();
    const size_t grooveCount = useEmbeddedGrooves ? mEmbeddedGrooves.size() : mGrooveFiles.size();
    const int groovesToDraw = std::min(maxVisibleGrooves, static_cast<int>(grooveCount));

    if (groovesToDraw == 0)
    {
      const IText grooveEmptyText {14.f, COLOR_WHITE, "Roboto-Regular", EAlign::Center, EVAlign::Middle};
      pGraphics->AttachControl(new ITextControl(grooveListRect.GetPadded(-4.f), "No embedded .mid files found", grooveEmptyText, COLOR_TRANSPARENT),
                               kNoTag,
                               kGroovePanelGroup);
    }
    else
    {
      for (int i = 0; i < groovesToDraw; ++i)
      {
        const float top = grooveListRect.T + (static_cast<float>(i) * kGrooveRowHeight);
        const IRECT rowRect(grooveListRect.L, top, grooveListRect.R, top + kGrooveRowHeight - 3.f);
        std::string grooveLabel;
        std::string groovePath;
        if (useEmbeddedGrooves)
        {
          grooveLabel = mEmbeddedGrooves[static_cast<size_t>(i)].name;
          groovePath = PathToUTF8(mEmbeddedGrooves[static_cast<size_t>(i)].tempFilePath);
        }
        else
        {
          grooveLabel = mGrooveFiles[static_cast<size_t>(i)].stem().string();
          groovePath = PathToUTF8(mGrooveFiles[static_cast<size_t>(i)]);
        }

        pGraphics->AttachControl(new GrooveFileButtonControl(rowRect, grooveLabel.c_str(), groovePath, grooveItemStyle),
                                 kNoTag,
                                 kGroovePanelGroup);
      }

      if (groovesToDraw < static_cast<int>(grooveCount))
      {
        const int hiddenCount = static_cast<int>(grooveCount) - groovesToDraw;
        WDL_String moreText;
        moreText.SetFormatted(32, "+%d more...", hiddenCount);
        const IText moreLabelText {13.f, COLOR_WHITE, "Roboto-Regular", EAlign::Far, EVAlign::Bottom};
        pGraphics->AttachControl(new ITextControl(grooveListRect.GetPadded(-4.f), moreText.Get(), moreLabelText, COLOR_TRANSPARENT),
                                 kNoTag,
                                 kGroovePanelGroup);
      }
    }

    pGraphics->ForControlInGroup(kGroovePanelGroup, [this](IControl* pControl) {
      pControl->Hide(!mGroovesPanelVisible);
    });

    constexpr float kBottomPanelHeight = 205.f;
    constexpr float kRightPanelWidth = 215.f;
    constexpr float kSectionGap = 12.f;
    constexpr float kLabelHeight = 24.f;

    const IRECT bottomPanel = bounds.GetFromBottom(kBottomPanelHeight);
    const IRECT sliderPanel = bottomPanel.GetFromRight(kRightPanelWidth);
    const IRECT drumPanel(bottomPanel.L, bottomPanel.T, sliderPanel.L - kSectionGap, bottomPanel.B);

    pGraphics->AttachControl(new IPanelControl(drumPanel, IColor(96, 0, 0, 0)));
    pGraphics->AttachControl(new IPanelControl(sliderPanel, IColor(112, 0, 0, 0)));

    const IRECT sliderControls = sliderPanel.GetPadded(-10.f);
    const IRECT chokeRect = sliderControls.GetFromBottom(58.f).GetPadded(-6.f);
    const IRECT fadersRect(sliderControls.L, sliderControls.T, sliderControls.R, chokeRect.T - 6.f);
    pGraphics->AttachControl(new IVSliderControl(fadersRect.GetGridCell(0, 1, 2).GetPadded(-16.f), kParamRoom, "Room", sliderStyle));
    pGraphics->AttachControl(new IVSliderControl(fadersRect.GetGridCell(1, 1, 2).GetPadded(-16.f), kParamMasterVolume, "Master", sliderStyle));
    pGraphics->AttachControl(new IVSwitchControl(chokeRect, kParamHiHatChoke, "Hi-Hat Choke", chokeStyle, true));

    const IRECT drumControls = drumPanel.GetPadded(-10.f);
    for (int i = 0; i < kNumDrumMappings; ++i)
    {
      const IRECT cell = drumControls.GetGridCell(i, 1, kNumDrumMappings).GetPadded(-3.f);
      const IRECT knobZone(cell.L, cell.T, cell.R, cell.B - kLabelHeight);
      const float knobSize = std::max(28.f, std::min(knobZone.W(), knobZone.H()) - 6.f);
      const IRECT knobRect = knobZone.GetCentredInside(knobSize);
      const IRECT labelRect(cell.L, cell.B - kLabelHeight, cell.R, cell.B);

      pGraphics->AttachControl(new FlashingDrumKnobControl(knobRect, kParamDrumVolStart + i, knobStyle),
                               kCtrlTagDrumKnobStart + i);
      pGraphics->AttachControl(new ITextControl(labelRect, kDrumLabels[i], drumLabelText, COLOR_TRANSPARENT));
    }
  };
#endif

  OnParamChange(kParamRoom);
  OnParamChange(kParamMasterVolume);
  OnParamChange(kParamHiHatChoke);
  for (int i = 0; i < kNumDrumMappings; ++i)
    OnParamChange(kParamDrumVolStart + i);
}

void VintageDrummer::ResolveAssetPaths()
{
  namespace fs = std::filesystem;

  std::vector<fs::path> searchRoots;

#if defined(OS_WIN)
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&UTF8ToPath),
                         &module))
  {
    WDL_String pluginPath;
    iplug::PluginPath(pluginPath, module);
    if (pluginPath.GetLength() > 0)
      searchRoots.push_back(UTF8ToPath(pluginPath.Get()));
  }
#endif

  std::error_code ec;
  const fs::path cwd = fs::current_path(ec);
  if (!cwd.empty())
    searchRoots.push_back(cwd);

  searchRoots.push_back(fs::path(__FILE__).parent_path());

  const std::array<fs::path, 4> sampleDirCandidates {
    "vintage_drummer_samples",
    "..\\vintage_drummer_samples",
    "..\\..\\vintage_drummer_samples",
    "..\\..\\..\\vintage_drummer_samples"
  };

  const std::array<fs::path, 4> bgCandidates {
    "bg.png",
    "..\\bg.png",
    "..\\..\\bg.png",
    "..\\..\\..\\bg.png"
  };

  const std::array<fs::path, 4> logoCandidates {
    "logo.png",
    "..\\logo.png",
    "..\\..\\logo.png",
    "..\\..\\..\\logo.png"
  };

  const std::array<fs::path, 8> grooveDirCandidates {
    "midi_groovs",
    "midi_grooves",
    "..\\midi_groovs",
    "..\\midi_grooves",
    "..\\..\\midi_groovs",
    "..\\..\\midi_grooves",
    "..\\..\\..\\midi_groovs",
    "..\\..\\..\\midi_grooves"
  };

  for (const auto& root : searchRoots)
  {
    for (const auto& rel : sampleDirCandidates)
    {
      const fs::path candidate = root / rel;
      if (!DirExists(candidate))
        continue;

      if (FileExists(candidate / kSampleFileNames[0]))
      {
        mSampleDirectory = candidate;
        break;
      }
    }

    if (!mSampleDirectory.empty())
      break;
  }

  for (const auto& root : searchRoots)
  {
    for (const auto& rel : bgCandidates)
    {
      const fs::path candidate = root / rel;
      if (FileExists(candidate))
      {
        mBackgroundImagePath = candidate;
        break;
      }
    }

    if (!mBackgroundImagePath.empty())
      break;
  }

  for (const auto& root : searchRoots)
  {
    for (const auto& rel : logoCandidates)
    {
      const fs::path candidate = root / rel;
      if (FileExists(candidate))
      {
        mLogoImagePath = candidate;
        break;
      }
    }

    if (!mLogoImagePath.empty())
      break;
  }

  for (const auto& root : searchRoots)
  {
    for (const auto& rel : grooveDirCandidates)
    {
      const fs::path candidate = root / rel;
      if (DirExists(candidate))
      {
        mGrooveDirectory = candidate;
        break;
      }
    }

    if (!mGrooveDirectory.empty())
      break;
  }

  if (mSampleDirectory.empty())
    mSampleDirectory = fs::path("vintage_drummer_samples");

  if (mBackgroundImagePath.empty())
    mBackgroundImagePath = fs::path("bg.png");

  if (mLogoImagePath.empty())
    mLogoImagePath = fs::path("logo.png");

  if (mGrooveDirectory.empty())
    mGrooveDirectory = fs::path("midi_groovs");

  std::error_code tempEc;
  mGrooveTempDirectory = fs::temp_directory_path(tempEc) / "VintageDrummer_midi_grooves";
  if (tempEc || mGrooveTempDirectory.empty())
    mGrooveTempDirectory = fs::path("VintageDrummer_midi_grooves_temp");
}

std::vector<std::filesystem::path> VintageDrummer::FindMidiGrooveFiles(const std::filesystem::path& directory) const
{
  namespace fs = std::filesystem;
  std::vector<fs::path> grooveFiles;

  if (!DirExists(directory))
    return grooveFiles;

  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(directory, ec))
  {
    if (ec)
      break;

    if (!entry.is_regular_file())
      continue;

    const fs::path filePath = entry.path();
    if (IsMidiFile(filePath))
      grooveFiles.push_back(filePath);
  }

  std::sort(grooveFiles.begin(), grooveFiles.end(), [](const fs::path& a, const fs::path& b) {
    return ToLowerASCII(a.filename().string()) < ToLowerASCII(b.filename().string());
  });

  return grooveFiles;
}

void VintageDrummer::ScanMidiGrooves()
{
  LoadEmbeddedGrooves();
  mGrooveFiles = FindMidiGrooveFiles(mGrooveDirectory);
}

void VintageDrummer::LoadEmbeddedGrooves()
{
  mEmbeddedGrooves.clear();

#if defined(OS_WIN)
  if (mGrooveTempDirectory.empty())
    return;

  std::error_code ec;
  std::filesystem::create_directories(mGrooveTempDirectory, ec);

  for (const auto& groove : kEmbeddedGrooveResources)
  {
    const std::filesystem::path outPath = mGrooveTempDirectory / groove.fileName;
    if (!EnsureEmbeddedGrooveFile(groove.resourceID, outPath))
      continue;

    mEmbeddedGrooves.push_back({outPath.stem().string(), outPath});
  }

  std::sort(mEmbeddedGrooves.begin(), mEmbeddedGrooves.end(), [](const GrooveData& a, const GrooveData& b) {
    return ToLowerASCII(a.name) < ToLowerASCII(b.name);
  });
#endif
}

bool VintageDrummer::EnsureEmbeddedGrooveFile(int resourceID, const std::filesystem::path& destinationPath) const
{
#if defined(OS_WIN)
  if (resourceID <= 0 || destinationPath.empty())
    return false;

  const void* pData = nullptr;
  int dataSize = 0;
  if (!GetWinResourceData(resourceID, MAKEINTRESOURCEW(10), pData, dataSize))
    return false;

  if (!pData || dataSize <= 0)
    return false;

  std::error_code ec;
  std::filesystem::create_directories(destinationPath.parent_path(), ec);

  std::ofstream outFile(destinationPath, std::ios::binary | std::ios::trunc);
  if (!outFile)
    return false;

  outFile.write(static_cast<const char*>(pData), static_cast<std::streamsize>(dataSize));
  return outFile.good();
#else
  (void) resourceID;
  (void) destinationPath;
  return false;
#endif
}

bool VintageDrummer::LoadAllSamples()
{
  bool allLoaded = true;

  for (size_t i = 0; i < mSamples.size(); ++i)
  {
    SampleData data;
    const bool ok = LoadEmbeddedWav(kSampleResourceIDs[i], data) || LoadWavFile(mSampleDirectory / kSampleFileNames[i], data);

    if (ok)
      mSamples[i] = std::move(data);
    else
      allLoaded = false;
  }

  return allLoaded;
}

bool VintageDrummer::LoadEmbeddedWav(int resourceID, SampleData& outData) const
{
#if defined(OS_WIN)
  if (resourceID <= 0)
    return false;

  const void* pData = nullptr;
  int dataSize = 0;
  if (!GetWinResourceData(resourceID, MAKEINTRESOURCEW(10), pData, dataSize))
    return false;
  return LoadWavData(static_cast<const uint8_t*>(pData), static_cast<size_t>(dataSize), outData);
#else
  (void) resourceID;
  (void) outData;
  return false;
#endif
}

bool VintageDrummer::LoadWavFile(const std::filesystem::path& filePath, SampleData& outData) const
{
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file)
    return false;

  const std::streamsize size = file.tellg();
  if (size <= 0)
    return false;

  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    return false;

  return LoadWavData(bytes.data(), bytes.size(), outData);
}

bool VintageDrummer::LoadWavData(const uint8_t* bytes, size_t size, SampleData& outData) const
{
  if (!bytes || size < 12)
    return false;

  if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0)
    return false;

  uint16_t format = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint16_t blockAlign = 0;
  const uint8_t* pcmData = nullptr;
  size_t pcmDataSize = 0;

  size_t offset = 12;

  while (offset + 8 <= size)
  {
    const uint8_t* chunkID = bytes + offset;
    const uint32_t chunkSize = ReadLE32(bytes + offset + 4);
    offset += 8;

    if (offset + static_cast<size_t>(chunkSize) > size)
      return false;

    const uint8_t* chunkData = bytes + offset;

    if (std::memcmp(chunkID, "fmt ", 4) == 0)
    {
      if (chunkSize < 16)
        return false;

      format = ReadLE16(chunkData);
      channels = ReadLE16(chunkData + 2);
      sampleRate = ReadLE32(chunkData + 4);
      blockAlign = ReadLE16(chunkData + 12);
      bitsPerSample = ReadLE16(chunkData + 14);
    }
    else if (std::memcmp(chunkID, "data", 4) == 0)
    {
      pcmData = chunkData;
      pcmDataSize = static_cast<size_t>(chunkSize);
    }

    offset += static_cast<size_t>(chunkSize);
    if (chunkSize & 1U)
      ++offset;

    if (format != 0 && pcmData)
      break;
  }

  if (format == 0 || channels == 0 || sampleRate == 0 || bitsPerSample == 0 || !pcmData || pcmDataSize == 0)
    return false;

  const uint16_t bytesPerSample = static_cast<uint16_t>((bitsPerSample + 7) / 8);
  if (bytesPerSample == 0)
    return false;

  const uint16_t minBlockAlign = static_cast<uint16_t>(channels * bytesPerSample);
  if (blockAlign == 0)
    blockAlign = minBlockAlign;

  if (blockAlign < minBlockAlign)
    return false;

  const size_t frameCount = pcmDataSize / blockAlign;
  if (frameCount == 0)
    return false;

  outData.left.resize(frameCount);
  outData.right.resize(frameCount);
  outData.sampleRate = static_cast<double>(sampleRate);

  for (size_t i = 0; i < frameCount; ++i)
  {
    const uint8_t* frame = pcmData + (i * blockAlign);

    float left = DecodeWaveSample(frame, format, bitsPerSample);
    float right = left;

    if (channels > 1)
      right = DecodeWaveSample(frame + bytesPerSample, format, bitsPerSample);

    outData.left[i] = std::clamp(left, -1.0f, 1.0f);
    outData.right[i] = std::clamp(right, -1.0f, 1.0f);
  }

  return outData.IsLoaded();
}

int VintageDrummer::NoteToSampleIndex(int midiNote) const
{
  for (int i = 0; i < static_cast<int>(kMidiNotes.size()); ++i)
  {
    // plus one octave fallback for DAWs with alternate note naming
    if (midiNote == kMidiNotes[i] || midiNote == (kMidiNotes[i] + 12))
      return i;
  }

  return -1;
}

void VintageDrummer::TriggerSample(int sampleIndex, float velocity)
{
  if (sampleIndex < 0 || sampleIndex >= static_cast<int>(mSamples.size()))
    return;

  const auto& sample = mSamples[static_cast<size_t>(sampleIndex)];
  if (!sample.IsLoaded())
    return;

  if (mHiHatChoke)
  {
    int chokeTarget = -1;
    if (sampleIndex == kClosedHatSampleIndex)
      chokeTarget = kOpenHatSampleIndex;
    else if (sampleIndex == kOpenHatSampleIndex)
      chokeTarget = kClosedHatSampleIndex;

    if (chokeTarget >= 0)
    {
      for (auto& voice : mVoices)
      {
        if (voice.active && voice.sampleIndex == chokeTarget)
          voice.active = false;
      }
    }
  }

  int voiceIndex = -1;
  uint64_t oldestAge = std::numeric_limits<uint64_t>::max();

  for (int i = 0; i < static_cast<int>(mVoices.size()); ++i)
  {
    if (!mVoices[static_cast<size_t>(i)].active)
    {
      voiceIndex = i;
      break;
    }

    if (mVoices[static_cast<size_t>(i)].age < oldestAge)
    {
      oldestAge = mVoices[static_cast<size_t>(i)].age;
      voiceIndex = i;
    }
  }

  if (voiceIndex < 0)
    return;

  Voice& voice = mVoices[static_cast<size_t>(voiceIndex)];
  voice.sampleIndex = sampleIndex;
  voice.position = 0.0;
  voice.increment = sample.sampleRate / std::max(1.0, mHostSampleRate);
  const size_t drumIdx = static_cast<size_t>(sampleIndex);
  const float trim = kSampleTrim[drumIdx] * mDrumGainFactors[drumIdx] * kGlobalSampleBoost;
  voice.gain = std::clamp(velocity * trim, 0.0f, 8.0f);
  voice.age = mVoiceAgeCounter++;
  voice.active = true;

#if IPLUG_EDITOR
  SendControlMsgFromDelegate(kCtrlTagDrumKnobStart + sampleIndex, kMsgTagDrumKnobFlash);
#endif
}

#if IPLUG_DSP
void VintageDrummer::OnReset()
{
  mHostSampleRate = GetSampleRate();
  if (mHostSampleRate <= 0.0)
    mHostSampleRate = 44100.0;

  for (auto& voice : mVoices)
    voice.active = false;

  if (mReverb)
  {
    mReverb->Init(mHostSampleRate);
    mReverb->SetRoomAmount(mRoomAmount);
  }
}

void VintageDrummer::OnParamChange(int paramIdx)
{
  if (paramIdx >= kParamDrumVolStart && paramIdx < (kParamDrumVolStart + kNumDrumSamples))
  {
    const int drumIdx = paramIdx - kParamDrumVolStart;
    mDrumGainFactors[static_cast<size_t>(drumIdx)] = static_cast<float>(GetParam(paramIdx)->Value() / 100.0);
    return;
  }

  switch (paramIdx)
  {
    case kParamRoom:
      mRoomAmount = static_cast<float>(GetParam(kParamRoom)->Value() / 100.0);
      if (mReverb)
        mReverb->SetRoomAmount(mRoomAmount);
      break;

    case kParamMasterVolume:
      // 50% == unity gain (normal sample loudness)
      mMasterGain = static_cast<float>(GetParam(kParamMasterVolume)->Value() / 50.0);
      break;

    case kParamHiHatChoke:
      mHiHatChoke = GetParam(kParamHiHatChoke)->Bool();
      break;

    default:
      break;
  }
}

void VintageDrummer::ProcessMidiMsg(const IMidiMsg& msg)
{
  if (msg.StatusMsg() != IMidiMsg::kNoteOn)
    return;

  if (msg.Velocity() <= 0)
    return;

  const int sampleIndex = NoteToSampleIndex(msg.NoteNumber());
  if (sampleIndex < 0)
    return;

  const float velocity = static_cast<float>(msg.Velocity()) / 127.0f;
  TriggerSample(sampleIndex, velocity);
}

void VintageDrummer::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nOutChans = NOutChansConnected();
  if (nOutChans <= 0)
    return;

  for (int ch = 0; ch < nOutChans; ++ch)
    std::memset(outputs[ch], 0, static_cast<size_t>(nFrames) * sizeof(sample));

  for (int s = 0; s < nFrames; ++s)
  {
    float dryL = 0.0f;
    float dryR = 0.0f;

    for (auto& voice : mVoices)
    {
      if (!voice.active || voice.sampleIndex < 0)
        continue;

      const auto& drum = mSamples[static_cast<size_t>(voice.sampleIndex)];
      if (!drum.IsLoaded() || drum.left.size() < 2)
      {
        voice.active = false;
        continue;
      }

      const size_t idx = static_cast<size_t>(voice.position);
      if (idx + 1 >= drum.left.size())
      {
        voice.active = false;
        continue;
      }

      const float frac = static_cast<float>(voice.position - static_cast<double>(idx));
      const float l = drum.left[idx] + ((drum.left[idx + 1] - drum.left[idx]) * frac);
      const float r = drum.right[idx] + ((drum.right[idx + 1] - drum.right[idx]) * frac);

      dryL += l * voice.gain;
      dryR += r * voice.gain;

      voice.position += voice.increment;
      if (voice.position >= static_cast<double>(drum.left.size() - 1))
        voice.active = false;
    }

    float wetL = 0.0f;
    float wetR = 0.0f;

    if (mReverb)
      mReverb->Process(dryL, dryR, wetL, wetR);

    const float outL = std::clamp((dryL + wetL) * mMasterGain, -1.0f, 1.0f);
    const float outR = std::clamp((dryR + wetR) * mMasterGain, -1.0f, 1.0f);

    outputs[0][s] = static_cast<sample>(outL);

    if (nOutChans > 1)
      outputs[1][s] = static_cast<sample>(outR);
  }
}
#endif
