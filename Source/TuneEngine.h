// Aura Tune — real-time pitch correction engine
// Pure C++ (no JUCE) so it can be unit-tested offline.
//
//  * Pitch detection : YIN on a decimated signal
//  * Pitch shifting  : TD-PSOLA (pitch-synchronous overlap-add) — keeps formants,
//                      so the voice does not get "chipmunk" colouring.
//  * Correction      : Retune Speed / Humanize / Flex-Tune / Key & Scale (Antares-style)
//  * Softness        : Note Transition glide + smooth 2-stage shaping (Waves-style)
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace aura
{

enum class ScaleType
{
    Chromatic = 0, Major, Minor, HarmonicMinor, MajorPentatonic,
    MinorPentatonic, Blues, Dorian, Mixolydian, Phrygian, NumScales
};

inline const char* scaleName (int i)
{
    static const char* names[] = { "Chromatic", "Major", "Minor", "Harmonic Minor",
                                   "Major Pentatonic", "Minor Pentatonic", "Blues",
                                   "Dorian", "Mixolydian", "Phrygian" };
    return names[std::clamp (i, 0, (int) ScaleType::NumScales - 1)];
}

/** 12-bit mask of pitch classes (bit 0 = C) for a key + scale. */
inline std::array<bool, 12> scaleMask (int key, int scale)
{
    static const std::vector<std::vector<int>> intervals = {
        { 0,1,2,3,4,5,6,7,8,9,10,11 },  // chromatic
        { 0,2,4,5,7,9,11 },             // major
        { 0,2,3,5,7,8,10 },             // natural minor
        { 0,2,3,5,7,8,11 },             // harmonic minor
        { 0,2,4,7,9 },                  // major pentatonic
        { 0,3,5,7,10 },                 // minor pentatonic
        { 0,3,5,6,7,10 },               // blues
        { 0,2,3,5,7,9,10 },             // dorian
        { 0,2,4,5,7,9,10 },             // mixolydian
        { 0,1,3,5,7,8,10 },             // phrygian
    };
    std::array<bool, 12> m {};
    for (int iv : intervals[(size_t) std::clamp (scale, 0, (int) intervals.size() - 1)])
        m[(size_t) ((key + iv) % 12)] = true;
    return m;
}

struct TuneParams
{
    int   key        = 0;      // 0 = C ... 11 = B
    int   scale      = 0;      // ScaleType
    std::array<bool, 12> removed {};   // notes switched off by the user

    // Antares-style correction
    float retuneMs   = 20.0f;  // 0..400 ms   (0 = hard "robot" tune)
    float humanize   = 0.0f;   // 0..1  slows correction on sustained notes
    float flexTune   = 0.0f;   // 0..1  only correct notes close to target
    float tracking   = 0.5f;   // 0..1  pitch detector strictness (relaxed = 1)
    int   inputType  = 1;      // 0 soprano, 1 alto/tenor, 2 low male, 3 instrument

    // Waves-style softness
    float transitionMs = 60.0f; // 0..500 ms glide between notes
    float softness     = 0.5f;  // 0..1  rounds every correction curve

    float transpose  = 0.0f;   // semitones
    float refHz      = 440.0f;
};

class TuneEngine
{
public:
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kMinHzGlobal = 65.0;   // lowest pitch we ever track (C2)
    static constexpr double kMaxHzGlobal = 1100.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        maxPeriod = (int) std::ceil (sr / kMinHzGlobal);
        latency   = 2 * maxPeriod + 32;

        size = 1;
        while (size < 4 * (latency + maxPeriod)) size <<= 1;
        mask = size - 1;
        input.assign ((size_t) size, 0.0f);
        acc.assign   ((size_t) size, 0.0f);
        wsum.assign  ((size_t) size, 0.0f);

        decim = sr >= 88200.0 ? 4 : (sr >= 44100.0 ? 2 : 1);
        dsr   = sr / decim;
        dSize = 1;
        while (dSize < 4 * (int) std::ceil (dsr / kMinHzGlobal)) dSize <<= 1;
        dMask = dSize - 1;
        dbuf.assign ((size_t) dSize, 0.0f);
        yin.assign  ((size_t) (dsr / kMinHzGlobal) + 4, 0.0f);

        hop = std::max (64, (int) std::round (sr * 0.005)); // detect every ~5 ms
        marks.assign (kMaxMarks, {});
        reset();
    }

    void reset()
    {
        std::fill (input.begin(), input.end(), 0.0f);
        std::fill (acc.begin(),   acc.end(),   0.0f);
        std::fill (wsum.begin(),  wsum.end(),  0.0f);
        std::fill (dbuf.begin(),  dbuf.end(),  0.0f);
        n = 0; dn = 0; decimAcc = 0; decimCount = 0; hopCount = 0;
        markHead = markCount = 0;
        lastMark = 0; ts = 0.0;
        period = (int) (sr / 150.0);
        voiced = false; unvoicedHops = 0; f0 = 0.0;
        target = -1; heldSec = 0.0; glide = glideVel = 0.0; glideInit = false;
        alignTarget = alignDelay = 0.0;
        desired = s1 = s2 = 0.0; corrWeight = 0.0;
        inputMidi.store (-1.0f); outputMidi.store (-1.0f); targetNote.store (-1);
        pushMark (0, period);
    }

    int getLatencySamples() const { return latency; }
    void setParams (const TuneParams& p) { params = p; }

    /** Process one mono sample. Returns {wet, dryAligned}. */
    inline void processSample (float x, float& wet, float& dry)
    {
        input[(size_t) (n & mask)] = x;

        // --- decimated copy for the pitch detector --------------------------
        decimAcc += x;
        if (++decimCount == decim)
        {
            dbuf[(size_t) (dn & dMask)] = decimAcc / (float) decim;
            ++dn; decimAcc = 0.0f; decimCount = 0;
        }
        if (++hopCount >= hop) { hopCount = 0; analyse(); }

        updateControl();

        // --- analysis pitch marks ------------------------------------------
        while (lastMark + 2 * period <= n)
        {
            lastMark += period;
            pushMark (lastMark, period);
        }

        // --- synthesis grains (TD-PSOLA) -------------------------------------
        {
            const double maxAlign = std::max (0.0, (double) (latency - 2 * period - 8));
            const double want = std::min (alignTarget, maxAlign);
            alignDelay += std::clamp (want - alignDelay, -1.0, 1.0);   // slew slowly, no jumps
            alignDelay = std::min (alignDelay, maxAlign);
        }
        const double ratio = std::pow (2.0, shiftSemis / 12.0);
        for (int guard = 0; guard < 8; ++guard)
        {
            const Mark* m = nearestMark ((long long) std::llround (ts));
            if (m == nullptr || ! m->closed || m->pos + m->right > n) break;
            const long long t = (long long) std::llround (ts);
            // apply the correction to the audio that was actually measured
            if (t + m->right > n - (long long) alignDelay) break;
            addGrain (t, *m);
            ts += std::max (1.0, (double) m->right / ratio);
        }

        // --- output -----------------------------------------------------------
        const long long e = n - latency;
        const size_t ei = (size_t) (e & mask);
        const float w = wsum[ei];
        wet = acc[ei] / std::max (w, 0.35f);
        acc[ei] = 0.0f; wsum[ei] = 0.0f;
        dry = input[ei];
        ++n;
    }

    // ---- read-outs for the UI (thread-safe) -------------------------------
    std::atomic<float> inputMidi { -1.0f };   // detected pitch (MIDI float), -1 = none
    std::atomic<float> outputMidi { -1.0f };  // corrected pitch
    std::atomic<int>   targetNote { -1 };     // target MIDI note

private:
    struct Mark { long long pos = 0; int left = 0; int right = 0; bool closed = false; };
    static constexpr int kMaxMarks = 512;

    void pushMark (long long pos, int per)
    {
        if (markCount > 0)
        {
            Mark& prev = marks[(size_t) ((markHead + markCount - 1) % kMaxMarks)];
            prev.right = per; prev.closed = true;
        }
        marks[(size_t) ((markHead + markCount) % kMaxMarks)] = { pos, per, per, false };
        if (markCount < kMaxMarks) ++markCount; else markHead = (markHead + 1) % kMaxMarks;
    }

    const Mark* nearestMark (long long t)
    {
        // drop marks that can never be chosen again
        while (markCount > 1)
        {
            const Mark& a = marks[(size_t) markHead];
            const Mark& b = marks[(size_t) ((markHead + 1) % kMaxMarks)];
            if (std::llabs (b.pos - t) <= std::llabs (a.pos - t)) { markHead = (markHead + 1) % kMaxMarks; --markCount; }
            else break;
        }
        return markCount > 0 ? &marks[(size_t) markHead] : nullptr;
    }

    void addGrain (long long t, const Mark& m)
    {
        // asymmetric Hann: left half spans the previous period, right half the next one,
        // so at ratio 1 neighbouring grains sum to exactly 1 (transparent pass-through)
        const long long firstWritable = n - latency + 1;   // never write into the past
        for (int k = -m.left + 1; k < m.right; ++k)
        {
            if (t + k < firstWritable) continue;
            const double P = k < 0 ? m.left : m.right;
            const float w = (float) (0.5 * (1.0 + std::cos (kPi * k / P)));
            const size_t oi = (size_t) ((t + k) & mask);
            acc[oi]  += input[(size_t) ((m.pos + k) & mask)] * w;
            wsum[oi] += w;
        }
    }

    // ---------------- pitch detection (YIN) ----------------------------------
    void analyse()
    {
        double minHz = 80, maxHz = 800;
        switch (params.inputType)
        {
            case 0: minHz = 150; maxHz = 1100; break;   // soprano
            case 1: minHz = 80;  maxHz = 800;  break;   // alto / tenor
            case 2: minHz = 65;  maxHz = 450;  break;   // low male
            default: minHz = 65; maxHz = 1100; break;   // instrument
        }
        const int tauMin = std::max (2, (int) (dsr / maxHz));
        const int tauMax = std::min ((int) yin.size() - 2, (int) (dsr / minHz));
        const int W = tauMax;
        const long long start = dn - W - tauMax;

        double energy = 0.0;
        for (int j = 0; j < W + tauMax; ++j)
        {
            const float v = dbuf[(size_t) ((start + j) & dMask)];
            energy += v * v;
        }
        const double rms = std::sqrt (energy / (W + tauMax));

        bool found = false; double tauEst = 0.0;
        if (rms > 0.003) // ~ -50 dBFS gate
        {
            yin[0] = 1.0f;
            double running = 0.0;
            for (int tau = 1; tau <= tauMax; ++tau)
            {
                double d = 0.0;
                for (int j = 0; j < W; ++j)
                {
                    const float a = dbuf[(size_t) ((dn - W + j) & dMask)];
                    const float b = dbuf[(size_t) ((dn - W + j - tau) & dMask)];
                    d += (double) (a - b) * (a - b);
                }
                running += d;
                yin[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
            }
            const double th = 0.07 + 0.2 * params.tracking;
            for (int tau = tauMin; tau < tauMax; ++tau)
            {
                if (yin[(size_t) tau] < th)
                {
                    while (tau + 1 < tauMax && yin[(size_t) tau + 1] < yin[(size_t) tau]) ++tau;
                    const double y0 = yin[(size_t) tau - 1], y1 = yin[(size_t) tau], y2 = yin[(size_t) tau + 1];
                    const double den = y0 - 2 * y1 + y2;
                    tauEst = tau + (std::abs (den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0.0);
                    found = true;
                    break;
                }
            }
        }

        const double hopSec = hop / sr;
        if (found)
        {
            double newF0 = dsr / tauEst;
            // light octave-jump guard
            if (voiced && f0 > 0.0)
            {
                const double r = newF0 / f0;
                if (std::abs (r - 2.0) < 0.08 || std::abs (r - 0.5) < 0.04) newF0 = f0;
            }
            f0 = newF0;
            voiced = true; unvoicedHops = 0;
            // the measurement describes audio ~ (W + tau)/2 samples ago (+ half a hop of staleness)
            alignTarget = decim * (W + tauEst) * 0.5 + hop * 0.5;
            period = std::clamp ((int) std::lround (sr / f0), (int) (sr / kMaxHzGlobal), maxPeriod);
        }
        else if (++unvoicedHops > 3)   // ~15 ms hold to bridge short dropouts
        {
            voiced = false;
            period = (int) (sr / 150.0);
        }

        // ---------------- target selection -------------------------------------
        if (voiced)
        {
            const double midi = 69.0 + 12.0 * std::log2 (f0 / params.refHz);
            auto allowed = scaleMask (params.key, params.scale);
            for (int i = 0; i < 12; ++i) if (params.removed[(size_t) i]) allowed[(size_t) i] = false;
            bool any = false; for (bool b : allowed) any |= b;

            int newTarget = (int) std::lround (midi);
            if (any)
            {
                const bool keep = target >= 0 && allowed[(size_t) (((target % 12) + 12) % 12)]
                                  && std::abs (midi - target) < 0.62; // hysteresis
                if (keep) newTarget = target;
                else
                {
                    double best = 1e9;
                    for (int c = (int) std::floor (midi) - 7; c <= (int) std::ceil (midi) + 7; ++c)
                        if (allowed[(size_t) (((c % 12) + 12) % 12)] && std::abs (c - midi) < best)
                            { best = std::abs (c - midi); newTarget = c; }
                }
            }

            if (newTarget != target) { target = newTarget; heldSec = 0.0; }
            else heldSec += hopSec;

            if (! glideInit) { glide = target; glideVel = 0.0; glideInit = true; }

            // Flex-Tune: leave notes alone unless the singer is near the target
            const double dev = std::abs (midi - target);
            double c = 1.0;
            if (params.flexTune > 0.0f)
            {
                const double thr = 0.5 * (1.0 - params.flexTune);
                const double x = std::clamp ((dev - thr) / 0.2, 0.0, 1.0);
                c = 1.0 - x * x * (3 - 2 * x);
            }
            corrWeight = c * (1.0 - 0.08 * params.softness);
            curMidi = midi;
            inputMidi.store ((float) midi);
            targetNote.store (target);
        }
        else
        {
            glideInit = false;
            corrWeight = 0.0;
            inputMidi.store (-1.0f);
            targetNote.store (-1);
        }
    }

    // ---------------- per-sample control smoothing --------------------------
    void updateControl()
    {
        const double dt = 1.0 / sr;

        // Note Transition (Waves-style): critically-damped glide between notes
        if (glideInit)
        {
            const double T = std::max (0.001, (params.transitionMs + 40.0 * params.softness) * 0.001);
            const double w0 = 2.0 / T;
            const double acc2 = w0 * w0 * (target - glide) - 2.0 * w0 * glideVel;
            glideVel += acc2 * dt;
            glide += glideVel * dt;
        }

        // desired correction in semitones
        double d = params.transpose;
        if (glideInit && corrWeight > 0.0) d += corrWeight * (glide - curMidi);

        // Retune Speed + Humanize (Antares-style): slower on sustained notes
        const double hf = smoothstep (0.08, 0.5, heldSec) * params.humanize;
        double tau1 = params.retuneMs * 0.001 * (1.0 + 3.0 * hf) + 0.12 * hf;
        if (! glideInit) tau1 = std::max (tau1, 0.03);           // release gently
        // Softness: even the fastest retune gets a velvet edge instead of a hard step
        tau1 = std::max (tau1, 0.0015 + 0.010 * params.softness);
        // ...and a short second stage rounds every corner (kept short so vibrato
        // correction stays in phase)
        const double tau2 = 0.001 + 0.006 * params.softness;

        s1 += (d - s1)  * (1.0 - std::exp (-dt / tau1));
        s2 += (s1 - s2) * (1.0 - std::exp (-dt / tau2));
        shiftSemis = std::clamp (s2, -24.0, 24.0);

        if ((n & 255) == 0)
            outputMidi.store (glideInit ? (float) (curMidi + shiftSemis) : -1.0f);
    }

    static double smoothstep (double a, double b, double x)
    {
        const double t = std::clamp ((x - a) / (b - a), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }

    TuneParams params;
    double sr = 44100.0, dsr = 22050.0;
    int maxPeriod = 0, latency = 0, size = 0, mask = 0, decim = 2, dSize = 0, dMask = 0, hop = 256;
    std::vector<float> input, acc, wsum, dbuf, yin;
    std::vector<Mark> marks;
    int markHead = 0, markCount = 0;
    long long n = 0, dn = 0, lastMark = 0;
    double ts = 0.0;
    float decimAcc = 0.0f; int decimCount = 0, hopCount = 0;

    int period = 294;
    bool voiced = false; int unvoicedHops = 0; double f0 = 0.0, curMidi = 0.0;
    int target = -1; double heldSec = 0.0;
    double glide = 0.0, glideVel = 0.0; bool glideInit = false;
    double alignTarget = 0.0, alignDelay = 0.0;
    double desired = 0.0, s1 = 0.0, s2 = 0.0, corrWeight = 0.0, shiftSemis = 0.0;
};

} // namespace aura
