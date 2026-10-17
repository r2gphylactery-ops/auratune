// Offline test for TuneEngine: synthesises an out-of-tune "voice",
// runs it through the engine and measures the output pitch.
#define _USE_MATH_DEFINES
#include "../Source/TuneEngine.h"
#include <cstdio>
#include <cstdint>
#include <fstream>

static double measurePitch (const std::vector<float>& x, size_t from, size_t len, double sr)
{
    // plain normalised autocorrelation over a long window
    const int t0 = (int) (sr / 1000), t1 = (int) (sr / 60);
    std::vector<double> rv ((size_t) t1 + 2, 0.0);
    double mx = 0;
    for (int tau = t0; tau <= t1 + 1; ++tau)
    {
        double num = 0, e1 = 0, e2 = 0;
        for (size_t i = from; i < from + len; ++i) { num += x[i] * x[i + tau]; e1 += x[i] * x[i]; e2 += x[i + tau] * x[i + tau]; }
        rv[(size_t) tau] = num / std::sqrt (e1 * e2 + 1e-12);
        mx = std::max (mx, rv[(size_t) tau]);
    }
    int best = t0;
    for (int tau = t0 + 1; tau <= t1; ++tau)   // first strong local max = fundamental
        if (rv[(size_t) tau] > 0.9 * mx && rv[(size_t) tau] >= rv[(size_t) tau - 1] && rv[(size_t) tau] >= rv[(size_t) tau + 1]) { best = tau; break; }
    // parabolic refinement
    auto r = [&] (int tau) { double num = 0, e1 = 0, e2 = 0;
        for (size_t i = from; i < from + len; ++i) { num += x[i] * x[i + tau]; e1 += x[i] * x[i]; e2 += x[i + tau] * x[i + tau]; }
        return num / std::sqrt (e1 * e2 + 1e-12); };
    const double a = r (best - 1), b = r (best), c = r (best + 1);
    const double t = best + 0.5 * (a - c) / (a - 2 * b + c);
    return sr / t;
}

static std::vector<float> voice (double sr, double seconds, double hz, double vibCents = 0)
{
    std::vector<float> out ((size_t) (sr * seconds));
    double ph = 0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const double f = hz * std::pow (2.0, vibCents / 1200.0 * std::sin (2 * M_PI * 5.5 * i / sr));
        ph += f / sr;
        double s = 0;
        for (int h = 1; h <= 20; ++h)
        {
            const double fh = h * f;
            const double formant = std::exp (-std::pow ((fh - 700) / 400, 2)) + 0.5 * std::exp (-std::pow ((fh - 1200) / 500, 2)) + 0.1;
            s += formant * std::sin (2 * M_PI * h * ph) / h;
        }
        out[i] = (float) (0.3 * s);
    }
    return out;
}

static double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

static std::vector<float> run (aura::TuneEngine& e, const std::vector<float>& in)
{
    std::vector<float> out (in.size());
    float w, d;
    for (size_t i = 0; i < in.size(); ++i) { e.processSample (in[i], w, d); out[i] = w; }
    return out;
}

static void writeWav (const char* path, const std::vector<float>& x, int sr)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + (uint32_t) x.size() * 2); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2); u16 (2); u16 (16);
    f.write ("data", 4); u32 ((uint32_t) x.size() * 2);
    for (float v : x) { int16_t s = (int16_t) std::clamp (v * 32767.0f, -32768.0f, 32767.0f); f.write ((char*) &s, 2); }
}

int main()
{
    int fails = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        struct Case { double inHz, expectHz; int scale; int key; const char* name; };
        const Case cases[] = {
            { 446.0, 440.0,       0, 0, "A4 +24c -> A4 (chromatic)" },
            { 130.0, 130.8128,    0, 0, "C3 -11c -> C3" },
            { 285.0, 293.6648,    1, 0, "C#4+ in C major -> D4" },
            { 660.0, 659.2551,    1, 0, "E5 ~ -> E5" },
            { 100.0, 97.9989,     0, 0, "G2 +35c -> G2" },
        };
        for (const auto& c : cases)
        {
            aura::TuneEngine e; e.prepare (sr);
            aura::TuneParams p; p.retuneMs = 0; p.softness = 0.3f; p.scale = c.scale; p.key = c.key;
            p.inputType = 3;
            e.setParams (p);
            auto in = voice (sr, 1.5, c.inHz);
            auto out = run (e, in);
            const double got = measurePitch (out, (size_t) (sr * 0.8), (size_t) (sr * 0.2), sr);
            const double err = cents (got, c.expectHz);
            const bool ok = std::abs (err) < 5.0;
            fails += ! ok;
            std::printf ("[%s] sr=%6.0f %-30s in %.1f Hz -> out %.2f Hz (err %+.1f c)\n", ok ? "OK " : "BAD", sr, c.name, c.inHz, got, err);
        }
        // pass-through quality: transpose 0, correction = 0 cents off → output ≈ input
        {
            aura::TuneEngine e; e.prepare (sr);
            aura::TuneParams p; p.inputType = 3; e.setParams (p);
            // unvoiced material (breaths, "s", "sh") must pass through untouched
            std::vector<float> in ((size_t) sr);
            uint32_t seed = 1; for (auto& v : in) { seed = seed * 1664525u + 1013904223u; v = 0.2f * ((seed >> 8) / 8388608.0f - 1.0f); }
            auto out = run (e, in);
            const int L = e.getLatencySamples();
            double num = 0, den = 0;
            for (size_t i = (size_t) (sr * 0.5); i < in.size(); ++i) { const double d = out[i] - in[i - (size_t) L]; num += d * d; den += in[i - (size_t) L] * in[i - (size_t) L]; }
            const double snr = 10 * std::log10 (den / (num + 1e-12));
            std::printf ("[%s] sr=%6.0f unvoiced passthrough SNR %.1f dB (latency %d smp = %.1f ms)\n", snr > 20 ? "OK " : "BAD", sr, snr, L, 1000.0 * L / sr);
            fails += snr <= 20;
        }
    }

    // audible demo: out-of-tune vibrato voice, soft settings
    {
        const double sr = 44100;
        aura::TuneEngine e; e.prepare (sr);
        aura::TuneParams p; p.retuneMs = 25; p.humanize = 0.3f; p.softness = 0.7f; p.transitionMs = 80; p.scale = 1;
        e.setParams (p);
        std::vector<float> in;
        for (double hz : { 255.0, 300.0, 335.0, 385.0 }) { auto v = voice (sr, 0.8, hz, 30); in.insert (in.end(), v.begin(), v.end()); }
        auto out = run (e, in);
        writeWav ("demo_in.wav", in, 44100); writeWav ("demo_out.wav", out, 44100);
        float peak = 0; for (float v : out) peak = std::max (peak, std::abs (v));
        std::printf ("demo peak %.3f (no blow-ups: %s)\n", peak, peak < 1.5f ? "yes" : "NO");
        fails += peak >= 1.5f;
    }
    std::printf (fails ? "\n%d FAILURES\n" : "\nALL PASSED\n", fails);
    return fails;
}
