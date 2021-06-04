#include "../Core/Context.h"
#include "../IO/Log.h"

#if !defined(__EMSCRIPTEN__) && !defined(MINI_URHO)

// Logging
void _OutputDebugStringV(const char* str, va_list args)
{
#if defined(FORGE_DEBUG)
    auto logger = Urho3D::Log::GetLogger("The-Forge");
    ea::string msg;
    msg.append_sprintf_va_list(str, args);
    URHO3D_LOGINFO(msg.c_str());
#endif
}

void _OutputDebugString(const char* str, ...)
{
#if defined(FORGE_DEBUG)
    va_list arglist;
	va_start(arglist, str);
	_OutputDebugStringV(str, arglist);
	va_end(arglist);
#endif
}

void _FailedAssert(const char* file, int line, const char* statement)
{
    auto logger = Urho3D::Log::GetLogger("The-Forge");
    ea::string msg;
    msg.append_sprintf("Assertion failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
    logger.Error(msg);
}

void _PrintUnicode(const char* str, bool error)
{
    auto logger = Urho3D::Log::GetLogger("The-Forge");
    if (error)
        logger.Error(str);
    else
        logger.Info(str);
}

// File IO
#include <TheForge/OS/Interfaces/IFileSystem.h>
#include <TheForge/OS/Interfaces/ILog.h>
#include "../IO/File.h"
#include "../IO/FileSystem.h"

static bool FileStreamOpen(IFileSystem* fileSystem, const ResourceDirectory, const char* fileName, FileMode mode, FileStream* pOut)
{
    auto* context = Urho3D::Context::GetInstance();
    auto file = context->CreateObject<Urho3D::File>();

    Urho3D::FileMode urhoMode;
    if (mode & FM_READ)
        urhoMode = Urho3D::FILE_READ;
    else if (mode & FM_WRITE)
        urhoMode = Urho3D::FILE_WRITE;
    else if (mode & FM_READ_WRITE)
        urhoMode = Urho3D::FILE_READWRITE;
    else
    {
        LOGF(LogLevel::eERROR, "Unknown FileMode for file %s", fileName);
        return false;
    }

    if (!file->Open(fileName, urhoMode))
    {
        LOGF(LogLevel::eERROR, "Opening file %s failed", fileName);
        return false;
    }

    if (mode & FM_APPEND)
        file->Seek(file->GetSize());

    *pOut = {};
    pOut->mMode = mode;
    pOut->mSize = file->GetSize();
    pOut->pIO = fileSystem;
    pOut->pFile = reinterpret_cast<FILE*>(file.Detach());
    return true;
}

static bool FileStreamClose(FileStream* pFile)
{
    if (Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile))
    {
        file->Close();
        delete file;
        return true;
    }
    else
    {
        LOGF(LogLevel::eERROR, "Error closing system FileStream");
        return false;
    }
}

static size_t FileStreamRead(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes)
{
    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    return file->Read(outputBuffer, bufferSizeInBytes);
}

static size_t FileStreamWrite(FileStream* pFile, const void* sourceBuffer, size_t byteCount)
{
    if ((pFile->mMode & (FM_WRITE | FM_APPEND)) == 0)
    {
        LOGF(LogLevel::eWARNING, "Writing to FileStream with mode %u", pFile->mMode);
        return 0;
    }

    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    size_t bytesWritten = file->Write(sourceBuffer, byteCount);
    if (bytesWritten != byteCount)
        LOGF(LogLevel::eWARNING, "Error writing to system FileStream: %s", strerror(errno));

    return bytesWritten;
}

static bool FileStreamSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    if ((pFile->mMode & FM_BINARY) == 0 && baseOffset != SBO_START_OF_FILE)
    {
        LOGF(LogLevel::eWARNING, "Text-mode FileStreams only support SBO_START_OF_FILE");
        return false;
    }

    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    switch (baseOffset)
    {
    case SBO_CURRENT_POSITION:
        seekOffset += file->GetPosition();
        break;
    case SBO_END_OF_FILE:
        seekOffset = file->GetSize() - seekOffset;
        break;
    default:
        break;
    }
    return file->Seek(seekOffset);
}

static ssize_t FileStreamGetSeekPosition(const FileStream* pFile)
{
    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    long int result = file->Tell();
    if (result == -1L)
        LOGF(LogLevel::eWARNING, "Error getting seek position in FileStream");
    return result;
}

static ssize_t FileStreamGetSize(const FileStream* pFile)
{
    return pFile->mSize;
}

static bool FileStreamFlush(FileStream* pFile)
{
    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    file->Flush();
    return true;
}

static bool FileStreamIsAtEnd(const FileStream* pFile)
{
    Urho3D::File* file = reinterpret_cast<Urho3D::File*>(pFile->pFile);
    return file->IsEof();
}

static IFileSystem gSystemFileIO =
    {
        FileStreamOpen,
        FileStreamClose,
        FileStreamRead,
        FileStreamWrite,
        FileStreamSeek,
        FileStreamGetSeekPosition,
        FileStreamGetSize,
        FileStreamFlush,
        FileStreamIsAtEnd
    };

IFileSystem* pSystemFileIO = &gSystemFileIO;

bool fsCreateDirectory(ResourceDirectory)
{
    // We don't really have a concept of resource directories.
    return true;
}

time_t fsGetLastModifiedTime(ResourceDirectory, const char* fileName)
{
    auto* context = Urho3D::Context::GetInstance();
    auto* fs = context->GetSubsystem<Urho3D::FileSystem>();
    return (time_t)fs->GetLastModifiedTime(fileName);
}

bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
    // Never used.
    return false;
}

int systemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile)
{
    auto* context = Urho3D::Context::GetInstance();
    auto* fs = context->GetSubsystem<Urho3D::FileSystem>();
    ea::string stdOut;
    ea::vector<ea::string> args;
    for (int i = 0; i < argumentCount; i++)
        args.push_back(arguments[i]);
    int result = fs->SystemRun(command, args, stdOut);
    if (stdOutFile)
    {
        Urho3D::File file(context);
        if (file.Open(stdOutFile, Urho3D::FILE_WRITE))
            file.Write(stdOut.c_str(), stdOut.size());
    }
    return result;
}

#endif  // __EMSCRIPTEN__
