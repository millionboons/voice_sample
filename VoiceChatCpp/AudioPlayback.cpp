#include "AudioPlayback.h"

AudioPlayback::AudioPlayback(int sampleRate, int framesPerBuffer, int numChannels)
    : stream(nullptr),
    sampleRate_(sampleRate),
    framesPerBuffer_(framesPerBuffer),
    numChannels_(numChannels)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("PortAudio initialization failed: " + std::string(Pa_GetErrorText(err)));
    }
}

AudioPlayback::~AudioPlayback() {
    stop();
    PaError err = Pa_Terminate();
    if (err != paNoError) {
        std::cerr << "PortAudio termination error: " << Pa_GetErrorText(err) << std::endl;
    }
}

bool AudioPlayback::start() {
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "Error: No default output device." << std::endl;
        return false;
    }
    outputParameters.channelCount = numChannels_;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream,
        nullptr, // No input
        &outputParameters,
        sampleRate_,
        framesPerBuffer_,
        paClipOff,
        paCallback,
        this
    );

    if (err != paNoError) {
        std::cerr << "PortAudio open stream error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "PortAudio start stream error: " << Pa_GetErrorText(err) << std::endl;
        return false;
    }
    return true;
}

void AudioPlayback::stop() {
    if (stream) {
        PaError err = Pa_StopStream(stream);
        if (err != paNoError && err != paStreamIsNotStopped) {
            std::cerr << "PortAudio stop stream error: " << Pa_GetErrorText(err) << std::endl;
        }
        err = Pa_CloseStream(stream);
        if (err != paNoError) {
            std::cerr << "PortAudio close stream error: " << Pa_GetErrorText(err) << std::endl;
        }
        stream = nullptr;
    }
}

void AudioPlayback::playBlocking(const std::vector<float>& audioData) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Append incoming audio data to the playback buffer
    playbackBuffer_.insert(playbackBuffer_.end(), audioData.begin(), audioData.end());
    // Potentially notify the callback that new data is available
    condVar_.notify_one();
}

int AudioPlayback::paCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
    AudioPlayback* This = static_cast<AudioPlayback*>(userData);
    float* out = static_cast<float*>(outputBuffer);
    unsigned long framesToRead = framesPerBuffer * This->numChannels_;

    std::unique_lock<std::mutex> lock(This->mutex_);

    // Simple jitter buffer logic:
    // If we don't have enough data, fill with silence.
    // In a real system, you'd implement a more sophisticated jitter buffer
    // that aims for a target buffer size and handles packet loss/reordering.
    if (This->playbackBuffer_.size() < framesToRead) {
        std::fill(out, out + framesToRead, 0.0f); // Fill with silence
        // Optionally, wait for more data using condVar_.wait_for or similar
        // This might introduce delays if not carefully managed.
        return paContinue;
    }

    // Copy data from buffer to output
    for (unsigned long i = 0; i < framesToRead; ++i) {
        out[i] = This->playbackBuffer_.front();
        This->playbackBuffer_.pop_front();
    }

    return paContinue;
}