#include "PluginEditor.h"

using namespace juce;
namespace prizan::ui
{
static Font mono (float size, bool bold = false) { return Font (FontOptions ("Menlo", size, bold ? Font::bold : Font::plain)); }
static Font sans (float size, int style = Font::plain) { return Font (FontOptions ("Helvetica Neue", size, style)); }
static const char* audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf";

static bool isAudio (const File& f) { return f.existsAsFile() && f.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf"); }

//==============================================================================
Look::Look()
{
    setColour (ResizableWindow::backgroundColourId, colours::room);
    setColour (PopupMenu::backgroundColourId, colours::panelHi);
    setColour (PopupMenu::textColourId, colours::silk);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (PopupMenu::headerTextColourId, colours::silkDim);
}

void Look::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (colours::panelHi);
    g.setColour (colours::bezel);
    g.drawRect (0, 0, w, h, 1);
}

void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool bipolar = (bool) s.getProperties()["bipolar"];
    auto at = [c] (float a, float r) { return c + Point<float> (std::sin (a), -std::cos (a)) * r; };

    // printed scale
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const bool major = i == 0 || i == 10 || (bipolar && i == 5);
        g.setColour (major ? colours::silk : colours::silkDim);
        g.drawLine (Line<float> (at (a, R - (major ? 7.0f : 5.0f)), at (a, R - 1.0f)), major ? 1.8f : 1.2f);
    }

    // thin value arc just inside the scale
    const float a = a0 + pos * (a1 - a0), from = bipolar ? 0.0f : a0;
    if (std::abs (a - from) > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, R - 9.5f, R - 9.5f, 0.0f, std::min (from, a), std::max (from, a), true);
        g.setColour (colours::accent);
        g.strokePath (arc, PathStrokeType (2.0f));
    }

    // knob body: knurled skirt and a darker cap
    const float kr = R - 12.0f;
    Path body;
    body.addEllipse (Rectangle<float> (kr * 2, kr * 2).withCentre (c));
    DropShadow (Colours::black.withAlpha (0.7f), 8, { 0, 3 }).drawForPath (g, body);
    g.setGradientFill (ColourGradient (colours::knobHi, c.x, c.y - kr, colours::knobLo, c.x, c.y + kr, false));
    g.fillPath (body);
    g.setColour (Colours::black.withAlpha (0.55f));
    for (int i = 0; i < 40; ++i)
    {
        const float ka = MathConstants<float>::twoPi * (float) i / 40.0f;
        g.drawLine (Line<float> (at (ka, kr - 3.5f), at (ka, kr - 0.5f)), 1.0f);
    }
    const float cr = kr * 0.74f;
    g.setGradientFill (ColourGradient (Colour (0xff34373b), c.x, c.y - cr, Colour (0xff17181a), c.x, c.y + cr, false));
    g.fillEllipse (Rectangle<float> (cr * 2, cr * 2).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (Rectangle<float> (cr * 2, cr * 2).withCentre (c), 1.0f);

    g.setColour (colours::silk);
    g.drawLine (Line<float> (at (a, kr * 0.32f), at (a, kr - 2.0f)), kr > 40.0f ? 3.2f : 2.6f);
}

void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    const auto slot = Rectangle<float> ((float) x, cy - 2.5f, (float) w, 5.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (slot, 2.5f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRect (slot.getX(), slot.getBottom(), slot.getWidth(), 1.0f);
    g.setColour (colours::accent.withAlpha (0.85f));
    g.fillRect ((float) x + 1.0f, cy - 0.75f, pos - (float) x - 1.0f, 1.5f);

    const auto cap = Rectangle<float> (14.0f, 24.0f).withCentre ({ pos, cy });
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff6e7176), cap.getX(), cap.getY(), Colour (0xff3a3d41), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (cap, 2.0f, 1.0f);
    g.setColour (colours::silk);
    g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 3.0f, 2.0f, cap.getHeight() - 6.0f);
}

void Look::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool go = (bool) b.getProperties()["go"];
    if (! down)
    {
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    }
    else r.translate (0.0f, 1.0f);
    Colour top = go ? Colour (0xfff0843c) : Colour (0xff585c61), bottom = go ? Colour (0xffc85d1a) : Colour (0xff3e4146);
    if (over && ! down) { top = top.brighter (0.08f); bottom = bottom.brighter (0.08f); }
    if (down) { top = top.darker (0.15f); bottom = bottom.darker (0.15f); }
    g.setGradientFill (ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.fillRect (r.getX() + 3.0f, r.getY() + 1.0f, r.getWidth() - 6.0f, 1.0f);

    if ((bool) b.getProperties()["led"])
    {
        const auto led = Rectangle<float> (9.0f, 4.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
        g.setColour (b.getToggleState() ? colours::led : colours::ledOff);
        g.fillRoundedRectangle (led, 1.0f);
        if (b.getToggleState())
        {
            g.setColour (colours::led.withAlpha (0.35f));
            g.fillRoundedRectangle (led.expanded (3.0f), 3.0f);
        }
    }
}

void Look::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    const bool go = (bool) b.getProperties()["go"], led = (bool) b.getProperties()["led"];
    auto r = b.getLocalBounds().toFloat().translated (0.0f, down ? 1.0f : 0.0f);
    if (led) r.removeFromLeft (14.0f);
    g.setColour (go ? Colour (0xff1a1206) : colours::silk);
    g.setFont (sans (11.5f, Font::bold).withExtraKerningFactor (0.08f));
    g.drawText (b.getButtonText(), r, Justification::centred, false);
}

//==============================================================================
Library::Library()
{
    PropertiesFile::Options o;
    o.applicationName = "Prizan";
    o.filenameSuffix = ".settings";
    o.folderName = "Prizan";
    o.osxLibrarySubFolder = "Application Support";
    props = std::make_unique<PropertiesFile> (o);
}

StringArray Library::folders() const
{
    auto list = StringArray::fromLines (props->getValue ("folders"));
    list.removeEmptyStrings();
    return list;
}

void Library::add (const File& dir)
{
    auto list = folders();
    list.addIfNotAlreadyThere (dir.getFullPathName());
    props->setValue ("folders", list.joinIntoString ("\n"));
    props->saveIfNeeded();
}

void Library::remove (const File& dir)
{
    auto list = folders();
    list.removeString (dir.getFullPathName());
    props->setValue ("folders", list.joinIntoString ("\n"));
    props->saveIfNeeded();
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

// all drawing happens inside the LCD glass, 8px in from the bezel
Display::Geo Display::geo() const
{
    const auto& info = proc.getSampleInfo();
    const float inset = 8.0f, h = (float) getHeight(), top = inset + 26.0f, bot = h - inset - 24.0f;
    const auto p = proc.currentParams();
    const double stretch = proc.apvts.getRawParameterValue ("stretch")->load();
    return { inset, (float) getWidth() - inset * 2.0f, top, bot, (top + bot) * 0.5f, (bot - top) * 0.5f * 0.96f,
             info.duration * stretch / std::pow (2.0, p.pitch / 12.0) };
}

Rectangle<float> Display::gripArea() const
{
    const auto g = geo();
    return { g.x0 + g.W - 16.0f, g.top, 16.0f, g.bot - g.top };
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
    const auto pp = params();
    int best = -1;
    float bd = 12.0f;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& p = pts[i];
        const Point<float> c (g.x0 + p.x * g.W, g.mid - (float) prizan::adrAt (pp, p.x * g.T, g.T) * p.y * g.amp);
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
    const float u = jlimit (lo, hi, (m.x - g.x0) / g.W);
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
        const float u = jlimit (0.002f, 0.998f, (m.x - g.x0) / g.W);
        size_t i = 0;
        while (i < pts.size() && pts[i].x <= u) ++i;
        pts.insert (pts.begin() + (long) i, { u, (float) prizan::shapeAt (params(), u), 0.0f });
        drag = Drag::point; dragIndex = (int) i;
        movePoint (dragIndex, m);
        push();
    }
    else if (e.mods.isAltDown())
    {
        const float u = (m.x - g.x0) / g.W;
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
        flash ("Length", String (roundToInt (v * 100.0f)) + "%");
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
    const auto gg = geo();
    const auto glass = Rectangle<float> (gg.x0, gg.x0, gg.W, b.getHeight() - gg.x0 * 2.0f);
    const Colour ink = colours::lcdInk;

    // bezel and backlit glass
    g.setColour (colours::bezel);
    g.fillRoundedRectangle (b, 6.0f);
    g.setGradientFill (ColourGradient (colours::lcdTop, 0, glass.getY(), colours::lcdBottom, 0, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, 3.0f);
    g.setColour (ink.withAlpha (0.045f));
    for (float x = glass.getX(); x < glass.getRight(); x += 3.0f) g.fillRect (x, glass.getY(), 1.0f, glass.getHeight());
    for (float y = glass.getY(); y < glass.getBottom(); y += 3.0f) g.fillRect (glass.getX(), y, glass.getWidth(), 1.0f);

    const auto& info = proc.getSampleInfo();
    const int W = (int) gg.W;
    const auto p = params();

    g.setColour (ink.withAlpha (0.12f));
    for (int i = 1; i < 8; ++i) g.fillRect (gg.x0 + (float) (i * W / 8), gg.top, 1.0f, gg.bot - gg.top);
    g.fillRect (gg.x0, gg.mid, (float) W, 1.0f);

    if (! info.mins.empty() && W > 0)
    {
        const bool rev = proc.apvts.getRawParameterValue ("reverse")->load() > 0.5f;
        std::vector<float> env ((size_t) W);
        for (int x = 0; x < W; ++x) env[(size_t) x] = (float) prizan::envAt (p, (x + 0.5) / W * gg.T, gg.T);

        // shade what the envelope cuts away
        g.setColour (ink.withAlpha (0.13f));
        for (const float s : { -1.0f, 1.0f })
        {
            Path sh;
            sh.startNewSubPath (gg.x0, s < 0 ? gg.top : gg.bot);
            for (int x = 0; x < W; ++x) sh.lineTo (gg.x0 + (float) x, gg.mid + s * env[(size_t) x] * gg.amp);
            sh.lineTo (gg.x0 + (float) W, s < 0 ? gg.top : gg.bot);
            sh.closeSubPath();
            g.fillPath (sh);
        }

        // original (faint) and shaped (dark) waveform
        const int N = (int) info.mins.size();
        for (int pass = 0; pass < 2; ++pass)
        {
            g.setColour (pass ? ink.withAlpha (0.9f) : ink.withAlpha (0.2f));
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
                g.fillRect (gg.x0 + (float) x, y0, 1.0f, std::max (0.8f, y1 - y0));
            }
        }

        Path line;
        for (int x = 0; x < W; ++x)
        {
            const float y = gg.mid - env[(size_t) x] * gg.amp;
            if (x == 0) line.startNewSubPath (gg.x0, y); else line.lineTo (gg.x0 + (float) x, y);
        }
        g.setColour (ink);
        g.strokePath (line, PathStrokeType (1.5f));

        for (size_t i = 0; i < pts.size(); ++i)
        {
            const auto& pt = pts[i];
            const Point<float> c (gg.x0 + pt.x * gg.W, gg.mid - (float) prizan::adrAt (p, pt.x * gg.T, gg.T) * pt.y * gg.amp);
            const float r = (int) i == hot ? 6.0f : 4.5f;
            g.setColour (ink);
            g.fillRect (Rectangle<float> (r * 2, r * 2).withCentre (c));
            g.setColour (colours::lcdTop);
            g.fillRect (Rectangle<float> (r * 2 - 4.0f, r * 2 - 4.0f).withCentre (c));
        }

        const float prog = proc.playProgress();
        if (prog >= 0.0f && prog <= 1.0f)
        {
            g.setColour (ink);
            g.fillRect (gg.x0 + prog * (float) W, gg.top, 2.0f, gg.bot - gg.top);
        }
    }

    // stretch grip
    const auto grip = gripArea();
    g.setColour (ink.withAlpha (drag == Drag::grip ? 0.22f : 0.1f));
    g.fillRect (grip);
    g.setColour (ink.withAlpha (0.5f));
    g.fillRect (grip.getX(), grip.getY(), 1.0f, grip.getHeight());
    g.setColour (ink);
    g.fillRect (grip.getCentreX() - 2.5f, grip.getCentreY() - 14.0f, 1.0f, 28.0f);
    g.fillRect (grip.getCentreX() + 1.5f, grip.getCentreY() - 14.0f, 1.0f, 28.0f);

    // text rows
    const auto topRow = Rectangle<float> (gg.x0 + 8.0f, gg.x0 + 5.0f, gg.W - 16.0f, 16.0f);
    const auto bottomRow = Rectangle<float> (gg.x0 + 8.0f, glass.getBottom() - 21.0f, gg.W - 16.0f, 16.0f);
    const float stretch = proc.apvts.getRawParameterValue ("stretch")->load();
    g.setColour (ink);
    g.setFont (mono (13.0f, true));
    g.drawText (info.name.toUpperCase(), topRow.withWidth (topRow.getWidth() * 0.55f), Justification::centredLeft, true);
    g.setFont (mono (12.0f));
    g.drawText (String (info.duration, 2) + "s " + String (info.sampleRate / 1000.0, 1) + "kHz " + (info.channels > 1 ? "ST" : "MONO"),
                topRow, Justification::centredRight, true);

    String out = "OUT " + String (gg.T, 2) + "s";
    if (std::abs (stretch - 1.0f) > 0.005f) out << "  LEN " << roundToInt (stretch * 100.0f) << "%";
    const float outW = GlyphArrangement::getStringWidth (mono (12.0f), out) + 2.0f;
    g.setFont (mono (12.0f));
    g.drawText (out, bottomRow, Justification::centredRight, false);
    float leftLimit = bottomRow.getRight() - outW - 12.0f;
    clearArea = {};
    if (! pts.empty())
    {
        const String clr = "CLEAR PTS";
        const float cw = GlyphArrangement::getStringWidth (mono (12.0f), clr) + 6.0f;
        clearArea = { leftLimit - cw, bottomRow.getY() - 1.0f, cw, bottomRow.getHeight() + 2.0f };
        g.setColour (ink);
        g.fillRect (clearArea);
        g.setColour (colours::lcdTop);
        g.drawText (clr, clearArea, Justification::centred, false);
        leftLimit = clearArea.getX() - 12.0f;
    }
    g.setColour (ink.withAlpha (flashTicks > 0 ? 1.0f : 0.6f));
    g.setFont (mono (12.0f, flashTicks > 0));
    g.drawText (flashTicks > 0 ? flashText : String ("CLICK=PLAY  CMD+CLICK=POINT  ALT+DRAG=BEND"),
                bottomRow.withRight (leftLimit), Justification::centredLeft, true);

    // a little glare across the glass
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), 0, glass.getY(), Colours::transparentWhite, 0, glass.getY() + glass.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (glass, 3.0f);

    if (dropHighlight)
    {
        const auto r = glass.reduced (6.0f);
        g.setColour (colours::lcdTop.withAlpha (0.92f));
        g.fillRect (r);
        Path border;
        border.addRectangle (r);
        Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        PathStrokeType (2.0f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (ink);
        g.fillPath (dashed);
        g.setFont (mono (20.0f, true));
        g.drawText ("DROP TO LOAD", r, Justification::centred, false);
    }
}

//==============================================================================
void DragOut::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    g.setColour (colours::bezel);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::accent);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setFont (sans (11.5f, Font::bold).withExtraKerningFactor (0.08f));
    g.drawText ("DRAG TO DAW", r, Justification::centred, false);
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
    constexpr int W = 1140, H = 460;
    const Rectangle<int> panel { 8, 8, 1124, 444 };
    const Rectangle<int> browse { 30, 22, 108, 28 }, prev { 144, 22, 30, 28 }, next { 178, 22, 30, 28 };
    const Rectangle<int> display { 28, 72, 580, 262 };
    constexpr int knobX = 628, colW = 112, knob = 88, row0 = 98, row1 = 228;
    const Rectangle<int> widthKnob { 982, 116, 136, 136 };
    constexpr int barTop = 350;
    const Rectangle<int> sliderRow { 28, 362, 1084, 30 };
    const Rectangle<int> status { 28, 404, 520, 32 };
}

PrizanEditor::PrizanEditor (PrizanProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), dragOut (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (display);

    setupKnob (attack, "attack", "Attack", false);
    setupKnob (decay, "decay", "Decay", false);
    setupKnob (release, "release", "Release", false);
    setupKnob (flanger, "flanger", "Flanger", false);
    setupKnob (pitch, "pitch", "Pitch", true);
    setupKnob (tone, "tone", "Tone", true);
    setupKnob (width, "width", "Width", false);

    for (auto [s, id, name] : { std::tuple<Slider*, const char*, const char*> { &mix, "mix", "Mix" }, { &rate, "rate", "Flange rate" },
                                { &feedback, "feedback", "Feedback" }, { &volume, "volume", "Volume" } })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*s);
        sliderAttachments.push_back (std::make_unique<SA> (proc.apvts, id, *s));
        auto* prm = proc.apvts.getParameter (id);
        s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        s->onValueChange = [this, sl = s, n = String (name)]
        {
            repaint (layout::sliderRow);
            display.flash (n, sl->getTextFromValue (sl->getValue()));
        };
    }

    reverseBtn.setClickingTogglesState (true);
    reverseBtn.getProperties().set ("led", true);
    reverseAttachment = std::make_unique<BA> (proc.apvts, "reverse", reverseBtn);
    saveBtn.getProperties().set ("go", true);
    for (auto* b : { &browseBtn, &prevBtn, &nextBtn, &reverseBtn, &loadBtn, &playBtn, &saveBtn }) addAndMakeVisible (*b);
    addAndMakeVisible (dragOut);
    dragOut.onStatus = [this] (const String& s) { setStatus (s); };

    browseBtn.setButtonText (String (CharPointer_UTF8 ("BROWSE \xe2\x96\xbe")));
    prevBtn.setButtonText (String (CharPointer_UTF8 ("\xe2\x97\x80")));
    nextBtn.setButtonText (String (CharPointer_UTF8 ("\xe2\x96\xb6")));
    prevBtn.setTooltip ("Previous sample in this folder");
    nextBtn.setTooltip ("Next sample in this folder");
    browseBtn.onClick = [this] { showBrowseMenu(); };
    prevBtn.onClick = [this] { step (-1); };
    nextBtn.onClick = [this] { step (1); };

    playBtn.onClick = [this] { proc.audition(); };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a one-shot", File::getSpecialLocation (File::userMusicDirectory), audioWildcard);
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

void PrizanEditor::setupKnob (Slider& s, const char* id, const char* name, bool bipolar)
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
    s.onValueChange = [this, &s, n = String (name)]
    {
        repaint (s.getBounds().withHeight (s.getHeight() + 18));
        display.flash (n, s.getTextFromValue (s.getValue()));
    };
}

//==============================================================================
void PrizanEditor::showBrowseMenu()
{
    menuFiles.clear();
    const auto current = proc.getSampleFile();
    constexpr int addId = 100000, removeBase = 200000, revealId = 300000;
    constexpr size_t maxFiles = 4000;

    std::function<void (PopupMenu&, const File&, int)> addDir = [&] (PopupMenu& m, const File& dir, int depth)
    {
        if (depth < 4)
        {
            auto subs = dir.findChildFiles (File::findDirectories, false);
            subs.sort();
            for (auto& sub : subs)
            {
                if (sub.getFileName().startsWithChar ('.')) continue;
                PopupMenu sm;
                addDir (sm, sub, depth + 1);
                if (sm.getNumItems() > 0) m.addSubMenu (sub.getFileName(), sm);
            }
        }
        auto files = dir.findChildFiles (File::findFiles, false, audioWildcard);
        files.sort();
        for (auto& f : files)
        {
            if (menuFiles.size() >= maxFiles) return;
            menuFiles.push_back (f);
            m.addItem ((int) menuFiles.size(), f.getFileNameWithoutExtension(), true, f == current);
        }
    };

    PopupMenu menu;
    const auto folders = library.folders();
    if (folders.isEmpty())
        menu.addItem (-1, "No folders yet. Add one below.", false);
    for (auto& path : folders)
    {
        const File dir (path);
        PopupMenu sm;
        if (dir.isDirectory()) addDir (sm, dir, 0);
        if (sm.getNumItems() == 0) sm.addItem (-1, dir.isDirectory() ? "No audio files here" : "Folder not found", false);
        menu.addSubMenu (dir.getFileName(), sm);
    }
    menu.addSeparator();
    menu.addItem (addId, "Add folder...");
    if (! folders.isEmpty())
    {
        PopupMenu rm;
        for (int i = 0; i < folders.size(); ++i) rm.addItem (removeBase + i, File (folders[i]).getFileName());
        menu.addSubMenu ("Remove folder", rm);
    }
    if (current.existsAsFile()) menu.addItem (revealId, "Show current sample in Finder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (browseBtn).withMinimumWidth (220),
        [this, folders] (int id)
        {
            if (id <= 0) return;
            if (id == addId) addFolder();
            else if (id >= revealId) proc.getSampleFile().revealToUser();
            else if (id >= removeBase) { library.remove (File (folders[id - removeBase])); setStatus ("Removed " + File (folders[id - removeBase]).getFileName() + " from Browse."); }
            else if (id <= (int) menuFiles.size()) loadFile (menuFiles[(size_t) id - 1]);
        });
}

void PrizanEditor::addFolder()
{
    chooser = std::make_unique<FileChooser> ("Add a sample folder", File::getSpecialLocation (File::userMusicDirectory));
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this] (const FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (! dir.isDirectory()) return;
                              library.add (dir);
                              setStatus ("Added " + dir.getFileName() + ". Open BROWSE to pick a sample.");
                          });
}

void PrizanEditor::step (int delta)
{
    const auto current = proc.getSampleFile();
    if (! current.existsAsFile()) { setStatus ("Load a sample from a folder first, then step through it."); return; }
    auto files = current.getParentDirectory().findChildFiles (File::findFiles, false, audioWildcard);
    files.sort();
    if (files.isEmpty()) return;
    int i = files.indexOf (current);
    i = ((i < 0 ? 0 : i + delta) % files.size() + files.size()) % files.size();
    loadFile (files[i]);
    proc.audition();
}

//==============================================================================
void PrizanEditor::paint (Graphics& g)
{
    g.fillAll (colours::room);
    const auto panel = layout::panel.toFloat();
    g.setGradientFill (ColourGradient (colours::panelHi, 0, panel.getY(), colours::panel, 0, panel.getY() + 120.0f, false));
    g.fillRoundedRectangle (panel, 8.0f);
    // fine horizontal grain in the plastic
    for (int y = layout::panel.getY() + 2; y < layout::panel.getBottom() - 2; y += 2)
    {
        g.setColour ((y / 2) % 2 ? Colours::white.withAlpha (0.012f) : Colours::black.withAlpha (0.03f));
        g.fillRect (layout::panel.getX() + 2, y, layout::panel.getWidth() - 4, 1);
    }
    g.setColour (Colours::black);
    g.drawRoundedRectangle (panel, 8.0f, 1.5f);

    // corner screws
    for (auto pt : { Point<float> (20, 20), Point<float> (1120, 20), Point<float> (20, 440), Point<float> (1120, 440) })
    {
        const auto r = Rectangle<float> (9, 9).withCentre (pt);
        g.setGradientFill (ColourGradient (Colour (0xff8a8d92), r.getX(), r.getY(), Colour (0xff3a3c40), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawLine (pt.x - 3.0f, pt.y + 1.5f, pt.x + 3.0f, pt.y - 1.5f, 1.2f);
    }

    // badge: italic wordmark + model plate, centred
    {
        const String word = "PRIZAN", model = "PZ-1";
        const auto wf = sans (30.0f, Font::bold | Font::italic).withExtraKerningFactor (0.14f);
        const auto mf = sans (13.0f, Font::bold).withExtraKerningFactor (0.06f);
        const float ww = GlyphArrangement::getStringWidth (wf, word), mw = GlyphArrangement::getStringWidth (mf, model) + 14.0f;
        const float total = ww + 10.0f + mw, x = (float) getWidth() * 0.5f - total * 0.5f;
        g.setColour (colours::silk);
        g.setFont (wf);
        g.drawText (word, Rectangle<float> (x, 18.0f, ww + 6.0f, 36.0f), Justification::centredLeft, false);
        const auto plate = Rectangle<float> (x + ww + 10.0f, 27.0f, mw, 19.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (plate, 2.0f);
        g.setColour (Colour (0xff1a1206));
        g.setFont (mf);
        g.drawText (model, plate, Justification::centred, false);
    }
    g.setColour (colours::silkDim);
    g.setFont (sans (11.0f, Font::bold).withExtraKerningFactor (0.18f));
    g.drawText ("16-VOICE ONE-SHOT SAMPLER", 820, 26, 292, 20, Justification::centredRight);

    // accent stripes
    g.setColour (colours::accent);
    g.fillRect (28, 60, 1084, 3);
    g.setColour (colours::silkDim.withAlpha (0.5f));
    g.fillRect (28, 65, 1084, 1);

    // knob labels and values
    const char* names[] = { "ATTACK", "DECAY", "RELEASE", "FLANGER", "PITCH", "TONE" };
    Slider* knobs[] = { &attack, &decay, &release, &flanger, &pitch, &tone };
    for (int i = 0; i < 6; ++i)
    {
        const auto kb = knobs[i]->getBounds();
        g.setColour (colours::silk);
        g.setFont (sans (11.5f, Font::bold).withExtraKerningFactor (0.12f));
        g.drawText (names[i], kb.getX() - 12, kb.getY() - 20, kb.getWidth() + 24, 16, Justification::centred);
        g.setColour (colours::silkDim);
        g.setFont (mono (11.0f));
        g.drawText (knobs[i]->getTextFromValue (knobs[i]->getValue()), kb.getX() - 12, kb.getBottom(), kb.getWidth() + 24, 16, Justification::centred);
    }
    // engraved divider
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRect (962, 80, 1, 250);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillRect (963, 80, 1, 250);

    const auto wk = width.getBounds();
    g.setColour (colours::silk);
    g.setFont (sans (14.0f, Font::bold).withExtraKerningFactor (0.2f));
    g.drawText ("WIDTH", wk.getX(), wk.getY() - 26, wk.getWidth(), 20, Justification::centred);
    g.setColour (colours::silkDim);
    g.setFont (mono (12.0f));
    g.drawText (width.getTextFromValue (width.getValue()), wk.getX(), wk.getBottom() + 2, wk.getWidth(), 16, Justification::centred);
    g.setFont (sans (10.5f, Font::bold).withExtraKerningFactor (0.1f));
    g.drawFittedText ("MONO  /  100% AS RECORDED\n/  200% WIDE", wk.getX() - 16, wk.getBottom() + 24, wk.getWidth() + 32, 30, Justification::centredTop, 2);

    // common bar
    {
        Path bar;
        bar.addRoundedRectangle ((float) layout::panel.getX() + 1.5f, (float) layout::barTop, (float) layout::panel.getWidth() - 3.0f,
                                 (float) (layout::panel.getBottom() - layout::barTop) - 1.5f, 7.0f, 7.0f, false, false, true, true);
        g.setColour (colours::panelLo);
        g.fillPath (bar);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRect (layout::panel.getX() + 2, layout::barTop, layout::panel.getWidth() - 4, 1);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRect (layout::panel.getX() + 2, layout::barTop + 1, layout::panel.getWidth() - 4, 1);
    }
    const char* barNames[] = { "MIX", "FLANGE RATE", "FEEDBACK", "VOLUME" };
    Slider* bars[] = { &mix, &rate, &feedback, &volume };
    for (int i = 0; i < 4; ++i)
    {
        const auto cell = bars[i]->getBounds();
        g.setColour (colours::silk);
        g.setFont (sans (11.5f, Font::bold).withExtraKerningFactor (0.1f));
        g.drawText (barNames[i], cell.getX() - 92, cell.getY(), 86, cell.getHeight(), Justification::centredRight);
        g.setColour (colours::silkDim);
        g.setFont (mono (11.0f));
        g.drawText (bars[i]->getTextFromValue (bars[i]->getValue()), cell.getRight() + 8, cell.getY(), 64, cell.getHeight(), Justification::centredLeft);
    }
    g.setColour (colours::silkDim);
    g.setFont (sans (12.5f));
    g.drawText (status.isNotEmpty() ? status : String ("Play MIDI notes (C3 = original pitch), click the screen, or press Space."),
                layout::status, Justification::centredLeft, true);
}

void PrizanEditor::resized()
{
    display.setBounds (layout::display);
    browseBtn.setBounds (layout::browse);
    prevBtn.setBounds (layout::prev);
    nextBtn.setBounds (layout::next);
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
        bars[i]->setBounds (layout::sliderRow.getX() + i * cellW + 96, layout::sliderRow.getY(), cellW - 96 - 76, layout::sliderRow.getHeight());

    int x = 1112;
    const int y = 404, h = 32;
    auto place = [&] (Component& c, int w) { x -= w; c.setBounds (x, y, w, h); x -= 8; };
    place (dragOut, 118);
    place (saveBtn, 92);
    place (playBtn, 64);
    place (loadBtn, 90);
    place (reverseBtn, 92);
}

void PrizanEditor::timerCallback()
{
    display.refreshPoints();
    display.tick();
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
    if (k == KeyPress::leftKey) { step (-1); return true; }
    if (k == KeyPress::rightKey) { step (1); return true; }
    return false;
}

bool PrizanEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isAudio (File (f)) || File (f).isDirectory()) return true;
    return false;
}

void PrizanEditor::fileDragEnter (const StringArray&, int, int) { display.dropHighlight = true; display.repaint(); }
void PrizanEditor::fileDragExit (const StringArray&) { display.dropHighlight = false; display.repaint(); }

void PrizanEditor::filesDropped (const StringArray& files, int, int)
{
    display.dropHighlight = false;
    for (auto& path : files)
    {
        const File f (path);
        if (f.isDirectory()) { library.add (f); setStatus ("Added folder " + f.getFileName() + " to BROWSE."); break; }
        if (isAudio (f)) { loadFile (f); break; }
    }
    display.repaint();
}

void PrizanEditor::loadFile (const File& f)
{
    String err;
    if (proc.loadSampleFile (f, err)) setStatus ("Loaded " + f.getFileName() + ". Arrow keys step through the folder.");
    else setStatus (err);
}
