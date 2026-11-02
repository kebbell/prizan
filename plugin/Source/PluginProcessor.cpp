#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             nullptr };
}

juce::String ms (float v)
{
    if (v >= 1.0f) return juce::String (v, 2) + " s";
    if (v < 0.01f) return juce::String (v * 1000.0f, 1) + " ms";
    return juce::String (juce::roundToInt (v * 1000.0f)) + " ms";
}
juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
juce::String sgn (float v) { return v > 0 ? "+" : ""; }

using Attr = juce::AudioParameterFloatAttributes;
std::unique_ptr<juce::AudioParameterFloat> param (const char* id, const char* name, juce::NormalisableRange<float> r,
                                                 float def, std::function<juce::String (float)> fmt)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
        Attr().withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); }));
}

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (param ("attack",  "Attack",  logRange (0.0005f, 2.0f), 0.002f, ms));
    p.push_back (param ("decay",   "Decay",   logRange (0.03f, 10.0f),  10.0f,  [] (float v) { return v >= 9.99f ? juce::String ("Off") : ms (v); }));
    p.push_back (param ("release", "Release", logRange (0.005f, 3.0f),  0.08f,  ms));
    p.push_back (param ("flanger", "Flanger", { 0.0f, 1.0f }, 0.3f, pct));
    p.push_back (param ("pitch",   "Pitch",   { -24.0f, 24.0f, 0.01f }, 0.0f, [] (float v)
    {
        const float r = std::round (v * 100.0f) / 100.0f;
        if (r == 0.0f) return juce::String ("0 st");
        return sgn (r) + (std::abs (r - std::round (r)) < 0.001f ? juce::String ((int) std::round (r)) : juce::String (r, 2)) + " st";
    }));
    p.push_back (param ("tone",    "Tone",    { -1.0f, 1.0f }, 0.0f, [] (float v)
    {
        return std::abs (v) < 0.005f ? juce::String ("Flat") : sgn (v) + juce::String (juce::roundToInt (v * 100.0f)) + "%";
    }));
    p.push_back (param ("width",   "Width",   { 0.0f, 2.0f }, 1.0f, [] (float v) { return v < 0.005f ? juce::String ("Mono") : pct (v); }));
    p.push_back (param ("mix",     "Mix",     { 0.0f, 1.0f }, 1.0f, [] (float v) { return pct (v) + " fx"; }));
    p.push_back (param ("rate",    "Flange rate", logRange (0.05f, 8.0f), 0.3f, [] (float v) { return juce::String (v, 2) + " Hz"; }));
    p.push_back (param ("feedback","Feedback", { 0.0f, 0.9f }, 0.45f, [] (float v) { return pct (v / 0.9f); }));
    p.push_back (param ("volume",  "Volume",  { 0.0f, 1.0f }, 0.8f, pct));
    p.push_back (param ("stretch", "Length",  logRange (0.25f, 4.0f), 1.0f, pct));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "reverse", 1 }, "Reverse", false));
    return { p.begin(), p.end() };
}
} // namespace

//==============================================================================
PrizanProcessor::PrizanProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PRIZAN", makeLayout())
{
    pAttack = raw ("attack"); pDecay = raw ("decay"); pRelease = raw ("release"); pFlanger = raw ("flanger");
    pPitch = raw ("pitch"); pTone = raw ("tone"); pWidth = raw ("width"); pMix = raw ("mix"); pRate = raw ("rate");
    pFeedback = raw ("feedback"); pVolume = raw ("volume"); pReverse = raw ("reverse"); pStretch = raw ("stretch");

    formats.registerBasicFormats();
    apvts.addParameterListener ("reverse", this);
    apvts.addParameterListener ("stretch", this);
    engine.prepare (44100.0, 512);
    loadDemo();
    startTimerHz (8);
}

PrizanProcessor::~PrizanProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("reverse", this);
    apvts.removeParameterListener ("stretch", this);
}

void PrizanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType l (engineLock);
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setSource (current);
}

bool PrizanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

prizan::Params PrizanProcessor::currentParams() const
{
    prizan::Params p;
    p.attack = pAttack->load(); p.decay = pDecay->load(); p.release = pRelease->load(); p.flanger = pFlanger->load();
    p.pitch = pPitch->load(); p.tone = pTone->load(); p.width = pWidth->load(); p.mix = pMix->load();
    p.rate = pRate->load(); p.feedback = pFeedback->load(); p.volume = pVolume->load();
    const juce::SpinLock::ScopedLockType l (pointsLock);
    p.endBend = endBend;
    p.numPoints = (int) std::min (points.size(), p.points.size());
    for (int i = 0; i < p.numPoints; ++i) p.points[(size_t) i] = points[(size_t) i];
    return p;
}

void PrizanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto& p = audioParams;
    p.attack = pAttack->load(); p.decay = pDecay->load(); p.release = pRelease->load(); p.flanger = pFlanger->load();
    p.pitch = pPitch->load(); p.tone = pTone->load(); p.width = pWidth->load(); p.mix = pMix->load();
    p.rate = pRate->load(); p.feedback = pFeedback->load(); p.volume = pVolume->load();
    {
        const juce::SpinLock::ScopedTryLockType tl (pointsLock);
        if (tl.isLocked())
        {
            p.endBend = endBend;
            p.numPoints = (int) std::min (points.size(), p.points.size());
            for (int i = 0; i < p.numPoints; ++i) p.points[(size_t) i] = points[(size_t) i];
        }
    }

    const juce::SpinLock::ScopedTryLockType lock (engineLock);
    if (! lock.isLocked()) return; // the sample is being swapped this instant

    if (pendingAudition.exchange (false)) engine.noteOn (60, 1.0f, p);

    const int n = buffer.getNumSamples();
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.process (buffer, pos, at - pos, p); pos = at; }
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
        // one-shot: note-offs are ignored, the sample always plays out with its release
    }
    if (pos < n) engine.process (buffer, pos, n - pos, p);
    progress = engine.progress();
}

//==============================================================================
void PrizanProcessor::rebuildSource()
{
    if (original.getNumSamples() < 2) return;
    juce::AudioBuffer<float> b;
    b.makeCopyOf (original);
    if (pReverse->load() > 0.5f)
        for (int c = 0; c < b.getNumChannels(); ++c) b.reverse (c, 0, b.getNumSamples());
    const double st = pStretch->load();
    if (std::abs (st - 1.0) > 0.005) b = prizan::wsolaStretch (b, st);

    prizan::Source::Ptr s = new prizan::Source();
    s->buffer = std::move (b);
    s->sampleRate = originalRate;

    prizan::Source::Ptr old;
    {
        const juce::SpinLock::ScopedLockType l (engineLock);
        old = current;
        current = s;
        engine.setSource (current);
    }
    // the old buffer is freed here, on the message thread
}

void PrizanProcessor::setOriginal (juce::AudioBuffer<float>&& stereo, double sr, const juce::String& name, int channels, const juce::File& file)
{
    original = std::move (stereo);
    originalRate = sr;
    sampleFile = file;

    info.name = name;
    info.sampleRate = sr;
    info.channels = channels;
    const int len = original.getNumSamples();
    info.duration = len / sr;
    constexpr int bins = 2048;
    info.mins.assign (bins, 0.0f);
    info.maxs.assign (bins, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int a = (int) ((long long) b * len / bins), z = std::max (a + 1, (int) ((long long) (b + 1) * len / bins));
        float lo = 0, hi = 0;
        for (int i = a; i < z && i < len; ++i)
        {
            const float v = (original.getSample (0, i) + original.getSample (1, i)) * 0.5f;
            lo = std::min (lo, v); hi = std::max (hi, v);
        }
        info.mins[(size_t) b] = lo; info.maxs[(size_t) b] = hi;
    }
    rebuildSource();
    ++sampleVersion;
}

bool PrizanProcessor::loadSampleFile (const juce::File& f, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr) { error = "Couldn't read that file. Use WAV, AIFF, MP3 or FLAC."; return false; }
    if (r->lengthInSamples > (juce::int64) (r->sampleRate * 60.0)) { error = "That file is over 60 seconds. Use a one-shot."; return false; }

    const int len = (int) r->lengthInSamples;
    const int chs = (int) std::max (1u, r->numChannels);
    juce::AudioBuffer<float> in (chs, len);
    r->read (&in, 0, len, 0, true, true);

    // trim leading silence so hits land on time
    int startAt = len;
    for (int c = 0; c < chs; ++c)
        for (int i = 0; i < std::min (startAt, len); ++i)
            if (std::abs (in.getSample (c, i)) > 0.003f) { startAt = i; break; }
    if (startAt >= len) { error = "That file is silent. Try another one-shot."; return false; }
    startAt = std::max (0, startAt - (int) (r->sampleRate * 0.002));

    const int outLen = len - startAt;
    juce::AudioBuffer<float> st (2, outLen);
    st.copyFrom (0, 0, in, 0, startAt, outLen);
    st.copyFrom (1, 0, in, chs > 1 ? 1 : 0, startAt, outLen);
    setOriginal (std::move (st), r->sampleRate, f.getFileNameWithoutExtension(), chs, f);
    return true;
}

void PrizanProcessor::loadDemo()
{
    constexpr double sr = 44100.0, T = 1.6, f0 = 220.0;
    const int len = (int) (sr * T);
    juce::AudioBuffer<float> b (2, len);
    b.clear();
    auto add = [&] (int ch, double f, double a, double d)
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr, k = std::exp (-d / sr);
        double e = a;
        for (int i = 0; i < len; ++i) { b.addSample (ch, i, (float) (e * std::sin (w * i))); e *= k; }
    };
    for (int n = 1; n <= 30 && f0 * n <= 16000.0; ++n)
    {
        const double a = 1.0 / n, d = 1.6 + n * 0.28;
        add (0, (f0 - 0.6) * n, a * 0.6, d);  add (1, (f0 - 0.6) * n, a * 0.25, d);
        add (0, (f0 + 0.9) * n, a * 0.25, d); add (1, (f0 + 0.9) * n, a * 0.6, d);
    }
    add (0, f0 / 2, 0.35, 1.8); add (1, f0 / 2, 0.35, 1.8);
    const float pk = std::max (b.getMagnitude (0, 0, len), b.getMagnitude (1, 0, len));
    for (int i = 0; i < len; ++i)
    {
        const float g = 0.9f / pk * std::min (1.0f, (float) i / 40.0f);
        b.setSample (0, i, b.getSample (0, i) * g);
        b.setSample (1, i, b.getSample (1, i) * g);
    }
    setOriginal (std::move (b), sr, "Demo stab", 2, {});
}

//==============================================================================
void PrizanProcessor::getPoints (std::vector<prizan::EnvPoint>& pts, float& bend) const
{
    const juce::SpinLock::ScopedLockType l (pointsLock);
    pts = points; bend = endBend;
}

void PrizanProcessor::setPoints (const std::vector<prizan::EnvPoint>& pts, float bend)
{
    {
        const juce::SpinLock::ScopedLockType l (pointsLock);
        points = pts;
        if (points.size() > 32) points.resize (32);
        endBend = bend;
    }
    ++pointsVersion;
}

juce::String PrizanProcessor::suggestedRenderName() const
{
    auto base = (info.name + "-prizan").retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ").trim();
    return base.isEmpty() ? juce::String ("prizan") : base;
}

bool PrizanProcessor::renderToFile (const juce::File& dest, juce::String& error)
{
    prizan::Source::Ptr src;
    {
        const juce::SpinLock::ScopedLockType l (engineLock);
        src = current;
    }
    if (src == nullptr) { error = "Load a sample first."; return false; }

    const auto p = currentParams();
    const double sr = src->sampleRate;
    const double T = src->duration() / std::pow (2.0, p.pitch / 12.0);
    const double tail = 0.05 + p.flanger * p.feedback * 0.4;
    const int total = (int) std::ceil ((T + tail) * sr);

    prizan::Engine e;
    e.prepare (sr, 1024);
    e.setSource (src);
    e.noteOn (60, 1.0f, p);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    e.process (out, 0, total, p);
    const float pk = std::max (out.getMagnitude (0, 0, total), out.getMagnitude (1, 0, total));
    if (pk > 0.98f) out.applyGain (0.98f / pk);

    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    auto os = dest.createOutputStream();
    if (os == nullptr) { error = "Couldn't write to " + dest.getFullPathName(); return false; }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) { error = "Couldn't create the WAV file."; return false; }
    os.release(); // the writer owns the stream now
    w->writeFromAudioSampleBuffer (out, 0, total);
    return true;
}

//==============================================================================
void PrizanProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("Points"), nullptr);
    juce::ValueTree pts ("Points");
    std::vector<prizan::EnvPoint> cp; float bend = 0;
    getPoints (cp, bend);
    pts.setProperty ("endBend", bend, nullptr);
    for (auto& pt : cp)
    {
        juce::ValueTree c ("P");
        c.setProperty ("x", pt.x, nullptr); c.setProperty ("y", pt.y, nullptr); c.setProperty ("b", pt.b, nullptr);
        pts.appendChild (c, nullptr);
    }
    state.appendChild (pts, nullptr);
    state.setProperty ("sampleFile", sampleFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PrizanProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);

    std::vector<prizan::EnvPoint> cp;
    const auto pts = state.getChildWithName ("Points");
    for (const auto& c : pts)
        cp.push_back ({ (float) c.getProperty ("x"), (float) c.getProperty ("y"), (float) c.getProperty ("b") });
    setPoints (cp, (float) pts.getProperty ("endBend", 0.0f));

    const juce::String path = state.getProperty ("sampleFile").toString();
    apvts.replaceState (state);
    if (path.isNotEmpty())
    {
        juce::String err;
        if (! loadSampleFile (juce::File (path), err)) rebuildSource();
    }
    else rebuildSource();
}

juce::AudioProcessorEditor* PrizanProcessor::createEditor() { return new PrizanEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrizanProcessor(); }
