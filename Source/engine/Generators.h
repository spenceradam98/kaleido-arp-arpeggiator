// Pattern generators and tools. All are deterministic for a given seed.
#pragma once

#include "ArpTypes.h"

#include <cstdint>

namespace kal::gen
{

// --- Generators -------------------------------------------------------------------------------
// Euclidean rhythm: `hits` onsets spread evenly over `steps`, rotated right by `rotation`.
void euclid (Pattern& p, int hits, int steps, int rotation);
// Random rhythm: each step on with probability `density` (0..1), some ons become ties.
void randomRhythm (Pattern& p, float density, float tieChance, uint32_t seed);
// Random values for any lane, spread around the lane default by `amount` (0..1).
void randomLane (Pattern& p, Lane lane, float amount, uint32_t seed);
// Markov melody: walks the semitone lane in scale degrees (use with degree mode) or semitones.
// stay/step/leap are relative weights of the next interval being 0, +-1..2 and +-3..7.
void markovMelody (Pattern& p, float stay, float step, float leap, uint32_t seed);
// Markov rhythm: two-state chain. pOnAfterOn / pOnAfterOff are the transition probabilities.
void markovRhythm (Pattern& p, float pOnAfterOn, float pOnAfterOff, uint32_t seed);
// Accent patterns for the velocity lane: every `period` steps gets `accent`, the rest `base`.
void accents (Pattern& p, int period, int offset, int base, int accent);
// Random everything, with musical limits.
void surprise (Pattern& p, uint32_t seed);
// Small random changes across all lanes; amount 0..1.
void mutate (Pattern& p, float amount, uint32_t seed);

// --- Tools ------------------------------------------------------------------------------------
void rotate (Pattern& p, Lane lane, int by);           // positive = to the right
void reverse (Pattern& p, Lane lane);
void invert (Pattern& p, Lane lane);                   // mirror within the lane's value range
void ramp (Pattern& p, Lane lane, bool up);            // linear ramp across the lane length
void humanise (Pattern& p, Lane lane, float amount, uint32_t seed);
void setLength (Pattern& p, Lane lane, int length);
void setAllLengths (Pattern& p, int length);
void clear (Pattern& p, Lane lane);
void doubleLength (Pattern& p, Lane lane);             // copies the lane after itself (max 32)

} // namespace kal::gen
