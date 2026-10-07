#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class VermaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 bg       = 0xff0a0e15;
    static constexpr juce::uint32 panel    = 0xff131923;
    static constexpr juce::uint32 panelHi  = 0xff1a2230;
    static constexpr juce::uint32 border   = 0xff253043;
    static constexpr juce::uint32 accent   = 0xff35c9ff;
    static constexpr juce::uint32 accent2  = 0xff8f6bff;
    static constexpr juce::uint32 text     = 0xffcfdbe8;
    static constexpr juce::uint32 textDim  = 0xff6d7c8f;
    static constexpr juce::uint32 screen   = 0xff070a10;

    VermaLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (bg));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (screen));
        setColour (juce::ComboBox::textColourId, juce::Colour (accent));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (border));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (accent));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (panel));
        setColour (juce::PopupMenu::textColourId, juce::Colour (text));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (accent).withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, juce::Colour (text));
        setColour (juce::TextButton::buttonColourId, juce::Colour (panel));
        setColour (juce::TextButton::textColourOffId, juce::Colour (textDim));
        setColour (juce::TextButton::textColourOnId, juce::Colour (accent));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);
        const auto c = r.getCentre();
        const float radius = size * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        const float trackW = juce::jmax (2.5f, radius * 0.13f);
        const float arcR = radius - trackW;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xff1f2835));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool bip = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        auto acc = juce::Colour (accent);
        if (! s.isEnabled()) acc = acc.withSaturation (0.1f).withAlpha (0.4f);

        if (std::abs (ang - from) > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (acc.withAlpha (0.22f));
            g.strokePath (val, juce::PathStrokeType (trackW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (acc);
            g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float bodyR = arcR - trackW * 1.2f;
        juce::ColourGradient grad (juce::Colour (0xff334052), c.x, c.y - bodyR, juce::Colour (0xff10151d), c.x, c.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (juce::Colour (0xff06080c));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        juce::Path ptr;
        ptr.addRoundedRectangle (-1.4f, -bodyR + 2.0f, 2.8f, bodyR * 0.5f, 1.4f);
        ptr.applyTransform (juce::AffineTransform::rotation (ang).translated (c.x, c.y));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillPath (ptr);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto led = r.removeFromLeft (r.getHeight()).reduced (3.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colour (accent) : juce::Colour (0xff2a3444).brighter (hi ? 0.15f : 0.0f));
        g.fillRoundedRectangle (led, 3.0f);
        if (on)
        {
            g.setColour (juce::Colour (accent).withAlpha (0.3f));
            g.drawRoundedRectangle (led.expanded (1.5f), 4.0f, 2.0f);
        }
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? juce::Colour (text) : juce::Colour (textDim));
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
        }
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (screen));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (border));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path p;
        const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        p.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (juce::Colour (accent));
        g.fillPath (p);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override  { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override                 { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.5f, juce::Font::bold)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto fill = on ? juce::Colour (accent).withAlpha (0.16f) : juce::Colour (screen);
        if (hi) fill = fill.brighter (0.12f);
        if (down) fill = fill.brighter (0.2f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? juce::Colour (accent) : juce::Colour (border));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
};
