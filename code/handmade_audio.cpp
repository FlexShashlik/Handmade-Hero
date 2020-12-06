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

    Assert((soundBuffer->sampleCount & 3) == 0);
    ui32 chunkCount = soundBuffer->sampleCount / 4;
    
    __m128 *realChannel0 = PushArray(tempArena, chunkCount, __m128, 16);
    __m128 *realChannel1 = PushArray(tempArena, chunkCount, __m128, 16);

    r32 secondsPerSample = 1.0f / (r32)soundBuffer->samplesPerSecond;
#define AudioStateOutputChannelCount 2
    
    __m128 zero = _mm_set1_ps(0.0f);
    __m128 one = _mm_set1_ps(1.0f);
    
    // NOTE: Clear out the mixer channels
    {
        __m128 *dest0 = realChannel0;
        __m128 *dest1 = realChannel1;
        for(ui32 sampleIndex = 0;
            sampleIndex < chunkCount;
            sampleIndex++)
        {
            _mm_store_ps((float *)dest0++, zero);
            _mm_store_ps((float *)dest1++, zero);
        }
    }

    // NOTE: Sum all sounds
    for(playing_sound **playingSoundPtr = &audioState->firstPlayingSound;
        *playingSoundPtr;
        )
    {
        playing_sound *playingSound = *playingSoundPtr;

        __m128 *dest0 = realChannel0;
        __m128 *dest1 = realChannel1;

        ui32 totalChunksToMix = chunkCount;
        b32 soundFinished = false;
        while(totalChunksToMix && !soundFinished)
        {
            loaded_sound *loadedSound = GetSound(assets, playingSound->id);
            if(loadedSound)
            {
                sound_id nextSoundInChain = GetNextSoundInChain(assets, playingSound->id);
                PrefetchSound(assets, nextSoundInChain);
                
                v2 volume = playingSound->currentVolume;
                v2 dVolume = secondsPerSample * playingSound->dCurrentVolume;
                v2 dVolumeChunk = 4.0f * dVolume;
                r32 dSample = playingSound->dSample;
                r32 dSampleChunk = 4.0f * dSample;

                // NOTE: Channel 0
                __m128 masterVolume0 = _mm_set1_ps(audioState->masterVolume.e[0]);
                __m128 volume0 = _mm_set_ps(volume.e[0] + 0.0f * dVolume.e[0],
                                            volume.e[0] + 1.0f * dVolume.e[0],
                                            volume.e[0] + 2.0f * dVolume.e[0],
                                            volume.e[0] + 3.0f * dVolume.e[0]);
                __m128 dVolume0 = _mm_set1_ps(dVolume.e[0]);
                __m128 dVolumeChunk0 = _mm_set1_ps(dVolumeChunk.e[0]);

                // NOTE: Channel 1
                __m128 masterVolume1 = _mm_set1_ps(audioState->masterVolume.e[1]);
                __m128 volume1 = _mm_set_ps(volume.e[1] + 0.0f * dVolume.e[1],
                                            volume.e[1] + 1.0f * dVolume.e[1],
                                            volume.e[1] + 2.0f * dVolume.e[1],
                                            volume.e[1] + 3.0f * dVolume.e[1]);
                __m128 dVolume1 = _mm_set1_ps(dVolume.e[1]);
                __m128 dVolumeChunk1 = _mm_set1_ps(dVolumeChunk.e[1]);

                Assert(playingSound->samplesPlayed >= 0.0f);

                ui32 chunksToMix = totalChunksToMix;
                r32 realChunksRemainingInSound = (loadedSound->sampleCount - RoundR32ToI32(playingSound->samplesPlayed)) / dSampleChunk;
                ui32 chunksRemainingInSound = RoundR32ToI32(realChunksRemainingInSound);
                if(chunksToMix > chunksRemainingInSound)
                {
                    chunksToMix = chunksRemainingInSound;
                }

                ui32 volumeEndsAt[AudioStateOutputChannelCount] = {};
                for(ui32 channelIndex = 0; channelIndex < ArrayCount(volumeEndsAt); channelIndex++)
                {
                    if(dVolumeChunk.e[channelIndex] != 0.0f)
                    {
                        r32 deltaVolume = (playingSound->targetVolume.e[channelIndex] - volume.e[channelIndex]);
                        ui32 volumeChunkCount = (ui32)((deltaVolume / dVolumeChunk.e[channelIndex]) + 0.5f);
                        if(chunksToMix > volumeChunkCount)
                        {
                            chunksToMix = volumeChunkCount;
                            volumeEndsAt[channelIndex] = volumeChunkCount;
                        }
                    }
                }
                
                // TODO: Handle stereo!
                r32 beginSamplePosition = playingSound->samplesPlayed;
                r32 endSamplePosition = beginSamplePosition + chunksToMix * dSampleChunk;
                r32 loopIndexC = (endSamplePosition - beginSamplePosition) / (r32)chunksToMix;
                for(ui32 i = 0; i < chunksToMix; i++)
                {
                    r32 samplePosition = beginSamplePosition + loopIndexC * (r32)i;
                    // TODO: Explicit volume calculation
#if 1
                    __m128 samplePos = _mm_setr_ps(samplePosition + 0.0f * dSample,
                                                   samplePosition + 1.0f * dSample,
                                                   samplePosition + 2.0f * dSample,
                                                   samplePosition + 3.0f * dSample);
                    __m128i sampleIndex = _mm_cvttps_epi32(samplePos);
                    __m128 frac = _mm_sub_ps(samplePos, _mm_cvtepi32_ps(sampleIndex));

                    __m128 sampleValueF = _mm_setr_ps(loadedSound->samples[0][((i32 *)&sampleIndex)[0]],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[1]],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[2]],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[3]]);

                    __m128 sampleValueC = _mm_setr_ps(loadedSound->samples[0][((i32 *)&sampleIndex)[0] + 1],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[1] + 1],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[2] + 1],
                                                      loadedSound->samples[0][((i32 *)&sampleIndex)[3] + 1]);

                    __m128 sampleValue = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, frac), sampleValueF),
                                                    _mm_mul_ps(frac, sampleValueC));
#else               
                    __m128 sampleValue = _mm_setr_ps(loadedSound->samples[0][RoundR32ToI32(samplePosition + 0.0f * dSample)],
                                                     loadedSound->samples[0][RoundR32ToI32(samplePosition + 1.0f * dSample)],
                                                     loadedSound->samples[0][RoundR32ToI32(samplePosition + 2.0f * dSample)],
                                                     loadedSound->samples[0][RoundR32ToI32(samplePosition + 3.0f * dSample)]);
#endif

                    __m128 d0 = _mm_load_ps((float *)&dest0[0]);
                    __m128 d1 = _mm_load_ps((float *)&dest1[0]);

                    d0 = _mm_add_ps(d0, _mm_mul_ps(_mm_mul_ps(masterVolume0, volume0), sampleValue));
                    d1 = _mm_add_ps(d1, _mm_mul_ps(_mm_mul_ps(masterVolume1, volume1), sampleValue));
                    
                    _mm_store_ps((float *)&dest0[0], d0);
                    _mm_store_ps((float *)&dest1[0], d1);

                    dest0++;
                    dest1++;
                    volume0 = _mm_add_ps(volume0, dVolumeChunk0);
                    volume1 = _mm_add_ps(volume1, dVolumeChunk1);
                }

                playingSound->currentVolume.e[0] = ((r32 *)&volume0)[0];
                playingSound->currentVolume.e[1] = ((r32 *)&volume1)[1];
                for(ui32 channelIndex = 0; channelIndex < ArrayCount(volumeEndsAt); channelIndex++)
                {
                    if(chunksToMix == volumeEndsAt[channelIndex])
                    {
                        playingSound->currentVolume.e[channelIndex] = playingSound->targetVolume.e[channelIndex];
                        playingSound->dCurrentVolume.e[channelIndex] = 0.0f;
                    }
                }

                playingSound->samplesPlayed = endSamplePosition;
                Assert(totalChunksToMix >= chunksToMix);
                totalChunksToMix -= chunksToMix;

                if(chunksToMix == chunksRemainingInSound)
                {
                    if(IsValid(nextSoundInChain))
                    {
                        playingSound->id = nextSoundInChain;
                        Assert(playingSound->samplesPlayed >= loadedSound->sampleCount);
                        playingSound->samplesPlayed -= (r32)loadedSound->sampleCount;
                        if(playingSound->samplesPlayed < 0.0f)
                        {
                            playingSound->samplesPlayed = 0.0f;
                        }
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
        __m128 *source0 = realChannel0;
        __m128 *source1 = realChannel1;
    
        __m128i *sampleOut = (__m128i *)soundBuffer->samples;
        for(ui32 sampleIndex = 0; sampleIndex < chunkCount; sampleIndex++)
        {
            __m128 s0 = _mm_load_ps((float *)source0++);
            __m128 s1 = _mm_load_ps((float *)source1++);
            
            __m128i l = _mm_cvtps_epi32(s0);
            __m128i r = _mm_cvtps_epi32(s1);

            __m128i lr0 = _mm_unpacklo_epi32(l, r);
            __m128i lr1 = _mm_unpackhi_epi32(l, r);

            // NOTE: _mm_packs handles clamp operation
            __m128i s01 = _mm_packs_epi32(lr0, lr1);

            *sampleOut++ = s01;
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
