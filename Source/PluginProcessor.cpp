// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;


// ---- Tube Screamer tone stage (active op-amp tone circuit after the 723 Hz passive low-pass) ----
// R7=1k, C5=0.22uF (passive LP); op-amp feedback Rf=1k; 20k tone pot between the (-) and (+) inputs;
// wiper -> R8=220R + C6=0.22uF to ground. Derived transfer function:
//   H(s) = (n0 + n1 s) / (d0 + d1 s + d2 s^2), discretised with the bilinear transform.
struct TsToneBiquad { double b0, b1, b2, a1, a2; };

static TsToneBiquad makeTsTone(double tone01, double fs)
{
  const double R7 = 1.0e3, C5 = 0.22e-6, R8 = 220.0, C6 = 0.22e-6, Rf = 1.0e3, Rpot = 20.0e3;
  const double tau = R8 * C6;

  const double x = juce::jlimit(0.0, 1.0, tone01);        // 0 = bass side, 1 = treble side
  const double r = x * x * (3.0 - 2.0 * x);               // G-taper-like curve
  const double w = juce::jlimit(0.003, 0.997, 1.0 - r);   // wiper: 1 -> (+) end, 0 -> (-) end
  const double a = 1.0 / (w * Rpot);                      // 1 / R(-)..wiper
  const double b = 1.0 / ((1.0 - w) * Rpot);              // 1 / wiper..R(+)
  const double Gp = a + b;

  const double n0 = Gp;
  const double n1 = C6 * (1.0 + Rf * a) + Gp * tau;
  const double d0 = Gp;
  const double d1 = C6 + Gp * tau + R7 * C5 * Gp + R7 * b * C6;
  const double d2 = R7 * C5 * (C6 + Gp * tau);

  const double K = 2.0 * fs;
  const double a0 = d0 + d1 * K + d2 * K * K;
  TsToneBiquad q;
  q.b0 = (n0 + n1 * K) / a0;
  q.b1 = (2.0 * n0) / a0;
  q.b2 = (n0 - n1 * K) / a0;
  q.a1 = (2.0 * d0 - 2.0 * d2 * K * K) / a0;
  q.a2 = (d0 - d1 * K + d2 * K * K) / a0;
  return q;
}


// ---- RBJ high-shelf (used by the general TONE above 7/10 of its travel) ----
static TsToneBiquad makeHighShelf(double f0, double gainDb, double fs)
{
  const double A = std::pow(10.0, gainDb / 40.0);
  const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
  const double cw = std::cos(w0), sw = std::sin(w0);
  const double alpha = sw / (2.0 * 0.7071);
  const double sA = 2.0 * std::sqrt(A) * alpha;
  const double a0 = (A + 1.0) - (A - 1.0) * cw + sA;
  TsToneBiquad q;
  q.b0 = (A * ((A + 1.0) + (A - 1.0) * cw + sA)) / a0;
  q.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0;
  q.b2 = (A * ((A + 1.0) + (A - 1.0) * cw - sA)) / a0;
  q.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0;
  q.a2 = ((A + 1.0) - (A - 1.0) * cw - sA) / a0;
  return q;
}

// ---- output level (dB RMS) of each stage for a reference tone, sampled at 0,5,10..100 % of the knob ----
//  COMP : 500 Hz sine, -15 dBFS peak        DRIVE: 500 Hz sine, -24 dBFS peak   (computed offline from the same models)
static const double kCompOutDb[21]  = { -23.92, -22.62, -21.40, -20.27, -19.22, -18.26, -17.39, -16.60, -15.91, -15.29, -14.75, -14.29, -13.88, -13.53, -13.24, -12.99, -12.78, -12.60, -12.45, -12.33, -12.23 };
static const double kDriveOutDb[21] = { -25.60, -24.75, -23.93, -23.05, -22.11, -21.12, -20.08, -19.01, -17.94, -16.91, -15.94, -15.08, -14.36, -13.80, -13.39, -13.07, -12.78, -12.54, -12.37, -12.24, -12.15 };

static double tableF(const double* t, double x01)               // knob 0..1 -> level dB
{
  const double pos = juce::jlimit(0.0, 1.0, x01) * 20.0;
  const int i = juce::jmin(19, (int) pos);
  return t[i] + (t[i + 1] - t[i]) * (pos - (double) i);
}

static double tableInv(const double* t, double dB)              // level dB -> knob 0..1 (table is increasing)
{
  if (dB <= t[0]) return 0.0;
  if (dB >= t[20]) return 1.0;
  for (int i = 0; i < 20; ++i)
    if (dB <= t[i + 1])
      return ((double) i + (dB - t[i]) / (t[i + 1] - t[i])) / 20.0;
  return 1.0;
}

APVTS::ParameterLayout DoctorMigranaProcessor::createLayout()
{
  using juce::AudioParameterFloat;
  using juce::AudioParameterBool;
  using juce::AudioParameterChoice;
  using juce::NormalisableRange;
  using juce::ParameterID;

  APVTS::ParameterLayout l;
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"boost", 1}, "Boost", NormalisableRange<float>(0.f, 24.f, 0.1f), 3.f));
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"comp", 1}, "Comp", NormalisableRange<float>(0.f, 100.f, 0.1f), 20.f));
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"compLevel", 1}, "Comp Level", NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"range", 1}, "Boost Range", NormalisableRange<float>(0.f, 100.f, 0.1f), 100.f));   // 0 = treble booster, 100 = full range
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"drive", 1}, "Drive", NormalisableRange<float>(0.f, 100.f, 0.1f), 70.f));
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"driveTone", 1}, "Drive Tone", NormalisableRange<float>(0.f, 100.f, 0.1f), 55.f));
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"driveLevel", 1}, "Drive Level", NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
  // general TONE: 7/10 of the travel (70 %) = the old maximum (open), above that extra treble
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"tone", 1}, "Tone", NormalisableRange<float>(0.f, 100.f, 0.1f), 70.f));
  // MASTER: 12 o'clock (50 % of the travel) = 0 dB (the pot does not affect), down to -24 dB and up to +12 dB
  NormalisableRange<float> masterRange(
    -24.f, 12.f,
    [](float lo, float hi, float v) { return v < 0.5f ? lo * (1.f - v / 0.5f) : hi * ((v - 0.5f) / 0.5f); },
    [](float lo, float hi, float v) { return v < 0.f ? 0.5f * (1.f - v / lo) : 0.5f + 0.5f * (v / hi); },
    [](float lo, float hi, float v) { return juce::jlimit(lo, hi, std::round(v * 10.f) / 10.f); });
  masterRange.interval = 0.1f;   // only used to display one decimal in the knob text (snapping is done by the lambda above)
  l.add(std::make_unique<AudioParameterFloat>(ParameterID{"master", 1}, "Master", masterRange, 0.f));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"boostOn", 1}, "Boost On", false));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"compOn", 1}, "Comp On", false));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"driveOn", 1}, "Drive On", true));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass", 1}, "Bypass", false));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"compLink", 1}, "Comp Link", false));
  l.add(std::make_unique<AudioParameterBool>(ParameterID{"driveLink", 1}, "Drive Link", false));
  l.add(std::make_unique<AudioParameterChoice>(ParameterID{"oversample", 1}, "Oversampling",
                                               juce::StringArray{"Off", "2x", "4x"}, 0));
  return l;
}

DoctorMigranaProcessor::DoctorMigranaProcessor()
: AudioProcessor(BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createLayout())
{
  apvts.state.setProperty("presetName", "Classic Overdrive", nullptr);
  startTimerHz(30);
}

bool DoctorMigranaProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  const auto& out = layouts.getMainOutputChannelSet();
  if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
    return false;
  return out == layouts.getMainInputChannelSet();
}

void DoctorMigranaProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  mSR = sampleRate;
  mMaxBlock = juce::jmax(32, samplesPerBlock);
  mAtt = std::exp(-1.0 / (0.010 * mSR));
  mRel = std::exp(-1.0 / (0.400 * mSR));   // Dyna Comp: slow release
  mEnv = 0.0;
  for (int c = 0; c < 2; ++c) mCmpEmph[c] = mCmpLp[c] = 0.0;
  for (int c = 0; c < 2; ++c) mBstLp[c] = mBstDcX[c] = mBstDcY[c] = 0.0;
  for (int c = 0; c < 2; ++c) { mToneLP[c] = mTsHp[c] = mTsLp[c] = mTsPost[c] = 0.0; }
  for (int c = 0; c < 2; ++c) mTsToneZ1[c] = mTsToneZ2[c] = mShelfZ1[c] = mShelfZ2[c] = 0.0;
  mDriveLevelG = std::pow(10.0, apvts.getRawParameterValue("driveLevel")->load() / 20.0);
  mBoostG = std::pow(10.0, apvts.getRawParameterValue("boost")->load() / 20.0);
  mMasterG = std::pow(10.0, apvts.getRawParameterValue("master")->load() / 20.0);

  for (int i = 0; i < 2; ++i)
  {
    // factor is a power of two: 1 -> 2x, 2 -> 4x. Polyphase IIR = low latency.
    mOs[i] = std::make_unique<juce::dsp::Oversampling<float>>(
      2, (size_t) (i + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    mOs[i]->initProcessing((size_t) mMaxBlock);
  }

  mCurrentOs = juce::jlimit(0, 2, (int) std::round(apvts.getRawParameterValue("oversample")->load()));
  setLatencySamples(mCurrentOs > 0 ? (int) std::round(mOs[mCurrentOs - 1]->getLatencyInSamples()) : 0);
}

void DoctorMigranaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
  juce::ScopedNoDenormals noDenormals;
  const int nCh = juce::jmin(buffer.getNumChannels(), 2);
  const int nFrames = buffer.getNumSamples();

  auto p = [this](const char* id) { return (double) apvts.getRawParameterValue(id)->load(); };

  float inPeak = 0.0f;
  for (int c = 0; c < nCh; ++c) inPeak = juce::jmax(inPeak, buffer.getMagnitude(c, 0, nFrames));
  inLevel.store(inPeak);

  if (p("bypass") > 0.5) { outLevel.store(inPeak); return; }

  const bool series = true;                 // fixed order: Boost > Comp > Drive
  const bool boostOn = p("boostOn") > 0.5;
  const bool compOn = p("compOn") > 0.5;
  const bool driveOn = p("driveOn") > 0.5;

  // oversampling selection (changes latency; state of the new filter is cleared)
  const int osIdx = juce::jlimit(0, 2, (int) std::round(p("oversample")));
  if (osIdx != mCurrentOs)
  {
    mCurrentOs = osIdx;
    if (osIdx > 0) mOs[osIdx - 1]->reset();
    for (int c = 0; c < 2; ++c) mTsHp[c] = mTsLp[c] = mTsPost[c] = 0.0;
    setLatencySamples(osIdx > 0 ? (int) std::round(mOs[osIdx - 1]->getLatencyInSamples()) : 0);
  }

  const double boostTarget = boostOn ? std::pow(10.0, p("boost") / 20.0) : 1.0;
  const double masterTarget = std::pow(10.0, p("master") / 20.0);

  // Booster: Dallas Rangemaster-style. The boost only lifts what is above a split frequency
  //   RANGE 0   -> split at ~2.6 kHz (treble booster, bass passes at unity)
  //   RANGE 100 -> split at ~10 Hz   (full-range boost)
  // Followed by germanium-like asymmetric soft clipping that only colours the sound when pushed.
  const double rangeN = juce::jlimit(0.0, 1.0, p("range") / 100.0);
  const double bstFc = 2600.0 * std::pow(10.0 / 2600.0, rangeN);
  const double bstTan = std::tan(juce::MathConstants<double>::pi * bstFc / mSR);
  const double bstK = bstTan / (1.0 + bstTan);
  const double bstDcR = std::exp(-2.0 * juce::MathConstants<double>::pi * 20.0 / mSR);

  auto boostSample = [&](double v, int c)
  {
    // one-pole split (TPT): lp / hp
    const double t = (v - mBstLp[c]) * bstK;
    const double lp = t + mBstLp[c];
    mBstLp[c] = lp + t;
    const double hp = v - lp;

    if (!boostOn) return v;

    double y = v + (mBoostG - 1.0) * hp;
    y = (y >= 0.0) ? std::tanh(y) : 0.7 * std::tanh(y / 0.7);      // asymmetric soft clip

    const double o = y - mBstDcX[c] + bstDcR * mBstDcY[c];          // DC blocker (asymmetry creates DC)
    mBstDcX[c] = y;
    mBstDcY[c] = o;
    return o;
  };

  // Compressor: MXR Dyna Comp-style (OTA whose gain is pulled down by a detector listening to the OUTPUT)
  //   Sensitivity -> maximum gain 0..36 dB; the louder the output, the less gain the OTA gets.
  //   Level (Output) -> makeup, which also moves the output high-pass between ~310 Hz (min) and ~52 Hz (max).
  const double sens = p("comp") / 100.0;
  const double cmpGmax = std::pow(10.0, sens * 36.0 / 20.0);
  const double cmpE0 = 0.5;              // output envelope at which the OTA gain hits its floor
  const double cmpGFloor = 0.01;
  const double cmpYsat = 0.9;            // OTA output saturation
  const double lvDb = p("compLevel");
  const double cmpOutG = std::pow(10.0, lvDb / 20.0) * 0.7;
  const double cmpHpFc = 310.0 * std::pow(52.0 / 310.0, (lvDb + 12.0) / 24.0);
  const double cmpEmphA = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 3000.0 / mSR);
  const double cmpHpA = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * cmpHpFc / mSR);

  // Drive: Tube Screamer-style clipping stage (non-inverting op-amp with diodes in the feedback loop)
  //   gain stage: x + Rf/Zg * x, Zg = 4.7k + 47nF  (high-pass corner ~720 Hz)
  //   feedback cap 51pF with Rf -> low-pass that moves down as drive goes up
  //   silicon diodes (1N914-like) -> symmetric soft clipping (~0.55 V)
  // Unlike the real pedal (whose minimum gain is already ~21 dB and clips with digital-level signals), the gain
  // here is dB-linear from about -2 dB (clean) at DRIVE = 0 up to +41 dB at DRIVE = 100, so the saturation
  // grows progressively along the whole knob.
  const double d = p("drive") / 100.0;
  const double tsGainDb = -2.0 + 43.3 * std::pow(d, 0.9);
  const double tsGain = std::pow(10.0, tsGainDb / 20.0);          // ~0.8 .. ~116
  const double rf = tsGain * 4.7e3;                               // equivalent Rf (only sets the feedback low-pass corner)
  const double fsOs = mSR * (double) (1 << osIdx);
  auto tpt = [fsOs](double fcHz)
  {
    const double fcSafe = juce::jmin(fcHz, 0.45 * fsOs);
    const double g = std::tan(juce::MathConstants<double>::pi * fcSafe / fsOs);
    return g / (1.0 + g);
  };
  const double gHp = tpt(720.0);
  const double gLp = tpt(1.0 / (2.0 * juce::MathConstants<double>::pi * rf * 51.0e-12));
  const double gPost = tpt(5000.0);                              // tames fizz after the clipper
  const double vd = 0.55;                                        // diode knee voltage
  const double refVolts = 1.0;                                   // 1.0 FS = 1 V at the pedal input
  // output level compensation: starts almost at unity and goes down by up to 6 dB as the gain grows
  // (reached at 80 % of the knob), so the loudness increases with DRIVE but does not jump.
  const double sm = juce::jlimit(0.0, 1.0, d / 0.8);
  const double driveOutGain = 0.9 * std::pow(10.0, -6.0 * (sm * sm * (3.0 - 2.0 * sm)) / 20.0);

  // General TONE: one-pole low-pass 800 Hz .. 12 kHz over the first 7/10 of the knob (the old range),
  // then, from 7/10 to 10/10, an extra high-shelf (+0 .. +9 dB above ~3 kHz) for more brightness.
  const double toneK = juce::jlimit(0.0, 1.0, p("tone") / 100.0);
  const double fc = 800.0 * std::pow(15.0, juce::jmin(toneK, 0.7) / 0.7);
  const double toneA = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * fc / mSR);
  const double shelfDb = toneK > 0.7 ? 9.0 * (toneK - 0.7) / 0.3 : 0.0;
  const TsToneBiquad shelf = makeHighShelf(3000.0, shelfDb, mSR);

  // DRIVE TONE: the Tube Screamer tone circuit, only inside the Drive pedal; DRIVE LEVEL: its output volume
  const TsToneBiquad tsTone = makeTsTone(p("driveTone") / 100.0, mSR);
  const double tsToneComp = 1.41;   // +3 dB, level compensation of the TS tone stage
  const double driveLevelTarget = std::pow(10.0, p("driveLevel") / 20.0);

  const double smooth = 0.002;

  auto comp = [&](double* x, int n)
  {
    if (!compOn) return;

    // OTA gain from the output envelope (feedback detector)
    const double g = cmpGmax * juce::jlimit(cmpGFloor, 1.0, 1.0 - mEnv / cmpE0);

    double yv[2] = {0.0, 0.0};
    double det = 0.0;
    for (int c = 0; c < n; ++c)
    {
      // OTA input network: gentle high-frequency emphasis (~+3.5 dB above 3 kHz)
      mCmpEmph[c] += cmpEmphA * (x[c] - mCmpEmph[c]);
      const double xe = x[c] + 0.5 * (x[c] - mCmpEmph[c]);
      yv[c] = cmpYsat * std::tanh(g * xe / cmpYsat);       // OTA saturation
      det = juce::jmax(det, std::fabs(yv[c]));
    }

    // envelope of the output: fast attack, slow release
    mEnv = (det > mEnv) ? mAtt * mEnv + (1.0 - mAtt) * det
                        : mRel * mEnv + (1.0 - mRel) * det;

    // output coupling high-pass + level
    for (int c = 0; c < n; ++c)
    {
      mCmpLp[c] += cmpHpA * (yv[c] - mCmpLp[c]);
      x[c] = (yv[c] - mCmpLp[c]) * cmpOutG;
    }
  };

  auto driveSample = [&](double x, int c)
  {
    const double v0 = x * refVolts;

    // high-pass 720 Hz (gain stage shunt: 4.7k + 47nF)
    double v = (v0 - mTsHp[c]) * gHp;
    const double lp1 = v + mTsHp[c];
    mTsHp[c] = lp1 + v;
    const double hp = v0 - lp1;

    // gain, then feedback low-pass (51 pF || Rf)
    const double amplified = tsGain * hp;
    v = (amplified - mTsLp[c]) * gLp;
    const double lp2 = v + mTsLp[c];
    mTsLp[c] = lp2 + v;

    // diode soft clipping of the feedback voltage, plus the clean (unity) path
    const double out = v0 + vd * std::tanh(lp2 / vd);

    // fizz-control low-pass
    v = (out - mTsPost[c]) * gPost;
    const double lp3 = v + mTsPost[c];
    mTsPost[c] = lp3 + v;

    return lp3 * driveOutGain / refVolts;
  };

  juce::dsp::AudioBlock<float> whole(buffer);

  for (int start = 0; start < nFrames; start += mMaxBlock)
  {
    const int n = juce::jmin(mMaxBlock, nFrames - start);
    auto blk = whole.getSubsetChannelBlock(0, (size_t) nCh).getSubBlock((size_t) start, (size_t) n);

    // 1) Boost (+ Comp when in series order)
    for (int s = 0; s < n; ++s)
    {
      mBoostG += smooth * (boostTarget - mBoostG);
      double x[2] = {0.0, 0.0};
      for (int c = 0; c < nCh; ++c) x[c] = boostSample((double) blk.getSample(c, s), c);
      if (series) comp(x, nCh);
      for (int c = 0; c < nCh; ++c) blk.setSample(c, s, (float) x[c]);
    }

    // 2) Drive (oversampled if enabled; always routed through the oversampler so latency stays constant)
    if (osIdx > 0)
    {
      auto& os = *mOs[osIdx - 1];
      juce::dsp::AudioBlock<const float> constBlk(blk);
      auto up = os.processSamplesUp(constBlk);
      if (driveOn)
        for (size_t c = 0; c < up.getNumChannels(); ++c)
          for (size_t s = 0; s < up.getNumSamples(); ++s)
            up.setSample((int) c, (int) s, (float) driveSample((double) up.getSample((int) c, (int) s), (int) c));
      os.processSamplesDown(blk);
    }
    else if (driveOn)
    {
      for (int s = 0; s < n; ++s)
        for (int c = 0; c < nCh; ++c)
          blk.setSample(c, s, (float) driveSample((double) blk.getSample(c, s), c));
    }

    // 2b) Drive tone (Tube Screamer tone circuit) + drive level. Only while the Drive pedal is active;
    //     the filter keeps running otherwise so its state stays continuous when the pedal is toggled.
    for (int s = 0; s < n; ++s)
    {
      mDriveLevelG += smooth * (driveLevelTarget - mDriveLevelG);
      for (int c = 0; c < nCh; ++c)
      {
        const double in = (double) blk.getSample(c, s);
        const double ts = tsTone.b0 * in + mTsToneZ1[c];
        mTsToneZ1[c] = tsTone.b1 * in - tsTone.a1 * ts + mTsToneZ2[c];
        mTsToneZ2[c] = tsTone.b2 * in - tsTone.a2 * ts;
        blk.setSample(c, s, (float) (driveOn ? ts * tsToneComp * mDriveLevelG : in));
      }
    }

    // 3) Comp after drive (non-series order)
    if (!series)
    {
      for (int s = 0; s < n; ++s)
      {
        double x[2] = {0.0, 0.0};
        for (int c = 0; c < nCh; ++c) x[c] = (double) blk.getSample(c, s);
        comp(x, nCh);
        for (int c = 0; c < nCh; ++c) blk.setSample(c, s, (float) x[c]);
      }
    }

    // 4) General tone + master
    for (int s = 0; s < n; ++s)
    {
      mMasterG += smooth * (masterTarget - mMasterG);
      for (int c = 0; c < nCh; ++c)
      {
        const double in = (double) blk.getSample(c, s);

        mToneLP[c] += toneA * (in - mToneLP[c]);
        const double lp = mToneLP[c];

        const double y = shelf.b0 * lp + mShelfZ1[c];                 // high-shelf (identity when shelfDb = 0)
        mShelfZ1[c] = shelf.b1 * lp - shelf.a1 * y + mShelfZ2[c];
        mShelfZ2[c] = shelf.b2 * lp - shelf.a2 * y;

        blk.setSample(c, s, (float) (y * mMasterG));
      }
    }
  }

  float outPeak = 0.0f;
  for (int c = 0; c < nCh; ++c) outPeak = juce::jmax(outPeak, buffer.getMagnitude(c, 0, nFrames));
  outLevel.store(outPeak);
}


// ============================ presets ============================
namespace
{
  struct FactoryPreset
  {
    const char* name;
    float boost, comp, drive, driveTone, master, range;
    bool boostOn, compOn, driveOn;
  };

  const FactoryPreset kFactory[DoctorMigranaProcessor::kNumFactoryPresets] = {
    //  name                boost comp  drive dTone master range   boostOn compOn driveOn
    { "Classic Overdrive",   3.f, 20.f, 70.f, 55.f, -2.f, 100.f,   false,  false,  true  },
    { "Clean Boost",        12.f, 25.f,  0.f, 70.f, -4.f, 100.f,   true,   true,   false },
    { "Crunch",              6.f, 30.f, 82.f, 60.f, -5.f, 100.f,   true,   true,   true  },
    { "Sustain Lead",        9.f, 70.f, 90.f, 50.f, -8.f,  55.f,   true,   true,   true  },
  };

  // every parameter that belongs to a preset / A-B snapshot (order matters for snapshots)
  const char* kFloatIds[9] = {"boost", "comp", "drive", "tone", "master", "compLevel", "range", "driveLevel", "driveTone"};
  const char* kBoolIds[5]  = {"boostOn", "compOn", "driveOn", "compLink", "driveLink"};
  const int kSnapshotSize = 14;
}

const char* DoctorMigranaProcessor::getFactoryPresetName(int index)
{
  return kFactory[juce::jlimit(0, kNumFactoryPresets - 1, index)].name;
}

void DoctorMigranaProcessor::setParamPlain(const char* id, double plainValue)
{
  if (auto* p = apvts.getParameter(id))
  {
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1((float) plainValue));
    p->endChangeGesture();
  }
}

void DoctorMigranaProcessor::loadFactoryPreset(int index)
{
  const auto& f = kFactory[juce::jlimit(0, kNumFactoryPresets - 1, index)];
  setParamPlain("boost", f.boost);
  setParamPlain("comp", f.comp);
  setParamPlain("drive", f.drive);
  setParamPlain("driveTone", f.driveTone);
  setParamPlain("master", f.master);
  setParamPlain("range", f.range);
  setParamPlain("boostOn", f.boostOn ? 1.0 : 0.0);
  setParamPlain("compOn", f.compOn ? 1.0 : 0.0);
  setParamPlain("driveOn", f.driveOn ? 1.0 : 0.0);
  setParamPlain("compLevel", 0.0);
  setParamPlain("driveLevel", 0.0);
  setParamPlain("tone", 70.0);          // 7/10 = neutral
  setParamPlain("compLink", 0.0);
  setParamPlain("driveLink", 0.0);
  setPresetName(f.name);
}

juce::File DoctorMigranaProcessor::getPresetsDir() const
{
  return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("DoctorMigrana").getChildFile("Presets");
}

juce::StringArray DoctorMigranaProcessor::getUserPresetNames() const
{
  juce::StringArray names;
  for (const auto& f : getPresetsDir().findChildFiles(juce::File::findFiles, false, "*.dmpreset"))
    names.add(f.getFileNameWithoutExtension());
  names.sortNatural();
  return names;
}

bool DoctorMigranaProcessor::saveUserPreset(const juce::String& name)
{
  const auto dir = getPresetsDir();
  if (!dir.createDirectory()) return false;

  juce::XmlElement xml("DMPreset");
  for (auto* id : kFloatIds) xml.setAttribute(id, (double) apvts.getRawParameterValue(id)->load());
  for (auto* id : kBoolIds)  xml.setAttribute(id, (double) apvts.getRawParameterValue(id)->load());

  const auto file = dir.getChildFile(juce::File::createLegalFileName(name) + ".dmpreset");
  if (!xml.writeTo(file)) return false;
  setPresetName(file.getFileNameWithoutExtension());
  return true;
}

bool DoctorMigranaProcessor::loadUserPreset(const juce::String& name)
{
  const auto file = getPresetsDir().getChildFile(name + ".dmpreset");
  auto xml = juce::XmlDocument::parse(file);
  if (xml == nullptr || !xml->hasTagName("DMPreset")) return false;

  for (auto* id : kFloatIds) if (xml->hasAttribute(id)) setParamPlain(id, xml->getDoubleAttribute(id));
  for (auto* id : kBoolIds)  if (xml->hasAttribute(id)) setParamPlain(id, xml->getDoubleAttribute(id));
  setPresetName(name);
  return true;
}

bool DoctorMigranaProcessor::deleteUserPreset(const juce::String& name)
{
  return getPresetsDir().getChildFile(name + ".dmpreset").deleteFile();
}

juce::String DoctorMigranaProcessor::getPresetName() const
{
  const auto n = apvts.state.getProperty("presetName").toString();
  return n.isEmpty() ? juce::String("Custom") : n;
}

void DoctorMigranaProcessor::setPresetName(const juce::String& name)
{
  apvts.state.setProperty("presetName", name, nullptr);
}


// ============================ A/B ============================
juce::String DoctorMigranaProcessor::makeSnapshot() const
{
  juce::StringArray v;
  for (auto* id : kFloatIds) v.add(juce::String((double) apvts.getRawParameterValue(id)->load(), 4));
  for (auto* id : kBoolIds)  v.add(juce::String((double) apvts.getRawParameterValue(id)->load(), 0));
  return getPresetName() + "|" + v.joinIntoString(",");
}

void DoctorMigranaProcessor::applySnapshot(const juce::String& snapshot)
{
  const auto name = snapshot.upToFirstOccurrenceOf("|", false, false);
  const auto vals = juce::StringArray::fromTokens(snapshot.fromFirstOccurrenceOf("|", false, false), ",", "");
  if (vals.size() != kSnapshotSize) return;       // snapshots from older versions are ignored

  int i = 0;
  for (auto* id : kFloatIds) setParamPlain(id, vals[i++].getDoubleValue());
  for (auto* id : kBoolIds)  setParamPlain(id, vals[i++].getDoubleValue());
  setPresetName(name);
}

int DoctorMigranaProcessor::abGetActive() const
{
  return (int) apvts.state.getProperty("abActive", 0) == 1 ? 1 : 0;
}

void DoctorMigranaProcessor::abToggle()
{
  const int active = abGetActive();
  const int other = 1 - active;
  const char* keys[2] = {"abA", "abB"};

  apvts.state.setProperty(keys[active], makeSnapshot(), nullptr);

  auto s = apvts.state.getProperty(keys[other]).toString();
  if (s.isEmpty()) s = makeSnapshot();      // first use: the other slot starts as a copy of the current one

  apvts.state.setProperty("abActive", other, nullptr);
  applySnapshot(s);
}

void DoctorMigranaProcessor::abCopy(int from, int to)
{
  if (from == to) return;
  const int active = abGetActive();
  const char* keys[2] = {"abA", "abB"};

  apvts.state.setProperty(keys[active], makeSnapshot(), nullptr);   // make sure the active slot is up to date

  const auto s = apvts.state.getProperty(keys[from]).toString();
  if (s.isEmpty()) return;

  apvts.state.setProperty(keys[to], s, nullptr);
  if (to == active) applySnapshot(s);
}


// ============================ COMP LINK / DRIVE LINK ============================
// Relation kept while linked:   B = B0 - ( f(A) - f(A0) )     (f = output level in dB vs knob position)
// (A0, B0) is the pair of knob values at the moment the link was switched on (or after a preset / A-B change).
void DoctorMigranaProcessor::updateLink(LinkState& st, bool linked, const char* idA, const char* idB,
                                        const double* table, double bMin, double bMax)
{
  if (!linked) { st.valid = false; return; }

  const double A = (double) apvts.getRawParameterValue(idA)->load();      // 0..100
  const double B = (double) apvts.getRawParameterValue(idB)->load();      // dB

  if (!st.valid)
  {
    st.a0 = st.lastA = A;
    st.b0 = st.lastB = B;
    st.valid = true;
    return;
  }

  const bool aMoved = std::abs(A - st.lastA) > 0.05;
  const bool bMoved = std::abs(B - st.lastB) > 0.05;

  if (aMoved && !bMoved)                       // knob A moved -> compensate with knob B
  {
    const double target = juce::jlimit(bMin, bMax, st.b0 - (tableF(table, A / 100.0) - tableF(table, st.a0 / 100.0)));
    setParamPlain(idB, target);
    st.lastA = A;
    st.lastB = (double) apvts.getRawParameterValue(idB)->load();
  }
  else if (bMoved && !aMoved)                  // knob B moved -> compensate with knob A
  {
    const double wanted = tableF(table, st.a0 / 100.0) + (st.b0 - B);
    setParamPlain(idA, 100.0 * tableInv(table, wanted));
    st.lastB = B;
    st.lastA = (double) apvts.getRawParameterValue(idA)->load();
  }
  else if (aMoved && bMoved)                   // both changed at once (preset, A/B, automation): re-baseline
  {
    st.a0 = st.lastA = A;
    st.b0 = st.lastB = B;
  }
}

void DoctorMigranaProcessor::timerCallback()
{
  updateLink(mCompLink, apvts.getRawParameterValue("compLink")->load() > 0.5f, "comp", "compLevel", kCompOutDb, -12.0, 12.0);
  updateLink(mDriveLink, apvts.getRawParameterValue("driveLink")->load() > 0.5f, "drive", "driveLevel", kDriveOutDb, -12.0, 12.0);
}

juce::AudioProcessorEditor* DoctorMigranaProcessor::createEditor()
{
  return new DoctorMigranaEditor(*this);
}

void DoctorMigranaProcessor::getStateInformation(juce::MemoryBlock& destData)
{
  if (auto xml = apvts.copyState().createXml())
    copyXmlToBinary(*xml, destData);
}

void DoctorMigranaProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  if (auto xml = getXmlFromBinary(data, sizeInBytes))
    if (xml->hasTagName(apvts.state.getType()))
      apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new DoctorMigranaProcessor();
}
