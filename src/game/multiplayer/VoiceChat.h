#pragma once

#include "NetworkRuntime.h"
#include "opus.h"
#include <deque>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

class cOpusCodec
{
    OpusEncoder* _encoder;
    OpusDecoder* _decoder;

public:
    cOpusCodec()
        : _encoder(NULL),
          _decoder(NULL)
    {
        __int32 error = 0;
        _encoder = opus_encoder_create(VOICE_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &error);
        if (error != 0 || !_encoder)
        {
            return;
        }
        _decoder = opus_decoder_create(VOICE_SAMPLE_RATE, 1, &error);
        if (error != 0 || !_decoder)
        {
            opus_encoder_destroy(_encoder);
            _encoder = NULL;
            return;
        }
        opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(20000));
        opus_encoder_ctl(_encoder, OPUS_SET_COMPLEXITY(4));
    }

    ~cOpusCodec()
    {
        if (_encoder)
        {
            opus_encoder_destroy(_encoder);
        }
        if (_decoder)
        {
            opus_decoder_destroy(_decoder);
        }
    }

    bool available() const
    {
        return _encoder != NULL && _decoder != NULL;
    }

    bool encode(const __int16* samples, vector<unsigned char>& encoded)
    {
        if (!available())
        {
            return false;
        }
        encoded.resize(VOICE_MAX_OPUS_BYTES);
        __int32 bytes = opus_encode(_encoder, samples, VOICE_SAMPLES_PER_PACKET, &encoded[0], VOICE_MAX_OPUS_BYTES);
        if (bytes <= 0)
        {
            encoded.clear();
            return false;
        }
        encoded.resize(bytes);
        return true;
    }

    bool decode(const NetworkVoicePacket& packet, vector<__int16>& samples)
    {
        if (!available() || packet.codec != VOICE_CODEC_OPUS || packet.data.empty())
        {
            return false;
        }
        samples.resize(VOICE_SAMPLES_PER_PACKET);
        __int32 decoded = opus_decode(_decoder, &packet.data[0], (__int32)packet.data.size(), &samples[0], VOICE_SAMPLES_PER_PACKET, 0);
        if (decoded <= 0)
        {
            samples.clear();
            return false;
        }
        samples.resize(decoded);
        return true;
    }
};

class cVoiceChat
{
    struct CaptureBuffer
    {
        WAVEHDR header;
        __int16 samples[VOICE_SAMPLES_PER_PACKET];
    };

    struct PlaybackBuffer
    {
        WAVEHDR header;
        vector<__int16> samples;
    };

    HWAVEIN _waveIn;
    HWAVEOUT _waveOut;
    CaptureBuffer _captureBuffers[4];
    std::deque<PlaybackBuffer*> _playbackBuffers;
    CRITICAL_SECTION _queueLock;
    std::deque<vector<unsigned char>> _captureQueue;
    cOpusCodec _opus;
    bool _recording;
    bool _transmitEnabled;
    bool _toggleWasDown;
    bool _captureReady;
    bool _playbackReady;

    static void CALLBACK CaptureCallback(HWAVEIN waveIn, UINT message, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR)
    {
        cVoiceChat* self = reinterpret_cast<cVoiceChat*>(instance);
        if (!self || message != WIM_DATA)
        {
            return;
        }

        WAVEHDR* header = reinterpret_cast<WAVEHDR*>(param1);
        if (self->_recording && header->dwBytesRecorded >= sizeof(__int16) * VOICE_SAMPLES_PER_PACKET)
        {
            vector<unsigned char> encoded;
            if (self->_opus.encode(reinterpret_cast<const __int16*>(header->lpData), encoded))
            {
                EnterCriticalSection(&self->_queueLock);
                self->_captureQueue.push_back(std::move(encoded));
                while (self->_captureQueue.size() > 16)
                {
                    self->_captureQueue.pop_front();
                }
                LeaveCriticalSection(&self->_queueLock);
            }
        }

        if (self->_recording)
        {
            header->dwBytesRecorded = 0;
            waveInAddBuffer(waveIn, header, sizeof(WAVEHDR));
        }
    }

    void openDevices()
    {
        WAVEFORMATEX format;
        ZeroMemory(&format, sizeof(format));
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = VOICE_SAMPLE_RATE;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        _captureReady = waveInOpen(&_waveIn, WAVE_MAPPER, &format, (DWORD_PTR)CaptureCallback, (DWORD_PTR)this, CALLBACK_FUNCTION) == MMSYSERR_NOERROR;
        if (_captureReady)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                ZeroMemory(&_captureBuffers[i], sizeof(_captureBuffers[i]));
                _captureBuffers[i].header.lpData = reinterpret_cast<LPSTR>(_captureBuffers[i].samples);
                _captureBuffers[i].header.dwBufferLength = sizeof(_captureBuffers[i].samples);
                waveInPrepareHeader(_waveIn, &_captureBuffers[i].header, sizeof(WAVEHDR));
            }
        }

        _playbackReady = waveOutOpen(&_waveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR;
    }

    void startRecording()
    {
        if (!_captureReady || _recording)
        {
            return;
        }
        if (_playbackReady)
        {
            waveOutReset(_waveOut);
            cleanupPlaybackBuffers(true);
        }
        _recording = true;
        for (size_t i = 0; i < 4; ++i)
        {
            _captureBuffers[i].header.dwBytesRecorded = 0;
            waveInAddBuffer(_waveIn, &_captureBuffers[i].header, sizeof(WAVEHDR));
        }
        waveInStart(_waveIn);
    }

    void stopRecording()
    {
        if (!_captureReady || !_recording)
        {
            return;
        }
        _recording = false;
        waveInStop(_waveIn);
        waveInReset(_waveIn);
    }

    bool popCapturedFrame(vector<unsigned char>& encoded)
    {
        bool result = false;
        EnterCriticalSection(&_queueLock);
        if (!_captureQueue.empty())
        {
            encoded = std::move(_captureQueue.front());
            _captureQueue.pop_front();
            result = true;
        }
        LeaveCriticalSection(&_queueLock);
        return result;
    }

    void playPacket(const NetworkVoicePacket& packet)
    {
        if (!_playbackReady)
        {
            return;
        }

        PlaybackBuffer* buffer = new PlaybackBuffer;
        ZeroMemory(&buffer->header, sizeof(buffer->header));
        _opus.decode(packet, buffer->samples);
        if (buffer->samples.empty())
        {
            delete buffer;
            return;
        }
        buffer->header.lpData = reinterpret_cast<LPSTR>(&buffer->samples[0]);
        buffer->header.dwBufferLength = (__int32)(buffer->samples.size() * sizeof(__int16));
        buffer->header.dwUser = reinterpret_cast<DWORD_PTR>(buffer);

        if (waveOutPrepareHeader(_waveOut, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            delete buffer;
            return;
        }
        if (waveOutWrite(_waveOut, &buffer->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            waveOutUnprepareHeader(_waveOut, &buffer->header, sizeof(WAVEHDR));
            delete buffer;
            return;
        }
        _playbackBuffers.push_back(buffer);
        while (_playbackBuffers.size() > 48)
        {
            PlaybackBuffer* old = _playbackBuffers.front();
            _playbackBuffers.pop_front();
            waveOutReset(_waveOut);
            waveOutUnprepareHeader(_waveOut, &old->header, sizeof(WAVEHDR));
            delete old;
        }
    }

    void cleanupPlaybackBuffers(bool force = false)
    {
        while (!_playbackBuffers.empty())
        {
            PlaybackBuffer* buffer = _playbackBuffers.front();
            if (!force && !(buffer->header.dwFlags & WHDR_DONE))
            {
                break;
            }
            _playbackBuffers.pop_front();
            waveOutUnprepareHeader(_waveOut, &buffer->header, sizeof(WAVEHDR));
            delete buffer;
        }
    }

public:
    cVoiceChat()
        : _waveIn(NULL),
          _waveOut(NULL),
          _recording(false),
          _transmitEnabled(false),
          _toggleWasDown(false),
          _captureReady(false),
          _playbackReady(false)
    {
        InitializeCriticalSection(&_queueLock);
        openDevices();
    }

    ~cVoiceChat()
    {
        stopRecording();
        if (_captureReady)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                waveInUnprepareHeader(_waveIn, &_captureBuffers[i].header, sizeof(WAVEHDR));
            }
            waveInClose(_waveIn);
        }
        if (_playbackReady)
        {
            waveOutReset(_waveOut);
            cleanupPlaybackBuffers(true);
            waveOutClose(_waveOut);
        }
        DeleteCriticalSection(&_queueLock);
    }

    void update(Game::Multiplayer::NetworkRuntime& network, bool talkKeyDown, bool voiceEnabled,
                bool pushToTalk)
    {
        if (!voiceEnabled)
        {
            _transmitEnabled = false;
            _toggleWasDown = talkKeyDown;
            stopRecording();
            NetworkVoicePacket packet;
            while (network.PollVoice(packet))
            {
            }
            cleanupPlaybackBuffers(true);
            return;
        }

        if (pushToTalk)
        {
            _transmitEnabled = talkKeyDown;
        }
        else if (talkKeyDown && !_toggleWasDown)
        {
            _transmitEnabled = !_transmitEnabled;
        }
        _toggleWasDown = talkKeyDown;

        if (_transmitEnabled)
        {
            startRecording();
        }
        else
        {
            stopRecording();
        }

        vector<unsigned char> outgoing;
        while (popCapturedFrame(outgoing))
        {
            network.SendVoiceFrame(std::move(outgoing));
        }
        NetworkVoicePacket incoming;
        while (network.PollVoice(incoming))
        {
            playPacket(incoming);
        }
        cleanupPlaybackBuffers();
    }
};
