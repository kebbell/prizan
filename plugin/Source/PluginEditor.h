#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace prizan::ui
{
namespace colours
{
    const juce::Colour page { 0xffe4ebe5 }, panel { 0xffffffff }, line { 0xffd6dfd8 }, ink { 0xff16241b },
                       muted { 0xff6a7b70 }, green { 0xff1fae5b }, greenDeep { 0xff15824a }, track { 0xff183a26 },
                       screen { 0xff0d2217 }, screenHi { 0xff14301f }, wave { 0xff5ef29a }, bar { 0xfff5f8f5 };
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

// Pitch snaps to whole semitones; hold Shift for cents.
struct PitchSlider : juce::Slider
{
    double snapValue (double v, DragMode) override
    {
        return juce::ModifierKeys::currentModifiers.isShiftDown() ? std::round (v * 100.0) / 100.0 : std::round (v);
    }
};

// The screen: waveform, envelope, hand-drawn points, stretch grip.
class Display : public juce::Component
{
public:
    explicit Display (PrizanProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refreshPoints();
    bool dropHighlight = false;

private:
    struct Geo { float W, top, bot, mid, amp; double T; };
    Geo geo() const;
    prizan::Params params() const;
    int hitPoint (juce::Point<float>) const;
    void movePoint (int i, juce::Point<float>);
    void push();
    juce::Rectangle<float> gripArea() const;

    PrizanProcessor& proc;
    std::vector<prizan::EnvPoint> pts;
    float endBend = 0;
    int seenVersion = -1, hot = -1;
    enum class Drag { none, point, bend, grip } drag = Drag::none;
    int dragIndex = -1;
    float bendStart = 0, bendDir = 1, dragStartY = 0, gripStartX = 0, gripStartValue = 1;
    juce::Rectangle<float> clearArea;
};

// A tile you drag straight into the DAW; it renders the current sound to a WAV first.
class DragOut : public juce::Component
{
public:
    explicit DragOut (PrizanProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    std::function<void (const juce::String&)> onStatus;

private:
    PrizanProcessor& proc;
    bool dragging = false;
};
} // namespace prizan::ui

class PrizanEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit PrizanEditor (PrizanProcessor&);
    ~PrizanEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override;
    void loadFile (const juce::File&);
    void setStatus (const juce::String&);
    void setupKnob (juce::Slider&, const char* id, bool bipolar);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PrizanProcessor& proc;
    prizan::ui::Look look;
    prizan::ui::Display display;
    juce::Slider attack, decay, release, flanger, tone, width;
    prizan::ui::PitchSlider pitch;
    juce::Slider mix, rate, feedback, volume;
    juce::TextButton reverseBtn { "Reverse" }, loadBtn { "Load file" }, playBtn { "Play" }, saveBtn { "Save WAV" };
    prizan::ui::DragOut dragOut;
    std::vector<std::unique_ptr<SA>> sliderAttachments;
    std::unique_ptr<BA> reverseAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizanEditor)
};
