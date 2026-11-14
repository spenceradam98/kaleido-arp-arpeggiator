// Preset text format and the factory library.
#pragma once

#include "ArpTypes.h"

#include <string>
#include <vector>

namespace kal
{

// Plain-text, line-based "key=value" format. Unknown keys are ignored and missing keys keep
// their defaults, so presets stay loadable as the engine grows.
std::string toText (const Preset& p);
bool fromText (const std::string& text, Preset& out);

// Sets a rhythm lane from a string: 'x' on, '.' rest, '-' tie. Length = string length.
void setRhythm (Pattern& p, const std::string& s);
void setLane (Pattern& p, Lane lane, const std::vector<int>& values);   // length = values.size()

const std::vector<Preset>& factoryPresets();

} // namespace kal
