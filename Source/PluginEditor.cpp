#include "PluginEditor.h"

VermaAudioProcessorEditor::VermaAudioProcessorEditor (VermaAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      master (p.apvts, "master", "MASTER"),
      oscPage (p), fxPage (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };

    for (auto* t : { &oscTab, &fxTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (1001);
    }
    oscTab.setToggleState (true, juce::dontSendNotification);
    oscTab.onClick = [this] { showPage (0); };
    fxTab.onClick = [this] { showPage (1); };

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd8e0ea));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff10151d));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a4556));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (VermaLookAndFeel::accent));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (VermaLookAndFeel::accent).withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (18.0f);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &oscTab, &fxTab, &master, &oscPage, &fxPage, &keyboard })
        addAndMakeVisible (c);
    fxPage.setVisible (false);

    setSize (1060, 744);
}

VermaAudioProcessorEditor::~VermaAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VermaAudioProcessorEditor::showPage (int idx)
{
    oscPage.setVisible (idx == 0);
    fxPage.setVisible (idx == 1);
}

void VermaAudioProcessorEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    const int next = ((proc.getCurrentProgram() + delta) % n + n) % n;
    presetBox.setSelectedId (next + 1, juce::sendNotificationSync);
}

void VermaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (VermaLookAndFeel::bg));

    auto header = getLocalBounds().removeFromTop (56).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141c29), 0.0f, 0.0f, juce::Colour (0xff0b0f16), 0.0f, header.getBottom(), false));
    g.fillRect (header);
    g.setColour (juce::Colour (VermaLookAndFeel::border));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());

    g.setGradientFill (juce::ColourGradient (juce::Colour (VermaLookAndFeel::accent), 18.0f, 0.0f,
                                             juce::Colour (VermaLookAndFeel::accent2), 160.0f, 0.0f, false));
    g.setFont (juce::Font (juce::FontOptions (32.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
    g.drawText ("VERMA", juce::Rectangle<int> (18, 6, 170, 36), juce::Justification::centredLeft);

    g.setColour (juce::Colour (VermaLookAndFeel::textDim));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("WAVETABLE SYNTH  by 0x.id", juce::Rectangle<int> (20, 38, 200, 14), juce::Justification::centredLeft);

    g.setColour (juce::Colour (VermaLookAndFeel::panel));
    g.fillRoundedRectangle (getLocalBounds().removeFromBottom (80).reduced (8, 6).toFloat(), 6.0f);
}

void VermaAudioProcessorEditor::resized()
{
    prevBtn.setBounds (250, 16, 26, 24);
    presetBox.setBounds (280, 16, 260, 24);
    nextBtn.setBounds (544, 16, 26, 24);
    oscTab.setBounds (620, 16, 64, 24);
    fxTab.setBounds (690, 16, 64, 24);
    master.setBounds (getWidth() - 74, 2, 60, 54);

    auto r = getLocalBounds();
    r.removeFromTop (64);
    auto kb = r.removeFromBottom (80).reduced (14, 12);
    r = r.reduced (8, 0);
    oscPage.setBounds (r);
    fxPage.setBounds (r);
    keyboard.setBounds (kb);
}
