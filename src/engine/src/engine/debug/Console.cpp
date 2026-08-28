#include <runtime/log/Log.hpp>
#include "engine/debug/Console.h"
#include "engine/utils/StringHelper.h"
#include "engine/Context.h"
namespace Engine {
Console::Console() {
}

Console::~Console() {
    WriteLog("destruct console");
}

void Console::Init() {
}

std::string Console::BuildUsage(const CommandEntry& entry) {
    std::string usage;
    for (const auto& arg : entry.Arguments) {
        usage += StringHelper::Sprintf(arg.Optional ? "[%s] " : "<%s> ", arg.Info.c_str());
    }
    return usage;
}

std::string Console::BuildUsage(const std::string& command) {
    return BuildUsage(GetCommand(command));
}

int32_t Console::Run(const std::string& command, std::string* output) {
    const std::vector<std::string> cmdArgs = StringHelper::Split(command, " ");
    if (cmdArgs.empty()) {
        WriteLog("Could not parse command: {}", command);
        return false;
    }

    const std::string& commandName = cmdArgs[0];
    if (!mCommands.contains(commandName)) {
        WriteLog("Command handler not found: {}", commandName);
        return false;
    }

    const CommandEntry entry = mCommands[commandName];
    int32_t commandResult = entry.Handler(Context::GetInstance()->GetConsole(), cmdArgs, output);
    if (output) {
        WriteLog("Command \"{}\" returned {} with output: {}", command, commandResult, *output);
    } else {
        WriteLog("Command \"{}\" returned {}", command, commandResult);
    }
    return commandResult;
}

bool Console::HasCommand(const std::string& command) {
    for (const auto& value : mCommands) {
        if (value.first == command) {
            return true;
        }
    }

    return false;
}

void Console::AddCommand(const std::string& command, CommandEntry entry) {
    if (!HasCommand(command)) {
        mCommands[command] = entry;
    } else {
        WriteLog("Attempting to add command {} that already exists", command);
    }
}

std::map<std::string, CommandEntry>& Console::GetCommands() {
    return mCommands;
}

CommandEntry& Console::GetCommand(const std::string& command) {
    return mCommands[command];
}
} // namespace Engine
