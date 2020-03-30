struct playing_sound
{
    r32 volume[2];
    sound_id id;
    i32 samplesPlayed;
    playing_sound *next;
};

struct audio_state
{
    memory_arena *permArena;
    playing_sound *firstPlayingSound;
    playing_sound *firstFreePlayingSound;
};
