#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <portaudio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <mutex>
#include <condition_variable>
#include <algorithm> // For std::copy

class AudioCapture {
public:
    AudioCapture(int sampleRate, int framesPerBuffer, int numChannels);
    ~AudioCapture();

    bool start();
    void stop();
    std::vector<float> readBlocking(); // Read a buffer of audio

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
    std::vector<float> audioBuffer_; // Buffer to store captured audio
    bool bufferReady_;
    std::mutex mutex_;
    std::condition_variable condVar_;
};

#endif // AUDIO_CAPTURE_H