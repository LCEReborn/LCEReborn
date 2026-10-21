#ifndef GAME_CLIENT_RENDER_SHADERPACKBRIDGE_HPP
#define GAME_CLIENT_RENDER_SHADERPACKBRIDGE_HPP

#include "platform/Platform.hpp"

#if LCE_PLATFORM_PC

#include "platform/pc/shaderpack/ShaderUniforms.hpp"

class BlockEntity;
class Entity;
class GameRenderer;
class LevelRenderer;

namespace ShaderPackBridge
{
bool BeginWorld(GameRenderer* gameRenderer, float partialTick);
void EndWorld();
bool Active();
bool InShadow();
bool SkipVanillaShadows();
int CloudMode();
bool RenderSkyDisc();
bool RenderSun();
bool RenderMoon();
bool UnderwaterOverlay();
bool Vignette();
bool WeatherDepth();
bool BeaconBeamDepth();
float HandDepth();
void SetStage(int stage);
void BeginTranslucent();
void BeginHand();
void EndHand();
void RenderShadows(LevelRenderer* levelRenderer, float partialTick);
void SetEntity(Entity* entity);
void SetBlockEntity(BlockEntity* blockEntity);
void ClearEntity();
}

#endif

#endif
