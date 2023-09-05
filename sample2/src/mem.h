//  $Id: mem.h,v 1.1 1999/10/10 01:43:28 cisc Exp $

#ifndef incl_mem_h
#define incl_mem_h

#include "common/device.h"
#include "if/ifcommon.h"

// ---------------------------------------------------------------------------

class GVRAMReverse : public Device
{
public:
    enum
    {
        out32 = 0, out35, out5x
    };

public:
	GVRAMReverse();
	~GVRAMReverse();

	bool Init(IMemoryManager* mm);
	void Cleanup();

	// IDevice Method
	const Descriptor* IFCALL GetDesc() const { return &descriptor; }

	// I/O port functions
	void IOCALL Out32(uint32_t, uint32_t data);
	void IOCALL Out35(uint32_t, uint32_t data);
	void IOCALL Out5x(uint32_t, uint32_t data);

	static uint32_t MRead(void*, uint32_t);
	static void MWrite(void*, uint32_t, uint32_t);

private:
    void Update();

    IMemoryManager* mm;
    int mid;

	uint32_t p32, p35, p5x;
	bool gvram;

    static const Descriptor descriptor;
//  static const InFuncPtr  indef[];
    static const OutFuncPtr outdef[];
};

#endif // incl_mem_h
