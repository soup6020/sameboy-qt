#pragma once

#include <functional>

extern "C" {
#include <Core/gb.h>
}

struct SDL_AudioStream;

// Pull-model stereo 16-bit output, equivalent to AppleCommon/GBAudioClient.
// The renderer is called on SDL's audio thread and must fill |frames| samples.
class AudioOutput
{
public:
    using Renderer = std::function<void(unsigned sampleRate, unsigned frames, GB_sample_t *buffer)>;

    AudioOutput(Renderer renderer, unsigned sampleRate);
    ~AudioOutput();

    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    bool start();
    void stop();
    bool isPlaying() const { return m_playing; }
    unsigned sampleRate() const { return m_sampleRate; }

private:
    static void callback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount);

    Renderer m_renderer;
    unsigned m_sampleRate;
    SDL_AudioStream *m_stream = nullptr;
    bool m_playing = false;
};
