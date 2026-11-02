#pragma once

// Prizan sound engine: one-shot voices -> tone -> stereo flanger -> width -> mix/volume.
// Mirrors the web version (index.html) so a sound made in one matches the other.

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

namespace prizan
{

struct EnvPoint { float x = 0.5f, y = 1.0f, b = 0.0f; };

struct Params
{
    float attack = 0.002f, decay = 10.0f, release = 0.08f, flanger = 0.3f, pitch = 0.0f, tone = 0.0f,
          width = 1.0f, mix = 1.0f, rate = 0.3f, feedback = 0.45f, volume = 0.8f;
    float endBend = 0.0f;
    int numPoints = 0;
    std::array<EnvPoint, 32> points {};
};

//==============================================================================
// Envelope. The timeline is the output length (sample length after pitch); release always lands on the end.
inline double adrAt (const Params& p, double t, double T)
{
    const double R = std::min ((double) p.release, T * 0.98);
    const double A = std::min ((double) p.attack, std::max (0.0005, T - R));
    const bool off = p.decay >= 9.99f;
    const double tau = p.decay / 4.6;
    auto pre = [&] (double x) { return (x < A ? x / A : 1.0) * ((off || x < A) ? 1.0 : std::exp (-(x - A) / tau)); };
    const double rs = T - R;
    if (t <= rs) return pre (t);
    return pre (rs) * std::max (0.0, 1.0 - (t - rs) / R);
}

// Hand-drawn points scale the knob envelope. Each point's b bends the segment that ends at it.
inline double shapeAt (const Params& p, double u)
{
    if (p.numPoints == 0) return 1.0;
    double x0 = 0.0, y0 = 1.0;
    for (int i = 0; i <= p.numPoints; ++i)
    {
        const bool end = i == p.numPoints;
        const double px = end ? 1.0 : p.points[(size_t) i].x;
        const double py = end ? 1.0 : p.points[(size_t) i].y;
        const double pb = end ? p.endBend : p.points[(size_t) i].b;
        if (u <= px)
        {
            const double span = px - x0, f = span > 0.0 ? (u - x0) / span : 1.0;
            return y0 + (py - y0) * std::pow (std::max (0.0, f), std::pow (2.0, pb * 3.0));
        }
        x0 = px; y0 = py;
    }
    return 1.0;
}

inline double envAt (const Params& p, double t, double T) { return adrAt (p, t, T) * shapeAt (p, t / T); }

//==============================================================================
struct Source : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Source>;
    juce::AudioBuffer<float> buffer;   // always 2 channels, reverse and stretch already applied
    double sampleRate = 44100.0;
    double duration() const { return buffer.getNumSamples() / sampleRate; }
};

// Pitch-preserving stretch (WSOLA): overlapping grains, each nudged to line up with the last.
inline juce::AudioBuffer<float> wsolaStretch (const juce::AudioBuffer<float>& in, double s)
{
    constexpr int N = 2048, Hs = 512, tol = 384;
    const double Ha = Hs / s;
    const int len = in.getNumSamples(), chs = in.getNumChannels();
    const int outLen = std::max (1, (int) std::lround (len * s));

    std::vector<float> win ((size_t) N), mono ((size_t) len, 0.0f), norm ((size_t) (outLen + N), 0.0f);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
    for (int c = 0; c < chs; ++c)
        for (int i = 0; i < len; ++i) mono[(size_t) i] += in.getSample (c, i);

    juce::AudioBuffer<float> out (chs, outLen + N);
    out.clear();
    int prev = 0;
    for (int k = 0; (long long) k * Hs < outLen; ++k)
    {
        const int target = (int) std::lround (k * Ha);
        int best = std::min (target, std::max (0, len - 1));
        if (k > 0)
        {
            const int ref = prev + Hs;
            double bc = -1.0e30;
            for (int d = -tol; d <= tol; d += 2)
            {
                const int p = target + d;
                if (p < 0 || p >= len) continue;
                double c = 0.0;
                for (int i = 0; i < 1024 && ref + i < len && p + i < len; i += 4)
                    c += (double) mono[(size_t) (ref + i)] * mono[(size_t) (p + i)];
                if (c > bc) { bc = c; best = p; }
            }
        }
        const int o = k * Hs;
        for (int i = 0; i < N; ++i)
        {
            const float w = (k == 0 && i < N / 2) ? 1.0f : win[(size_t) i]; // first grain keeps the transient sharp
            const int idx = best + i;
            if (idx < len)
                for (int c = 0; c < chs; ++c) out.addSample (c, o + i, in.getSample (c, idx) * w);
            norm[(size_t) (o + i)] += w;
        }
        prev = best;
    }

    juce::AudioBuffer<float> r (chs, outLen);
    for (int c = 0; c < chs; ++c)
        for (int i = 0; i < outLen; ++i)
        {
            const float n = norm[(size_t) i];
            r.setSample (c, i, n > 1.0e-3f ? out.getSample (c, i) / std::max (n, 0.5f) : 0.0f);
        }
    return r;
}

//==============================================================================
// Topology-preserving state-variable filter (12 dB/oct), low and high outputs.
struct Svf
{
    double ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.41421356;
    void set (double fc, double sr)
    {
        fc = juce::jlimit (10.0, sr * 0.49, fc);
        const double g = std::tan (juce::MathConstants<double>::pi * fc / sr);
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    void tick (double v0, double& low, double& high)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
        low = v2; high = v0 - k * v1 - v2;
    }
};

//==============================================================================
class Engine
{
public:
    void prepare (double sr, int maxBlock)
    {
        hostRate = sr;
        maxBlockSize = std::max (16, maxBlock);
        bus.setSize (2, maxBlockSize);
        flSize = (int) (sr * 0.05) + 4;
        for (auto& b : flBuf) b.assign ((size_t) flSize, 0.0f);
        haasDelay = (int) std::lround (sr * 0.013);
        haasSize = haasDelay + 4;
        haasBuf.assign ((size_t) haasSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        for (auto& b : flBuf) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (haasBuf.begin(), haasBuf.end(), 0.0f);
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        hh.reset();
        flW = haasW = 0; lfoPhase = 0.25; first = true;
    }

    void setSource (Source::Ptr s) { source = std::move (s); for (auto& v : voices) v.active = false; }

    void noteOn (int note, float vel, const Params& p)
    {
        if (source == nullptr || source->buffer.getNumSamples() < 2) return;
        Voice* v = nullptr;
        for (auto& c : voices) if (! c.active) { v = &c; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& c : voices) if (c.age < v->age) v = &c;
        }
        const double rate = std::pow (2.0, (note - 60 + p.pitch) / 12.0);
        v->active = true; v->pos = 0.0; v->t = 0.0; v->vel = vel; v->age = ++ageCounter;
        v->inc = rate * source->sampleRate / hostRate;
        v->T = source->duration() / rate;
    }

    void allOff() { for (auto& v : voices) v.active = false; }

    // progress (0..1) of the newest sounding voice, -1 when silent
    float progress() const
    {
        const Voice* n = nullptr;
        for (auto& v : voices) if (v.active && (n == nullptr || v.age > n->age)) n = &v;
        return n == nullptr ? -1.0f : (float) (n->t / n->T);
    }

    void process (juce::AudioBuffer<float>& out, int start, int num, const Params& p)
    {
        while (num > 0)
        {
            const int n = std::min (num, maxBlockSize);
            processChunk (out, start, n, p);
            start += n; num -= n;
        }
    }

private:
    struct Voice { bool active = false; double pos = 0, inc = 1, t = 0, T = 1; float vel = 1; uint64_t age = 0; };
    struct Gains { float dry, wet, fb, width, haas, mix, vol; };

    void processChunk (juce::AudioBuffer<float>& out, int start, int n, const Params& p)
    {
        float* L = bus.getWritePointer (0);
        float* R = bus.getWritePointer (1);
        juce::FloatVectorOperations::clear (L, n);
        juce::FloatVectorOperations::clear (R, n);

        if (source != nullptr)
        {
            const float* sL = source->buffer.getReadPointer (0);
            const float* sR = source->buffer.getReadPointer (1);
            const int len = source->buffer.getNumSamples();
            const double dt = 1.0 / hostRate;
            for (auto& v : voices)
            {
                for (int i = 0; i < n && v.active; ++i)
                {
                    const int i0 = (int) v.pos;
                    if (v.t >= v.T || i0 >= len - 1) { v.active = false; break; }
                    const float fr = (float) (v.pos - i0);
                    const float g = (float) envAt (p, v.t, v.T) * v.vel;
                    L[i] += (sL[i0] + (sL[i0 + 1] - sL[i0]) * fr) * g;
                    R[i] += (sR[i0] + (sR[i0 + 1] - sR[i0]) * fr) * g;
                    v.pos += v.inc; v.t += dt;
                }
            }
        }

        // tone: left darkens (low-pass), right thins (high-pass)
        const double lpF = p.tone < 0 ? 20000.0 * std::pow (2.0, p.tone * 7.0) : 20000.0;
        const double hpF = p.tone > 0 ? 20.0 * std::pow (2.0, p.tone * 7.5) : 20.0;
        for (int c = 0; c < 2; ++c) { lp[c].set (lpF, hostRate); hp[c].set (hpF, hostRate); }
        hh.set (280.0, hostRate);

        const float amp = p.flanger * 0.0034f, base = 0.0006f + amp;
        const Gains target { 1.0f - p.flanger * 0.4f, p.flanger * 0.55f, p.feedback, p.width,
                             std::max (0.0f, p.width - 1.0f) * 0.32f, p.mix, p.volume };
        if (first) { cur = target; first = false; }
        const double lfoInc = p.rate / hostRate;

        const int outChans = out.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            const float a = (float) (i + 1) / (float) n;
            auto lerp = [a] (float x, float y) { return x + (y - x) * a; };
            const Gains g { lerp (cur.dry, target.dry), lerp (cur.wet, target.wet), lerp (cur.fb, target.fb),
                            lerp (cur.width, target.width), lerp (cur.haas, target.haas),
                            lerp (cur.mix, target.mix), lerp (cur.vol, target.vol) };

            const double plain[2] = { L[i], R[i] };
            double x[2] = { L[i], R[i] }, lo, hi;
            for (int c = 0; c < 2; ++c)
            {
                hp[c].tick (x[c], lo, hi); x[c] = hi;
                lp[c].tick (x[c], lo, hi); x[c] = lo;
            }

            // flanger: stereo pair of short modulated delays, LFO inverted on the right
            const double tri = lfoPhase < 0.5 ? 4.0 * lfoPhase - 1.0 : 3.0 - 4.0 * lfoPhase;
            lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            double f[2];
            for (int c = 0; c < 2; ++c)
            {
                const double d = (base + (c == 0 ? amp : -amp) * tri) * hostRate;
                double rp = flW - d; if (rp < 0) rp += flSize;
                const int r0 = (int) rp, r1 = (r0 + 1) % flSize;
                const double frac = rp - r0;
                auto& buf = flBuf[(size_t) c];
                const double y = buf[(size_t) r0] + (buf[(size_t) r1] - buf[(size_t) r0]) * frac;
                buf[(size_t) flW] = (float) (x[c] + g.fb * y);
                f[c] = g.dry * x[c] + g.wet * y;
            }
            flW = (flW + 1) % flSize;

            // width: mid/side; above 100% a delayed, high-passed copy of mid widens mono samples
            const double mid = (f[0] + f[1]) * 0.5;
            double side = (f[0] - f[1]) * 0.5;
            haasBuf[(size_t) haasW] = (float) mid;
            const double hv = haasBuf[(size_t) ((haasW - haasDelay + haasSize) % haasSize)];
            haasW = (haasW + 1) % haasSize;
            hh.tick (hv, lo, hi);
            side = (side + hi * g.haas) * g.width;

            const double o0 = (plain[0] * (1.0 - g.mix) + (mid + side) * g.mix) * g.vol;
            const double o1 = (plain[1] * (1.0 - g.mix) + (mid - side) * g.mix) * g.vol;
            if (outChans >= 2) { out.setSample (0, start + i, (float) o0); out.setSample (1, start + i, (float) o1); }
            else if (outChans == 1) out.setSample (0, start + i, (float) ((o0 + o1) * 0.5));
        }
        cur = target;
    }

    Source::Ptr source;
    std::array<Voice, 16> voices {};
    uint64_t ageCounter = 0;
    double hostRate = 44100.0;
    int maxBlockSize = 512;
    juce::AudioBuffer<float> bus;
    Svf lp[2], hp[2], hh;
    std::array<std::vector<float>, 2> flBuf;
    std::vector<float> haasBuf;
    int flSize = 1, flW = 0, haasSize = 1, haasW = 0, haasDelay = 1;
    double lfoPhase = 0.25;
    Gains cur {};
    bool first = true;
};

} // namespace prizan
