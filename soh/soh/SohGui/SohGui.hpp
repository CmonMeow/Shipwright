//
//  SohGui.hpp
//  soh
//
//  Created by David Chavez on 24.08.22.
//

#ifndef SohGui_hpp
#define SohGui_hpp

#include <stdio.h>
#include "SohMenu.h"
#include "SohModals.h"

namespace SohGui {
void SetupHooks();
void SetupMenu();
void SetupMenuElements();
void Draw();
void Destroy();
void RegisterPopup(std::string title, std::string message, std::string button1 = "OK", std::string button2 = "",
                   std::function<void()> button1callback = nullptr, std::function<void()> button2callback = nullptr);
size_t PopupsQueued();
bool DismissPopup(std::string title);
void ShowEscMenu();
UIWidgets::Colors GetMenuThemeColor();
std::shared_ptr<SohMenu> GetSohMenu();
} // namespace SohGui

#define THEME_COLOR SohGui::GetMenuThemeColor()

#endif /* SohGui_hpp */
