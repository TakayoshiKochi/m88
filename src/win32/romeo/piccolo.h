//	$Id: piccolo.h,v 1.2 2002/05/31 09:45:22 cisc Exp $

#ifndef incl_romeo_piccolo_h
#define incl_romeo_piccolo_h

#include "types.h"
#include "timekeep.h"
#include "critsect.h"

//	�x�����M�Ή� ROMEO �h���C�o
//	
class PiccoloChip
{
public:
    virtual ~PiccoloChip(){};
	virtual int	 Init(uint c) = 0;
	virtual void Reset() = 0;
	virtual bool SetReg(uint32 at, uint addr, uint data) = 0;
	virtual void SetChannelMask(uint mask) = 0;
	virtual void SetVolume(int ch, int value) = 0;
};

enum PICCOLO_CHIPTYPE
{
	PICCOLO_INVALID = 0,
	PICCOLO_YMF288,
	PICCOLO_YM2608
};

class Piccolo
{
public:
	virtual ~Piccolo();

	enum PICCOLO_ERROR
	{
		PICCOLO_SUCCESS = 0,
		PICCOLOE_UNKNOWN = -32768,
		PICCOLOE_DLL_NOT_FOUND,
		PICCOLOE_ROMEO_NOT_FOUND,
		PICCOLOE_HARDWARE_NOT_AVAILABLE,
		PICCOLOE_HARDWARE_IN_USE,
		PICCOLOE_TIME_OUT_OF_RANGE,
		PICCOLOE_THREAD_ERROR,
	};

	static Piccolo* GetInstance();

	// �x���o�b�t�@�̃T�C�Y��ݒ�
	bool SetLatencyBufferSize(uint entry);

	// �x�����Ԃ̍ő�l��ݒ�
	// SetReg ���Ăяo���ꂽ�Ƃ��Ananosec ��ȍ~�̃��W�X�^�������݂��w������ at �̒l���w�肵���ꍇ
	// �Ăяo���͋p������邩������Ȃ��B
	bool SetMaximumLatency(uint nanosec);

	// ���\�b�h�Ăяo�����_�ł̎��Ԃ�n��(�P�ʂ� nanosec)
	uint32 GetCurrentTime();

	// 
	virtual int GetChip(PICCOLO_CHIPTYPE type, PiccoloChip** pc) = 0;
	virtual void SetReg( uint addr, uint data) = 0;

	int IsDriverBased() { return avail; }

public:
	bool DrvSetReg(uint32 at, uint addr, uint data);
	void DrvReset();
	void DrvRelease();

protected:
	struct Event
	{
		uint at;
		uint addr;
		uint data;
	};

	static Piccolo *instance;

	Piccolo();
	virtual int Init();
	void Cleanup();
	static uint CALLBACK ThreadEntry(void* arg);
	uint ThreadMain();

	TimeKeeper timekeeper;
	CriticalSection cs;

	bool Push(Event&);
	Event* Top();
	void Pop();

	Event* events;
	int evread;
	int evwrite;
	int eventries;

	int maxlatency;

	uint32 addr;
	uint32 irq;

	volatile bool shouldterminate;
	volatile bool active;
	HANDLE hthread;
	uint idthread;

	int avail;
};


#endif // incl_romeo_piccolo_h
