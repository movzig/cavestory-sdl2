#include "player.h"

#include "damage_texts.h"
#include "first_cave_bat.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "projectile.h"
#include "timer.h"

const units::FPS kFps{60};
const auto kMaxFrameTime = std::chrono::milliseconds{5 * 1000 / 60};
units::Tile Game::kScreenWidth{20};
units::Tile Game::kScreenHeight{15};

Game::Game() :
    sdlEngine_(),
    graphics_(),
    player_{new Player(graphics_,
            Vector<units::Game>{
            units::tileToGame(Game::kScreenWidth/2),
            units::tileToGame(Game::kScreenHeight/2)}
            )
    },
    bat_{new FirstCaveBat(graphics_,
            Vector<units::Game>{
            units::tileToGame(7),
            units::tileToGame(Game::kScreenHeight/2 + 1)}
            )
    },
    map_{Map::createTestMap(graphics_)},
    bump_particle_(),
    damage_texts_()
{
    runEventLoop();
}

Game::~Game()
{
}

void Game::runEventLoop() {
    Input input;
    SDL_Event event;

    damage_texts_.addDamageable(player_);
    damage_texts_.addDamageable(bat_);
    const auto bump_pos = Vector<units::Game>{
        units::tileToGame(kScreenWidth) / 2,
        units::tileToGame(kScreenHeight) / 2
    };
    bump_particle_.reset(new HeadBumpParticle(graphics_, bump_pos));

    bool running{true};
    auto last_updated_time = std::chrono::high_resolution_clock::now();
    while (running) {
        using std::chrono::high_resolution_clock;
        using std::chrono::milliseconds;
        using std::chrono::duration_cast;

        const auto start_time = high_resolution_clock::now();

        input.beginNewFrame();
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                input.keyDownEvent(event);
                break;
            case SDL_KEYUP:
                input.keyUpEvent(event);
                break;
            default:
                break;
            }
        }
        if (input.wasKeyPressed(SDL_SCANCODE_Q)) {
            running = false;
        }

        // Player Horizontal Movement
        if (input.isKeyHeld(SDL_SCANCODE_LEFT)
                && input.isKeyHeld(SDL_SCANCODE_RIGHT)) {
            // if both left and right are being pressed we need to stop moving
            player_->stopMoving();
        } else if (input.isKeyHeld(SDL_SCANCODE_LEFT)) {
            player_->startMovingLeft();
        } else if (input.isKeyHeld(SDL_SCANCODE_RIGHT)) {
            player_->startMovingRight();
        } else {
            player_->stopMoving();
        }

        if (input.isKeyHeld(SDL_SCANCODE_UP) &&
                input.isKeyHeld(SDL_SCANCODE_DOWN)) {
            player_->lookHorizontal();
        } else if (input.isKeyHeld(SDL_SCANCODE_UP)) {
            player_->lookUp();
        } else if (input.isKeyHeld(SDL_SCANCODE_DOWN)) {
            player_->lookDown();
        } else {
            player_->lookHorizontal();
        }

        // Player Jump
        if (input.wasKeyPressed(SDL_SCANCODE_Z)) {
            player_->startJump();
        } else if (input.wasKeyReleased(SDL_SCANCODE_Z)) {
            player_->stopJump();
        }
        // Player Fire
        if (input.wasKeyPressed(SDL_SCANCODE_X)) {
            player_->startFire();
        } else if (input.wasKeyReleased(SDL_SCANCODE_X)) {
            player_->stopFire();
        }

        // update scene and last_updated_time
        const auto current_time = high_resolution_clock::now();
        const auto upd_elapsed_time = current_time - last_updated_time;

        update(std::min(
                    duration_cast<milliseconds>(upd_elapsed_time),
                    kMaxFrameTime)
                );
        last_updated_time = current_time;

        draw(graphics_);

        // calculate delay for constant fps
        const auto end_time = high_resolution_clock::now();
        const auto elapsed_time = duration_cast<milliseconds>(
                end_time - start_time);

        const auto delay_duration = milliseconds(1000) / kFps - elapsed_time;
        if (delay_duration.count() >= 0)
            SDL_Delay(delay_duration.count());
    }
}

void Game::update(const std::chrono::milliseconds elapsed_time)
{
    Timer::updateAll(elapsed_time);
    damage_texts_.update(elapsed_time);
    bump_particle_->update(elapsed_time);

    //TODO: update map when it is changed
    player_->update(elapsed_time, *map_);
    auto player_pos = player_->getCenterPos();
    if (bat_) {
        if (!bat_->update(elapsed_time, player_pos.x)) {
            bat_.reset();
        }
    }

    auto projectiles = player_->getProjectiles();
    for (auto projectile: projectiles) {
        auto projectile_rect = projectile->getCollisionRectangle();
        if (bat_ && bat_->getCollisionRectangle().collidesWith(projectile_rect)) {
            projectile->collideWithEnemy();
            bat_->takeDamage(projectile->getContactDamage());
        }
    }

    if (bat_) {
        const auto batRect = bat_->getDamageRectangle();
        if (batRect.collidesWith(player_->getDamageRectangle())) {
            player_->takeDamage(bat_->contactDamage());
        }
    }
}

void Game::draw(Graphics& graphics) const
{
    graphics.clear();

    map_->drawBackground(graphics);
    if (bat_)
        bat_->draw(graphics);
    player_->draw(graphics);
    map_->draw(graphics);
    bump_particle_->draw(graphics);

    damage_texts_.draw(graphics);
    player_->drawHUD(graphics);

    graphics.flip();
}
