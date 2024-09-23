#pragma once

#include "../Base/Object.h"

#include <Containers/StringView.h>
#include <IO/Stream.h>

using namespace Death::Containers;

namespace nCine
{
	class IAudioLoader;

	/// A class representing an OpenAL buffer
	/*! It inherits from `Object` because a buffer can be
	 *  shared by more than one `AudioBufferPlayer` object. */
	class AudioBuffer : public Object
	{
	public:
		enum class Format {
			Mono8,
			Stereo8,
			Mono16,
			Stereo16
		};

		/// Creates an OpenAL buffer name
		AudioBuffer();
		/// A constructor creating a buffer from memory
		//AudioBuffer(const unsigned char* bufferPtr, unsigned long int bufferSize);
		/// A constructor creating a buffer from a file
		explicit AudioBuffer(StringView filename);
		AudioBuffer(std::unique_ptr<Death::IO::Stream> fileHandle, StringView filename);
		~AudioBuffer() override;

		AudioBuffer(const AudioBuffer&) = delete;
		AudioBuffer& operator=(const AudioBuffer&) = delete;
		AudioBuffer(AudioBuffer&& other) noexcept;
		AudioBuffer& operator=(AudioBuffer&& other) noexcept;

		/// Initializes an empty buffer with the specified format and frequency
		void init(Format format, int frequency);

		//bool loadFromMemory(const unsigned char* bufferPtr, unsigned long int bufferSize);
		bool loadFromFile(StringView filename);
		bool loadFromStream(std::unique_ptr<Death::IO::Stream> fileHandle, StringView filename);
		/// Loads samples in raw PCM format from a memory buffer
		bool loadFromSamples(const unsigned char* bufferPtr, unsigned long int bufferSize);

		/// Returns the OpenAL buffer id
		inline unsigned int bufferId() const {
			return bufferId_;
		}

		/// Returns the number of bytes per sample
		inline int bytesPerSample() const {
			return bytesPerSample_;
		}
		/// Returns the number of audio channels
		inline int numChannels() const {
			return numChannels_;
		}
		/// Returns the samples frequency
		inline int frequency() const {
			return frequency_;
		}

		/// Returns number of samples
		inline unsigned long int numSamples() const {
			return numSamples_;
		}
		/// Returns the duration in seconds
		inline float duration() const {
			return duration_;
		}

		/// Returns the size of the buffer in bytes
		inline unsigned long bufferSize() const {
			return numSamples_ * numChannels_ * bytesPerSample_;
		}

		inline static ObjectType sType() {
			return ObjectType::AudioBuffer;
		}

	private:
		/// The OpenAL buffer id
		unsigned int bufferId_;

		/// Number of bytes per sample
		int bytesPerSample_;
		/// Number of channels
		int numChannels_;
		/// Samples frequency
		int frequency_;

		/// Number of samples
		unsigned long int numSamples_;
		/// Duration in seconds
		float duration_;

		/// Loads audio samples based on information from the audio loader and reader
		bool load(IAudioLoader& audioLoader);
	};
}
