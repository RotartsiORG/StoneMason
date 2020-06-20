//
// Created by grant on 1/24/20.
//

#include "stms/audio.hpp"
#include "stb/stb_vorbis.c"
#include <stms/logging.hpp>

namespace stms::al {

    ALenum handleAlError() {
        ALenum error = alGetError();
        if (error != AL_NO_ERROR) {
            STMS_PUSH_ERROR("[** OpenAL ERROR **]: {}", error);
        }
        return error;
    }

    ALBuffer::ALBuffer() {
        alGenBuffers(1, &id);
    }

    ALBuffer::~ALBuffer() {
        alDeleteBuffers(1, &id);
    }

    ALBuffer &ALBuffer::operator=(ALBuffer &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        alDeleteBuffers(1, &id);
        this->id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    ALBuffer::ALBuffer(ALBuffer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void ALBuffer::loadFromFile(const char *filename) const {
        short *data;
        int channels;
        int sampleRate;

        int len = stb_vorbis_decode_filename(filename, &channels, &sampleRate, &data);
        if (len == 0 || channels == 0 || sampleRate == 0 || data == nullptr) {
            STMS_PUSH_ERROR("Failed to load ALBuffer audio from {}!", filename);
            return;
        }

        if (channels > 1) {
            len *= 2 * sizeof(short);
            alBufferData(id, AL_FORMAT_STEREO16, data, len, sampleRate);
        } else {
            len *= sizeof(short);
            alBufferData(id, AL_FORMAT_MONO16, data, len, sampleRate);
        }

        free(data); // !VERY IMPORTANT!
    }

    ALBuffer::ALBuffer(const char *filename) {
        alGenBuffers(1, &id);
        loadFromFile(filename);
    }


    ALSource::ALSource() {
        alGenSources(1, &id);
    }

    ALSource::~ALSource() {
        alDeleteSources(1, &id);
    }

    ALSource::ALSource(ALSource &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALSource &ALSource::operator=(ALSource &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alDeleteSources(1, &id);

        this->id = rhs.id;
        rhs.id = 0;

        return *this;
    }

    ALContext::~ALContext() {
        alcDestroyContext(id);
    }

    ALContext::ALContext(ALContext &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALContext &ALContext::operator=(ALContext &&rhs) noexcept {
        if (&rhs == this) {
            return *this;
        }

        alcDestroyContext(this->id);

        this->id = rhs.id;
        rhs.id = nullptr;

        return *this;
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
            STMS_PUSH_ERROR("[** ALC ERROR **] {}", error);
            return true;
        } else {
            return false;
        }
    }

    ALDevice::ALDevice(ALDevice &&rhs) noexcept {
        *this = std::move(rhs);
    }

    ALDevice &ALDevice::operator=(ALDevice &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        alcCloseDevice(this->id);

        this->id = rhs.id;
        rhs.id = nullptr;

        return *this;
    }

    ALContext::ALContext(ALDevice *dev, ALCint *attribs) {
        id = alcCreateContext(dev->id, attribs);
    }
}
