#include <vector>
#include <string_view>
#include <fstream>
#include <bit>
#include <stdexcept>
#include <string>
#include <cstddef>
#include <initializer_list>
#include <chrono>
#include <thread>

#include "fmt/format.h"

#ifdef MDP_MINIAUDIO
#include "miniaudio.h"
#endif

// Resources:
//   Standard MIDI-File Format Spec. 1.1
//      https://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html#BMA1_
//   MIDI Files Specification
//      http://www.somascape.org/midi/tech/mfile.html
//   Outline of the Standard MIDI File Structure
//      https://www.ccarh.org/courses/253/handout/smf/
//   Variable-length quantity
//      https://en.wikipedia.org/wiki/Variable-length_quantity
//   Programming MIDI by javdix9
//      https://youtu.be/040BKtnDdg0?si=AdAnEDt5iF9dta0T

namespace mdp {
class istrm {
public:
    istrm(std::string const& path) : m_data(), m_cursor(0), m_end(0) {
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

    auto read_string(std::size_t size) -> std::string {
        std::string str{};
        str.resize(size, 0x00);
        peek_consume(reinterpret_cast<std::uint8_t*>(str.data()), size);
        return str;
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
};

enum class event_type : std::uint8_t {
    note_off         = 0x80,
    note_on          = 0x90,
    after_touch      = 0xA0,
    control_change   = 0xB0,
    program_change   = 0xC0,
    channel_pressure = 0xD0,
    pitch_bend       = 0xE0,
    system_exclusive = 0xF0,
};;

constexpr auto operator==(std::uint8_t a, event_type b) -> bool {
    using T = std::underlying_type_t<event_type>;
    return static_cast<T>(b) == a;
}

enum class meta_type : std::uint8_t {
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

struct mtrk {
    char const* type = "MTrk";

    auto str() const -> std::string {
        return fmt::format("{} {{ }}",
                           type);
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
        std::uint8_t prev_status = 0x00;
        istrm.set_max_consume(m_byte_size);
        while (istrm.has_next()) {
            dt = istrm.read_vlq();
            auto status = istrm.peek();
            if (status < 0x80) status = prev_status;

            if ((status & 0xF0) == event_type::program_change) {
                prev_status = status;

                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t program = istrm.peek_consume() & 0x7F;
                fmt::print("Program Change CH: {}, PR: {}\n", channel, program);
            } else if ((status & 0xF0) == event_type::note_on) {
                prev_status = status;

                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t key = istrm.peek_consume() & 0x7F;
                std::uint8_t vel = istrm.peek_consume() & 0x7F;
                fmt::print("{} Note On CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);
            } else if ((status & 0xF0) == event_type::note_off) {
                prev_status = status;

                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t key = istrm.peek_consume() & 0x7F;
                std::uint8_t vel = istrm.peek_consume() & 0x7F;
                fmt::print("{} Note Off CH: {}, Key: {}, Vel {}\n", dt, channel, key, vel);
            } else if ((status & 0xF0) == event_type::control_change) {
                prev_status = status;

                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t c = istrm.peek_consume() & 0x7F;
                std::uint8_t v = istrm.peek_consume() & 0x7F;
                fmt::print("{} Channel Mode Messages CH: {}, c: {}, v: {}\n", dt, channel, c, v);
            } else if ((status & 0xF0) == event_type::pitch_bend) {
                prev_status = status;

                std::uint8_t channel = istrm.peek_consume() & 0x0F;
                std::uint8_t l = istrm.peek_consume() & 0x7F;
                std::uint8_t m = istrm.peek_consume() & 0x7F;
                fmt::print("{} Pitch Wheel Change CH: {}, l: {}, m: {}\n", dt, channel, l, m);
            } else if ((status & 0xF0) == event_type::system_exclusive) {
                prev_status = 0x00;

                if (istrm.peek() == 0xFF) {
                    istrm.consume();
                    auto const meta_value = static_cast<meta_type>(istrm.peek_consume());
                    auto const len  = istrm.read_vlq();

                    switch (meta_value) {
                        case meta_type::sequence_num: {
                            fmt::print("Sequence Number\n");
                            break;
                        }
                        case meta_type::text: {
                            fmt::print("Text Event\n");
                            break;
                        }
                        case meta_type::copyright: {
                            fmt::print("Copyright Notice\n");
                            break;
                        }
                        case meta_type::track_name: {
                            fmt::print("Tack Name: {}\n", istrm.read_string(len));
                            break;
                        }
                        case meta_type::instrument_name: {
                            fmt::print("Instrument Name\n");
                            break;
                        }
                        case meta_type::lyric: {
                            fmt::print("Lyric\n");
                            break;
                        }
                        case meta_type::marker: {
                            fmt::print("Marker\n");
                            break;
                        }
                        case meta_type::cue_point: {
                            fmt::print("Cue Point\n");
                            break;
                        }
                        case meta_type::channel_prefix: {
                            fmt::print("MIDI Channel Prefix\n");
                            break;
                        }
                        case meta_type::track_end: {
                            fmt::print("End of Track\n");
                            break;
                        }
                        case meta_type::tempo: {
                            std::uint8_t tempo_data[3]{0};
                            istrm.peek_consume(tempo_data, sizeof tempo_data);
                            std::uint32_t const tempo = (std::uint32_t(tempo_data[2]) << 0)
                                                      | (std::uint32_t(tempo_data[1]) << 8)
                                                      | (std::uint32_t(tempo_data[0]) << 16);
                            fmt::print("Set Tempo {} us ({} BPM)\n", tempo, 60'000'000/tempo);
                            break;
                        }
                        case meta_type::smpte_offset: {
                            fmt::print("SMPTE Offset\n");
                            break;
                        }
                        case meta_type::time_signature: {
                            auto const numerator    = istrm.peek_consume();
                            auto const denominator  = istrm.peek_consume();
                            auto const clicks       = istrm.peek_consume();
                            auto const quarter_note = istrm.peek_consume();
                            fmt::print("Time Signature {}/{}, {} MIDI clocks, {}\n", numerator, denominator, clicks, quarter_note);
                            break;
                        }
                        case meta_type::key_signature: {
                            fmt::print("Key Signature\n");
                            break;
                        }
                        case meta_type::seq_specific: {
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
    mdp::midi midi{};
    midi.open("./SuperMarioBros.mid");

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

