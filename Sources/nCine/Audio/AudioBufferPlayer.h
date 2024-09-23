#pragma once

#include "IAudioPlayer.h"

namespace nCine
{
	class AudioBuffer;

	/// Audio buffer player class
	class AudioBufferPlayer : public IAudioPlayer
	{
		DEATH_RUNTIME_OBJECT(IAudioPlayer);

	public:
		/// Default constructor
		AudioBufferPlayer();
		/// A constructor creating a player from a shared buffer
		explicit AudioBufferPlayer(AudioBuffer* audioBuffer);
		~AudioBufferPlayer() override;

		/// Default move constructor
		AudioBufferPlayer(AudioBufferPlayer&&) = default;
		/// Default move assignment operator
		AudioBufferPlayer& operator=(AudioBufferPlayer&&) = default;

		unsigned int bufferId() const override;

		int bytesPerSample() const override;
		int numChannels() const override;
		int frequency() const override;

		unsigned long int numSamples() const override;
		float duration() const override;

		unsigned long int bufferSize() const override;

		/// Gets the audio buffer used for playing
		inline const AudioBuffer* audioBuffer() const {
			return audioBuffer_;
		}
		/// Sets the audio buffer used for playing
		void setAudioBuffer(AudioBuffer* audioBuffer);

		void play() override;
		void pause() override;
		void stop() override;

		/// Updates the player state
		void updateState() override;

		inline static ObjectType sType() {
			return ObjectType::AudioBufferPlayer;
		}

	private:
		AudioBuffer* audioBuffer_;

		/// Deleted copy constructor
		AudioBufferPlayer(const AudioBufferPlayer&) = delete;
		/// Deleted assignment operator
		AudioBufferPlayer& operator=(const AudioBufferPlayer&) = delete;
	};
}
