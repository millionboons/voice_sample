#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <stdexcept> // For std::runtime_error

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
const int SAMPLE_RATE = 48000;
const int FRAMES_PER_BUFFER = 480; // 10ms of audio at 48kHz
// For ultra-low latency, could go to 240 (5ms) or 120 (2.5ms)
const int NUM_CHANNELS = 1;      // Mono
const int BITRATE = 20000;       // Opus bitrate (20kbps is good for speech)

// Target IP address and port for destination (hardcoded for simplicity)
// In a real app, this would come from a discovery mechanism
// *** IMPORTANT: Change these IPs to the actual IP addresses of your LAN computers! ***
const std::string TARGET_IP = "192.168.1.100"; // Replace with actual peer IP
const unsigned short TARGET_PORT = 12345;
const unsigned short LISTEN_PORT = 12345;


int main() {
    std::cout << "Starting real-time voice communication system...\n";

    // Initialize modules (basic error checking)
    try {
        if (!AudioCodec::initializeEncoder(SAMPLE_RATE, NUM_CHANNELS, BITRATE)) {
            std::cerr << "Failed to initialize Opus encoder.\n";
            return 1;
        }
        if (!AudioCodec::initializeDecoder(SAMPLE_RATE, NUM_CHANNELS)) {
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
            AudioCapture capture(SAMPLE_RATE, FRAMES_PER_BUFFER, NUM_CHANNELS);
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
            AudioPlayback playback(SAMPLE_RATE, FRAMES_PER_BUFFER, NUM_CHANNELS);
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

    std::cout << "System shutdown.\n";
    return 0;
}