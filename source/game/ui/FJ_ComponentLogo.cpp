#include "game/ui/FJ_ComponentLogo.hpp"

#include <string>

namespace
{
std::string gLogoLanguageLabels[3] = {"Default", "Chinese", "Japanese"};
}

FJ_ComponentLogo::FJ_ComponentLogo(fuiRenderNode* renderNode) : FJ_Document(renderNode), m_logoLanguage(-1)
{
    SetLogoLanguage(0);
}

void FJ_ComponentLogo::SetLogoLanguage(int language)
{
    if (m_logoLanguage != language)
    {
        getChildByName(std::string("LogoObject"))->getChildAt(0)->gotoAndStop(gLogoLanguageLabels[language]);
        m_logoLanguage = language;
    }
}

FJ_ComponentLogo* FJ_ComponentLogo::Create(fuiRenderNode* renderNode)
{
    FJ_ComponentLogo* logo = new FJ_ComponentLogo(renderNode);
    return logo;
}

unsigned int FJ_ComponentLogo::getType()
{
    return 5;
}
