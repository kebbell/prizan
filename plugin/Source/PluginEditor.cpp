#include "PluginEditor.h"

using namespace juce;
namespace prizan::ui
{
static Font mono (float size) { return Font (FontOptions ("Menlo", size, Font::plain)); }
static Font sans (float size, int style = Font::plain) { return Font (FontOptions (size, style)); }

//==============================================================================
Look::Look()
{
    setColour (ResizableWindow::backgroundColourId, colours::page);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::ink);
}

void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const float stroke = size > 110.0f ? 12.0f : 11.0f;
    const float r = size * 0.5f - stroke * 0.5f - 1.0f;
    const auto c = bounds.getCentre();
    const PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::butt);

    Path trk;
    trk.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (colours::track);
    g.strokePath (trk, st);

    const float a = a0 + pos * (a1 - a0);
    const float from = (bool) s.getProperties()["bipolar"] ? 0.0f : a0;
    if (std::abs (a - from) > 0.001f)
    {
        Path v;
        v.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (from, a), std::max (from, a), true);
        g.setColour (colours::green);
        g.strokePath (v, st);
    }

    const float dr = r - stroke * 0.5f - 3.0f;
    const auto disc = Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (c);
    Path discPath;
    discPath.addEllipse (disc);
    DropShadow (Colours::black.withAlpha (0.25f), 6, { 0, 2 }).drawForPath (g, discPath);
    g.setGradientFill (ColourGradient (Colours::white, c.x - dr * 0.3f, c.y - dr * 0.5f, Colour (0xffe9eeea), c.x, c.y + dr, true));
    g.fillPath (discPath);

    const Point<float> dir (std::sin (a), -std::cos (a));
    g.setColour (colours::greenDeep);
    g.drawLine (Line<float> (c + dir * (dr - 1.0f), c + dir * (dr - (size > 110.0f ? 12.0f : 8.0f))), 2.0f);

    g.setColour (colours::muted);
    g.setFont (mono (size > 110.0f ? 14.0f : 11.0f));
    g.drawText (s.getTextFromValue (s.getValue()), disc.reduced (4.0f, 0.0f), Justification::centred, false);
}

void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    const auto r = Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::green);
    g.fillRoundedRectangle (r.withRight (pos), 3.0f);
    const auto thumb = Rectangle<float> (16.0f, 16.0f).withCentre ({ pos, cy });
    g.setColour (Colours::black.withAlpha (0.18f));
    g.fillEllipse (thumb.translated (0.0f, 1.5f));
    g.setColour (Colours::white);
    g.fillEllipse (thumb);
    g.setColour (colours::greenDeep);
    g.drawEllipse (thumb.reduced (1.0f), 2.0f);
}

void Look::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState(), go = (bool) b.getProperties()["go"];
    Colour fill = on ? colours::track : go ? colours::green : colours::panel;
    if (down) fill = fill.darker (0.12f);
    else if (over) fill = fill.darker (0.05f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    if (! on && ! go)
    {
        g.setColour (colours::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
}

void Look::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool on = b.getToggleState(), go = (bool) b.getProperties()["go"];
    g.setColour (on ? colours::wave : go ? Colours::white : colours::ink);
    g.setFont (sans (14.0f, Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

//==============================================================================
Display::Display (PrizanProcessor& p) : proc (p) { refreshPoints(); }

prizan::Params Display::params() const
{
    auto p = proc.currentParams();
    p.endBend = endBend;
    p.numPoints = (int) std::min (pts.size(), p.points.size());
    for (int i = 0; i < p.numPoints; ++i) p.points[(size_t) i] = pts[(size_t) i];
    return p;
}

Display::Geo Display::geo() const
{
    const auto& info = proc.getSampleInfo();
    const float h = (float) getHeight(), top = 30.0f, bot = h - 26.0f;
    const auto p = proc.currentParams();
    const double stretch = proc.apvts.getRawParameterValue ("stretch")->load();
    return { (float) getWidth(), top, bot, (top + bot) * 0.5f, (bot - top) * 0.5f * 0.96f,
             info.duration * stretch / std::pow (2.0, p.pitch / 12.0) };
}

Rectangle<float> Display::gripArea() const
{
    const auto g = geo();
    return { g.W - 16.0f, g.top, 16.0f, g.bot - g.top };
}

void Display::refreshPoints()
{
    if (drag != Drag::none) return;
    const int v = proc.pointsVersion.load();
    if (v == seenVersion) return;
    seenVersion = v;
    proc.getPoints (pts, endBend);
}

void Display::push()
{
    proc.setPoints (pts, endBend);
    seenVersion = proc.pointsVersion.load();
    repaint();
}

int Display::hitPoint (Point<float> m) const
{
    const auto g = geo();
    int best = -1;
    float bd = 12.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& p = pts[i];
        const auto pp = params();
        const Point<float> c (p.x * g.W, g.mid - (float) prizan::adrAt (pp, p.x * g.T, g.T) * p.y * g.amp);
        const float d = c.getDistanceFrom (m);
        if (d < bd) { bd = d; best = (int) i; }
    }
    return best;
}

void Display::movePoint (int i, Point<float> m)
{
    const auto g = geo();
    const float lo = i > 0 ? pts[(size_t) i - 1].x + 0.002f : 0.002f;
    const float hi = i < (int) pts.size() - 1 ? pts[(size_t) i + 1].x - 0.002f : 0.998f;
    const float u = jlimit (lo, hi, m.x / g.W);
    const float a = (float) prizan::adrAt (params(), u * g.T, g.T);
    const float v = jlimit (0.0f, 1.0f, (g.mid - m.y) / g.amp);
    pts[(size_t) i].x = u;
    pts[(size_t) i].y = a > 0.02f ? jlimit (0.0f, 1.0f, v / a) : v;
}

void Display::mouseDown (const MouseEvent& e)
{
    const auto m = e.position;
    const auto g = geo();
    if (gripArea().contains (m))
    {
        drag = Drag::grip;
        gripStartX = m.x;
        gripStartValue = proc.apvts.getRawParameterValue ("stretch")->load();
        proc.apvts.getParameter ("stretch")->beginChangeGesture();
        return;
    }
    if (! pts.empty() && clearArea.contains (m)) { pts.clear(); endBend = 0; hot = -1; push(); return; }

    const int h = hitPoint (m);
    if (h >= 0) { drag = Drag::point; dragIndex = h; }
    else if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        if (pts.size() >= 32) return;
        const float u = jlimit (0.002f, 0.998f, m.x / g.W);
        size_t i = 0;
        while (i < pts.size() && pts[i].x <= u) ++i;
        pts.insert (pts.begin() + (long) i, { u, (float) prizan::shapeAt (params(), u), 0.0f });
        drag = Drag::point; dragIndex = (int) i;
        movePoint (dragIndex, m);
        push();
    }
    else if (e.mods.isAltDown())
    {
        const float u = m.x / g.W;
        size_t s = 0;
        while (s < pts.size() && pts[s].x < u) ++s;
        const float y0 = s > 0 ? pts[s - 1].y : 1.0f, y1 = s < pts.size() ? pts[s].y : 1.0f;
        drag = Drag::bend; dragIndex = (int) s;
        bendStart = s < pts.size() ? pts[s].b : endBend;
        bendDir = y0 > y1 ? 1.0f : y0 < y1 ? -1.0f : 1.0f;
        dragStartY = m.y;
    }
    else { proc.audition(); return; }
    hot = drag == Drag::point ? dragIndex : -1;
    repaint();
}

void Display::mouseDrag (const MouseEvent& e)
{
    const auto m = e.position;
    if (drag == Drag::grip)
    {
        float v = gripStartValue * std::pow (2.0f, (m.x - gripStartX) / (e.mods.isShiftDown() ? 480.0f : 120.0f));
        v = std::abs (v - 1.0f) < 0.02f ? 1.0f : jlimit (0.25f, 4.0f, v);
        auto* prm = proc.apvts.getParameter ("stretch");
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        repaint();
    }
    else if (drag == Drag::point) { movePoint (dragIndex, m); push(); }
    else if (drag == Drag::bend)
    {
        const float b = jlimit (-1.0f, 1.0f, bendStart + bendDir * (dragStartY - m.y) / 110.0f);
        if (dragIndex < (int) pts.size()) pts[(size_t) dragIndex].b = b; else endBend = b;
        push();
    }
}

void Display::mouseUp (const MouseEvent&)
{
    if (drag == Drag::grip) proc.apvts.getParameter ("stretch")->endChangeGesture();
    drag = Drag::none;
}

void Display::mouseMove (const MouseEvent& e)
{
    const int h = hitPoint (e.position);
    if (h != hot) { hot = h; repaint(); }
    setMouseCursor (gripArea().contains (e.position) ? MouseCursor::LeftRightResizeCursor
                    : h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::PointingHandCursor);
}

void Display::mouseDoubleClick (const MouseEvent& e)
{
    if (gripArea().contains (e.position))
    {
        auto* prm = proc.apvts.getParameter ("stretch");
        prm->setValueNotifyingHost (prm->convertTo0to1 (1.0f));
        return;
    }
    const int i = hitPoint (e.position);
    if (i >= 0)
    {
        pts.erase (pts.begin() + i);
        if (pts.empty()) endBend = 0;
        hot = -1;
        push();
    }
}

void Display::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (colours::screenHi, 0, 0, colours::screen, 0, b.getHeight(), false));
    g.fillRoundedRectangle (b, 6.0f);

    const auto& info = proc.getSampleInfo();
    const auto gg = geo();
    const int W = getWidth();
    const auto p = params();

    g.setColour (colours::wave.withAlpha (0.08f));
    for (int i = 1; i < 8; ++i) g.fillRect ((float) (i * W / 8), gg.top, 1.0f, gg.bot - gg.top);
    g.fillRect (0.0f, gg.mid, (float) W, 1.0f);

    if (! info.mins.empty() && W > 0)
    {
        const bool rev = proc.apvts.getRawParameterValue ("reverse")->load() > 0.5f;
        std::vector<float> env ((size_t) W);
        for (int x = 0; x < W; ++x) env[(size_t) x] = (float) prizan::envAt (p, (x + 0.5) / W * gg.T, gg.T);

        // shade what the envelope cuts away
        g.setColour (Colours::black.withAlpha (0.35f));
        for (const float s : { -1.0f, 1.0f })
        {
            Path sh;
            sh.startNewSubPath (0.0f, s < 0 ? gg.top : gg.bot);
            for (int x = 0; x < W; ++x) sh.lineTo ((float) x, gg.mid + s * env[(size_t) x] * gg.amp);
            sh.lineTo ((float) W, s < 0 ? gg.top : gg.bot);
            sh.closeSubPath();
            g.fillPath (sh);
        }

        // original (dim) and shaped (bright) waveform
        const int N = (int) info.mins.size();
        for (int pass = 0; pass < 2; ++pass)
        {
            g.setColour (pass ? colours::wave : colours::wave.withAlpha (0.22f));
            for (int x = 0; x < W; ++x)
            {
                const int b0 = x * N / W, b1 = std::max (b0 + 1, (x + 1) * N / W);
                float lo = 0, hi = 0;
                for (int k = b0; k < b1 && k < N; ++k)
                {
                    const int i = rev ? N - 1 - k : k;
                    lo = std::min (lo, info.mins[(size_t) i]); hi = std::max (hi, info.maxs[(size_t) i]);
                }
                const float kk = pass ? env[(size_t) x] : 1.0f;
                const float y0 = gg.mid - hi * kk * gg.amp, y1 = gg.mid - lo * kk * gg.amp;
                g.fillRect ((float) x, y0, 1.0f, std::max (0.6f, y1 - y0));
            }
        }

        Path line;
        for (int x = 0; x < W; ++x)
        {
            const float y = gg.mid - env[(size_t) x] * gg.amp;
            if (x == 0) line.startNewSubPath (0.0f, y); else line.lineTo ((float) x, y);
        }
        g.setColour (Colours::white.withAlpha (0.75f));
        g.strokePath (line, PathStrokeType (1.5f));

        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto& pt = pts[i];
            const Point<float> c (pt.x * gg.W, gg.mid - (float) prizan::adrAt (p, pt.x * gg.T, gg.T) * pt.y * gg.amp);
            const float r = (int) i == hot ? 6.0f : 4.5f;
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
            g.setColour (colours::green);
            g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 2.0f);
        }

        const float prog = proc.playProgress();
        if (prog >= 0.0f && prog <= 1.0f)
        {
            g.setColour (Colours::white);
            g.fillRect (prog * (float) W, gg.top, 2.0f, gg.bot - gg.top);
        }
    }

    // stretch grip
    const auto grip = gripArea();
    g.setColour (colours::wave.withAlpha (drag == Drag::grip ? 0.16f : 0.06f));
    g.fillRect (grip);
    g.setColour (colours::wave.withAlpha (0.35f));
    g.fillRect (grip.getX(), grip.getY(), 1.0f, grip.getHeight());
    g.setColour (colours::wave.withAlpha (0.8f));
    g.fillRect (grip.getCentreX() - 2.5f, grip.getCentreY() - 14.0f, 1.0f, 28.0f);
    g.fillRect (grip.getCentreX() + 1.5f, grip.getCentreY() - 14.0f, 1.0f, 28.0f);

    // text
    const float stretch = proc.apvts.getRawParameterValue ("stretch")->load();
    g.setFont (mono (13.0f));
    g.setColour (colours::wave);
    g.drawText (info.name.toUpperCase(), Rectangle<float> (12, 8, b.getWidth() * 0.55f, 16), Justification::centredLeft, true);
    g.setFont (mono (12.0f));
    g.setColour (colours::wave.withAlpha (0.75f));
    g.drawText (String (info.duration, 2) + " s  " + String (info.sampleRate / 1000.0, 1) + " kHz  " + (info.channels > 1 ? "stereo" : "mono"),
                Rectangle<float> (b.getWidth() * 0.5f, 8, b.getWidth() * 0.5f - 12, 16), Justification::centredRight, true);

    String out = "out " + String (gg.T, 2) + " s";
    if (std::abs (stretch - 1.0f) > 0.005f) out << "  length " << roundToInt (stretch * 100.0f) << "%";
    const float outW = GlyphArrangement::getStringWidth (mono (12.0f), out) + 2.0f;
    const auto bottom = Rectangle<float> (12, b.getHeight() - 22, b.getWidth() - 24, 16);
    g.setColour (colours::wave.withAlpha (0.75f));
    g.drawText (out, bottom, Justification::centredRight, false);
    float leftLimit = bottom.getRight() - outW - 12.0f;
    clearArea = {};
    if (! pts.empty())
    {
        const String clr = "clear points";
        const float cw = GlyphArrangement::getStringWidth (mono (12.0f), clr) + 2.0f;
        clearArea = { leftLimit - cw, bottom.getY(), cw, bottom.getHeight() };
        g.setColour (colours::wave);
        g.drawText (clr, clearArea, Justification::centredLeft, false);
        g.fillRect (clearArea.getX(), clearArea.getBottom() - 1.0f, cw - 2.0f, 1.0f);
        leftLimit = clearArea.getX() - 12.0f;
    }
    g.setColour (colours::wave.withAlpha (0.5f));
    g.drawText ("Drop a one-shot. Click plays. Cmd-click adds a point, Alt-drag bends.",
                bottom.withRight (leftLimit), Justification::centredLeft, true);

    if (dropHighlight)
    {
        const auto r = b.reduced (8.0f);
        g.setColour (colours::screen.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 4.0f);
        Path border;
        border.addRoundedRectangle (r, 4.0f);
        Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        PathStrokeType (2.0f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (colours::wave);
        g.fillPath (dashed);
        g.setFont (sans (22.0f, Font::bold));
        g.drawText ("Drop to load", r, Justification::centred, false);
    }
}

//==============================================================================
void DragOut::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::wave);
    g.setFont (sans (14.0f, Font::bold));
    g.drawText ("Drag to DAW", r, Justification::centred, false);
}

void DragOut::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4) return;
    dragging = true;
    const auto dir = File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Prizan").getChildFile ("Renders");
    dir.createDirectory();
    const auto f = dir.getNonexistentChildFile (proc.suggestedRenderName(), ".wav", false);
    String err;
    if (! proc.renderToFile (f, err)) { if (onStatus) onStatus (err); return; }
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    if (onStatus) onStatus ("Rendered " + f.getFileName() + ". Drop it on an Ableton track.");
}
} // namespace prizan::ui

//==============================================================================
using namespace prizan::ui;

namespace layout
{
    constexpr int W = 1140, H = 450;
    const Rectangle<int> panel { 10, 10, 1120, 430 };
    const Rectangle<int> display { 28, 64, 580, 262 };
    constexpr int knobX = 630, colW = 112, knob = 84, row0 = 92, row1 = 216;
    const Rectangle<int> widthKnob { 986, 118, 132, 132 };
    constexpr int barTop = 340;
    const Rectangle<int> sliderRow { 28, 352, 1084, 28 };
    const Rectangle<int> status { 28, 392, 560, 34 };
}

PrizanEditor::PrizanEditor (PrizanProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), dragOut (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (display);

    setupKnob (attack, "attack", false);
    setupKnob (decay, "decay", false);
    setupKnob (release, "release", false);
    setupKnob (flanger, "flanger", false);
    setupKnob (pitch, "pitch", true);
    setupKnob (tone, "tone", true);
    setupKnob (width, "width", false);

    for (auto [s, id] : { std::pair<Slider*, const char*> { &mix, "mix" }, { &rate, "rate" }, { &feedback, "feedback" }, { &volume, "volume" } })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
        sliderAttachments.push_back (std::make_unique<SA> (proc.apvts, id, *s));
        auto* prm = proc.apvts.getParameter (id);
        s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        s->onValueChange = [this] { repaint (layout::sliderRow); };
    }

    reverseBtn.setClickingTogglesState (true);
    reverseAttachment = std::make_unique<BA> (proc.apvts, "reverse", reverseBtn);
    saveBtn.getProperties().set ("go", true);
    for (auto* b : { &reverseBtn, &loadBtn, &playBtn, &saveBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (dragOut);
    dragOut.onStatus = [this] (const String& s) { setStatus (s); };

    playBtn.onClick = [this] { proc.audition(); };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a one-shot", File::getSpecialLocation (File::userMusicDirectory),
                                                 "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) loadFile (f); });
    };
    saveBtn.onClick = [this]
    {
        const auto dir = File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Prizan");
        dir.createDirectory();
        chooser = std::make_unique<FileChooser> ("Save your sound", dir.getChildFile (proc.suggestedRenderName() + ".wav"), "*.wav");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                              [this] (const FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == File()) return;
                                  f = f.withFileExtension ("wav");
                                  String err;
                                  if (proc.renderToFile (f, err)) setStatus ("Saved " + f.getFullPathName());
                                  else setStatus (err);
                              });
    };

    setWantsKeyboardFocus (true);
    setSize (layout::W, layout::H);
    startTimerHz (30);
}

PrizanEditor::~PrizanEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PrizanEditor::setupKnob (Slider& s, const char* id, bool bipolar)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (-2.35619f, 2.35619f, true);
    s.getProperties().set ("bipolar", bipolar);
    s.setMouseDragSensitivity (220);
    s.setVelocityModeParameters (0.35, 1, 0.0, true, ModifierKeys::shiftModifier);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SA> (proc.apvts, id, s));
    auto* prm = proc.apvts.getParameter (id);
    s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void PrizanEditor::paint (Graphics& g)
{
    g.fillAll (colours::page);
    const auto panel = layout::panel.toFloat();
    DropShadow (Colours::black.withAlpha (0.12f), 18, { 0, 6 }).drawForRectangle (g, layout::panel);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (panel, 12.0f);

    // brand: neon-tube lettering, small and centred
    {
        Font f (FontOptions ("Futura", 30.0f, Font::bold));
        f = f.withExtraKerningFactor (0.22f);
        GlyphArrangement ga;
        ga.addLineOfText (f, "PRIZAN", 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        const auto pb = p.getBounds();
        p.applyTransform (AffineTransform::translation ((float) getWidth() * 0.5f - pb.getCentreX(), 38.0f - pb.getCentreY()));
        g.setColour (colours::greenDeep);
        g.strokePath (p, PathStrokeType (4.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colours::white);
        g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (colours::greenDeep);
        g.strokePath (p, PathStrokeType (0.8f));
    }

    // knob labels
    g.setColour (colours::ink);
    g.setFont (sans (15.0f, Font::bold));
    const char* names[] = { "Attack", "Decay", "Release", "Flanger", "Pitch", "Tone" };
    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3, y = (i < 3 ? layout::row0 : layout::row1) - 22;
        g.drawText (names[i], layout::knobX + col * layout::colW, y, layout::colW, 18, Justification::centred);
    }
    g.setColour (colours::line);
    g.fillRect (970, 70, 1, 250);
    g.setColour (colours::ink);
    g.setFont (sans (17.0f, Font::bold).withExtraKerningFactor (0.06f));
    g.drawText ("WIDTH", layout::widthKnob.getX(), layout::widthKnob.getY() - 26, layout::widthKnob.getWidth(), 20, Justification::centred);
    g.setColour (colours::muted);
    g.setFont (sans (12.0f));
    g.drawFittedText ("0 is mono, 100% as recorded,\n200% extra wide", layout::widthKnob.getX() - 10, layout::widthKnob.getBottom() + 6,
                      layout::widthKnob.getWidth() + 20, 34, Justification::centredTop, 2);

    // utility bar
    {
        Path bar;
        bar.addRoundedRectangle ((float) layout::panel.getX(), (float) layout::barTop, (float) layout::panel.getWidth(),
                                 (float) (layout::panel.getBottom() - layout::barTop), 12.0f, 12.0f, false, false, true, true);
        g.setColour (colours::bar);
        g.fillPath (bar);
        g.setColour (colours::line);
        g.fillRect (layout::panel.getX(), layout::barTop, layout::panel.getWidth(), 1);
    }
    const char* barNames[] = { "Mix", "Flange rate", "Feedback", "Volume" };
    Slider* bars[] = { &mix, &rate, &feedback, &volume };
    for (int i = 0; i < 4; ++i)
    {
        const auto cell = bars[i]->getBounds();
        g.setColour (colours::ink);
        g.setFont (sans (14.0f, Font::bold));
        g.drawText (barNames[i], cell.getX() - 86, cell.getY(), 82, cell.getHeight(), Justification::centredRight);
        g.setColour (colours::muted);
        g.setFont (mono (11.0f));
        g.drawText (bars[i]->getTextFromValue (bars[i]->getValue()), cell.getRight() + 6, cell.getY(), 62, cell.getHeight(), Justification::centredLeft);
    }
    g.setColour (colours::muted);
    g.setFont (sans (13.0f));
    g.drawText (status.isNotEmpty() ? status : String ("Play MIDI notes (C3 is the original pitch) or click the screen."),
                layout::status, Justification::centredLeft, true);
}

void PrizanEditor::resized()
{
    display.setBounds (layout::display);
    Slider* knobs[] = { &attack, &decay, &release, &flanger, &pitch, &tone };
    for (int i = 0; i < 6; ++i)
    {
        const int col = i % 3;
        knobs[i]->setBounds (layout::knobX + col * layout::colW + (layout::colW - layout::knob) / 2,
                             i < 3 ? layout::row0 : layout::row1, layout::knob, layout::knob);
    }
    width.setBounds (layout::widthKnob);

    const int cellW = layout::sliderRow.getWidth() / 4;
    Slider* bars[] = { &mix, &rate, &feedback, &volume };
    for (int i = 0; i < 4; ++i)
        bars[i]->setBounds (layout::sliderRow.getX() + i * cellW + 90, layout::sliderRow.getY(), cellW - 90 - 72, layout::sliderRow.getHeight());

    int x = 1112;
    const int y = 392, h = 34;
    auto place = [&] (Component& c, int w) { x -= w; c.setBounds (x, y, w, h); x -= 8; };
    place (dragOut, 124);
    place (saveBtn, 96);
    place (playBtn, 66);
    place (loadBtn, 90);
    place (reverseBtn, 84);
}

void PrizanEditor::timerCallback()
{
    display.refreshPoints();
    display.repaint();
    if (statusTicks > 0 && --statusTicks == 0) { status.clear(); repaint (layout::status); }
}

void PrizanEditor::setStatus (const String& s)
{
    status = s;
    statusTicks = 30 * 8;
    repaint (layout::status);
}

bool PrizanEditor::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::spaceKey) { proc.audition(); return true; }
    return false;
}

static bool isAudio (const String& f)
{
    return File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf");
}

bool PrizanEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isAudio (f)) return true;
    return false;
}

void PrizanEditor::fileDragEnter (const StringArray&, int, int) { display.dropHighlight = true; display.repaint(); }
void PrizanEditor::fileDragExit (const StringArray&) { display.dropHighlight = false; display.repaint(); }

void PrizanEditor::filesDropped (const StringArray& files, int, int)
{
    display.dropHighlight = false;
    for (auto& f : files)
        if (isAudio (f)) { loadFile (File (f)); break; }
    display.repaint();
}

void PrizanEditor::loadFile (const File& f)
{
    String err;
    if (proc.loadSampleFile (f, err)) setStatus ("Loaded " + f.getFileName() + ". Play a note or click the screen.");
    else setStatus (err);
}
