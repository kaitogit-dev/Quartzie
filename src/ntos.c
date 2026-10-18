#include <stdint.h>
#include <stddef.h>

#define STATUS_SUCCESS ((int32_t)0)
#define STATUS_INVALID_HANDLE ((int32_t)0xc0000008u)
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000du)
#define STATUS_OBJECT_NAME_NOT_FOUND ((int32_t)0xc0000034u)
#define STATUS_OBJECT_PATH_NOT_FOUND ((int32_t)0xc000003au)
#define STATUS_OBJECT_NAME_COLLISION ((int32_t)0xc0000035u)
#define STATUS_ACCESS_DENIED ((int32_t)0xc0000022u)
#define STATUS_INSUFFICIENT_RESOURCES ((int32_t)0xc000009au)
#define STATUS_END_OF_FILE ((int32_t)0xc0000011u)
#define STATUS_DISK_FULL ((int32_t)0xc000007fu)
#define STATUS_IO_DEVICE_ERROR ((int32_t)0xc0000185u)
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bbu)
#define STATUS_BUFFER_TOO_SMALL ((int32_t)0xc0000023u)
#define NT_SUCCESS(s) ((s) >= 0)
#define FILE_OPEN 1u
#define FILE_CREATE 2u
#define FILE_OPEN_IF 3u
#define FILE_OVERWRITE_IF 5u
#define FILE_READ_DATA 0x0001u
#define FILE_WRITE_DATA 0x0002u
#define FILE_READ_ATTRIBUTES 0x0080u
#define FILE_SYNCHRONOUS_IO_NONALERT 0x20u
#define IRP_MJ_CREATE 0u
#define IRP_MJ_CLOSE 2u
#define IRP_MJ_READ 3u
#define IRP_MJ_WRITE 4u
#define IRP_MJ_QUERY_INFORMATION 5u
#define IRP_MJ_FLUSH_BUFFERS 9u
#define NT_HANDLE_SLOTS 24
#define NT_REG_KEY_SLOTS 8
#define NT_REG_MAX_VALUES 8
#define REG_DWORD 4u
#define NT_FILE_STANDARD_INFORMATION 5u

typedef int32_t NTSTATUS;
typedef uint32_t HANDLE;
typedef uint32_t ACCESS_MASK;
typedef struct { uint16_t Length, MaximumLength; const uint16_t *Buffer; } UNICODE_STRING;
typedef struct { uint32_t Length; HANDLE RootDirectory; const UNICODE_STRING *ObjectName; uint32_t Attributes; void *SecurityDescriptor; void *SecurityQualityOfService; } OBJECT_ATTRIBUTES;
typedef struct { NTSTATUS Status; uintptr_t Information; } IO_STATUS_BLOCK;
typedef struct { int64_t AllocationSize, EndOfFile; uint32_t NumberOfLinks; uint8_t DeletePending, Directory; uint8_t Spare[2]; } FILE_STANDARD_INFORMATION;

typedef struct _IRP IRP;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef NTSTATUS (*PDRIVER_DISPATCH)(DEVICE_OBJECT *, IRP *);
struct _DRIVER_OBJECT { const char *DriverName; PDRIVER_DISPATCH MajorFunction[16]; uint32_t DeviceCount; };
struct _DEVICE_OBJECT { const char *DeviceName; DRIVER_OBJECT *DriverObject; uint32_t Flags; };
typedef struct { DEVICE_OBJECT *DeviceObject; uint8_t MajorFunction; uint8_t MinorFunction; uint16_t Flags; } IO_STACK_LOCATION;
struct _IRP { IO_STATUS_BLOCK IoStatus; IO_STACK_LOCATION Stack; void *SystemBuffer; uint32_t Length; uint32_t Offset; uint32_t Access; uint32_t Disposition; uint32_t BackendHandle; const char *FileName; };

static DRIVER_OBJECT nt_disk_driver;
static DEVICE_OBJECT nt_disk_device;
static int nt_driver_ready;
static char nt_dos_link[24];
static struct { uint16_t Generation; uint8_t Type; uint8_t Owner; uint32_t Backend; uint32_t Access; char Name[20]; } nt_handles[NT_HANDLE_SLOTS];
static uint32_t nt_irp_total;
static uint32_t nt_irp_failure;
static uint32_t nt_registry_generation;
static int nt_registry_dirty;
static int nt_registry_loaded;
static struct { char Name[20]; uint32_t Type; uint32_t Data; uint8_t Active; uint8_t Reserved[3]; } nt_registry_values[NT_REG_MAX_VALUES];

static int nt_name_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return 0; i++; }
    return a[i] == 0 && b[i] == 0;
}

static void nt_ascii_copy(char *dst, const char *src, unsigned cap) {
    unsigned i = 0;
    if (cap == 0) return;
    while (i + 1 < cap && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int RtlInitUnicodeStringFromAscii(UNICODE_STRING *unicode, uint16_t *buffer, int capacity, const char *ascii) {
    if (!unicode || !buffer || !ascii || capacity <= 0) return 0;
    int n = 0;
    while (ascii[n] && n < capacity - 1) { buffer[n] = (uint8_t)ascii[n]; n++; }
    if (ascii[n]) return 0;
    buffer[n] = 0;
    unicode->Buffer = buffer;
    unicode->Length = (uint16_t)(n * 2);
    unicode->MaximumLength = (uint16_t)(capacity * 2);
    return 1;
}

static int nt_unicode_ascii(const UNICODE_STRING *str, char *ascii, unsigned capacity) {
    if (!str || !str->Buffer || str->Length % 2 || !capacity || str->Length / 2 >= capacity) return 0;
    unsigned n = str->Length / 2;
    for (unsigned i = 0; i < n; i++) {
        uint16_t c = str->Buffer[i];
        if (c < 32 || c > 126) return 0;
        ascii[i] = (char)c;
    }
    ascii[n] = 0;
    return 1;
}

static uint32_t nt_make_handle(int slot) { return ((uint32_t)nt_handles[slot].Generation << 16) | (uint32_t)(slot + 1); }
static int nt_valid_handle(HANDLE value, int type) {
    unsigned n = value & 0xffffu;
    if (n == 0 || n > NT_HANDLE_SLOTS || (value >> 16) == 0) return -1;
    int slot = (int)n - 1;
    if (nt_handles[slot].Generation != (uint16_t)(value >> 16) || nt_handles[slot].Type != type) return -1;
    return slot;
}
static HANDLE nt_alloc_handle(int type, int owner) {
    for (int i = 0; i < NT_HANDLE_SLOTS; i++) {
        if (nt_handles[i].Type) continue;
        nt_handles[i].Generation++;
        if (!nt_handles[i].Generation) nt_handles[i].Generation = 1;
        nt_handles[i].Type = (uint8_t)type;
        nt_handles[i].Owner = (uint8_t)owner;
        nt_handles[i].Backend = 0;
        nt_handles[i].Access = 0;
        return nt_make_handle(i);
    }
    return 0;
}

static NTSTATUS IoCreateDriver(const char *name, DRIVER_OBJECT *driver) {
    if (!name || !driver) return STATUS_INVALID_PARAMETER;
    driver->DriverName = name;
    driver->DeviceCount = 0;
    for (int i = 0; i < 16; i++) driver->MajorFunction[i] = NULL;
    return STATUS_SUCCESS;
}
static NTSTATUS IoCreateDevice(DRIVER_OBJECT *driver, const char *name, DEVICE_OBJECT *device) {
    if (!driver || !name || !device) return STATUS_INVALID_PARAMETER;
    device->DeviceName = name;
    device->DriverObject = driver;
    device->Flags = 0;
    driver->DeviceCount++;
    return STATUS_SUCCESS;
}
static NTSTATUS IoCreateSymbolicLink(const char *dos_name, const char *device_name) {
    if (!nt_name_equal(dos_name, "\\??\\D:") || !nt_name_equal(device_name, nt_disk_device.DeviceName)) return STATUS_OBJECT_PATH_NOT_FOUND;
    nt_ascii_copy(nt_dos_link, dos_name, sizeof(nt_dos_link));
    return STATUS_SUCCESS;
}
static NTSTATUS IoCallDriver(DEVICE_OBJECT *device, IRP *irp) {
    if (!device || !irp || !device->DriverObject) return STATUS_INVALID_PARAMETER;
    nt_irp_total++;
    uint8_t major = irp->Stack.MajorFunction;
    PDRIVER_DISPATCH dispatch = major < 16 ? device->DriverObject->MajorFunction[major] : NULL;
    NTSTATUS status = dispatch ? dispatch(device, irp) : STATUS_NOT_SUPPORTED;
    irp->IoStatus.Status = status;
    if (!NT_SUCCESS(status)) nt_irp_failure++;
    return status;
}

static NTSTATUS nt_disk_dispatch(DEVICE_OBJECT *device, IRP *irp) {
    if (device != &nt_disk_device || !storage_ready) return STATUS_IO_DEVICE_ERROR;
    int major = irp->Stack.MajorFunction;
    if (major == IRP_MJ_CREATE) {
        uint32_t flags = 0;
        if (irp->Access & FILE_READ_DATA) flags |= CM_OPEN_READ;
        if (irp->Access & FILE_WRITE_DATA) flags |= CM_OPEN_WRITE;
        if (irp->Disposition == FILE_CREATE || irp->Disposition == FILE_OPEN_IF || irp->Disposition == FILE_OVERWRITE_IF) flags |= CM_OPEN_CREATE;
        if (irp->Disposition == FILE_CREATE || irp->Disposition == FILE_OVERWRITE_IF) flags |= CM_OPEN_TRUNCATE;
        int exists = storage_slot(irp->FileName) >= 0;
        if (irp->Disposition == FILE_CREATE && exists) return STATUS_OBJECT_NAME_COLLISION;
        if (irp->Disposition == FILE_OPEN && !exists) return STATUS_OBJECT_NAME_NOT_FOUND;
        irp->BackendHandle = cm_file_open((int)irp->Access >> 16, irp->FileName, flags);
        if (!irp->BackendHandle) return STATUS_IO_DEVICE_ERROR;
        irp->IoStatus.Information = exists ? 1u : 2u;
        return STATUS_SUCCESS;
    }
    if (major == IRP_MJ_READ) {
        int count = cm_file_read(irp->BackendHandle, irp->SystemBuffer, (int)irp->Length);
        if (count < 0) return STATUS_ACCESS_DENIED;
        irp->IoStatus.Information = (uintptr_t)count;
        return count ? STATUS_SUCCESS : STATUS_END_OF_FILE;
    }
    if (major == IRP_MJ_WRITE) {
        int count = cm_file_write(irp->BackendHandle, irp->SystemBuffer, (int)irp->Length);
        if (count < 0) return STATUS_DISK_FULL;
        irp->IoStatus.Information = (uintptr_t)count;
        return STATUS_SUCCESS;
    }
    if (major == IRP_MJ_FLUSH_BUFFERS) return cm_file_flush(irp->BackendHandle) ? STATUS_SUCCESS : STATUS_IO_DEVICE_ERROR;
    if (major == IRP_MJ_CLOSE) return cm_file_close(irp->BackendHandle) ? STATUS_SUCCESS : STATUS_IO_DEVICE_ERROR;
    if (major == IRP_MJ_QUERY_INFORMATION) {
        int slot = cm_handle_slot(irp->BackendHandle, CM_OBJECT_FILE);
        if (slot < 0 || irp->Length < sizeof(FILE_STANDARD_INFORMATION)) return STATUS_BUFFER_TOO_SMALL;
        FILE_STANDARD_INFORMATION *info = (FILE_STANDARD_INFORMATION *)irp->SystemBuffer;
        info->AllocationSize = STORAGE_MAX_BYTES;
        info->EndOfFile = cm_files[slot].length;
        info->NumberOfLinks = 1;
        info->DeletePending = 0;
        info->Directory = 0;
        info->Spare[0] = info->Spare[1] = 0;
        irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);
        return STATUS_SUCCESS;
    }
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS NtInitializeIoManager(void) {
    NTSTATUS s = IoCreateDriver("\\Driver\\CinmixFS", &nt_disk_driver);
    if (!NT_SUCCESS(s)) return s;
    s = IoCreateDevice(&nt_disk_driver, "\\Device\\CinmixDisk", &nt_disk_device);
    if (!NT_SUCCESS(s)) return s;
    nt_disk_driver.MajorFunction[IRP_MJ_CREATE] = nt_disk_dispatch;
    nt_disk_driver.MajorFunction[IRP_MJ_CLOSE] = nt_disk_dispatch;
    nt_disk_driver.MajorFunction[IRP_MJ_READ] = nt_disk_dispatch;
    nt_disk_driver.MajorFunction[IRP_MJ_WRITE] = nt_disk_dispatch;
    nt_disk_driver.MajorFunction[IRP_MJ_QUERY_INFORMATION] = nt_disk_dispatch;
    nt_disk_driver.MajorFunction[IRP_MJ_FLUSH_BUFFERS] = nt_disk_dispatch;
    nt_driver_ready = NT_SUCCESS(IoCreateSymbolicLink("\\??\\D:", nt_disk_device.DeviceName));
    return nt_driver_ready ? STATUS_SUCCESS : STATUS_OBJECT_PATH_NOT_FOUND;
}

static NTSTATUS nt_path_resolve(const OBJECT_ATTRIBUTES *attr, char *name, unsigned maxlen) {
    char raw[96];
    if (!attr || attr->Length != sizeof(OBJECT_ATTRIBUTES) || attr->RootDirectory || !nt_unicode_ascii(attr->ObjectName, raw, sizeof(raw))) return STATUS_INVALID_PARAMETER;
    const char *rest = NULL;
    const char *dos = "\\??\\D:";
    const char *device = "\\Device\\CinmixDisk";
    int prefix = 0;
    if (nt_driver_ready && nt_name_equal(nt_dos_link, dos)) {
        while (dos[prefix] && raw[prefix] == dos[prefix]) prefix++;
        if (!dos[prefix]) rest = raw + prefix;
    }
    if (!rest) {
        prefix = 0;
        while (device[prefix] && raw[prefix] == device[prefix]) prefix++;
        if (!device[prefix]) rest = raw + prefix;
    }
    if (!rest) {
        const char *alias = "\\Device\\QuartzieDisk";
        prefix = 0;
        while (alias[prefix] && raw[prefix] == alias[prefix]) prefix++;
        if (!alias[prefix]) rest = raw + prefix;
    }
    if (!rest || (*rest != '\\' && *rest != '/')) return STATUS_OBJECT_PATH_NOT_FOUND;
    rest++;
    if (!storage_name_valid(rest)) return STATUS_OBJECT_NAME_NOT_FOUND;
    int n = 0;
    while (rest[n]) n++;
    if ((unsigned)n >= maxlen) return STATUS_BUFFER_TOO_SMALL;
    nt_ascii_copy(name, rest, maxlen);
    return STATUS_SUCCESS;
}

static NTSTATUS NtCreateFile(HANDLE *FileHandle, ACCESS_MASK DesiredAccess, const OBJECT_ATTRIBUTES *ObjectAttributes, IO_STATUS_BLOCK *IoStatusBlock,
                             const int64_t *AllocationSize, uint32_t FileAttributes, uint32_t ShareAccess, uint32_t CreateDisposition,
                             uint32_t CreateOptions, const void *EaBuffer, uint32_t EaLength) {
    (void)AllocationSize; (void)FileAttributes; (void)ShareAccess; (void)EaBuffer;
    if (!FileHandle || !IoStatusBlock || !nt_driver_ready || EaLength || (CreateOptions & ~FILE_SYNCHRONOUS_IO_NONALERT) ||
        !(DesiredAccess & (FILE_READ_DATA | FILE_WRITE_DATA)) || (DesiredAccess & ~(FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES)) ||
        (CreateDisposition != FILE_OPEN && CreateDisposition != FILE_CREATE && CreateDisposition != FILE_OPEN_IF && CreateDisposition != FILE_OVERWRITE_IF)) return STATUS_INVALID_PARAMETER;
    *FileHandle = 0;
    char name[20];
    NTSTATUS status = nt_path_resolve(ObjectAttributes, name, sizeof(name));
    if (!NT_SUCCESS(status)) { IoStatusBlock->Status = status; IoStatusBlock->Information = 0; return status; }
    for (int j = 0; j < NT_HANDLE_SLOTS; j++) {
        if (nt_handles[j].Type == 1 && nt_name_equal(nt_handles[j].Name, name) && CreateDisposition == FILE_CREATE) {
            IoStatusBlock->Status = STATUS_OBJECT_NAME_COLLISION;
            IoStatusBlock->Information = 0;
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }
    HANDLE h = nt_alloc_handle(1, 0);
    if (!h) { IoStatusBlock->Status = STATUS_INSUFFICIENT_RESOURCES; return STATUS_INSUFFICIENT_RESOURCES; }
    int slot = nt_valid_handle(h, 1);
    IRP irp = {0};
    irp.Stack.DeviceObject = &nt_disk_device;
    irp.Stack.MajorFunction = IRP_MJ_CREATE;
    irp.FileName = name;
    irp.Access = DesiredAccess | ((uint32_t)0 << 16);
    irp.Disposition = CreateDisposition;
    status = IoCallDriver(&nt_disk_device, &irp);
    *IoStatusBlock = irp.IoStatus;
    if (!NT_SUCCESS(status)) { nt_handles[slot].Type = 0; return status; }
    nt_handles[slot].Backend = irp.BackendHandle;
    nt_handles[slot].Access = DesiredAccess;
    nt_ascii_copy(nt_handles[slot].Name, name, sizeof(nt_handles[slot].Name));
    *FileHandle = h;
    return STATUS_SUCCESS;
}

static NTSTATUS nt_rw_file(HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock, void *buffer, uint32_t Length, const int64_t *ByteOffset, int write) {
    int slot = nt_valid_handle(FileHandle, 1);
    if (slot < 0) return STATUS_INVALID_HANDLE;
    if (!IoStatusBlock || (!buffer && Length) || Length > STORAGE_MAX_BYTES || (ByteOffset && (*ByteOffset < 0 || *ByteOffset > STORAGE_MAX_BYTES))) return STATUS_INVALID_PARAMETER;
    if (!(nt_handles[slot].Access & (write ? FILE_WRITE_DATA : FILE_READ_DATA))) return STATUS_ACCESS_DENIED;
    if (ByteOffset && !cm_file_seek(nt_handles[slot].Backend, (uint32_t)*ByteOffset)) return STATUS_INVALID_PARAMETER;
    IRP irp = {0};
    irp.Stack.DeviceObject = &nt_disk_device;
    irp.Stack.MajorFunction = write ? IRP_MJ_WRITE : IRP_MJ_READ;
    irp.BackendHandle = nt_handles[slot].Backend;
    irp.SystemBuffer = buffer;
    irp.Length = Length;
    NTSTATUS status = IoCallDriver(&nt_disk_device, &irp);
    *IoStatusBlock = irp.IoStatus;
    return status;
}

static NTSTATUS NtReadFile(HANDLE FileHandle, HANDLE Event, void *ApcRoutine, void *ApcContext, IO_STATUS_BLOCK *IoStatusBlock,
                           void *Buffer, uint32_t Length, const int64_t *ByteOffset, uint32_t *Key) {
    (void)Event; (void)ApcRoutine; (void)ApcContext; (void)Key;
    return nt_rw_file(FileHandle, IoStatusBlock, Buffer, Length, ByteOffset, 0);
}
static NTSTATUS NtWriteFile(HANDLE FileHandle, HANDLE Event, void *ApcRoutine, void *ApcContext, IO_STATUS_BLOCK *IoStatusBlock,
                            const void *Buffer, uint32_t Length, const int64_t *ByteOffset, uint32_t *Key) {
    (void)Event; (void)ApcRoutine; (void)ApcContext; (void)Key;
    return nt_rw_file(FileHandle, IoStatusBlock, (void *)Buffer, Length, ByteOffset, 1);
}
static NTSTATUS NtFlushBuffersFile(HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock) {
    int slot = nt_valid_handle(FileHandle, 1);
    if (slot < 0) return STATUS_INVALID_HANDLE;
    if (!IoStatusBlock) return STATUS_INVALID_PARAMETER;
    IRP irp = {0};
    irp.Stack.MajorFunction = IRP_MJ_FLUSH_BUFFERS;
    irp.BackendHandle = nt_handles[slot].Backend;
    NTSTATUS status = IoCallDriver(&nt_disk_device, &irp);
    *IoStatusBlock = irp.IoStatus;
    return status;
}
static NTSTATUS NtQueryInformationFile(HANDLE FileHandle, IO_STATUS_BLOCK *IoStatusBlock, void *FileInformation, uint32_t Length, uint32_t FileInformationClass) {
    int slot = nt_valid_handle(FileHandle, 1);
    if (slot < 0) return STATUS_INVALID_HANDLE;
    if (!IoStatusBlock || !FileInformation || FileInformationClass != NT_FILE_STANDARD_INFORMATION) return STATUS_INVALID_PARAMETER;
    IRP irp = {0};
    irp.Stack.MajorFunction = IRP_MJ_QUERY_INFORMATION;
    irp.BackendHandle = nt_handles[slot].Backend;
    irp.SystemBuffer = FileInformation;
    irp.Length = Length;
    NTSTATUS status = IoCallDriver(&nt_disk_device, &irp);
    *IoStatusBlock = irp.IoStatus;
    return status;
}
static NTSTATUS NtClose(HANDLE Handle) {
    int slot = nt_valid_handle(Handle, 1);
    if (slot < 0) return STATUS_INVALID_HANDLE;
    IRP irp = {0};
    irp.Stack.MajorFunction = IRP_MJ_CLOSE;
    irp.BackendHandle = nt_handles[slot].Backend;
    NTSTATUS status = IoCallDriver(&nt_disk_device, &irp);
    if (NT_SUCCESS(status)) nt_handles[slot].Type = 0;
    return status;
}
static NTSTATUS nt_file_abort(HANDLE Handle) {
    int slot = nt_valid_handle(Handle, 1);
    if (slot < 0) return STATUS_INVALID_HANDLE;
    if (!cm_file_abort(nt_handles[slot].Backend)) return STATUS_IO_DEVICE_ERROR;
    nt_handles[slot].Type = 0;
    return STATUS_SUCCESS;
}
static NTSTATUS nt_open_ascii(HANDLE *result, const char *path, ACCESS_MASK access, uint32_t disposition) {
    if (!nt_driver_ready) NtInitializeIoManager();
    uint16_t buffer[96];
    UNICODE_STRING name;
    if (!RtlInitUnicodeStringFromAscii(&name, buffer, 96, path)) return STATUS_INVALID_PARAMETER;
    OBJECT_ATTRIBUTES attr = {sizeof(OBJECT_ATTRIBUTES), 0, &name, 0, NULL, NULL};
    IO_STATUS_BLOCK ios = {0};
    return NtCreateFile(result, access, &attr, &ios, NULL, 0, 0, disposition, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
}

static uint32_t nt_registry_crc(const uint8_t *p, unsigned size) {
    uint32_t crc = 0xffffffffu;
    for (unsigned i = 0; i < size; i++) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
static int nt_registry_value_slot(const char *name, int create) {
    int vacant = -1;
    for (int i = 0; i < NT_REG_MAX_VALUES; i++) {
        if (nt_registry_values[i].Active && nt_name_equal(nt_registry_values[i].Name, name)) return i;
        if (!nt_registry_values[i].Active && vacant < 0) vacant = i;
    }
    return create ? vacant : -1;
}
static void nt_registry_load(void) {
    nt_registry_loaded = 1;
    if (!storage_ready) return;
    HANDLE h = 0;
    if (!NT_SUCCESS(nt_open_ascii(&h, "\\??\\D:\\system.reg", FILE_READ_DATA, FILE_OPEN))) return;
    uint8_t data[512] = {0};
    IO_STATUS_BLOCK io;
    NTSTATUS st = NtReadFile(h, 0, NULL, NULL, &io, data, sizeof(data), NULL, NULL);
    NtClose(h);
    if (!NT_SUCCESS(st) || io.Information != sizeof(data)) return;
    if (data[0] != 'N' || data[1] != 'T' || data[2] != 'R' || data[3] != 'G' || data[4] != 1) return;
    uint32_t expected = (uint32_t)data[8] | (uint32_t)data[9] << 8 | (uint32_t)data[10] << 16 | (uint32_t)data[11] << 24;
    if (nt_registry_crc(data + 12, 500) != expected) return;
    for (int i = 0; i < NT_REG_MAX_VALUES; i++) {
        int offset = 16 + i * 32;
        nt_registry_values[i].Active = data[offset];
        nt_registry_values[i].Type = data[offset + 1];
        nt_registry_values[i].Data = (uint32_t)data[offset + 2] | (uint32_t)data[offset + 3] << 8 | (uint32_t)data[offset + 4] << 16 | (uint32_t)data[offset + 5] << 24;
        for (int j = 0; j < 20; j++) nt_registry_values[i].Name[j] = (char)data[offset + 8 + j];
        nt_registry_values[i].Name[19] = 0;
    }
    nt_registry_generation++;
}
static NTSTATUS nt_registry_flush(void) {
    if (!nt_registry_dirty) return STATUS_SUCCESS;
    if (!storage_ready) return STATUS_IO_DEVICE_ERROR;
    uint8_t data[512] = {0};
    data[0] = 'N'; data[1] = 'T'; data[2] = 'R'; data[3] = 'G'; data[4] = 1;
    for (int i = 0; i < NT_REG_MAX_VALUES; i++) {
        int offset = 16 + i * 32;
        data[offset] = nt_registry_values[i].Active;
        data[offset + 1] = (uint8_t)nt_registry_values[i].Type;
        uint32_t number = nt_registry_values[i].Data;
        data[offset + 2] = (uint8_t)number;
        data[offset + 3] = (uint8_t)(number >> 8);
        data[offset + 4] = (uint8_t)(number >> 16);
        data[offset + 5] = (uint8_t)(number >> 24);
        for (int j = 0; j < 20; j++) data[offset + 8 + j] = (uint8_t)nt_registry_values[i].Name[j];
    }
    uint32_t crc = nt_registry_crc(data + 12, 500);
    data[8] = (uint8_t)crc; data[9] = (uint8_t)(crc >> 8); data[10] = (uint8_t)(crc >> 16); data[11] = (uint8_t)(crc >> 24);
    HANDLE h = 0;
    NTSTATUS status = nt_open_ascii(&h, "\\??\\D:\\system.reg", FILE_WRITE_DATA, FILE_OVERWRITE_IF);
    if (!NT_SUCCESS(status)) return status;
    IO_STATUS_BLOCK io = {0};
    status = NtWriteFile(h, 0, NULL, NULL, &io, data, sizeof(data), NULL, NULL);
    if (NT_SUCCESS(status)) status = NtFlushBuffersFile(h, &io);
    if (NT_SUCCESS(status)) status = NtClose(h);
    else nt_file_abort(h);
    if (NT_SUCCESS(status)) { nt_registry_dirty = 0; nt_registry_generation++; }
    return status;
}
static NTSTATUS NtCreateKey(HANDLE *KeyHandle, ACCESS_MASK DesiredAccess, const OBJECT_ATTRIBUTES *ObjectAttributes, uint32_t TitleIndex,
                           const UNICODE_STRING *Class, uint32_t CreateOptions, uint32_t *Disposition) {
    (void)DesiredAccess; (void)TitleIndex; (void)Class; (void)CreateOptions;
    char path[96];
    if (!KeyHandle || !ObjectAttributes || !nt_unicode_ascii(ObjectAttributes->ObjectName, path, sizeof(path))) return STATUS_INVALID_PARAMETER;
    if (!nt_name_equal(path, "\\Registry\\Machine\\Software\\Cinmix") &&
        !nt_name_equal(path, "\\Registry\\Machine\\Software\\Quartzie")) return STATUS_OBJECT_PATH_NOT_FOUND;
    HANDLE h = nt_alloc_handle(2, 0);
    if (!h) return STATUS_INSUFFICIENT_RESOURCES;
    *KeyHandle = h;
    if (Disposition) *Disposition = 1;
    return STATUS_SUCCESS;
}
static NTSTATUS NtOpenKey(HANDLE *KeyHandle, ACCESS_MASK DesiredAccess, const OBJECT_ATTRIBUTES *ObjectAttributes) {
    return NtCreateKey(KeyHandle, DesiredAccess, ObjectAttributes, 0, NULL, 0, NULL);
}
static NTSTATUS NtSetValueKey(HANDLE KeyHandle, const UNICODE_STRING *ValueName, uint32_t TitleIndex, uint32_t Type, const void *Data, uint32_t DataSize) {
    (void)TitleIndex;
    if (nt_valid_handle(KeyHandle, 2) < 0) return STATUS_INVALID_HANDLE;
    char name[20];
    if (Type != REG_DWORD || DataSize != sizeof(uint32_t) || !Data || !nt_unicode_ascii(ValueName, name, sizeof(name))) return STATUS_INVALID_PARAMETER;
    int slot = nt_registry_value_slot(name, 1);
    if (slot < 0) return STATUS_INSUFFICIENT_RESOURCES;
    nt_registry_values[slot].Active = 1;
    nt_registry_values[slot].Type = Type;
    nt_ascii_copy(nt_registry_values[slot].Name, name, sizeof(nt_registry_values[slot].Name));
    nt_registry_values[slot].Data = *(const uint32_t *)Data;
    nt_registry_dirty = 1;
    return nt_registry_flush();
}
static NTSTATUS NtQueryValueKey(HANDLE KeyHandle, const UNICODE_STRING *ValueName, uint32_t KeyValueInformationClass,
                                void *KeyValueInformation, uint32_t Length, uint32_t *ResultLength) {
    if (nt_valid_handle(KeyHandle, 2) < 0) return STATUS_INVALID_HANDLE;
    if (KeyValueInformationClass != 2 || !KeyValueInformation) return STATUS_INVALID_PARAMETER;
    char name[20];
    if (!nt_unicode_ascii(ValueName, name, sizeof(name))) return STATUS_INVALID_PARAMETER;
    int slot = nt_registry_value_slot(name, 0);
    if (slot < 0) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (ResultLength) *ResultLength = 16;
    if (Length < 16) return STATUS_BUFFER_TOO_SMALL;
    uint8_t *out = (uint8_t *)KeyValueInformation;
    for (int i = 0; i < 16; i++) out[i] = 0;
    out[4] = (uint8_t)REG_DWORD;
    out[8] = 4;
    uint32_t value = nt_registry_values[slot].Data;
    out[12] = (uint8_t)value; out[13] = (uint8_t)(value >> 8); out[14] = (uint8_t)(value >> 16); out[15] = (uint8_t)(value >> 24);
    return STATUS_SUCCESS;
}
static NTSTATUS NtCloseObject(HANDLE Handle) {
    int key = nt_valid_handle(Handle, 2);
    if (key >= 0) { nt_handles[key].Type = 0; return STATUS_SUCCESS; }
    return NtClose(Handle);
}
static int nt_open_handle_count(void) {
    int n = 0;
    for (int i = 0; i < NT_HANDLE_SLOTS; i++) if (nt_handles[i].Type) n++;
    return n;
}
static void nt_save_theme(uint32_t color) {
    if (!storage_ready) return;
    HANDLE h = 0;
    uint16_t keybuf[60], namebuf[20];
    UNICODE_STRING key, name;
    RtlInitUnicodeStringFromAscii(&key, keybuf, 60, "\\Registry\\Machine\\Software\\Cinmix");
    RtlInitUnicodeStringFromAscii(&name, namebuf, 20, "Theme");
    OBJECT_ATTRIBUTES attr = {sizeof(OBJECT_ATTRIBUTES), 0, &key, 0, NULL, NULL};
    if (NT_SUCCESS(NtOpenKey(&h, 3, &attr))) { NtSetValueKey(h, &name, 0, REG_DWORD, &color, 4); NtCloseObject(h); }
}
static int nt_restore_theme(int *value) {
    if (!storage_ready || !value) return 0;
    HANDLE h = 0;
    uint16_t keybuf[60], namebuf[20];
    UNICODE_STRING key, name;
    RtlInitUnicodeStringFromAscii(&key, keybuf, 60, "\\Registry\\Machine\\Software\\Cinmix");
    RtlInitUnicodeStringFromAscii(&name, namebuf, 20, "Theme");
    OBJECT_ATTRIBUTES attr = {sizeof(OBJECT_ATTRIBUTES), 0, &key, 0, NULL, NULL};
    if (!NT_SUCCESS(NtOpenKey(&h, 1, &attr))) return 0;
    uint8_t data[16];
    NTSTATUS status = NtQueryValueKey(h, &name, 2, data, sizeof(data), NULL);
    NtCloseObject(h);
    if (!NT_SUCCESS(status)) return 0;
    uint32_t color = (uint32_t)data[12] | (uint32_t)data[13] << 8 | (uint32_t)data[14] << 16 | (uint32_t)data[15] << 24;
    if (color > 3) return 0;
    *value = (int)color;
    return 1;
}
