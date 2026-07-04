#include "platform_audio.h"

#include "resource_path.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>

#include <mutex>
#include <string>

namespace billiardgl {

namespace {

class AudioSystem {
public:
    AudioSystem() : initialized_(false), disabled_(false), background_(NULL), hit_(NULL), ballIn_(NULL), gameOver_(NULL) {}

    ~AudioSystem()
    {
        if (background_) {
            Mix_FreeChunk(background_);
        }
        if (hit_) {
            Mix_FreeChunk(hit_);
        }
        if (ballIn_) {
            Mix_FreeChunk(ballIn_);
        }
        if (gameOver_) {
            Mix_FreeChunk(gameOver_);
        }
        if (initialized_) {
            Mix_CloseAudio();
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }

    void playBackgroundLoop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ensureReady()) {
            return;
        }
        if (!background_) {
            background_ = loadChunk("background.wav");
        }
        if (background_ && !Mix_Playing(backgroundChannel_)) {
            backgroundChannel_ = Mix_PlayChannel(-1, background_, -1);
        }
    }

    void playHit()
    {
        playEffect(hit_, "hit.wav");
    }

    void playBallIn()
    {
        playEffect(ballIn_, "ballin.wav");
    }

    void playGameOver()
    {
        playEffect(gameOver_, "GameOver.wav");
    }

private:
    bool ensureReady()
    {
        if (disabled_) {
            return false;
        }
        if (initialized_) {
            return true;
        }
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            disabled_ = true;
            return false;
        }
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            disabled_ = true;
            return false;
        }
        Mix_AllocateChannels(16);
        initialized_ = true;
        return true;
    }

    Mix_Chunk* loadChunk(const char* fileName)
    {
        const std::string path = audioPath(fileName);
        return Mix_LoadWAV(path.c_str());
    }

    void playEffect(Mix_Chunk*& chunk, const char* fileName)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!ensureReady()) {
            return;
        }
        if (!chunk) {
            chunk = loadChunk(fileName);
        }
        if (chunk) {
            Mix_PlayChannel(-1, chunk, 0);
        }
    }

    std::mutex mutex_;
    bool initialized_;
    bool disabled_;
    int backgroundChannel_ = -1;
    Mix_Chunk* background_;
    Mix_Chunk* hit_;
    Mix_Chunk* ballIn_;
    Mix_Chunk* gameOver_;
};

AudioSystem& audioSystem()
{
    static AudioSystem system;
    return system;
}

}  // namespace

void playBackgroundLoop()
{
    audioSystem().playBackgroundLoop();
}

void playHit()
{
    audioSystem().playHit();
}

void playBallIn()
{
    audioSystem().playBallIn();
}

void playGameOver()
{
    audioSystem().playGameOver();
}

}  // namespace billiardgl
