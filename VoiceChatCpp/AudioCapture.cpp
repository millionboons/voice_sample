#include "AudioCapture.h"

AudioCapture::AudioCapture(int sampleRate, int framesPerBuffer, int numChannels)
    : stream(nullptr),
    sampleRate_(sampleRate),
    framesPerBuffer_(framesPerBuffer),
    numChannels_(numChannels),
    audioBuffer_(framesPerBuffer* numChannels),
    bufferReady_(false)
{


    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("PortAudio initialization failed: " + std::string(Pa_GetErrorText(err)));
    }
}

AudioCapture::~AudioCapture() {
    stop();
    PaError err = Pa_Terminate();
    if (err != paNoError) {
        std::cerr << "PortAudio termination error: " << Pa_GetErrorText(err) << std::endl;
    }
}

bool AudioCapture::start() {
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        std::cerr << "Error: No default input device." << std::endl;
        return false;
    }
    inputParameters.channelCount = numChannels_;
    inputParameters.sampleFormat = paFloat32; // Use float32 for audio processing
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream,
        &inputParameters,
        nullptr, // No output
        sampleRate_,
        framesPerBuffer_,
        paClipOff, // We won't be clipping here
        paCallback,
        this // Pass 'this' as userData to access member variables
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

void AudioCapture::stop() {
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

std::vector<float> AudioCapture::readBlocking() {
    std::unique_lock<std::mutex> lock(mutex_);
    condVar_.wait(lock, [this] { return bufferReady_; }); // Wait until new buffer is ready
    bufferReady_ = false; // Reset flag
    return audioBuffer_; // Return a copy (could optimize with move semantics or shared_ptr)
}

int AudioCapture::paCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
    AudioCapture* This = static_cast<AudioCapture*>(userData);
    const float* in = static_cast<const float*>(inputBuffer);

    if (inputBuffer == nullptr) {
        // No input, fill with silence or handle error
        return paContinue;
    }

    std::lock_guard<std::mutex> lock(This->mutex_);
    // Copy input buffer to our internal buffer
    std::copy(in, in + (framesPerBuffer * This->numChannels_), This->audioBuffer_.begin());
    This->bufferReady_ = true;
    This->condVar_.notify_one(); // Notify thread that buffer is ready

    return paContinue;
}