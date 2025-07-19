
// Ensure this is the first thing in main.cpp if you're on Windows
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

// Then standard library headers
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <stdexcept>
#include <objbase.h>



// Include your custom headers for audio and network modules
#include "AudioCapture.h"
#include "AudioPlayback.h"
#include "NetworkSender.h"
#include "NetworkReceiver.h"
#include "AudioCodec.h" // For Opus
#include "PacketQueue.h" // A thread-safe queue for audio packets

// Global queues for inter-thread communication
PacketQueue<std::vector<unsigned char>> sendQueue; // Raw audio frames or encoded packets
PacketQueue<std::vector<unsigned char>> recvQueue; // Received network packets

// Example configuration (you'd make this dynamic)
int SAMPLE_RATE_ENCODE = 48000;
int SAMPLE_RATE_DECODE = 48000;
int INPUT_NUM_CHANNELS = 2; // Mono input
int OUTPUT_NUM_CHANNELS = 2; // Stereo output (for playback)
int FRAMES_PER_BUFFER = 240; // 10ms of audio at 48kHz
// For ultra-low latency, could go to 240 (5ms) or 120 (2.5ms)
int BITRATE = 64000;       // Opus bitrate (20kbps is good for speech)

// Target IP address and port for destination (hardcoded for simplicity)
// In a real app, this would come from a discovery mechanism
// *** IMPORTANT: Change these IPs to the actual IP addresses of your LAN computers! ***
std::string TARGET_IP = "192.168.1.34"; // Replace with actual peer IP
const unsigned short TARGET_PORT = 12345;
const unsigned short LISTEN_PORT = 12345;


int getsamplerates() {

    // Initialize COM before using PortAudio on Windows with WASAPI
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "COM initialization failed.\n";
        return -1;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    PaDeviceIndex inputDevice = Pa_GetDefaultInputDevice();
    if (inputDevice == paNoDevice) {
        std::cerr << "Error: No default input device found." << std::endl;
        return 1;
    }

    const PaDeviceInfo* inputInfo = Pa_GetDeviceInfo(inputDevice);
    if (!inputInfo) {
        std::cerr << "Error: Failed to get default input device info." << std::endl;
        return 1;
    }

    SAMPLE_RATE_ENCODE = static_cast<int>(inputInfo->defaultSampleRate);
    INPUT_NUM_CHANNELS = inputInfo->maxInputChannels;

    std::cout << "   Default Microphone:" << std::endl;
    std::cout << "   Name: " << inputInfo->name << std::endl;
    std::cout << "   Default Sample Rate: " << SAMPLE_RATE_ENCODE << std::endl;
    std::cout << "   Max Input Channels: " << INPUT_NUM_CHANNELS << std::endl;


    PaDeviceIndex outputDevice = Pa_GetDefaultOutputDevice();
    if (outputDevice == paNoDevice) {
        std::cerr << "Error: No default output device found." << std::endl;
        return 1;
    }

    const PaDeviceInfo* outputInfo = Pa_GetDeviceInfo(outputDevice);
    if (!outputInfo) {
        std::cerr << "Error: Failed to get default output device info." << std::endl;
        return 1;
    }

    SAMPLE_RATE_DECODE = static_cast<int>(outputInfo->defaultSampleRate);
    OUTPUT_NUM_CHANNELS = outputInfo->maxOutputChannels;

    std::cout << "   Default Speaker:" << std::endl;
    std::cout << "   Name: " << outputInfo->name << std::endl;
    std::cout << "   Default Sample Rate: " << SAMPLE_RATE_DECODE << std::endl;
    std::cout << "   Max Output Channels: " << outputInfo->maxOutputChannels << std::endl;

    SAMPLE_RATE_DECODE = SAMPLE_RATE_ENCODE; // For simplicity, use the same sample rate for encoding and decoding

}


int main() {
    std::cout << "Starting real-time voice communication system...\n";
    getsamplerates();

    // Open the file
    std::ifstream configFile("ip.txt");
    if (!configFile) {
        std::cerr << "Error: Could not open ip.txt" << std::endl;
        return 1;
    }

    // Read the first line
    configFile >> FRAMES_PER_BUFFER;
    configFile >> BITRATE;
    configFile >> TARGET_IP;
    //std::getline(inputFile, TARGET_IP);


    // Close the file
    configFile.close();

    // Show result
    std::cout << "IP Address read from file: " << TARGET_IP << std::endl;
    // std::cout << "Loaded configuration:\n";
    // std::cout << "  SAMPLE_RATE = " << SAMPLE_RATE << "\n";
    std::cout << "  FRAMES_PER_BUFFER = " << FRAMES_PER_BUFFER << "\n";
    // std::cout << "  NUM_CHANNELS = " << NUM_CHANNELS << "\n";
    std::cout << "  BITRATE = " << BITRATE << "\n";
    std::cout << "  TARGET_IP = " << TARGET_IP << "\n";



    // Initialize modules (basic error checking)
    try {
        if (!AudioCodec::initializeEncoder(SAMPLE_RATE_ENCODE, INPUT_NUM_CHANNELS, BITRATE)) {
            std::cerr << "Failed to initialize Opus encoder.\n";
            return 1;
        }
        if (!AudioCodec::initializeDecoder(SAMPLE_RATE_DECODE, OUTPUT_NUM_CHANNELS)) {
            std::cerr << "Failed to initialize Opus decoder.\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        return 1;
    }


    // --- Create and start threads ---

    // 1. Audio Capture Thread
    std::thread captureThread([&]() {
        try {


            AudioCapture capture(SAMPLE_RATE_ENCODE, FRAMES_PER_BUFFER, INPUT_NUM_CHANNELS);
            if (!capture.start()) {
                std::cerr << "Failed to start audio capture.\n";
                return;
            }
            std::cout << "Audio capture started.\n";
            while (true) { // Loop indefinitely (add a stop condition for a real app)
                std::vector<float> audioData = capture.readBlocking();
                if (!audioData.empty()) {
                    // Encode and push to send queue (simplified, might need separate thread for encoding)
                    std::vector<unsigned char> encodedPacket = AudioCodec::encode(audioData);
                    if (!encodedPacket.empty()) {
                        sendQueue.push(encodedPacket);
                    }
                }
            }
            capture.stop();
        }
        catch (const std::exception& e) {
            std::cerr << "Audio capture thread error: " << e.what() << std::endl;
        }
        });

    // 2. Network Send Thread
    std::thread senderThread([&]() {
        try {
            NetworkSender sender(TARGET_IP, TARGET_PORT);
            std::cout << "Network sender started.\n";
            while (true) {
                std::vector<unsigned char> packet = sendQueue.pop(); // Blocks until data available
                if (!packet.empty()) {
                    sender.sendPacket(packet);
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Network sender thread error: " << e.what() << std::endl;
        }
        });

    // 3. Network Receive Thread
    std::thread receiverThread([&]() {
        try {
            NetworkReceiver receiver(LISTEN_PORT);
            if (!receiver.start()) {
                std::cerr << "Failed to start network receiver.\n";
                return;
            }
            std::cout << "Network receiver started.\n";
            while (true) {
                std::vector<unsigned char> packet = receiver.receivePacketBlocking();
                if (!packet.empty()) {
                    recvQueue.push(packet);
                }
            }
            receiver.stop();
        }
        catch (const std::exception& e) {
            std::cerr << "Network receiver thread error: " << e.what() << std::endl;
        }
        });

    // 4. Audio Playback Thread
    std::thread playbackThread([&]() {
        try {
            AudioPlayback playback(SAMPLE_RATE_DECODE, FRAMES_PER_BUFFER, OUTPUT_NUM_CHANNELS);
            if (!playback.start()) {
                std::cerr << "Failed to start audio playback.\n";
                return;
            }
            std::cout << "Audio playback started.\n";
            while (true) {
                std::vector<unsigned char> encodedPacket = recvQueue.pop(); // Blocks
                if (!encodedPacket.empty()) {
                    std::vector<float> decodedAudio = AudioCodec::decode(encodedPacket);
                    if (!decodedAudio.empty()) {
                        playback.playBlocking(decodedAudio);
                    }
                }
            }
            playback.stop();
        }
        catch (const std::exception& e) {
            std::cerr << "Audio playback thread error: " << e.what() << std::endl;
        }
        });


    // Join threads (in a real app, you'd have a graceful shutdown mechanism)
    captureThread.join();
    senderThread.join();
    receiverThread.join();
    playbackThread.join();

    AudioCodec::cleanupEncoder();
    AudioCodec::cleanupDecoder();
    Pa_Terminate(); // Terminate PortAudio if used

    std::cout << "System shutdown.\n";
    return 0;
}


