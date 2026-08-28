#pragma once

#include <engine/resource/Resource.h>
#include <engine/resource/ResourceFactoryBinary.h>
#include <engine/resource/ResourceFactoryXML.h>
#include "port/resource/type/AudioSoundFont.h"

namespace SOH {
class ResourceFactoryBinaryAudioSoundFontV2 final : public Engine::ResourceFactoryBinary {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
};

class ResourceFactoryXMLSoundFontV0 final : public Engine::ResourceFactoryXML {
  public:
    std::shared_ptr<Engine::IResource> ReadResource(std::shared_ptr<Engine::File> file,
                                                  std::shared_ptr<Engine::ResourceInitData> initData) override;
    static int8_t MediumStrToInt(const char* str, const char* file);
    static int8_t CachePolicyToInt(const char* str, const char* file);

  private:
    void ParseDrums(AudioSoundFont* soundFont, tinyxml2::XMLElement* element);
    void ParseInstruments(AudioSoundFont* soundFont, tinyxml2::XMLElement* element);
    void ParseSfxTable(AudioSoundFont* soundFont, tinyxml2::XMLElement* element);
    std::vector<AdsrEnvelope> ParseEnvelopes(AudioSoundFont* soundFont, tinyxml2::XMLElement* element,
                                             unsigned int* count);
};

} // namespace SOH
