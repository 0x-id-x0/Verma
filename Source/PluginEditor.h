#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "VermaLookAndFeel.h"

using APVTS = juce::AudioProcessorValueTreeState;
using LNF = VermaLookAndFeel;

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& labelText) : name (labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity (180);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, juce::Colour (LNF::textDim));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        if (auto* p = state.getParameter (paramId))
        {
            if (p->getNormalisableRange().start < 0.0f) slider.getProperties().set ("bipolar", true);
            slider.setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        }

        slider.addMouseListener (this, false);
        slider.onValueChange = [this] { if (showingValue) showValue(); };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (14));
        slider.setBounds (r);
    }

    void mouseEnter (const juce::MouseEvent&) override { showingValue = true; showValue(); }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (slider.isMouseButtonDown()) return;
        showingValue = false;
        label.setText (name, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, juce::Colour (LNF::textDim));
    }
    void mouseUp (const juce::MouseEvent& e) override { if (! slider.isMouseOver()) mouseExit (e); }

    juce::Slider slider;
    juce::Label label;

private:
    juce::String name;
    bool showingValue = false;
    std::unique_ptr<APVTS::SliderAttachment> attachment;

    void showValue()
    {
        label.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, juce::Colour (LNF::accent));
    }
};

class Toggle : public juce::ToggleButton
{
public:
    Toggle (APVTS& s, const juce::String& id, const juce::String& text = {}) : juce::ToggleButton (text)
    {
        attachment = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

class Choice : public juce::ComboBox
{
public:
    Choice (APVTS& s, const juce::String& id)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            addItemList (c->choices, 1);
        setJustificationType (juce::Justification::centredLeft);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, id, *this);
    }
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

//==============================================================================
class Section : public juce::Component
{
public:
    explicit Section (juce::String t) : title (std::move (t)) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (LNF::panel));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (LNF::panelHi));
        g.fillRoundedRectangle (r.withHeight (24.0f), 6.0f);
        g.fillRect (r.withTop (r.getY() + 18.0f).withHeight (6.0f));
        g.setColour (juce::Colour (LNF::border));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.drawHorizontalLine (24, r.getX(), r.getRight());
        g.setColour (juce::Colour (LNF::text));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (title, juce::Rectangle<int> (titleX, 0, 220, 24), juce::Justification::centredLeft);
    }

protected:
    juce::String title;
    int titleX = 10;

    void add (std::initializer_list<juce::Component*> comps) { for (auto* c : comps) addAndMakeVisible (c); }
};

//==============================================================================
class Screen : public juce::Component, private juce::Timer
{
public:
    Screen() { startTimerHz (30); }
protected:
    void paintScreen (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LNF::screen));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff141b26));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getWidth() * (float) i / 8.0f), 2.0f, r.getBottom() - 2.0f);
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getHeight() * (float) i / 4.0f), 2.0f, r.getRight() - 2.0f);
    }
    void paintBorder (juce::Graphics& g)
    {
        g.setColour (juce::Colour (LNF::border));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
    }
private:
    void timerCallback() override { repaint(); }
};

class WavetableDisplay : public Screen
{
public:
    WavetableDisplay (VermaAudioProcessor& p, const juce::String& px)
        : bank (p.getBank()), on (p.apvts.getRawParameterValue (px + "On")),
          table (p.apvts.getRawParameterValue (px + "Table")), pos (p.apvts.getRawParameterValue (px + "Pos")) {}

    void paint (juce::Graphics& g) override
    {
        paintScreen (g);
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const bool active = on->load() > 0.5f;
        const int tb = juce::jlimit (0, verma::kNumTables - 1, (int) table->load());
        const float ps = pos->load();
        const float W = r.getWidth() * 0.74f;
        const float H = r.getHeight() * 0.52f;

        auto framePath = [&] (int f0, int f1, float frac, float depth)
        {
            const float* a = bank.getFrame (tb, 0, f0);
            const float* b = bank.getFrame (tb, 0, f1);
            const float x0 = r.getX() + depth * (r.getWidth() - W);
            const float yc = r.getY() + r.getHeight() * 0.70f - depth * r.getHeight() * 0.40f;
            juce::Path p;
            const int pts = 128;
            for (int i = 0; i <= pts; ++i)
            {
                const int idx = juce::jmin (verma::kTableSize, i * verma::kTableSize / pts);
                const float s = a[idx] + (b[idx] - a[idx]) * frac;
                const float x = x0 + W * (float) i / (float) pts;
                const float y = yc - s * H * 0.5f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };

        auto acc = juce::Colour (active ? LNF::accent : LNF::textDim);
        for (int f = verma::kFrames - 1; f >= 0; f -= 2)
        {
            const float depth = (float) f / (float) (verma::kFrames - 1);
            g.setColour (acc.withAlpha (0.10f + 0.08f * (1.0f - depth)));
            g.strokePath (framePath (f, f, 0.0f, depth), juce::PathStrokeType (1.0f));
        }

        const float fpos = ps * (float) (verma::kFrames - 1);
        const int f0 = juce::jlimit (0, verma::kFrames - 1, (int) fpos);
        const int f1 = juce::jmin (f0 + 1, verma::kFrames - 1);
        auto cur = framePath (f0, f1, fpos - (float) f0, ps);
        g.setColour (acc.withAlpha (0.25f));
        g.strokePath (cur, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (active ? juce::Colours::white.interpolatedWith (acc, 0.35f) : acc);
        g.strokePath (cur, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (LNF::textDim));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("FRAME " + juce::String (juce::roundToInt (fpos) + 1) + "/" + juce::String (verma::kFrames),
                    getLocalBounds().reduced (6, 4), juce::Justification::topRight);
        paintBorder (g);
    }

private:
    const verma::WavetableBank& bank;
    std::atomic<float>* on;
    std::atomic<float>* table;
    std::atomic<float>* pos;
};

class FilterDisplay : public Screen
{
public:
    explicit FilterDisplay (APVTS& s)
        : on (s.getRawParameterValue ("fOn")), type (s.getRawParameterValue ("fType")),
          cutoff (s.getRawParameterValue ("fCutoff")), res (s.getRawParameterValue ("fRes")) {}

    void paint (juce::Graphics& g) override
    {
        paintScreen (g);
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        const bool active = on->load() > 0.5f;
        const int t = (int) type->load();
        const float fc = cutoff->load();
        const float rr = res->load();
        const float q = 0.6f + rr * rr * 11.0f;

        juce::Path p;
        const int pts = 160;
        for (int i = 0; i <= pts; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / (float) pts);
            float mag = 1.0f;
            if (active)
            {
                const float w = f / fc;
                const float den = std::sqrt ((1.0f - w * w) * (1.0f - w * w) + (w / q) * (w / q));
                if (t == 0 || t == 1) mag = 1.0f / den;
                else if (t == 2) mag = (w * w) / den;
                else mag = (w / q) / den;
                if (t == 1) mag *= 1.0f / den;
            }
            const float db = juce::jlimit (-48.0f, 24.0f, 20.0f * std::log10 (juce::jmax (1.0e-6f, mag)));
            const float x = r.getX() + r.getWidth() * (float) i / (float) pts;
            const float y = juce::jmap (db, 24.0f, -48.0f, r.getY(), r.getBottom());
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        auto acc = juce::Colour (active ? LNF::accent : LNF::textDim);
        juce::Path fill (p);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.28f), 0.0f, r.getY(), acc.withAlpha (0.0f), 0.0f, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (acc);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        paintBorder (g);
    }

private:
    std::atomic<float>* on;
    std::atomic<float>* type;
    std::atomic<float>* cutoff;
    std::atomic<float>* res;
};

class EnvDisplay : public Screen
{
public:
    EnvDisplay (APVTS& s, const juce::String& px)
        : a (s.getRawParameterValue (px + "A")), d (s.getRawParameterValue (px + "D")),
          su (s.getRawParameterValue (px + "S")), rl (s.getRawParameterValue (px + "R")) {}

    void paint (juce::Graphics& g) override
    {
        paintScreen (g);
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const float wa = std::sqrt (a->load()), wd = std::sqrt (d->load()), wr = std::sqrt (rl->load());
        const float ws = 0.35f * (wa + wd + wr) + 0.2f;
        const float scale = r.getWidth() / (wa + wd + ws + wr);
        const float x0 = r.getX(), xA = x0 + wa * scale, xD = xA + wd * scale, xS = xD + ws * scale, xR = xS + wr * scale;
        const float yB = r.getBottom(), yT = r.getY(), yS = yB - su->load() * r.getHeight();

        juce::Path p;
        p.startNewSubPath (x0, yB);
        p.lineTo (xA, yT);
        p.quadraticTo (xA + (xD - xA) * 0.15f, yS, xD, yS);
        p.lineTo (xS, yS);
        p.quadraticTo (xS + (xR - xS) * 0.15f, yB, xR, yB);

        auto acc = juce::Colour (LNF::accent);
        juce::Path fill (p);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (acc.withAlpha (0.3f), 0.0f, yT, acc.withAlpha (0.0f), 0.0f, yB, false));
        g.fillPath (fill);
        g.setColour (acc);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        g.setColour (juce::Colours::white);
        for (auto pt : { juce::Point<float> (xA, yT), juce::Point<float> (xD, yS), juce::Point<float> (xS, yS) })
            g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
        paintBorder (g);
    }

private:
    std::atomic<float>* a;
    std::atomic<float>* d;
    std::atomic<float>* su;
    std::atomic<float>* rl;
};

class LfoDisplay : public Screen
{
public:
    LfoDisplay (APVTS& s, const juce::String& px)
        : shape (s.getRawParameterValue (px + "Shape")), depth (s.getRawParameterValue (px + "Depth")),
          target (s.getRawParameterValue (px + "Target")) {}

    void paint (juce::Graphics& g) override
    {
        paintScreen (g);
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const int sh = (int) shape->load();
        const float amp = juce::jmax (0.06f, depth->load());
        const bool active = (int) target->load() != 0;
        static const float steps[8] = { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, 0.5f, -0.6f, -0.1f };

        juce::Path p;
        const int pts = 200;
        for (int i = 0; i <= pts; ++i)
        {
            const float ph = (float) i / (float) pts;
            float v;
            switch (sh)
            {
                case 0:  v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
                case 1:  v = -(1.0f - 4.0f * std::abs (ph - 0.5f)); break;
                case 2:  v = 1.0f - 2.0f * ph; break;
                case 3:  v = ph < 0.5f ? 1.0f : -1.0f; break;
                default: v = steps[juce::jmin (7, (int) (ph * 8.0f))]; break;
            }
            const float x = r.getX() + r.getWidth() * ph;
            const float y = r.getCentreY() - v * amp * r.getHeight() * 0.46f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        auto acc = juce::Colour (active ? LNF::accent2 : LNF::textDim);
        g.setColour (acc.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (acc);
        g.strokePath (p, juce::PathStrokeType (2.0f));
        paintBorder (g);
    }

private:
    std::atomic<float>* shape;
    std::atomic<float>* depth;
    std::atomic<float>* target;
};

class Scope : public Screen
{
public:
    explicit Scope (VermaAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        paintScreen (g);
        constexpr int size = VermaAudioProcessor::scopeSize;
        const int wp = proc.scopeWritePos.load();
        for (int i = 0; i < size; ++i)
            tmp[i] = proc.scopeBuffer[(wp + i) % size];

        int start = 0;
        for (int i = 1; i < size / 2; ++i)
            if (tmp[i - 1] < 0.0f && tmp[i] >= 0.0f) { start = i; break; }

        auto r = getLocalBounds().toFloat().reduced (4.0f);
        juce::Path p;
        const int count = size / 2;
        for (int i = 0; i < count; i += 2)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) count;
            const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, tmp[start + i]) * r.getHeight() * 0.45f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        auto acc = juce::Colour (LNF::accent);
        g.setColour (acc.withAlpha (0.2f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setColour (acc);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        g.setColour (juce::Colour (LNF::textDim));
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.drawText ("OUTPUT", getLocalBounds().reduced (8, 4), juce::Justification::topLeft);
        paintBorder (g);
    }

private:
    VermaAudioProcessor& proc;
    float tmp[VermaAudioProcessor::scopeSize] {};
};

//==============================================================================
class OscSection : public Section
{
public:
    OscSection (VermaAudioProcessor& p, const juce::String& px, const juce::String& t)
        : Section (t), onBtn (p.apvts, px + "On"), table (p.apvts, px + "Table"), display (p, px),
          pos (p.apvts, px + "Pos", "WT POS"), level (p.apvts, px + "Level", "LEVEL"), pan (p.apvts, px + "Pan", "PAN"),
          oct (p.apvts, px + "Oct", "OCT"), semi (p.apvts, px + "Semi", "SEMI"), fine (p.apvts, px + "Fine", "FINE"),
          unison (p.apvts, px + "Unison", "UNISON"), detune (p.apvts, px + "Detune", "DETUNE"), blend (p.apvts, px + "Blend", "BLEND")
    {
        titleX = 34;
        add ({ &onBtn, &table, &display, &pos, &level, &pan, &oct, &semi, &fine, &unison, &detune, &blend });
    }

    void resized() override
    {
        auto r = getLocalBounds();
        onBtn.setBounds (8, 2, 22, 22);
        table.setBounds (r.getWidth() - 178, 3, 170, 19);
        const int dw = (int) ((float) r.getWidth() * 0.5f);
        display.setBounds (10, 32, dw - 10, 112);
        const int kw = (dw - 10) / 3;
        pos.setBounds (10, 148, kw, 58);
        level.setBounds (10 + kw, 148, kw, 58);
        pan.setBounds (10 + 2 * kw, 148, kw, 58);
        const int gx = dw + 8;
        const int gw = (r.getWidth() - gx - 8) / 3;
        Knob* grid[6] = { &oct, &semi, &fine, &unison, &detune, &blend };
        for (int i = 0; i < 6; ++i)
            grid[i]->setBounds (gx + (i % 3) * gw, 34 + (i / 3) * 86, gw, 80);
    }

private:
    Toggle onBtn;
    Choice table;
    WavetableDisplay display;
    Knob pos, level, pan, oct, semi, fine, unison, detune, blend;
};

class SubNoiseSection : public Section
{
public:
    explicit SubNoiseSection (APVTS& s)
        : Section ("SUB / NOISE"), subOn (s, "subOn"), shape (s, "subShape"),
          oct (s, "subOct", "OCT"), level (s, "subLevel", "SUB"), noise (s, "noiseLevel", "NOISE")
    {
        titleX = 34;
        add ({ &subOn, &shape, &oct, &level, &noise });
    }
    void resized() override
    {
        subOn.setBounds (8, 2, 22, 22);
        shape.setBounds (getWidth() - 128, 3, 120, 19);
        const int w = (getWidth() - 20) / 3;
        oct.setBounds (10, 28, w, getHeight() - 32);
        level.setBounds (10 + w, 28, w, getHeight() - 32);
        noise.setBounds (10 + 2 * w, 28, w, getHeight() - 32);
    }
private:
    Toggle subOn;
    Choice shape;
    Knob oct, level, noise;
};

class FilterSection : public Section
{
public:
    explicit FilterSection (APVTS& s)
        : Section ("FILTER"), onBtn (s, "fOn"), type (s, "fType"), display (s),
          cutoff (s, "fCutoff", "CUTOFF"), res (s, "fRes", "RES"), drive (s, "fDrive", "DRIVE"),
          env (s, "fEnv", "ENV 2"), key (s, "fKey", "KEY")
    {
        titleX = 34;
        add ({ &onBtn, &type, &display, &cutoff, &res, &drive, &env, &key });
    }
    void resized() override
    {
        onBtn.setBounds (8, 2, 22, 22);
        type.setBounds (getWidth() - 128, 3, 120, 19);
        display.setBounds (10, 32, getWidth() - 20, 96);
        const int w = (getWidth() - 20) / 5;
        Knob* ks[5] = { &cutoff, &res, &drive, &env, &key };
        for (int i = 0; i < 5; ++i) ks[i]->setBounds (10 + i * w, 134, w, getHeight() - 140);
    }
private:
    Toggle onBtn;
    Choice type;
    FilterDisplay display;
    Knob cutoff, res, drive, env, key;
};

class EnvSection : public Section
{
public:
    EnvSection (APVTS& s, const juce::String& px, const juce::String& t)
        : Section (t), display (s, px), a (s, px + "A", "A"), d (s, px + "D", "D"), su (s, px + "S", "S"), r (s, px + "R", "R")
    {
        add ({ &display, &a, &d, &su, &r });
    }
    void resized() override
    {
        display.setBounds (10, 32, getWidth() - 20, 58);
        const int w = (getWidth() - 20) / 4;
        Knob* ks[4] = { &a, &d, &su, &r };
        for (int i = 0; i < 4; ++i) ks[i]->setBounds (10 + i * w, 94, w, getHeight() - 98);
    }
private:
    EnvDisplay display;
    Knob a, d, su, r;
};

class LfoSection : public Section
{
public:
    LfoSection (APVTS& s, const juce::String& px, const juce::String& t)
        : Section (t), shape (s, px + "Shape"), target (s, px + "Target"), display (s, px),
          rate (s, px + "Rate", "RATE"), depth (s, px + "Depth", "DEPTH")
    {
        add ({ &shape, &target, &display, &rate, &depth });
    }
    void resized() override
    {
        shape.setBounds (getWidth() - 98, 3, 90, 19);
        display.setBounds (10, 32, getWidth() - 20, 58);
        const int w = (getWidth() - 20) / 4;
        rate.setBounds (10, 94, w, getHeight() - 98);
        depth.setBounds (10 + w, 94, w, getHeight() - 98);
        target.setBounds (10 + 2 * w + 4, 112, 2 * w - 8, 22);
    }
    void paint (juce::Graphics& g) override
    {
        Section::paint (g);
        g.setColour (juce::Colour (LNF::textDim));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("DESTINATION", target.getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);
    }
private:
    Choice shape, target;
    LfoDisplay display;
    Knob rate, depth;
};

class FxSection : public Section
{
public:
    FxSection (APVTS& s, const juce::String& t, const juce::String& onId,
               std::initializer_list<std::pair<const char*, const char*>> knobs)
        : Section (t), onBtn (s, onId)
    {
        titleX = 34;
        addAndMakeVisible (onBtn);
        for (auto& k : knobs)
        {
            auto* kn = items.add (new Knob (s, k.first, k.second));
            addAndMakeVisible (kn);
        }
    }
    void resized() override
    {
        onBtn.setBounds (8, 2, 22, 22);
        const int cols = 2;
        const int w = (getWidth() - 20) / cols;
        for (int i = 0; i < items.size(); ++i)
            items[i]->setBounds (10 + (i % cols) * w, 34 + (i / cols) * 92, w, 86);
    }
private:
    Toggle onBtn;
    juce::OwnedArray<Knob> items;
};

//==============================================================================
class OscPage : public juce::Component
{
public:
    explicit OscPage (VermaAudioProcessor& p)
        : oscA (p, "a", "OSC A"), oscB (p, "b", "OSC B"), subNoise (p.apvts), filter (p.apvts), scope (p),
          env1 (p.apvts, "e1", "ENV 1  (AMP)"), env2 (p.apvts, "e2", "ENV 2  (FILTER)"),
          lfo1 (p.apvts, "l1", "LFO 1"), lfo2 (p.apvts, "l2", "LFO 2")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &oscA, &oscB, &subNoise, &filter, &scope, &env1, &env2, &lfo1, &lfo2 })
            addAndMakeVisible (c);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto mod = r.removeFromBottom (160);
        r.removeFromBottom (8);
        auto left = r.removeFromLeft (540);
        r.removeFromLeft (8);
        oscA.setBounds (left.removeFromTop ((left.getHeight() - 8) / 2));
        left.removeFromTop (8);
        oscB.setBounds (left);
        subNoise.setBounds (r.removeFromTop (100));
        r.removeFromTop (8);
        filter.setBounds (r.removeFromTop (212));
        r.removeFromTop (8);
        scope.setBounds (r);
        const int w = (mod.getWidth() - 24) / 4;
        juce::Component* ms[4] = { &env1, &env2, &lfo1, &lfo2 };
        for (int i = 0; i < 4; ++i) { ms[i]->setBounds (mod.removeFromLeft (w)); mod.removeFromLeft (8); }
    }
private:
    OscSection oscA, oscB;
    SubNoiseSection subNoise;
    FilterSection filter;
    Scope scope;
    EnvSection env1, env2;
    LfoSection lfo1, lfo2;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (VermaAudioProcessor& p)
        : dist (p.apvts, "DISTORTION", "distOn", { { "distDrive", "DRIVE" }, { "distMix", "MIX" } }),
          chorus (p.apvts, "CHORUS", "chOn", { { "chRate", "RATE" }, { "chDepth", "DEPTH" }, { "chMix", "MIX" } }),
          delay (p.apvts, "DELAY", "dlOn", { { "dlTime", "TIME" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" } }),
          reverb (p.apvts, "REVERB", "rvOn", { { "rvSize", "SIZE" }, { "rvDamp", "DAMP" }, { "rvWidth", "WIDTH" }, { "rvMix", "MIX" } }),
          scope (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &dist, &chorus, &delay, &reverb, &scope })
            addAndMakeVisible (c);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (226);
        r.removeFromTop (8);
        const int w = (top.getWidth() - 24) / 4;
        juce::Component* fx[4] = { &dist, &chorus, &delay, &reverb };
        for (int i = 0; i < 4; ++i) { fx[i]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8); }
        scope.setBounds (r);
    }
private:
    FxSection dist, chorus, delay, reverb;
    Scope scope;
};

//==============================================================================
class VermaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VermaAudioProcessorEditor (VermaAudioProcessor&);
    ~VermaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VermaLookAndFeel lnf;
    VermaAudioProcessor& proc;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, oscTab { "OSC" }, fxTab { "FX" };
    Knob master;
    OscPage oscPage;
    FxPage fxPage;
    juce::MidiKeyboardComponent keyboard;

    void showPage (int idx);
    void stepPreset (int delta);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VermaAudioProcessorEditor)
};
