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

    Assert((soundBuffer->sampleCount & 7) == 0);
    ui32 sampleCount8 = soundBuffer->sampleCount / 8;
    ui32 sampleCount4 = soundBuffer->sampleCount / 4;
    
    __m128 *realChannel0 = PushArray(tempArena, sampleCount4, __m128, 16);
    __m128 *realChannel1 = PushArray(tempArena, sampleCount4, __m128, 16);

    r32 secondsPerSample = 1.0f / (r32)soundBuffer->samplesPerSecond;
#define AudioStateOutputChannelCount 2
    
    // NOTE: Clear out the mixer channels
    __m128 zero = _mm_set1_ps(0.0f);
   {
        __m128 *dest0 = realChannel0;
        __m128 *dest1 = realChannel1;
        for(ui32 sampleIndex = 0;
            sampleIndex < sampleCount4;
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

        ui32 totalSamplesToMix8 = sampleCount8;
        b32 soundFinished = false;
        while(totalSamplesToMix8 && !soundFinished)
        {
            loaded_sound *loadedSound = GetSound(assets, playingSound->id);
            if(loadedSound)
            {
                asset_sound_info *info = GetSoundInfo(assets, playingSound->id);
                PrefetchSound(assets, info->nextIDToPlay);
            
                v2 volume = playingSound->currentVolume;
                v2 dVolume = secondsPerSample * playingSound->dCurrentVolume;
                v2 dVolume8 = 8.0f * dVolume;
                r32 dSample = playingSound->dSample;
                r32 dSample8 = 8.0f * dSample;

                __m128 masterVolume4_0 = _mm_set1_ps(audioState->masterVolume.e[0]);
                __m128 masterVolume4_1 = _mm_set1_ps(audioState->masterVolume.e[1]);
                
                __m128 volume4_0 = _mm_set_ps(volume.e[0] + 0.0f * dVolume.e[0],
                                              volume.e[0] + 1.0f * dVolume.e[0],
                                              volume.e[0] + 2.0f * dVolume.e[0],
                                              volume.e[0] + 3.0f * dVolume.e[0]);
                __m128 dVolume4_0 = _mm_set1_ps(dVolume.e[0]);
                __m128 dVolume84_0 = _mm_set1_ps(dVolume8.e[0]);
                
                __m128 volume4_1 = _mm_set_ps(volume.e[1] + 0.0f * dVolume.e[1],
                                              volume.e[1] + 1.0f * dVolume.e[1],
                                              volume.e[1] + 2.0f * dVolume.e[1],
                                              volume.e[1] + 3.0f * dVolume.e[1]);
                __m128 dVolume4_1 = _mm_set1_ps(dVolume.e[1]);
                __m128 dVolume84_1 = _mm_set1_ps(dVolume8.e[1]);

                Assert(playingSound->samplesPlayed >= 0.0f);

                ui32 samplesToMix8 = totalSamplesToMix8;
                r32 realSamplesRemainingInSound8 = (loadedSound->sampleCount - RoundR32ToI32(playingSound->samplesPlayed)) / dSample8;
                ui32 samplesRemainingInSound8 = RoundR32ToI32(realSamplesRemainingInSound8);
                if(samplesToMix8 > samplesRemainingInSound8)
                {
                    samplesToMix8 = samplesRemainingInSound8;
                }

                b32 volumeEnded[AudioStateOutputChannelCount] = {};
                for(ui32 channelIndex = 0; channelIndex < ArrayCount(volumeEnded); channelIndex++)
                {
                    // TODO: Fix the "both volumes end at the same time" bug
                    if(dVolume8.e[channelIndex] != 0.0f)
                    {
                        r32 deltaVolume = (playingSound->targetVolume.e[channelIndex] - volume.e[channelIndex]);
                        ui32 volumeSampleCount8 = (ui32)((deltaVolume / dVolume8.e[channelIndex]) + 0.5f);
                        if(samplesToMix8 > volumeSampleCount8)
                        {
                            samplesToMix8 = volumeSampleCount8;
                            volumeEnded[channelIndex] = true;
                        }
                    }
                }
                
                // TODO: Handle stereo!
                r32 samplePosition = playingSound->samplesPlayed;
                for(ui32 i = 0; i < samplesToMix8; i++)
                {
#if 0
                    r32 offsetSamplePosition = samplePosition + (r32)sampleOffset * dSample;
                    ui32 sampleIndex = FloorR32ToI32(offsetSamplePosition);
                    r32 frac = offsetSamplePosition - (r32)sampleIndex;
                        
                    r32 sample0 = (r32)loadedSound->samples[0][sampleIndex];
                    r32 sample1 = (r32)loadedSound->samples[0][sampleIndex + 1];
                    r32 sampleValue = Lerp(sample0, frac, sample1);
#else               
                    __m128 sampleValue_0 = _mm_setr_ps(loadedSound->samples[0][RoundR32ToI32(samplePosition + 0.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 1.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 2.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 3.0f * dSample)]);
                    
                    __m128 sampleValue_1 = _mm_setr_ps(loadedSound->samples[0][RoundR32ToI32(samplePosition + 4.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 5.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 6.0f * dSample)],
                                                       loadedSound->samples[0][RoundR32ToI32(samplePosition + 7.0f * dSample)]);
#endif

                    __m128 d0_0 = _mm_load_ps((float *)&dest0[0]);
                    __m128 d0_1 = _mm_load_ps((float *)&dest0[1]);
                    __m128 d1_0 = _mm_load_ps((float *)&dest1[0]);
                    __m128 d1_1 = _mm_load_ps((float *)&dest1[1]);

                    d0_0 = _mm_add_ps(d0_0, _mm_mul_ps(_mm_mul_ps(masterVolume4_0, volume4_0), sampleValue_0));
                    d0_1 = _mm_add_ps(d0_1, _mm_mul_ps(_mm_mul_ps(masterVolume4_0, _mm_add_ps(dVolume4_0, volume4_0)), sampleValue_1));
                    d1_0 = _mm_add_ps(d1_0, _mm_mul_ps(_mm_mul_ps(masterVolume4_1, volume4_1), sampleValue_0));
                    d1_1 = _mm_add_ps(d1_1, _mm_mul_ps(_mm_mul_ps(masterVolume4_1, _mm_add_ps(dVolume4_1, volume4_1)), sampleValue_1));

                    _mm_store_ps((float *)&dest0[0], d0_0);
                    _mm_store_ps((float *)&dest0[1], d0_1);
                    _mm_store_ps((float *)&dest1[0], d1_0);
                    _mm_store_ps((float *)&dest1[1], d1_1);

                    dest0 += 2;
                    dest1 += 2;
                    volume4_0 = _mm_add_ps(volume4_0, dVolume84_0);
                    volume4_1 = _mm_add_ps(volume4_1, dVolume84_1);
                    volume += dVolume8;
                    samplePosition += dSample8;
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

                playingSound->samplesPlayed = samplePosition;
                Assert(totalSamplesToMix8 >= samplesToMix8);
                totalSamplesToMix8 -= samplesToMix8;

                if((ui32)playingSound->samplesPlayed >= loadedSound->sampleCount)
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
        __m128 *source0 = realChannel0;
        __m128 *source1 = realChannel1;
    
        __m128i *sampleOut = (__m128i *)soundBuffer->samples;
        for(ui32 sampleIndex = 0; sampleIndex < sampleCount4; sampleIndex++)
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
