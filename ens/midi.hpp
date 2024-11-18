/**
 * MIDI Parser, for use only with this project as it is made for learning only.
 *
 * Copyright (c) 2024 porter@nrz.se
 */
#ifndef ENS_MIDI_HPP
#define ENS_MIDI_HPP

#include <vector>
#include <string>

namespace ens {
/**
 * Resources:
 *   Standard MIDI-File Format Spec. 1.1
 *      https://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html#BMA1_
 *   MIDI Files Specification
 *      http://www.somascape.org/midi/tech/mfile.html
 *   Outline of the Standard MIDI File Structure
 *      https://www.ccarh.org/courses/253/handout/smf/
 *   Variable-length quantity
 *      https://en.wikipedia.org/wiki/Variable-length_quantity
 *   Programming MIDI by javdix9
 *      https://youtu.be/040BKtnDdg0?si=AdAnEDt5iF9dta0T
 *
 * MIDI have 128 notes in the MIDI standard (0-127).
 *
 * f_n = f_0 \cdot 2^{\frac{n_m - 69}{12}}
 *
 * Source: https://www.translatorscafe.com/unit-converter/en-US/calculator/note-frequency/
 */

class istrm {
public:
    istrm(std::string const& path);

    auto consume(std::size_t size = 1) -> std::size_t;
    auto peek_consume() -> std::uint8_t;
    auto peek_consume(std::uint8_t* dst, std::size_t size) -> void;
    auto peek() const -> std::uint8_t;
    auto peek(std::uint8_t* dst, std::size_t size) const -> std::size_t;
    auto has_next(std::size_t offset = 1) const -> bool;
    auto set_max_consume(std::size_t offset) -> void;
    auto reset_max_consume() -> void;

    auto read_string(std::size_t size) -> std::string;
    auto read_vlq() -> std::uint32_t;

private:
    std::vector<std::uint8_t> m_data;
    std::size_t m_cursor;
    std::size_t m_end;
};

enum class midi_event_type : std::uint8_t {
    note_off         = 0x80,
    note_on          = 0x90,
    after_touch      = 0xA0,
    control_change   = 0xB0,
    program_change   = 0xC0,
    channel_pressure = 0xD0,
    pitch_bend       = 0xE0,
    system_exclusive = 0xF0,
};

constexpr auto operator==(std::uint8_t a, midi_event_type b) -> bool {
    using T = std::underlying_type_t<midi_event_type>;
    return static_cast<T>(b) == a;
}

enum class midi_meta_type : std::uint8_t {
    none            = 0x00,  // Not meta event
    sequence_num    = 0x00,  // Sequence Number
    text            = 0x01,  // Text Event
    copyright       = 0x02,  // Copyright Notice
    track_name      = 0x03,  // Sequence/Track Name
    instrument_name = 0x04,  // Instrument Name
    lyric           = 0x05,  // Lyric
    marker          = 0x06,  // Marker
    cue_point       = 0x07,  // Cue Point
    channel_prefix  = 0x20,  // MIDI Channel Prefix
    track_end       = 0x2F,  // End of Track
    tempo           = 0x51,  // Set Tempo (in microseconds per MIDI quarter-note)
    smpte_offset    = 0x54,  // SMPTE Offset
    time_signature  = 0x58,  // Time Signature
    key_signature   = 0x59,  // Key Signature
    seq_specific    = 0x7F,  // Sequencer Specific Meta-Event
};

struct midi_event {
    midi_event_type type;
    std::uint32_t dt;
    std::size_t index;
};

struct midi_note {
   std::uint8_t channel;
   std::uint8_t key;
   std::uint8_t velocity;
};

struct mtrk {
    char const* type = "MTrk";
    std::uint32_t ticks{0};
    std::uint32_t tempo{};
    std::vector<midi_event> events{};
    std::vector<midi_note> notes{};

    auto str() const -> std::string;

    static auto read(ens::istrm& istrm) -> mtrk;

private:
    auto parse(ens::istrm& istrm) -> void;
private:
    std::uint32_t m_byte_size{};
};

struct mthd {
    char const* type = "MThd";
    std::uint32_t length;
    std::uint16_t format;
    std::uint16_t tracks;
    std::uint16_t division;

    auto str() const -> std::string;
    static auto read(ens::istrm& istrm) -> mthd;
};

class midi {
public:
    auto open(std::string const& midi_file) -> void;
    auto header() const -> mthd const& { return m_header; }
    auto tracks() const -> std::vector<mtrk> const& { return m_tracks; }

private:
    mthd m_header{};
    std::vector<mtrk> m_tracks{};
};
}

#endif
