#include "okeanBus.h"
#include "../bus/periphery.h"

#define MEMORY_MAP_MASK 0x37

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
	case MemoryMapTypeValue::EXT192_RAM1:
		ram_map[0] = &EXT_RAM1[0];
		ram_map[1] = &EXT_RAM1[BLOCK_SIZE];
		ram_map[2] = &EXT_RAM1[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT192_RAM2:
		ram_map[0] = &EXT_RAM1[BLOCK_SIZE * 2];
		ram_map[1] = &EXT_RAM1[BLOCK_SIZE * 3];
		ram_map[2] = &EXT_RAM1[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT192_RAM3:
		ram_map[0] = &EXT_RAM2[0];
		ram_map[1] = &EXT_RAM2[BLOCK_SIZE];
		ram_map[2] = &EXT_RAM2[BLOCK_SIZE * 2];
		ram_map[3] = OS;
		break;
	case MemoryMapTypeValue::EXT192_RAM4:
		ram_map[0] = &EXT_RAM2[BLOCK_SIZE * 2];
		ram_map[1] = &EXT_RAM2[BLOCK_SIZE * 3];
		ram_map[2] = &EXT_RAM2[BLOCK_SIZE * 2];
		ram_map[3] = OS;
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
							  { 0x0000FF00, 0x00BF00FF, 0x00FF0000, 0x00FFFF00 },
							  { 0x00000000, 0x0000FFFF, 0x0000FF00, 0x00FFFFFF },
							  { 0x00000000, 0x00FFFF00, 0x0000FF00, 0x000000FF },
							  { 0x00000000, 0x00FF0000, 0x000000FF, 0x00FFFF00 },
							  { 0x00FF0000, 0x00FFFF00, 0x00FFFFFF, 0x000000FF },
							  { 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
							};
uint32_t paletteMono[8][2] = { 
	{ 0x00FFFFFF, 0x00000000},
	{ 0x0000007F, 0x0000FF00},
	{ 0x00FF0000, 0x007F0000},
	{ 0x00CF00CF, 0x000000FF},
	{ 0x00007F00, 0x00BF00FF},
	{ 0x00004B96, 0x00FFFF00},
	{ 0x00FF4000, 0x00000000},
	{ 0x007F7F7F, 0x007F7F7F}
};

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
	uint8_t pal = read(0xE1) & 0x07;
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
			bitmapArray[item++] = paletteMono[pal][cl];
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
	if (cpm_path == motitor_path) {
		loadData(cpm_path, ram->getMemoryBlock(OkeanBus::MemoryType::OS), BLOCK_SIZE * 2);
	} else {
		loadData(motitor_path, ram->getMemoryBlock(OkeanBus::MemoryType::MONITOR), BLOCK_SIZE);
		loadData(cpm_path, ram->getMemoryBlock(OkeanBus::MemoryType::OS), BLOCK_SIZE);
	}
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

uint8_t* OkeanBus::getExtendedRAM(OkeanBus::ExtRAMBank bank) {
	switch (bank) {
	case OkeanBus::ExtRAMBank::BANK1:
		return &ram->EXT_RAM[0];
	case OkeanBus::ExtRAMBank::BANK2:
		return &ram->EXT_RAM1[0];
	case OkeanBus::ExtRAMBank::BANK3:
		return &ram->EXT_RAM2[0];
	}
	return nullptr;
}

#define EMPTY_FSB 0xE5
#define DIR_SIZE 1024
#define RECORD_NUMBER 0x0C
#define FSB_BLOCK_SIZE 32

bool OkeanBus::getFileRecord(FSBRecord &record, uint8_t next, uint8_t numRecord, std::string fileName) {
	uint8_t* dir = getExtendedRAM(ExtRAMBank::BANK1);
	uint8_t currentFSB = 0;
	uint8_t* endOfDir = dir + DIR_SIZE;
	while (dir < endOfDir) {
		if (*dir != EMPTY_FSB && *(dir + RECORD_NUMBER) == numRecord) {
			if (numRecord != 0 || currentFSB == next) {
				uint8_t name[13];
				memcpy(name, dir + 1, 8);
				uint8_t pos = 8;
				name[pos] = 0;
				pos--;
				while (name[pos] == 0x20) {
					name[pos] = 0;
					pos--;
				}
				std::string fname(reinterpret_cast<char*>(&name[0]));
				memcpy(name, dir + 9, 3);
				name[3] = 0;
				std::string fext(reinterpret_cast<char*>(&name[0]));
				fname = fname + "." + fext;
				if (fileName.empty() || fname == fileName) {
					record.fileName = fname;
					record.sizeInBlocks += *(dir + 15);
					uint8_t bl = 0;
					while (bl < 16 && *(dir + bl + 16) > 0) {
						record.blocks.push_back(*(dir + bl + 16));
						bl++;
					}
					if (*(dir + 15) == 0x80) {
						getFileRecord(record, next, numRecord + 1, fname);
					}
					return true;
				}
			}
			if (numRecord == 0) {
				currentFSB++;
			}
		}
		dir += FSB_BLOCK_SIZE;
	}
	return false;
}

uint8_t* OkeanBus::getPointerByBlock(uint8_t block) {
	uint32_t  size = block * 1024;
	uint8_t* offset = nullptr;
	if (size <= BLOCK_SIZE * 4) {
		offset = getExtendedRAM(ExtRAMBank::BANK1) + size;
	} else {
		size -= (BLOCK_SIZE * 4);
		if (size <= BLOCK_SIZE * 4) {
			offset = getExtendedRAM(ExtRAMBank::BANK2) + size;
		} else {
			size -= (BLOCK_SIZE * 4);
			offset = getExtendedRAM(ExtRAMBank::BANK3) + size;
		}
	}
	return offset;
}

void OkeanBus::copyFilesToHost(std::string path) {
	uint8_t fileItem = 0;
	bool flag = true;
	while (flag) {
		FSBRecord fsbRecord;
		fsbRecord.fileName = "";
		fsbRecord.sizeInBlocks = 0;
		fsbRecord.blocks.clear();
		if (flag = getFileRecord(fsbRecord, fileItem)) {
			std::ofstream fileOut(path + "/" + fsbRecord.fileName, std::ios::out | std::ios::binary);
			if (!fileOut.is_open()) {
				exit(EXIT_FAILURE);
				return;
			}
			uint8_t item = 0;
			while (fsbRecord.sizeInBlocks > 0) {
				uint8_t* ptr = getPointerByBlock(fsbRecord.blocks.at(item));
				uint8_t wrBl = fsbRecord.sizeInBlocks * 128 > 0x400 ? 0x400 / 128 : fsbRecord.sizeInBlocks;
				uint16_t len = wrBl * 128;
				fileOut.write((const char*)ptr, len);
				fsbRecord.sizeInBlocks -= wrBl;
				item++;
			}
			fileOut.close();
			fileItem++;
		}
	}
}

void OkeanBus::prepareEmptyBlocks(std::vector<uint8_t> &emptyFSB, std::vector<uint8_t> &emptyBlocks) {
	emptyFSB.clear();
	emptyBlocks.clear();
	for (uint8_t i = 1; i < 192; i++) {
		emptyBlocks.push_back(i);
	}
	uint8_t* dir = getExtendedRAM(ExtRAMBank::BANK1);
	for (uint8_t i = 0; i < 32; i++) {
		if (*dir != EMPTY_FSB) {
			auto blocks = dir + 16;
			for (uint8_t i = 0; i < 16; i++) {
				auto nb = *blocks;
				if (nb != 0) {
					emptyBlocks.erase(std::remove_if(emptyBlocks.begin(), emptyBlocks.end(), [nb](unsigned char x) {
						return x == nb; 
					}));
				}
				blocks++;
			}
		} else {
			emptyFSB.push_back(i);
		}
		dir += 32;
	}
}

std::string OkeanBus::getFileName(std::string fullName) {
	auto nm = fullName.find_last_of('/');
	auto fileName = fullName;
	if (nm == std::string::npos) {
		nm = fullName.find_last_of('\\');
	}
	if (nm == std::string::npos) {
		nm = fullName.find_last_of(':');
	}
	if (nm != std::string::npos) {
		fileName = fullName.substr(nm+1);
	}
	return fileName;
}

void OkeanBus::copyFilesFromHost(std::string fileName) {
	std::vector<uint8_t> emptyFSB;
	std::vector<uint8_t> emptyBlocks;
	prepareEmptyBlocks(emptyFSB, emptyBlocks);

	std::ifstream file(fileName, std::ios::in | std::ios::binary);
	if (file.is_open())
	{
		file.seekg(0, std::ios::end);
		auto fileSize = file.tellg();
		if (fileSize > 0) {
			file.seekg(0, std::ios::beg);
			auto buffer = new uint8_t[fileSize];
			file.read((std::ifstream::char_type *) buffer, (std::streamsize) fileSize);
			file.close();
			uint8_t* dir = getExtendedRAM(ExtRAMBank::BANK1);
			auto fn = getFileName(fileName);
			auto curBuff = buffer;
			uint8_t itemFsb = 0;
			uint8_t itemBlock = 0;
			bool force = false;
			while (fileSize > 0 || force) {
				force = false;
				auto fsb = dir + emptyFSB.at(itemFsb) * 32;
				memset(fsb, 0x00, 32);
				memset(fsb + 1, 0x20, 11);
				auto nm = fn.find_last_of('.');
				if (nm == std::string::npos) {
					nm = fn.length();
				}
				auto data = fn.c_str();
				memcpy(fsb + 1, data, nm > 8 ? 8 : nm);
				if (nm < fn.length()) {
					auto s = fn.length() - nm;
					memcpy(fsb + 9, &data[nm + 1], s > 3 ? 3 : s);
				}
				*(fsb + 12) = itemFsb;
				uint8_t blockSize = fileSize > 0 ? (fileSize / 128) >= 0x80 ? 0x80 : (fileSize / 128 + 1) : 0;
				if (blockSize == 0x80) {
					force = true;
					itemFsb++;
				}
				*(fsb + 15) = blockSize;
				if (blockSize * 128 >= fileSize) {
					fileSize = 0;
				}
				else {
					fileSize -= (blockSize * 128);
				}
				uint8_t index = 16;
				while (blockSize > 0) {
					auto dst = getPointerByBlock(emptyBlocks.at(itemBlock));
					*(fsb + index) = emptyBlocks.at(itemBlock);
					index++;
					itemBlock++;
					uint16_t length = blockSize > 8 ? 1024 : blockSize * 128;
					blockSize -= blockSize > 8 ? 8 : blockSize;
					memcpy(dst, curBuff, length);
					curBuff += length;
				}
			}
			delete[] buffer;
		}
	}
}
