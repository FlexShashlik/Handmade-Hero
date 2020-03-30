internal void
OutputTestSineWave(game_state *gameState, game_sound_output_buffer *soundBuffer, i32 toneHz)
{
    i16 toneVolume = 3000;
    i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;
    i16 *sampleOut = soundBuffer->samples;
    
    for(i32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
    {
#if 1
        r32 sineValue = sinf(gameState->tSine);
        i16 sampleValue = (i16)(sineValue * toneVolume);
#else
        i16 sampleValue = 0;
#endif
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
#if 1
        gameState->tSine += Tau32 / (r32)wavePeriod;
        if(gameState->tSine > Tau32)
        {
            gameState->tSine -= Tau32;
        }
#endif
    }
}

internal playing_sound *
PlaySound(audio_state *audioState, sound_id soundID)
{
    if(!audioState->firstFreePlayingSound)
    {
        audioState->firstFreePlayingSound = PushStruct(audioState->permArena, playing_sound);
        audioState->firstFreePlayingSound->next = 0;
    }
    
    playing_sound *playingSound = audioState->firstFreePlayingSound;
    audioState->firstFreePlayingSound = playingSound->next;
    
    playingSound->samplesPlayed = 0;
    playingSound->volume[0] = 1.0f;
    playingSound->volume[1] = 1.0f;
    playingSound->id = soundID;
    
    playingSound->next = audioState->firstPlayingSound;
    audioState->firstPlayingSound = playingSound;

    return playingSound;
}

internal void
OutputPlayingSounds
(
    audio_state *audioState, game_sound_output_buffer *soundBuffer,
    game_assets *assets, memory_arena *tempArena
)
{
    temporary_memory mixerMemory = BeginTemporaryMemory(tempArena);
    
    r32 *realChannel0 = PushArray(tempArena, soundBuffer->sampleCount, r32);
    r32 *realChannel1 = PushArray(tempArena, soundBuffer->sampleCount, r32);

    // NOTE: Clear out the mixer channels
    {
        r32 *dest0 = realChannel0;
        r32 *dest1 = realChannel1;
        for(i32 sampleIndex = 0;
            sampleIndex < soundBuffer->sampleCount;
            sampleIndex++)
        {
            *dest0++ = 0.0f;
            *dest1++ = 0.0f;
        }
    }

    // NOTE: Sum all sounds
    for(playing_sound **playingSoundPtr = &audioState->firstPlayingSound;
        *playingSoundPtr;
        )
    {
        playing_sound *playingSound = *playingSoundPtr;

        r32 *dest0 = realChannel0;
        r32 *dest1 = realChannel1;

        ui32 totalSamplesToMix = soundBuffer->sampleCount;
        b32 soundFinished = false;
        while(totalSamplesToMix && !soundFinished)
        {
            loaded_sound *loadedSound = GetSound(assets, playingSound->id);
            if(loadedSound)
            {
                asset_sound_info *info = GetSoundInfo(assets, playingSound->id);
                PrefetchSound(assets, info->nextIDToPlay);
            
                // TODO: Handle stereo!
                r32 volume0 = playingSound->volume[0];
                r32 volume1 = playingSound->volume[1];
                
                Assert(playingSound->samplesPlayed >= 0);

                ui32 samplesToMix = totalSamplesToMix;
                ui32 samplesRemainingInSound = loadedSound->sampleCount - playingSound->samplesPlayed;
                if(samplesToMix > samplesRemainingInSound)
                {
                    samplesToMix = samplesRemainingInSound;
                }
            
                for(ui32 sampleIndex = playingSound->samplesPlayed;
                    sampleIndex < playingSound->samplesPlayed + samplesToMix;
                    sampleIndex++)
                {
                    r32 sampleValue = loadedSound->samples[0][sampleIndex];
                    *dest0++ += volume0 * sampleValue;
                    *dest1++ += volume1 * sampleValue;
                }

                Assert(totalSamplesToMix >= samplesToMix);
                playingSound->samplesPlayed += samplesToMix;
                totalSamplesToMix -= samplesToMix;

                if((ui32)playingSound->samplesPlayed == loadedSound->sampleCount)
                {
                    if(IsValid(info->nextIDToPlay))
                    {
                        playingSound->id = info->nextIDToPlay;
                        playingSound->samplesPlayed = 0;
                    }
                    else
                    {
                        soundFinished = true;
                    }
                }
                else
                {
                    Assert(totalSamplesToMix == 0);
                }
            }
            else
            {
                LoadSound(assets, playingSound->id);
                break;
            }
        }

        if(soundFinished)
        {
            *playingSoundPtr = playingSound->next;
            playingSound->next = audioState->firstFreePlayingSound;
            audioState->firstFreePlayingSound = playingSound;
        }
        else
        {
            playingSoundPtr = &playingSound->next;
        }
    }

    // NOTE: Convert to 16-bit
    {
        r32 *source0 = realChannel0;
        r32 *source1 = realChannel1;
    
        i16 *sampleOut = soundBuffer->samples;
        for(i32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; sampleIndex++)
        {
            *sampleOut++ = (i16)(*source0++ + 0.5f);
            *sampleOut++ = (i16)(*source1++ + 0.5f);
        }
    }
    
    EndTemporaryMemory(mixerMemory);
}

internal void
InitializeAudioState(audio_state *audioState, memory_arena *permArena)
{
    audioState->permArena = permArena;
    audioState->firstPlayingSound = 0;
    audioState->firstFreePlayingSound = 0;
}
