struct playing_sound
{
    v2 currentVolume;
    v2 dCurrentVolume;
    v2 targetVolume;
    
    sound_id id;
    i32 samplesPlayed;
    playing_sound *next;
};

struct audio_state
{
    memory_arena *permArena;
    playing_sound *firstPlayingSound;
    playing_sound *firstFreePlayingSound;

    v2 masterVolume;
};
