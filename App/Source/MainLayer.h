#pragma once
#include "Core/Layer.h"
#include "Core/Renderer/Renderer2D.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <optional>
#include <map>
#include <unordered_map>
#include <json/json.hpp>
#include <flecs/flecs.h>

#ifdef _WIN32
#include <windows.h>
#else
typedef int HANDLE;
#endif

#include "Serial/SerialPort.h"

struct WellknownResource {
	std::string Uri;
	std::string Desc;
	std::string Methods;
};

struct ButtonConfig {
	std::string TargetNode;
	std::string TargetUrl;
	bool loaded = false;
};

struct NordicChip {
	std::string Id;
	std::string Ip;
	std::string Role;
	bool HasLed;
	bool HasButton;
	int LastSeen;
	Renderer::Texture2D* Texture;

	std::unordered_map<int, bool> LedStates;
	bool LedStatesLoaded = false;

	std::vector<WellknownResource> Resources;
	bool ResourcesLoaded = false;

	ButtonConfig BtnCfg;
};

enum class LedState : int {
	Off = 0,
	On = 1,
};

struct LedStatus
{
	std::unordered_map<int, LedState> Leds;
};

struct Button
{
	bool Configured;
	std::string Uri;
};


class MainLayer : public Core::Layer
{
public:
	MainLayer();
	~MainLayer();

	void OnUpdate(float ts) override;
	void OnRender() override;

private:
	void ScanAvailablePorts();
	bool OpenSerialPort(const std::string& portName, int baudRate);
	void CloseSerialPort();
	bool SendCommand(const std::string& command);

	void AttemptAutoConnect();

	void QueryNodeLedStatus(const std::string& nodeId);
	void ProcessReceivedLine(const std::string& line);
	bool IsJsonLine(const std::string& line);
	void ParseJsonResponse(const std::string& jsonStr);

	void HandleListNodes(const nlohmann::json& data);
	void HandleLedResponse(const nlohmann::json& data);
	void HandleCoApResponse(const nlohmann::json& data);

	void HandleButtonConfigResponse(const nlohmann::json& data);
	void HandleWellknownResponse(const nlohmann::json& data);

	void RenderConnectionPanel();
	void RenderCommandPanel();
	void RenderLogPanel();
public:
	static inline std::atomic<bool> s_Loaded = false;

#ifdef _WIN32
	HANDLE m_SerialHandle = INVALID_HANDLE_VALUE;
#else
	int m_SerialHandle = -1;
#endif
private:

	flecs::world m_World;
	flecs::entity m_SelectedNode;
	std::shared_ptr<Renderer::Texture2D> m_NordicImg;

	Serial::SerialPort m_Serial;

	bool m_AutoConnectEnabled = true;
	bool m_ConnectionStable = false;
	std::chrono::steady_clock::time_point m_LastConnectionAttempt;
	std::chrono::steady_clock::time_point m_LastDataReceived;

	bool m_Connected = false;
	std::string m_SelectedPort;
	int m_BaudRate = 115200;

	std::vector<std::string> m_AvailablePorts;

	char m_OTCommand[256] = "";
	std::vector<std::string> m_ResponseLog;
	std::mutex m_LogMutex;

	std::thread m_ReadThread;
	std::atomic<bool> m_Running{ false };

	std::mutex m_DataMutex;

	std::string m_JsonAccumulator;
	bool m_AccumulatingJson = false;
	int m_BraceCount = 0;
};