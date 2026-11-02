#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace prizan::ui
{
// 90s rack-synth palette: charcoal plastic, backlit LCD, orange accent stripe.
namespace colours
{
    const juce::Colour room { 0xff161719 }, panel { 0xff2a2c2f }, panelHi { 0xff36393d }, panelLo { 0xff202225 },
                       silk { 0xffe8e6df }, silkDim { 0xff9a9c9e }, accent { 0xffe8742a }, led { 0xffff3b2f },
                       ledOff { 0xff4a1a16 }, lcdTop { 0xffbdd673 }, lcdBottom { 0xff9db552 }, lcdInk { 0xff1e2a0e },
                       bezel { 0xff0c0d0e }, knobHi { 0xff3d4044 }, knobLo { 0xff111214 };
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
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

// Pitch snaps to whole semitones; hold Shift for cents.
struct PitchSlider : juce::Slider
{
    double snapValue (double v, DragMode) override
    {
        return juce::ModifierKeys::currentModifiers.isShiftDown() ? std::round (v * 100.0) / 100.0 : std::round (v);
    }
};

// The LCD: waveform, envelope, hand-drawn points, stretch grip, parameter readout.
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
    void flash (const juce::String& name, const juce::String& value) { flashText = name.toUpperCase() + "  " + value; flashTicks = 45; }
    void tick() { if (flashTicks > 0) --flashTicks; }
    bool dropHighlight = false;

private:
    struct Geo { float x0, W, top, bot, mid, amp; double T; };
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
    juce::String flashText;
    int flashTicks = 0;
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

// Sample folders the user added from Finder, shared by every Prizan instance.
class Library
{
public:
    Library();
    juce::StringArray folders() const;
    void add (const juce::File& dir);
    void remove (const juce::File& dir);

private:
    std::unique_ptr<juce::PropertiesFile> props;
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
    void setupKnob (juce::Slider&, const char* id, const char* name, bool bipolar);
    void showBrowseMenu();
    void addFolder();
    void step (int delta);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PrizanProcessor& proc;
    prizan::ui::Look look;
    prizan::ui::Library library;
    prizan::ui::Display display;
    juce::Slider attack, decay, release, flanger, tone, width;
    prizan::ui::PitchSlider pitch;
    juce::Slider mix, rate, feedback, volume;
    juce::TextButton browseBtn { "BROWSE" }, prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton reverseBtn { "REVERSE" }, loadBtn { "LOAD FILE" }, playBtn { "PLAY" }, saveBtn { "SAVE WAV" };
    prizan::ui::DragOut dragOut;
    std::vector<std::unique_ptr<SA>> sliderAttachments;
    std::unique_ptr<BA> reverseAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::File> menuFiles;
    juce::String status;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrizanEditor)
};
