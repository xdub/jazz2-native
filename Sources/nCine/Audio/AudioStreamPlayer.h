#pragma once

#include "IAudioPlayer.h"
#include "AudioStream.h"

namespace nCine
{
	/// Audio stream player class
	class AudioStreamPlayer : public IAudioPlayer
	{
		DEATH_RUNTIME_OBJECT(IAudioPlayer);

	public:
		/// Default constructor
		AudioStreamPlayer();
		/// A constructor creating a player from a file
		explicit AudioStreamPlayer(const StringView& filename);
		~AudioStreamPlayer() override;

		/// Default move constructor
		AudioStreamPlayer(AudioStreamPlayer&&) = default;
		/// Default move assignment operator
		AudioStreamPlayer& operator=(AudioStreamPlayer&&) = default;

		//bool loadFromMemory(const unsigned char* bufferPtr, unsigned long int bufferSize);
		bool loadFromFile(const char* filename);

		inline unsigned int bufferId() const override {
			return audioStream_.bufferId();
		}

		inline int bytesPerSample() const override {
			return audioStream_.bytesPerSample();
		}
		inline int numChannels() const override {
			return audioStream_.numChannels();
		}
		inline int frequency() const override {
			return audioStream_.frequency();
		}

		inline unsigned long int numSamples() const override {
			return audioStream_.numSamples();
		}
		inline float duration() const override {
			return audioStream_.duration();
		}

		inline unsigned long bufferSize() const override {
			return audioStream_.bufferSize();
		}

		inline unsigned long int numStreamSamples() const {
			return audioStream_.numStreamSamples();
		}
		inline int streamBufferSize() const {
			return audioStream_.streamBufferSize();
		}

		void play() override;
		void pause() override;
		void stop() override;
		void setLooping(bool value) override;

		/// Updates the player state and the stream buffer queue
		void updateState() override;

		inline static ObjectType sType() {
			return ObjectType::AudioStreamPlayer;
		}

	private:
		AudioStream audioStream_;

		/// Deleted copy constructor
		AudioStreamPlayer(const AudioStreamPlayer&) = delete;
		/// Deleted assignment operator
		AudioStreamPlayer& operator=(const AudioStreamPlayer&) = delete;
	};
}
