// A tiny built-in synth so the arp can be heard without routing it to an instrument
// (handy in the standalone app). Two detuned saws + square through a one-pole low-pass.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace kal
{

class PreviewSynth
{
public:
    void prepare (double sr) { sampleRate = sr; for (auto& v : voices) v = {}; }

    void noteOn (int ch, int pitch, int vel)
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age > target->age) target = &v;
        }
        *target = {};
        target->active = true;
        target->channel = ch;
        target->pitch = pitch;
        target->gain = 0.12f * (float) vel / 127.0f;
        target->inc = 440.0 * std::pow (2.0, (pitch - 69) / 12.0) / sampleRate;
    }

    void noteOff (int ch, int pitch)
    {
        for (auto& v : voices)
            if (v.active && ! v.releasing && v.channel == ch && v.pitch == pitch) v.releasing = true;
    }

    void allOff() { for (auto& v : voices) v.releasing = true; }

    void render (float* left, float* right, int start, int num, float level)
    {
        const float attack = (float) (1.0 / (0.004 * sampleRate));
        const float release = (float) std::exp (-1.0 / (0.12 * sampleRate));
        const float decay = (float) std::exp (-1.0 / (0.9 * sampleRate));
        const float cutoffCoef = (float) (1.0 - std::exp (-2.0 * 3.14159265 * 2600.0 / sampleRate));
        for (auto& v : voices)
        {
            if (! v.active) continue;
            ++v.age;
            for (int i = start; i < start + num; ++i)
            {
                if (v.releasing) v.env *= release;
                else if (v.attackDone) v.env = 0.55f + (v.env - 0.55f) * decay;
                else { v.env += attack; if (v.env >= 1.0f) { v.env = 1.0f; v.attackDone = true; } }

                v.p1 += v.inc; if (v.p1 >= 1.0) v.p1 -= 1.0;
                v.p2 += v.inc * 1.006; if (v.p2 >= 1.0) v.p2 -= 1.0;
                const float saw = (float) (v.p1 * 2.0 - 1.0) + (float) (v.p2 * 2.0 - 1.0);
                const float sq = v.p1 < 0.5 ? 0.4f : -0.4f;
                v.lp += cutoffCoef * ((saw * 0.5f + sq) - v.lp);
                const float s = v.lp * v.env * v.gain * level;
                left[i] += s;
                right[i] += s;
                if (v.releasing && v.env < 1.0e-4f) { v.active = false; break; }
            }
        }
    }

private:
    struct Voice
    {
        bool active = false, releasing = false, attackDone = false;
        int channel = 0, pitch = 0;
        uint32_t age = 0;
        float env = 0.0f, gain = 0.0f, lp = 0.0f;
        double p1 = 0.0, p2 = 0.37, inc = 0.0;
    };
    std::array<Voice, 24> voices {};
    double sampleRate = 44100.0;
};

} // namespace kal
