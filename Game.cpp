#include "Game.h"

#include "Constants.h"
#include "HouseData.h"
#include "ObjectType.h"
#include "Random.h"
#include "Rect.h"
#include "Renderer.h"
#include "SoundResources.h"
#include "WindowGeometry.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <SDL2/SDL.h>

using namespace std::chrono_literals;

namespace
{

constexpr int32_t bonusTimeToBeat = 256;

constexpr bool rectsOverlap(const Rect& a, const Rect& b)
{
    return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
}

int32_t simInitialVelocity(uint16_t amount, int32_t limit)
{
    int32_t simPos = (static_cast<int32_t>(amount) + 32) * 32;
    int32_t simVel = 0;
    do
    {
        simVel += 8;
        simPos += simVel;
    } while (simPos <= limit);
    return -simVel;
}

} // namespace

Game::Game(std::vector<HouseRec> houses,
    Renderer& renderer,
    SoundResources& sounds,
    const Preferences& prefs,
    size_t startRoom)
    : houses_(std::move(houses)),
      renderer_(renderer),
      sounds_(sounds),
      keyLeft_(prefs.keyLeft),
      keyRight_(prefs.keyRight),
      keyThrust_(prefs.keyThrust),
      keyFireBand_(prefs.keyFireBand)
{
    gameState_.roomIndex = startRoom;
}

GameResult Game::run()
{
    SDL_CaptureMouse(SDL_FALSE);

    while (phase_ != GamePhase::done)
    {
        if (prevTime_ == std::chrono::steady_clock::time_point {})
        {
            prevTime_ = std::chrono::steady_clock::now();
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = now - prevTime_;
        prevTime_ = now;
        accumulator_ += elapsed;

        if (phase_ == GamePhase::gameOver)
        {
            processEvents();
            while (accumulator_ >= tickDuration)
            {
                updateGameOverAnim();
                accumulator_ -= tickDuration;
            }
            renderer_.beginFrame();
            renderer_.drawGameOverScreen(playerProgress_, *gameOverState_);
            renderer_.endOfFrame();
        }
        else if (phase_ == GamePhase::endScreen)
        {
            processEvents();
            while (accumulator_ >= tickDuration)
            {
                updateEndScreenAnim();
                accumulator_ -= tickDuration;
            }
            if (endScreenAnimState_->done)
            {
                result_ = GameResult::returnToTitle;
                phase_ = GamePhase::done;
            }
            renderer_.beginFrame();
            renderer_.drawEndScreen(*endScreenAnimState_);
            renderer_.endOfFrame();
        }
        else
        {
            processEvents();
            while (accumulator_ >= tickDuration)
            {
                savePrevPositions();
                ++gameState_.animTick;

                bool roomTransition = false;
                ++playerProgress_.loopsInRoom;
                if (timeBonusOverlay_.ticksRemaining > 0)
                {
                    --timeBonusOverlay_.ticksRemaining;
                }
                updateGliderMode(roomTransition);
                const bool normalPhysics = playerState_.mode != GliderMode::ascending
                    && playerState_.mode != GliderMode::descending && playerState_.mode != GliderMode::fadingOut
                    && playerState_.mode != GliderMode::fadingIn && playerState_.mode != GliderMode::shredding;
                if (normalPhysics && !roomTransition)
                {
                    processInput();
                }
                if (normalPhysics && !roomTransition)
                {
                    updateObjects(roomTransition);
                }
                if (normalPhysics && !roomTransition)
                {
                    updateEnemies(roomTransition);
                }
                if (normalPhysics && !roomTransition)
                {
                    applyPhysics(roomTransition);
                }
                updateBand();

                accumulator_ -= tickDuration;

                if (roomTransition)
                {
                    accumulator_ = 0ns;
                    snapPrevPositions();
                    break;
                }
            }

            draw(static_cast<float>(accumulator_.count()) / static_cast<float>(tickDuration.count()),
                timeBonusOverlay_.ticksRemaining > 0 ? timeBonusOverlay_.amount : -1);
        }
    }

    return result_;
}

bool Game::advanceHouse()
{
    if (houseIndex_ + 1 < houses_.size())
    {
        ++houseIndex_;
        prevTime_ = std::chrono::steady_clock::time_point {};
        accumulator_ = 0ns;
        gameState_ = GameState {};
        houseProgress_ = HouseProgress {};
        playerState_ = PlayerState {};
        playerProgress_.loopsInRoom = 0;
        playerProgress_.roomsCompleted = 0;
        return true;
    }
    phase_ = GamePhase::endScreen;
    endScreenAnimState_.emplace();
    return false;
}

void Game::updateGameOverAnim()
{
    constexpr std::array sequenceIndex = {0, 1, 2, 3, 4, 5, 3, 6};

    if (gameOverState_->phase == 0)
    {
        ++gameOverState_->revealTimer;
        if (gameOverState_->revealTimer >= 6)
        {
            const int32_t n = gameOverState_->revealNext;
            gameOverState_->letters.push_back({sequenceIndex[n], 113 + n * 36, 100});
            gameOverState_->revealTimer = 0;
            ++gameOverState_->revealNext;
            if (gameOverState_->revealNext == 8)
            {
                gameOverState_->phase = 1;
                gameOverState_->fallX = 113;
            }
        }
    }
    else if (gameOverState_->phase == 1)
    {
        const int32_t whichLetter = sequenceIndex[gameOverState_->fallLetter];
        gameOverState_->fallX += rng::randomInt(0, gameOverState_->fallIter * 2) - gameOverState_->fallIter;
        gameOverState_->letters.push_back({whichLetter, gameOverState_->fallX, 100 + gameOverState_->fallIter * 8});
        gameOverState_->fallX += 36;
        ++gameOverState_->fallLetter;
        if (gameOverState_->fallLetter == 8)
        {
            gameOverState_->fallLetter = 0;
            gameOverState_->fallX = 113;
            ++gameOverState_->fallIter;
            if (gameOverState_->fallIter > 20)
            {
                gameOverState_->phase = 2;
            }
        }
    }
    else
    {
        ++gameOverState_->waitTick;
        if (gameOverState_->waitTick >= 200)
        {
            result_ = GameResult::returnToTitle;
            phase_ = GamePhase::done;
        }
    }
}

void Game::updateEndScreenAnim()
{
    ++endScreenAnimState_->animSubTick;
    if (endScreenAnimState_->animSubTick < 2)
    {
        return;
    }
    endScreenAnimState_->animSubTick = 0;

    if (endScreenAnimState_->boltErase)
    {
        sounds_.play(SoundResources::Sound::lightning2);
        endScreenAnimState_->boltActive = false;
        endScreenAnimState_->boltErase = false;
        endScreenAnimState_->boltSegments.clear();
    }

    ++endScreenAnimState_->tick;
    endScreenAnimState_->twisterFrame = endScreenAnimState_->tick % 4;
    if ((endScreenAnimState_->tick % 4) == 0)
    {
        --endScreenAnimState_->twisterX;
    }

    endScreenAnimState_->gliderX -= 2;
    if ((endScreenAnimState_->tick % 2) == 0)
    {
        --endScreenAnimState_->gliderY;
    }

    if (!endScreenAnimState_->boltActive && rng::randomInt(0, 19) == 0)
    {
        endScreenAnimState_->boltSegments.clear();
        int32_t x = rng::randomInt(200, 299);
        for (int32_t i = 0; i <= 8; ++i)
        {
            const int32_t nx = x + rng::randomInt(0, 6) - 3;
            endScreenAnimState_->boltSegments.emplace_back(x, nx);
            x = nx;
        }
        endScreenAnimState_->boltActive = true;
        endScreenAnimState_->boltErase = true;
    }

    if (endScreenAnimState_->tick >= 200)
    {
        endScreenAnimState_->done = true;
    }
}

void Game::killGlider(bool& roomTransition)
{
    sounds_.play(SoundResources::Sound::aww);
    playerState_.mode = GliderMode::fadingOut;
    playerState_.srcNum = playerState_.isRight ? 0 : 2;
    playerState_.forVel = 0;
    playerState_.shiftAmount = 0;
    playerState_.burnPhase = 0;
    playerState_.fadePhase = 0;
    roomTransition = true;
}

void Game::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            result_ = GameResult::quit;
            phase_ = GamePhase::done;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                const int32_t windowWidth = event.window.data1;
                const int32_t windowHeight = event.window.data2;
                renderer_.updateForWindowSize(windowWidth, windowHeight);
            }
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.scancode)
            {
            case SDL_SCANCODE_ESCAPE:
                result_ = GameResult::returnToTitle;
                phase_ = GamePhase::done;
                break;
            case SDL_SCANCODE_1:
                gameState_.roomIndex
                    = gameState_.roomIndex == 0 ? houses_[houseIndex_].numberORooms - 1 : gameState_.roomIndex - 1;
                break;
            case SDL_SCANCODE_2:
                gameState_.roomIndex
                    = gameState_.roomIndex == houses_[houseIndex_].numberORooms - 1 ? 0 : gameState_.roomIndex + 1;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

void Game::updateGliderMode(bool& roomTransition)
{
    if (playerState_.mode == GliderMode::ascending)
    {
        playerState_.destRect.left = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.left) - 2);
        playerState_.destRect.right = playerState_.destRect.left + 48;
        playerState_.destRect.top = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.top) - 6);
        playerState_.destRect.bottom = playerState_.destRect.top + 20;
        if (playerState_.destRect.top < 220)
        {
            playerState_.mode = GliderMode::normal;
        }
    }
    else if (playerState_.mode == GliderMode::descending)
    {
        playerState_.destRect.left = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.left) + 2);
        playerState_.destRect.right = playerState_.destRect.left + 48;
        playerState_.destRect.top = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.top) + 6);
        playerState_.destRect.bottom = playerState_.destRect.top + 20;
        if (playerState_.destRect.top > 120)
        {
            playerState_.mode = GliderMode::normal;
        }
    }
    else if (playerState_.mode == GliderMode::burning)
    {
        playerState_.burnPhase = 1 - playerState_.burnPhase;
        playerState_.srcNum = playerState_.isRight ? 24 + static_cast<size_t>(playerState_.burnPhase)
                                                   : 26 + static_cast<size_t>(playerState_.burnPhase);
        playerState_.forVel = playerState_.isRight ? 1 : -1;
        if (gameState_.animTick >= playerState_.burnUntil)
        {
            playerState_.destRect.top = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.bottom) - 20);
            playerState_.mode = GliderMode::fadingOut;
            playerState_.srcNum = playerState_.isRight ? 0 : 2;
            playerState_.forVel = 0;
            playerState_.burnPhase = 0;
            playerState_.fadePhase = 0;
        }
    }
    else if (playerState_.mode == GliderMode::shredding)
    {
        ++playerState_.fadePhase;
        // fadePhase 0-16: pull-in; 17-52: strips grow; 53+: strips fall at 8px/frame
        if (playerState_.fadePhase > 52)
        {
            const int32_t fallFrames = playerState_.fadePhase - 52;
            const int32_t stripsTop = playerState_.shredBlade + fallFrames * 8;
            if (stripsTop > 342)
            {
                --playerProgress_.lives;
                if (playerProgress_.lives < 0)
                {
                    phase_ = GamePhase::gameOver;
                    gameOverState_.emplace();
                    roomTransition = true;
                    return;
                }
                const int32_t spawnLeft = playerState_.enteredLeft ? 0 : 464;
                playerState_.destRect.left = static_cast<uint16_t>(spawnLeft);
                playerState_.destRect.right = static_cast<uint16_t>(spawnLeft + 48);
                playerState_.destRect.top = 40;
                playerState_.destRect.bottom = 60;
                playerState_.mode = GliderMode::fadingIn;
                playerState_.srcNum = playerState_.isRight ? 0 : 2;
                playerState_.liftAmount = constants::gravityAmount;
                playerState_.forVel = 0;
                playerState_.fadePhase = 0;
                playerState_.touchingObjects.clear();
                roomTransition = true;
            }
        }
    }
    else if (playerState_.mode == GliderMode::fadingOut)
    {
        playerState_.fadePhase++;
        if (playerState_.fadePhase > 16)
        {
            --playerProgress_.lives;
            if (playerProgress_.lives < 0)
            {
                phase_ = GamePhase::gameOver;
                gameOverState_.emplace();
                roomTransition = true;
                return;
            }
            const int32_t spawnLeft = playerState_.enteredLeft ? 0 : 464;
            playerState_.destRect.left = static_cast<uint16_t>(spawnLeft);
            playerState_.destRect.right = static_cast<uint16_t>(spawnLeft + 48);
            playerState_.destRect.top = 40;
            playerState_.destRect.bottom = 60;
            playerState_.mode = GliderMode::fadingIn;
            playerState_.srcNum = playerState_.isRight ? 0 : 2;
            playerState_.liftAmount = constants::gravityAmount;
            playerState_.forVel = 0;
            playerState_.fadePhase = 0;
            playerState_.touchingObjects.clear();
        }
    }
    else if (playerState_.mode == GliderMode::fadingIn)
    {
        playerState_.fadePhase++;
        if (playerState_.fadePhase == 1)
        {
            sounds_.play(SoundResources::Sound::beamIn);
        }
        if (playerState_.fadePhase > 16)
        {
            playerState_.mode = GliderMode::normal;
            playerState_.srcNum = playerState_.isRight ? 0 : 2;
            playerState_.fadePhase = 0;
        }
    }
    else if (playerState_.mode != GliderMode::normal)
    {
        static constexpr size_t turnSrcNums[2][12] = {
            {4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9}, // turningRtToLf
            {9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4}, // turningLfToRt
        };
        const size_t tableIdx = (playerState_.mode == GliderMode::turningRtToLf) ? 0 : 1;
        playerState_.srcNum = turnSrcNums[tableIdx][playerState_.turnPhase];
        playerState_.turnPhase++;
        if (playerState_.turnPhase > 11)
        {
            playerState_.isRight = (playerState_.mode == GliderMode::turningLfToRt);
            playerState_.turnPhase = 0;
            playerState_.mode = GliderMode::normal;
        }
    }
}

void Game::processInput()
{
    if (playerState_.mode != GliderMode::normal)
    {
        return;
    }

    const auto* keys = SDL_GetKeyboardState(nullptr);
    const bool goingRight = keys[keyRight_];
    const bool goingLeft = keys[keyLeft_];
    const bool forward = playerState_.isRight ? goingRight : goingLeft;
    const bool backward = playerState_.isRight ? goingLeft : goingRight;

    if (forward)
    {
        playerState_.isForward = true;
        playerState_.srcNum = playerState_.isRight ? 0 : 2;
        playerState_.forVel = playerState_.isRight ? constants::maxThrust : -constants::maxThrust;
    }
    else if (backward)
    {
        playerState_.isForward = false;
        playerState_.srcNum = playerState_.isRight ? 1 : 3;
        playerState_.forVel = playerState_.isRight ? -constants::maxThrust : constants::maxThrust;
    }
    else
    {
        playerState_.isForward = true;
        playerState_.forVel = 0;
        playerState_.srcNum = playerState_.isRight ? 0 : 2;
    }

    if (keys[keyThrust_] && playerProgress_.energy > 0)
    {
        --playerProgress_.energy;
        sounds_.play(SoundResources::Sound::push);
        const bool goingPositive = (playerState_.isRight == playerState_.isForward);
        playerState_.forVel = goingPositive ? constants::batteryBoost : -constants::batteryBoost;
    }

    if (keys[keyFireBand_] && !playerState_.bandBorne && playerProgress_.bands > 0)
    {
        --playerProgress_.bands;
        playerState_.bandBorne = true;
        playerState_.bandPhase = 0;
        const auto bandTop = static_cast<uint16_t>(playerState_.destRect.top + 6);
        if (playerState_.isRight)
        {
            playerState_.bandDest = {bandTop,
                playerState_.destRect.right,
                static_cast<uint16_t>(bandTop + 7),
                static_cast<uint16_t>(playerState_.destRect.right + 16)};
            playerState_.bandVelocity = 16;
        }
        else
        {
            const auto bandLeft = static_cast<uint16_t>(playerState_.destRect.left - 16);
            playerState_.bandDest
                = {bandTop, bandLeft, static_cast<uint16_t>(bandTop + 7), static_cast<uint16_t>(bandLeft + 16)};
            playerState_.bandVelocity = -16;
        }
        sounds_.play(SoundResources::Sound::fireBand);
    }
}

void Game::updateObjects(bool& roomTransition)
{
    const int32_t newLeft
        = static_cast<int32_t>(playerState_.destRect.left) + playerState_.forVel + playerState_.shiftAmount;
    const auto& currentRoom = houses_[houseIndex_].theRooms[gameState_.roomIndex];

    if (playerState_.mode != GliderMode::ascending && playerState_.mode != GliderMode::descending
        && playerState_.mode != GliderMode::fadingOut && playerState_.mode != GliderMode::fadingIn)
    {
        if (currentRoom.rightOpen && newLeft > 500)
        {
            if (gameState_.roomIndex + 1 < houses_[houseIndex_].numberORooms)
            {
                if (playerState_.enteredLeft && !houseProgress_.roomBonusAwarded.contains(gameState_.roomIndex))
                {
                    const auto loops = playerProgress_.loopsInRoom;
                    const auto bonus
                        = std::max(0, (bonusTimeToBeat - loops) * (playerProgress_.roomsCompleted / 5 + 1));
                    playerProgress_.score += bonus;
                    timeBonusOverlay_ = {bonus, 60};
                    houseProgress_.roomBonusAwarded.insert(gameState_.roomIndex);
                    ++playerProgress_.roomsCompleted;
                }
                playerProgress_.loopsInRoom = 0;
                gameState_.roomIndex++;
                playerState_.enteredLeft = true;
                playerState_.destRect.left = 0;
                playerState_.destRect.right = 48;
                roomTransition = true;
                playerState_.touchingObjects.clear();
            }
            else if (advanceHouse())
            {
                roomTransition = true;
            }
        }
        else if (currentRoom.leftOpen && newLeft < 0 && gameState_.roomIndex > 0)
        {
            playerProgress_.loopsInRoom = 0;
            gameState_.roomIndex--;
            playerState_.enteredLeft = false;
            playerState_.destRect.left = 464;
            playerState_.destRect.right = 512;
            roomTransition = true;
            playerState_.touchingObjects.clear();
        }
        else
        {
            constexpr int32_t minLeft = 0;
            const int32_t maxLeft = currentRoom.rightOpen ? 511 : (512 - 48);
            const auto clampedLeft = std::clamp(newLeft, minLeft, maxLeft);
            playerState_.destRect.left = static_cast<uint16_t>(clampedLeft);
            playerState_.destRect.right = static_cast<uint16_t>(clampedLeft + 48);
        }
    }

    playerState_.liftAmount = constants::gravityAmount;
    playerState_.shiftAmount = 0;
    if (playerState_.verticalTransitionCooldown > 0)
    {
        playerState_.verticalTransitionCooldown--;
    }

    if (roomTransition)
    {
        return;
    }

    const auto& room = houses_[houseIndex_].theRooms[gameState_.roomIndex];
    const bool airOut
        = houseProgress_.effectiveConditionCode(gameState_.roomIndex, room.conditionCode) == ConditionCode::AIR_OUT;
    std::unordered_set<ObjectKey, ObjectKeyHash> nowTouching;

    const Rect hotRect {
        static_cast<uint16_t>(playerState_.destRect.top + 5),
        static_cast<uint16_t>(playerState_.destRect.left + 10),
        static_cast<uint16_t>(playerState_.destRect.bottom - 5),
        static_cast<uint16_t>(playerState_.destRect.right - 10),
    };

    for (size_t i = 0; !roomTransition && i < room.numberOObjects; i++)
    {
        const auto& object = room.theObjects[i];
        const auto objectType = static_cast<ObjectType>(object.objectIs);
        const int32_t centerX = (static_cast<int32_t>(object.boundRect.left) + object.boundRect.right) / 2;

        if (objectType == ObjectType::floorVent)
        {
            if (!airOut)
            {
                const Rect ventRect {
                    .top = object.amount,
                    .left = static_cast<uint16_t>(centerX - 8),
                    .bottom = constants::floorVert,
                    .right = static_cast<uint16_t>(centerX + 8),
                };
                if (rectsOverlap(hotRect, ventRect))
                {
                    playerState_.liftAmount = constants::liftVentAmount;
                }
            }
        }
        else if (objectType == ObjectType::ceilingVent)
        {
            if (!airOut)
            {
                const Rect ventRect {constants::ceilingVert,
                    static_cast<uint16_t>(centerX - 8),
                    object.amount,
                    static_cast<uint16_t>(centerX + 8)};
                if (rectsOverlap(hotRect, ventRect))
                {
                    playerState_.liftAmount = constants::dropVentAmount;
                }
            }
        }
        else if (objectType == ObjectType::ceilingDuct)
        {
            const Rect ductRect {
                constants::ceilingVert,
                static_cast<uint16_t>(centerX - 8),
                object.amount,
                static_cast<uint16_t>(centerX + 8),
            };
            if (houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn))
            {
                if (!airOut && rectsOverlap(hotRect, ductRect))
                {
                    playerState_.liftAmount = constants::dropVentAmount;
                }
            }
            else
            {
                if (!airOut && rectsOverlap(hotRect, ductRect))
                {
                    playerState_.liftAmount = constants::liftVentAmount;
                }
                if (playerState_.verticalTransitionCooldown == 0 && rectsOverlap(hotRect, object.boundRect)
                    && object.extra > 0)
                {
                    if (static_cast<size_t>(object.extra) - 1 < houses_[houseIndex_].numberORooms)
                    {
                        const auto destRoomIndex = static_cast<size_t>(object.extra) - 1;
                        gameState_.roomIndex = destRoomIndex;
                        int32_t exitLeft = 232;
                        const auto& destRoomData = houses_[houseIndex_].theRooms[destRoomIndex];
                        for (size_t j = 0; j < destRoomData.numberOObjects; j++)
                        {
                            const auto& destObject = destRoomData.theObjects[j];
                            if (static_cast<ObjectType>(destObject.objectIs) == ObjectType::ceilingDuct
                                && houseProgress_.effectiveIsOn(destRoomIndex, j, destObject.isOn))
                            {
                                exitLeft = destObject.boundRect.left;
                                break;
                            }
                        }
                        playerState_.destRect.left = static_cast<uint16_t>(exitLeft);
                        playerState_.destRect.right = static_cast<uint16_t>(exitLeft + 48);
                        playerState_.destRect.top = constants::ceilingVert;
                        playerState_.destRect.bottom = constants::ceilingVert + 20;
                        playerState_.liftAmount = constants::gravityAmount;
                        playerState_.verticalTransitionCooldown = 10;
                        roomTransition = true;
                        sounds_.play(SoundResources::Sound::hey);
                    }
                    else if (advanceHouse())
                    {
                        roomTransition = true;
                        sounds_.play(SoundResources::Sound::hey);
                    }
                }
            }
        }
        else if (objectType == ObjectType::candle)
        {
            const Rect candleRect {
                object.amount,
                static_cast<uint16_t>(centerX - 12),
                object.boundRect.top,
                static_cast<uint16_t>(centerX + 4),
            };
            if (rectsOverlap(hotRect, candleRect))
            {
                playerState_.liftAmount = constants::liftVentAmount;
                if (playerState_.mode == GliderMode::normal && playerState_.destRect.bottom > object.boundRect.top)
                {
                    sounds_.play(SoundResources::Sound::yow);
                    playerState_.mode = GliderMode::burning;
                    playerState_.burnPhase = playerState_.isRight ? 1 : 0;
                    playerState_.burnUntil = gameState_.animTick + 150;
                    playerState_.destRect.top
                        = static_cast<uint16_t>(static_cast<int32_t>(playerState_.destRect.bottom) - 36);
                }
            }
        }
        else if (objectType == ObjectType::toaster)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.toasterStates[key];
            if (!state.initialised)
            {
                state.bottomPos = (static_cast<int32_t>(object.boundRect.bottom) - 3) * 32;
                state.position = state.bottomPos;
                state.velocity = simInitialVelocity(object.amount, state.bottomPos);
                state.phase = 60;
                state.paused = false;
                state.initialised = true;
            }
            if (!state.paused)
            {
                if (gameState_.animTick % 2 == 0)
                {
                    state.phase++;
                    if (state.phase > 65)
                    {
                        state.phase = 60;
                    }
                }
                state.velocity += 8;
                state.position += state.velocity;
                if (state.position > state.bottomPos)
                {
                    sounds_.play(SoundResources::Sound::toastDrop);
                    state.velocity = -state.velocity;
                    state.position = state.bottomPos;
                    state.pauseUntil = gameState_.animTick + static_cast<uint32_t>(object.extra) / 2 + 1;
                    state.paused = true;
                    state.phase = 0;
                }
            }
            else if (gameState_.animTick >= state.pauseUntil)
            {
                sounds_.play(SoundResources::Sound::toastJump);
                state.paused = false;
                state.phase = 60;
            }
            if (!state.paused && playerState_.mode == GliderMode::normal)
            {
                const int32_t toastBottom = state.position / 32;
                const Rect toastRect {
                    static_cast<uint16_t>(toastBottom - 31),
                    static_cast<uint16_t>(object.boundRect.left + 3),
                    static_cast<uint16_t>(toastBottom),
                    static_cast<uint16_t>(object.boundRect.left + 35),
                };
                if (rectsOverlap(hotRect, toastRect))
                {
                    killGlider(roomTransition);
                }
            }
        }
        else if (objectType == ObjectType::fishBowl)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.fishStates[key];
            if (!state.initialised)
            {
                state.bottomPos = (static_cast<int32_t>(object.boundRect.top) + 24) * 32;
                state.position = state.bottomPos;
                state.velocity = simInitialVelocity(object.amount, state.bottomPos);
                state.phase = 69;
                state.paused = false;
                state.initialised = true;
            }
            if (!state.paused)
            {
                if (state.velocity > -16 && state.velocity < 16)
                {
                    state.phase = 69;
                }
                else if (state.velocity < 0)
                {
                    state.phase = 66;
                }
                else
                {
                    state.phase = 68;
                }
                state.velocity += 8;
                state.position += state.velocity;
                if (state.position > state.bottomPos)
                {
                    state.velocity = -state.velocity;
                    state.position = state.bottomPos;
                    state.pauseUntil = gameState_.animTick + static_cast<uint32_t>(object.extra) / 2 + 1;
                    state.paused = true;
                }
            }
            else if (gameState_.animTick >= state.pauseUntil)
            {
                state.paused = false;
                state.phase = 66;
                sounds_.play(SoundResources::Sound::drip);
            }
            if (!state.paused && playerState_.mode == GliderMode::normal)
            {
                const int32_t fishBottom = state.position / 32;
                const Rect fishRect {
                    static_cast<uint16_t>(fishBottom - 16),
                    static_cast<uint16_t>(object.boundRect.left + 8),
                    static_cast<uint16_t>(fishBottom),
                    static_cast<uint16_t>(object.boundRect.left + 24),
                };
                if (rectsOverlap(hotRect, fishRect))
                {
                    killGlider(roomTransition);
                }
            }
        }
        else if (objectType == ObjectType::teaKettle)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.teaKtlStates[key];
            if (!state.initialised)
            {
                state.nextEventAt = gameState_.animTick;
                state.phase = 0;
                state.steamActive = false;
                state.initialised = true;
            }
            if (!state.steamActive && gameState_.animTick >= state.nextEventAt)
            {
                state.steamActive = true;
                state.phase = 0;
                sounds_.play(SoundResources::Sound::teaKettle);
            }
            if (state.steamActive)
            {
                state.phase++;
                if (state.phase > 10)
                {
                    state.phase = 0;
                    state.steamActive = false;
                    state.nextEventAt = gameState_.animTick + static_cast<uint32_t>(object.amount) / 2 + 1;
                }
                const Rect steamRect {
                    static_cast<uint16_t>(std::max(static_cast<int32_t>(constants::ceilingVert),
                        static_cast<int32_t>(object.boundRect.top) - 128)),
                    static_cast<uint16_t>(std::max(0, static_cast<int32_t>(object.boundRect.left) - 128)),
                    object.boundRect.top,
                    object.boundRect.left,
                };
                if (rectsOverlap(hotRect, steamRect))
                {
                    playerState_.liftAmount = constants::liftVentAmount;
                    playerState_.shiftAmount = constants::fanThrustAmount;
                }
            }
        }
        else if (objectType == ObjectType::drip)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.dripStates[key];
            if (!state.initialised)
            {
                state.phase = 53;
                state.position = static_cast<int32_t>(object.boundRect.bottom) * 32;
                state.velocity = 0;
                state.initialised = true;
            }
            const bool binaryFlip = (gameState_.animTick % 2) != 0;
            if (state.phase < 57)
            {
                if (binaryFlip)
                {
                    state.phase++;
                    if (state.phase == 57)
                    {
                        sounds_.play(SoundResources::Sound::drip);
                        state.position += 160;
                    }
                }
            }
            else
            {
                constexpr int32_t accel = 12;
                const int32_t limit = static_cast<int32_t>(object.amount) * 32;
                state.velocity += accel;
                state.position += state.velocity;
                if (state.position > limit)
                {
                    state.velocity = 0;
                    state.position = static_cast<int32_t>(object.boundRect.bottom) * 32;
                    state.phase = 53;
                }
            }
            if (state.phase >= 57 && playerState_.mode == GliderMode::normal)
            {
                const int32_t dropBottom = state.position / 32;
                const Rect dropRect {
                    static_cast<uint16_t>(dropBottom - 14),
                    object.boundRect.left,
                    static_cast<uint16_t>(dropBottom),
                    static_cast<uint16_t>(object.boundRect.left + 16),
                };
                if (rectsOverlap(hotRect, dropRect))
                {
                    killGlider(roomTransition);
                }
            }
        }
        else if (objectType == ObjectType::ball)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.ballStates[key];
            if (!state.initialised)
            {
                state.reset = static_cast<int32_t>(object.boundRect.bottom) * 32;
                state.position = state.reset;
                state.velocity = simInitialVelocity(object.amount, state.reset);
                state.initialised = true;
            }
            state.velocity += 8;
            state.position += state.velocity;
            if (state.position > state.reset)
            {
                sounds_.play(SoundResources::Sound::bounce);
                state.velocity = -state.velocity;
                state.position = state.reset;
            }
            if (playerState_.mode == GliderMode::normal)
            {
                const int32_t ballBottom = state.position / 32;
                const Rect ballRect {
                    static_cast<uint16_t>(ballBottom - 32),
                    object.boundRect.left,
                    static_cast<uint16_t>(ballBottom),
                    static_cast<uint16_t>(object.boundRect.left + 32),
                };
                if (rectsOverlap(hotRect, ballRect))
                {
                    killGlider(roomTransition);
                }
            }
        }
        else if (objectType == ObjectType::grease)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.greaseStates[key];
            if (!state.initialised)
            {
                const bool isOn = houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn);
                state.currentRight = object.boundRect.right;
                if (!isOn)
                {
                    state.reset = 999;
                    state.currentRight = object.amount;
                }
                state.initialised = true;
            }

            if (state.reset > 0 && state.reset != 999)
            {
                state.reset++;
                if (state.reset > 4)
                {
                    state.currentRight++;
                    if (state.currentRight >= object.amount)
                    {
                        state.currentRight = object.amount;
                        state.reset = 999;
                    }
                }
            }

            if (state.reset == 0 && houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn)
                && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::greaseFall);
                houseProgress_.objectIsOnOverrides[{gameState_.roomIndex, i}] = 0;
                state.reset = 1;
            }

            if (state.reset >= 1 && state.currentRight > object.boundRect.right)
            {
                const Rect slideZone {
                    .top = static_cast<uint16_t>(object.boundRect.bottom - 6),
                    .left = object.boundRect.right,
                    .bottom = object.boundRect.bottom,
                    .right = static_cast<uint16_t>(state.currentRight),
                };
                if (rectsOverlap(hotRect, slideZone))
                {
                    const int32_t snapDelta = object.boundRect.bottom - playerState_.destRect.bottom;
                    playerState_.destRect.top = static_cast<uint16_t>(playerState_.destRect.top + snapDelta);
                    playerState_.destRect.bottom = static_cast<uint16_t>(playerState_.destRect.bottom + snapDelta);
                    playerState_.sliding = true;
                }
            }
        }
        else if (objectType == ObjectType::clock)
        {
            const auto amount = houseProgress_.effectiveAmount(gameState_.roomIndex, i, object.amount);
            if (amount > 0 && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::clock);
                playerProgress_.score += amount;
                houseProgress_.objectAmountOverrides[{gameState_.roomIndex, i}] = 0;
            }
        }
        else if (objectType == ObjectType::paper)
        {
            const auto amount = houseProgress_.effectiveAmount(gameState_.roomIndex, i, object.amount);
            if (amount > 0 && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::extra);
                playerProgress_.score += amount;
                ++playerProgress_.lives;
                houseProgress_.objectAmountOverrides[{gameState_.roomIndex, i}] = 0;
            }
        }
        else if (objectType == ObjectType::bonusRect)
        {
            const auto amount = houseProgress_.effectiveAmount(gameState_.roomIndex, i, object.amount);
            if (amount > 0 && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::extra);
                playerProgress_.score += amount;
                houseProgress_.objectAmountOverrides[{gameState_.roomIndex, i}] = 0;
                sounds_.play(SoundResources::Sound::goodMove);
            }
        }
        else if (objectType == ObjectType::battery)
        {
            const auto amount = houseProgress_.effectiveAmount(gameState_.roomIndex, i, object.amount);
            if (amount > 0 && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::energize);
                playerProgress_.energy += amount;
                houseProgress_.objectAmountOverrides[{gameState_.roomIndex, i}] = 0;
            }
        }
        else if (objectType == ObjectType::rubberBand)
        {
            const auto amount = houseProgress_.effectiveAmount(gameState_.roomIndex, i, object.amount);
            if (amount > 0 && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::getBand);
                playerProgress_.bands += amount;
                houseProgress_.objectAmountOverrides[{gameState_.roomIndex, i}] = 0;
            }
        }
        else if (objectType == ObjectType::table || objectType == ObjectType::shelf || objectType == ObjectType::books
            || objectType == ObjectType::cabinet || objectType == ObjectType::macintosh
            || objectType == ObjectType::toaster || objectType == ObjectType::obstacleRect)
        {
            if (playerState_.mode == GliderMode::normal && rectsOverlap(hotRect, object.boundRect))
            {
                killGlider(roomTransition);
            }
        }
        else if (objectType == ObjectType::upStairs)
        {
            const Rect triggerRect {
                object.boundRect.top,
                static_cast<uint16_t>(object.boundRect.left + 32),
                static_cast<uint16_t>(object.boundRect.top + 8),
                static_cast<uint16_t>(object.boundRect.right - 32),
            };
            if (rectsOverlap(hotRect, triggerRect) && object.amount > 0)
            {
                if (static_cast<size_t>(object.amount) - 1 < houses_[houseIndex_].numberORooms)
                {
                    const auto destRoomIdx = static_cast<size_t>(object.amount) - 1;
                    gameState_.roomIndex = destRoomIdx;
                    int32_t exitLeft = 232;
                    const auto& destRoomData = houses_[houseIndex_].theRooms[destRoomIdx];
                    for (size_t j = 0; j < destRoomData.numberOObjects; j++)
                    {
                        const auto& destObj = destRoomData.theObjects[j];
                        if (static_cast<ObjectType>(destObj.objectIs) == ObjectType::downStairs)
                        {
                            exitLeft = destObj.boundRect.left + 64;
                            break;
                        }
                    }
                    playerState_.destRect.left = static_cast<uint16_t>(exitLeft);
                    playerState_.destRect.right = static_cast<uint16_t>(exitLeft + 48);
                    playerState_.destRect.top = static_cast<uint16_t>(constants::floorVert - 20);
                    playerState_.destRect.bottom = static_cast<uint16_t>(constants::floorVert);
                    playerState_.mode = GliderMode::ascending;
                    roomTransition = true;
                }
                else if (advanceHouse())
                {
                    roomTransition = true;
                }
            }
        }
        else if (objectType == ObjectType::downStairs)
        {
            const Rect triggerRect {
                static_cast<uint16_t>(object.boundRect.bottom - 8),
                static_cast<uint16_t>(object.boundRect.left + 32),
                object.boundRect.bottom,
                static_cast<uint16_t>(object.boundRect.right - 32),
            };
            if (rectsOverlap(hotRect, triggerRect) && object.amount > 0)
            {
                if (static_cast<size_t>(object.amount) - 1 < houses_[houseIndex_].numberORooms)
                {
                    const auto destRoomIdx = static_cast<size_t>(object.amount) - 1;
                    gameState_.roomIndex = destRoomIdx;
                    int32_t exitLeft = 232;
                    const auto& destRoomData = houses_[houseIndex_].theRooms[destRoomIdx];
                    for (size_t j = 0; j < destRoomData.numberOObjects; j++)
                    {
                        const auto& destObj = destRoomData.theObjects[j];
                        if (static_cast<ObjectType>(destObj.objectIs) == ObjectType::upStairs)
                        {
                            exitLeft = destObj.boundRect.left + 64;
                            break;
                        }
                    }
                    playerState_.destRect.left = static_cast<uint16_t>(exitLeft);
                    playerState_.destRect.right = static_cast<uint16_t>(exitLeft + 48);
                    playerState_.destRect.top = static_cast<uint16_t>(constants::ceilingVert + 20);
                    playerState_.destRect.bottom = static_cast<uint16_t>(constants::ceilingVert + 40);
                    playerState_.mode = GliderMode::descending;
                    roomTransition = true;
                }
                else if (advanceHouse())
                {
                    roomTransition = true;
                }
            }
        }
        else if (objectType == ObjectType::exitRect)
        {
            if (rectsOverlap(hotRect, object.boundRect))
            {
                if (object.amount != 0 && static_cast<size_t>(object.amount) - 1 < houses_[houseIndex_].numberORooms)
                {
                    const auto destRoomIdx = static_cast<size_t>(object.amount) - 1;
                    gameState_.roomIndex = destRoomIdx;
                    int32_t exitLeft = 232;
                    const auto& destRoomData = houses_[houseIndex_].theRooms[destRoomIdx];
                    for (size_t j = 0; j < destRoomData.numberOObjects; j++)
                    {
                        const auto& destObj = destRoomData.theObjects[j];
                        if (static_cast<ObjectType>(destObj.objectIs) == ObjectType::ceilingDuct
                            && houseProgress_.effectiveIsOn(destRoomIdx, j, destObj.isOn))
                        {
                            exitLeft = destObj.boundRect.left;
                            break;
                        }
                    }
                    playerState_.destRect.left = static_cast<uint16_t>(exitLeft);
                    playerState_.destRect.right = static_cast<uint16_t>(exitLeft + 48);
                    playerState_.destRect.top = constants::ceilingVert;
                    playerState_.destRect.bottom = constants::ceilingVert + 20;
                    playerState_.liftAmount = constants::gravityAmount;
                    roomTransition = true;
                    playerState_.touchingObjects.clear();
                    sounds_.play(SoundResources::Sound::hey);
                }
                else if (advanceHouse())
                {
                    roomTransition = true;
                    sounds_.play(SoundResources::Sound::hey);
                }
            }
        }
        else if (objectType == ObjectType::leftFan
            && houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn))
        {
            const Rect fanRect {
                static_cast<uint16_t>(object.boundRect.top + 10),
                object.amount,
                static_cast<uint16_t>(object.boundRect.top + 30),
                object.boundRect.left,
            };
            if (rectsOverlap(hotRect, fanRect))
            {
                playerState_.shiftAmount = constants::fanThrustAmount;
                if (playerState_.isRight && playerState_.mode == GliderMode::normal)
                {
                    playerState_.mode = GliderMode::turningRtToLf;
                    playerState_.turnPhase = 0;
                }
            }
        }
        else if (objectType == ObjectType::rightFan
            && houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn))
        {
            const Rect fanRect {
                static_cast<uint16_t>(object.boundRect.top + 10),
                object.boundRect.right,
                static_cast<uint16_t>(object.boundRect.top + 30),
                object.amount,
            };
            if (rectsOverlap(hotRect, fanRect))
            {
                playerState_.shiftAmount = -constants::fanThrustAmount;
                if (!playerState_.isRight && playerState_.mode == GliderMode::normal)
                {
                    playerState_.mode = GliderMode::turningLfToRt;
                    playerState_.turnPhase = 0;
                }
            }
        }
        else if (objectType == ObjectType::shredder)
        {
            if (houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn)
                && playerState_.mode == GliderMode::normal && rectsOverlap(hotRect, object.boundRect))
            {
                sounds_.play(SoundResources::Sound::shredder);
                playerState_.mode = GliderMode::shredding;
                playerState_.fadePhase = 0;
                playerState_.shredBlade = object.boundRect.bottom - 8;
                playerState_.destRect.left = static_cast<uint16_t>(object.boundRect.left + 8);
                playerState_.destRect.right = playerState_.destRect.left + 48;
                playerState_.forVel = 0;
                playerState_.shiftAmount = 0;
                roomTransition = true;
                sounds_.playTimed(SoundResources::Sound::shredder,
                    std::chrono::duration_cast<std::chrono::milliseconds>(52 * tickDuration));
            }
        }
        else if (objectType == ObjectType::outlet)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.outletStates[key];
            if (state.stateChangeAt == 0)
            {
                state.stateChangeAt = gameState_.animTick + 1;
            }
            if (!state.isSparking && gameState_.animTick >= state.stateChangeAt)
            {
                constexpr uint32_t animTickDuration = 30;
                sounds_.playTimed(SoundResources::Sound::zap,
                    std::chrono::duration_cast<std::chrono::milliseconds>(animTickDuration * tickDuration));
                state.isSparking = true;
                state.stateChangeAt = gameState_.animTick + animTickDuration;
            }
            else if (state.isSparking && gameState_.animTick >= state.stateChangeAt)
            {
                state.isSparking = false;
                const uint32_t idleFrames = static_cast<uint32_t>(object.amount) / 2 + 1;
                state.stateChangeAt = gameState_.animTick + idleFrames;
            }
            if (state.isSparking && playerState_.mode == GliderMode::normal && rectsOverlap(hotRect, object.boundRect))
            {
                killGlider(roomTransition);
            }
        }
        else if (objectType == ObjectType::window)
        {
            const ObjectKey key {gameState_.roomIndex, i};
            auto& state = gameState_.windowStates[key];
            if (!state.initialised)
            {
                const auto panes = computeWindowPanes(object.boundRect.toSDLRect(), object.isOn);

                const int32_t bL = panes.top.x;
                const int32_t bR = bL + panes.top.w;
                const int32_t bT = panes.top.y;
                const int32_t bB = panes.bot.y + panes.bot.h;
                const int32_t segH = std::max(1, (bB - bT) / 7);

                for (auto& bolt : state.bolts)
                {
                    bolt[0][0] = bL + rng::randomInt(0, std::max(1, bR - bL) - 1);
                    bolt[0][1] = bT;
                    for (int32_t j = 1; j < 8; j++)
                    {
                        const int32_t nx = bolt[j - 1][0] + rng::randomInt(0, segH * 4 + 1) - segH * 2;
                        bolt[j][0] = std::clamp(nx, bL + 1, bR - 1);
                        bolt[j][1] = (j == 7) ? bB : segH * j + bT;
                    }
                }
                state.whichBolt = rng::randomInt(0, 2);
                state.whatTime = gameState_.animTick + 60 + static_cast<uint32_t>(rng::randomInt(0, 59));
                state.whatPhase = 0;
                state.initialised = true;
            }

            if (state.whatPhase == 0)
            {
                if (gameState_.animTick >= state.whatTime)
                {
                    sounds_.play(SoundResources::Sound::lightning2);
                    state.whatPhase = 1;
                }
            }
            else if (state.whatPhase > 3)
            {
                state.whatPhase = 0;
                state.whichBolt = rng::randomInt(0, 2);
                state.whatTime = gameState_.animTick + static_cast<uint32_t>(rng::randomInt(0, 149));
            }
            else
            {
                state.whatPhase++;
            }

            if (houseProgress_.effectiveIsOn(gameState_.roomIndex, i, object.isOn))
            {
                playerState_.liftAmount += rng::randomInt(-5, 5);
                playerState_.shiftAmount += rng::randomInt(-5, 5);
            }
        }
        else if (objectType == ObjectType::lightSwitch)
        {
            if (rectsOverlap(hotRect, object.boundRect))
            {
                const ObjectKey key {gameState_.roomIndex, i};
                nowTouching.insert(key);
                if (!playerState_.touchingObjects.contains(key))
                {
                    const auto effectiveCode
                        = houseProgress_.effectiveConditionCode(gameState_.roomIndex, room.conditionCode);
                    if (effectiveCode == ConditionCode::LIGHTS_OUT)
                    {
                        houseProgress_.roomConditionOverrides[gameState_.roomIndex] = ConditionCode::NONE;
                        sounds_.play(SoundResources::Sound::lightsOn);
                    }
                }
            }
        }
        else if (objectType == ObjectType::guitar)
        {
            if (rectsOverlap(hotRect, object.boundRect))
            {
                const ObjectKey key {gameState_.roomIndex, i};
                nowTouching.insert(key);
                if (!playerState_.touchingObjects.contains(key))
                {
                    sounds_.play(SoundResources::Sound::guitar);
                }
            }
        }
        else if (objectType == ObjectType::thermostat)
        {
            if (rectsOverlap(hotRect, object.boundRect))
            {
                const ObjectKey key {gameState_.roomIndex, i};
                nowTouching.insert(key);
                if (!playerState_.touchingObjects.contains(key))
                {
                    const auto effectiveCode
                        = houseProgress_.effectiveConditionCode(gameState_.roomIndex, room.conditionCode);
                    if (effectiveCode == ConditionCode::AIR_OUT)
                    {
                        houseProgress_.roomConditionOverrides[gameState_.roomIndex] = ConditionCode::NONE;
                        sounds_.play(SoundResources::Sound::blowerOn);
                    }
                }
            }
        }
        else if (objectType == ObjectType::powerSwitch)
        {
            if (rectsOverlap(hotRect, object.boundRect))
            {
                const ObjectKey key {gameState_.roomIndex, i};
                nowTouching.insert(key);
                if (!playerState_.touchingObjects.contains(key))
                {
                    if (object.amount > 0 && static_cast<size_t>(object.amount) - 1 < room.numberOObjects)
                    {
                        const size_t linkedIdx = static_cast<size_t>(object.amount) - 1;
                        const auto& linkedObj = room.theObjects[linkedIdx];
                        const uint8_t currentIsOn
                            = houseProgress_.effectiveIsOn(gameState_.roomIndex, linkedIdx, linkedObj.isOn);
                        houseProgress_.objectIsOnOverrides[{gameState_.roomIndex, linkedIdx}] = currentIsOn ? 0 : 1;
                    }
                    sounds_.play(SoundResources::Sound::lightsOn);
                }
            }
        }
    }

    playerState_.touchingObjects = std::move(nowTouching);
}

void Game::updateEnemies(bool& roomTransition)
{
    const auto& room = houses_[houseIndex_].theRooms[gameState_.roomIndex];
    if (room.animateNumber == 0)
    {
        return;
    }

    auto& animState = gameState_.animateStates[gameState_.roomIndex];
    const uint16_t count = std::min(static_cast<uint16_t>(16), room.animateNumber);
    const uint32_t delay = room.animateDelay;

    auto initEnemy = [&](EnemyInstance& e, uint16_t idx) {
        if (room.animateKind == AnimateKind::DART)
        {
            e.top = rng::randomInt(0, 149);
            e.left = 512;
            e.right = 576;
            e.bottom = e.top + 22;
            e.horizontalOffset = -8;
            e.verticalOffset = 1;
            e.phase = 0;
        }
        else if (room.animateKind == AnimateKind::COPTER)
        {
            e.left = rng::randomInt(256, 511);
            e.right = e.left + 32;
            e.top = -32;
            e.bottom = 0;
            e.horizontalOffset = -4;
            e.verticalOffset = 2;
            e.phase = rng::randomInt(0, 7);
        }
        else
        {
            e.left = rng::randomInt(50, 449);
            e.right = e.left + 32;
            e.top = 342;
            e.bottom = 374;
            e.horizontalOffset = 0;
            e.verticalOffset = -3;
            e.phase = rng::randomInt(0, 7);
        }
        e.tickStamp = gameState_.animTick + (idx * delay / 2 / std::max(count, static_cast<uint16_t>(1)));
        e.unSeen = false;
        e.prevLeft = e.left;
        e.prevTop = e.top;
    };

    if (!animState.initialised)
    {
        for (uint16_t i = 0; i < count; ++i)
        {
            initEnemy(animState.enemies[i], i);
        }
        animState.initialised = true;
    }

    const Rect hotRect {
        static_cast<uint16_t>(playerState_.destRect.top + 5),
        static_cast<uint16_t>(playerState_.destRect.left + 10),
        static_cast<uint16_t>(playerState_.destRect.bottom - 5),
        static_cast<uint16_t>(playerState_.destRect.right - 10),
    };

    for (uint16_t i = 0; i < count && !roomTransition; ++i)
    {
        auto& enemy = animState.enemies[i];
        if (enemy.unSeen)
        {
            if (gameState_.animTick >= enemy.tickStamp)
            {
                initEnemy(enemy, i);
            }
            continue;
        }

        enemy.left += enemy.horizontalOffset;
        enemy.right += enemy.horizontalOffset;
        enemy.top += enemy.verticalOffset;
        enemy.bottom += enemy.verticalOffset;

        if (enemy.phase != -1)
        {
            enemy.phase = (enemy.phase + 1) % 8;
        }

        if (enemy.right < 0 || enemy.left > 512 || enemy.bottom < 0 || enemy.top > 342)
        {
            enemy.unSeen = true;
            enemy.tickStamp = gameState_.animTick + delay / 2;
            continue;
        }

        const Rect enemyRect {
            static_cast<uint16_t>(std::max(0, enemy.top)),
            static_cast<uint16_t>(std::max(0, enemy.left)),
            static_cast<uint16_t>(std::max(0, enemy.bottom)),
            static_cast<uint16_t>(std::max(0, enemy.right)),
        };
        if (rectsOverlap(hotRect, enemyRect))
        {
            killGlider(roomTransition);
        }
    }
}

void Game::applyPhysics(bool& roomTransition)
{
    playerState_.liftAmount += 1;
    if (playerState_.sliding)
    {
        playerState_.liftAmount = 0;
        playerState_.sliding = false;
    }
    else
    {
        auto newTop = static_cast<int32_t>(playerState_.destRect.top) + playerState_.liftAmount;
        if (newTop < static_cast<int32_t>(constants::ceilingVert))
        {
            newTop = static_cast<int32_t>(constants::ceilingVert);
        }
        if (newTop == static_cast<int32_t>(constants::ceilingVert))
        {
            playerState_.liftAmount = constants::gravityAmount;
        }
        playerState_.destRect.top = static_cast<uint16_t>(newTop);
        playerState_.destRect.bottom = static_cast<uint16_t>(newTop + 20);
        if (playerState_.destRect.bottom > constants::floorLimit)
        {
            killGlider(roomTransition);
        }
    }
}

void Game::updateBand()
{
    if (!playerState_.bandBorne)
    {
        return;
    }

    if (playerState_.bandDest.left > 512 || static_cast<int32_t>(playerState_.bandDest.left) < -16)
    {
        playerState_.bandBorne = false;
        return;
    }

    playerState_.bandPhase = (playerState_.bandPhase + 1) % 3;
    playerState_.bandDest.left = static_cast<uint16_t>(playerState_.bandDest.left + playerState_.bandVelocity);
    playerState_.bandDest.right = static_cast<uint16_t>(playerState_.bandDest.right + playerState_.bandVelocity);

    const auto& bandRoom = houses_[houseIndex_].theRooms[gameState_.roomIndex];
    if (bandRoom.animateNumber > 0)
    {
        auto& [enemies, initialised] = gameState_.animateStates[gameState_.roomIndex];
        const uint16_t count = std::min(static_cast<uint16_t>(enemies.size()), bandRoom.animateNumber);
        for (uint16_t j = 0; j < count; ++j)
        {
            auto& enemy = enemies[j];
            if (enemy.unSeen || enemy.phase == -1)
            {
                continue;
            }
            const Rect enemyRect {
                static_cast<uint16_t>(std::max(0, enemy.top)),
                static_cast<uint16_t>(std::max(0, enemy.left)),
                static_cast<uint16_t>(std::max(0, enemy.bottom)),
                static_cast<uint16_t>(std::max(0, enemy.right)),
            };
            if (rectsOverlap(playerState_.bandDest, enemyRect))
            {
                enemy.phase = -1;
                enemy.horizontalOffset = 0;
                enemy.verticalOffset = 12;
                switch (bandRoom.animateKind)
                {
                case AnimateKind::DART:
                    playerProgress_.score += 300;
                    sounds_.play(SoundResources::Sound::crunch);
                    break;
                case AnimateKind::COPTER:
                    playerProgress_.score += 200;
                    sounds_.play(SoundResources::Sound::crunch);
                    break;
                case AnimateKind::BALOON:
                    playerProgress_.score += 100;
                    sounds_.play(SoundResources::Sound::pop);
                    break;
                }
            }
        }
    }
}

void Game::draw(const float alpha, const int32_t timeBonusAmount) const
{
    renderer_.beginFrame();
    auto& roomData = houses_[houseIndex_].theRooms[gameState_.roomIndex];
    renderer_.drawRoom(gameState_, houseProgress_, roomData);
    renderer_.drawObjects(gameState_, houseProgress_, roomData, playerState_, alpha);
    renderer_.drawEnemies(gameState_, roomData, alpha);
    renderer_.drawGlider(playerState_, alpha);
    renderer_.drawBand(playerState_);
    renderer_.drawStatusBar(playerProgress_, roomData, gameState_.roomIndex + 1);
    if (timeBonusAmount >= 0)
    {
        renderer_.drawTimeBonusOverlay(timeBonusAmount);
    }
    renderer_.endOfFrame();
}

void Game::savePrevPositions()
{
    playerState_.prevRect = playerState_.destRect;

    for (auto& [key, state] : gameState_.dripStates)
    {
        if (key.roomIndex == gameState_.roomIndex)
        {
            state.prevPosition = state.position;
        }
    }
    for (auto& [key, state] : gameState_.toasterStates)
    {
        if (key.roomIndex == gameState_.roomIndex)
        {
            state.prevPosition = state.position;
        }
    }
    for (auto& [key, state] : gameState_.fishStates)
    {
        if (key.roomIndex == gameState_.roomIndex)
        {
            state.prevPosition = state.position;
        }
    }
    for (auto& [key, state] : gameState_.ballStates)
    {
        if (key.roomIndex == gameState_.roomIndex)
        {
            state.prevPosition = state.position;
        }
    }

    if (const auto it = gameState_.animateStates.find(gameState_.roomIndex); it != gameState_.animateStates.end())
    {
        for (auto& enemy : it->second.enemies)
        {
            enemy.prevLeft = enemy.left;
            enemy.prevTop = enemy.top;
        }
    }
}

void Game::snapPrevPositions()
{
    playerState_.prevRect = playerState_.destRect;

    for (auto& state : gameState_.dripStates | std::views::values)
    {
        state.prevPosition = state.position;
    }
    for (auto& state : gameState_.toasterStates | std::views::values)
    {
        state.prevPosition = state.position;
    }
    for (auto& state : gameState_.fishStates | std::views::values)
    {
        state.prevPosition = state.position;
    }
    for (auto& state : gameState_.ballStates | std::views::values)
    {
        state.prevPosition = state.position;
    }

    for (auto& roomState : gameState_.animateStates | std::views::values)
    {
        for (auto& enemy : roomState.enemies)
        {
            enemy.prevLeft = enemy.left;
            enemy.prevTop = enemy.top;
        }
    }
}
