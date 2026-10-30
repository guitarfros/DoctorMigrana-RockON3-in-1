// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GuitarFros - Doctor Migrana - RockON 3-in-1
#pragma once
#include "PluginProcessor.h"

// Design space = the 1920x1280 background image. Everything is scaled from it.

// Knob: the cap, its shadow and the printed 0-10 scale are part of the background image (fixed lighting);
// only the pointer line is drawn here, rotated according to the parameter value.
class PointerKnob : public juce::Slider
{
public:
  PointerKnob()
  {
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paint(juce::Graphics& g) override
  {
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float Rk = b.getWidth() * (58.0f / 180.0f);          // knob radius (design: 58 px at scale 1)
    const float prop = (float) valueToProportionOfLength(getValue());
    const float angle = juce::degreesToRadians(-135.0f + prop * 270.0f);

    const float w = Rk * 0.085f;                                // pointer: from 0.18 R to 0.66 R, on top of the cap
    juce::Path p;
    p.addRoundedRectangle(-w * 0.5f, -Rk * 0.66f, w, Rk * 0.48f, w * 0.5f);
    const auto t = juce::AffineTransform::rotation(angle).translated(cx, cy);

    g.setColour(juce::Colours::white.withAlpha(0.35f));          // thin highlight under the inlay
    g.fillPath(p, t.translated(w * 0.25f, w * 0.35f));
    g.setColour(juce::Colour(30, 24, 18));
    g.fillPath(p, t);
  }
};

// Footswitch: invisible button over the pedal + LED drawn above it.
class PedalButton : public juce::ToggleButton
{
public:
  explicit PedalButton(juce::Colour ledColour) : colour(ledColour)
  {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool, bool down) override
  {
    const float s = getWidth() / 140.0f;           // design width of this component = 140
    const juce::Point<float> led(70.0f * s, 29.0f * s);   // LED centre (design y 909 - top 880)
    const bool on = getToggleState();

    if (on)
    {
      juce::ColourGradient glow(colour.withAlpha(0.55f), led.x, led.y,
                                colour.withAlpha(0.0f), led.x + 42.0f * s, led.y, true);
      g.setGradientFill(glow);
      g.fillEllipse(led.x - 42.0f * s, led.y - 42.0f * s, 84.0f * s, 84.0f * s);
      juce::ColourGradient core(juce::Colours::white.interpolatedWith(colour, 0.35f), led.x, led.y,
                                colour, led.x + 14.0f * s, led.y, true);
      g.setGradientFill(core);
    }
    else
    {
      g.setColour(colour.darker(0.9f).withMultipliedBrightness(0.45f));
    }
    g.fillEllipse(led.x - 13.0f * s, led.y - 13.0f * s, 26.0f * s, 26.0f * s);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(led.x - 13.0f * s, led.y - 13.0f * s, 26.0f * s, 26.0f * s, 1.5f * s);

    if (down)   // pressed feedback on the pedal
    {
      g.setColour(juce::Colours::black.withAlpha(0.28f));
      const float cy = (996.0f - 880.0f) * s;
      g.fillEllipse(70.0f * s - 64.0f * s, cy - 64.0f * s, 128.0f * s, 128.0f * s);
    }
  }

private:
  juce::Colour colour;
};

// Bypass: lever sprite (on/off) + ON LED + label. Toggle state = bypassed.
class BypassSwitch : public juce::ToggleButton
{
public:
  BypassSwitch(juce::Image leverOn, juce::Image leverOff)
  : on(std::move(leverOn)), off(std::move(leverOff))
  {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool, bool) override
  {
    const float s = getWidth() / 140.0f;      // design area: x 296..436, y 355..590
    const bool bypassed = getToggleState();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // ON LED at design (364,377). The LED bezel and the "ON" / "BYPASS" labels are baked in the background;
    // only the light is drawn here (lit while the plugin is active).
    if (!bypassed)
    {
      const juce::Point<float> led(68.0f * s, 22.0f * s);
      const juce::Colour c(255, 40, 30);
      juce::ColourGradient glow(c.withAlpha(0.6f), led.x, led.y, c.withAlpha(0.0f), led.x + 42.0f * s, led.y, true);
      g.setGradientFill(glow);
      g.fillEllipse(led.x - 42.0f * s, led.y - 42.0f * s, 84.0f * s, 84.0f * s);
      juce::ColourGradient core(juce::Colour(255, 200, 180), led.x, led.y, c, led.x + 12.0f * s, led.y, true);
      g.setGradientFill(core);
      g.fillEllipse(led.x - 11.0f * s, led.y - 11.0f * s, 22.0f * s, 22.0f * s);
    }

    // lever: design rect (306,452)-(430,535) -> local (10,97), 124x83
    g.drawImage(bypassed ? off : on, juce::Rectangle<float>(10.0f * s, 97.0f * s, 124.0f * s, 83.0f * s));
  }

private:
  juce::Image on, off;
};

// Small toggle switch (MODE / SERIES): lever sprite up/down + LED. State true = up = LED on.
class ToggleSwitch : public juce::ToggleButton
{
public:
  ToggleSwitch(juce::Image upImg, juce::Image downImg) : up(std::move(upImg)), down(std::move(downImg))
  {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
  }

  void paintButton(juce::Graphics& g, bool, bool) override
  {
    const float s = getWidth() / 140.0f;     // design area: width 140, height 90, origin (298, cy-45)
    const bool isUp = getToggleState();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    if (isUp)
    {
      const juce::Point<float> led(24.0f * s, 41.0f * s);
      const juce::Colour c(255, 40, 30);
      juce::ColourGradient glow(c.withAlpha(0.55f), led.x, led.y, c.withAlpha(0.0f), led.x + 34.0f * s, led.y, true);
      g.setGradientFill(glow);
      g.fillEllipse(led.x - 34.0f * s, led.y - 34.0f * s, 68.0f * s, 68.0f * s);
      juce::ColourGradient core(juce::Colour(255, 190, 170), led.x, led.y, c, led.x + 13.0f * s, led.y, true);
      g.setGradientFill(core);
      g.fillEllipse(led.x - 12.0f * s, led.y - 12.0f * s, 24.0f * s, 24.0f * s);
    }

    // lever: design rect 76x72, placed at local (57, 9)
    g.drawImage(isUp ? up : down, juce::Rectangle<float>(57.0f * s, 9.0f * s, 76.0f * s, 72.0f * s));
  }

private:
  juce::Image up, down;
};

// Invisible gear button (top-right): opens the settings menu.
class GearButton : public juce::Component
{
public:
  std::function<void()> onClick;

  GearButton() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

  void paint(juce::Graphics& g) override
  {
    if (hover)
    {
      g.setColour(juce::Colour(235, 220, 170).withAlpha(0.45f));
      g.drawEllipse(getLocalBounds().toFloat().reduced(3.0f), 2.0f);
    }
  }
  void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
  void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
  void mouseUp(const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }

private:
  bool hover = false;
};

// Preset selector: draws the current preset name over the patched box; click opens the menu.
class PresetBox : public juce::Component, private juce::Timer
{
public:
  std::function<juce::String()> nameSource;
  std::function<void()> onClick;

  PresetBox() { setMouseCursor(juce::MouseCursor::PointingHandCursor); startTimerHz(5); }

  void paint(juce::Graphics& g) override
  {
    const float s = getWidth() / 355.0f;    // design area: x 1302..1657, y 172..217
    if (hover)
    {
      g.setColour(juce::Colours::white.withAlpha(0.06f));
      g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f * s), 8.0f * s);
    }

    g.setColour(juce::Colour(232, 232, 232));
    g.setFont(juce::FontOptions(26.0f * s, juce::Font::bold));
    g.drawFittedText(name, juce::Rectangle<int>((int) (105.0f * s), (int) (9.0f * s), (int) (176.0f * s), (int) (31.0f * s)),
                     juce::Justification::centredLeft, 1, 0.7f);

    juce::Path tri;     // little down triangle, design x 1588..1606, y 188..203
    tri.addTriangle(286.0f * s, 16.0f * s, 304.0f * s, 16.0f * s, 295.0f * s, 31.0f * s);
    g.setColour(juce::Colour(190, 190, 190));
    g.fillPath(tri);
  }

  void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
  void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
  void mouseUp(const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }

private:
  void timerCallback() override
  {
    if (!nameSource) return;
    const auto n = nameSource();
    if (n != name) { name = n; repaint(); }
  }

  juce::String name;
  bool hover = false;
};

// A/B box: left part toggles A<->B (active letter highlighted), right part (chevron) opens a copy menu.
class ABBox : public juce::Component, private juce::Timer
{
public:
  std::function<int()> activeSource;
  std::function<void()> onToggle;
  std::function<void()> onMenu;

  ABBox() { setMouseCursor(juce::MouseCursor::PointingHandCursor); startTimerHz(10); }

  void paint(juce::Graphics& g) override
  {
    const float s = getWidth() / 140.0f;     // design area: x 1517..1657, y 116..161
    const float split = 63.0f * s;           // left button width (design x 1517..1580)

    if (hoverLeft)
    {
      g.setColour(juce::Colours::white.withAlpha(0.08f));
      g.fillRoundedRectangle(2.0f * s, 2.0f * s, split - 3.0f * s, getHeight() - 4.0f * s, 8.0f * s);
    }
    else if (hoverRight)
    {
      g.setColour(juce::Colours::white.withAlpha(0.06f));
      g.fillRoundedRectangle(split + 2.0f * s, 2.0f * s, getWidth() - split - 4.0f * s, getHeight() - 4.0f * s, 8.0f * s);
    }

    g.setFont(juce::FontOptions(30.0f * s, juce::Font::bold));
    const juce::Colour on(255, 235, 190), off(130, 130, 130);
    auto cell = [&](const juce::String& t, float cx, juce::Colour col)
    {
      g.setColour(col);
      g.drawText(t, juce::Rectangle<float>((cx - 10.0f) * s, 6.0f * s, 20.0f * s, 34.0f * s), juce::Justification::centred);
    };
    cell("A", 21.0f, active == 0 ? on : off);
    cell("/", 33.0f, off);
    cell("B", 45.0f, active == 1 ? on : off);
  }

  void mouseMove(const juce::MouseEvent& e) override { updateHover(e); }
  void mouseEnter(const juce::MouseEvent& e) override { updateHover(e); }
  void mouseExit(const juce::MouseEvent&) override { hoverLeft = hoverRight = false; repaint(); }
  void mouseUp(const juce::MouseEvent& e) override
  {
    if (!e.mouseWasClicked()) return;
    if (e.x < splitX()) { if (onToggle) onToggle(); }
    else                { if (onMenu) onMenu(); }
  }

private:
  int splitX() const { return (int) (63.0f * getWidth() / 140.0f); }
  void updateHover(const juce::MouseEvent& e)
  {
    const bool left = e.x < splitX();
    if (left != hoverLeft || left == hoverRight) { hoverLeft = left; hoverRight = !left; repaint(); }
  }
  void timerCallback() override
  {
    if (!activeSource) return;
    const int a = activeSource();
    if (a != active) { active = a; repaint(); }
  }

  int active = 0;
  bool hoverLeft = false, hoverRight = false;
};

// LED level meter (14 LEDs) with a dB read-out.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
  LevelMeter(std::atomic<float>& src, juce::String title, bool alignRight)
  : source(src), name(std::move(title)), right(alignRight)
  {
    startTimerHz(30);
  }

  void paint(juce::Graphics& g) override
  {
    const float s = getWidth() / 230.0f;     // design width of this component = 230
    const float db = juce::Decibels::gainToDecibels(level, -60.0f);

    g.setColour(juce::Colour(205, 195, 180));
    g.setFont(juce::FontOptions(26.0f * s, juce::Font::bold));
    g.drawText(name + " " + juce::String((int) std::round(db)) + "dB",
               juce::Rectangle<float>(4.0f * s, 4.0f * s, 222.0f * s, 30.0f * s),
               right ? juce::Justification::centredRight : juce::Justification::centredLeft);

    const int n = 14;
    const float y = 44.0f * s;
    for (int i = 0; i < n; ++i)
    {
      const float thr = -42.0f + (float) i * 3.5f;
      const bool lit = db >= thr;
      const juce::Colour base = i < 9 ? juce::Colour(150, 255, 60)
                              : i < 12 ? juce::Colour(255, 220, 40)
                                       : juce::Colour(255, 50, 40);
      const float x = (12.0f + (float) i * 15.8f) * s;
      const float r = 4.6f * s;
      if (lit)
      {
        g.setColour(base.withAlpha(0.28f));
        g.fillEllipse(x - r * 2.2f, y - r * 2.2f, r * 4.4f, r * 4.4f);
        g.setColour(base);
      }
      else g.setColour(base.darker(0.95f).withAlpha(0.75f));
      g.fillEllipse(x - r, y - r, r * 2.0f, r * 2.0f);
    }
  }

private:
  void timerCallback() override
  {
    const float v = source.load();
    level = juce::jmax(v, level * 0.82f);
    repaint();
  }

  std::atomic<float>& source;
  juce::String name;
  bool right;
  float level = 0.0f;
};

class DoctorMigranaEditor : public juce::AudioProcessorEditor
{
public:
  explicit DoctorMigranaEditor(DoctorMigranaProcessor&);
  ~DoctorMigranaEditor() override = default;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void showSettingsMenu();
  void showPresetMenu();
  void showABMenu();

  using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

  DoctorMigranaProcessor& proc;
  juce::Image background;

  std::unique_ptr<PointerKnob> knobs[9];
  std::unique_ptr<PedalButton> pedals[3];
  std::unique_ptr<BypassSwitch> bypass;
  std::unique_ptr<ToggleSwitch> compLinkSw, driveLinkSw;
  std::unique_ptr<LevelMeter> inMeter, outMeter;
  std::unique_ptr<GearButton> gear;
  std::unique_ptr<PresetBox> presetBox;
  std::unique_ptr<ABBox> abBox;

  std::unique_ptr<SliderAtt> sliderAtts[9];
  std::unique_ptr<ButtonAtt> buttonAtts[6];

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DoctorMigranaEditor)
};
