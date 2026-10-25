#pragma once

#define EFIAPI __attribute__((ms_abi))

typedef unsigned long long UINTN;
typedef unsigned int UINT32;
typedef unsigned short CHAR16;
typedef unsigned char UINT8;
typedef void* EFI_HANDLE;

typedef struct {
    UINT32 Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} EFI_GUID;

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    int PixelFormat;
    int PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    unsigned long long FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    long long (EFIAPI *QueryMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber,
        UINTN *SizeOfInfo,
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );
    long long (EFIAPI *SetMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber
    );
    long long (EFIAPI *Blt)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
        EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
        UINTN SourceX, UINTN SourceY,
        UINTN DestinationX, UINTN DestinationY,
        UINTN Width, UINTN Height,
        UINTN Delta
    );
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    unsigned long long Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL; void* RestoreTPL;
    long long (EFIAPI *AllocatePages)(int Type, int MemoryType, UINTN Pages, unsigned long long *Memory);
    void* FreePages;
    long long (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
    void* AllocatePool; void* FreePool;
    void* CalculateCrc32;
    void* CreateEvent; void* SetTimer; void* WaitForEvent; void* SignalEvent; void* CloseEvent; void* CheckEvent;
    void* InstallProtocolInterface; void* ReinstallProtocolInterface; void* UninstallProtocolInterface; void* HandleProtocol;
    void* VoidReserved; void* RegisterProtocolNotify;
    void* LocateHandle; void* LocateDevicePath; void* InstallConfigurationTable;
    void* LoadImage; void* StartImage; void* Exit; void* UnloadImage;
    long long (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void* GetNextMonotonicCount; void* Stall; void* SetWatchdogTimer;
    void* ConnectController; void* DisconnectController;
    void* OpenProtocol; void* CloseProtocol; void* ProtocolPerHandle;
    void* LocateHandleBuffer;
    long long (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

// Структура конфигурационной таблицы UEFI (нужна для поиска RSDP)
typedef struct {
    EFI_GUID VendorGuid;
    void*    VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    void* ConsoleInHandle; void* ConIn;
    void* ConsoleOutHandle; void* ConOut;
    void* StandardErrorHandle; void* StdErr;
    void* RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;               // Количество таблиц конфигурации
    EFI_CONFIGURATION_TABLE *ConfigurationTable; // Массив таблиц конфигурации
} EFI_SYSTEM_TABLE;

// Структура передачи данных из загрузчика в ядро
typedef struct {
    void* FrameBufferBase;
    unsigned long long FrameBufferSize;
    unsigned int HorizontalResolution;
    unsigned int VerticalResolution;
    unsigned int PixelsPerScanLine;
    void* VirtualFrameBuffer;
    void* SystemTable;                        // Указатель на EFI_SYSTEM_TABLE для ядра
} BootInfo;
