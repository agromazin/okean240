#include "../dev/recordplayer.h"
#include <iostream>
#include <fstream>
#include <array>
#include <memory>
#include <functional>

#define CLOCK_FREQ 3000000.
#define FILE_SIZE_OFFSET 4
#define SEGMENT_WAVE_OFFSET 8
#define SEGMENT_INSIDE_WAVE_OFFSET 12



uint8_t SEGMENT_RIFF[4] = {'R', 'I', 'F', 'F'};
uint8_t SEGMENT_WAVE[4] = { 'W', 'A', 'V', 'E' };
uint8_t SEGMENT_FMT[4] = { 'f', 'm', 't', ' ' };
uint8_t SEGMENT_DATA[4] = { 'd', 'a', 't', 'a' };

RecordPlayer::RecordPlayer() {
	m_status = PlaySatus::IDLE;
	m_data = nullptr;
}

RecordPlayer::~RecordPlayer() {
	delete[] m_data;
}
void RecordPlayer::clock() {
	if (m_status == PlaySatus::PLAYING || m_status == PlaySatus::SILENCE) {
		if (newSample) {
			newSample = false;
			if (m_status == PlaySatus::SILENCE) {
				auto sample = getSample(m_currentIndex);
				if (abs(sample) > m_silence_level) {
					auto next = 0 - getSample(m_currentIndex + 1) - getSample(m_currentIndex + 2);
					m_status = PlaySatus::PLAYING;
					m_next = NEXT_VALUE::HIGH;
					if ((next - sample) < 0) {
						m_next = NEXT_VALUE::LOW;
					}
					firstAfterSilence = true;
				}
			} else {
				if (isSilence()) {
					m_status = PlaySatus::SILENCE;
				}
			}
			m_endHandler(m_status, m_next == NEXT_VALUE::HIGH ? 0 : 1);
		}
		m_current_time += 1.0;
		if (m_current_time > m_oneSampleTicks) {
			newSample = true;
			m_current_time -= m_oneSampleTicks;
			m_currentIndex++;
			auto sample = getSample(m_currentIndex);
			if (m_currentIndex >= m_frameCount) {
				m_status = PlaySatus::LOADED;
				m_endHandler(m_status, 0);
			}
			if (m_status == PlaySatus::PLAYING) {
				if (firstAfterSilence) {
					if (abs(sample) < m_silence_level) {
						firstAfterSilence = false;
					} else {
						sample = -sample;
					}
				}
				if (abs(sample) > m_silence_level) {
					if (sample > 0) {
						m_next = NEXT_VALUE::HIGH;
					} else {
						m_next = NEXT_VALUE::LOW;
					}
				}
			}
		}
	}
}

int32_t RecordPlayer::findSegment(uint32_t seg, uint8_t* data, uint32_t size ) {
	int32_t index = 0;
	while (index < size) {
		if (*((uint32_t*)&data[index]) == seg) {
			return index;
		}
		index += *((uint32_t*)&data[index + 4]) + 8;
	}
	return -1;
}

bool RecordPlayer::init(std::string fileName, Bus endHandler) {
	m_endHandler = endHandler;
	std::ifstream file(fileName, std::ios::in | std::ios::binary);

	if (file.is_open())
	{
		file.seekg(0, std::ios::end);
		m_dataLength = file.tellg();
		if (m_dataLength > 0) {
			file.seekg(0, std::ios::beg);
			m_data = new uint8_t[m_dataLength];
			file.read((std::ifstream::char_type *) m_data, (std::streamsize) m_dataLength);
			file.close();
			T_WAVHEADER *wav = (T_WAVHEADER*)(m_data);
			uint32_t name = *((uint32_t*)&SEGMENT_RIFF[0]);
			if (*((uint32_t*)m_data) == name) {
				m_dataSize = *(((uint32_t*)&m_data[FILE_SIZE_OFFSET]));
				name = *((uint32_t*)&SEGMENT_WAVE[0]);
				uint32_t nameSrc = *((uint32_t*)&m_data[SEGMENT_WAVE_OFFSET]);
				if (name == nameSrc) {
					auto index = findSegment(*((uint32_t*)&SEGMENT_FMT[0]), &m_data[SEGMENT_INSIDE_WAVE_OFFSET], m_dataSize - SEGMENT_INSIDE_WAVE_OFFSET);
					if (index >= 0) {
						T_FMTHEADER *fmt = (T_FMTHEADER*)(&m_data[SEGMENT_INSIDE_WAVE_OFFSET + index]);
						m_formatType = fmt->formatType;
						m_channels = fmt->channels;
						m_samplingRate = fmt->samplingRate;
						m_bitRate = fmt->bitRate;
						m_bytePerFrame = fmt->bytePerFrame;
						m_bitPerSample = fmt->bitPerSample;
						index = findSegment(*((uint32_t*)&SEGMENT_DATA[0]), &m_data[SEGMENT_INSIDE_WAVE_OFFSET], m_dataSize - SEGMENT_INSIDE_WAVE_OFFSET);
						if (index >= 0) {
							T_DATAHEADER *dataHeader = (T_DATAHEADER*)(&m_data[SEGMENT_INSIDE_WAVE_OFFSET + index]);
							m_samples = &dataHeader->data[0];
							m_dataSize = dataHeader->dataLength;
							m_frameCount = m_dataSize / m_bytePerFrame;
							m_silenceSamples = m_samplingRate / 200;
							calculateLevels();
							m_oneSampleTicks = (CLOCK_FREQ / m_samplingRate);// *1.5;
							m_status = PlaySatus::LOADED;
							return true;
						}
					}
				}

			}
		}
	}
	return false;
}

RecordPlayer::PlaySatus RecordPlayer::getRecordPlayerStatus(double_t &percent) {
	percent = 0;
	if (m_status != PlaySatus::IDLE) {
		percent = (double_t)m_currentIndex / (double_t)m_frameCount;
	}
	return m_status;
}

void RecordPlayer::start() {
	if (m_status == PlaySatus::LOADED) {
		m_currentIndex = 0;
		m_current_time = 0;
		newSample = true;
		firstAfterSilence = false;
		if (isSilence()) {
			m_status = PlaySatus::SILENCE;
		} else {
			m_status = PlaySatus::PLAYING;
		}
	}
}

bool RecordPlayer::isSilence() {
	uint32_t endIndex = m_currentIndex + m_silenceSamples;
	if (endIndex > m_frameCount) {
		endIndex = m_frameCount;
	}
	auto level = m_silence_level;// *3;
	for (uint32_t i = m_currentIndex; i < endIndex; i++) {
		if (abs(getSample(i)) > level) {
			return false;
		}
	}
	return true;
}

void RecordPlayer::calculateLevels() {
	double_t last_value = 0;
	double_t max_value = 0;
	double_t local_max_value = 0;
	for (uint32_t i = 0; i < m_frameCount; i++) {
		auto v = getSample(i);
		if (abs(v) > max_value) {
			max_value = v;
		}
	}
	m_one_level = max_value / 100 * 50;
	m_zero_level = -max_value / 100 * 30;
	m_silence_level = abs(max_value) / 100 * 20;
}

double_t RecordPlayer::getSample(uint32_t i) {
	if (m_channels == 1) {
		if (m_bytePerFrame == 2) {
			int16_t *k = (int16_t*)(m_samples + i * 2);
			return *k;
		} else {
			int8_t *k = (int8_t*)m_samples;
			return k[i];
		}
	} else {
		if (m_bytePerFrame == 4) {
			int16_t *k1 = (int16_t*)(m_samples + i * 4);
			int16_t *k2 = (int16_t*)(m_samples + i * 4 + 2);
			return *k1;
		}
		else {
			int8_t *k = (int8_t*)m_samples;
			auto j = i * 2;
			return k[j++];
		}
	}
}