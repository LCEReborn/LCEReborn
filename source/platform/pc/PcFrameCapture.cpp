#include "platform/pc/PcFrameCapture.hpp"

#if LCE_DEBUG_BUILD

#include <png.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "game/core/Minecraft.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"
#include "platform/pc/GlApi.hpp"
#include "platform/pc/Log.hpp"
#include "platform/pc/PcApplication.hpp"
#include "platform/pc/PcLaunchOptions.hpp"
#include "platform/pc/PcStartWorld.hpp"

namespace
{
enum CaptureState
{
    CaptureState_WaitForWorld,
    CaptureState_Settling,
    CaptureState_Capturing,
    CaptureState_Done
};

const Logger sLog("FrameCapture");
CaptureState sState = CaptureState_WaitForWorld;
double sSettleStart = 0.0;
int sCaptured = 0;

double NowSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Finish()
{
    PcRequestExit();
    sState = CaptureState_Done;
}

void PlaceCamera(const PcLaunchOptions& options)
{
    if (options.position.empty() && options.rotation.empty())
        return;
    Minecraft* minecraft = Minecraft::GetInstance();
    double x = options.position.empty() ? minecraft->mPlayer->mX : options.position[0];
    double y = options.position.empty() ? minecraft->mPlayer->mY : options.position[1];
    double z = options.position.empty() ? minecraft->mPlayer->mZ : options.position[2];
    float yaw = options.rotation.empty() ? minecraft->mPlayer->mYRot : static_cast<float>(options.rotation[0]);
    float pitch = options.rotation.empty() ? minecraft->mPlayer->mXRot : static_cast<float>(options.rotation[1]);
    minecraft->mPlayer->moveTo(x, y, z, yaw, pitch);
    LOG_INFO(sLog, "Moved to %.2f,%.2f,%.2f facing %.1f,%.1f", x, y, z, yaw, pitch);
}

bool WritePng(const std::filesystem::path& path, int width, int height, const std::vector<unsigned char>& rgba)
{
    png_image image;
    std::memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = static_cast<png_uint_32>(width);
    image.height = static_cast<png_uint_32>(height);
    image.format = PNG_FORMAT_RGBA;
    return png_image_write_to_file(&image, path.string().c_str(), 0, &rgba[0], -width * 4, NULL) != 0;
}
}

void PcFrameCaptureTick()
{
    const PcLaunchOptions& options = PcGetLaunchOptions();
    if (options.captureFrames <= 0 && options.position.empty() && options.rotation.empty())
        return;

    switch (sState)
    {
    case CaptureState_WaitForWorld:
        if (!PcInWorld())
            break;
        PlaceCamera(options);
        if (options.captureFrames <= 0)
        {
            sState = CaptureState_Done;
            break;
        }
        sSettleStart = NowSeconds();
        sState = CaptureState_Settling;
        break;
    case CaptureState_Settling:
        if (NowSeconds() - sSettleStart < options.captureDelay)
            break;
        {
            std::error_code error;
            std::filesystem::create_directories(std::filesystem::path(options.captureDir), error);
            if (error)
            {
                LOG_ERROR(sLog, "The capture folder %s could not be created", options.captureDir.c_str());
                Finish();
                break;
            }
        }
        LOG_INFO(sLog, "Capturing %d frames to %s", options.captureFrames, options.captureDir.c_str());
        sState = CaptureState_Capturing;
        break;
    case CaptureState_Capturing:
    case CaptureState_Done:
        break;
    }
}

void PcFrameCaptureFrame(int width, int height)
{
    if (sState != CaptureState_Capturing || width <= 0 || height <= 0)
        return;

    const PcLaunchOptions& options = PcGetLaunchOptions();
    GLint readFramebuffer = 0;
    Gl::GetIntegerv(Gl::READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    Gl::BindFramebuffer(Gl::READ_FRAMEBUFFER, 0);
    Gl::ReadBuffer(Gl::BACK);
    Gl::PixelStorei(Gl::PACK_ALIGNMENT, 4);
    std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    Gl::ReadPixels(0, 0, width, height, Gl::RGBA, Gl::UNSIGNED_BYTE, &rgba[0]);
    Gl::BindFramebuffer(Gl::READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
    for (size_t index = 3; index < rgba.size(); index += 4)
        rgba[index] = 255;

    char name[32];
    std::snprintf(name, sizeof(name), "frame_%04d.png", sCaptured);
    std::filesystem::path path = std::filesystem::path(options.captureDir) / name;
    if (!WritePng(path, width, height, rgba))
    {
        LOG_ERROR(sLog, "A captured frame could not be written to %s", path.string().c_str());
        Finish();
        return;
    }

    if (++sCaptured >= options.captureFrames)
    {
        LOG_INFO(sLog, "Captured %d frames", sCaptured);
        Finish();
    }
}

#endif
