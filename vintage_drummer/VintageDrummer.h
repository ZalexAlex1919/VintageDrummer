#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

const int kNumPresets = 1;

enum EParams
{
  kParamRoom = 0,
  kParamMasterVolume,
  kParamHiHatChoke,
  kParamKickVolume,
  kParamSnareVolume,
  kParamSideStickVolume,
  kParamClosedHatVolume,
  kParamFloorTomVolume,
  kParamLowTomVolume,
  kParamMidTomVolume,
  kParamHiTomVolume,
  kParamCrashVolume,
  kParamOpenHatVolume,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class VintageDrummer final : public Plugin
{
public:
  VintageDrummer(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
#endif

private:
  struct SampleData
  {
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate = 44100.0;

    bool IsLoaded() const
    {
      return !left.empty() && left.size() == right.size();
    }
  };

  struct Voice
  {
    int sampleIndex = -1;
    double position = 0.0;
    double increment = 1.0;
    float gain = 0.0f;
    // Monotonic counter used to steal the oldest active voice when polyphony is full.
    uint64_t age = 0;
    bool active = false;
  };

  struct GrooveData
  {
    std::string name;
    std::filesystem::path tempFilePath;
  };

  struct ReverbState;

  static constexpr int kNumDrumSamples = 10;
  static constexpr int kMaxVoices = 32;
  static constexpr int kParamDrumVolStart = kParamKickVolume;

  void ResolveAssetPaths();
  void ScanMidiGrooves();
  std::vector<std::filesystem::path> FindMidiGrooveFiles(const std::filesystem::path& directory) const;
  void LoadEmbeddedGrooves();
  bool EnsureEmbeddedGrooveFile(int resourceID, const std::filesystem::path& destinationPath) const;
  bool LoadAllSamples();
  bool LoadEmbeddedWav(int resourceID, SampleData& outData) const;
  bool LoadWavData(const uint8_t* bytes, size_t size, SampleData& outData) const;
  bool LoadWavFile(const std::filesystem::path& filePath, SampleData& outData) const;
  int NoteToSampleIndex(int midiNote) const;
  void TriggerSample(int sampleIndex, float velocity);

  std::filesystem::path mSampleDirectory;
  std::filesystem::path mBackgroundImagePath;
  std::filesystem::path mLogoImagePath;
  std::filesystem::path mGrooveDirectory;
  std::filesystem::path mGrooveTempDirectory;
  std::vector<std::filesystem::path> mGrooveFiles;
  std::vector<GrooveData> mEmbeddedGrooves;

  std::array<SampleData, kNumDrumSamples> mSamples {};
  std::array<Voice, kMaxVoices> mVoices {};
  std::unique_ptr<ReverbState> mReverb;

  double mHostSampleRate = 44100.0;
  float mRoomAmount = 0.0f;
  float mMasterGain = 1.0f;
  bool mHiHatChoke = true;
  bool mGroovesPanelVisible = false;
  std::array<float, kNumDrumSamples> mDrumGainFactors {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
  uint64_t mVoiceAgeCounter = 1;
};
