#include "AudioOutput.h"

#include <QtGlobal>
#include <SDL3/SDL.h>
#include <vector>

AudioOutput::AudioOutput(Renderer renderer, unsigned sampleRate)
    : m_renderer(std::move(renderer)), m_sampleRate(sampleRate)
{
}

AudioOutput::~AudioOutput()
{
    stop();
}

bool AudioOutput::start()
{
    if (m_playing) {
        return true;
    }
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        qWarning("sameboy-qt: SDL audio init failed: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = int(m_sampleRate);
    m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &AudioOutput::callback, this);
    if (!m_stream) {
        qWarning("sameboy-qt: could not open audio device: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(m_stream);
    m_playing = true;
    return true;
}

void AudioOutput::stop()
{
    if (!m_stream) {
        return;
    }
    m_playing = false;
    SDL_DestroyAudioStream(m_stream);
    m_stream = nullptr;
}

void AudioOutput::callback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    const unsigned frames = unsigned(additionalAmount) / sizeof(GB_sample_t);
    if (!frames) {
        return;
    }
    thread_local std::vector<GB_sample_t> buffer;
    buffer.resize(frames);
    self->m_renderer(self->m_sampleRate, frames, buffer.data());
    SDL_PutAudioStreamData(stream, buffer.data(), int(frames * sizeof(GB_sample_t)));
}
