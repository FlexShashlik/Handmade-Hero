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
    playingSound->currentVolume = playingSound->targetVolume = {1.0f, 1.0f};
    playingSound->dCurrentVolume = {0, 0};
    playingSound->id = soundID;
    playingSound->dSample = 1.0f;
    
    playingSound->next = audioState->firstPlayingSound;
    audioState->firstPlayingSound = playingSound;

    return playingSound;
}

internal void
ChangeVolume
(
    audio_state *audioState, playing_sound *sound, r32 fadeDurationInSeconds, v2 volume
)
{
    if(fadeDurationInSeconds <= 0.0f)
    {
        sound->currentVolume = sound->targetVolume = volume;
    }
    else
    {
        r32 oneOverFade = 1.0f / fadeDurationInSeconds;
        sound->targetVolume = volume;
        sound->dCurrentVolume = oneOverFade * (sound->targetVolume - sound->currentVolume);
    }
}

internal void
ChangePitch
(audio_state *audioState, playing_sound *sound, r32 dSample)
{
    sound->dSample = dSample;
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

    r32 secondsPerSample = 1.0f / (r32)soundBuffer->samplesPerSecond;
#define AudioStateOutputChannelCount 2
    
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
            
                v2 volume = playingSound->currentVolume;
                v2 dVolume = secondsPerSample * playingSound->dCurrentVolume;
                r32 dSample = 1.0f * playingSound->dSample;

                Assert(playingSound->samplesPlayed >= 0.0f);

                ui32 samplesToMix = totalSamplesToMix;
                r32 realSamplesRemainingInSound = (loadedSound->sampleCount - RoundR32ToI32(playingSound->samplesPlayed)) / dSample;
                ui32 samplesRemainingInSound = RoundR32ToI32(realSamplesRemainingInSound);
                if(samplesToMix > samplesRemainingInSound)
                {
                    samplesToMix = samplesRemainingInSound;
                }

                b32 volumeEnded[AudioStateOutputChannelCount] = {};
                for(ui32 channelIndex = 0; channelIndex < ArrayCount(volumeEnded); channelIndex++)
                {
                    if(dVolume.e[channelIndex] != 0.0f)
                    {
                        r32 deltaVolume = (playingSound->targetVolume.e[channelIndex] -
                                           volume.e[channelIndex]);
                        ui32 volumeSampleCount = (ui32)((deltaVolume /
                                                         dVolume.e[channelIndex]) + 0.5f);
                        if(samplesToMix > volumeSampleCount)
                        {
                            samplesToMix = volumeSampleCount;
                            volumeEnded[channelIndex] = true;
                        }
                    }
                }
                
                // TODO: Handle stereo!
                r32 samplePosition = playingSound->samplesPlayed;
                for(ui32 i = 0; i < samplesToMix; i++)
                {
#if 1
                    ui32 sampleIndex = FloorR32ToI32(samplePosition);
                    r32 frac = samplePosition - (r32)sampleIndex;
                    r32 sample0 = (r32)loadedSound->samples[0][sampleIndex];
                    r32 sample1 = (r32)loadedSound->samples[0][sampleIndex + 1];
                    r32 sampleValue = Lerp(sample0, frac, sample1);
#else               
                    ui32 sampleIndex = RoundR32ToI32(samplePosition);
                    r32 sampleValue = loadedSound->samples[0][sampleIndex];
#endif
                    *dest0++ += audioState->masterVolume.e[0] * volume.e[0] * sampleValue;
                    *dest1++ += audioState->masterVolume.e[1] * volume.e[1] * sampleValue;

                    volume += dVolume;
                    samplePosition += dSample;
                }

                playingSound->currentVolume = volume;

                for(ui32 channelIndex = 0; channelIndex < ArrayCount(volumeEnded); channelIndex++)
                {
                    if(volumeEnded[channelIndex])
                    {
                        playingSound->currentVolume.e[channelIndex] = playingSound->targetVolume.e[channelIndex];
                        playingSound->dCurrentVolume.e[channelIndex] = 0.0f;
                    }
                }

                Assert(totalSamplesToMix >= samplesToMix);
                playingSound->samplesPlayed = samplePosition;
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
            // TODO: Once this is in SIMD, clamp!
            
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
    audioState->masterVolume = {1.0f, 1.0f};
}
