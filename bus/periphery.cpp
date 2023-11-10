#include <fstream>
#include <array>
#include <memory>
#include <functional>
#include "../bus/periphery.h"
#include <stdio.h>

#define DEVIDER 1.5
//std::shared_ptr<std::fstream> fileOutVI53;
// =========================== VI53 =================================

K580VI53::K580VI53(uint8_t addr_t1, uint8_t addr_t2, uint8_t addr_t3, uint8_t addr_mode, uint8_t addr1, uint8_t bitInt1, uint8_t addr2, uint8_t bitInt2, uint8_t addr3, uint8_t bitInt3) :
	m_addr_t1(addr_t1), 
	m_addr_t2(addr_t2), 
	m_addr_t3(addr_t3), 
	m_addr_mode(addr_mode), 
	m_addr1(addr1), 
	m_bitInt1(bitInt1), 
	m_addr2(addr2), 
	m_bitInt2(bitInt2), 
	m_addr3(addr3), 
	m_bitInt3(bitInt3), 
	m_devider(0.0) {
/*	fileOutVI53 = std::make_shared<std::fstream>("timeouts.bin", std::ios::app);
	if (!fileOutVI53->is_open())
	{
		exit(EXIT_FAILURE);
		return;
	}
*/
}

void K580VI53::clock() {
//	m_devider += 1.0;
//	if (m_devider >= (DEVIDER / 1.5)) {
		for (uint8_t i = 0; i < 3; i++) {
			if (m_timers[i] != nullptr && m_timers[i]->m_mode->getStatus() == K580VI53::TimerStatus::INPROGRESS) {
				m_timers[i]->m_mode->clock();
			}
		}
//		m_devider = m_devider - (DEVIDER / 1.5);
//	}
}

uint8_t K580VI53::channelNumber(uint8_t data) {
	return (data >> 6) & 0x03;
}

K580VI53::TimerStatus K580VI53::accessType(uint8_t data) {
	auto type = (data >> 4) & 0x03;
	switch (type) {
	case 0:
		return K580VI53::TimerStatus::INPROGRESS;
	case 1:
		return K580VI53::TimerStatus::WAIT_LBYTE;
	case 2:
		return K580VI53::TimerStatus::WAIT_MBYTE;
	case 3:
		return K580VI53::TimerStatus::WAIT_WORD;
	}
	return K580VI53::TimerStatus::INPROGRESS;
}

uint8_t K580VI53::timerMode(uint8_t data) {
	return (data >> 1) & 0x07;
}

K580VI53::CounterMode K580VI53::counterMode(uint8_t data) {
	if (data & 1) {
		return K580VI53::CounterMode::BIN_DEC;
	}
	return K580VI53::CounterMode::BIN;
}

void K580VI53::writeTimerCounter(uint8_t chNum, uint8_t value, uint8_t addr, uint8_t bitInt) {
	if (m_timers[chNum] != nullptr) {
		uint8_t *ar = (uint8_t *)&m_timers[chNum]->m_counter;
		switch (m_timers[chNum]->m_currentStatus) {
		case K580VI53::TimerStatus::INPROGRESS:
			return;
		case K580VI53::TimerStatus::WAIT_WORD:
			ar[0] = value;
			m_timers[chNum]->m_currentStatus = K580VI53::TimerStatus::WAIT_MBYTE;
			return;
		case K580VI53::TimerStatus::WAIT_LBYTE:
			ar[0] |= value;
			m_timers[chNum]->m_currentStatus = m_timers[chNum]->m_createdStatus;
			break;
		case K580VI53::TimerStatus::WAIT_MBYTE:
			ar[1] = value;
			m_timers[chNum]->m_currentStatus = m_timers[chNum]->m_createdStatus;
			break;
		}
		m_timers[chNum]->m_mode->set(m_timers[chNum]->m_counter);
		auto b = m_bus;
		m_timers[chNum]->m_mode->init([chNum, addr, bitInt, b](uint8_t val) {
			b(addr, val, BasePeriphery::ActivationType::BIT, bitInt);
		}
		);
	}
}

void K580VI53::write(uint8_t address, uint8_t value) {
	if (address == m_addr_t1) {
		writeTimerCounter(0, value, m_addr1, m_bitInt1);
	}
	else if (address == m_addr_t2) {
		writeTimerCounter(1, value, m_addr2, m_bitInt2);
	}
	else if (address == m_addr_t3) {
		writeTimerCounter(2, value, m_addr3, m_bitInt3);
	}
	else if (address == m_addr_mode) {
		auto chNum = channelNumber(value);
		auto accT = accessType(value);
		auto tm = timerMode(value);
		auto cm = counterMode(value);
		if (chNum < 3) {
			if (accT != K580VI53::TimerStatus::INPROGRESS) {
				if (m_timers[chNum] == nullptr) {
					m_timers[chNum] = std::make_shared<K580VI53::Timer53>();
					m_timers[chNum]->m_counter = 0;
				} else {
					m_timers[chNum]->m_counter = m_timers[chNum]->m_mode->read();
				}
				m_timers[chNum]->m_createdStatus = accT;
				m_timers[chNum]->m_currentStatus = accT;
				switch (tm) {
				case 0:
				case 1:
				case 2:
					break;
				case 3:
					m_timers[chNum]->m_mode = std::make_shared<Mode3>(cm, accT);
					break;
				case 4:
				case 5:
					break;
				}
			} else if (accT == K580VI53::TimerStatus::INPROGRESS && m_timers[chNum] != nullptr) {
				m_timers[chNum]->m_counter = m_timers[chNum]->m_mode->read();
				m_timers[chNum]->m_currentStatus = m_timers[chNum]->m_createdStatus;
			}
		}
	}
}

uint8_t K580VI53::readTimerCounter(uint8_t chNum) {
	if (m_timers[chNum] != nullptr) {
		switch (m_timers[chNum]->m_currentStatus) {
		case K580VI53::TimerStatus::INPROGRESS:
			return 0;
		case K580VI53::TimerStatus::WAIT_WORD: {
/*			char vv[10];
			sprintf_s(vv, 10, "%X\n", m_timers[chNum]->m_counter);
			fileOutVI53->write((const char*)(&vv[0]), 5);
*/
			m_timers[chNum]->m_currentStatus = K580VI53::TimerStatus::WAIT_MBYTE;
			return static_cast<uint8_t>(m_timers[chNum]->m_counter & 0xFF);
		}
		case K580VI53::TimerStatus::WAIT_LBYTE:
			m_timers[chNum]->m_currentStatus = m_timers[chNum]->m_createdStatus;
			return static_cast<uint8_t>(m_timers[chNum]->m_counter & 0xFF);
		case K580VI53::TimerStatus::WAIT_MBYTE:
			m_timers[chNum]->m_currentStatus = m_timers[chNum]->m_createdStatus;
			return static_cast<uint8_t>((m_timers[chNum]->m_counter >> 8) & 0xFF);
		}
	}
	return 0;
}

uint8_t K580VI53::read(uint8_t address) {
	if (address == m_addr_t1) {
		return readTimerCounter(0);
	}
	else if (address == m_addr_t2) {
		return readTimerCounter(1);
	}
	else if (address == m_addr_t3) {
		return readTimerCounter(2);
	}
	return 0;
}

void K580VI53::Mode3::init(K580VI53::Mode::K580VI53_Host host) {
	K580VI53::Mode::init(host);
	m_current = m_counter;
	m_status = K580VI53::TimerStatus::INPROGRESS;
}

void K580VI53::Mode3::set(uint16_t counter) {
	if (counter == 0) {
		m_counter = 0xFFFF;
	}
	m_counter = counter;
	m_current = m_counter;
	m_half = m_counter / 2;
};

void K580VI53::Mode3::clock() {
//	if (m_current == m_counter) {
//		m_host(0xFF);
//	}

	if (m_half == m_current) {
			m_host(0x00);
	}
	m_current--;
	if (m_current == 0) {
		m_current = m_counter;
		m_host(0xFF);
	}
}


// ============================  VN59 ==========================

#define ICW1_MASK 0x10
#define ICW1_TABLE_STEP 0x04
#define OCW2_TYPE 0x18
#define OCW2_READ_TYPE 0x3

K580VN59::K580VN59(uint8_t addr_CW1, uint8_t addr_CW2, uint8_t addr_INT) :
	m_port_CW1(addr_CW1),
	m_port_CW2(addr_CW2),
	m_port_INT(addr_INT),
	m_status(VN59_STATUS::IDLE),
	m_irr(0x00), 
	m_imr(0x00),
	m_int_table(0),
	m_table_step (0),
	m_readType(VN59_READ_TYPE::NONE) {

}

void K580VN59::clock() {

}

void K580VN59::write(uint8_t address, uint8_t value) {
	if (m_status == VN59_STATUS::IDLE) {
		if (address == m_port_CW1) {
			if (value & ICW1_MASK) {
				if (value & ICW1_TABLE_STEP) {
					m_int_table = (value & 0xE0);
					m_table_step = 4;
				} else {
					m_int_table = (value & 0xC0);
					m_table_step = 8;
				}
			}
		} else if (address == m_port_CW2 && m_table_step != 0) {
			uint16_t v = value;
			v = v << 8;
			v = v & 0xFF00;
			m_int_table |= v;
			m_status = VN59_STATUS::READY;
		} else if (address == m_port_INT) {
			m_irr = value;
		}
	} else if (m_status == VN59_STATUS::READY) {
		if (address == m_port_CW2) {
			m_imr = value;
		} else if (address == m_port_CW1) {
			if ((value & OCW2_TYPE) == 0x08) {
				switch (value & OCW2_READ_TYPE) {
				case 0:
				case 1:
					m_readType = VN59_READ_TYPE::NONE;
					break;
				case 2:
					m_readType = VN59_READ_TYPE::IRR;
					break;
				case 3:
					m_readType = VN59_READ_TYPE::ISR;
					break;
				}
			}
		} else if (address == m_port_INT) {
			m_irr = value;
		}
	}
}

uint8_t K580VN59::read(uint8_t address) {
	if (m_status != VN59_STATUS::IDLE) {
		if (m_readType == VN59_READ_TYPE::IRR) {
			return m_irr;
		}
	}
	return 0;
}

// ============================  VV55 ==========================

#define VV55_MODE_MASK 0x60
#define VV55_PORT_C_BITS 0x0E
K580VV55::K580VV55(uint8_t addr) : m_addr(addr) {
	m_port[0].addr = m_addr & 0xFC;
	m_port[0].dirH = DIRECTION::DIR_OUT;
	m_port[0].mode = MODE::MODE0;
	m_port[0].data = 0;
	m_port[1].addr = m_addr & 0xFC + 1;
	m_port[1].dirH = DIRECTION::DIR_OUT;
	m_port[1].mode = MODE::MODE0;
	m_port[1].data = 0;
	m_port[2].addr = m_addr & 0xFC + 2;
	m_port[2].dirH = DIRECTION::DIR_OUT;
	m_port[2].dirL = DIRECTION::DIR_OUT;
	m_port[2].mode = MODE::MODE0;
	m_port[2].data = 0;
}

void K580VV55::clock() {

}

void K580VV55::init(uint8_t notifyPort, K580VÌ55_Host host) {
	m_host = host;
	m_notifyPort = notifyPort;
}

void K580VV55::write(uint8_t address, uint8_t value) {
	if ((address & 0xFC) == (m_addr & 0xFC)) {
		auto item = address & 0x03;
		if (item == 3) {
			if (value & 0x80) {
				uint8_t mode = (value & VV55_MODE_MASK) >> 5;
				if (mode < 2) {
					m_port[0].mode = mode == 0 ? MODE::MODE0 : MODE::MODE1;
					m_port[0].dirH = (value & 0x10) ? DIRECTION::DIR_IN : DIRECTION::DIR_OUT;
					m_port[2].dirH = (value & 0x08) ? DIRECTION::DIR_IN : DIRECTION::DIR_OUT;
					m_port[1].mode = (value & 0x04) ? MODE::MODE0 : MODE::MODE1;
					m_port[1].dirH = (value & 0x02) ? DIRECTION::DIR_IN : DIRECTION::DIR_OUT;
					m_port[2].dirL = (value & 0x01) ? DIRECTION::DIR_IN : DIRECTION::DIR_OUT;
				} else if (mode == 2) {

				}
			} else {
				uint8_t nb = (value & VV55_PORT_C_BITS) >> 1;
				uint8_t mask = 1 << nb;
				if (value & 1) {
					m_port[2].data |= mask;
				} else {
					m_port[2].data &= ~mask;
				}
				m_host(m_port[2].addr, m_port[2].data);
			}
		} else {
			m_port[item].data = value;
			if (m_port[item].dirH == DIRECTION::DIR_IN || m_notifyPort == address || m_notifyPort == 0xFF) {
				m_host(address, value);
			}
		}
	}
}

uint8_t K580VV55::read(uint8_t address) {
	if ((address & 0xFC) == (m_addr & 0xFC)) {
		auto item = address & 0x03;
		if (item != 3) {
			return m_port[item].data;
		}
	}
	return 0;
}
