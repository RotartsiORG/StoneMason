//
// Created by grant on 1/24/20.
//

#include "stms/audio.hpp"
#include "stb/stb_vorbis.c"
#include <iostream>
#include <stms/logging.hpp>

namespace stms {

    ALDevice defaultAlDevice;
    ALContext defaultAlContext(&defaultAlDevice, nullptr);

    bool handleAlError() {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            STMS_WARN("[** OpenAL ERROR **]: {}", error);
            return true;
        } else {
            return false;
        }
    }

    ALBuffer::ALBuffer(const char *filename) {
        len = stb_vorbis_decode_filename(filename, &channels, &sampleRate, &data);
        alGenBuffers(1, &id);

        if (channels > 1) {
            alBufferData(id, AL_FORMAT_STEREO16, (const void *) data, len * 2 * sizeof(short), sampleRate);
        } else {
            alBufferData(id, AL_FORMAT_MONO16, (const void *) data, len * sizeof(short), sampleRate);
        }
    }

    ALBuffer::~ALBuffer() {
        alDeleteBuffers(1, &id);
    }

    ALBuffer &ALBuffer::operator=(ALBuffer const &rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        alDeleteBuffers(1, &id);

        this->len = rhs.len;
        this->channels = rhs.channels;
        this->data = rhs.data;
        this->sampleRate = rhs.sampleRate;

        this->id = rhs.id;

        return *this;
    }

    ALBuffer::ALBuffer(ALBuffer &rhs) {
        *this = rhs;
    }


    ALSource::ALSource() {
        alGenSources(1, &id);
    }

    ALSource::~ALSource() {
        alDeleteSources(1, &id);
    }

    ALSource::ALSource(ALSource &rhs) {
        *this = rhs;
    }

    ALSource &ALSource::operator=(ALSource const &rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alDeleteSources(1, &id);

        this->id = rhs.id;

        return *this;
    }

    ALContext::ALContext(ALDevice *dev, ALCint *attribs) noexcept {
        id = alcCreateContext(dev->id, attribs);
        bind(); // Bind by default.
    }

    ALContext::~ALContext() {
        alcDestroyContext(id);
    }

    ALDevice::ALDevice(const ALCchar *devname) noexcept {
        id = alcOpenDevice(devname);
    }

    ALDevice::~ALDevice() {
        alcCloseDevice(id);
    }

    bool ALDevice::handleError() {
        ALCenum error = alcGetError(id);
        if (error != ALC_NO_ERROR) {
            STMS_WARN("[** ALC ERROR **] {}", error);
            return true;
        } else {
            return false;
        }
    }
}
