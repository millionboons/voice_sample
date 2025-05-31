#ifndef AUDIO_PLAYBACK_H
#define AUDIO_PLAYBACK_H

#include <portaudio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <deque> // For simple playback buffer/jitter
#include <algorithm> // For std::fill

class AudioPlayback {
public:
    AudioPlayback(int sampleRate, int framesPerBuffer, int numChannels);
    ~AudioPlayback();

    bool start();
    void stop();
    void playBlocking(const std::vector<float>& audioData); // Add audio to playback buffer

private:
    static int paCallback(const void* inputBuffer, void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData);

    PaStream* stream;
    int sampleRate_;
    int framesPerBuffer_;
    int numChannels_;

    // Simple playback buffer (can be optimized with a proper jitter buffer)
    std::deque<float> playbackBuffer_;
    std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // AUDIO_PLAYBACK_H