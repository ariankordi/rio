#ifndef RIO_FILE_DEVICE_H
#define RIO_FILE_DEVICE_H

#include <container/rio_TList.h>
#include <misc/rio_MemUtil.h>

#include <string>

namespace rio {

class FileHandle;

class FileDevice : public TListNode<FileDevice*>
{
public:
    enum FileOpenFlag
    {
        FILE_OPEN_FLAG_READ,        // r
        FILE_OPEN_FLAG_WRITE,       // w
        FILE_OPEN_FLAG_READ_WRITE,  // r+
        FILE_OPEN_FLAG_CREATE       // w+
    };

    enum SeekOrigin
    {
        SEEK_ORIGIN_BEGIN,
        SEEK_ORIGIN_CURRENT,
        SEEK_ORIGIN_END
    };

public:
    static const u32 cBufferMinAlignment = 64;

public:
    struct LoadArg
    {
        LoadArg()
            : path("")
            , buffer(nullptr)
            , buffer_size(0)
            , alignment(cBufferMinAlignment)
            , read_size(0)
            , roundup_size(0)
            , need_unload(false)
        {
        }

        std::string path;
        u8*         buffer;
        u32         buffer_size;
        u32         alignment;
        u32         read_size;
        u32         roundup_size;
        bool        need_unload;
    };

public:
    FileDevice(const std::string& drive_name)
        : TListNode<FileDevice*>(this)
        , mDriveName(drive_name)
    {
    }

    virtual ~FileDevice();

    const std::string& getDriveName() const
    {
        return mDriveName;
    }

    void setDriveName(const std::string& drive_name)
    {
        mDriveName = drive_name;
    }

    u8* load(LoadArg& arg)
    {
        u8* ret = tryLoad(arg);
        RIO_ASSERT(ret);
        return ret;
    }

    static void unload(u8* data)
    {
        RIO_ASSERT(data);
        MemUtil::free(data);
    }

    FileDevice* open(FileHandle* handle, const std::string& filename, FileOpenFlag flag)
    {
        FileDevice* device = tryOpen(handle, filename, flag);
        RIO_ASSERT(device);
        return device;
    }

    bool close(FileHandle* handle)
    {
        bool success = tryClose(handle);
        RIO_ASSERT(success);
        return success;
    }

    u32 read(FileHandle* handle, u8* buf, u32 size)
    {
        u32 read_size = 0;
        [[maybe_unused]] bool success = tryRead(&read_size, handle, buf, size);
        RIO_ASSERT(success);
        return read_size;
    }

    u32 write(FileHandle* handle, const u8* buf, u32 size)
    {
        u32 write_size = 0;
        [[maybe_unused]] bool success = tryWrite(&write_size, handle, buf, size);
        RIO_ASSERT(success);
        return write_size;
    }

    bool seek(FileHandle* handle, s32 offset, SeekOrigin origin)
    {
        bool success = trySeek(handle, offset, origin);
        RIO_ASSERT(success);
        return success;
    }

    u32 getCurrentSeekPos(FileHandle* handle)
    {
        u32 pos = 0;
        [[maybe_unused]] bool success = tryGetCurrentSeekPos(&pos, handle);
        RIO_ASSERT(success);
        return pos;
    }

    u32 getFileSize(const std::string& path)
    {
        u32 size = 0;
        [[maybe_unused]] bool success = tryGetFileSize(&size, path);
        RIO_ASSERT(success);
        return size;
    }

    u32 getFileSize(FileHandle* handle)
    {
        u32 size = 0;
        [[maybe_unused]] bool success = tryGetFileSize(&size, handle);
        RIO_ASSERT(success);
        return size;
    }

    u8* tryLoad(LoadArg& arg);
    FileDevice* tryOpen(FileHandle* handle, const std::string& filename, FileOpenFlag flag);
    bool tryClose(FileHandle* handle);
    bool tryRead(u32* read_size, FileHandle* handle, u8* buf, u32 size);
    bool tryWrite(u32* write_size, FileHandle* handle, const u8* buf, u32 size);
    bool trySeek(FileHandle* handle, s32 offset, SeekOrigin origin);
    bool tryGetCurrentSeekPos(u32* pos, FileHandle* handle);
    bool tryGetFileSize(u32* size, const std::string& path);
    bool tryGetFileSize(u32* size, FileHandle* handle);

protected:
    virtual u8* doLoad_(LoadArg& arg);
    virtual FileDevice* doOpen_(FileHandle* handle, const std::string& filename, FileOpenFlag flag) = 0;
    virtual bool doClose_(FileHandle* handle) = 0;
    virtual bool doRead_(u32* read_size, FileHandle* handle, u8* buf, u32 size) = 0;
    virtual bool doWrite_(u32* write_size, FileHandle* handle, const u8* buf, u32 size) = 0;
    virtual bool doSeek_(FileHandle* handle, s32 offset, SeekOrigin origin) = 0;
    virtual bool doGetCurrentSeekPos_(u32* pos, FileHandle* handle) = 0;
    virtual bool doGetFileSize_(u32* size, const std::string& path) = 0;
    virtual bool doGetFileSize_(u32* size, FileHandle* handle) = 0;

protected:
    struct FileHandleInner
    {
        u32         position;
        uintptr_t   handle;
    };

    static FileHandleInner* getFileHandleInner_(FileHandle* handle);

protected:
    std::string mDriveName;

    friend class FileHandle;
    friend class FileDeviceMgr;
};

class FileHandle
{
public:
    FileHandle()
        : mDevice(nullptr)
        , mOriginalDevice(nullptr)
    {
    }

private:
    FileHandle(const FileHandle&);
    const FileHandle& operator=(const FileHandle&);

public:
    virtual ~FileHandle()
    {
        FileDevice* device = mOriginalDevice;
        if (device)
            device->tryClose(this);
    }

    FileDevice* getDevice() const { return mDevice; }
    FileDevice* getOriginalDevice() const { return mOriginalDevice; }
    bool isOpen() const { return mDevice != nullptr; }

    bool close();
    u32 read(u8* buf, u32 size);
    u32 write(const u8* buf, u32 size);
    bool seek(s32 offset, FileDevice::SeekOrigin origin);
    u32 getCurrentSeekPos();
    u32 getFileSize();

    bool tryClose();
    bool tryRead(u32* read_size, u8* buf, u32 size);
    bool tryWrite(u32* write_size, const u8* buf, u32 size);
    bool trySeek(s32 offset, FileDevice::SeekOrigin origin);
    bool tryGetCurrentSeekPos(u32* pos);
    bool tryGetFileSize(u32* size);

private:
    FileDevice*                 mDevice;
    FileDevice*                 mOriginalDevice;
    FileDevice::FileHandleInner mHandleInner;

    friend class FileDevice;
};

inline FileDevice::FileHandleInner*
FileDevice::getFileHandleInner_(FileHandle* handle)
{
    return &handle->mHandleInner;
}

}

#endif // RIO_FILE_DEVICE_H