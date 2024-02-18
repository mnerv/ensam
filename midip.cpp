#include <vector>
#include <string_view>
#include <fstream>
#include <bit>
#include <stdexcept>
#include <string>
#include <cstddef>
#include <initializer_list>

#include "fmt/format.h"

#ifdef MDP_MINIAUDIO
#include "miniaudio.h"
#endif

// https://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html#BMA1_
// http://www.somascape.org/midi/tech/mfile.html
// https://www.ccarh.org/courses/253/handout/smf/
// https://en.wikipedia.org/wiki/Variable-length_quantity

namespace mdp {
class istrm {
public:
    istrm(std::string const& path) : m_data(), m_cursor(0), m_end(0), m_sequence_size(0) {
        std::ifstream fstrm(path, std::ios::binary);
        if (!fstrm.is_open())
            throw std::runtime_error(fmt::format("Error openning file: {}", path));
        fstrm.unsetf(std::ios::skipws);  // Stop eating new lines in binary mode!!!

        fstrm.seekg(0, std::ios::end);
        auto const file_size = static_cast<std::size_t>(fstrm.tellg());
        fstrm.seekg(0, std::ios::beg);
        m_data.reserve(file_size);

        std::copy(std::istream_iterator<char>(fstrm), std::istream_iterator<char>(),
                  std::back_inserter(m_data));

        m_end = m_data.size();
        fstrm.close();
    }

    auto consume(std::size_t size = 1) -> std::size_t {
        if (m_cursor >= m_data.size()) {
            m_cursor = m_data.size();
            return 0;
        }
        m_cursor += size;
        return size;
    }
    auto consume_last_sequence() -> void {
        m_cursor += m_sequence_size;
        m_sequence_size = 0;
    }
    auto peek_consume() -> std::uint8_t {
        return m_data[m_cursor++];
    }
    auto peek_consume(std::uint8_t* dst, std::size_t size) -> void {
        if (m_cursor + size > m_data.size()) return;
        for (std::size_t i = 0; i < size; ++i) {
            dst[i] = m_data[i + m_cursor];
        }
        m_cursor += size;
    }
    auto peek() const -> std::uint8_t {
        return m_data[m_cursor];
    }
    auto peek(std::uint8_t* dst, std::size_t size) const -> std::size_t {
        if (m_cursor + size > m_data.size()) return 0;
        for (std::size_t i = 0; i < size; ++i) {
            dst[i] = m_data[i + m_cursor];
        }
        return size;
    }
    auto is_sequence_match(std::initializer_list<std::uint8_t> const& seq) -> bool {
        if (m_cursor + seq.size() > m_data.size()) return false;
        m_sequence_size = seq.size();
        auto it = seq.begin();
        for (std::size_t i = 0; i < m_sequence_size; ++i, ++it) {
            if (*it != m_data[m_cursor + i]) return false;
        }
        return true;
    }
    auto has_next(std::size_t offset = 1) const -> bool {
        return m_cursor + offset < m_data.size() && m_cursor + offset < m_end;
    }
    auto set_max_consume(std::size_t offset) -> void {
        if (m_cursor + offset >= m_end) return;
        m_end = m_cursor + offset;
    }
    auto reset_max_consume() -> void {
        m_end = m_data.size();
    }

    auto read_vlq() -> std::uint32_t {
        if (!(peek() & 0x80)) return std::uint32_t(peek_consume());
        std::uint32_t value = peek_consume() & 0x7F;
        do {
            value = (value << 7) + (peek() & 0x7F);
        } while (peek_consume() & 0x80);
        return value;
    }

private:
    std::vector<std::uint8_t> m_data;
    std::size_t m_cursor;
    std::size_t m_end;
    std::size_t m_sequence_size;
};

struct sysex_event {};

struct midi_event {};

enum class meta_type : std::uint32_t {
    none         = 0x00'00'00,  // Not meta event
    seq_num      = 0xFF'00'02,  // Sequence Number
    text         = 0xFF'01,     // Text Event
    copyright    = 0xFF'02,     // Copyright Notice
    trk_name     = 0xFF'03,     // Sequence/Track Name
    instr_name   = 0xFF'04,     // Instrument Name
    lyric        = 0xFF'05,     // Lyric
    marker       = 0xFF'06,     // Marker
    cue_point    = 0xFF'07,     // Cue Point
    ch_prefix    = 0xFF'20'01,  // MIDI Channel Prefix
    trk_end      = 0xFF'2F,     // End of Track
    tempo        = 0xFF'51'03,  // Set Tempo (in microseconds per MIDI quarter-note)
    smpte_offset = 0xFF'54'05,  // SMPTE Offset
    time_signa   = 0xFF'58'04,  // Time Signature
    key_signa    = 0xFF'59'02,  // Key Signature
    seq_spec     = 0xFF'7F'00,  // Sequencer Specific Meta-Event
};

struct meta_event {};

struct meta_seq {};

struct meta_text {
    std::string text;
};

struct meta_ch_prefix {
    std::uint8_t channel;
};

struct meta_tempo {
    std::uint8_t numerator;
    std::uint8_t denominator;
    std::uint8_t clicks;
    std::uint8_t quarter_note;
};

enum class mtrk_event_type : std::uint32_t {
    meta,
    midi,
    sysex
};

struct mtrk_event {
    mtrk_event_type type;
    std::size_t index;
};

struct mtrk {
    char const* type = "MTrk";
    std::vector<mtrk_event> events{};

    auto str() const -> std::string {
        return fmt::format("{} {{ events: {} }}",
                           type,  events.size());
    }

    static auto read(mdp::istrm& istrm) -> mtrk {
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

private:
    auto parse(mdp::istrm& istrm) -> void {
        [[maybe_unused]]std::uint32_t dt = 0;
        istrm.set_max_consume(m_byte_size);
        while (istrm.has_next()) {
            dt = istrm.read_vlq();
            if (istrm.is_sequence_match({0xFF, 0x00, 0x02})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Sequence Number\n");
            } else if (istrm.is_sequence_match({0xFF, 0x01})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Text Event\n");
            } else if (istrm.is_sequence_match({0xFF, 0x02})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Copyright Notice\n");
            } else if (istrm.is_sequence_match({0xFF, 0x03})) {
                istrm.consume_last_sequence();
                std::uint32_t len = istrm.read_vlq();
                std::vector<char> text_data{};
                text_data.resize(len, 0x00);
                istrm.peek_consume(reinterpret_cast<std::uint8_t*>(text_data.data()), len);
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Tack Name: {}\n", std::string_view{text_data});
            } else if (istrm.is_sequence_match({0xFF, 0x04})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Instrument Name\n");
            } else if (istrm.is_sequence_match({0xFF, 0x05})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Lyric\n");
            } else if (istrm.is_sequence_match({0xFF, 0x06})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Marker\n");
            } else if (istrm.is_sequence_match({0xFF, 0x07})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Cue Point\n");
            } else if (istrm.is_sequence_match({0xFF, 0x20, 0x01})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("MIDI Channel Prefix\n");
            } else if (istrm.is_sequence_match({0xFF, 0x2F, 0x00})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("End of Track\n");
            } else if (istrm.is_sequence_match({0xFF, 0x51, 0x03})) {
                istrm.consume_last_sequence();
                std::uint8_t tempo_data[3]{0};
                istrm.peek_consume(tempo_data, sizeof tempo_data);
                std::uint32_t const tempo = (std::uint32_t(tempo_data[2]) << 0)
                                          | (std::uint32_t(tempo_data[1]) << 8)
                                          | (std::uint32_t(tempo_data[0]) << 16);
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Set Tempo {} ms per quarter note\n", tempo);
            } else if (istrm.is_sequence_match({0xFF, 0x54, 0x05})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("SMPTE Offset\n");
            } else if (istrm.is_sequence_match({0xFF, 0x58, 0x04})) {
                istrm.consume_last_sequence();
                auto const numerator    = istrm.peek_consume();
                auto const denominator  = istrm.peek_consume();
                auto const clicks       = istrm.peek_consume();
                auto const quarter_note = istrm.peek_consume();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Time Signature {}/{}, {} MIDI clocks, {}\n", numerator, denominator, clicks, quarter_note);
            } else if (istrm.is_sequence_match({0xFF, 0x59, 0x02})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Key Signature");
            } else if (istrm.is_sequence_match({0xFF, 0x7F})) {
                istrm.consume_last_sequence();
                events.push_back({mtrk_event_type::meta, 0});
                fmt::print("Sequencer Specific Meta-Event\n");
            } else if ((istrm.peek() & 0xF0) == 0xC0) {
                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t program = istrm.peek_consume() & 0x7F;
                events.push_back({mtrk_event_type::midi, 0});
                fmt::print("Program Change CH: {}, PR: {}\n", channel, program);
            } else if ((istrm.peek() & 0xF0) == 0x90) {
                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t key = istrm.peek_consume() & 0x7F;
                std::uint8_t vel = istrm.peek_consume() & 0x7F;
                events.push_back({mtrk_event_type::midi, 0});
                fmt::print("{} Note On CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);
            } else if ((istrm.peek() & 0xF0) == 0x80) {
                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t key = istrm.peek_consume() & 0x7F;
                std::uint8_t vel = istrm.peek_consume() & 0x7F;
                events.push_back({mtrk_event_type::midi, 0});
                fmt::print("{} Note Off CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);
            } else if ((istrm.peek() & 0xF0) == 0xB0) {
                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t c = istrm.peek_consume() & 0x7F;
                std::uint8_t v = istrm.peek_consume() & 0x7F;
                events.push_back({mtrk_event_type::midi, 0});
                fmt::print("{} Channel Mode Messages CH: {}, c: {}, v: {}\n", dt, channel, c, v);
            } else if ((istrm.peek() & 0xF0) == 0xE0) {
                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t l = istrm.peek_consume() & 0x7F;
                std::uint8_t m = istrm.peek_consume() & 0x7F;
                events.push_back({mtrk_event_type::midi, 0});
                fmt::print("{} Pitch Wheel Change CH: {}, l: {}, m: {}\n", dt, channel, l, m);
            } else {
                fmt::print("Not Implemented: {:#04x}\n", istrm.peek_consume());
            }
        }
        istrm.reset_max_consume();
    }

private:
    std::uint32_t m_byte_size{};
};

struct mthd {
    char const* type = "MThd";
    std::uint32_t length;
    std::uint16_t format;
    std::uint16_t tracks;
    std::uint16_t division;

    auto str() const -> std::string {
        return fmt::format("{} {{ length: {}, format: {}, tracks: {}, division: {} }}",
                           type,  length, format, tracks, division);
    }

    static auto read(mdp::istrm& istrm) -> mthd {
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
};

class midi {
public:
    auto open(std::string const& midi_file) -> void {
        istrm strm(midi_file);
        m_header = mthd::read(strm);
        while (strm.has_next())
            m_tracks.push_back(mtrk::read(strm));

        fmt::print("{}\n", m_header.str());
        for (auto const& track : m_tracks)
            fmt::print("{}\n", track.str());
    }

private:
    mthd m_header{};
    std::vector<mtrk> m_tracks{};
};
}

#ifdef MDP_MINIAUDIO
auto callback([[maybe_unused]]ma_device* device, [[maybe_unused]]void* output, [[maybe_unused]]void const* input, [[maybe_unused]]ma_uint32 frame_count) -> void {
    ma_waveform* ptr = (ma_waveform*)device->pUserData;
    ma_waveform_read_pcm_frames(ptr, output, frame_count, nullptr);
}
#endif

static auto entry([[maybe_unused]]std::vector<std::string_view> const& args) -> void {
    // waveform_config = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_square, 0.1, 880.0);
    // ma_waveform_init(&waveform_config, &waveform);
    // std::this_thread::sleep_for(2s);

#ifdef MDP_MINIAUDIO
    ma_waveform_config waveform_config{};
    ma_waveform waveform{};
    ma_device device{};

    ma_device_config config  = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate        = 48'000;
    config.dataCallback      = callback;
    config.pUserData         = &waveform;

    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
        throw std::runtime_error("Failed to open playback device.");

    fmt::print("Device Name: {}\n", device.playback.name);
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        throw std::runtime_error("Failed to start playback device.\n");
    }

    using namespace std::chrono_literals;
    for (auto i = 0; i < 128; ++i) {
        waveform_config = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_square, 0.05, 440.0);
        ma_waveform_init(&waveform_config, &waveform);
        std::this_thread::sleep_for(50ms);
        waveform_config = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_square, 0.0, 440.0);
        ma_waveform_init(&waveform_config, &waveform);
        std::this_thread::sleep_for(50ms);
    }

    ma_device_uninit(&device);
#endif
}

auto main([[maybe_unused]]int argc, [[maybe_unused]]char const* argv[]) -> int {
    try {
        entry({argv, std::next(argv, argc)});
    } catch (std::exception const& e) {
        fmt::print(stderr, "entry: {}\n", e.what());
        return 1;
    }
    return 0;
}

