#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
LONG __Printf(__reg("a6") void *, __reg("a0") CONST_STRPTR format, ...)="\tmove.l\ta0,d1\n\tmove.l\td2,-(a7)\n\tmove.l\ta7,d2\n\taddq.l\t#4,d2\n\tjsr\t-954(a6)\n\tmove.l\t(a7)+,d2";
#define Printf(...) __Printf(DOSBase, __VA_ARGS__)
#endif

#include "CiaTimer.h"
#include "TimingFunctions.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

const char Version[] = "$VER: TestResidentSpeed 0.59 (10.1.2022) by Patrik Axelsson";

enum ComponentType {
	ComponentType_None,
	ComponentType_Memory,
	ComponentType_Resident,
	ComponentType_LibBase,
};

struct Component {
	enum ComponentType type;
	void *address;
	size_t size;
	char name[32];
};

static bool IsLink(struct Resident *resident);
static struct Resident **GetLinkAddr(struct Resident *resident);
static void PrintResidentList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, const struct Resident **residentList, struct Resident **seenResidents, void *bestMem, bool showAll, bool verbose);
static void PrintResident(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct Resident *resident, void *bestMem, bool showAll, bool verbose);
static void PrintLibraryList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct List *libList, void *bestMem, bool showAll, bool verbose);
static void PrintLibrary(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct Library *libBase, void *bestMem, bool showAll, bool verbose);

static void * PrintMemoryList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct List *memList, bool showAll, bool verbose);

LONG TestResidentSpeed(void) {
	struct ExecBase *SysBase = *(struct ExecBase **) 4;
	// Variables needing cleanup at end
	struct DosLibrary *DOSBase = NULL;
	struct RDArgs *args = NULL;
	struct CiaTimer ciaTimerStore;
	struct CiaTimer *ciaTimer = NULL;
	struct Resident **seenResidentPtrList = NULL;
	
	LONG retval = RETURN_ERROR;

	DOSBase = (void *) OpenLibrary("dos.library", 36);
	if(NULL == DOSBase) {
		goto cleanup;
	}
	
	void *argsValues[2] = {0};
	args = ReadArgs("SHOWALL/S,VERBOSE/S", (void *) argsValues, NULL);
	if (NULL == args) {
		PrintFault(IoErr(), NULL);
		goto cleanup;
	}

	const bool showAll = NULL != argsValues[0];
	const bool verbose = NULL != argsValues[1];
	
	ciaTimer = AllocCiaTimer(SysBase, &ciaTimerStore);
	if (NULL == ciaTimer) {
		Printf("Failed allocating a timer!\n");
		goto cleanup;
	}

	seenResidentPtrList = AllocVec(sizeof(struct Resident *) * 1024, MEMF_ANY | MEMF_CLEAR);
	if (NULL == seenResidentPtrList) {
		Printf("Could not allocate buffer");
	}

	StartCiaTimer(ciaTimer);

	if (verbose) {
		Printf("%-8s %-23s %-7s %-4s %-8s %-8s %-6s %-4s %-8s %-6s %-5s %-8s %-7s\n", "Type", "Name", "Ver", "Size", "Address", "Location", "TSize", "EClk", "Speed", "ITSize", "IEClk", "ISpeed", "Speed %");
	}
	else {
		Printf("%-8s %-23s %-7s %-4s %-8s %-8s %-7s\n", "Type", "Name", "Ver", "Size", "Location", "Speed", "Speed %");
	}
	void *bestMem = PrintMemoryList(SysBase, DOSBase, ciaTimer, &SysBase->MemList, showAll, verbose);
	//unsigned maxSpeed = 0;
	PrintResidentList(SysBase, DOSBase, ciaTimer, SysBase->ResModules, seenResidentPtrList, bestMem, showAll, verbose);
	//PrintLibraryList(SysBase, DOSBase, ciaTimer, &SysBase->LibList, bestMem, showAll, verbose);
	//PrintLibraryList(SysBase, DOSBase, ciaTimer, &SysBase->DeviceList, bestMem, showAll, verbose);
	//PrintLibraryList(SysBase, DOSBase, ciaTimer, &SysBase->ResourceList, bestMem, showAll, verbose);

	retval = RETURN_OK;
cleanup:
	FreeVec(seenResidentPtrList);
	FreeCiaTimer(ciaTimer);
	if (NULL != args) {
		FreeArgs(args);
	}
	CloseLibrary((void *) DOSBase);
	
	return retval;
}

static bool IsLink(struct Resident *resident) {
	return (ULONG) resident >> 31 == 1;
}

static struct Resident **GetLinkAddr(struct Resident *resident) {
	return (void *) (((ULONG) resident << 1) >> 1);
}

struct ReadResult {
	size_t length;
	unsigned eClocks;
};


struct InitTable {
	ULONG it_LibBaseSize;
	void **it_FuncTable;
	ULONG *it_DataTable;
	APTR it_InitRoutine;
};

struct HumanSize {
	char string[8];
};

static void SPrintF(struct ExecBase *SysBase, char *buffer, const char *format, ...) {
	static const ULONG putCharProc = 0x16c04e75; // move.b d0,(a3)+; rts
	va_list varArgs;
	va_start(varArgs, format);
	RawDoFmt(format, varArgs, (void *) &putCharProc, buffer);
	va_end(varArgs);
}

struct HumanSize CalcHumanSize(struct ExecBase *SysBase, size_t size) {
	struct HumanSize humanSize;
	humanSize.string[0] = '\0';
	char units[] = {'k', 'M', 'G', 'T'};
	char unit = '\0';
	const char *format = "%lu";
	if (size < 999) {
		SPrintF(SysBase, humanSize.string, "%4lu", size);
	} 
	else {
		for (size_t i = 0; i < sizeof(units); i++) {
			if (size / 1024 < 999) {
				unit = units[i];
				if (size / 1024 < 10) {
					SPrintF(SysBase, humanSize.string, "%lu.%01lu%lc", size / 1024, (((size % 1024) * 999) / 1024) / 100, unit);
				}
				else {
					SPrintF(SysBase, humanSize.string, "%3lu%lc", size / 1024, unit);
				}
				break;
			}

			size /= 1024;
		}
	}

	return humanSize;
}

//TODO: Return calculated start and end of resident
static size_t CalcSizeFromResidentContents(struct Resident *resident) {
	void *minAddress = (void *) resident;
	void *maxAddress = (void *) (resident + 1);

	const struct InitTable *initTable = (void *) resident->rt_Init;
	const bool isAutoInit = NULL != initTable && RTF_AUTOINIT & resident->rt_Flags;

	const void * const addressList[] = {
		resident->rt_Name,
		resident->rt_IdString,
		resident->rt_Init,
		isAutoInit ? initTable->it_FuncTable : NULL,
		isAutoInit ? initTable->it_DataTable : NULL,
		isAutoInit ? initTable->it_InitRoutine : NULL
	};

	for (size_t i = 0; i < ARRAY_SIZE(addressList); i++) {
		const void *address = addressList[i];

		if (address != NULL) {
			if (address < minAddress) {
				minAddress = address;
			} 
			if (address > maxAddress) {
				maxAddress = address;
			}
		}
	}

	if (isAutoInit) {
		if (-1 == *(WORD *) initTable->it_FuncTable) {
			WORD minFuncOffset = 0;
			WORD maxFuncOffset = 0;
			WORD funcOffset;
			WORD *funcOffsetTable = ((WORD *) initTable->it_FuncTable) + 1;
			while (-1 != (funcOffset = *funcOffsetTable++)) {
				if (funcOffset < minFuncOffset) {
					minFuncOffset = funcOffset;
				}
				if (funcOffset > maxFuncOffset) {
					maxFuncOffset = funcOffset;
				}
			}

			if ((void *) funcOffsetTable > maxAddress) {
				maxAddress = funcOffsetTable;
			}
			
			void *minFunctionAddress = (uint8_t *) initTable->it_FuncTable + minFuncOffset;
			if (minFunctionAddress < minAddress) {
				minAddress = minFunctionAddress;
			}
			void *maxFunctionAddress = (uint8_t *) initTable->it_FuncTable + maxFuncOffset;
			if (maxFunctionAddress > maxAddress) {
				maxAddress = maxFunctionAddress;
			}
		}
		else {
			void *function;
			void **functionTable = initTable->it_FuncTable;;
			while (-1 != (ULONG) (function = *functionTable++)) {
				if (function < minAddress) {
					minAddress = function;
				}
				if (function > maxAddress) {
					maxAddress = function;
				}
			}
			if ((void *) functionTable > maxAddress) {
				maxAddress = functionTable;
			}
		}
	}

	return (uint8_t *) maxAddress - (uint8_t *) minAddress;
}

static struct TimingResult TimeReads(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, void *addr, size_t length) {

	struct TimingResult medianResult;
	size_t proposedTestLength = 0;
	do {
		proposedTestLength += 256;
		const size_t testLength = proposedTestLength < length ? proposedTestLength : length;
		// Make several tests and get the median to filter out oddly slow results (memory refresh?)
		struct TimingResult results[5];
		for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
			Disable();
			CacheClearU();
			// Dry run to populate the instruction cache
			uint8_t testBuffer[384];
			TimeConsequtiveReads(ciaTimer->registers.lowByte, testBuffer, length < sizeof(testBuffer) ? length : sizeof(testBuffer)); 
			results[i] = TimeConsequtiveReads(ciaTimer->registers.lowByte, addr, testLength);
			Enable();
		}

		for (size_t i = 0; i < ARRAY_SIZE(results) - 1; i++) {
			uint32_t currentMinEClocks = results[i].eClocks;
			size_t minIndex = i;
			for (size_t j = i; j < ARRAY_SIZE(results); j++) {
				if (results[j].eClocks < currentMinEClocks) {
					currentMinEClocks = results[j].eClocks;
					minIndex = j;
				}
			}
			struct TimingResult tmpResult = results[i];
			results[i] = results[minIndex];
			results[minIndex] = tmpResult;
		}

		/*for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
			Printf("%lu: %lu %lu %lu\n", i, testLength, results[i].eClocks, results[i].actualLength);
		}*/

		medianResult = results[ARRAY_SIZE(results) >> 1];

		if (medianResult.eClocks > 128) {
			break;
		}
	} while (proposedTestLength < length);


	return medianResult;
}


static bool IsPrintableIso8859(uint8_t c) {
	return !(c < 0x20 || (c > 0x7e && c < 0xa0));
}

static const char *GetLocation(struct ExecBase *SysBase, void *addr) {
	const ULONG memType = TypeOfMem(addr);
	const size_t addrNum = (size_t) addr; 
	
	return memType & MEMF_CHIP ?
			"Chip RAM" :
			memType & MEMF_FAST ?
					"Fast RAM" :
					(addrNum >= 0xe00000 && addrNum <= 0xefffff) ?
							"Ext  ROM" :
							(addrNum >= 0xF00000 && addrNum <= 0xf7ffff) ?
									"Diag ROM" :
									(addrNum >= 0xf80000 && addrNum <= 0xffffff) ?
											"Kick ROM" :
											"Unknown";
}

static void PrintResident(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct Resident *resident, void *bestMem, bool showAll, bool verbose) {
	const size_t endskipSize = (resident->rt_EndSkip >= (void *) resident) ?
			((uint8_t *) resident->rt_EndSkip - (uint8_t *) resident) :
			0;
	const size_t calcSize = CalcSizeFromResidentContents(resident);
	const size_t size = calcSize > endskipSize ? calcSize : endskipSize;
	uint8_t sanitizedName[24];
	int i = 0;
	int j = 0;
	while (j < (sizeof(sanitizedName) - 1)) {
		const uint8_t c = resident->rt_Name[i++];
		if (c == '\0')
			break;
		if(IsPrintableIso8859(c)) {
			sanitizedName[j++] = c;
		}
	}
	sanitizedName[j] = '\0';
	

	const struct TimingResult result = TimeReads(SysBase, DOSBase, ciaTimer, resident, size);
	const uint32_t speed = result.eClocks != 0 ?
			(uint32_t) ((((uint64_t) result.actualLength) * SysBase->ex_EClockFrequency) / result.eClocks) :
			-1;
	const struct TimingResult resultBestMem = TimeReads(SysBase, DOSBase, ciaTimer, (uint8_t *) bestMem + (((size_t) resident) & 0xff), size);
	const uint32_t speedBestMem = resultBestMem.eClocks != 0 ?
			(uint32_t) ((((uint64_t) resultBestMem.actualLength) * SysBase->ex_EClockFrequency) / resultBestMem.eClocks) :
			-1;
	const unsigned percentage = 0 != speedBestMem ?
			((uint64_t) speed * 100 * 10) / speedBestMem :
			0;
	const bool sameSize = result.actualLength == resultBestMem.actualLength;
	const bool oneEclockMore = result.eClocks == resultBestMem.eClocks + 1;
	if (showAll || percentage < 98 * 10 && !(sameSize && oneEclockMore)) {
		const struct HumanSize humanSize = CalcHumanSize(SysBase, size);
		if (verbose) {
			Printf(
					"%-8s %-23s %3lu     %s %08lx %-8s %6lu %4lu %4lu.%03lu %6lu %5lu %4lu.%03lu %5lu.%01lu\n",
					"Resident",
					sanitizedName,
					resident->rt_Version,
					humanSize.string,
					resident,
					GetLocation(SysBase, resident),
					result.actualLength,
					result.eClocks,
					speed / (1024 * 1024),
					((speed % (1024 * 1024)) * 999) / (1024 * 1024),
					resultBestMem.actualLength,
					resultBestMem.eClocks,
					speedBestMem / (1024 * 1024),
					((speedBestMem % (1024 * 1024)) * 999) / (1024 * 1024),
					percentage / 10,
					percentage % 10

			);
		}
		else {
			Printf(
					"%-8s %-23s %3lu     %s %-8s %4lu.%03lu %5lu.%01lu\n",
					"Resident",
					sanitizedName,
					resident->rt_Version,
					humanSize.string,
					GetLocation(SysBase, resident),
					speed / (1024 * 1024),
					((speed % (1024 * 1024)) * 999) / (1024 * 1024),
					percentage / 10,
					percentage % 10
			);
		
		}
	}
	struct Library *libBase = (void *) FindName(&SysBase->LibList, resident->rt_Name);
	if (NULL != libBase) {
		PrintLibrary(SysBase, DOSBase, ciaTimer, libBase, bestMem, showAll, verbose);
	}
	libBase = (void *) FindName(&SysBase->DeviceList, resident->rt_Name);
	if (NULL != libBase) {
		PrintLibrary(SysBase, DOSBase, ciaTimer, libBase, bestMem, showAll, verbose);
	}
	libBase = (void *) FindName(&SysBase->ResourceList, resident->rt_Name);
	if (NULL != libBase) {
		PrintLibrary(SysBase, DOSBase, ciaTimer, libBase, bestMem, showAll, verbose);
	}
}

static void PrintResidentList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, const struct Resident **residentPtrList, struct Resident **seenResidentPtrList, void *bestMem, bool showAll, bool verbose) {
	for (struct Resident **residentPtr = residentPtrList; *residentPtr != NULL; residentPtr++) {
		struct Resident *resident = *residentPtr;
		if (IsLink(resident)) {
			const struct Resident **residentLinkPtr = GetLinkAddr(resident);
			//Printf("--- Link to resident list at %08lx\n", residentLinkPtr);
			PrintResidentList(SysBase, DOSBase, ciaTimer, residentLinkPtr, seenResidentPtrList, bestMem, showAll, verbose);
			//Printf("--- Back from link to resident list at %08lx\n", residentLinkPtr);
		}
		else if (resident->rt_MatchWord == RTC_MATCHWORD && resident == FindResident(resident->rt_Name)) {
			struct Resident **currentSeenResident = seenResidentPtrList;
			struct Resident **seenResidentPtr;
			bool residentSeen = false;
			for (seenResidentPtr = seenResidentPtrList; *seenResidentPtr != NULL; seenResidentPtr++) {
				if (*seenResidentPtr == resident) {
					residentSeen = true;
					break;
				}
			}

			if (residentSeen) {
				continue;
			}
			else {
				*seenResidentPtr = resident;
			}

			PrintResident(SysBase, DOSBase, ciaTimer, resident, bestMem, showAll, verbose);

		}
	}
}

static void PrintLibrary(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct Library *libBase, void *bestMem, bool showAll, bool verbose) {
	const size_t size = (size_t) libBase->lib_PosSize + libBase->lib_NegSize;
	void *startAddr = (uint8_t *) libBase - libBase->lib_NegSize;
	uint8_t sanitizedName[24];
	sanitizedName[0] = '\0';
	int i = 0;
	int j = 0;
	if (NULL != libBase->lib_Node.ln_Name) {
		if (0 == strcmp("FileSystem.resource", libBase->lib_Node.ln_Name)) {
			return;
		}
		while (j < (sizeof(sanitizedName) - 1)) {
			const uint8_t c = libBase->lib_Node.ln_Name[i++];
			if (c == '\0')
				break;
			if(IsPrintableIso8859(c)) {
				sanitizedName[j++] = c;
			}
		}
	}
	sanitizedName[j] = '\0';

	const struct TimingResult result = TimeReads(SysBase, DOSBase, ciaTimer, startAddr, size);
	const uint32_t speed = result.eClocks != 0 ?
			(uint32_t) ((((uint64_t) result.actualLength) * SysBase->ex_EClockFrequency) / result.eClocks) :
			-1;
	const struct TimingResult resultBestMem = TimeReads(SysBase, DOSBase, ciaTimer, (uint8_t *) bestMem + (((size_t) startAddr) & 0xff), size);
	const uint32_t speedBestMem = resultBestMem.eClocks != 0 ?
			(uint32_t) ((((uint64_t) resultBestMem.actualLength) * SysBase->ex_EClockFrequency) / resultBestMem.eClocks) :
			-1;
	const unsigned percentage = 0 != speedBestMem ?
			((uint64_t) speed * 100 * 10) / speedBestMem :
			0;
			const bool sameSize = result.actualLength == resultBestMem.actualLength;
			const bool oneEclockMore = result.eClocks == resultBestMem.eClocks + 1;
	if (showAll || percentage < 98 * 10 && !(sameSize && oneEclockMore)) {
		const struct HumanSize humanSize = CalcHumanSize(SysBase, size); 
		if (verbose) {
			Printf(
					"%-8s %-23s %3lu.%-3lu %s %08lx %-8s %6lu %4lu %4lu.%03lu %6lu %5lu %4lu.%03lu %5lu.%01lu\n",
					"LibBase",
					sanitizedName,
					libBase->lib_Version,
					libBase->lib_Revision,
					humanSize.string,
					libBase,
					GetLocation(SysBase, libBase),
					result.actualLength,
					result.eClocks,
					speed / (1024 * 1024),
					((speed % (1024 * 1024)) * 999) / (1024 * 1024),
					resultBestMem.actualLength,
					resultBestMem.eClocks,
					speedBestMem / (1024 * 1024),
					((speedBestMem % (1024 * 1024)) * 999) / (1024 * 1024),
					percentage / 10,
					percentage % 10

			);
		}
		else {
			Printf(
					"%-8s %-23s %3lu.%-3lu %s %-8s %4lu.%03lu %5lu.%01lu\n",
					"LibBase",
					sanitizedName,
					libBase->lib_Version,
					libBase->lib_Revision,
					humanSize.string,
					GetLocation(SysBase, libBase),
					speed / (1024 * 1024),
					((speed % (1024 * 1024)) * 999) / (1024 * 1024),
					percentage / 10,
					percentage % 10

			);
		}
	}
}

static void PrintLibraryList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct List *libList, void *bestMem, bool showAll, bool verbose) {

	struct Library *libBase;
	for (libBase = (void *) libList->lh_Head; libBase->lib_Node.ln_Succ; libBase = (void *) libBase->lib_Node.ln_Succ) {
		PrintLibrary(SysBase, DOSBase, ciaTimer, libBase, bestMem, showAll, verbose);
	}
}

static void *PrintMemoryList(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct List *memList, bool showAll, bool verbose) {
	struct MemHeader *mem;
	void *bestMem = NULL;
	for (mem = (void *) memList->lh_Head; NULL != mem->mh_Node.ln_Succ; mem = (void *) mem->mh_Node.ln_Succ) {
		{
			const size_t size = (uint8_t *) mem->mh_Upper - (uint8_t *) mem;
			void *startAddr = (uint8_t *) mem;
			uint8_t sanitizedName[29];
			sanitizedName[0] = '\0';
			int i = 0;
			int j = 0;
			if (NULL != mem->mh_Node.ln_Name) {
				while (j < (sizeof(sanitizedName) - 1)) {
					const uint8_t c = mem->mh_Node.ln_Name[i++];
					if (c == '\0')
						break;
					if (IsPrintableIso8859(c)) {
						sanitizedName[j++] = c;
					}
				}
				sanitizedName[j] = '\0';
			}
		
			const struct TimingResult result = TimeReads(SysBase, DOSBase, ciaTimer, startAddr, size);
			const uint32_t speed = result.eClocks != 0 ?
					(uint32_t) ((((uint64_t) result.actualLength) * SysBase->ex_EClockFrequency) / result.eClocks) :
					0;
			if (showAll) {
				const struct HumanSize humanSize = CalcHumanSize(SysBase, size); 
				if (verbose) {
					Printf(
							"%-8s %-23s         %s %08lx %-8s %6lu %4lu %4lu.%03lu\n",
							"Memory",
							sanitizedName,
							humanSize.string,
							mem,
							GetLocation(SysBase, (uint8_t *) mem + 128),
							result.actualLength,
							result.eClocks,
							speed / (1024 * 1024),
							((speed % (1024 * 1024)) * 999) / (1024 * 1024)

					);
				}
				else {
					Printf(
							"%-8s %-23s         %s %-8s %4lu.%03lu\n",
							"Memory",
							sanitizedName,
							humanSize.string,
							GetLocation(SysBase, (uint8_t *) mem + 128),
							speed / (1024 * 1024),
							((speed % (1024 * 1024)) * 999) / (1024 * 1024)
					);
				}
			}
			if (NULL == bestMem) {
				bestMem = mem;
			}
		}
	}
	return bestMem;
}
