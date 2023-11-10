#pragma once

#include <fstream>
#include <array>
#include <memory>
#include <functional>

#include "../cpu/IO.hpp"
#include "../cpu/cpu.hpp"
#include "../bus/periphery.h"
#include "../dev/recordplayer.h"

#define BLOCK_SIZE 16384
//#define WRITE_WAV


class OkeanBus : public IO<uint8_t>, public std::enable_shared_from_this<OkeanBus> {
public:
	enum class EventType { VIDEO, RECORD_PLAYER };
	using BusEvent = std::function< void(EventType type) >;
	OkeanBus();
	virtual ~OkeanBus() = default;
	void execute(BusEvent handler);
	void clock();
	void load(std::string motitor_path, std::string cpm_path);
	virtual uint8_t read(uint8_t address) const override;
	virtual void write(uint8_t address, uint8_t data) override;
	bool isColorDisplay();
	void createBitmap(uint32_t *bitmapArray);
	void loadFileToRecordPlayer(std::string filename);
	void startRecordPlayer();
	RecordPlayer::PlaySatus getRecordPlayerStatus(double_t &percent);
private:
	uint8_t ports[256];
	std::shared_ptr<K580VI53> m_k580vi53;
	std::shared_ptr<K580VN59> m_k580vn59;
	std::shared_ptr<K580VV55> m_k580vv55_keyb;
	std::shared_ptr<RecordPlayer> m_recordPlayer;


	enum class MemoryType { MONITOR, OS, VIDEO };

	enum MemoryMapTypeValue {
		INIT = 0x20,
		NORM = 0x00,
		VIDEO = 0x10,
		VIDEO1 = 0x01,
		EXT_RAM1 = 0x02,
		EXT_RAM2 = 0x03,
		EXT_RAM3 = 0x12
	};

	class RAM : public IO<uint16_t>
	{
	private:
		uint8_t* ram_map[4];
		uint8_t ROM[BLOCK_SIZE];
		uint8_t* OS = &ROM[0];		
		uint8_t* MONITOR = &ROM[BLOCK_SIZE/2];
		uint8_t VIDEO_RAM[BLOCK_SIZE];
		uint8_t NORM_RAM[BLOCK_SIZE * 3];
		uint8_t EXT_RAM[BLOCK_SIZE * 4];
		friend class OkeanBus;
	private:
		uint8_t* getMemoryBlock(MemoryType type);
		void configureMAP(uint8_t data);
		std::weak_ptr<OkeanBus> m_bus;
	public:
		RAM(std::weak_ptr<OkeanBus> bus);
		virtual ~RAM() = default;
		virtual uint8_t read(uint16_t address) const override;
		virtual void write(uint16_t address, uint8_t data) override;
	};

	std::shared_ptr<RAM> ram;
	std::shared_ptr<Cpu> cpu;

	void loadData(std::string path, uint8_t* data, uint16_t size);
	uint8_t* getVideoRAM();
	BusEvent m_handler;
	void createColorBitmap(uint32_t *bitmapArray);
	void createMonoBitmap(uint32_t *bitmapArray);
};
