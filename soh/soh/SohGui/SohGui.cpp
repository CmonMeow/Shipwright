//
//  SohGui.cpp
//  soh
//
//  Created by David Chavez on 24.08.22.
//

#include "SohGui.hpp"

#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <libultraship/libultraship.h>

#ifdef __APPLE__
#include <fast/backends/gfx_metal.h>
#endif

#ifdef __SWITCH__
#include <port/switch/SwitchImpl.h>
#endif
#include "include/global.h"
#include "include/z64audio.h"
#include "soh/SaveManager.h"
#include "soh/OTRGlobals.h"
#include "soh/resource/type/Skeleton.h"

namespace SohGui {

// MARK: - Helpers

std::string GetWindowButtonText(const char* text, bool menuOpen) {
    char buttonText[100] = "";
    if (menuOpen) {
        strcat(buttonText, ICON_FA_CHEVRON_RIGHT " ");
    }
    strcat(buttonText, text);
    if (!menuOpen) {
        strcat(buttonText, "  ");
    }
    return buttonText;
}

// MARK: - Delegates

std::shared_ptr<SohMenu> mSohMenu;
std::shared_ptr<SohModalWindow> mModalWindow;

UIWidgets::Colors GetMenuThemeColor() {
    return mSohMenu->GetMenuThemeColor();
}

std::shared_ptr<SohMenu> GetSohMenu() {
    return mSohMenu;
}

void SetupMenu() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    mSohMenu = std::make_shared<SohMenu>(CVAR_WINDOW("Menu"), "Port Menu");
    gui->SetMenu(mSohMenu);

    mModalWindow = std::make_shared<SohModalWindow>(CVAR_WINDOW("ModalWindow"), "Modal Window");
    gui->AddGuiWindow(mModalWindow);
    mModalWindow->Show();
}

void SetupMenuElements() {
    mSohMenu->AddMenuElements();
}

void Destroy() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
    gui->RemoveAllGuiWindows();

    mModalWindow = nullptr;
}

void RegisterPopup(std::string title, std::string message, std::string button1, std::string button2,
                   std::function<void()> button1callback, std::function<void()> button2callback) {
    mModalWindow->RegisterPopup(title, message, button1, button2, button1callback, button2callback);
}

size_t PopupsQueued() {
    return mModalWindow->PopupsQueued();
}

bool DismissPopup(std::string title) {
    if (mModalWindow->IsPopupOpen(title)) {
        mModalWindow->DismissPopup();
        return true;
    }
    return false;
}

void ShowEscMenu() {
    mSohMenu->Show();
}
} // namespace SohGui
