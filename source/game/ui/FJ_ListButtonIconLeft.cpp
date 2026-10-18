#include "game/ui/FJ_ListButtonIconLeft.hpp"

#include <cstdio>

#include "game/ui/FJ_Factory.hpp"
#include "game/ui/FJ_FuiNode.hpp"
#include "game/ui/FJ_FuiNodeStage.hpp"
#include "game/ui/fui.hpp"
#include "game/ui/fuiRenderNode.hpp"
#include "game/ui/fuiRenderNodeCodeGenRect.hpp"

FJ_ListButtonIconLeft::FJ_ListButtonIconLeft(fuiRenderNode* renderNode)
    : FJ_Button(renderNode),
      m_pIconPlaceholder(NULL),
      m_pIconPlaceholder1(NULL),
      m_pTimer(NULL)
{
}

void FJ_ListButtonIconLeft::Init(const std::wstring& name, int id)
{
    SetLabelAlignment(4);
    m_textOnly = true;
    FJ_Button::Init(name, id);
}

void FJ_ListButtonIconLeft::SetSlotID(int slotId, bool hasSlot)
{
    FJ_FuiNode* slotControl = FJ_FuiNode::asMovieClip(getChildByName("SlotControl"));
    if (slotControl == NULL)
        return;

    if (!hasSlot)
    {
        removeChild(slotControl, true);
        return;
    }

    fuiRect rect;
    rect.xMin = slotControl->getX();
    rect.yMin = slotControl->getY();
    rect.xMax = rect.xMin + slotControl->getWidth() / getScaleX();
    rect.yMax = rect.yMin + slotControl->getHeight();

    char name[64];
    std::sprintf(name, "iggy_slot_%d_%d", slotId, m_id);
    fuiRenderNodeCodeGenRect* renderNode = new fuiRenderNodeCodeGenRect(getStage()->m_pRenderNode, &rect, name);
    renderNode->setAlpha(0.25f);
    removeChild(slotControl, true);
    FJ_FuiNode* node = new FJ_FuiNode(renderNode);
    renderNode->setFuiNode(node);
    addChild(node);
}

void FJ_ListButtonIconLeft::SetTextureName(const std::wstring& textureName, const std::wstring& textureName1)
{
    if (!textureName.empty())
    {
        if (m_pIconPlaceholder == NULL)
        {
            FJ_FuiNode* iconControl = getChildByName("IconPlaceholder")->asMovieClip();
            iconControl->setScaleX(iconControl->getScaleX() / getScaleX());
            iconControl->setScaleY(iconControl->getScaleY() / getScaleY());

            fuiRect rect;
            rect.xMin = 0.0f;
            rect.yMin = 0.0f;
            rect.xMax = iconControl->getWidth() / iconControl->getScaleX();
            rect.yMax = iconControl->getHeight() / iconControl->getScaleY();

            unsigned int width;
            unsigned int height;
            bool hasAlpha = false;
            unsigned int texture = gFui->getCustomTexture(textureName, &width, &height, &hasAlpha);
            fuiRenderNodeCodeGenRect* renderNode = new fuiRenderNodeCodeGenRect(
                getStage()->m_pRenderNode, &rect, "CustomTexture", width, height, texture, true, hasAlpha);
            m_pIconPlaceholder = new FJ_FuiNode(renderNode);
            renderNode->setFuiNode(m_pIconPlaceholder);
            iconControl->addChild(m_pIconPlaceholder);
        }
        else
        {
            unsigned int width;
            unsigned int height;
            bool hasAlpha = false;
            unsigned int texture = gFui->getCustomTexture(textureName, &width, &height, &hasAlpha);
            static_cast<fuiRenderNodeCodeGenRect*>(m_pIconPlaceholder->m_pRenderNode)
                ->setTexture(width, height, texture, hasAlpha);
        }
    }

    if (!textureName1.empty())
    {
        if (m_pIconPlaceholder1 == NULL)
        {
            FJ_FuiNode* iconControl = getChildByName("IconPlaceholder1")->asMovieClip();
            if (iconControl != NULL)
            {
                iconControl->setScaleX(iconControl->getScaleX() / getScaleX());
                iconControl->setScaleY(iconControl->getScaleY() / getScaleY());

                fuiRect rect;
                rect.xMin = 0.0f;
                rect.yMin = 0.0f;
                rect.xMax = iconControl->getWidth() / iconControl->getScaleX();
                rect.yMax = iconControl->getHeight() / iconControl->getScaleY();

                unsigned int width;
                unsigned int height;
                bool hasAlpha = false;
                unsigned int texture = gFui->getCustomTexture(textureName1, &width, &height, &hasAlpha);
                fuiRenderNodeCodeGenRect* renderNode = new fuiRenderNodeCodeGenRect(
                    getStage()->m_pRenderNode, &rect, "CustomTexture", width, height, texture, true, hasAlpha);
                m_pIconPlaceholder1 = new FJ_FuiNode(renderNode);
                renderNode->setFuiNode(m_pIconPlaceholder1);
                iconControl->addChild(m_pIconPlaceholder1);
            }
        }
        else
        {
            unsigned int width;
            unsigned int height;
            bool hasAlpha = false;
            unsigned int texture = gFui->getCustomTexture(textureName1, &width, &height, &hasAlpha);
            static_cast<fuiRenderNodeCodeGenRect*>(m_pIconPlaceholder1->m_pRenderNode)
                ->setTexture(width, height, texture, hasAlpha);
        }
    }
}

void FJ_ListButtonIconLeft::ShowTimer()
{
    FJ_FuiNode* iconControl = FJ_FuiNode::asMovieClip(getChildByName("IconPlaceholder"));
    if (iconControl == NULL || m_pIconPlaceholder != NULL || m_pTimer != NULL)
        return;
    iconControl->setScaleX(iconControl->getScaleX() / getScaleX());
    iconControl->setScaleY(iconControl->getScaleY() / getScaleY());
    float width = iconControl->getWidth() / iconControl->getScaleX();
    float height = iconControl->getHeight() / iconControl->getScaleY();

    FJ_FuiNode* timer = FJ_Factory::createObject("fourj.FJ_Timer", iconControl->m_pRenderNode, true, NULL);
    if (timer == NULL)
        return;
    iconControl->addChild(timer);
    m_pTimer = timer;
    float timerWidth = timer->getWidth();
    float timerHeight = timer->getHeight();
    if (timerWidth <= 0.0f || timerHeight <= 0.0f)
        return;
    float scale = width / timerWidth < height / timerHeight ? width / timerWidth : height / timerHeight;
    timer->setScaleX(scale);
    timer->setScaleY(scale);
    timer->setX((width - timerWidth * scale) * 0.5f);
    timer->setY((height - timerHeight * scale) * 0.5f);
}
