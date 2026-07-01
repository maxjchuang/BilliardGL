#include "platform_audio.h"

#include "resource_path.h"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#endif

namespace billiardgl {

#if defined(_WIN32)

namespace {

void playFileAsync(const char* fileName)
{
    const std::string path = audioPath(fileName);
    PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC);
}

}  // namespace

void playBackgroundLoop()
{
    const std::string path = audioPath("background.wav");
    PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
}

void playHit()
{
    playFileAsync("hit.wav");
}

void playBallIn()
{
    playFileAsync("ballin.wav");
}

void playGameOver()
{
    playFileAsync("GameOver.wav");
}

#else

void playBackgroundLoop() {}
void playHit() {}
void playBallIn() {}
void playGameOver() {}

#endif

}  // namespace billiardgl
