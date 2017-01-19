#include "Common.h"

#ifdef FO_WINDOWS
# include <io.h>
#else
# include <dirent.h>
# include <sys/stat.h>
#endif

static void PreparePath( const char* fname, char* result )
{
    result[ 0 ] = 0;
    if( fname[ 0 ] != '.' && fname[ 0 ] != '/' )
        Str::Copy( result, MAX_FOPATH, "./" );
    Str::Append( result, MAX_FOPATH, fname );
    NormalizePathSlashes( result );
}

#ifdef FO_WINDOWS
static uint64 FileTimeToUInt64( FILETIME ft )
{
    union
    {
        FILETIME       ft;
        ULARGE_INTEGER ul;
    } t;
    t.ft = ft;
    return PACKUINT64( t.ul.HighPart, t.ul.LowPart );
}

static wchar_t* MBtoWC( const char* mb, wchar_t* buf )
{
    char mb_[ MAX_FOPATH ];
    Str::Copy( mb_, mb );
    for( char* s = mb_; *s; s++ )
        if( *s == '/' )
            *s = '\\';
    if( MultiByteToWideChar( CP_UTF8, 0, mb_, -1, buf, MAX_FOPATH - 1 ) == 0 )
        buf[ 0 ] = 0;
    return buf;
}

static char* WCtoMB( const wchar_t* wc, char* buf )
{
    if( WideCharToMultiByte( CP_UTF8, 0, wc, -1, buf, MAX_FOPATH - 1, nullptr, nullptr ) == 0 )
        buf[ 0 ] = 0;
    NormalizePathSlashes( buf );
    return buf;
}
#endif

#ifdef FONLINE_CLIENT
struct FileDesc
{
    SDL_RWops* Ops;
    bool       WriteThrough;
};

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    SDL_RWops* ops = SDL_RWFromFile( fname_, write ? "wb" : "rb" );
    if( !ops && write )
    {
        MakeDirectoryTree( fname_ );
        ops = SDL_RWFromFile( fname_, "wb" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    SDL_RWops* ops = SDL_RWFromFile( fname_, "ab" );
    if( !ops )
    {
        MakeDirectoryTree( fname_ );
        ops = SDL_RWFromFile( fname_, "ab" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void* FileOpenForReadWrite( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    SDL_RWops* ops = SDL_RWFromFile( fname_, "r+b" );
    if( !ops )
    {
        MakeDirectoryTree( fname_ );
        ops = SDL_RWFromFile( fname_, "r+b" );
    }
    if( !ops )
        return nullptr;

    FileDesc* fd = new FileDesc();
    fd->Ops = ops;
    fd->WriteThrough = write_through;
    return (void*) fd;
}

void FileClose( void* file )
{
    if( file )
    {
        SDL_RWclose( ( (FileDesc*) file )->Ops );
        delete (FileDesc*) file;
    }
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    uint rb_ = (uint) SDL_RWread( ( (FileDesc*) file )->Ops, buf, sizeof( char ), len );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    SDL_RWops* ops = ( (FileDesc*) file )->Ops;
    bool       result = ( (uint) SDL_RWwrite( ops, buf, sizeof( char ), len ) == len );
    if( result && ( (FileDesc*) file )->WriteThrough )
    {
        if( ops->type == SDL_RWOPS_WINFILE )
        {
            # ifdef __WIN32__
            FlushFileBuffers( (HANDLE) ops->hidden.windowsio.h );
            # endif
        }
        else if( ops->type == SDL_RWOPS_STDFILE )
        {
            # ifdef HAVE_STDIO_H
            fflush( ops->hidden.stdio.fp );
            # endif
        }
    }
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return SDL_RWseek( ( ( (FileDesc*) file )->Ops ), offset, origin ) != -1;
}

uint FileGetPointer( void* file )
{
    return (uint) SDL_RWtell( ( (FileDesc*) file )->Ops );
}

uint64 FileGetWriteTime( void* file )
{
    SDL_RWops* ops = ( (FileDesc*) file )->Ops;
    if( ops->type == SDL_RWOPS_WINFILE )
    {
        # ifdef __WIN32__
        FILETIME tc, ta, tw;
        GetFileTime( (HANDLE) ops->hidden.windowsio.h, &tc, &ta, &tw );
        return FileTimeToUInt64( tw );
        # endif
    }
    else if( ops->type == SDL_RWOPS_STDFILE )
    {
        # ifdef HAVE_STDIO_H
        int         fd = fileno( ops->hidden.stdio.fp );
        struct stat st;
        fstat( fd, &st );
        return (uint64) st.st_mtime;
        # endif
    }
    return (uint64) 1;
}

uint FileGetSize( void* file )
{
    Sint64 size = SDL_RWsize( ( (FileDesc*) file )->Ops );
    return (uint) ( size <= 0 ? 0 : size );
}

#else
# ifdef FO_WINDOWS

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file;
    if( write )
    {
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
        if( file == INVALID_HANDLE_VALUE )
        {
            MakeDirectoryTree( fname );
            file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
        }
    }
    else
    {
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
    return file;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    if( file == INVALID_HANDLE_VALUE )
    {
        MakeDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
    if( !FileSetPointer( file, 0, SEEK_END ) )
    {
        FileClose( file );
        return nullptr;
    }
    return file;
}

void* FileOpenForReadWrite( const char* fname, bool write_through /* = false */ )
{
    wchar_t wc[ MAX_FOPATH ];
    HANDLE  file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    if( file == INVALID_HANDLE_VALUE )
    {
        MakeDirectoryTree( fname );
        file = CreateFileW( MBtoWC( fname, wc ), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, write_through ? FILE_FLAG_WRITE_THROUGH : 0, nullptr );
    }
    if( file == INVALID_HANDLE_VALUE )
        return nullptr;
    return file;
}

void FileClose( void* file )
{
    if( file )
        CloseHandle( (HANDLE) file );
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    DWORD dw = 0;
    BOOL  result = ReadFile( (HANDLE) file, buf, len, &dw, nullptr );
    if( rb )
        *rb = dw;
    return result && dw == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    DWORD dw = 0;
    return WriteFile( (HANDLE) file, buf, len, &dw, nullptr ) && dw == len;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return SetFilePointer( (HANDLE) file, offset, nullptr, origin ) != INVALID_SET_FILE_POINTER;
}

uint FileGetPointer( void* file )
{
    return (uint) SetFilePointer( (HANDLE) file, 0, nullptr, FILE_CURRENT );
}

uint64 FileGetWriteTime( void* file )
{
    FILETIME tc, ta, tw;
    GetFileTime( (HANDLE) file, &tc, &ta, &tw );
    return FileTimeToUInt64( tw );
}

uint FileGetSize( void* file )
{
    DWORD high;
    return GetFileSize( (HANDLE) file, &high );
}

# else

struct FileDesc
{
    FILE* f;
    bool  writeThrough;
};

void* FileOpen( const char* fname, bool write, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    FILE* f = fopen( fname_, write ? "wb" : "rb" );
    if( !f && write )
    {
        MakeDirectoryTree( fname_ );
        f = fopen( fname_, "wb" );
    }
    if( !f )
        return nullptr;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void* FileOpenForAppend( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    FILE* f = fopen( fname_, "ab" );
    if( !f )
    {
        MakeDirectoryTree( fname_ );
        f = fopen( fname_, "ab" );
    }
    if( !f )
        return nullptr;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void* FileOpenForReadWrite( const char* fname, bool write_through /* = false */ )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    FILE* f = fopen( fname_, "r+b" );
    if( !f )
    {
        MakeDirectoryTree( fname_ );
        f = fopen( fname_, "r+b" );
    }
    if( !f )
        return nullptr;
    FileDesc* fd = new FileDesc();
    fd->f = f;
    fd->writeThrough = write_through;
    return (void*) fd;
}

void FileClose( void* file )
{
    if( file )
    {
        fclose( ( (FileDesc*) file )->f );
        delete (FileDesc*) file;
    }
}

bool FileRead( void* file, void* buf, uint len, uint* rb /* = NULL */ )
{
    uint rb_ = (uint) fread( buf, sizeof( char ), len, ( (FileDesc*) file )->f );
    if( rb )
        *rb = rb_;
    return rb_ == len;
}

bool FileWrite( void* file, const void* buf, uint len )
{
    bool result = ( (uint) fwrite( buf, sizeof( char ), len, ( (FileDesc*) file )->f ) == len );
    if( result && ( (FileDesc*) file )->writeThrough )
        fflush( ( (FileDesc*) file )->f );
    return result;
}

bool FileSetPointer( void* file, int offset, int origin )
{
    return fseek( ( (FileDesc*) file )->f, offset, origin ) == 0;
}

uint FileGetPointer( void* file )
{
    return (uint) ftell( ( (FileDesc*) file )->f );
}

uint64 FileGetWriteTime( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->f );
    struct stat st;
    fstat( fd, &st );
    return (uint64) st.st_mtime;
}

uint FileGetSize( void* file )
{
    int         fd = fileno( ( (FileDesc*) file )->f );
    struct stat st;
    fstat( fd, &st );
    return (uint) st.st_size;
}
# endif
#endif

#ifdef FO_WINDOWS
bool FileDelete( const char* fname )
{
    wchar_t wc[ MAX_FOPATH ];
    return DeleteFileW( MBtoWC( fname, wc ) ) != FALSE;
}

bool FileExist( const char* fname )
{
    wchar_t wc[ MAX_FOPATH ];
    return !_waccess( MBtoWC( fname, wc ), 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    wchar_t wc1[ MAX_FOPATH ];
    wchar_t wc2[ MAX_FOPATH ];
    return MoveFileW( MBtoWC( fname, wc1 ), MBtoWC( new_fname, wc2 ) ) != FALSE;
}

void* FileFindFirst( const char* path, const char* extension, FindData& fd )
{
    char query[ MAX_FOPATH ];
    if( extension )
        Str::Format( query, "%s*.%s", path, extension );
    else
        Str::Format( query, "%s*", path );

    WIN32_FIND_DATAW wfd;
    wchar_t          wc[ MAX_FOPATH ];
    HANDLE           h = FindFirstFileW( MBtoWC( query, wc ), &wfd );
    if( h == INVALID_HANDLE_VALUE )
        return nullptr;

    char mb[ MAX_FOPATH ];
    fd.FileName = WCtoMB( wfd.cFileName, mb );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    fd.FileSize = wfd.nFileSizeLow;
    fd.WriteTime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( fd.IsDirectory && ( fd.FileName == "." || fd.FileName == ".." ) )
        if( !FileFindNext( h, fd ) )
            return false;

    return h;
}

bool FileFindNext( void* descriptor, FindData& fd )
{
    WIN32_FIND_DATAW wfd;
    if( !FindNextFileW( (HANDLE) descriptor, &wfd ) )
        return false;

    char mb[ MAX_FOPATH ];
    fd.FileName = WCtoMB( wfd.cFileName, mb );
    fd.IsDirectory = ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
    fd.FileSize = wfd.nFileSizeLow;
    fd.WriteTime = FileTimeToUInt64( wfd.ftLastWriteTime );
    if( fd.IsDirectory && ( fd.FileName == "." || fd.FileName == ".." ) )
        return FileFindNext( (HANDLE) descriptor, fd );

    return true;
}

void FileFindClose( void* descriptor )
{
    if( descriptor )
        FindClose( (HANDLE) descriptor );
}

#else

bool FileDelete( const char* fname )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    return remove( fname_ );
}

bool FileExist( const char* fname )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );

    return !access( fname_, 0 );
}

bool FileRename( const char* fname, const char* new_fname )
{
    char fname_[ MAX_FOPATH ];
    PreparePath( fname, fname_ );
    char new_fname_[ MAX_FOPATH ];
    PreparePath( new_fname, new_fname_ );

    return !rename( fname_, new_fname_ );
}

struct FileFind
{
    DIR* d;
    char path[ MAX_FOPATH ];
    char ext[ 32 ];
};

void* FileFindFirst( const char* path, const char* extension, FindData& fd )
{
    char path_[ MAX_FOPATH ];
    PreparePath( path, path_ );

    // Open dir
    DIR* h = opendir( path_ );
    if( !h )
        return nullptr;

    // Create own descriptor
    FileFind* ff = new FileFind;
    memzero( ff, sizeof( FileFind ) );
    ff->d = h;
    Str::Copy( ff->path, path_ );
    if( ff->path[ Str::Length( ff->path ) - 1 ] != '/' )
        Str::Append( ff->path, "/" );
    if( extension )
        Str::Copy( ff->ext, extension );

    // Find first entire
    if( !FileFindNext( ff, fd ) )
    {
        FileFindClose( ff );
        return nullptr;
    }
    return ff;
}

bool FileFindNext( void* descriptor, FindData& fd )
{
    // Cast descriptor
    FileFind* ff = (FileFind*) descriptor;

    // Read entire
    struct dirent* ent = readdir( ff->d );
    if( !ent )
        return false;

    // Skip '.' and '..'
    if( Str::Compare( ent->d_name, "." ) || Str::Compare( ent->d_name, ".." ) )
        return FileFindNext( descriptor, fd );

    // Read entire information
    bool        valid = false;
    bool        dir = false;
    char        fname[ MAX_FOPATH ];
    uint        file_size;
    uint64      write_time;
    Str::Format( fname, "%s%s", ff->path, ent->d_name );
    struct stat st;
    if( !stat( fname, &st ) )
    {
        dir = S_ISDIR( st.st_mode );
        if( dir || S_ISREG( st.st_mode ) )
        {
            valid = true;
            file_size = (uint) st.st_size;
            write_time = (uint64) st.st_mtime;
        }
    }

    // Skip not dirs and regular files
    if( !valid )
        return FileFindNext( descriptor, fd );

    // Find by extensions
    if( ff->ext[ 0 ] )
    {
        // Skip dirs
        if( dir )
            return FileFindNext( descriptor, fd );

        // Compare extension
        const char* ext = nullptr;
        for( const char* name = ent->d_name; *name; name++ )
            if( *name == '.' )
                ext = name;
        if( !ext || !*( ++ext ) || !Str::CompareCase( ext, ff->ext ) )
            return FileFindNext( descriptor, fd );
    }

    // Fill find data
    fd.FileName = ent->d_name;
    fd.IsDirectory = dir;
    fd.FileSize = file_size;
    fd.WriteTime = write_time;
    return true;
}

void FileFindClose( void* descriptor )
{
    if( descriptor )
    {
        FileFind* ff = (FileFind*) descriptor;
        closedir( ff->d );
        delete ff;
    }
}
#endif

char* NormalizePathSlashes( char* path )
{
    for( char* s = path; *s; s++ )
        if( *s == '\\' )
            *s = '/';
    return path;
}

bool ResolvePath( char* path )
{
    #ifdef FO_WINDOWS
    wchar_t path_[ MAX_FOPATH ];
    wchar_t wc[ MAX_FOPATH ];
    if( !GetFullPathNameW( MBtoWC( path, wc ), MAX_FOPATH, path_, nullptr ) )
        return false;
    char mb[ MAX_FOPATH ];
    Str::Copy( path, MAX_FOPATH, WCtoMB( path_, mb ) );
    for( char* s = path; *s; s++ )
        if( *s == '/' )
            *s = '\\';
    return true;

    #else
    char path_[ MAX_FOPATH ];
    PreparePath( path, path_ );
    char path__[ MAX_FOPATH ];
    realpath( path_, path__ );
    Str::Copy( path, MAX_FOPATH, path__ );
    return true;
    #endif
}

bool MakeDirectory( const char* path )
{
    #ifdef FO_WINDOWS
    wchar_t wc[ MAX_FOPATH ];
    return CreateDirectoryW( MBtoWC( path, wc ), nullptr ) != FALSE;
    #else
    char path_[ MAX_FOPATH ];
    PreparePath( path, path_ );
    return mkdir( path_, 0x0777 ) == 0;
    #endif
}

uint MakeDirectoryTree( const char* path )
{
    uint  result = 0;
    char* work = Str::Duplicate( path );
    NormalizePathSlashes( work );
    for( char* ptr = work; *ptr; ++ptr )
    {
        if( *ptr == '/' )
        {
            *ptr = 0;
            if( MakeDirectory( work ) )
                result++;
            *ptr = '/';
        }
    }
    delete[] work;
    return result;
}
