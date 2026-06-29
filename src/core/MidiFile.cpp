#include "MidiFile.h"
// (WavSynth lives in its own translation unit; see WavSynth.cpp)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace rhythm {

namespace {

void putU32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back((v >> 24) & 0xff);
    b.push_back((v >> 16) & 0xff);
    b.push_back((v >> 8) & 0xff);
    b.push_back(v & 0xff);
}

void putU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back((v >> 8) & 0xff);
    b.push_back(v & 0xff);
}

// MIDI variable-length quantity.
void putVarLen(std::vector<uint8_t>& b, uint32_t value)
{
    uint8_t buffer[5];
    int count = 0;
    buffer[count++] = value & 0x7f;
    while ((value >>= 7) > 0)
        buffer[count++] = (value & 0x7f) | 0x80;
    for (int i = count - 1; i >= 0; --i)
        b.push_back(buffer[i]);
}

struct RawEvent {
    long tick;
    uint8_t status, data1, data2;
    int order; // tie-break: note-offs (0) before note-ons (1) at same tick
};

} // namespace

bool writeMidiFile(const std::string& path, const Sequence& sequence,
                   double bpm, int ticksPerQuarter)
{
    if (ticksPerQuarter <= 0) ticksPerQuarter = 480;

    std::vector<RawEvent> events;
    events.reserve(sequence.size() * 2);

    for (const auto& e : sequence) {
        int ch = std::clamp(e.channel - 1, 0, 15);
        int vel = std::clamp((int)std::lround(e.velocity * 127.0f), 1, 127);
        int note = std::clamp(e.note, 0, 127);
        long onTick = std::lround(e.startBeats * ticksPerQuarter);
        long offTick = std::lround(e.endBeats() * ticksPerQuarter);
        if (offTick <= onTick) offTick = onTick + 1;
        events.push_back({ onTick, (uint8_t)(0x90 | ch),
                           (uint8_t)note, (uint8_t)vel, 1 });
        events.push_back({ offTick, (uint8_t)(0x80 | ch),
                           (uint8_t)note, 0, 0 });
    }

    std::sort(events.begin(), events.end(),
              [](const RawEvent& a, const RawEvent& b) {
                  if (a.tick != b.tick) return a.tick < b.tick;
                  return a.order < b.order;
              });

    // --- Build the track chunk --------------------------------------------
    std::vector<uint8_t> track;

    // Tempo meta event (FF 51 03 tttttt).
    uint32_t usPerQuarter = (uint32_t)std::lround(60000000.0 / bpm);
    putVarLen(track, 0);
    track.push_back(0xff); track.push_back(0x51); track.push_back(0x03);
    track.push_back((usPerQuarter >> 16) & 0xff);
    track.push_back((usPerQuarter >> 8) & 0xff);
    track.push_back(usPerQuarter & 0xff);

    long prevTick = 0;
    for (const auto& ev : events) {
        putVarLen(track, (uint32_t)std::max<long>(0, ev.tick - prevTick));
        track.push_back(ev.status);
        track.push_back(ev.data1);
        track.push_back(ev.data2);
        prevTick = ev.tick;
    }

    // End of track.
    putVarLen(track, 0);
    track.push_back(0xff); track.push_back(0x2f); track.push_back(0x00);

    // --- Assemble the file ------------------------------------------------
    std::vector<uint8_t> out;
    // Header chunk.
    out.push_back('M'); out.push_back('T'); out.push_back('h'); out.push_back('d');
    putU32(out, 6);
    putU16(out, 0); // format 0
    putU16(out, 1); // one track
    putU16(out, (uint16_t)ticksPerQuarter);
    // Track chunk.
    out.push_back('M'); out.push_back('T'); out.push_back('r'); out.push_back('k');
    putU32(out, (uint32_t)track.size());
    out.insert(out.end(), track.begin(), track.end());

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
    return f.good();
}

} // namespace rhythm
