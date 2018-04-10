#ifndef PLAYER_H
#define PLAYER_H

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
class Player
{
    
public:
    explicit Player();
    ~Player();
    void initPlayer();
    void play(char *buffer,int size);
private:
   int size;
   snd_pcm_t *handle;
   snd_pcm_uframes_t frames;
};

#endif // PLAYER_H
