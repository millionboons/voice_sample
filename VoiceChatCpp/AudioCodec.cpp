#include "AudioCodec.h"

OpusEncoder* AudioCodec::encoder = nullptr;
OpusDecoder* AudioCodec::decoder = nullptr;
int AudioCodec::numChannels_ = 0;
int AudioCodec::sampleRate_ = 0;

bool AudioCodec::initializeEncoder(int sampleRate, int channels, int bitrate) {
    int error;
    encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
        return false;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(0)); // Disable VBR for more consistent latency
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(1)); // Lowest complexity for speed
    opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16)); // Use 16-bit depth
   //opus_encoder_ctl(encoder, OPUS_SET_SIGNAL_TYPE(OPUS_SIGNAL_VOICE));

    // For ultra-low latency, the frame size is critical.
    // opus_encode takes frames based on sample_rate, num_channels, and frames_per_buffer.
    // If you set FRAMES_PER_BUFFER to 480 (10ms) in main.cpp, that's what you encode.
    // Opus will handle the internal frame sizes (2.5ms, 5ms, 10ms, 20ms, 40ms, 60ms) automatically
    // based on the input frame size, but providing a small input frame size helps.

    numChannels_ = channels;
    sampleRate_ = sampleRate;
    return true;
}

bool AudioCodec::initializeDecoder(int sampleRate, int channels) {
    int error;
    decoder = opus_decoder_create(sampleRate, channels, &error);
    if (error != OPUS_OK) {
        std::cerr << "Failed to create Opus decoder: " << opus_strerror(error) << std::endl;
        return false;
    }
    numChannels_ = channels;
    sampleRate_ = sampleRate;
    return true;
}

std::vector<unsigned char> AudioCodec::encode(const std::vector<float>& pcmData) {
    if (!encoder) {
        std::cerr << "Encoder not initialized.\n";
        return {};
    }

    // opus_encode expects int16_t (short), so convert float to int16_t
    std::vector<opus_int16> pcm_int16(pcmData.size());
    for (size_t i = 0; i < pcmData.size(); ++i) {
        pcm_int16[i] = static_cast<opus_int16>(pcmData[i] * 32767.0f); // Scale float to int16_t range
    }

    const int MAX_PACKET_SIZE = 4000; // Max size for an Opus packet (reasonable for speech)
    std::vector<unsigned char> encodedData(MAX_PACKET_SIZE);

    // Number of samples per channel in the input pcmData
    int frame_size = static_cast<int>(pcm_int16.size() / numChannels_);

    opus_int32 len = opus_encode(encoder, pcm_int16.data(), frame_size,
        encodedData.data(), (opus_int32)MAX_PACKET_SIZE);

    if (len < 0) {
        std::cerr << "Opus encoding failed: " << opus_strerror(len) << std::endl;
        return {};
    }

    encodedData.resize(len); // Resize to actual encoded data length
    return encodedData;
}

std::vector<float> AudioCodec::decode(const std::vector<unsigned char>& encodedData) {
    if (!decoder) {
        std::cerr << "Decoder not initialized.\n";
        return {};
    }

    // Determine the number of samples in the decoded frame
    // Max frame size is 60ms, which at 48kHz is 2880 samples
    const int MAX_FRAME_SIZE = 6 * sampleRate_ / 100; // 60ms at sampleRate
    std::vector<opus_int16> pcm_int16(MAX_FRAME_SIZE * numChannels_);

    opus_int32 frame_size = opus_decode(decoder, encodedData.data(), (opus_int32)encodedData.size(),
        pcm_int16.data(), MAX_FRAME_SIZE, 0); // 0 for not-FEC

    if (frame_size < 0) {
        std::cerr << "Opus decoding failed: " << opus_strerror(frame_size) << std::endl;
        return {};
    }

    std::vector<float> decodedData(frame_size * numChannels_);
    for (int i = 0; i < frame_size * numChannels_; ++i) {
        decodedData[i] = static_cast<float>(pcm_int16[i]) / 32767.0f; // Scale back to float
    }
    return decodedData;
}

void AudioCodec::cleanupEncoder() {
    if (encoder) {
        opus_encoder_destroy(encoder);
        encoder = nullptr;
    }
}

void AudioCodec::cleanupDecoder() {
    if (decoder) {
        opus_decoder_destroy(decoder);
        decoder = nullptr;
    }
}