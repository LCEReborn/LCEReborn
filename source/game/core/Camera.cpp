#include "game/core/Camera.hpp"

#include <cmath>
#include <cstring>

#include "platform/Platform.hpp"

#if LCE_PLATFORM_WIIU
#include <cafe/mtx.h>
#endif

#include "game/core/CameraController.hpp"
#include "game/core/FloatBuffer.hpp"
#include "game/core/GlStateManager.hpp"
#include "game/core/MemoryTracker.hpp"
#include "game/core/Mth.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/util/Vec3.hpp"
#include "game/world/Level.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockState.hpp"
#include "game/world/block/LiquidBlock.hpp"
#include "game/world/block/Material.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"
#include "game/world/generation/Player.hpp"

FloatBuffer* gCameraModelViewMatrix = MemoryTracker::createFloatBuffer(16);
FloatBuffer* gCameraProjectionMatrix = MemoryTracker::createFloatBuffer(16);
float gCameraPositionX = 0.0f;
float gCameraPositionY = 0.0f;
float gCameraPositionZ = 0.0f;
float gCameraXOff = 0.0f;
float gCameraYOff = 0.0f;
float gCameraZOff = 0.0f;
float gCameraYOff2 = 0.0f;
float gCameraXOff2 = 0.0f;
float gCameraOffsetsX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float gCameraOffsetsY[4] = {0.0f, 0.0f, 0.0f, 0.0f};
float gCameraOffsetsZ[4] = {0.0f, 0.0f, 0.0f, 0.0f};

#if LCE_PLATFORM_PC
static void Matrix44Multiply(const float a[4][4], const float b[4][4], float result[4][4])
{
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            result[row][column] = a[row][0] * b[0][column] + a[row][1] * b[1][column] + a[row][2] * b[2][column] +
                                  a[row][3] * b[3][column];
        }
    }
}

static bool Matrix44Inverse(const float source[4][4], float inverse[4][4])
{
    float work[4][4];
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            work[row][column] = source[row][column];
            inverse[row][column] = row == column ? 1.0f : 0.0f;
        }
    }
    for (int pivot = 0; pivot < 4; ++pivot)
    {
        float largest = 0.0f;
        int swap = pivot;
        for (int row = pivot; row < 4; ++row)
        {
            float magnitude = fabsf(work[row][pivot]);
            if (magnitude > largest)
            {
                largest = magnitude;
                swap = row;
            }
        }
        if (largest == 0.0f)
            return false;
        if (swap != pivot)
        {
            for (int column = 0; column < 4; ++column)
            {
                float swapped = work[pivot][column];
                work[pivot][column] = work[swap][column];
                work[swap][column] = swapped;
                swapped = inverse[pivot][column];
                inverse[pivot][column] = inverse[swap][column];
                inverse[swap][column] = swapped;
            }
        }
        float scale = 1.0f / work[pivot][pivot];
        for (int column = 0; column < 4; ++column)
        {
            work[pivot][column] *= scale;
            inverse[pivot][column] *= scale;
        }
        for (int row = 0; row < 4; ++row)
        {
            if (row == pivot)
                continue;
            float factor = work[row][pivot];
            for (int column = 0; column < 4; ++column)
            {
                work[row][column] -= work[pivot][column] * factor;
                inverse[row][column] -= inverse[pivot][column] * factor;
            }
        }
    }
    return true;
}
#endif

void Camera::prepare(boost::shared_ptr<Player> player, bool secondPass)
{
    GlStateManager::getMatrix(0, gCameraModelViewMatrix);
    GlStateManager::getMatrix(1, gCameraProjectionMatrix);

    float modelView[4][4];
    float projection[4][4];
    float combined[4][4];
    float inverse[4][4];
    std::memcpy(modelView, gCameraModelViewMatrix->mBuffer, sizeof(modelView));
    std::memcpy(projection, gCameraProjectionMatrix->mBuffer, sizeof(projection));
#if LCE_PLATFORM_WIIU
    ASM_MTX44Concat(modelView, projection, combined);
    C_MTX44Inverse(combined, inverse);
#elif LCE_PLATFORM_PC
    Matrix44Multiply(modelView, projection, combined);
    Matrix44Inverse(combined, inverse);
#endif
    gCameraPositionX = inverse[3][0] / inverse[3][3];
    gCameraPositionY = inverse[3][1] / inverse[3][3];
    gCameraPositionZ = inverse[3][2] / inverse[3][3];

    CameraController* controller = player->GetCameraController();
    float xRotation = player->mXRot;
    float yRotation = player->mYRot;
    if (controller != NULL && player->GetThirdPersonView(false) == 3)
    {
        xRotation += (float)controller->getCurrentRot()->x;
        yRotation += (float)controller->getCurrentRot()->y;
    }

    float cosYaw = cosf(yRotation * 0.017453292f);
    float side = (float)(1 - 2 * (int)secondPass);
    gCameraXOff = cosYaw * side;
    gCameraZOff = sinf(yRotation * 0.017453292f) * side;
    gCameraYOff2 = -gCameraZOff * sinf(xRotation * 0.017453292f) * side;
    gCameraXOff2 = gCameraXOff * sinf(xRotation * 0.017453292f) * side;
    gCameraYOff = cosf(xRotation * 0.017453292f);

    int index = dynamic_cast<MultiplayerLocalPlayer*>(player.get())->mUserIndex;
    gCameraOffsetsX[index] = gCameraPositionX;
    gCameraOffsetsY[index] = gCameraPositionY;
    gCameraOffsetsZ[index] = gCameraPositionZ;
}

Vec3* Camera::getCameraPos(boost::shared_ptr<Entity> entity, double partialTick)
{
    double x = (entity->mX - entity->mXo) * partialTick + entity->mXo;
    double y = (entity->mY - entity->mYo) * partialTick + entity->mYo;
    double z = (entity->mZ - entity->mZo) * partialTick + entity->mZo;
    return Vec3::newTemp(x + gCameraPositionX, y + gCameraPositionY, z + gCameraPositionZ);
}

BlockPos Camera::getCameraBlockPos(boost::shared_ptr<LivingEntity> entity, double partialTick)
{
    return BlockPos(getCameraPos(entity, partialTick));
}

const BlockState* Camera::getBlockAt(Level* level, boost::shared_ptr<Entity> entity, float partialTick)
{
    Vec3* position = getCameraPos(entity, partialTick);
    BlockPos blockPos(position);
    const BlockState* state = level->getBlockState(blockPos);
    unsigned short extraElement = level->getExtraElement(blockPos.x, blockPos.y, blockPos.z);
    if (extraElement != 0)
    {
        state = Block::getStateByElementId(extraElement);
    }

    if (state->getMaterial()->isLiquid())
    {
        float height = 0.0f;
        if (state->getBlock()->isLiquidBlock())
        {
            height = LiquidBlock::getHeightFromData(state->getValue(LiquidBlock::LEVEL)) - 0.11111111f;
        }

        float top = (float)(blockPos.y + 1) - height;
        if (position->y >= top)
        {
            state = level->getBlockState(blockPos.above());
        }
    }
    return state;
}

void Camera::UpdatePlayerOffsets(boost::shared_ptr<Player> player)
{
    int index = dynamic_cast<MultiplayerLocalPlayer*>(player.get())->mUserIndex;
    gCameraPositionX = gCameraOffsetsX[index];
    gCameraPositionY = gCameraOffsetsY[index];
    gCameraPositionZ = gCameraOffsetsZ[index];
}
