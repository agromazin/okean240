#pragma once

#include <fstream>
#include <array>
#include <memory>
#include <functional>

class BasePeriphery {
public:
	enum class ActivationType { VALUE, BIT };
	using Bus = std::function< void(uint8_t port, uint8_t value, ActivationType type, uint8_t mask) >;
	BasePeriphery() {};
	virtual ~BasePeriphery() = default;

	virtual void init(Bus bus) { m_bus = bus; };
	virtual void write(uint8_t address, uint8_t value) { m_value = value; };
	virtual uint8_t read(uint8_t address) { return m_value; };
	virtual void clock() {};
protected:
	uint8_t m_value;
	Bus m_bus;
};

class K580VI53 : public BasePeriphery {
public:
	K580VI53(uint8_t addr_t1, uint8_t addr_t2, uint8_t addr_t3, uint8_t addr_mode, uint8_t addr1, uint8_t bitInt1, uint8_t addr2, uint8_t bitInt2, uint8_t addr3, uint8_t bitInt3);
	virtual ~K580VI53() = default;
	void clock() override;
	virtual void write(uint8_t address, uint8_t value) override;
	virtual uint8_t read(uint8_t address) override;
private:
	enum class CounterMode { BIN, BIN_DEC };
	enum class TimerStatus { INPROGRESS, WAIT_LBYTE, WAIT_MBYTE, WAIT_WORD };
	class Mode {
	public:
		using K580VI53_Host = std::function< void(uint8_t value) >;

		Mode(CounterMode counterMode, TimerStatus status) : m_counterMode(counterMode), m_status(status) {};
		virtual ~Mode() = default;
		virtual void init(K580VI53_Host host) { m_host = host; };
		virtual void clock() = 0;
		virtual uint16_t read() = 0;
		virtual void set(uint16_t counter) { m_counter = counter; };
		TimerStatus getStatus() { return m_status; };
	protected:
		uint16_t m_counter;
		CounterMode m_counterMode;
		K580VI53_Host m_host;
		TimerStatus m_status;
	};

	class Mode3 : public Mode {
	public:
		Mode3(CounterMode counterMode, TimerStatus status) : Mode(counterMode, status) {};
		virtual ~Mode3() = default;
		void clock() override;
		uint16_t read() { return m_current; };
		void init(K580VI53_Host host) override;
		virtual void set(uint16_t counter) override;
	private:
		uint16_t m_current;
		uint16_t m_half;
	};

	class Timer53 {
	public:
		Timer53() {};
		virtual ~Timer53() = default;
		std::shared_ptr<Mode> m_mode;
		TimerStatus m_currentStatus;
		TimerStatus m_createdStatus;
		uint16_t m_counter;
	};
	uint8_t channelNumber(uint8_t data);
	TimerStatus accessType(uint8_t data);
	uint8_t timerMode(uint8_t data);
	CounterMode counterMode(uint8_t data);
	void writeTimerCounter(uint8_t chNum, uint8_t value, uint8_t addr, uint8_t bitInt);
	uint8_t readTimerCounter(uint8_t chNum);

private:
	uint8_t m_addr_t1;
	uint8_t m_addr_t2;
	uint8_t m_addr_t3;
	uint8_t m_addr_mode;
	uint8_t m_addr1;
	uint8_t m_bitInt1;
	uint8_t m_addr2;
	uint8_t m_bitInt2;
	uint8_t m_addr3;
	uint8_t m_bitInt3;
	std::shared_ptr<Timer53> m_timers[3];
	double_t m_devider;

};

class K580VN59 : public BasePeriphery {
public:
	K580VN59(uint8_t addr_CW1, uint8_t addr_CW2, uint8_t addr_INT);
	virtual ~K580VN59() = default;
	void clock() override;
	virtual void write(uint8_t address, uint8_t value) override;
	virtual uint8_t read(uint8_t address) override;
private:
	enum class VN59_STATUS { IDLE, READY };
	enum class VN59_READ_TYPE { IRR, ISR , NONE};
	uint8_t m_port_CW1;
	uint8_t m_port_CW2;
	uint8_t m_port_INT;
	VN59_STATUS m_status;
	uint8_t m_irr;
	uint8_t m_imr;
	uint16_t m_int_table;
	uint8_t m_table_step;
	VN59_READ_TYPE m_readType;
};

class K580VV55 : public BasePeriphery {
public:
	using K580VÌ55_Host = std::function< void(uint8_t port, uint8_t value) >;
	K580VV55(uint8_t addr);
	virtual ~K580VV55() = default;
	void clock() override;
	virtual void write(uint8_t address, uint8_t value) override;
	virtual uint8_t read(uint8_t address) override;
	void init(uint8_t notifyPort, K580VÌ55_Host host);
private:
	enum class MODE { MODE0, MODE1, MODE2 };
	enum class DIRECTION { DIR_IN, DIR_OUT, DIR_BOTH };
	class VV_PORT {
	public:
		uint8_t addr;
		MODE mode;
		DIRECTION dirH;
		DIRECTION dirL;
		uint8_t data;
	};
	VV_PORT m_port[3];
	uint8_t m_addr;
	K580VÌ55_Host m_host;
	uint8_t m_notifyPort;
};
