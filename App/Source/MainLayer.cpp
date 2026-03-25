#include "MainLayer.h"

#include "Core/Application.h"
#include "Core/Renderer/RenderCommand.h"
#include "Core/Renderer/Renderer.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <implot.h>

#include <chrono>
#include <cstring>
#include <print>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include "Fonts/Fonts.h"
#include "Fonts/Icons.h"

using json = nlohmann::json;


MainLayer::MainLayer()
{
	Renderer::RenderCommand::Init();
	Renderer::Renderer2D::Init();
	Core::Application::Get().GetSpecification().UseDockspace = true;

	{
		auto& IO = ImGui::GetIO();
		ImFontConfig FontCfg{};
		FontCfg.FontDataOwnedByAtlas = false;
		static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		ImFontConfig iconsConfig{};
		iconsConfig.MergeMode = true;
		iconsConfig.PixelSnapH = true;
		iconsConfig.GlyphMinAdvanceX = Fonts::s_DefaultFontSize;
		iconsConfig.GlyphMaxAdvanceX = Fonts::s_DefaultFontSize;
		iconsConfig.OversampleH = 3;
		iconsConfig.OversampleV = 3;

		Fonts::s_DefaultFont = IO.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(Fonts::MainFont), sizeof(Fonts::MainFont), Fonts::s_DefaultFontSize, &FontCfg);
		IO.Fonts->AddFontFromMemoryCompressedTTF(Fonts::FontAwesome, sizeof(Fonts::FontAwesome), Fonts::s_DefaultFontSize, &iconsConfig, iconRanges);
	}

	m_NordicImg = Renderer::Texture2D::Create("assets/nordic.png");

	m_World.component<NordicChip>();

	std::string name = "Nordic_";

	m_Serial.SetDataCallback([&](const std::string& line) {
		ProcessReceivedLine(line);
	});


	m_AutoConnectEnabled = true;
	m_ConnectionStable = false;
	m_LastConnectionAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);
	m_LastDataReceived = std::chrono::steady_clock::now();


	ScanAvailablePorts();
}

MainLayer::~MainLayer()
{
	CloseSerialPort();
	m_Serial.Close();
}


void MainLayer::ScanAvailablePorts()
{
	m_AvailablePorts = Serial::SerialPort::ScanPorts();
}


bool MainLayer::OpenSerialPort(const std::string& portName, int baudRate)
{
	if (m_Connected)
	{
		CloseSerialPort();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	Serial::SerialConfig config;
	config.PortName = portName;
	config.BaudRate = baudRate;

	int retries = 3;
	for (int attempt = 0; attempt < retries; attempt++)
	{
		if (m_Serial.Open(config))
		{
			m_Connected = true;
			m_SelectedPort = portName;
			m_ConnectionStable = false;
			m_LastDataReceived = std::chrono::steady_clock::now();

			std::lock_guard<std::mutex> lock(m_LogMutex);
			m_ResponseLog.push_back("[Connected] Successfully connected to " + portName + " at " + std::to_string(baudRate) + " baud");

			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			SendCommand("br nodes");

			if (!s_Loaded)
				s_Loaded = true;

			return true;
		}

		if (attempt < retries - 1)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
	}

	std::lock_guard<std::mutex> lock(m_LogMutex);
	m_ResponseLog.push_back("[Error] Failed to connect to " + portName + " after " + std::to_string(retries) + " attempts");
	return false;
}

void MainLayer::CloseSerialPort()
{
	if (m_Connected)
	{
		m_Running = false;
		m_Serial.Close();
		m_Connected = false;
		m_ConnectionStable = false;

		std::lock_guard<std::mutex> lock(m_LogMutex);
		m_ResponseLog.push_back("[Disconnected] Closed connection to " + m_SelectedPort);
	}
}

void MainLayer::AttemptAutoConnect()
{
	if (!m_AutoConnectEnabled || m_Connected)
		return;

	auto now = std::chrono::steady_clock::now();
	auto timeSinceAttempt = std::chrono::duration_cast<std::chrono::seconds>(now - m_LastConnectionAttempt).count();

	if (timeSinceAttempt < 5)
		return;

	m_LastConnectionAttempt = now;

	ScanAvailablePorts();

	if (m_AvailablePorts.empty())
		return;

	if (!m_SelectedPort.empty())
	{
		for (const auto& port : m_AvailablePorts)
		{
			if (port == m_SelectedPort)
			{
				if (OpenSerialPort(port, m_BaudRate))
					return;
			}
		}
	}

	for (const auto& port : m_AvailablePorts)
	{
#ifdef _WIN32
		if (port.find("Bluetooth") != std::string::npos)
			continue;
#endif

		if (OpenSerialPort(port, m_BaudRate))
		{
			return;
		}
	}
}

bool MainLayer::SendCommand(const std::string& command)
{
	return m_Serial.Send(command);
}

void MainLayer::QueryNodeLedStatus(const std::string& nodeId)
{
	for (int led_id = 0; led_id < 4; led_id++)
	{
		std::string cmd = "br led status " + nodeId + " " + std::to_string(led_id);
		SendCommand(cmd);
	}
}

bool MainLayer::IsJsonLine(const std::string& line)
{
	size_t start = line.find_first_not_of(" \t");
	if (start != std::string::npos && line[start] == '{')
		return true;
	return false;
}

void MainLayer::ProcessReceivedLine(const std::string& line)
{
	{
		std::lock_guard<std::mutex> lock(m_LogMutex);
		m_ResponseLog.push_back(line);
		if (m_ResponseLog.size() > 1000)
			m_ResponseLog.erase(m_ResponseLog.begin());
	}

	if (m_AccumulatingJson)
	{
		m_JsonAccumulator += line;

		for (char c : line)
		{
			if (c == '{') m_BraceCount++;
			else if (c == '}') m_BraceCount--;
		}

		if (m_BraceCount == 0)
		{
			ParseJsonResponse(m_JsonAccumulator);
			m_JsonAccumulator.clear();
			m_AccumulatingJson = false;
		}
	}
	else if (IsJsonLine(line))
	{
		m_JsonAccumulator = line;
		m_BraceCount = 0;

		for (char c : line)
		{
			if (c == '{') m_BraceCount++;
			else if (c == '}') m_BraceCount--;
		}

		if (m_BraceCount == 0)
		{
			ParseJsonResponse(m_JsonAccumulator);
			m_JsonAccumulator.clear();
		}
		else
		{
			m_AccumulatingJson = true;
		}
	}
}

void MainLayer::ParseJsonResponse(const std::string& jsonStr)
{
	try
	{
		json j = json::parse(jsonStr);
		std::string type = j.value("type", "");

		if (type == "node_list")
		{
			HandleListNodes(j);
		}
		else if (type == "led_status")
		{
			HandleLedResponse(j);
		}
		else if (type == "wellknown")   HandleWellknownResponse(j);
		else if (type == "button_config") HandleButtonConfigResponse(j);
	}
	catch (const std::exception& e)
	{
		std::lock_guard<std::mutex> lock(m_LogMutex);
		m_ResponseLog.push_back("[JSON Error] " + std::string(e.what()));
	}
}

void MainLayer::HandleWellknownResponse(const nlohmann::json& data) {
	std::string nodeId = data.value("node", "");
	auto e = m_World.entity(nodeId.c_str());

	if (e.is_valid() && e.has<NordicChip>() && data.contains("response")) {
		auto& chip = e.get_mut<NordicChip>();
		chip.Resources.clear();
		auto& resp = data["response"];
		if (resp.contains("resources") && resp["resources"].is_array()) {
			for (const auto& r : resp["resources"]) {
				chip.Resources.push_back({
					r.value("uri", ""),
					r.value("desc", ""),
					r.value("methods", "")
					});
			}
		}
		chip.ResourcesLoaded = true;
	}
}

void MainLayer::HandleButtonConfigResponse(const nlohmann::json& data) {
	std::string nodeId = data.value("node", "");
	auto e = m_World.entity(nodeId.c_str());

	if (e.is_valid() && e.has<NordicChip>() && data.contains("response")) {
		auto& chip = e.get_mut<NordicChip>();
		auto& resp = data["response"];
		if (resp.contains("buttons") && resp["buttons"].is_array() && !resp["buttons"].empty()) {
			auto& first_btn = resp["buttons"][0];
			chip.BtnCfg.TargetNode = first_btn.value("target", "");
			chip.BtnCfg.TargetUrl = first_btn.value("uri", "");
			chip.BtnCfg.loaded = true;
		}
	}
}

void MainLayer::HandleListNodes(const nlohmann::json& data)
{
	std::lock_guard<std::mutex> lock(m_DataMutex);

	if (!data.contains("nodes") || !data["nodes"].is_array()) return;

	std::unordered_set<std::string> incomingIds;

	for (const auto& n : data["nodes"])
	{
		std::string id = n.value("id", "Unknown");
		incomingIds.insert(id);

		auto e = m_World.entity(id.c_str());

		e.set<NordicChip>({
			id,
			n.value("ip", "0.0.0.0"),
			n.value("role", "unknown"),
			n["caps"].value("has_led", false),
			n["caps"].value("has_button", false),
			n.value("last_seen_sec", 0),
			m_NordicImg.get(),
			{},
			false
			});
	}

	m_World.each<NordicChip>([&](flecs::entity e, NordicChip& chip)
		{
			if (!incomingIds.contains(chip.Id))
			{
				Core::Application::Get().QueueEvent([=] {
					e.destruct();
					});
			}
		});
}

void MainLayer::HandleLedResponse(const nlohmann::json& data)
{
	std::lock_guard<std::mutex> lock(m_DataMutex);

	std::string node = data.value("node", "");
	int led_id = data.value("led_id", 0);
	int state = data.value("state", 0);

	auto e = m_World.entity(node.c_str());
	if (e.is_valid() && e.has<NordicChip>())
	{
		auto& chip = e.get_mut<NordicChip>();
		chip.LedStates[led_id] = (state == 1);
		chip.LedStatesLoaded = true;
	}
}

void MainLayer::HandleCoApResponse(const nlohmann::json& data)
{
	std::lock_guard<std::mutex> lock(m_LogMutex);
	std::string dest = data.value("destination", "");
	std::string status = data.value("status", "");
	m_ResponseLog.push_back("[CoAP] Sent to " + dest + " - " + status);
}

void MainLayer::OnUpdate(float ts)
{
	AttemptAutoConnect();
}

void MainLayer::OnRender()
{
	Renderer::Renderer2D::ResetStats();
	Renderer::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.1f, 1.0f });
	Renderer::RenderCommand::Clear();

	ImGui::PushFont(Fonts::s_DefaultFont);

	ImGui::Begin("Nordic Network Dashboard");

	if (m_Connected) {
		if (ImGui::Button(ICON_FA_RECYCLE " Scan Network")) {
			SendCommand("br nodes");
		}
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), ICON_FA_EXCLAMATION " Disconnected");
	}

	ImGui::Separator();

	float cardWidth = 280.0f;
	float cardHeight = 320.0f;
	float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	ImGuiStyle& style = ImGui::GetStyle();

	m_World.each<NordicChip>([&](flecs::entity e, NordicChip& chip) {
		ImGui::PushID(chip.Id.c_str());

		ImGui::BeginGroup();

		ImVec2 cardStart = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImVec2 cardMin = cardStart;
		ImVec2 cardMax = ImVec2(cardStart.x + cardWidth, cardStart.y + cardHeight);
		bool isHovered = ImGui::IsMouseHoveringRect(cardMin, cardMax);
		bool isClicked = isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

		ImU32 bgColor = isHovered ? IM_COL32(55, 55, 65, 255) : IM_COL32(45, 45, 50, 255);
		ImU32 borderColor = isHovered ? IM_COL32(120, 120, 140, 255) : IM_COL32(100, 100, 110, 255);
		float borderThickness = isHovered ? 2.0f : 1.0f;

		drawList->AddRectFilled(cardStart, cardMax, bgColor, 8.0f);
		drawList->AddRect(cardStart, cardMax, borderColor, 8.0f, 0, borderThickness);

		if (isHovered) {
			drawList->AddRect(cardStart, cardMax, IM_COL32(140, 140, 160, 100), 8.0f, 0, 3.0f);
		}

		if (isClicked) {
			m_SelectedNode = e;
			//if (chip.HasLed && !chip.LedStatesLoaded) {
			//	QueryNodeLedStatus(chip.Id);
			//}
		}

		ImGui::SetCursorScreenPos(ImVec2(cardStart.x + cardWidth - 35, cardStart.y + 5));
		if (ImGui::Button(ICON_FA_GEAR, ImVec2(30, 30))) {
			ImGui::OpenPopup("NodeSettingsPopup");
		}

		float imgSize = isHovered ? 200.0f : 192.0f;
		ImGui::SetCursorScreenPos(ImVec2(cardStart.x + (cardWidth - imgSize) * 0.5f, cardStart.y + 35 + (192.0f - imgSize) * 0.5f));
		if (chip.Texture && chip.Texture->IsLoaded()) {
			ImGui::Image((ImTextureID)(uintptr_t)chip.Texture->GetRendererID(), ImVec2(imgSize, imgSize));
		}
		else {
			ImGui::Dummy(ImVec2(imgSize, imgSize));
		}

		float textY = cardStart.y + 35 + 192.0f + 10;
		float textWidth = ImGui::CalcTextSize(chip.Id.c_str()).x;
		ImGui::SetCursorScreenPos(ImVec2(cardStart.x + (cardWidth - textWidth) * 0.5f, textY));
		ImVec4 textColor = isHovered ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
		ImGui::TextColored(textColor, "%s", chip.Id.c_str());

		const char* statusIcon = (chip.Role == "leader") ? ICON_FA_CROWN : ICON_FA_MICROCHIP;
		float iconWidth = ImGui::CalcTextSize(statusIcon).x;
		ImGui::SetCursorScreenPos(ImVec2(cardStart.x + (cardWidth - iconWidth) * 0.5f, textY + 20));
		ImVec4 iconColor = isHovered ? ImVec4(0.5f, 0.8f, 1.0f, 1.0f) : ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
		ImGui::TextColored(iconColor, statusIcon);

		if (isHovered) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
		}

		if (ImGui::BeginPopup("NodeSettingsPopup")) {
			ImGui::Text(ICON_FA_SLIDERS " Configure: %s", chip.Id.c_str());
			ImGui::Separator();
			ImGui::Text("IP: %s", chip.Ip.c_str());
			if (chip.HasLed) {
				if (ImGui::MenuItem(ICON_FA_LIGHTBULB " Toggle LED")) {
					SendCommand("br led toggle " + chip.Id);
				}
				if (ImGui::MenuItem(ICON_FA_RECYCLE " Refresh LED Status")) {
					QueryNodeLedStatus(chip.Id);
				}
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("NodeDetailsPopup")) {
			ImGui::Text(ICON_FA_MICROCHIP " Node Details: %s", chip.Id.c_str());
			ImGui::Separator();
			ImGui::Text("ID: %s", chip.Id.c_str());
			ImGui::Text("IP: %s", chip.Ip.c_str());
			ImGui::Text("Role: %s", chip.Role.c_str());
			ImGui::Text("Has LED: %s", chip.HasLed ? "Yes" : "No");
			ImGui::Text("Has Button: %s", chip.HasButton ? "Yes" : "No");
			ImGui::Text("Last Seen: %d sec ago", chip.LastSeen);
			ImGui::Separator();
			if (ImGui::Button("Close")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::SetCursorScreenPos(cardStart);
		ImGui::Dummy(ImVec2(cardWidth, cardHeight));

		ImGui::EndGroup();

		float lastCardX2 = ImGui::GetItemRectMax().x;
		float nextCardX2 = lastCardX2 + style.ItemSpacing.x + cardWidth;
		if (nextCardX2 < windowVisibleX2)
			ImGui::SameLine();

		ImGui::PopID();
		});

	ImGui::End();

	if (m_SelectedNode.is_valid()) {
		ImGui::Begin("Chip Details", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		const NordicChip& chip = m_SelectedNode.get<NordicChip>();

		ImGui::Text(ICON_FA_MICROCHIP " %s", chip.Id.c_str());
		ImGui::Separator();

		ImGui::Text("IP: %s", chip.Ip.c_str());
		ImGui::Text("Role: %s", chip.Role.c_str());
		ImGui::Text("Last Seen: %d sec ago", chip.LastSeen);

		ImGui::Separator();

		if (chip.HasLed) {
			ImGui::Text(ICON_FA_LIGHTBULB " LED Control:");

			if (!chip.LedStatesLoaded) {
				ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Loading...");
				ImGui::SameLine();
				if (ImGui::SmallButton(ICON_FA_RECYCLE " Refresh##2")) {
					QueryNodeLedStatus(chip.Id);
				}
			}
			else {
				for (const auto& [led_id, is_on] : chip.LedStates) {
					ImVec4 color = is_on ? ImVec4(1, 1, 0, 1) : ImVec4(0.3f, 0.3f, 0.3f, 1);
					const char* status = is_on ? "ON " : "OFF";

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					ImVec2 pos = ImGui::GetCursorScreenPos();
					float radius = 7.0f;
					ImU32 circleColor = is_on ? IM_COL32(255, 255, 0, 255) : IM_COL32(80, 80, 80, 255);
					drawList->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius + 2), radius, circleColor);
					ImGui::Dummy(ImVec2(radius * 2 + 4, radius * 2));

					ImGui::SameLine();
					ImGui::TextColored(color, "LED %d: %s", led_id, status);

					ImGui::SameLine();
					std::string toggleBtnId = ICON_FA_POWER_OFF " Toggle##" + std::to_string(led_id);
					if (ImGui::SmallButton(toggleBtnId.c_str())) {
						std::string cmd = "ot_coap send " + chip.Id + " /led/" + std::to_string(led_id) + "/toggle";
						SendCommand(cmd);
					}
				}

				if (ImGui::SmallButton(ICON_FA_RECYCLE " Refresh All")) {
					QueryNodeLedStatus(chip.Id);
				}
			}
		}

		ImGui::Separator();

		ImGui::Separator();
		ImGui::Text(ICON_FA_LIST " CoAP Resources");
		if (ImGui::SmallButton(ICON_FA_RECYCLE " Query Resources")) {
			SendCommand("br wellknown " + chip.Id);
		}

		if (chip.ResourcesLoaded) {
			if (ImGui::BeginTable("ResourcesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 150))) {
				ImGui::TableSetupColumn("URI");
				ImGui::TableSetupColumn("Desc");
				ImGui::TableSetupColumn("Methods");
				ImGui::TableHeadersRow();
				for (auto& res : chip.Resources) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextUnformatted(res.Uri.c_str());
					ImGui::TableNextColumn(); ImGui::TextUnformatted(res.Desc.c_str());
					ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(0, 1, 0, 1), res.Methods.c_str());
				}
				ImGui::EndTable();
			}
		}

		if (chip.HasButton) {
			ImGui::Separator();
			ImGui::Text(ICON_FA_GEAR " Button Configuration");

			static char targetNode[64] = "";
			static char targetUri[128] = "";

			if (chip.BtnCfg.loaded) {
				ImGui::Text("TargetNode");
				ImGui::Text(chip.BtnCfg.TargetNode.c_str());
				ImGui::Text("TargetUrl");
				ImGui::Text(chip.BtnCfg.TargetUrl.c_str());
			}

			ImGui::InputText("Target Node", targetNode, sizeof(targetNode));
			ImGui::InputText("Target URI", targetUri, sizeof(targetUri));

			if (ImGui::Button(ICON_FA_SHARE " Apply Configuration")) {
				std::string cmd = "br configure " + chip.Id + " 0 " + targetNode + " " + targetUri;
				SendCommand(cmd);
			}

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_RECYCLE " Refresh")) {
				SendCommand("br button config " + chip.Id);
			}

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH " Clear")) {
				SendCommand("br clear " + chip.Id + " 0");
				targetNode[0] = '\0';
				targetUri[0] = '\0';
			}
		}

		ImGui::Separator();

		if (ImGui::Button("Close")) {
			m_SelectedNode = flecs::entity();
		}

		ImGui::End();
	}

	RenderConnectionPanel();
	RenderCommandPanel();
	RenderLogPanel();
	ImGui::PopFont();
}


void MainLayer::RenderConnectionPanel()
{
	ImGui::Begin("Connection");

	if (ImGui::Button("Refresh Ports"))
		ScanAvailablePorts();

	ImGui::SameLine();
	ImGui::Text("Ports: %zu", m_AvailablePorts.size());

	static int selectedPortIndex = 0;
	if (!m_AvailablePorts.empty())
	{
		if (ImGui::BeginCombo("Port", m_AvailablePorts[selectedPortIndex].c_str()))
		{
			for (int i = 0; i < (int)m_AvailablePorts.size(); i++)
			{
				bool isSelected = (selectedPortIndex == i);
				if (ImGui::Selectable(m_AvailablePorts[i].c_str(), isSelected))
					selectedPortIndex = i;
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	static const int baudRates[] = { 9600, 19200, 38400, 57600, 115200 };
	static int baudIndex = 4;
	if (ImGui::BeginCombo("Baud", std::to_string(baudRates[baudIndex]).c_str()))
	{
		for (int i = 0; i < 5; i++)
		{
			if (ImGui::Selectable(std::to_string(baudRates[i]).c_str(), baudIndex == i))
			{
				baudIndex = i;
				m_BaudRate = baudRates[i];
			}
		}
		ImGui::EndCombo();
	}

	if (!m_Connected)
	{
		if (ImGui::Button("Connect") && !m_AvailablePorts.empty())
			OpenSerialPort(m_AvailablePorts[selectedPortIndex], m_BaudRate);
	}
	else
	{
		if (ImGui::Button("Disconnect"))
			CloseSerialPort();
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
	}

	ImGui::End();
}

void MainLayer::RenderCommandPanel()
{
	ImGui::Begin("Commands");
	ImGui::Text("Custom Command:");

	ImGui::PushItemWidth(-80);
	bool enter = ImGui::InputText("##cmd", m_OTCommand, sizeof(m_OTCommand),
		ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::PopItemWidth();

	ImGui::SameLine();
	if ((ImGui::Button("Send") || enter) && m_Connected && strlen(m_OTCommand) > 0)
	{
		SendCommand(m_OTCommand);
		m_OTCommand[0] = '\0';
	}

	ImGui::End();
}

static ImVec4 AnsiColorToImGui(int code)
{
	switch (code)
	{
	case 30: return ImVec4(0, 0, 0, 1);
	case 31: return ImVec4(1, 0.3f, 0.3f, 1);
	case 32: return ImVec4(0.4f, 1, 0.4f, 1);
	case 33: return ImVec4(1, 1, 0.4f, 1);
	case 34: return ImVec4(0.4f, 0.6f, 1, 1);
	case 35: return ImVec4(1, 0.4f, 1, 1);
	case 36: return ImVec4(0.4f, 1, 1, 1);
	case 37: return ImVec4(1, 1, 1, 1);

	case 90: return ImVec4(0.5f, 0.5f, 0.5f, 1);
	case 91: return ImVec4(1, 0.5f, 0.5f, 1);
	case 92: return ImVec4(0.5f, 1, 0.5f, 1);
	case 93: return ImVec4(1, 1, 0.5f, 1);
	case 94: return ImVec4(0.5f, 0.7f, 1, 1);
	case 95: return ImVec4(1, 0.5f, 1, 1);
	case 96: return ImVec4(0.5f, 1, 1, 1);
	case 97: return ImVec4(1, 1, 1, 1);
	}
	return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

static void ImGuiTextAnsi(const std::string& text)
{
	ImVec4 currentColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
	size_t i = 0;

	while (i < text.size())
	{
		if (text[i] == '\x1B' && i + 1 < text.size() && text[i + 1] == '[')
		{
			i += 2;
			int lastColorCode = -1;

			while (i < text.size() && text[i] != 'm')
			{
				if (isdigit(text[i]))
				{
					int code = 0;
					while (i < text.size() && isdigit(text[i]))
					{
						code = code * 10 + (text[i] - '0');
						i++;
					}
					if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
						lastColorCode = code;

					if (code == 0)
						currentColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				}
				else
				{
					i++;
				}
			}

			if (i < text.size() && text[i] == 'm')
			{
				if (lastColorCode != -1)
					currentColor = AnsiColorToImGui(lastColorCode);
				i++;
			}
		}
		else
		{
			size_t start = i;
			while (i < text.size() && text[i] != '\x1B')
				i++;

			ImGui::SameLine(0, 0);
			ImGui::TextColored(
				currentColor,
				"%.*s",
				int(i - start),
				text.data() + start
			);
		}
	}

	ImGui::NewLine();
}


void MainLayer::RenderLogPanel()
{
	ImGui::Begin("Log");

	if (ImGui::Button("Clear"))
	{
		std::lock_guard<std::mutex> lock(m_LogMutex);
		m_ResponseLog.clear();
	}

	ImGui::SameLine();
	static bool autoScroll = true;
	ImGui::Checkbox("Auto-scroll", &autoScroll);

	ImGui::Separator();
	ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	{
		std::lock_guard<std::mutex> lock(m_LogMutex);
		for (const auto& line : m_ResponseLog)
		{
			ImGuiTextAnsi(line);
		}

	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
	ImGui::End();
}