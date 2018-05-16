﻿#include "kmain.h"
#include "gdt.h"
#include "ProcessUtil.h"
#include "PEImage.h"
#include "memory.h"
#include "Page.h"
#include "GDT.h"
#include "IDT.h"
#include "PIC.h"
#include "PIT.h"

extern "C" void ModeSwitchAndJumpKernel64();

_declspec(naked) void multiboot_entry(void)
{
	__asm {
		align 4

		multiboot_header:
		//멀티부트 헤더 사이즈 : 0X20
		dd(MULTIBOOT_HEADER_MAGIC); magic number
			dd(MULTIBOOT_HEADER_FLAGS); flags
			dd(CHECKSUM); checksum
			dd(HEADER_ADRESS); //헤더 주소 KERNEL_LOAD_ADDRESS+ALIGN(0x100064)
		dd(KERNEL_LOAD_ADDRESS); //커널이 로드된 가상주소 공간
		dd(00); //사용되지 않음
		dd(00); //사용되지 않음
		dd(HEADER_ADRESS + 0x20); //커널 시작 주소 : 멀티부트 헤더 주소 + 0x20, kernel_entry

	kernel_entry:
		mov     esp, KERNEL_STACK; //스택 설정

		push    0; //플래그 레지스터 초기화
		popf

			//GRUB에 의해 담겨 있는 정보값을 스택에 푸쉬한다.
			push    ebx; //멀티부트 구조체 포인터
		push    eax; //매직 넘버

		//위의 두 파라메터와 함께 kmain 함수를 호출한다.
		call    kmain; //C++ 메인 함수 호출

		//루프를 돈다. kmain이 리턴되지 않으면 아래 코드는 수행되지 않는다.
	halt:
		jmp halt;
	}
}

bool DetectionCPUID();
bool IsLongModeCheckPossible();
bool IsLongModePossible();
uint32_t FindKernel64Entry(const char* filename, char* buffer);
void EnterKernel64(void* entry, multiboot_info* info, Module* module);
void EnterKernel64_3(void* entry, multiboot_info* info, Module* moudle);
char* g_szKernelName = "SKYOS64_SYS";

extern "C" void __writecr4(unsigned __int64 Data);
extern "C"  unsigned long __readcr4(void);
uint32_t kernelEntry;
VOID HardwareInitialize();

void EnablePaging(bool state)
{
#ifdef _MSC_VER
	_asm
	{
		mov	eax, cr0
		cmp[state], 1
		je	enable
		jmp disable
		enable :
		or eax, 0x80000000		//set bit 31
			mov	cr0, eax
			jmp done
			disable :
		and eax, 0x7FFFFFFF		//clear bit 31
			mov	cr0, eax
			done :
	}
#endif
}

void kmain(unsigned long magic, unsigned long addr)
{
	SkyConsole::Initialize();
	//InitializePageTable();	
	
//	SkyConsole::Initialize();
	//GDTInitialize();
//	HardwareInitialize();
//	EnablePaging(false);
	/*SkyConsole::Initialize();

	SkyConsole::Print("32Bit Kernel Entered..\n");

	if (DetectionCPUID() == false)
		return;

	SkyConsole::Print("CPUID Detected..\n");

	if (IsLongModeCheckPossible() == false)
		return;

	SkyConsole::Print("Long Mode Check Possible..\n");

	if (IsLongModePossible() == false)
		return;

	SkyConsole::Print("Long Mode Possible..\n");
	*/
	const multiboot_info_t* mb_info = (multiboot_info_t*)addr;            /* Make pointer to multiboot_info_t struct */
	uint32_t mb_flags = mb_info->flags;                  /* Get flags from mb_info */

	void* kentry = nullptr;                                           /* Pointer to the kernel entry point */

	if (mb_flags & MULTIBOOT_INFO_MODS)
	{
		uint32_t mods_count = mb_info->mods_count;   /* Get the amount of modules available */
		uint32_t mods_addr = (uint32_t)mb_info->Modules;     /* And the starting address of the modules */

		for (uint32_t mod = 0; mod < mods_count; mod++)
		{
			Module* module = (Module*)(mods_addr + (mod * sizeof(Module)));     /* Loop through all modules */

			const char* module_string = (const char*)module->Name;
			/* Here I check if module_string is equals to the one i assigned my kernel
			you could skip this check if you had a way of determining the kernel module */

			SkyConsole::Print("Module Name : %s\n", module_string);

			if (strcmp(module_string, g_szKernelName) == 0)
			{
				SkyConsole::Print("64 Kernel Found. Name : %s\n", module_string);
				SkyConsole::Print("Start : %x, End : %x\n", module->ModuleStart, module->ModuleEnd);

				SkyConsole::Print("Calcalate 64 Entry Point\n", module_string);
				kernelEntry = FindKernel64Entry(module_string, (char*)module->ModuleStart);

				if (kernelEntry != 0)
				{
					SkyConsole::Print("SkyOS64 Entry Point 0x%x\n", kernelEntry);
					SkyConsole::Print("Module Size %x\n", (int)module->ModuleEnd - (int)module->ModuleStart);
					

					int skyos64 = 0x00180000;
					memcpy((void*)skyos64, (void*)module->ModuleStart, (int)module->ModuleEnd - (int)module->ModuleStart);
					char* aaa = (char*)skyos64;

					for (int i = 0; i < (int)module->ModuleEnd - (int)module->ModuleStart; i++)
					{
						if (aaa[i] == 'm')
							if (aaa[i + 1] == 'a')
								if (aaa[i + 2] == 'r')
								{
									SkyConsole::Print("detected\n");
									break;
								}
					}
					
					GDTInitialize();
					InitializePageTable();

					//PAGE64();
					//for (;;);
					
					EnterKernel64((void*)kernelEntry, nullptr, module);

				}
			}
		}
	}
	for (;;);

}


bool DetectionCPUID()
{
	bool result = false;
	__asm
	{
		pushfd
		pop eax

		; Copy to ECX as well for comparing later on
		mov ecx, eax

		; Flip the ID bit
		xor eax, 1 << 21

		; Copy EAX to FLAGS via the stack
		push eax
		popfd

		; Copy FLAGS back to EAX(with the flipped bit if CPUID is supported)
		pushfd
		pop eax

		; Restore FLAGS from the old version stored in ECX(i.e.flipping the ID bit
		; back if it was ever flipped).
		push ecx
		popfd

		; Compare EAX and ECX.If they are equal then that means the bit wasn't
		; flipped, and CPUID isn't supported.
		xor eax, ecx
		jz NoCPUID
		mov result, 1
		NoCPUID:

	}

	return result;
}

bool IsLongModeCheckPossible()
{
	bool result = false;
	__asm
	{
		mov eax, 0x80000000; Set the A - register to 0x80000000.
		cpuid; CPU identification.
		cmp eax, 0x80000001; Compare the A - register with 0x80000001.
		jb NoLongMode; It is less, there is no long mode.
		mov result, 1
		NoLongMode:
	}

	return result;
}

bool IsLongModePossible()
{
	bool result = false;
	__asm
	{
		mov eax, 0x80000001; Set the A - register to 0x80000001.
		cpuid; CPU identification.
		test edx, 1 << 29; Test if the LM - bit, which is bit 29, is set in the D - register.
		jz NoLongMode; They aren't, there is no long mode.
		mov result, 1
		NoLongMode:
	}

	return result;
}

uint32_t FindKernel64Entry(const char* szFileName, char* buf)
{
	if (!ValidatePEImage(buf)) {
		SkyConsole::Print("Invalid PE Format!! %s\n", szFileName);
		return 0;
	}

	IMAGE_DOS_HEADER* dosHeader = 0;
	IMAGE_NT_HEADERS64* ntHeaders = 0;

	SkyConsole::Print("Valid PE Format %s\n", szFileName);

	dosHeader = (IMAGE_DOS_HEADER*)buf;
	ntHeaders = (IMAGE_NT_HEADERS64*)(dosHeader->e_lfanew + (uint32_t)buf);
	SkyConsole::Print("sizeofcode 0x%x\n", ntHeaders->OptionalHeader.Magic);

	uint32_t entryPoint = (uint32_t)ntHeaders->OptionalHeader.AddressOfEntryPoint + ntHeaders->OptionalHeader.ImageBase;
	return 	entryPoint;
}

void EnterKernel64(void* entry, multiboot_info* info, Module* module)
{	
	unsigned long regCR4 = __readcr4();
	__asm or regCR4, 0x20
	__writecr4(regCR4);
	__asm
	{
		mov eax, 0x60000
		mov cr3, eax

		; IA_EFER 레지스터의 LME 비트를 활성화
		mov ecx, 0xC0000080
		rdmsr
		or eax, 0x0100
		wrmsr

		; Write Table
		mov eax, cr0
		or eax, 0xE0000000
		xor eax, 0x60000000
		; NW(29) = 0, CD(30) = 0, PG(31) = 1
		mov cr0, eax
	}

	int skyos64 = 0x00180000;
	memcpy((void*)skyos64, (void*)module->ModuleStart, (int)module->ModuleEnd - (int)module->ModuleStart);
	char* aaa = (char*)skyos64;

	for (int i = 0; i < (int)module->ModuleEnd - (int)module->ModuleStart; i++)
	{
		if (aaa[i] == 'm')
			if (aaa[i + 1] == 'a')
				if (aaa[i + 2] == 'r')
				{
					SkyConsole::Print("detected\n");
					break;
				}
	}
	
	ModeSwitchAndJumpKernel64();

	__asm
	{

		\
		; IA - 32e 세그먼트 설렉트 후 2MB 영역으로 점프
		jmp cs:entry

		; Not Entry
		jmp $

	}
}

void EnterKernel64_3(void* entry, multiboot_info* info, Module* module)
{
	SkyConsole::Print("sdffsd");
	__asm
	{
		push	ebp
		mov	ebp, esp; Set up the stack so the variables passed from the C code can be read

		mov	esi, [ebp + 8]; This is the kernel entry point
		mov[k_ptr], esi

		mov	ax, 0x10; Reload data segment selectors
		mov	ss, ax
		mov	ds, ax
		mov	es, ax
		hlt

		jmp	cs : jmp_k; Reload code selector by jumping to 64 - bit code
		jmp_k :

		mov	edi, [ebp + 12]; 1st argument of kernel_main(pointer to multiboot structure)
			mov	eax, [k_ptr]; This is transformed to mov rax, [k_ptr] and uses the double word reserved below
			dd(0); Trick the processor, contains high address of k_ptr; as higher half of the address to k_ptr

			jmp	eax; This part is plain bad, tricking the processor is not the best thing to do here
			k_ptr :
		dd(0)
	}

}









void HardwareInitialize()
{
	GDTInitialize();
	IDTInitialize(0x8);
	PICInitialize(0x20, 0x28);
	InitializePIT();
}