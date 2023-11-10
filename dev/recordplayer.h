#pragma once
#include <fstream>
#include <array>
#include <memory>
#include <functional>

typedef struct {
	uint8_t SEGMENT_RIFF[4];
	uint32_t fileLength;
	uint8_t WAVE[4];
} T_WAVHEADER;

typedef struct {
	uint8_t SEGMENT_FMT[4];
	uint32_t size;
	uint16_t formatType;
	uint16_t channels;
	uint32_t samplingRate;
	uint32_t bitRate;
	uint16_t bytePerFrame;
	uint16_t bitPerSample;
} T_FMTHEADER;

typedef struct {
	uint8_t SEGMENT_DATA[4];
	uint32_t dataLength;
	uint8_t data[1];
} T_DATAHEADER;


class RecordPlayer {
public:
	enum class PlaySatus { IDLE, LOADED, PLAYING, SILENCE };
	using Bus = std::function<void(RecordPlayer::PlaySatus status, uint8_t value)>;
	RecordPlayer();
	virtual ~RecordPlayer();
	void clock();
	bool init(std::string fileName, Bus endHandler);
	void start();
	RecordPlayer::PlaySatus getRecordPlayerStatus(double_t &percent);
private:
	bool isSilence();
	void calculateLevels();
	double_t getSample(uint32_t i);
	int32_t findSegment(uint32_t seg, uint8_t* data, uint32_t size);
private:
	enum class NEXT_VALUE { LOW, HIGH };
	Bus m_endHandler;
	double_t m_current_time;
	double_t m_oneSampleTicks;
	uint32_t m_currentSample;
	PlaySatus m_status, m_status_old;
	uint8_t m_value;
	uint8_t *m_data;
	int32_t m_dataLength;
	uint32_t m_currentIndex;
	double_t m_one_level;
	double_t m_zero_level;	
	double_t m_silence_level;
	uint32_t m_frameCount;
	uint16_t m_formatType;
	uint16_t m_channels;
	uint32_t m_samplingRate;
	uint32_t m_bitRate;
	uint16_t m_bytePerFrame;
	uint16_t m_bitPerSample;
	uint8_t *m_samples;
	uint32_t m_dataSize;
	uint32_t m_silenceSamples;
	NEXT_VALUE m_next;
	double_t m_max;
	bool newSample;
	bool firstAfterSilence;
};