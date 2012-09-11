//	$Id: piccolo.cpp,v 1.3 2003/04/22 13:16:36 cisc Exp $

#include "headers.h"
#include <winioctl.h>
#include "piccolo.h"
#include "piccolo_romeo.h"
#include "piccolo_gimic.h"
#include "romeo.h"
#include "misc.h"
#include "status.h"
#include "../../piccolo/piioctl.h"

#define LOGNAME "piccolo"
#include "diag.h"

Piccolo* Piccolo::instance = NULL;

// ---------------------------------------------------------------------------
//
//
Piccolo* Piccolo::GetInstance()
{
	if ( instance ) {
		return instance;
	} else {
		instance = new Piccolo_Romeo();
		if ( instance->Init() == PICCOLO_SUCCESS ) {
			return instance;
		}
		delete instance;
		instance = new Piccolo_Gimic();
		if ( instance->Init() == PICCOLO_SUCCESS ) {
			return instance;
		}
	}

	return 0;
}

Piccolo::Piccolo()
: active(false), hthread(0), idthread(0), avail(0), events(0), evread(0), evwrite(0), eventries(0),
  maxlatency(0), shouldterminate(true)
{
}

Piccolo::~Piccolo()
{
}

// ---------------------------------------------------------------------------
//
//
int Piccolo::Init()
{
	// thread �쐬
	shouldterminate = false;
	if (!hthread)
	{
		hthread = (HANDLE) 
			_beginthreadex(NULL, 0, ThreadEntry, 
				reinterpret_cast<void*>(this), 0, &idthread);
	}
	if (!hthread)
		return PICCOLOE_THREAD_ERROR;

	SetMaximumLatency(1000000);
	SetLatencyBufferSize(16384);
	return PICCOLO_SUCCESS;
}

// ---------------------------------------------------------------------------
//	��n��
//
void Piccolo::Cleanup()
{
	if (hthread)
	{
		shouldterminate = true;
		if (WAIT_TIMEOUT == WaitForSingleObject(hthread, 3000))
		{
			TerminateThread(hthread, 0);
		}
		CloseHandle(hthread);
		hthread = 0;
	}
}

// ---------------------------------------------------------------------------
//	Core Thread
//
uint Piccolo::ThreadMain()
{
	::SetThreadPriority(hthread, THREAD_PRIORITY_TIME_CRITICAL);
	while (!shouldterminate)
	{
		Event* ev;
		const int waitdefault = 2;
		int wait = waitdefault;
		{
			CriticalSection::Lock lock(cs);
			uint32 time = GetCurrentTime();
			while ((ev = Top()) && !shouldterminate)
			{
				int32 d = ev->at - time;

				if (d >= 1000)
				{
					if (d > maxlatency)
						d = maxlatency;
					wait = d / 1000;
					break;
				}
				SetReg(ev->addr, ev->data);
				Pop();
			}
		}
		if (wait > waitdefault)
			wait = waitdefault;
		Sleep(wait);
	}
	return 0;
}

// ---------------------------------------------------------------------------
//	�L���[�ɒǉ�
//
bool Piccolo::Push(Piccolo::Event& ev)
{
	if ((evwrite + 1) % eventries == evread)
		return false;
	events[evwrite] = ev;
	evwrite = (evwrite + 1) % eventries;
	return true;
}

// ---------------------------------------------------------------------------
//	�L���[�����Ⴄ
//
Piccolo::Event* Piccolo::Top()
{
	if (evwrite == evread)
		return 0;
	
	return &events[evread];
}

void Piccolo::Pop()
{
	evread = (evread + 1) % eventries;
}


// ---------------------------------------------------------------------------
//	�T�u�X���b�h�J�n�_
//
uint CALLBACK Piccolo::ThreadEntry(void* arg)
{
	return reinterpret_cast<Piccolo*>(arg)->ThreadMain();
}

// ---------------------------------------------------------------------------
//
//
bool Piccolo::SetLatencyBufferSize(uint entries)
{
	CriticalSection::Lock lock(cs);
	Event* ne = new Event[entries];
	if (!ne)
		return false;

	delete[] events;
	events = ne;
	eventries = entries;
	evread = 0;
	evwrite = 0;
	return true;
}

bool Piccolo::SetMaximumLatency(uint nanosec)
{
	maxlatency = nanosec;
	return true;
}

uint32 Piccolo::GetCurrentTime()
{
	return ::GetTickCount() * 1000;
}

// ---------------------------------------------------------------------------

void Piccolo::DrvReset()
{
	CriticalSection::Lock lock(cs);

	// �{���͊Y������G���g�������폜���ׂ������c
	evread = 0;
	evwrite = 0;
}

bool Piccolo::DrvSetReg(uint32 at, uint addr, uint data)
{
	if (int32(at - GetCurrentTime()) > maxlatency)
	{
//		statusdisplay.Show(100, 0, "Piccolo: Time %.6d", at - GetCurrentTime());
		return false;
	}

	Event ev;
	ev.at = at;
	ev.addr = addr;
	ev.data = data;

/*int d = evwrite - evread;
if (d < 0) d += eventries;
statusdisplay.Show(100, 0, "Piccolo: Time %.6d  Buf: %.6d  R:%.8d W:%.8d w:%.6d", at - GetCurrentTime(), d, evread, GetCurrentTime(), asleep);
*/
	return Push(ev);
}

void Piccolo::DrvRelease()
{
}
