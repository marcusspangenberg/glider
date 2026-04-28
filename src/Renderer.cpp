#include "Renderer.h"
#include "Colors.h"
#include "Constants.h"
#include "fonts/GlyphsSans.h"
#include "GameOverState.h"
#include "GameState.h"
#include "HouseData.h"
#include "HouseProgress.h"
#include "PlayerProgress.h"
#include "PlayerState.h"
#include "Rect.h"
#include "Resources.h"
#include "WindowGeometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <SDL_image.h>
#include <string>
#include <vector>

namespace
{

int32_t interpolate(const int32_t prev, const int32_t curr, const float alpha)
{
    return prev + static_cast<int32_t>(std::roundf(static_cast<float>(curr - prev) * alpha));
}

void setColor(SDL_Renderer* renderer, const SDL_Color& color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void drawLineH(SDL_Renderer* renderer, const int32_t x1, const int32_t y, const int32_t x2, const int32_t thickness)
{
    const SDL_Rect r = {std::min(x1, x2), y, std::abs(x2 - x1), thickness};
    SDL_RenderFillRect(renderer, &r);
}

void drawLineV(SDL_Renderer* renderer, const int32_t x, const int32_t y1, const int32_t y2, const int32_t thickness)
{
    const SDL_Rect r = {x, std::min(y1, y2), thickness, std::abs(y2 - y1)};
    SDL_RenderFillRect(renderer, &r);
}

void drawRectOutline(SDL_Renderer* renderer, const SDL_Rect rect, const int32_t thickness)
{
    const SDL_Rect top = {rect.x, rect.y, rect.w, thickness};
    const SDL_Rect bottom = {rect.x, rect.y + rect.h - thickness, rect.w, thickness};
    const SDL_Rect left = {rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness};
    const SDL_Rect right = {rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - 2 * thickness};
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bottom);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);
}

void fillAndFrame(SDL_Renderer* renderer, const SDL_Color& color, const SDL_Rect rect, const int32_t s)
{
    setColor(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
    setColor(renderer, Colors::black);
    drawRectOutline(renderer, rect, s);
}

void highlightRect(SDL_Renderer* renderer, const SDL_Color& color, const SDL_Rect rect, const int32_t s)
{
    setColor(renderer, color);
    drawLineH(renderer, rect.x + s, rect.y + s, rect.x + rect.w - 2 * s, s);
    drawLineV(renderer, rect.x + rect.w - 2 * s, rect.y + s, rect.y + rect.h - 2 * s, s);
}

void lowlightRect(SDL_Renderer* renderer, const SDL_Rect rect, const int32_t s)
{
    setColor(renderer, Colors::lightBrown);
    drawLineV(renderer, rect.x - s, rect.y, rect.y + rect.h, s);

    setColor(renderer, Colors::darkGray);
    drawLineH(renderer, rect.x, rect.y - s, rect.x + rect.w, s);
    drawLineV(renderer, rect.x + rect.w, rect.y - s, rect.y + rect.h, s);
}

constexpr SDL_Rect insetRect(const SDL_Rect rect, const int32_t width, const int32_t height)
{
    return {rect.x + width, rect.y + height, rect.w - (width * 2), rect.h - (height * 2)};
}

constexpr SDL_Rect offsetRect(const SDL_Rect rect, const int32_t x, const int32_t y)
{
    return {rect.x + x, rect.y + y, rect.w, rect.h};
}

constexpr int32_t calcBottom(const SDL_Rect rect)
{
    return rect.y + rect.h;
}

void fillEllipse(SDL_Renderer* renderer, int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t lineThickness)
{
    for (int32_t dy = -ry; dy <= ry; ++dy)
    {
        const float ratio = static_cast<float>(dy) / static_cast<float>(ry);
        const auto halfW = static_cast<int32_t>(static_cast<float>(rx) * std::sqrt(1.0f - ratio * ratio));
        const SDL_Rect r = {cx - halfW, cy + dy, 2 * halfW, lineThickness};
        SDL_RenderFillRect(renderer, &r);
    }
}

void fillPolygon(SDL_Renderer* renderer, const std::initializer_list<SDL_FPoint> points, const SDL_Color color)
{
    const auto n = static_cast<int32_t>(points.size());
    if (n < 3)
    {
        return;
    }
    std::vector<SDL_Vertex> vertices;
    vertices.reserve(n);
    for (const auto& p : points)
    {
        vertices.push_back({p, color, {0.0f, 0.0f}});
    }
    std::vector<int32_t> indices;
    indices.reserve((n - 2) * 3);
    for (int32_t i = 1; i < n - 1; ++i)
    {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), n, indices.data(), static_cast<int32_t>(indices.size()));
}

} // namespace

Renderer::Renderer(SDL_Renderer* sdlRenderer, Resources& resources, SDL_Rect screenRect, const int32_t renderScale)
    : sdlRenderer_(sdlRenderer),
      resources_(resources),
      screenRect_(screenRect),
      physScreenRect_({screenRect.x * renderScale,
          screenRect.y * renderScale,
          screenRect.w * renderScale,
          screenRect.h * renderScale}),
      renderScale_(renderScale),
      viewport_({0, 0, screenRect.w * renderScale, screenRect.h * renderScale})
{
    glyphAtlasSans_ = IMG_LoadTexture(sdlRenderer_, "fonts/glyphs_sans.png");
    if (glyphAtlasSans_)
    {
        SDL_SetTextureBlendMode(glyphAtlasSans_, SDL_BLENDMODE_BLEND);
    }
}

Renderer::~Renderer()
{
    if (glyphAtlasSans_)
    {
        SDL_DestroyTexture(glyphAtlasSans_);
    }
}

void Renderer::updateForWindowSize(const int32_t windowWidth, const int32_t windowHeight)
{
    renderScale_ = getRenderScale(windowWidth, windowHeight);
    physScreenRect_ = {0, 0, screenRect_.w * renderScale_, screenRect_.h * renderScale_};
    const int32_t offsetX = (windowWidth - physScreenRect_.w) / 2;
    const int32_t offsetY = (windowHeight - physScreenRect_.h) / 2;
    viewport_ = {offsetX, offsetY, physScreenRect_.w, physScreenRect_.h};
}

int32_t Renderer::getRenderScale(const int32_t windowWidth, const int32_t windowHeight) const
{
    return std::max(1, std::min(windowWidth / screenRect_.w, windowHeight / screenRect_.h));
}

void Renderer::beginFrame() const
{
    SDL_RenderSetViewport(sdlRenderer_, nullptr);
    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
    setColor(sdlRenderer_, Colors::black);
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderClear(sdlRenderer_);
    SDL_RenderSetViewport(sdlRenderer_, &viewport_);
}

void Renderer::drawRoom(const GameState& gameState, const HouseProgress& progress, const RoomData& roomData) const
{
    const auto conditionCode = progress.effectiveConditionCode(gameState.roomIndex, roomData.conditionCode);

    if (conditionCode == ConditionCode::LIGHTS_OUT)
    {
        setColor(sdlRenderer_, Colors::black);
        SDL_RenderFillRect(sdlRenderer_, &physScreenRect_);
        return;
    }

    const auto backgroundTexture = resources_.getTexture(roomData.backPictID);
    assert(backgroundTexture);

    int32_t i = 0;
    for (const auto tile : roomData.tileOrder)
    {
        constexpr int32_t tileWidth = 64;
        const SDL_Rect srcRect = {tileWidth * tile, 0, tileWidth, screenRect_.h};
        const SDL_Rect dstRect
            = {tileWidth * i * renderScale_, 0, tileWidth * renderScale_, screenRect_.h * renderScale_};
        SDL_RenderCopy(sdlRenderer_, backgroundTexture, &srcRect, &dstRect);
        ++i;
    }
}

void Renderer::drawObjects(const GameState& gameState,
    const HouseProgress& progress,
    const RoomData& roomData,
    const PlayerState& playerState,
    const float alpha) const
{
    const auto conditionCode = progress.effectiveConditionCode(gameState.roomIndex, roomData.conditionCode);

    std::array<size_t, RoomData::numObjects> drawOrder {};
    std::iota(drawOrder.begin(), drawOrder.end(), 0);
    std::ranges::stable_sort(drawOrder, [&](const size_t a, const size_t b) {
        return drawOrderPriority(roomData.theObjects[a]) < drawOrderPriority(roomData.theObjects[b]);
    });

    for (const auto i : drawOrder)
    {
        const auto& object = roomData.theObjects[i];
        const auto objectType = static_cast<ObjectType>(object.objectIs);
        if (objectType == ObjectType::nullObject)
        {
            continue;
        }

        const auto srcRect = resources_.getSourceRect(objectType);
        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, srcRect.w, srcRect.h});

        if (conditionCode == ConditionCode::LIGHTS_OUT)
        {
            switch (objectType)
            {
            case ObjectType::lightSwitch:
                SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &srcRect, &destRect);
                break;
            case ObjectType::drip:
                drawDrip(object, gameState, i, alpha);
                break;
            case ObjectType::ball:
                drawBall(object, gameState, i, alpha);
                break;
            case ObjectType::outlet:
                drawOutlet(object, gameState, i);
                break;
            case ObjectType::fishBowl:
                drawFishBowl(object, gameState, i, alpha, false);
                break;
            case ObjectType::toaster:
                drawToaster(object, gameState, i, alpha, false);
                break;
            case ObjectType::candle:
                drawCandle(object, gameState.animTick, false);
                break;
            case ObjectType::window:
                drawWindow(object, gameState, i, false);
                break;
            default:
                break;
            }
        }
        else
        {
            switch (objectType)
            {
            case ObjectType::upStairs:
                SDL_RenderCopy(sdlRenderer_, resources_.getTexture(198), &srcRect, &destRect);
                break;
            case ObjectType::downStairs:
                SDL_RenderCopy(sdlRenderer_, resources_.getTexture(199), &srcRect, &destRect);
                break;
            case ObjectType::table:
                drawTable(object);
                break;
            case ObjectType::shelf:
                drawShelf(object);
                break;
            case ObjectType::cabinet:
                drawCabinet(object);
                break;
            case ObjectType::mirror:
                drawMirror(object, playerState, alpha);
                break;
            case ObjectType::window:
                drawWindow(object, gameState, i);
                break;
            case ObjectType::candle:
                drawCandle(object, gameState.animTick);
                break;
            case ObjectType::outlet:
                drawOutlet(object, gameState, i);
                break;
            case ObjectType::drip:
                drawDrip(object, gameState, i, alpha);
                break;
            case ObjectType::fishBowl:
                drawFishBowl(object, gameState, i, alpha);
                break;
            case ObjectType::ball:
                drawBall(object, gameState, i, alpha);
                break;
            case ObjectType::toaster:
                drawToaster(object, gameState, i, alpha);
                break;
            case ObjectType::grease:
                drawGrease(object, gameState, i);
                break;
            case ObjectType::exitRect:
            case ObjectType::nullObject:
            case ObjectType::obstacleRect:
                break;
            case ObjectType::clock:
            case ObjectType::paper:
            case ObjectType::rubberBand:
            case ObjectType::battery:
                if (progress.effectiveAmount(gameState.roomIndex, i, object.amount) > 0)
                {
                    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &srcRect, &destRect);
                }
                break;
            default:
                SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &srcRect, &destRect);
                break;
            }
        }
    }

    if (showAirflow_ && conditionCode != ConditionCode::LIGHTS_OUT)
    {
        drawAirflow(roomData, gameState, progress);
    }
}

void Renderer::drawEnemies(const GameState& gameState, const RoomData& roomData, const float alpha) const
{
    if (roomData.animateNumber == 0)
    {
        return;
    }
    const auto it = gameState.animateStates.find(gameState.roomIndex);
    if (it == gameState.animateStates.end() || !it->second.initialised)
    {
        return;
    }
    const auto& state = it->second;
    const auto tex = resources_.getTexture(128);
    const uint16_t count = std::min(static_cast<uint16_t>(16), roomData.animateNumber);

    for (uint16_t i = 0; i < count; ++i)
    {
        const auto& enemy = state.enemies[i];
        if (enemy.unSeen)
        {
            continue;
        }
        SDL_Rect src {};
        {
            const auto& rects = resources_.getEnemyRects(roomData.animateKind);
            src = (enemy.phase == -1) ? rects[0] : rects[1 + enemy.phase];
        }
        const int32_t drawLeft = interpolate(enemy.prevLeft, enemy.left, alpha);
        const int32_t drawTop = interpolate(enemy.prevTop, enemy.top, alpha);
        const auto dst = mapRect({drawLeft, drawTop, enemy.right - enemy.left, enemy.bottom - enemy.top});
        SDL_RenderCopy(sdlRenderer_, tex, &src, &dst);
    }
}

void Renderer::drawGlider(const PlayerState& playerState, const float interpAlpha) const
{
    uint8_t alpha = 255;
    if (playerState.mode == GliderMode::fadingIn)
    {
        alpha = static_cast<uint8_t>((playerState.fadePhase * 255) / 16);
    }
    else if (playerState.mode == GliderMode::fadingOut)
    {
        alpha = static_cast<uint8_t>(((16 - playerState.fadePhase) * 255) / 16);
    }

    auto* tex = resources_.getTexture(128);
    SDL_SetTextureAlphaMod(tex, alpha);

    const auto sourceRect = resources_.getGliderSourceRect(playerState.srcNum);

    const int32_t drawLeft = interpolate(playerState.prevRect.left, playerState.destRect.left, interpAlpha);
    const int32_t drawTop = interpolate(playerState.prevRect.top, playerState.destRect.top, interpAlpha);

    if (playerState.mode == GliderMode::shredding)
    {
        if (playerState.fadePhase <= 16)
        {
            const int32_t gliderH = sourceRect.h;
            const int32_t visible = gliderH - (playerState.fadePhase * gliderH / 16);
            if (visible > 0)
            {
                auto src = sourceRect;
                src.y += gliderH - visible;
                src.h = visible;
                const auto dst = mapRect({drawLeft, drawTop + (gliderH - visible), src.w, src.h});
                SDL_RenderCopy(sdlRenderer_, tex, &src, &dst);
            }
        }
        else
        {
            constexpr int32_t stripSrcX = 256;
            constexpr int32_t stripSrcBottom = 208;
            constexpr int32_t stripMaxH = 36;
            const int32_t stripsH = std::min(stripMaxH, playerState.fadePhase - 16);
            const int32_t fallFrames = std::max(0, playerState.fadePhase - 52);
            const int32_t stripsTop = playerState.shredBlade + fallFrames * 8;
            const SDL_Rect src = {stripSrcX, stripSrcBottom - stripsH, 48, stripsH};
            const auto dst = mapRect({drawLeft, stripsTop, 48, stripsH});
            SDL_RenderCopy(sdlRenderer_, tex, &src, &dst);
        }
        SDL_SetTextureAlphaMod(tex, 255);
        return;
    }

    const auto dstRect = mapRect({drawLeft, drawTop, sourceRect.w, sourceRect.h});
    SDL_RenderCopy(sdlRenderer_, tex, &sourceRect, &dstRect);

    const auto shadowSourceRect = resources_.getGliderShadowRect(playerState.isRight ? 0 : 1);
    const auto shadowDstRect = mapRect({drawLeft, constants::floorVert, shadowSourceRect.w, shadowSourceRect.h});
    SDL_SetTextureAlphaMod(tex, 128);
    SDL_RenderCopy(sdlRenderer_, tex, &shadowSourceRect, &shadowDstRect);

    SDL_SetTextureAlphaMod(tex, 255);
}

void Renderer::drawBand(const PlayerState& playerState) const
{
    if (!playerState.bandBorne)
    {
        return;
    }
    const SDL_Rect& src = resources_.getBandRects()[playerState.bandPhase];
    const auto dst = mapRect({playerState.bandDest.left, playerState.bandDest.top, src.w, src.h});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &src, &dst);
}

void Renderer::endOfFrame() const
{
    SDL_RenderSetViewport(sdlRenderer_, nullptr);
    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
    SDL_RenderPresent(sdlRenderer_);
}

void Renderer::setShowAirflow(bool show)
{
    showAirflow_ = show;
}

void Renderer::renderText(std::string_view text,
    const SDL_Color color,
    const SDL_Rect clipRect,
    const int32_t align) const
{
    if (!glyphAtlasSans_ || text.empty())
    {
        return;
    }

    const float glyphScale = static_cast<float>(renderScale_) / 3.0f;
    float textW = 0;
    for (const char ch : text)
    {
        const int32_t index = static_cast<uint8_t>(ch) - GlyphsSans::firstChar;
        if (index >= 0 && index < GlyphsSans::numGlyphs)
        {
            textW += static_cast<float>(GlyphsSans::advance[index]) * glyphScale;
        }
    }

    auto startX = static_cast<float>(clipRect.x);
    if (align == 0)
    {
        startX = static_cast<float>(clipRect.x) + (static_cast<float>(clipRect.w) - textW) / 2.0f;
    }
    else if (align == 1)
    {
        startX = static_cast<float>(clipRect.x) + static_cast<float>(clipRect.w) - textW;
    }

    const float scaledCellH = GlyphsSans::cellH * glyphScale;
    const float scaledCapH = GlyphsSans::capHeight * glyphScale;
    const float scaledCapOffset = GlyphsSans::capOffset * glyphScale;
    const float startY
        = static_cast<float>(clipRect.y) + (static_cast<float>(clipRect.h) - scaledCapH) / 2.0f - scaledCapOffset;

    SDL_SetTextureColorMod(glyphAtlasSans_, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(glyphAtlasSans_, color.a);
    SDL_RenderSetClipRect(sdlRenderer_, &clipRect);

    for (const char ch : text)
    {
        const int32_t index = static_cast<uint8_t>(ch) - GlyphsSans::firstChar;
        if (index >= 0 && index < GlyphsSans::numGlyphs)
        {
            const int32_t glyphW = GlyphsSans::advance[index];
            const SDL_Rect src {GlyphsSans::xOffset[index], 0, glyphW, GlyphsSans::cellH};
            const SDL_FRect dst {startX, startY, std::roundf(static_cast<float>(glyphW) * glyphScale), scaledCellH};
            SDL_RenderCopyF(sdlRenderer_, glyphAtlasSans_, &src, &dst);
            startX += static_cast<float>(glyphW) * glyphScale;
        }
    }

    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
}

void Renderer::drawStatusBar(const PlayerProgress& progress, const RoomData& roomData, const size_t roomNumber) const
{
    const auto barRect = mapRect({0, 0, screenRect_.w, 24});
    setColor(sdlRenderer_, Colors::black);
    SDL_RenderFillRect(sdlRenderer_, &barRect);
    auto* tex128 = resources_.getTexture(128);

    const auto roomNameFrame = mapRect({3, 2, 159, 21});
    setColor(sdlRenderer_, Colors::white);
    drawRectOutline(sdlRenderer_, roomNameFrame, renderScale_);
    {
        const auto textClip = mapRect({7, 2, 151, 21});
        renderText(roomData.roomName, Colors::white, textClip, -1);
    }

    {
        constexpr SDL_Rect src = {36, 318, 18, 23};
        const auto dst = mapRect({165, 1, 18, 23});
        SDL_RenderCopy(sdlRenderer_, tex128, &src, &dst);
        renderText(std::to_string(roomNumber), Colors::white, dst, 0);
    }

    const auto scoreFrame = mapRect({220, 2, 78, 21});
    setColor(sdlRenderer_, Colors::white);
    drawRectOutline(sdlRenderer_, scoreFrame, renderScale_);
    {
        const auto textClip = mapRect({223, 2, 75, 21});
        renderText(std::to_string(progress.score), Colors::cyan, textClip, -1);
    }

    const auto rightFrame = mapRect({302, 2, 208, 21});
    setColor(sdlRenderer_, Colors::white);
    drawRectOutline(sdlRenderer_, rightFrame, renderScale_);

    if (progress.energy > 0)
    {
        const auto batNumClip = mapRect({308, 5, 18, 15});
        renderText(std::to_string(progress.energy), Colors::yellow, batNumClip, 1);
        constexpr SDL_Rect batIconSrc = {46, 301, 9, 15};
        const auto batIconDst = mapRect({326, 5, 9, 15});
        SDL_RenderCopy(sdlRenderer_, tex128, &batIconSrc, &batIconDst);
    }

    if (progress.bands > 0)
    {
        const auto bandNumClip = mapRect({341, 5, 18, 15});
        renderText(std::to_string(progress.bands), Colors::green, bandNumClip, 1);
        constexpr SDL_Rect bandIconSrc = {36, 301, 9, 15};
        const auto bandIconDst = mapRect({359, 5, 9, 15});
        SDL_RenderCopy(sdlRenderer_, tex128, &bandIconSrc, &bandIconDst);
    }

    {
        constexpr SDL_Rect lifeGliderSrc = {0, 318, 35, 15};
        const int32_t livesToShow = std::min(progress.lives, 3);
        for (int32_t i = 0; i < livesToShow; ++i)
        {
            const auto dst = mapRect({470 - i * 37, 5, 35, 15});
            SDL_RenderCopy(sdlRenderer_, tex128, &lifeGliderSrc, &dst);
        }
        if (progress.lives > 3)
        {
            const auto lifeCountClip = mapRect({384, 5, 12, 15});
            renderText(std::to_string(progress.lives), Colors::red, lifeCountClip, 0);
        }
    }
}

void Renderer::drawTimeBonusOverlay(const int32_t amount) const
{
    const auto bgRect = mapRect({190, 100, 122, 17});
    setColor(sdlRenderer_, Colors::black);
    SDL_RenderFillRect(sdlRenderer_, &bgRect);
    setColor(sdlRenderer_, Colors::white);
    drawRectOutline(sdlRenderer_, bgRect, renderScale_);
    const auto textClip = mapRect({193, 102, 116, 13});
    renderText("Time Bonus = " + std::to_string(amount), Colors::yellow, textClip, -1);
}

void Renderer::drawEndScreen(const EndScreenAnimState& animState) const
{
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(130), nullptr, &physScreenRect_);
    auto* tex128 = resources_.getTexture(128);
    constexpr SDL_Rect twisterSrc[4] = {
        {208, 126, 48, 62},
        {208, 189, 48, 62},
        {208, 252, 48, 62},
        {256, 268, 48, 62},
    };
    const auto twisterDst = mapRect({animState.twisterX, 138, 48, 62});
    SDL_RenderCopy(sdlRenderer_, tex128, &twisterSrc[animState.twisterFrame], &twisterDst);

    constexpr SDL_Rect gliderSrc = {235, 315, 21, 10};
    const auto gliderDst = mapRect({animState.gliderX, animState.gliderY, 21, 10});
    SDL_RenderCopy(sdlRenderer_, tex128, &gliderSrc, &gliderDst);

    if (animState.boltActive)
    {
        SDL_SetRenderDrawColor(sdlRenderer_, 255, 255, 255, 255);
        for (int32_t i = 0; i < static_cast<int32_t>(animState.boltSegments.size()); ++i)
        {
            constexpr int32_t cloudBottom = 140;
            const auto [x1, x2] = animState.boltSegments[i];
            const int32_t y1 = i * 8 + cloudBottom + 1;
            const int32_t y2 = (i + 1) * 8 + cloudBottom;
            SDL_RenderDrawLine(sdlRenderer_,
                x1 * renderScale_,
                y1 * renderScale_,
                x2 * renderScale_,
                y2 * renderScale_);
        }
    }
}

void Renderer::drawGameOverScreen(const PlayerProgress& progress, const GameOverAnimState& animState) const
{
    setColor(sdlRenderer_, Colors::black);
    SDL_RenderFillRect(sdlRenderer_, &physScreenRect_);

    renderText(std::to_string(progress.score), Colors::yellow, mapRect({0, 60, screenRect_.w, 30}), 0);

    constexpr std::array letterSources {
        SDL_Rect {375, 33, 34, 45},
        SDL_Rect {375, 77, 34, 45},
        SDL_Rect {375, 121, 34, 45},
        SDL_Rect {375, 165, 34, 45},
        SDL_Rect {375, 209, 34, 45},
        SDL_Rect {375, 253, 34, 45},
        SDL_Rect {375, 297, 34, 45},
    };

    auto* tex128 = resources_.getTexture(128);
    for (const auto& [srcIdx, destX, destY] : animState.letters)
    {
        const auto& source = letterSources[srcIdx];
        const auto dest = mapRect({destX, destY, 34, 45});
        SDL_RenderCopy(sdlRenderer_, tex128, &source, &dest);
    }
}

void Renderer::drawTable(const ObjectData& object) const
{
    const auto sourceRect = resources_.getSourceRect(ObjectType::table);
    if (sourceRect.w == 0 || sourceRect.h == 0)
    {
        return;
    }
    const auto width = object.boundRect.right - object.boundRect.left;
    const auto height = object.boundRect.bottom - object.boundRect.top;

    {
        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, width, height});

        setColor(sdlRenderer_, Colors::brown);
        SDL_RenderFillRect(sdlRenderer_, &destRect);

        setColor(sdlRenderer_, Colors::black);
        drawRectOutline(sdlRenderer_, destRect, renderScale_);
    }

    {
        const int32_t cx = object.boundRect.left
            + (static_cast<int32_t>(object.boundRect.top - constants::floorVert) / 5) + width / 2;
        constexpr int32_t cy = constants::floorVert + 10;
        SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdlRenderer_, 0, 0, 0, 64);
        fillEllipse(sdlRenderer_,
            cx * renderScale_,
            cy * renderScale_,
            (width / 2) * renderScale_,
            10 * renderScale_,
            renderScale_);
    }

    {
        const auto legDestRect = mapRect({((object.boundRect.left + object.boundRect.right) / 2) - 2,
            object.boundRect.bottom,
            5,
            static_cast<int32_t>(constants::floorVert) - 7 - static_cast<int32_t>(object.boundRect.bottom)});
        setColor(sdlRenderer_, Colors::black);
        SDL_RenderFillRect(sdlRenderer_, &legDestRect);
    }

    {
        setColor(sdlRenderer_, Colors::white);
        const int32_t cx = ((object.boundRect.left + object.boundRect.right) / 2) + 1;
        const int32_t lineTop = object.boundRect.bottom + (object.boundRect.right - object.boundRect.left) / 8;
        drawLineV(sdlRenderer_,
            cx * renderScale_,
            lineTop * renderScale_,
            (static_cast<int32_t>(constants::floorVert) - 7) * renderScale_,
            renderScale_);
    }

    {
        setColor(sdlRenderer_, Colors::lightBrown);
        const int32_t cx = (object.boundRect.left + object.boundRect.right) / 2;
        const int32_t lineTop = object.boundRect.bottom + (object.boundRect.right - object.boundRect.left) / 8;
        drawLineV(sdlRenderer_,
            cx * renderScale_,
            lineTop * renderScale_,
            (static_cast<int32_t>(constants::floorVert) - 7) * renderScale_,
            renderScale_);
    }

    {
        const auto topDesctRect = mapRect({((object.boundRect.left + object.boundRect.right) / 2) - 31,
            constants::floorVert - 7,
            sourceRect.w,
            sourceRect.h});
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &sourceRect, &topDesctRect);
    }
}

void Renderer::drawShelf(const ObjectData& object) const
{
    const auto sourceRect = resources_.getSourceRect(ObjectType::shelf);
    if (sourceRect.w == 0 || sourceRect.h == 0)
    {
        return;
    }
    const auto width = object.boundRect.right - object.boundRect.left;
    const auto height = object.boundRect.bottom - object.boundRect.top;

    {
        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, width, height});
        setColor(sdlRenderer_, Colors::lightBrown);
        SDL_RenderFillRect(sdlRenderer_, &destRect);
        setColor(sdlRenderer_, Colors::black);
        drawRectOutline(sdlRenderer_, destRect, renderScale_);

        setColor(sdlRenderer_, Colors::white);
        drawLineH(sdlRenderer_,
            (object.boundRect.left + 1) * renderScale_,
            (object.boundRect.top + 1) * renderScale_,
            (object.boundRect.right - 2) * renderScale_,
            renderScale_);
    }

    {
        const auto scaleFloat = static_cast<float>(renderScale_);
        const auto left = static_cast<float>(object.boundRect.left) * scaleFloat;
        const auto right = static_cast<float>(object.boundRect.right) * scaleFloat;
        const auto bottom = static_cast<float>(object.boundRect.top + height) * scaleFloat;
        SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
        fillPolygon(sdlRenderer_,
            {
                {right, bottom - 1.0f * scaleFloat},
                {right - 15.0f * scaleFloat, bottom + 14.0f * scaleFloat},
                {left - 15.0f * scaleFloat, bottom + 14.0f * scaleFloat},
                {left, bottom - 1.0f * scaleFloat},
            },
            {0, 0, 0, 64});
    }

    {
        auto destRect = mapRect({object.boundRect.left + 15, object.boundRect.bottom - 2, sourceRect.w, sourceRect.h});
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &sourceRect, &destRect);
        destRect.x = (object.boundRect.right - 25) * renderScale_;
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &sourceRect, &destRect);
    }
}

void Renderer::drawCabinet(const ObjectData& object) const
{
    const auto scaleFloat = static_cast<float>(renderScale_);
    const auto width = object.boundRect.right - object.boundRect.left;
    const auto height = object.boundRect.bottom - object.boundRect.top;

    auto panelOffset = 0;
    if (object.boundRect.bottom > 280)
    {
        {
            const auto left = static_cast<float>(object.boundRect.left) * scaleFloat;
            const auto top = static_cast<float>(object.boundRect.top) * scaleFloat;
            const auto bottom = static_cast<float>(object.boundRect.bottom) * scaleFloat;
            SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
            fillPolygon(sdlRenderer_,
                {
                    {left, top + 5.0f * scaleFloat},
                    {left - 15.0f * scaleFloat, top + 20.0f * scaleFloat},
                    {left - 15.0f * scaleFloat, bottom - 10.0f * scaleFloat},
                    {left, bottom},
                },
                {0, 0, 0, 64});
        }

        {
            const auto destRect = mapRect({object.boundRect.left, object.boundRect.top + 7, width, height - 12});
            setColor(sdlRenderer_, Colors::brown);
            SDL_RenderFillRect(sdlRenderer_, &destRect);
            setColor(sdlRenderer_, Colors::black);
            drawRectOutline(sdlRenderer_, destRect, renderScale_);
        }

        {
            const auto destRect = mapRect({object.boundRect.left + 2, object.boundRect.bottom - 5, width - 4, 5});
            setColor(sdlRenderer_, Colors::black);
            SDL_RenderFillRect(sdlRenderer_, &destRect);
            drawRectOutline(sdlRenderer_, destRect, renderScale_);
        }

        {
            const auto destRect = mapRect({object.boundRect.left - 2, object.boundRect.top, width + 4, 7});
            setColor(sdlRenderer_, Colors::lightBrown);
            SDL_RenderFillRect(sdlRenderer_, &destRect);
            setColor(sdlRenderer_, Colors::black);
            drawRectOutline(sdlRenderer_, destRect, renderScale_);
        }

        panelOffset = 5;
    }
    else
    {
        {
            const auto left = static_cast<float>(object.boundRect.left) * scaleFloat;
            const auto right = static_cast<float>(object.boundRect.right) * scaleFloat;
            const auto top = static_cast<float>(object.boundRect.top) * scaleFloat;
            const auto bottom = static_cast<float>(object.boundRect.bottom) * scaleFloat;
            SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
            fillPolygon(sdlRenderer_,
                {
                    {left, top},
                    {left - 15.0f * scaleFloat, top + 15.0f * scaleFloat},
                    {left - 15.0f * scaleFloat, bottom + 15.0f * scaleFloat},
                    {right - 15.0f * scaleFloat, bottom + 15.0f * scaleFloat},
                    {right, bottom},
                    {left, bottom},
                },
                {0, 0, 0, 64});
        }

        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, width, height});
        setColor(sdlRenderer_, Colors::brown);
        SDL_RenderFillRect(sdlRenderer_, &destRect);
        setColor(sdlRenderer_, Colors::black);
        drawRectOutline(sdlRenderer_, destRect, renderScale_);
    }

    const auto numPanels = width / 48;
    if (numPanels == 0)
    {
        const auto destRect = mapRect({object.boundRect.left + 5,
            object.boundRect.top + 5 + panelOffset,
            width - 10,
            height - (5 + panelOffset) * 2});
        setColor(sdlRenderer_, Colors::lightBrown);
        drawRectOutline(sdlRenderer_, destRect, renderScale_);
        drawLineV(sdlRenderer_,
            destRect.x + 3 * renderScale_,
            destRect.y + 3 * renderScale_,
            destRect.y + destRect.h - 4 * renderScale_,
            renderScale_);
        drawLineH(sdlRenderer_,
            destRect.x + 3 * renderScale_,
            destRect.y + destRect.h - 4 * renderScale_,
            destRect.x + destRect.w - 4 * renderScale_,
            renderScale_);

        setColor(sdlRenderer_, Colors::black);
        drawLineV(sdlRenderer_,
            destRect.x + destRect.w - 4 * renderScale_,
            destRect.y + destRect.h - 4 * renderScale_,
            destRect.y + 3 * renderScale_,
            renderScale_);
        drawLineH(sdlRenderer_,
            destRect.x + destRect.w - 4 * renderScale_,
            destRect.y + 3 * renderScale_,
            destRect.x + 3 * renderScale_,
            renderScale_);
    }
    else
    {
        const auto panelsWidth = (width - (numPanels + 1) * 5) / numPanels;
        SDL_Rect logicalRect = {object.boundRect.left + 5,
            object.boundRect.top + 5 + panelOffset,
            panelsWidth,
            height - (5 + panelOffset) * 2};

        for (auto i = 0; i < numPanels; ++i)
        {
            const auto destRect = mapRect(logicalRect);
            setColor(sdlRenderer_, Colors::lightBrown);
            drawLineV(sdlRenderer_,
                destRect.x + 3 * renderScale_,
                destRect.y + 3 * renderScale_,
                destRect.y + destRect.h - 4 * renderScale_,
                renderScale_);
            drawLineH(sdlRenderer_,
                destRect.x + 3 * renderScale_,
                destRect.y + destRect.h - 4 * renderScale_,
                destRect.x + destRect.w - 4 * renderScale_,
                renderScale_);

            setColor(sdlRenderer_, Colors::black);
            drawLineV(sdlRenderer_,
                destRect.x + destRect.w - 4 * renderScale_,
                destRect.y + destRect.h - 4 * renderScale_,
                destRect.y + 3 * renderScale_,
                renderScale_);
            drawLineH(sdlRenderer_,
                destRect.x + destRect.w - 4 * renderScale_,
                destRect.y + 3 * renderScale_,
                destRect.x + 3 * renderScale_,
                renderScale_);

            logicalRect = {logicalRect.x + 5 + panelsWidth, logicalRect.y, logicalRect.w, logicalRect.h};
        }
    }
}

void Renderer::drawMirror(const ObjectData& object, const PlayerState& playerState, float alpha) const
{
    const auto width = object.boundRect.right - object.boundRect.left;
    const auto height = object.boundRect.bottom - object.boundRect.top;

    const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, width, height});
    setColor(sdlRenderer_, Colors::brown);
    SDL_RenderFillRect(sdlRenderer_, &destRect);
    setColor(sdlRenderer_, Colors::black);
    drawRectOutline(sdlRenderer_, destRect, renderScale_);

    const auto rect = insetRect(destRect, 3 * renderScale_, 3 * renderScale_);
    setColor(sdlRenderer_, Colors::white);
    SDL_RenderFillRect(sdlRenderer_, &rect);
    drawRectOutline(sdlRenderer_, rect, renderScale_);

    const auto clipRect = insetRect(destRect, 5 * renderScale_, 5 * renderScale_);
    SDL_RenderSetClipRect(sdlRenderer_, &clipRect);

    const int32_t drawLeft = interpolate(playerState.prevRect.left, playerState.destRect.left, alpha) - 16;
    const int32_t drawTop = interpolate(playerState.prevRect.top, playerState.destRect.top, alpha) - 32;

    const auto gliderSrc = resources_.getGliderSourceRect(playerState.srcNum);
    const auto reflectionDestRect = mapRect({drawLeft, drawTop, gliderSrc.w, gliderSrc.h});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &gliderSrc, &reflectionDestRect);

    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
}

void Renderer::drawWindow(const ObjectData& object,
    const GameState& gameState,
    const size_t objectIndex,
    const bool frameVisible) const
{
    const auto width = object.boundRect.right - object.boundRect.left;
    const auto height = object.boundRect.bottom - object.boundRect.top;

    if (frameVisible)
    {
        {
            const auto scaleFloat = static_cast<float>(renderScale_);
            const auto left = static_cast<float>(object.boundRect.left) * scaleFloat;
            const auto right = static_cast<float>(object.boundRect.right) * scaleFloat;
            const auto top = static_cast<float>(object.boundRect.top) * scaleFloat;
            const auto bottom = static_cast<float>(object.boundRect.bottom) * scaleFloat;
            SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
            fillPolygon(sdlRenderer_,
                {
                    {left, top},
                    {left - 15.0f * scaleFloat, top + 15.0f * scaleFloat},
                    {left - 15.0f * scaleFloat, bottom + 15.0f * scaleFloat},
                    {right - 15.0f * scaleFloat, bottom + 15.0f * scaleFloat},
                    {right, bottom},
                    {left, bottom},
                },
                {0, 0, 0, 64});
        }

        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, width, height});
        fillAndFrame(sdlRenderer_, Colors::brown, destRect, renderScale_);
        highlightRect(sdlRenderer_, Colors::lightBrown, destRect, renderScale_);

        {
            const auto rect = mapRect({object.boundRect.left - 4, object.boundRect.top, width + 8, 6});
            fillAndFrame(sdlRenderer_, Colors::brown, rect, renderScale_);
            highlightRect(sdlRenderer_, Colors::lightBrown, rect, renderScale_);
        }

        fillAndFrame(sdlRenderer_,
            Colors::brown,
            mapRect({object.boundRect.left - 2, object.boundRect.top + 6, width + 4, 10}),
            renderScale_);

        {
            const auto rect = mapRect({object.boundRect.left - 4, object.boundRect.top + height - 6, width + 8, 6});
            fillAndFrame(sdlRenderer_, Colors::brown, rect, renderScale_);
            highlightRect(sdlRenderer_, Colors::lightBrown, rect, renderScale_);
        }

        {
            const auto rect = insetRect(destRect, 8 * renderScale_, 16 * renderScale_);
            fillAndFrame(sdlRenderer_, Colors::brown, rect, renderScale_);
            lowlightRect(sdlRenderer_, rect, renderScale_);
        }

        {
            auto rect = insetRect(destRect, 8 * renderScale_, 16 * renderScale_);
            rect.h = rect.h / 2 + 2 * renderScale_;
            fillAndFrame(sdlRenderer_, Colors::brown, rect, renderScale_);

            lowlightRect(sdlRenderer_, insetRect(rect, 6 * renderScale_, 6 * renderScale_), renderScale_);
            lowlightRect(sdlRenderer_, insetRect(rect, 8 * renderScale_, 8 * renderScale_), renderScale_);

            const auto paneRect = insetRect(rect, 10 * renderScale_, 10 * renderScale_);
            fillAndFrame(sdlRenderer_, Colors::black, paneRect, renderScale_);
            lowlightRect(sdlRenderer_, paneRect, renderScale_);
        }

        {
            auto rect = insetRect(destRect, 8 * renderScale_, 16 * renderScale_);
            const auto oldY = rect.y;
            rect.y = ((calcBottom(destRect) + destRect.y) / 2) + 2 * renderScale_;
            rect.h -= rect.y - oldY;
            setColor(sdlRenderer_, Colors::black);
            SDL_RenderFillRect(sdlRenderer_, &rect);
        }

        {
            auto rect = insetRect(destRect, 8 * renderScale_, 16 * renderScale_);
            const auto oldY = rect.y;
            rect.y = ((calcBottom(destRect) + destRect.y) / 2) - 2 * renderScale_;
            rect.h -= rect.y - oldY;
            if (object.isOn)
            {
                rect = offsetRect(rect,
                    0,
                    (26 - ((calcBottom(destRect) - destRect.y) / 2 / renderScale_)) * renderScale_);
            }
            fillAndFrame(sdlRenderer_, Colors::brown, rect, renderScale_);

            lowlightRect(sdlRenderer_, insetRect(rect, 6 * renderScale_, 6 * renderScale_), renderScale_);
            lowlightRect(sdlRenderer_, insetRect(rect, 8 * renderScale_, 8 * renderScale_), renderScale_);

            const auto paneRect = insetRect(rect, 10 * renderScale_, 10 * renderScale_);
            fillAndFrame(sdlRenderer_, Colors::black, paneRect, renderScale_);
            lowlightRect(sdlRenderer_, paneRect, renderScale_);
        }
    }

    const auto it = gameState.windowStates.find({gameState.roomIndex, objectIndex});
    if (it == gameState.windowStates.end() || !it->second.initialised || it->second.whatPhase == 0)
    {
        return;
    }
    const auto& windowState = it->second;

    const auto windowPanes = computeWindowPanes(object.boundRect.toSDLRect(), object.isOn);
    const auto topPane = mapRect(windowPanes.top);
    const auto botPane = mapRect(windowPanes.bot);

    if (windowState.whatPhase == 4)
    {
        setColor(sdlRenderer_, Colors::black);
        SDL_RenderFillRect(sdlRenderer_, &topPane);
        SDL_RenderFillRect(sdlRenderer_, &botPane);
        return;
    }

    if (windowState.whatPhase == 2)
    {
        return;
    }

    const auto& bolt = windowState.bolts[windowState.whichBolt];
    setColor(sdlRenderer_, Colors::white);
    const std::array panes = {SDL_Rect {topPane}, SDL_Rect {botPane}};
    for (const auto& pane : panes)
    {
        SDL_RenderSetClipRect(sdlRenderer_, &pane);
        for (size_t j = 1; j < 8; ++j)
        {
            SDL_RenderDrawLine(sdlRenderer_,
                bolt[j - 1][0] * renderScale_,
                bolt[j - 1][1] * renderScale_,
                bolt[j][0] * renderScale_,
                bolt[j][1] * renderScale_);
            SDL_RenderDrawLine(sdlRenderer_,
                bolt[j - 1][0] * renderScale_ + renderScale_,
                bolt[j - 1][1] * renderScale_,
                bolt[j][0] * renderScale_ + renderScale_,
                bolt[j][1] * renderScale_);
        }
    }
    SDL_RenderSetClipRect(sdlRenderer_, nullptr);
}

void Renderer::drawToaster(const ObjectData& object,
    const GameState& gameState,
    const size_t objectIndex,
    const float alpha,
    const bool bodyVisible) const
{
    const auto bodySrcRect = resources_.getSourceRect(ObjectType::toaster);
    const auto bodyDestRect = mapRect({object.boundRect.left, object.boundRect.top, bodySrcRect.w, bodySrcRect.h});
    if (bodyVisible)
    {
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &bodySrcRect, &bodyDestRect);
    }

    const auto it = gameState.toasterStates.find({gameState.roomIndex, objectIndex});
    if (it == gameState.toasterStates.end() || !it->second.initialised || it->second.paused)
    {
        return;
    }
    const auto& state = it->second;

    const auto& toastRects = resources_.getSourceRects(ObjectType::toaster);
    const auto& frameSrcRect = toastRects[1 + std::clamp(state.phase - 60, 0, 5)];

    const int32_t toastBottom = interpolate(state.prevPosition / 32, state.position / 32, alpha);
    const int32_t toastTop = toastBottom - 31;

    const auto clipTop = static_cast<int32_t>(object.amount);
    const auto clipBottom = static_cast<int32_t>(object.boundRect.top);
    const int32_t visTop = std::max(toastTop, clipTop);
    const int32_t visBottom = std::min(toastBottom, clipBottom);

    if (visTop >= visBottom)
    {
        return;
    }

    const int32_t srcOffsetY = visTop - toastTop;
    const SDL_Rect toastSrcRect = {frameSrcRect.x, frameSrcRect.y + srcOffsetY, 32, visBottom - visTop};
    const auto toastDestRect = mapRect({object.boundRect.left + 3, visTop, 32, visBottom - visTop});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &toastSrcRect, &toastDestRect);
}

void Renderer::drawBall(const ObjectData& object,
    const GameState& gameState,
    const size_t objectIndex,
    const float alpha) const
{
    const auto it = gameState.ballStates.find({gameState.roomIndex, objectIndex});
    if (it == gameState.ballStates.end() || !it->second.initialised)
    {
        return;
    }
    const auto& state = it->second;
    const auto src = resources_.getSourceRect(ObjectType::ball);
    const int32_t bottom = interpolate(state.prevPosition / 32, state.position / 32, alpha);
    const auto destRect = mapRect({object.boundRect.left, bottom - src.h, src.w, src.h});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &src, &destRect);
}

void Renderer::drawFishBowl(const ObjectData& object,
    const GameState& gameState,
    const size_t objectIndex,
    const float alpha,
    const bool bowlVisible) const
{
    const auto bowlSrcRect = resources_.getSourceRect(ObjectType::fishBowl);
    const auto bowlDestRect = mapRect({object.boundRect.left, object.boundRect.top, bowlSrcRect.w, bowlSrcRect.h});
    if (bowlVisible)
    {
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &bowlSrcRect, &bowlDestRect);
    }

    const auto it = gameState.fishStates.find({gameState.roomIndex, objectIndex});
    if (it == gameState.fishStates.end() || !it->second.initialised)
    {
        return;
    }
    const auto& state = it->second;

    const int32_t frameIdx = state.paused ? 3 : std::clamp(state.phase - 66, 0, 3);
    const auto& fishSrcRect = resources_.getSourceRects(ObjectType::fishBowl)[1 + frameIdx];

    const int32_t fishBottom = interpolate(state.prevPosition / 32, state.position / 32, alpha);
    const auto fishDestRect = mapRect({object.boundRect.left + 8, fishBottom - 16, fishSrcRect.w, fishSrcRect.h});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &fishSrcRect, &fishDestRect);
}

void Renderer::drawDrip(const ObjectData& object,
    const GameState& gameState,
    const size_t objectIndex,
    const float alpha) const
{
    const auto it = gameState.dripStates.find({gameState.roomIndex, objectIndex});
    if (it == gameState.dripStates.end() || !it->second.initialised)
    {
        return;
    }
    const auto& state = it->second;

    const auto& dripRects = resources_.getSourceRects(ObjectType::drip);
    const int32_t formIdx = std::clamp(state.phase - 53, 0, 3);
    const auto& formSrcRect = dripRects[formIdx];
    const auto formDestRect = mapRect({object.boundRect.left, object.boundRect.top, formSrcRect.w, formSrcRect.h});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &formSrcRect, &formDestRect);

    if (state.phase >= 57)
    {
        const auto& fallSrcRect = dripRects[4];
        const int32_t bottom = interpolate(state.prevPosition / 32, state.position / 32, alpha);
        const auto fallDestRect
            = mapRect({object.boundRect.left, bottom - fallSrcRect.h, fallSrcRect.w, fallSrcRect.h});
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &fallSrcRect, &fallDestRect);
    }
}

void Renderer::drawOutlet(const ObjectData& object, const GameState& gameState, const size_t objectIndex) const
{
    const auto it = gameState.outletStates.find({gameState.roomIndex, objectIndex});
    const bool sparking = (it != gameState.outletStates.end() && it->second.isSparking);

    if (!sparking)
    {
        const auto sourceRect = resources_.getSourceRect(ObjectType::outlet);
        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, sourceRect.w, sourceRect.h});
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &sourceRect, &destRect);
    }
    else
    {
        const auto& srcRect = resources_.getSourceRects(ObjectType::outlet)[1 + gameState.animTick % 2];
        const auto destRect = mapRect({object.boundRect.left, object.boundRect.top, srcRect.w, srcRect.h});
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &srcRect, &destRect);
    }
}

void Renderer::drawCandle(const ObjectData& object, const uint32_t animTick, const bool bodyVisible) const
{
    const auto bodySrcRect = resources_.getSourceRect(ObjectType::candle);
    const auto bodyDestRect = mapRect({object.boundRect.left, object.boundRect.top, bodySrcRect.w, bodySrcRect.h});
    if (bodyVisible)
    {
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &bodySrcRect, &bodyDestRect);
    }

    const auto& flameSrcRect = resources_.getSourceRects(ObjectType::candle)[1 + animTick % 3];
    const auto flameDestRect = mapRect({object.boundRect.left + 5, object.boundRect.top - 12, 16, 12});
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &flameSrcRect, &flameDestRect);
}

void Renderer::drawGrease(const ObjectData& objectData, const GameState& gameState, const size_t objectIndex) const
{
    const auto it = gameState.greaseStates.find({gameState.roomIndex, objectIndex});
    int32_t reset = objectData.isOn ? 0 : 999;
    int32_t currentRight = objectData.isOn ? objectData.boundRect.right : objectData.amount;
    if (it != gameState.greaseStates.end())
    {
        reset = it->second.reset;
        currentRight = it->second.currentRight;
    }

    const auto destRect = mapRect({objectData.boundRect.left, objectData.boundRect.top, 32, 29});
    if (reset <= 1)
    {
        const auto src = resources_.getSourceRect(ObjectType::grease);
        SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &src, &destRect);
        return;
    }

    const auto spillSrcRect = (reset == 2) ? resources_.getSourceRects(ObjectType::grease)[1]
                                           : resources_.getSourceRects(ObjectType::grease)[2];
    SDL_RenderCopy(sdlRenderer_, resources_.getTexture(128), &spillSrcRect, &destRect);

    if (currentRight > objectData.boundRect.right)
    {
        setColor(sdlRenderer_, Colors::black);
        const auto lineRect = mapRect({objectData.boundRect.right,
            objectData.boundRect.bottom - 2,
            currentRight - objectData.boundRect.right + 2,
            2});
        SDL_RenderFillRect(sdlRenderer_, &lineRect);
    }
}

void Renderer::drawDebugRect(const ObjectData& objectData) const
{
    setColor(sdlRenderer_, Colors::red);
    const auto destRect = mapRect(objectData.boundRect.toSDLRect());
    SDL_RenderFillRect(sdlRenderer_, &destRect);
}

SDL_Rect Renderer::mapRect(const SDL_Rect rect) const
{
    return {rect.x * renderScale_, rect.y * renderScale_, rect.w * renderScale_, rect.h * renderScale_};
}

void Renderer::drawAirflow(const RoomData& roomData, const GameState& gameState, const HouseProgress& progress) const
{
    SDL_SetRenderDrawBlendMode(sdlRenderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdlRenderer_, 64, 128, 255, 50);

    for (size_t i = 0; i < roomData.numberOObjects; ++i)
    {
        const auto& object = roomData.theObjects[i];
        const auto objectType = static_cast<ObjectType>(object.objectIs);
        const int32_t centerX = (static_cast<int32_t>(object.boundRect.left) + object.boundRect.right) / 2;

        SDL_Rect airRect {};
        constexpr int32_t airflowWidth = 16;

        switch (objectType)
        {
        case ObjectType::floorVent:
            airRect = mapRect({centerX - 8, object.amount, airflowWidth, object.boundRect.top - object.amount});
            break;
        case ObjectType::ceilingVent:
        case ObjectType::ceilingDuct:
            airRect = mapRect(
                {centerX - 8, object.boundRect.bottom, airflowWidth, object.amount - object.boundRect.bottom});
            break;
        case ObjectType::candle:
            airRect = mapRect({centerX - 10, object.amount, airflowWidth, object.boundRect.top - object.amount - 12});
            break;
        case ObjectType::leftFan:
            if (progress.effectiveIsOn(gameState.roomIndex, i, object.isOn))
            {
                airRect = mapRect({object.amount,
                    object.boundRect.top + 10,
                    static_cast<int32_t>(object.boundRect.left) - object.amount,
                    20});
            }
            break;
        case ObjectType::rightFan:
            if (progress.effectiveIsOn(gameState.roomIndex, i, object.isOn))
            {
                airRect = mapRect({object.boundRect.right,
                    object.boundRect.top + 10,
                    object.amount - static_cast<int32_t>(object.boundRect.right),
                    20});
            }
            break;
        default:
            continue;
        }

        if (airRect.w > 0 && airRect.h > 0)
        {
            SDL_RenderFillRect(sdlRenderer_, &airRect);
        }
    }
}

int32_t Renderer::drawOrderPriority(const ObjectData& obj)
{
    switch (static_cast<ObjectType>(obj.objectIs))
    {
    case ObjectType::upStairs:
    case ObjectType::downStairs:
        return 0;
    case ObjectType::ceilingDuct:
    case ObjectType::floorVent:
    case ObjectType::ceilingVent:
        return 1;
    case ObjectType::window:
    case ObjectType::mirror:
        return 2;
    case ObjectType::cabinet:
    case ObjectType::table:
    case ObjectType::shelf:
        return 3;
    default:
        return 4;
    }
}
