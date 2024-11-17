/**
 * MIDI Parser, for use only with this project as it is made for learning only.
 *
 * Copyright (c) 2024 porter@nrz.se
 */
#include "midi.hpp"

#include <fstream>

#include "fmt/format.h"
#include "utils.hpp"

namespace ens {
istrm::istrm(std::string const& path) : m_data(), m_cursor(0), m_end(0) {
    std::ifstream fstrm(path, std::ios::binary);
    if (!fstrm.is_open())
        throw std::runtime_error(fmt::format("Error openning file: {}", path));
    fstrm.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!

    fstrm.seekg(0, std::ios::end);
    auto const file_size = static_cast<std::size_t>(fstrm.tellg());
    fstrm.seekg(0, std::ios::beg);
    m_data.reserve(file_size);

    // TODO: Maybe don't copy everything, but copy everytime we do a peek
    std::copy(std::istream_iterator<char>(fstrm), std::istream_iterator<char>(),
              std::back_inserter(m_data));

    m_end = m_data.size();
    fstrm.close();
}

auto istrm::consume(std::size_t size) -> std::size_t {
    if (m_cursor >= m_data.size()) {
        m_cursor = m_data.size();
        return 0;
    }
    m_cursor += size;
    return size;
}

auto istrm::peek_consume() -> std::uint8_t {
    return m_data[m_cursor++];
}

auto istrm::peek_consume(std::uint8_t* dst, std::size_t size) -> void {
    if (m_cursor + size > m_data.size()) return;
    for (std::size_t i = 0; i < size; ++i) {
        dst[i] = m_data[i + m_cursor];
    }
    m_cursor += size;
}

auto istrm::peek() const -> std::uint8_t {
    return m_data[m_cursor];
}

auto istrm::peek(std::uint8_t* dst, std::size_t size) const -> std::size_t {
    if (m_cursor + size > m_data.size()) return 0;
    for (std::size_t i = 0; i < size; ++i) {
        dst[i] = m_data[i + m_cursor];
    }
    return size;
}

auto istrm::has_next(std::size_t offset) const -> bool {
    return m_cursor + offset < m_data.size() && m_cursor + offset < m_end;
}

auto istrm::set_max_consume(std::size_t offset) -> void {
    if (m_cursor + offset >= m_end) return;
    m_end = m_cursor + offset;
}

auto istrm::reset_max_consume() -> void {
    m_end = m_data.size();
}

auto istrm::read_string(std::size_t size) -> std::string {
    std::string str{};
    str.resize(size, 0x00);
    peek_consume(reinterpret_cast<std::uint8_t*>(str.data()), size);
    return str;
}

auto istrm::read_vlq() -> std::uint32_t {
    if (!(peek() & 0x80)) return std::uint32_t(peek_consume());
    std::uint32_t value = peek_consume() & 0x7F;
    do {
        value = (value << 7) + (peek() & 0x7F);
    } while (peek_consume() & 0x80);
    return value;
}

auto mtrk::str() const -> std::string {
    auto const bpm = tempo != 0 ? 60'000'000/tempo : 0;
    return fmt::format("{} {{ tempo: {} us, BPM: {}, events: {}, length: {} }}",
                       type, tempo, bpm, events.size(), ticks);
}

auto mtrk::read(ens::istrm& istrm) -> mtrk {
    mtrk chunk{};
    char type_buffer[5]{0};
    istrm.peek(reinterpret_cast<std::uint8_t*>(type_buffer), sizeof type_buffer - 1);
    if (std::string_view{type_buffer} != std::string_view{chunk.type})
        throw std::runtime_error(fmt::format("Not a MIDI track: {}", std::string_view{type_buffer}));
    istrm.consume(sizeof type_buffer - 1);

    istrm.peek_consume(reinterpret_cast<std::uint8_t*>(&chunk.m_byte_size), sizeof chunk.m_byte_size);
    chunk.m_byte_size = std::byteswap(chunk.m_byte_size);
    chunk.parse(istrm);
    return chunk;
}

auto mtrk::parse(ens::istrm& istrm) -> void {
    std::uint32_t dt = 0;
    std::uint8_t prev_status = 0x00;
    istrm.set_max_consume(m_byte_size);
    std::uint32_t time = 0;

    while (istrm.has_next()) {
        dt = istrm.read_vlq();
        time += dt;
        auto status = istrm.peek();
        if (status < 0x80) status = prev_status;

        if ((status & 0xF0) == midi_event_type::program_change) {
            prev_status = status;

            std::uint8_t channel = istrm.peek_consume() & 0x0F;
            std::uint8_t program = istrm.peek_consume() & 0x7F;
            fmt::print("Program Change CH: {}, PR: {}\n", channel, program);

            events.push_back({midi_event_type::program_change, dt, lim<std::size_t>::max()});
        } else if ((status & 0xF0) == midi_event_type::note_on) {
            prev_status = status;

            std::uint8_t channel = istrm.peek_consume() & 0x0F;
            std::uint8_t key = istrm.peek_consume() & 0x7F;
            std::uint8_t vel = istrm.peek_consume() & 0x7F;
            fmt::print("{} Note On CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);

            events.push_back({midi_event_type::note_on, dt, notes.size()});
            notes.push_back({channel, key, vel});
        } else if ((status & 0xF0) == midi_event_type::note_off) {
            prev_status = status;

            std::uint8_t channel = istrm.peek_consume() & 0x0F;
            std::uint8_t key = istrm.peek_consume() & 0x7F;
            std::uint8_t vel = istrm.peek_consume() & 0x7F;
            fmt::print("{} Note Off CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);

            events.push_back({midi_event_type::note_off, dt, notes.size()});
            notes.push_back({channel, key, vel});
        } else if ((status & 0xF0) == midi_event_type::control_change) {
            prev_status = status;

            std::uint8_t channel = istrm.peek_consume() & 0x0F;
            std::uint8_t c = istrm.peek_consume() & 0x7F;
            std::uint8_t v = istrm.peek_consume() & 0x7F;
            fmt::print("{} Channel Mode Messages CH: {}, c: {}, v: {}\n", dt, channel, c, v);

            events.push_back({midi_event_type::control_change, dt, lim<std::size_t>::max()});
        } else if ((status & 0xF0) == midi_event_type::pitch_bend) {
            prev_status = status;

            std::uint8_t channel = istrm.peek_consume() & 0x0F;
            std::uint8_t l = istrm.peek_consume() & 0x7F;
            std::uint8_t m = istrm.peek_consume() & 0x7F;
            fmt::print("{} Pitch Wheel Change CH: {}, l: {}, m: {}\n", dt, channel, l, m);

            events.push_back({midi_event_type::pitch_bend, dt, 0});
        } else if ((status & 0xF0) == midi_event_type::system_exclusive) {
            prev_status = 0x00;
            events.push_back({midi_event_type::system_exclusive, dt, lim<std::size_t>::max()});

            if (istrm.peek() == 0xFF) {
                istrm.consume();
                auto const meta_value = static_cast<midi_meta_type>(istrm.peek_consume());
                auto const len  = istrm.read_vlq();

                switch (meta_value) {
                    case midi_meta_type::sequence_num: {
                        fmt::print("Sequence Number\n");
                        break;
                    }
                    case midi_meta_type::text: {
                        fmt::print("Text Event\n");
                        break;
                    }
                    case midi_meta_type::copyright: {
                        fmt::print("Copyright Notice\n");
                        break;
                    }
                    case midi_meta_type::track_name: {
                        fmt::print("Tack Name: {}\n", istrm.read_string(len));
                        break;
                    }
                    case midi_meta_type::instrument_name: {
                        fmt::print("Instrument Name\n");
                        break;
                    }
                    case midi_meta_type::lyric: {
                        fmt::print("Lyric\n");
                        break;
                    }
                    case midi_meta_type::marker: {
                        fmt::print("Marker\n");
                        break;
                    }
                    case midi_meta_type::cue_point: {
                        fmt::print("Cue Point\n");
                        break;
                    }
                    case midi_meta_type::channel_prefix: {
                        fmt::print("MIDI Channel Prefix\n");
                        break;
                    }
                    case midi_meta_type::track_end: {
                        fmt::print("End of Track\n");
                        break;
                    }
                    case midi_meta_type::tempo: {
                        std::uint8_t tempo_data[3]{0};
                        istrm.peek_consume(tempo_data, sizeof tempo_data);
                        tempo = (std::uint32_t(tempo_data[2]) << 0)
                              | (std::uint32_t(tempo_data[1]) << 8)
                              | (std::uint32_t(tempo_data[0]) << 16);
                        fmt::print("Set Tempo {} us ({} BPM)\n", tempo, 60'000'000/tempo);
                        break;
                    }
                    case midi_meta_type::smpte_offset: {
                        fmt::print("SMPTE Offset\n");
                        break;
                    }
                    case midi_meta_type::time_signature: {
                        auto const numerator    = istrm.peek_consume();
                        auto const denominator  = istrm.peek_consume();
                        auto const clicks       = istrm.peek_consume();
                        auto const quarter_note = istrm.peek_consume();
                        fmt::print("Time Signature {}/{}, {} MIDI clocks, {}\n", numerator, denominator, clicks, quarter_note);
                        break;
                    }
                    case midi_meta_type::key_signature: {
                        fmt::print("Key Signature\n");
                        break;
                    }
                    case midi_meta_type::seq_specific: {
                        fmt::print("Sequencer Specific Meta-Event: {}\n", istrm.read_string(len));
                        break;
                    }
                    default:{
                        fmt::print("Unrecognised Meta Event: {:#04x}", static_cast<std::uint8_t>(meta_value));
                        break;
                    }
                }
            } else if (istrm.peek() == 0xF0) {
                istrm.consume();
                // System Exclusive Begin
                fmt::print("System Exclusive Begin: {}\n", istrm.read_string(istrm.read_vlq()));
            } else if (istrm.peek() == 0xF7) {
                istrm.consume();
                // System Exclusive End
                fmt::print("System Exclusive End: {}\n", istrm.read_string(istrm.read_vlq()));
            }
        } else {
            fmt::print("Not Implemented: {:#04x}\n", istrm.peek_consume());
        }
    }
    istrm.reset_max_consume();
    ticks = time;
}

auto mthd::str() const -> std::string {
    return fmt::format("{} {{ length: {}, format: {}, tracks: {}, division: {} }}",
                       type,  length, format, tracks, division);
}

auto mthd::read(istrm& istrm) -> mthd {
    mthd chunk{};
    char type_buffer[5]{0};
    istrm.peek(reinterpret_cast<std::uint8_t*>(type_buffer), sizeof type_buffer - 1);
    if (std::string_view{type_buffer} != std::string_view{chunk.type})
        throw std::runtime_error(fmt::format("Not a MIDI file: type {}", std::string_view{type_buffer}));
    istrm.consume(sizeof type_buffer - 1);

    istrm.peek_consume(reinterpret_cast<std::uint8_t*>(&chunk.length), sizeof chunk.length);
    chunk.length = std::byteswap(chunk.length);

    istrm.peek_consume(reinterpret_cast<std::uint8_t*>(&chunk.format), sizeof chunk.format);
    chunk.format = std::byteswap(chunk.format);

    istrm.peek_consume(reinterpret_cast<std::uint8_t*>(&chunk.tracks), sizeof chunk.tracks);
    chunk.tracks = std::byteswap(chunk.tracks);

    istrm.peek_consume(reinterpret_cast<std::uint8_t*>(&chunk.division), sizeof chunk.division);
    chunk.division = std::byteswap(chunk.division);

    return chunk;
}

auto midi::open(std::string const& midi_file) -> void {
    istrm strm(midi_file);
    m_header = mthd::read(strm);
    while (strm.has_next())
        m_tracks.push_back(mtrk::read(strm));

    fmt::print("{}\n", m_header.str());
    for (auto const& track : m_tracks)
        fmt::print("{}\n", track.str());
}
}
