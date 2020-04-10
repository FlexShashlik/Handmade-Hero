struct playing_sound
{
    v2 currentVolume;
    v2 dCurrentVolume;
    v2 targetVolume;

    r32 dSample;
    
    sound_id id;
    r32 samplesPlayed;
    playing_sound *next;
};

struct audio_state
{
    memory_arena *permArena;
    playing_sound *firstPlayingSound;
    playing_sound *firstFreePlayingSound;

    v2 masterVolume;
};
