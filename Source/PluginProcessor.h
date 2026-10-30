// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class DoctorMigranaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
  DoctorMigranaProcessor();
  ~DoctorMigranaProcessor() override { stopTimer(); }

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState apvts;

  // ---- presets ----
  static constexpr int kNumFactoryPresets = 4;
  static const char* getFactoryPresetName(int index);
  void loadFactoryPreset(int index);

  juce::File getPresetsDir() const;
  juce::StringArray getUserPresetNames() const;
  bool saveUserPreset(const juce::String& name);
  bool loadUserPreset(const juce::String& name);
  bool deleteUserPreset(const juce::String& name);

  juce::String getPresetName() const;
  void setPresetName(const juce::String& name);

  // ---- A/B comparison ----
  int abGetActive() const;                 // 0 = A, 1 = B
  void abToggle();                         // store current in active slot, load the other
  void abCopy(int from, int to);           // copy a slot over the other one

  // peak levels (linear) for the UI meters
  std::atomic<float> inLevel{0.0f}, outLevel{0.0f};

private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  void setParamPlain(const char* id, double plainValue);
  juce::String makeSnapshot() const;
  void applySnapshot(const juce::String& snapshot);

  // COMP/COMP LEVEL and DRIVE/DRIVE LEVEL "link": when on, moving one knob moves the other in the
  // opposite direction so the output loudness stays (approximately) constant. Runs on the message thread.
  struct LinkState { bool valid = false; double a0 = 0, b0 = 0, lastA = 0, lastB = 0; };
  void timerCallback() override;
  void updateLink(LinkState& st, bool linked, const char* idA, const char* idB,
                  const double* table, double bMin, double bMax);
  LinkState mCompLink, mDriveLink;

  double mSR = 44100.0;
  int mMaxBlock = 512;
  double mEnv = 0.0, mAtt = 0.0, mRel = 0.0;
  // Rangemaster-style booster states
  double mBstLp[2] = {0.0, 0.0};
  double mBstDcX[2] = {0.0, 0.0};
  double mBstDcY[2] = {0.0, 0.0};
  // Dyna Comp-style compressor: input emphasis + output high-pass states
  double mCmpEmph[2] = {0.0, 0.0};
  double mCmpLp[2] = {0.0, 0.0};
  double mToneLP[2] = {0.0, 0.0};
  // Tube Screamer-style drive stage filter states (TPT one-poles, run at the oversampled rate)
  double mTsHp[2] = {0.0, 0.0};
  double mTsLp[2] = {0.0, 0.0};
  double mTsPost[2] = {0.0, 0.0};
  // Tube Screamer tone biquad state (transposed direct form II)
  double mTsToneZ1[2] = {0.0, 0.0};
  double mTsToneZ2[2] = {0.0, 0.0};
  // general tone: high-shelf biquad state (extra treble above 7/10 of the knob)
  double mShelfZ1[2] = {0.0, 0.0};
  double mShelfZ2[2] = {0.0, 0.0};
  double mDriveLevelG = 1.0;
  double mBoostG = 1.0, mMasterG = 1.0;

  // oversampling for the drive stage: [0] = 2x, [1] = 4x
  std::unique_ptr<juce::dsp::Oversampling<float>> mOs[2];
  int mCurrentOs = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoctorMigranaProcessor)
};
