#include "okeanBus.h"
#include "../bus/periphery.h"

#define MEMORY_MAP_MASK 0x33

#include <thread>
#ifdef _PTHREAD
#include "pthread.h"
#endif

#ifdef WRITE_WAV
std::shared_ptr<std::fstream> fileOut;
#endif

std::weak_ptr<OkeanBus> sbus;

//extern std::shared_ptr<std::fstream> fileOutVI53;
//bool flagWrite = false;
//uint16_t lastCounter;
OkeanBus::OkeanBus() {
	for (int i = 0; i < sizeof(ports); i++) {
		ports[i] = 0;
	}
}

OkeanBus::RAM::RAM(std::weak_ptr<OkeanBus> bus) {
	m_bus = bus;
}

uint8_t* OkeanBus::RAM::getMemoryBlock(OkeanBus::MemoryType type) {
	switch (type) {
	case OkeanBus::MemoryType::MONITOR:
		return MONITOR;
	case OkeanBus::MemoryType::OS:
		return OS;
	case OkeanBus::MemoryType::VIDEO:
		return VIDEO_RAM;
	}
	return nullptr;
}

uint8_t OkeanBus::RAM::read(uint16_t address) const {
	return ram_map[address >> 14][address & 0x3FFF];
}

void OkeanBus::RAM::write(uint16_t address, uint8_t data) {
	uint16_t int_addr = address & 0x3FFF;
	uint16_t index = address >> 14;
	ram_map[index][int_addr] = data;
	if (ram_map[index] == VIDEO_RAM) {
		if (auto bus = m_bus.lock()) {
			bus->m_handler(OkeanBus::EventType::VIDEO);
		}
	}
}

void OkeanBus::RAM::configureMAP(uint8_t data) {
	if ((data & MEMORY_MAP_MASK) >= MemoryMapTypeValue::INIT) {
		ram_map[0] = OS;
		ram_map[1] = OS;
		ram_map[2] = OS;
		ram_map[3] = OS;
		return;
	}
	auto type = data & MEMORY_MAP_MASK;
	switch (type) {
	case MemoryMapTypeValue::NORM:
		ram_map[0] = &NORM_RAM[0];
		ram_map[1] = &NORM_RAM[BLOCK_SIZE];
		ram_map[2] = &NORM_RAM[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::VIDEO:
		ram_map[0] = &NORM_RAM[0];
		ram_map[1] = &NORM_RAM[BLOCK_SIZE];
		ram_map[2] = &NORM_RAM[BLOCK_SIZE * 2];
		ram_map[3] = VIDEO_RAM;
		break;
	case MemoryMapTypeValue::VIDEO1:
		ram_map[0] = &NORM_RAM[BLOCK_SIZE * 2];
		ram_map[1] = VIDEO_RAM;
		ram_map[2] = &NORM_RAM[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT_RAM1:
		ram_map[0] = &EXT_RAM[0];
		ram_map[1] = &EXT_RAM[BLOCK_SIZE];
		ram_map[2] = &EXT_RAM[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT_RAM2:
		ram_map[0] = &EXT_RAM[BLOCK_SIZE * 2];
		ram_map[1] = &EXT_RAM[BLOCK_SIZE * 3];
		ram_map[2] = &EXT_RAM[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT_RAM3:
		ram_map[0] = &EXT_RAM[0];
		ram_map[1] = &EXT_RAM[BLOCK_SIZE];
		ram_map[2] = &EXT_RAM[BLOCK_SIZE * 2];
		ram_map[3] = &EXT_RAM[BLOCK_SIZE * 3];
		break;
	}
}

uint8_t* OkeanBus::getVideoRAM() {
	return ram->VIDEO_RAM;
}

uint8_t OkeanBus::read(uint8_t address) const {
	switch (address) {
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
		return m_k580vi53->read(address);
	case 0x80:
	case 0x81:
		return m_k580vn59->read(address);
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
		return m_k580vv55_keyb->read(address);
	}
	return ports[address];
}

void OkeanBus::write(uint8_t address, uint8_t data) {
	switch (address) {
	case 0xC1:
		ram->configureMAP(data);
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
		m_k580vi53->write(address, data);
		return;
	case 0x80:
	case 0x81:
		m_k580vn59->write(address, data);
		return;
	case 0xFF:
		m_k580vn59->write(address, data);
		ports[address] = data;
		return;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
		m_k580vv55_keyb->write(address, data);
		break;
	}
	ports[address] = data;
}

void OkeanBus::clock() {
/*	if (flagWrite) {
		auto count = cpu->getCounter();
		if (count != lastCounter) {
			lastCounter = count;
			char vv[10];
			sprintf_s(vv, 10, "ad %4X\n", lastCounter);
			fileOutVI53->write((const char*)(&vv[0]), 9);
		}
	}
*/
	m_k580vi53->clock();
	if (m_recordPlayer != nullptr) {
		m_recordPlayer->clock();
	}
	cpu->clock();
}

bool OkeanBus::isColorDisplay() {
	uint8_t c = read(0xE1) & 0x40;
	return c == 0x40;
}

uint32_t paletteColor[8][4] = { 
							  { 0x00000000, 0x00FF0000, 0x0000FF00, 0x000000FF }, 
							  { 0x00FFFFFF, 0x00FF0000, 0x0000FF00, 0x000000FF },
							  { 0x00FF0000, 0x0000FF00, 0x0000BFFF, 0x00FFFF00 },
							  { 0x00000000, 0x00FF0000, 0x00FF00FF, 0x00FFFFFF },
							  { 0x00000000, 0x00FF0000, 0x00FFFF00, 0x000000FF },
							  { 0x00000000, 0x000000FF, 0x0000FF00, 0x00FFFF00 },
							  { 0x0000FF00, 0x00FFFFFF, 0x00FFFF00, 0x000000FF },
							  { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
							};
uint32_t paletteMono[] = { 0x00FFFFFF, 0x00000000 };

void OkeanBus::createColorBitmap(uint32_t *bitmapArray) {
	uint8_t pal = read(0xE1) & 0x07;
	uint8_t mask = 1;
	uint16_t updated_offset = read(0xC0);
	uint16_t item = 0;
	for (uint16_t i = 0; i < 256; i++) {
		auto videoram1 = getVideoRAM() + i;
		auto videoram2 = videoram1 + 0x100;
		int16_t y = i - updated_offset;
		if (y < 0) { y = y + 256; }
		item = y * 256;
		for (uint16_t j = 0; j < 256; j++) {
			uint8_t cl = (*videoram1 & mask) ? 2 : 0;
			cl |= (*videoram2 & mask) ? 1 : 0;
			if (mask != 0x80) {
				mask = mask << 1;
			}
			else {
				videoram1 += 0x200;
				videoram2 += 0x200;
				mask = 1;
			}
			bitmapArray[item++] = paletteColor[pal][cl];
		}
	}
}

void OkeanBus::createMonoBitmap(uint32_t *bitmapArray) {
	uint8_t mask = 1;
	uint16_t updated_offset = read(0xC0);
	uint32_t item = 0;
	for (uint16_t i = 0; i < 256; i++) {
		auto videoram = getVideoRAM() + i;
		int16_t y = i - updated_offset;
		if (y < 0) { y = y + 256; }
		item = y * 512;
		for (uint16_t j = 0; j < 512; j++) {
			uint8_t cl = (*videoram & mask) ? 1 : 0;
			if (mask != 0x80) {
				mask = mask << 1;
			} else {
				videoram += 0x100;
				mask = 1;
			}
			bitmapArray[item++] = paletteMono[cl];
		}
	}
}

void OkeanBus::createBitmap(uint32_t *bitmapArray) {
	if (isColorDisplay()) {
		createColorBitmap(bitmapArray);
	} else {
		createMonoBitmap(bitmapArray);
	}
}

#ifdef _PTHREAD
void* tF(void* param) {
	std::weak_ptr<OkeanBus> wbus = *(std::weak_ptr<OkeanBus>*)(param);
	std::shared_ptr<OkeanBus> bus = wbus.lock();
	//	while ((bus = wbus.lock()) != nullptr)
	while (true)
	{
		bus->clock();
	}
}
#else
void threadFunction(std::weak_ptr<Cpu> wcpu, std::weak_ptr<OkeanBus> wbus) {
	std::shared_ptr<OkeanBus> bus = wbus.lock();
	//	while ((bus = wbus.lock()) != nullptr)
	while (true)
	{
		bus->clock();
	}
}
#endif
void OkeanBus::execute(BusEvent handler) {
	m_handler = handler;
	// Communication betweet RAM and CPU
	cpu->connect(ram);
	cpu->connect(shared_from_this());
	ram->configureMAP(MemoryMapTypeValue::INIT);
	cpu->setCounter(0xE000);
	m_k580vi53 = std::make_shared<K580VI53>(0x60, 0x61, 0x62, 0x63, 0xFF, 0x10, 0, 0, 0, 0);
	sbus = shared_from_this();
	std::weak_ptr<OkeanBus> wbus = sbus;
	// timer
	m_k580vi53->init([wbus](uint8_t port, uint8_t value, BasePeriphery::ActivationType type, uint8_t mask) {
		if (auto bus = wbus.lock()) {
			if (port != 0) {
				uint8_t v = bus->ports[port];
				if (value) { v |= mask; } else { v &= ~mask;}
				bus->write(port, v);
			}
		}
	});
// interrupt controller
	m_k580vn59 = std::make_shared<K580VN59>(0x80, 0x81, 0xFF);
// keyb port
	m_k580vv55_keyb = std::make_shared<K580VV55>(0x40);
	m_k580vv55_keyb->init(0x42, [wbus](uint8_t port, uint8_t value) {
		if (auto bus = wbus.lock()) {
			uint8_t v = bus->ports[0xFF];
			uint8_t mask = 0x02;
			if (port == 0x42) {
				if (value & 0x80) {
					v &= ~mask;
				}
				bus->write(0xFF, v);
			} else if (port == 0x40) {
				v |= mask;
				bus->write(0xFF, v);
			}

		}
	});
#ifdef _PTHREAD
	pthread_t tid; /* идентификатор потока */
	pthread_attr_t attr; /* отрибуты потока */

	pthread_attr_init(&attr);
	struct sched_param sch_par;
	pthread_attr_getschedparam(&attr, &sch_par);
	sch_par.sched_priority = sched_get_priority_max(SCHED_RR);
	pthread_attr_setschedparam(&attr, &sch_par);
	
	pthread_create(&tid, &attr, tF, &sbus);
#else
	std::thread t1(threadFunction, cpu, wbus);
	t1.detach();
#endif
}

void OkeanBus::loadData(std::string path, uint8_t* data, uint16_t size) {
	std::ifstream file(path, std::ios::in | std::ios::binary);

	if (!file.is_open())
	{
		std::cerr << "File not found " << path;
		exit(EXIT_FAILURE);

		return;
	}

	file.read((std::ifstream::char_type *) data, (std::streamsize) size);
	file.close();
}


void OkeanBus::load(std::string motitor_path, std::string cpm_path) {
	ram = std::make_shared<RAM>(shared_from_this());
	cpu = std::make_shared<Cpu>();
	loadData(motitor_path, ram->getMemoryBlock(OkeanBus::MemoryType::MONITOR), BLOCK_SIZE);
	loadData(cpm_path, ram->getMemoryBlock(OkeanBus::MemoryType::OS), BLOCK_SIZE);
}

#define MASK_BLOCK 0x08
#define MASK_DATA 0x04

void OkeanBus::loadFileToRecordPlayer(std::string filename) {
	std::weak_ptr<OkeanBus> wbus = shared_from_this();
	m_recordPlayer = std::make_shared <RecordPlayer>();
	if (!m_recordPlayer->init(filename, [wbus](RecordPlayer::PlaySatus status, uint8_t value) {
		if (auto bus = wbus.lock()) {
			uint8_t vv[4];
			if (status == RecordPlayer::PlaySatus::SILENCE) {
				auto v = bus->read(0x41);
				v &= ~MASK_BLOCK;
				v &= ~MASK_DATA;
				bus->write(0x41, v);
				*((int16_t*)&vv[0]) = 0;
				*((int16_t*)&vv[2]) = -30000;
			} else if (status == RecordPlayer::PlaySatus::PLAYING) {
				auto v = bus->read(0x41);
				v |= MASK_BLOCK;
				if(value)
					v |= MASK_DATA;
				else
					v &= ~MASK_DATA;
				bus->write(0x41, v);
				*((int16_t*)&vv[0]) = value ? 30000 : -30000;
				*((int16_t*)&vv[2]) = 30000;
			}
			else if (status == RecordPlayer::PlaySatus::LOADED) {
#ifdef WRITE_WAV
				fileOut->close();
#endif
				bus->m_handler(OkeanBus::EventType::RECORD_PLAYER);
				return;
			}
#ifdef WRITE_WAV
			fileOut->write((const char*)(&vv[0]), 4);
#endif
		}
	})) {
		m_recordPlayer = nullptr;
	}
}
void OkeanBus::startRecordPlayer() {
	if (m_recordPlayer != nullptr) {
#ifdef WRITE_WAV
		fileOut = std::make_shared<std::fstream>("wav.bin", std::ios::app | std::ios::binary);
		if (!fileOut->is_open())
		{
			std::cerr << "File not found " << "C:\\work\\project\\OkeanEmu\\wav.bin";
			exit(EXIT_FAILURE);
			return;
		}
		flagWrite = true;
#endif
		m_recordPlayer->start();
		m_handler(OkeanBus::EventType::RECORD_PLAYER);
	}
}

RecordPlayer::PlaySatus OkeanBus::getRecordPlayerStatus(double_t &percent) {
	RecordPlayer::PlaySatus status = RecordPlayer::PlaySatus::IDLE;
	double_t p = 0;
	if (m_recordPlayer != nullptr) {
		status = m_recordPlayer->getRecordPlayerStatus(p);
	}
	percent = p;
	return status;
}