#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <vector>
#include <string>
#include <iostream> // Keep this for now, though it might be part of the operator<< issue
#include <opus.h> // <-- Use THIS if your 'Additional Include Directories' poi

class AudioCodec {
public:
    static bool initializeEncoder(int sampleRate, int channels, int bitrate);
    static bool initializeDecoder(int sampleRate, int channels);
    static std::vector<unsigned char> encode(const std::vector<float>& pcmData);
    static std::vector<float> decode(const std::vector<unsigned char>& encodedData);
    static void cleanupEncoder();
    static void cleanupDecoder();

private:
    static OpusEncoder* encoder;
    static OpusDecoder* decoder;
    static int numChannels_;
    static int sampleRate_;
};

#endif // AUDIO_CODEC_H