// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
  // scale = size of the knob relative to the full-size ones (it matches the cap baked in the background image)
  struct KnobDef { const char* id; float cx, cy, scale; };

  // Layout (design space 1920x1280)
  //   main row:  RANGE | BOOST | COMP | COMP LEVEL | DRIVE | DRIVE TONE, DRIVE LEVEL (smaller, staggered)
  //   top right: TONE | MASTER
  const KnobDef knobDefs[9] = {
    {"range",      407.f,  720.f, 0.62f},
    {"boost",      627.f,  720.f, 1.00f},
    {"comp",       846.f,  720.f, 1.00f},
    {"compLevel",  1066.f, 720.f, 0.62f},
    {"drive",      1288.f, 720.f, 1.00f},
    {"driveTone",  1452.f, 665.f, 0.62f},
    {"driveLevel", 1568.f, 785.f, 0.62f},
    {"tone",       1390.f, 425.f, 0.70f},
    {"master",     1550.f, 425.f, 0.70f},
  };

  const char* pedalIds[3] = {"boostOn", "compOn", "driveOn"};
  const float pedalX[3] = {640.f, 958.f, 1276.f};
  const juce::Colour pedalColours[3] = {juce::Colour(255, 40, 30), juce::Colour(255, 215, 40), juce::Colour(255, 40, 30)};

  juce::Image load(const void* d, int n) { return juce::ImageCache::getFromMemory(d, n); }
}

DoctorMigranaEditor::DoctorMigranaEditor(DoctorMigranaProcessor& p)
: AudioProcessorEditor(&p), proc(p)
{
  background = load(BinaryData::bg_jpg, BinaryData::bg_jpgSize);

  for (int i = 0; i < 9; ++i)
  {
    // knob component is 180 * scale design px wide (hit area); the cap itself is in the background
    knobs[i] = std::make_unique<PointerKnob>();
    auto& k = *knobs[i];
    addAndMakeVisible(k);
    sliderAtts[i] = std::make_unique<SliderAtt>(proc.apvts, knobDefs[i].id, k);
    k.setPopupDisplayEnabled(true, true, this);
    if (auto* par = proc.apvts.getParameter(knobDefs[i].id))
      k.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
  }

  for (int i = 0; i < 3; ++i)
  {
    pedals[i] = std::make_unique<PedalButton>(pedalColours[i]);
    addAndMakeVisible(*pedals[i]);
    buttonAtts[i] = std::make_unique<ButtonAtt>(proc.apvts, pedalIds[i], *pedals[i]);
  }

  bypass = std::make_unique<BypassSwitch>(load(BinaryData::lever_on_png, BinaryData::lever_on_pngSize),
                                          load(BinaryData::lever_off_png, BinaryData::lever_off_pngSize));
  addAndMakeVisible(*bypass);
  buttonAtts[3] = std::make_unique<ButtonAtt>(proc.apvts, "bypass", *bypass);

  // COMP LINK / DRIVE LINK (the old MODE switches): lever up + LED on = linked
  compLinkSw = std::make_unique<ToggleSwitch>(load(BinaryData::mode_up_png, BinaryData::mode_up_pngSize),
                                              load(BinaryData::mode_down_png, BinaryData::mode_down_pngSize));
  driveLinkSw = std::make_unique<ToggleSwitch>(load(BinaryData::series_up_png, BinaryData::series_up_pngSize),
                                               load(BinaryData::series_down_png, BinaryData::series_down_pngSize));
  addAndMakeVisible(*compLinkSw);
  addAndMakeVisible(*driveLinkSw);
  buttonAtts[4] = std::make_unique<ButtonAtt>(proc.apvts, "compLink", *compLinkSw);
  buttonAtts[5] = std::make_unique<ButtonAtt>(proc.apvts, "driveLink", *driveLinkSw);

  presetBox = std::make_unique<PresetBox>();
  presetBox->nameSource = [this] { return proc.getPresetName(); };
  presetBox->onClick = [this] { showPresetMenu(); };
  addAndMakeVisible(*presetBox);

  abBox = std::make_unique<ABBox>();
  abBox->activeSource = [this] { return proc.abGetActive(); };
  abBox->onToggle = [this] { proc.abToggle(); };
  abBox->onMenu = [this] { showABMenu(); };
  addAndMakeVisible(*abBox);

  gear = std::make_unique<GearButton>();
  gear->onClick = [this] { showSettingsMenu(); };
  addAndMakeVisible(*gear);

  inMeter = std::make_unique<LevelMeter>(proc.inLevel, "INPUT", false);
  outMeter = std::make_unique<LevelMeter>(proc.outLevel, "OUTPUT", true);
  addAndMakeVisible(*inMeter);
  addAndMakeVisible(*outMeter);

  setResizable(true, true);
  getConstrainer()->setFixedAspectRatio(1920.0 / 1280.0);
  setResizeLimits(480, 320, 1920, 1280);
  setSize(960, 640);
}

void DoctorMigranaEditor::paint(juce::Graphics& g)
{
  g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
  if (background.isValid())
    g.drawImage(background, getLocalBounds().toFloat());
  else
    g.fillAll(juce::Colour(28, 28, 30));

}

void DoctorMigranaEditor::resized()
{
  const float s = getWidth() / 1920.0f;
  auto R = [s](float x, float y, float w, float h) {
    return juce::Rectangle<int>((int) std::round(x * s), (int) std::round(y * s),
                                (int) std::round(w * s), (int) std::round(h * s));
  };

  for (int i = 0; i < 9; ++i)
  {
    const float sz = 180.f * knobDefs[i].scale;
    knobs[i]->setBounds(R(knobDefs[i].cx - sz * 0.5f, knobDefs[i].cy - sz * 0.5f, sz, sz));
  }

  for (int i = 0; i < 3; ++i)
    pedals[i]->setBounds(R(pedalX[i] - 70.f, 880.f, 140.f, 190.f));

  // ON/BYPASS moved to the left column (where the MODE blocks were); the link switches took its old place
  bypass->setBounds(R(296.f, 355.f, 140.f, 235.f));

  abBox->setBounds(R(1517.f, 116.f, 140.f, 45.f));
  presetBox->setBounds(R(1302.f, 172.f, 355.f, 45.f));
  gear->setBounds(R(1462.f, 114.f, 52.f, 52.f));

  compLinkSw->setBounds(R(472.f, 351.f, 140.f, 90.f));
  driveLinkSw->setBounds(R(472.f, 465.f, 140.f, 90.f));

  inMeter->setBounds(R(310.f, 1080.f, 230.f, 60.f));
  outMeter->setBounds(R(1375.f, 1080.f, 230.f, 60.f));
}

void DoctorMigranaEditor::showSettingsMenu()
{
  auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter("oversample"));
  if (par == nullptr) return;

  juce::PopupMenu m;
  m.addSectionHeader("Oversampling (Drive)");
  const char* names[3] = {"Off", "2x", "4x"};
  for (int i = 0; i < 3; ++i)
    m.addItem(i + 1, names[i], true, par->getIndex() == i);
  m.addSeparator();
  m.addItem(100, "About / Credits...");

  m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(gear.get()),
                  [par](int result)
                  {
                    if (result >= 1 && result <= 3)
                    {
                      par->beginChangeGesture();
                      *par = result - 1;
                      par->endChangeGesture();
                    }
                    else if (result == 100)
                    {
                      const juce::String text =
                        juce::String(JucePlugin_Name) + "  v" + JucePlugin_VersionString + "\n"
                        + "by " + JucePlugin_Manufacturer + "\n\n"
                        + juce::String::fromUTF8("GUI artwork: images generated with Meta AI (except the Doctor Migra\xc3\xb1" "a logo,\n"
                                                 "which is not AI-generated and is property of GuitarFros). Knob caps are rendered by code.\n\n")
                        + "Built with JUCE (AGPLv3). Source code: see the project page on GitHub.\n"
                        + "VST is a registered trademark of Steinberg Media Technologies GmbH.\n\n"
                        + "Inspired by classic pedal circuits (Rangemaster-style booster, MXR Dyna Comp-style\n"
                        + "compressor, Tube Screamer-style overdrive). Not affiliated with any of those brands.";
                      juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "About", text, "OK");
                    }
                  });
}

void DoctorMigranaEditor::showPresetMenu()
{
  const auto userNames = proc.getUserPresetNames();

  juce::PopupMenu m;
  m.addSectionHeader("Factory presets");
  for (int i = 0; i < DoctorMigranaProcessor::kNumFactoryPresets; ++i)
    m.addItem(1 + i, DoctorMigranaProcessor::getFactoryPresetName(i), true,
              proc.getPresetName() == DoctorMigranaProcessor::getFactoryPresetName(i));

  if (userNames.size() > 0)
  {
    m.addSeparator();
    m.addSectionHeader("User presets");
    for (int i = 0; i < userNames.size(); ++i)
      m.addItem(100 + i, userNames[i], true, proc.getPresetName() == userNames[i]);
  }

  m.addSeparator();
  m.addItem(1000, "Save preset as...");
  if (userNames.size() > 0)
  {
    juce::PopupMenu del;
    for (int i = 0; i < userNames.size(); ++i)
      del.addItem(2000 + i, userNames[i]);
    m.addSubMenu("Delete preset", del);
  }

  auto* procPtr = &proc;
  m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetBox.get()),
    [procPtr, userNames](int r)
    {
      if (r >= 1 && r <= DoctorMigranaProcessor::kNumFactoryPresets)
        procPtr->loadFactoryPreset(r - 1);
      else if (r >= 100 && r < 100 + userNames.size())
        procPtr->loadUserPreset(userNames[r - 100]);
      else if (r >= 2000 && r < 2000 + userNames.size())
        procPtr->deleteUserPreset(userNames[r - 2000]);
      else if (r == 1000)
      {
        auto* w = new juce::AlertWindow("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", procPtr->getPresetName());
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true,
          juce::ModalCallbackFunction::create([procPtr, w](int res)
          {
            if (res == 1)
            {
              const auto n = w->getTextEditorContents("name").trim();
              if (n.isNotEmpty()) procPtr->saveUserPreset(n);
            }
          }),
          true);
      }
    });
}

void DoctorMigranaEditor::showABMenu()
{
  juce::PopupMenu m;
  m.addSectionHeader("A/B");
  m.addItem(1, "Copy A to B");
  m.addItem(2, "Copy B to A");

  auto* procPtr = &proc;
  m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(abBox.get()),
                  [procPtr](int r)
                  {
                    if (r == 1) procPtr->abCopy(0, 1);
                    else if (r == 2) procPtr->abCopy(1, 0);
                  });
}
