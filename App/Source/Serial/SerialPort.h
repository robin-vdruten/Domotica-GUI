#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace Serial {

    struct SerialConfig {
        std::string PortName;
        int BaudRate = 115200;
    };

    class SerialPort {
    public:
        using DataCallback = std::function<void(const std::string&)>;

        SerialPort();
        ~SerialPort();

        bool Open(const SerialConfig& config);
        void Close();
        bool Send(const std::string& data);

        void SetDataCallback(const DataCallback& callback) { m_Callback = callback; }
        bool IsConnected() const { return m_Connected; }

        static std::vector<std::string> ScanPorts();

    private:
        void ReadLoop();

    private:
        DataCallback m_Callback;
        std::atomic<bool> m_Connected{ false };
        std::atomic<bool> m_Running{ false };

        std::thread m_ReadThread;
        void* m_Handle = nullptr;
    };
}