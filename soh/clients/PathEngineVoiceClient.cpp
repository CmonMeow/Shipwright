#include "Network/ShipwrightNetworkRuntime.h"
#include "Network/VoiceChat.h"
#include "libultraship/log/PathEngineLog.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace {

constexpr int CHAT_CLIENT_WIDTH = 704;
constexpr int CHAT_CLIENT_HEIGHT = 420;
constexpr UINT_PTR NETWORK_TIMER = 1;

enum ControlId {
    IDC_ADDRESS = 100,
    IDC_CONNECT,
    IDC_HOST,
    IDC_HISTORY,
    IDC_INPUT,
    IDC_SEND,
    IDC_VOICE,
    IDC_STATUS,
};

struct ChatClient {
    HWND window = nullptr;
    HWND address = nullptr;
    HWND connect = nullptr;
    HWND host = nullptr;
    HWND history = nullptr;
    HWND input = nullptr;
    HWND send = nullptr;
    HWND voice = nullptr;
    HWND status = nullptr;
    HFONT font = nullptr;
    bool quit = false;
    std::string lastStatus;
    SoH::Network::ShipwrightNetworkRuntime network;
    std::unique_ptr<cVoiceChat> voiceChat;
};

ChatClient* gClient = nullptr;

std::string WindowText(HWND window) {
    const int length = GetWindowTextLengthA(window);
    std::string text(static_cast<size_t>(std::max(length, 0)), '\0');
    if (length > 0) {
        GetWindowTextA(window, text.data(), length + 1);
    }
    return text;
}

void AppendHistory(ChatClient& client, const std::string& line) {
    if (line.empty()) {
        return;
    }
    const int length = GetWindowTextLengthA(client.history);
    SendMessageA(client.history, EM_SETSEL, length, length);
    if (length > 0) {
        SendMessageA(client.history, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>("\r\n"));
    }
    SendMessageA(client.history, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageA(client.history, EM_SCROLLCARET, 0, 0);
}

void SetStatus(ChatClient& client) {
    std::string status;
    if (!client.network.IsActive()) {
        status = "Disconnected";
    } else if (!client.network.IsSecure()) {
        status = client.network.IsHost() ? "Hosting; waiting for encrypted peers" : "Establishing encrypted session";
    } else if (client.network.IsHost()) {
        status = "Hosting securely";
    } else {
        status = "Connected securely | " + std::to_string(client.network.LatencyMilliseconds()) + " ms | " +
                 std::to_string(client.network.ThroughputBytesPerSecond()) + " B/s";
    }
    if (status != client.lastStatus) {
        client.lastStatus = status;
        SetWindowTextA(client.status, status.c_str());
    }
}

void Disconnect(ChatClient& client) {
    if (client.voiceChat) {
        client.voiceChat->update(client.network, false, false, true);
    }
    client.network.Disconnect();
    SetWindowTextA(client.connect, "Connect");
    SetWindowTextA(client.host, "Host");
    EnableWindow(client.connect, TRUE);
    EnableWindow(client.host, TRUE);
    SetStatus(client);
}

void Connect(ChatClient& client) {
    if (client.network.IsActive()) {
        Disconnect(client);
        return;
    }
    const std::string address = WindowText(client.address);
    if (address.empty()) {
        AppendHistory(client, "system: enter a server address");
        return;
    }
    EnableWindow(client.connect, FALSE);
    EnableWindow(client.host, FALSE);
    SetWindowTextA(client.status, "Connecting...");
    if (!client.network.Connect(address)) {
        AppendHistory(client, "system: connection failed");
        EnableWindow(client.connect, TRUE);
        EnableWindow(client.host, TRUE);
        SetStatus(client);
        return;
    }
    SetWindowTextA(client.connect, "Disconnect");
    EnableWindow(client.connect, TRUE);
    SetFocus(client.input);
}

void Host(ChatClient& client) {
    if (client.network.IsActive()) {
        Disconnect(client);
        return;
    }
    const std::string value = WindowText(client.address);
    const size_t colon = value.find_last_of(':');
    const std::string portText = colon == std::string::npos ? value : value.substr(colon + 1);
    char* end = nullptr;
    const unsigned long port = std::strtoul(portText.c_str(), &end, 10);
    const uint16_t selectedPort = end != portText.c_str() && *end == '\0' && port > 0 && port <= 49151
                                      ? static_cast<uint16_t>(port)
                                      : DEFAULT_NETWORK_PORT;
    EnableWindow(client.connect, FALSE);
    EnableWindow(client.host, FALSE);
    if (!client.network.Host(selectedPort, "Ship of Harkinian")) {
        AppendHistory(client, "system: unable to host on port " + std::to_string(selectedPort));
        EnableWindow(client.connect, TRUE);
        EnableWindow(client.host, TRUE);
        SetStatus(client);
        return;
    }
    SetWindowTextA(client.host, "Stop");
    EnableWindow(client.host, TRUE);
    SetFocus(client.input);
}

void Send(ChatClient& client) {
    const std::string text = WindowText(client.input);
    if (text.empty()) {
        return;
    }
    bool sent = false;
    if (text.rfind("/w ", 0) == 0) {
        const size_t idEnd = text.find(' ', 3);
        if (idEnd != std::string::npos) {
            char* end = nullptr;
            const long target = std::strtol(text.substr(3, idEnd - 3).c_str(), &end, 10);
            if (end && *end == '\0') {
                sent = client.network.SendPrivateChat(static_cast<int32_t>(target), text.substr(idEnd + 1));
            }
        }
    } else {
        sent = client.network.SendChat(text);
    }
    if (sent) {
        SetWindowTextA(client.input, "");
    } else {
        AppendHistory(client, "system: message was not sent");
    }
}

void Update(ChatClient& client) {
    client.network.Update();
    NetworkChatLine line;
    while (client.network.PollChat(line)) {
        AppendHistory(client, line.text);
    }

    const bool voiceEnabled = SendMessageA(client.voice, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool inputActive = GetFocus() == client.input;
    const bool talkKeyDown = !inputActive && (GetAsyncKeyState('V') & 0x8000) != 0;
    if (client.voiceChat) {
        client.voiceChat->update(client.network, talkKeyDown, voiceEnabled, true);
    }

    if (!client.network.IsActive()) {
        SetWindowTextA(client.connect, "Connect");
        SetWindowTextA(client.host, "Host");
        EnableWindow(client.connect, TRUE);
        EnableWindow(client.host, TRUE);
    }
    SetStatus(client);
}

void Layout(ChatClient& client, int width, int height) {
    constexpr int margin = 10;
    constexpr int row = 26;
    constexpr int gap = 6;
    constexpr int button = 86;
    const int addressWidth = std::max(120, width - margin * 2 - button * 2 - gap * 2);
    MoveWindow(client.address, margin, margin, addressWidth, row, TRUE);
    MoveWindow(client.connect, margin + addressWidth + gap, margin, button, row, TRUE);
    MoveWindow(client.host, margin + addressWidth + gap + button + gap, margin, button, row, TRUE);

    const int statusY = margin + row + gap;
    MoveWindow(client.status, margin, statusY + 4, width - margin * 2 - 170, row, TRUE);
    MoveWindow(client.voice, width - margin - 164, statusY, 164, row, TRUE);

    const int historyY = statusY + row + gap;
    const int inputY = height - margin - row;
    MoveWindow(client.history, margin, historyY, width - margin * 2, std::max(40, inputY - gap - historyY), TRUE);
    MoveWindow(client.input, margin, inputY, width - margin * 2 - button - gap, row, TRUE);
    MoveWindow(client.send, width - margin - button, inputY, button, row, TRUE);
}

void CreateControls(ChatClient& client, HWND window) {
    client.address = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "127.0.0.1:777",
                                     WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, window,
                                     reinterpret_cast<HMENU>(IDC_ADDRESS), nullptr, nullptr);
    client.connect = CreateWindowExA(0, "BUTTON", "Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                                     window, reinterpret_cast<HMENU>(IDC_CONNECT), nullptr, nullptr);
    client.host = CreateWindowExA(0, "BUTTON", "Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, window,
                                  reinterpret_cast<HMENU>(IDC_HOST), nullptr, nullptr);
    client.history = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                     0, 0, 0, 0, window, reinterpret_cast<HMENU>(IDC_HISTORY), nullptr, nullptr);
    client.input = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0,
                                   0, window, reinterpret_cast<HMENU>(IDC_INPUT), nullptr, nullptr);
    client.send = CreateWindowExA(0, "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, window,
                                  reinterpret_cast<HMENU>(IDC_SEND), nullptr, nullptr);
    client.voice = CreateWindowExA(0, "BUTTON", "Voice (hold V)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0,
                                   0, window, reinterpret_cast<HMENU>(IDC_VOICE), nullptr, nullptr);
    client.status = CreateWindowExA(0, "STATIC", "Disconnected", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, window,
                                    reinterpret_cast<HMENU>(IDC_STATUS), nullptr, nullptr);
    SendMessageA(client.input, EM_SETLIMITTEXT, CHAT_MAX_MESSAGE_CHARS, 0);
    SendMessageA(client.voice, BM_SETCHECK, BST_CHECKED, 0);

    client.font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HWND controls[] = { client.address, client.connect, client.host, client.history,
                              client.input,   client.send,    client.voice, client.status };
    for (HWND control : controls) {
        SendMessageA(control, WM_SETFONT, reinterpret_cast<WPARAM>(client.font), TRUE);
    }
    AppendHistory(client, "Enter sends chat. Use /w playerId message for E2E private chat. Hold V outside the message box to talk.");
}

LRESULT CALLBACK ChatWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    ChatClient* client = gClient;
    switch (message) {
        case WM_CREATE:
            if (client) {
                client->window = window;
                CreateControls(*client, window);
                SetTimer(window, NETWORK_TIMER, 10, nullptr);
            }
            return 0;
        case WM_CLOSE:
        case WM_DESTROY:
            if (client) {
                client->quit = true;
            }
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
            if (!client) {
                break;
            }
            if (LOWORD(wParam) == IDC_CONNECT && HIWORD(wParam) == BN_CLICKED) {
                Connect(*client);
                return 0;
            }
            if (LOWORD(wParam) == IDC_HOST && HIWORD(wParam) == BN_CLICKED) {
                Host(*client);
                return 0;
            }
            if (LOWORD(wParam) == IDC_SEND && HIWORD(wParam) == BN_CLICKED) {
                Send(*client);
                return 0;
            }
            break;
        case WM_TIMER:
            if (client && wParam == NETWORK_TIMER) {
                Update(*client);
                return 0;
            }
            break;
        case WM_SIZE:
            if (client && client->history) {
                Layout(*client, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 520;
            info->ptMinTrackSize.y = 280;
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

bool InputWantsEnter(HWND input, MSG& message) {
    return message.hwnd == input && message.message == WM_KEYDOWN && message.wParam == VK_RETURN;
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int showCommand) {
    ClearLog();
    Error("PathEngine voice client starting");
    const bool lifecycleTest = commandLine && std::strcmp(commandLine, "--lifecycle-test") == 0;

    ChatClient client;
    Error("PathEngine voice client runtime initialized");
    gClient = &client;

    WNDCLASSEXA windowClass;
    std::memset(&windowClass, 0, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC | CS_DBLCLKS;
    windowClass.lpfnWndProc = ChatWndProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = "PathEngineChatClient";
    windowClass.hIconSm = windowClass.hIcon;

    if (!RegisterClassExA(&windowClass)) {
        Error("Failed to create chat window class: GetLastError() %lu", GetLastError());
        return EXIT_FAILURE;
    }
    Error("PathEngine voice client window class registered");

    RECT rect = { 0, 0, CHAT_CLIENT_WIDTH, CHAT_CLIENT_HEIGHT };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
    AdjustWindowRectEx(&rect, style, FALSE, 0);
    HWND window = CreateWindowExA(0, windowClass.lpszClassName, "PathEngine Voice and Text", style, CW_USEDEFAULT,
                                  CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                                  instance, nullptr);
    if (!window) {
        Error("Failed to create chat window: GetLastError() %lu", GetLastError());
        UnregisterClassA(windowClass.lpszClassName, instance);
        return EXIT_FAILURE;
    }
    Error("PathEngine voice client window created");

    ShowWindow(window, showCommand == 0 ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(window);
    client.voiceChat = std::make_unique<cVoiceChat>();
    Error("PathEngine voice client audio initialized");
    if (commandLine && *commandLine && !lifecycleTest) {
        SetWindowTextA(client.address, commandLine);
    }
    if (lifecycleTest) {
        PostMessageA(window, WM_CLOSE, 0, 0);
    }

    MSG message;
    while (!client.quit && GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (InputWantsEnter(client.input, message)) {
            Send(client);
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    KillTimer(window, NETWORK_TIMER);
    Disconnect(client);
    client.voiceChat.reset();
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    UnregisterClassA(windowClass.lpszClassName, instance);
    gClient = nullptr;
    Error("PathEngine voice client stopped");
    return EXIT_SUCCESS;
}
