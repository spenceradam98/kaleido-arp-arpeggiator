#include "Generators.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace kal::gen
{

namespace
{
    int clampLane (Lane l, int v) { return std::clamp (v, laneSpec (l).minValue, laneSpec (l).maxValue); }
    int len (const Pattern& p, Lane l) { return std::clamp (p.lane (l).length, 1, kMaxSteps); }
}

void euclid (Pattern& p, int hits, int steps, int rotation)
{
    steps = std::clamp (steps, 1, kMaxSteps);
    hits = std::clamp (hits, 0, steps);
    auto& r = p.lane (Lane::Rhythm);
    r.length = steps;
    for (int i = 0; i < steps; ++i)
    {
        // Bresenham form of the Bjorklund distribution; step 0 is always a hit when hits > 0.
        const bool on = hits > 0 && ((i * hits) % steps) < hits;
        const int dst = (((i + rotation) % steps) + steps) % steps;
        r.values[(size_t) dst] = on ? kOn : kRest;
    }
}

void randomRhythm (Pattern& p, float density, float tieChance, uint32_t seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    auto& r = p.lane (Lane::Rhythm);
    for (int i = 0; i < len (p, Lane::Rhythm); ++i)
    {
        int v = u (rng) < density ? kOn : kRest;
        if (v == kOn && i > 0 && r.values[(size_t) i - 1] != kRest && u (rng) < tieChance) v = kTie;
        r.values[(size_t) i] = v;
    }
    r.values[0] = kOn;   // always start on the one
}

void randomLane (Pattern& p, Lane lane, float amount, uint32_t seed)
{
    if (lane == Lane::Rhythm) { randomRhythm (p, 0.35f + 0.6f * amount, 0.15f * amount, seed); return; }
    std::mt19937 rng (seed);
    const auto& spec = laneSpec (lane);
    auto& ld = p.lane (lane);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (int i = 0; i < len (p, lane); ++i)
    {
        int v = spec.defaultValue;
        switch (lane)
        {
            case Lane::Velocity:    v = 96 + (int) std::lround (u (rng) * 31.0f * amount * 1.4f); break;
            case Lane::Gate:        v = 60 + (int) std::lround (u (rng) * 55.0f * amount); break;
            case Lane::Octave:      v = std::abs (u (rng)) < 0.25f + 0.5f * amount ? (int) std::lround (u (rng) * (1.0f + amount)) : 0; break;
            case Lane::Semitone:    v = (int) std::lround (u (rng) * 7.0f * amount); break;
            case Lane::Probability: v = 100 - (int) std::lround (std::abs (u (rng)) * 70.0f * amount); break;
            case Lane::Ratchet:     v = std::abs (u (rng)) < 0.2f * amount + 0.05f ? 2 + (int) (std::abs (u (rng)) * 2.5f * amount) : 1; break;
            default: break;
        }
        ld.values[(size_t) i] = clampLane (lane, v);
    }
}

void markovMelody (Pattern& p, float stay, float step, float leap, uint32_t seed)
{
    std::mt19937 rng (seed);
    const float total = std::max (0.0001f, stay + step + leap);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    auto& ld = p.lane (Lane::Semitone);
    int cur = 0;
    for (int i = 0; i < len (p, Lane::Semitone); ++i)
    {
        if (i > 0)
        {
            const float r = u (rng) * total;
            int delta = 0;
            if (r < stay) delta = 0;
            else if (r < stay + step) delta = 1 + (int) (u (rng) * 2.0f);
            else delta = 3 + (int) (u (rng) * 5.0f);
            if (u (rng) < 0.5f) delta = -delta;
            // Pull back towards the centre so long walks stay usable.
            if (std::abs (cur + delta) > 9) delta = -delta;
            cur = std::clamp (cur + delta, -12, 12);
        }
        ld.values[(size_t) i] = cur;
    }
}

void markovRhythm (Pattern& p, float pOnAfterOn, float pOnAfterOff, uint32_t seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    auto& r = p.lane (Lane::Rhythm);
    bool on = true;
    r.values[0] = kOn;
    for (int i = 1; i < len (p, Lane::Rhythm); ++i)
    {
        on = u (rng) < (on ? pOnAfterOn : pOnAfterOff);
        r.values[(size_t) i] = on ? kOn : kRest;
    }
}

void accents (Pattern& p, int period, int offset, int base, int accent)
{
    period = std::max (1, period);
    auto& ld = p.lane (Lane::Velocity);
    for (int i = 0; i < kMaxSteps; ++i)
        ld.values[(size_t) i] = clampLane (Lane::Velocity, (((i - offset) % period + period) % period) == 0 ? accent : base);
}

void surprise (Pattern& p, uint32_t seed)
{
    std::mt19937 rng (seed);
    const int lengths[] = { 8, 12, 16, 16, 16, 5, 7, 6, 9, 32 };
    auto pick = [&] { return lengths[rng() % 10]; };
    for (int i = 0; i < kNumLanes; ++i) p.lane ((Lane) i).length = 16;
    p.lane (Lane::Rhythm).length = pick() > 10 ? 16 : 8 + (int) (rng() % 9);
    randomRhythm (p, 0.7f, 0.1f, (uint32_t) rng());
    p.lane (Lane::Velocity).length = pick();
    randomLane (p, Lane::Velocity, 0.6f, (uint32_t) rng());
    p.lane (Lane::Gate).length = pick();
    randomLane (p, Lane::Gate, 0.5f, (uint32_t) rng());
    p.lane (Lane::Octave).length = pick();
    randomLane (p, Lane::Octave, 0.3f, (uint32_t) rng());
    p.lane (Lane::Semitone).length = pick();
    if (rng() & 1u) markovMelody (p, 1.0f, 2.0f, 0.6f, (uint32_t) rng()); else p.resetLane (Lane::Semitone);
    p.lane (Lane::Probability).length = pick();
    randomLane (p, Lane::Probability, 0.3f, (uint32_t) rng());
    p.lane (Lane::Ratchet).length = pick();
    randomLane (p, Lane::Ratchet, 0.4f, (uint32_t) rng());
}

void mutate (Pattern& p, float amount, uint32_t seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    for (int li = 0; li < kNumLanes; ++li)
    {
        const auto lane = (Lane) li;
        const auto& spec = laneSpec (lane);
        auto& ld = p.lane (lane);
        const int range = spec.maxValue - spec.minValue;
        for (int i = 0; i < len (p, lane); ++i)
        {
            if (u (rng) >= amount * 0.35f) continue;
            int& v = ld.values[(size_t) i];
            if (lane == Lane::Rhythm) v = v == kRest ? kOn : (u (rng) < 0.8f ? kRest : kTie);
            else
            {
                const int delta = std::max (1, (int) std::lround (range * 0.15f * amount));
                v = clampLane (lane, v + (int) std::lround ((u (rng) * 2.0f - 1.0f) * delta));
            }
        }
    }
}

void rotate (Pattern& p, Lane lane, int by)
{
    auto& ld = p.lane (lane);
    const int n = len (p, lane);
    by = ((by % n) + n) % n;
    std::rotate (ld.values.begin(), ld.values.begin() + (n - by), ld.values.begin() + n);
}

void reverse (Pattern& p, Lane lane)
{
    auto& ld = p.lane (lane);
    std::reverse (ld.values.begin(), ld.values.begin() + len (p, lane));
}

void invert (Pattern& p, Lane lane)
{
    const auto& spec = laneSpec (lane);
    auto& ld = p.lane (lane);
    for (int i = 0; i < len (p, lane); ++i)
    {
        int& v = ld.values[(size_t) i];
        if (lane == Lane::Rhythm) v = v == kRest ? kOn : kRest;
        else if (lane == Lane::Octave || lane == Lane::Semitone) v = -v;
        else v = spec.minValue + spec.maxValue - v;
    }
}

void ramp (Pattern& p, Lane lane, bool up)
{
    const auto& spec = laneSpec (lane);
    auto& ld = p.lane (lane);
    const int n = len (p, lane);
    int lo = spec.minValue, hi = spec.maxValue;
    if (lane == Lane::Velocity) lo = 30;
    if (lane == Lane::Gate) { lo = 10; hi = 100; }
    if (lane == Lane::Rhythm) return;
    for (int i = 0; i < n; ++i)
    {
        const float t = n > 1 ? (float) i / (float) (n - 1) : 1.0f;
        ld.values[(size_t) i] = (int) std::lround (lo + (hi - lo) * (up ? t : 1.0f - t));
    }
}

void humanise (Pattern& p, Lane lane, float amount, uint32_t seed)
{
    if (lane == Lane::Rhythm || lane == Lane::Octave) return;
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    const auto& spec = laneSpec (lane);
    auto& ld = p.lane (lane);
    const float span = (spec.maxValue - spec.minValue) * 0.08f * amount;
    for (int i = 0; i < len (p, lane); ++i)
        ld.values[(size_t) i] = clampLane (lane, ld.values[(size_t) i] + (int) std::lround (u (rng) * span));
}

void setLength (Pattern& p, Lane lane, int length) { p.lane (lane).length = std::clamp (length, 1, kMaxSteps); }

void setAllLengths (Pattern& p, int length)
{
    for (int i = 0; i < kNumLanes; ++i) setLength (p, (Lane) i, length);
}

void clear (Pattern& p, Lane lane)
{
    const int n = p.lane (lane).length;
    p.resetLane (lane);
    p.lane (lane).length = n;
}

void doubleLength (Pattern& p, Lane lane)
{
    auto& ld = p.lane (lane);
    const int n = len (p, lane);
    const int m = std::min (kMaxSteps, n * 2);
    for (int i = n; i < m; ++i) ld.values[(size_t) i] = ld.values[(size_t) (i - n)];
    ld.length = m;
}

} // namespace kal::gen
