#include "SerialPort.h"

#ifdef _WIN32
#include <windows.h>
#define INVALID_HANDLE INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#define INVALID_HANDLE (void*)-1
#endif

namespace Serial {

    SerialPort::SerialPort() : m_Handle(INVALID_HANDLE) {}

    SerialPort::~SerialPort() { Close(); }

    bool SerialPort::Open(const SerialConfig& config) {
        if (m_Connected) Close();

#ifdef _WIN32

        m_Handle = CreateFileA(config.PortName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_Handle == INVALID_HANDLE) return false;

        DCB dcb = { 0 };
        dcb.DCBlength = sizeof(dcb);
        GetCommState(m_Handle, &dcb);
        dcb.BaudRate = config.BaudRate;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        SetCommState(m_Handle, &dcb);

        COMMTIMEOUTS timeouts = { 0 };
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        SetCommTimeouts(m_Handle, &timeouts);
#else
        int fd = open(config.PortName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) return false;
        m_Handle = (void*)(intptr_t)fd;
#endif

        m_Connected = true;
        m_Running = true;
        m_ReadThread = std::thread(&SerialPort::ReadLoop, this);
        return true;
    }

    void SerialPort::ReadLoop() {
        char buffer[1024];
        std::string lineBuffer;
        lineBuffer.reserve(4096);

        while (m_Running) {
            int bytesRead = 0;
#ifdef _WIN32
            DWORD dwBytesRead;
            if (ReadFile(m_Handle, buffer, sizeof(buffer) - 1, &dwBytesRead, nullptr))
                bytesRead = static_cast<int>(dwBytesRead);
#else
            bytesRead = read((int)(intptr_t)m_Handle, buffer, sizeof(buffer) - 1);
#endif
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                lineBuffer.append(buffer, bytesRead);

                size_t pos;
                while ((pos = lineBuffer.find('\n')) != std::string::npos)
                {
                    std::string line = lineBuffer.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();

                    if (!line.empty())
                        m_Callback(line);

                    lineBuffer.erase(0, pos + 1);
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    bool SerialPort::Send(const std::string& data) {
        if (!m_Connected) return false;
        std::string msg = data + "\r\n";
#ifdef _WIN32
        DWORD written;
        return WriteFile(m_Handle, msg.c_str(), (DWORD)msg.size(), &written, nullptr);
#else
        return write((int)(intptr_t)m_Handle, msg.c_str(), msg.size()) > 0;
#endif
    }

    void SerialPort::Close() {
        m_Running = false;
        if (m_ReadThread.joinable()) m_ReadThread.join();
        if (m_Handle != INVALID_HANDLE) {
#ifdef _WIN32
            CloseHandle(m_Handle);
#else
            close((int)(intptr_t)m_Handle);
#endif
            m_Handle = INVALID_HANDLE;
        }
        m_Connected = false;
    }

    std::vector<std::string> SerialPort::ScanPorts()
    {
        std::vector<std::string> availablePorts;

#ifdef _WIN32
        for (int i = 1; i <= 255; i++)
        {
            std::string portName = "COM" + std::to_string(i);
            std::string accessName = "\\\\.\\" + portName;

            HANDLE hPort = CreateFileA(accessName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,                
                nullptr,                
                OPEN_EXISTING,
                0,                
                nullptr);

            if (hPort != INVALID_HANDLE_VALUE)
            {
                availablePorts.push_back(portName);
                CloseHandle(hPort);
            }
        }
#else
        DIR* dir = opendir("/dev");
        if (dir)
        {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                std::string name = entry->d_name;

                if (name.find("ttyUSB") == 0 ||
                    name.find("ttyACM") == 0 ||
                    name.find("tty.usbserial") == 0 ||
                    name.find("tty.usbmodem") == 0)
                {
                    availablePorts.push_back("/dev/" + name);
                }
            }
            closedir(dir);
        }
#endif
        return availablePorts;
    }
}