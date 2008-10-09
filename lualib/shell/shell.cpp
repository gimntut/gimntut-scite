//build@ gcc -shared -o shell.dll -I shell.cpp scite.la -lstdc++

#include <windows.h>
#include <shlwapi.h>

extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
}

#pragma warning(push)
#pragma warning(disable: 4710)

template < class T, int defSize >
class CMemBuffer
{
public:
	CMemBuffer()
	 : m_iSize( defSize )
	 , m_pData( NULL )
	{
		SetLength( defSize );
	}

	~CMemBuffer()
	{
		SetLength( 0 );
	}

	BOOL IsBufferEmpty()
	{
		return m_pData == NULL;
	}

	T* GetBuffer()
	{
		return m_pData;
	}

	T& operator [] ( int nItem )
	{
		return m_pData[ nItem ];
	}

	int GetBufferLength()
	{
		return m_iSize;
	}

	// ���������� ������ ������ �����
	// 0 - ������� �����
	BOOL SetLength( int lenNew )
	{
		if ( lenNew > 0 )
		{
			T* sNew = (T*)malloc( lenNew * sizeof(T) );
//			T* sNew = (T*)::VirtualAlloc( NULL, lenNew * sizeof(T), MEM_COMMIT, PAGE_READWRITE );
			if ( sNew != NULL )
			{
				if ( !IsBufferEmpty() )
				{
					memcpy( sNew,
							m_pData,
							lenNew > m_iSize ? m_iSize * sizeof(T) : lenNew * sizeof(T) );
//					::VirtualFree( m_pData, 0, MEM_RELEASE );
					free( m_pData );
					m_pData = NULL;
				}
				m_pData = sNew;
				m_iSize = lenNew;
			}
			else
			{
				return FALSE;
			}
		}
		else
		{
			if ( !IsBufferEmpty() )
			{
//				::VirtualFree( m_pData, 0, MEM_RELEASE );
				free( m_pData );
				m_pData = NULL;
			}
			m_iSize = 0;
		}
		return TRUE;
	}

private:
	T* m_pData;
	int m_iSize;
};

class CSimpleString
{
public:
	CSimpleString()
	 : m_iLen( 0 )
	{
	}

	const char* GetString()
	{
		return ( m_iLen == 0 || m_sData.IsBufferEmpty() ) ? "" : m_sData.GetBuffer();
	}

	char& operator [] ( int nItem )
	{
		return m_sData[ nItem ];
	}

	int GetLenght()
	{
		return m_iLen;
	}

	void Empty()
	{
		m_sData.SetLength( 0 );
		m_iLen = 0;
	}

	void Append( const char *str, int len = -1 )
	{
		if ( str != NULL )
		{
			if ( len == -1 ) len = lstrlenA( str );
			int newLength = m_iLen + len;
			if ( m_sData.SetLength( newLength + 1 ) )
			{
				m_sData[ m_iLen ] = '\0';
				lstrcpynA( &m_sData[ m_iLen ], str, len + 1 );
				m_iLen = newLength;
			}
		}
	}

private:
	CMemBuffer< char, 128 > m_sData;
	int m_iLen;
};

class CPath
{
public:
	CPath( const char* lpszFileName )
	{
		if ( lpszFileName != NULL )
		{
			// ��������� ��������
			m_sPathOriginal.Append( lpszFileName );

			if ( ::PathIsURLA( lpszFileName ) == TRUE )
			{
				m_sPath.Append( lpszFileName );
			}
			else // ������ ��������������
			{
				// 1. ���������� ���������� ���������
				CMemBuffer< char, 1024 > sExpanded;
				::ExpandEnvironmentStringsA( lpszFileName, sExpanded.GetBuffer(), 1024 );
				// 2. ������� � ���� .. � . (�������� � ������������� ����)
				CMemBuffer< char, 1024 > sCanonical;
				::PathCanonicalizeA( sCanonical.GetBuffer(), sExpanded.GetBuffer() );
				// 3. ������� ������ �������
				::PathRemoveBlanksA( sCanonical.GetBuffer() );
				// 4. ��������� ���������� �� ��������������� ����
				if ( ::PathFileExists( sCanonical.GetBuffer() ) == TRUE )
				{
					::PathMakePrettyA( sCanonical.GetBuffer() );
					::PathRemoveBackslashA( sCanonical.GetBuffer() );
					m_sPath.Append( sCanonical.GetBuffer() );
					if ( ::PathIsDirectoryA( sCanonical.GetBuffer() ) == FALSE )
					{
						m_sFileName.Append( ::PathFindFileNameA( sCanonical.GetBuffer() ) );
						::PathRemoveFileSpecA( sCanonical.GetBuffer() );
					}
					m_sPathDir.Append( sCanonical.GetBuffer() );
				}
				else
				{
					// 5. �������� ���������
					char* pArg = ::PathGetArgsA( sCanonical.GetBuffer() );
					m_sFileParams.Append( pArg );
					::PathRemoveArgsA( sCanonical.GetBuffer() );
					// 6. ������ ���� �� ��������
					::PathUnquoteSpacesA( sCanonical.GetBuffer() );
					::PathRemoveBackslash( sCanonical.GetBuffer() );
					::PathMakePrettyA( sCanonical.GetBuffer() );
					// 7. ��������� ��������������� ���� ��� �����������
					if ( ::PathIsDirectoryA( sCanonical.GetBuffer() ) != FALSE )
					{
						m_sPath.Append( sCanonical.GetBuffer() );
						m_sPathDir.Append( sCanonical.GetBuffer() );
					}
					else
					{
						// 8. ��������� ���������� � ����� .exe, ���� ����
						::PathAddExtensionA( sCanonical.GetBuffer(), NULL );
						// 9. ��������� ���� �� ����� ����
						if ( ::PathFileExists( sCanonical.GetBuffer() ) == TRUE )
						{
							m_sPath.Append( sCanonical.GetBuffer() );
							m_sFileName.Append( ::PathFindFileNameA( sCanonical.GetBuffer() ) );
							::PathRemoveFileSpecA( sCanonical.GetBuffer() );
							m_sPathDir.Append( sCanonical.GetBuffer() );
						}
						else
						{
							// 10. ���������� �����
							::PathFindOnPathA( sCanonical.GetBuffer(), NULL );
							::PathMakePrettyA( sCanonical.GetBuffer() );
							m_sPath.Append( sCanonical.GetBuffer() );
							if ( ::PathFileExists( sCanonical.GetBuffer() ) == TRUE )
							{
								m_sFileName.Append( ::PathFindFileNameA( sCanonical.GetBuffer() ) );
								::PathRemoveFileSpecA( sCanonical.GetBuffer() );
								m_sPathDir.Append( sCanonical.GetBuffer() );
							}
						}
					}
				}
			}
		}
	}

	const char* GetPath()
	{
		return m_sPath.GetLenght() > 0 ? m_sPath.GetString() : NULL;
	}

	const char* GetDirectory()
	{
		return m_sPathDir.GetLenght() > 0 ? m_sPathDir.GetString() : NULL;
	}

	const char* GetFileParams()
	{
		return m_sFileParams.GetLenght() > 0 ? m_sFileParams.GetString() : NULL;
	}

private:
	CSimpleString m_sPathOriginal;
	CSimpleString m_sPath;
	CSimpleString m_sPathDir;
	CSimpleString m_sFileName;
	CSimpleString m_sFileParams;

public:
	static DWORD GetFileAttributes( const char* lpszFileName )
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if ( ::GetFileAttributesEx( lpszFileName, GetFileExInfoStandard, &fad ) == FALSE )
		{
			return ((DWORD)-1); //INVALID_FILE_ATTRIBUTES;
		}
		return fad.dwFileAttributes;
	}
	static BOOL SetFileAttributes( const char* lpszFileName, DWORD dwFileAttributes )
	{
		return ::SetFileAttributesA( lpszFileName, dwFileAttributes );
	}
	static BOOL IsDirectory( const char* lpszFileName )
	{
		return ::PathIsDirectoryA( lpszFileName ) != FALSE;
	}
	static BOOL IsFileExists( const char* lpszFileName )
	{
		return IsPathExist( lpszFileName ) == TRUE &&
			   IsDirectory( lpszFileName ) == FALSE;
	}
	static BOOL IsPathExist( const char* lpszFileName )
	{
		return ::PathFileExistsA( lpszFileName ) != FALSE;
	}
};

// �������� ��������� ��������� �� ������
// ��� ������������ ������ ����� ������� LocalFree
static char* GetLastErrorString( DWORD* lastErrorCode, int* iLenMsg )
{
	char* lpMsgBuf;
	*lastErrorCode = ::GetLastError();
	::FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
					  NULL,
					  *lastErrorCode,
					  MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
					  (LPSTR)&lpMsgBuf,
					  0,
					  NULL );

	*iLenMsg = lstrlenA( lpMsgBuf );

	// trim right
	while ( *iLenMsg > 0 )
	{
		(*iLenMsg)--;
		if ( lpMsgBuf[ *iLenMsg ] == '\n' ||
			 lpMsgBuf[ *iLenMsg ] == '\r' ||
			 lpMsgBuf[ *iLenMsg ] == '.' ||
			 lpMsgBuf[ *iLenMsg ] == ' ' )
		{
			lpMsgBuf[ *iLenMsg ] = 0;
		}
		else
		{
			break;
		}
	}
	(*iLenMsg)++;
	return lpMsgBuf;
}

static void lua_pushlasterr( lua_State* L, const char* lpszFunction )
{
	DWORD dw;
	int iLenMsg;
	char* lpMsgBuf = GetLastErrorString( &dw, &iLenMsg );

	if ( lpszFunction == NULL )
	{
		lua_pushstring( L, lpMsgBuf );
	}
	else
	{
		UINT uBytes = ( iLenMsg + lstrlenA( lpszFunction ) + 40 ) * sizeof(char);
		char* lpDisplayBuf = (char*)::LocalAlloc( LMEM_ZEROINIT, uBytes );
		sprintf( lpDisplayBuf, "%s failed with error %d: %s", lpszFunction, dw, lpMsgBuf );
		lua_pushstring( L, lpDisplayBuf );
		::LocalFree( lpDisplayBuf );
	}
	::LocalFree( lpMsgBuf );
}

static int msgbox( lua_State* L )
{
	const char* text = luaL_checkstring( L, 1 );
	const char* title = lua_tostring( L, 2 );
	int options = (int)lua_tonumber( L, 3 ) | MB_TASKMODAL;
	int retCode = ::MessageBox( NULL, text, title == NULL ? "SciTE" : title, options );
	lua_pushnumber( L, retCode );
	return 1;
}

static int getfileattr( lua_State *L )
{
	const char* FN = luaL_checkstring( L, -1 );
	lua_pushnumber( L, CPath::GetFileAttributes( FN ) );
	return 1;
}

static int setfileattr( lua_State* L )
{
	const char* FN = luaL_checkstring( L, -2 );
	DWORD attr = luaL_checkint( L, -1 );
	lua_pushboolean( L, CPath::SetFileAttributes( FN, attr ) );
	return 1;
}

static int fileexists( lua_State* L )
{
	const char* FN = luaL_checkstring( L, 1 );
	lua_pushboolean( L, CPath::IsPathExist( FN ) );
	return 1;
}

// ��������� ����� CreateProcess � ������� ������
static BOOL RunProcessHide( CPath& path, DWORD* out_exitcode, CSimpleString* strOut )
{
	static const int MAX_CMD = 1024;

	STARTUPINFO si = { sizeof(si) };
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	// ������������� ����������� ������ �� ������ �����/������
	BOOL bUsePipes = FALSE;
	HANDLE FWritePipe = NULL;
	HANDLE FReadPipe = NULL;
	SECURITY_ATTRIBUTES pa = { sizeof(pa), NULL, TRUE };
	bUsePipes = ::CreatePipe( &FReadPipe, &FWritePipe, &pa, 0 );
	if ( bUsePipes != FALSE )
	{
		si.hStdOutput = FWritePipe;
		si.hStdInput = FReadPipe;
		si.hStdError = FWritePipe;
		si.dwFlags = STARTF_USESTDHANDLES | si.dwFlags;
	}

	// ��������� �������
	CMemBuffer< char, MAX_CMD > bufCmdLine; // ��������� ����� ������ MAX_CMD
	bufCmdLine.GetBuffer()[0] = 0;
	strcat( bufCmdLine.GetBuffer(), "\"" );
	strcat( bufCmdLine.GetBuffer(), path.GetPath() );
	strcat( bufCmdLine.GetBuffer(), "\"" );
	if ( path.GetFileParams() != NULL )
	{
		strcat( bufCmdLine.GetBuffer(), " " );
		strcat( bufCmdLine.GetBuffer(), path.GetFileParams() );
	}

	PROCESS_INFORMATION pi = { 0 };
	BOOL RetCode = ::CreateProcess( NULL, // �� ���������� ��� �����, ��� � ������ �������
									bufCmdLine.GetBuffer(), // ������ �������
									NULL, // Process handle not inheritable
									NULL, // Thread handle not inheritable
									TRUE, // Set handle inheritance to FALSE
									0, // No creation flags
									NULL, // Use parent's environment block
									NULL, //path.GetDirectory(), // ������������� ����������� �������
									&si, // STARTUPINFO
									&pi ); // PROCESS_INFORMATION

	// ���� ��������� ������ �������� �� ������
	if ( RetCode == FALSE )
	{
		::CloseHandle( FReadPipe );
		::CloseHandle( FWritePipe );
		return FALSE;
	}

	// ��������� ��������� ������, � ��� ��� ������������� 
	::CloseHandle( pi.hThread );

	// ������� ���������� ������ ��������
	try
	{
		DWORD BytesToRead = 0;
		DWORD BytesRead = 0;
		DWORD TotalBytesAvail = 0;
		DWORD PipeReaded = 0;
		DWORD exit_code = 0;
		CMemBuffer< char, MAX_CMD > bufStr; // ��������� ����� ������ MAX_CMD
		while ( ::PeekNamedPipe( FReadPipe, NULL, 0, &BytesRead, &TotalBytesAvail, NULL ) )
		{
			if ( TotalBytesAvail == 0 )
			{
				if ( ::GetExitCodeProcess( pi.hProcess, &exit_code ) == FALSE ||
					 exit_code != STILL_ACTIVE )
				{
					break;
				}
				else
				{
					continue;
				}
			}
			else
			{
				while ( TotalBytesAvail > BytesRead )
				{
					if ( TotalBytesAvail - BytesRead > MAX_CMD - 1 )
					{
						BytesToRead = MAX_CMD - 1;
					}
					else
					{
						BytesToRead = TotalBytesAvail - BytesRead;
					}
					if ( ::ReadFile( FReadPipe,
									 bufCmdLine.GetBuffer(),
									 BytesToRead,
									 &PipeReaded,
									 NULL ) == FALSE )
					{
						break;
					}
					if ( PipeReaded <= 0 ) continue;
					BytesRead += PipeReaded;
					bufCmdLine[ PipeReaded ] = '\0';
					::OemToAnsi( bufCmdLine.GetBuffer(), bufStr.GetBuffer() );
					strOut->Append( bufStr.GetBuffer() );
				}
			}
		}
	}
	catch (...)
	{
	}

	// ��� ���������� ��������
	::GetExitCodeProcess( pi.hProcess, out_exitcode );
	::CloseHandle( pi.hProcess );
	::CloseHandle( FReadPipe );
	::CloseHandle( FWritePipe );
	return TRUE;
}

// ��������� ����� ShellExecuteEx � ������� ������
// (��. ��������� � ��������)
static BOOL ExecuteHide( CPath& path, DWORD* out_exitcode, CSimpleString* strOut )
{
	HANDLE hSaveStdin = NULL;
	HANDLE hSaveStdout = NULL;
	HANDLE hChildStdoutRdDup = NULL;
	HANDLE hChildStdoutWr = NULL;
	try
	{
		// ���������� �������
		STARTUPINFO si = { sizeof(si) };
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi = { 0 };
		::CreateProcess( NULL, // �� ���������� ��� �����, ��� � ������ �������
						 "cmd", // Command line
						 NULL, // Process handle not inheritable
						 NULL, // Thread handle not inheritable
						 TRUE, // Set handle inheritance to FALSE
						 0, // No creation flags
						 NULL, // Use parent's environment block
						 NULL, // Use parent's starting directory
						 &si, // STARTUPINFO
						 &pi ); // PROCESS_INFORMATION
		// �������� ����� ������� ������ ���������
		::WaitForSingleObject( pi.hProcess, 100 );
		BOOL hResult = FALSE;
		HMODULE hLib = LoadLibraryA("Kernel32.dll");
		if ( hLib != NULL )
		{
			typedef BOOL (STDAPICALLTYPE *ATTACHCONSOLE)( DWORD dwProcessId );
			ATTACHCONSOLE _AttachConsole = NULL;
			_AttachConsole = (ATTACHCONSOLE)GetProcAddress( hLib, "AttachConsole" );
			if ( _AttachConsole ) hResult = _AttachConsole( pi.dwProcessId );
			FreeLibrary( hLib );
		}
		if ( hResult == FALSE ) AllocConsole();

		TerminateProcess( pi.hProcess, 0 );
		CloseHandle( pi.hProcess );
		CloseHandle( pi.hThread );

		HANDLE hChildStdinRd;
		HANDLE hChildStdinWr;
		HANDLE hChildStdinWrDup;
		HANDLE hChildStdoutRd;

		// Set the bInheritHandle flag so pipe handles are inherited. 
		SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
		BOOL fSuccess;

		// The steps for redirecting child process's STDOUT: 
		//     1. Save current STDOUT, to be restored later. 
		//     2. Create anonymous pipe to be STDOUT for child process. 
		//     3. Set STDOUT of the parent process to be write handle to 
		//        the pipe, so it is inherited by the child process. 
		//     4. Create a noninheritable duplicate of the read handle and
		//        close the inheritable read handle. 

		// Save the handle to the current STDOUT. 
		hSaveStdout = GetStdHandle( STD_OUTPUT_HANDLE );

		// Create a pipe for the child process's STDOUT.
		if ( !CreatePipe( &hChildStdoutRd, &hChildStdoutWr, &saAttr, 0 ) ) throw(1);

		// Set a write handle to the pipe to be STDOUT. 
		if ( !SetStdHandle( STD_OUTPUT_HANDLE, hChildStdoutWr ) ) throw(1);

		// Create noninheritable read handle and close the inheritable read 
		// handle.
		fSuccess = DuplicateHandle( GetCurrentProcess(),
									hChildStdoutRd,
									GetCurrentProcess(),
									&hChildStdoutRdDup,
									0,
									FALSE,
									DUPLICATE_SAME_ACCESS );
		if( fSuccess == FALSE ) throw(1);
		CloseHandle( hChildStdoutRd );

		// The steps for redirecting child process's STDIN: 
		//     1.  Save current STDIN, to be restored later. 
		//     2.  Create anonymous pipe to be STDIN for child process. 
		//     3.  Set STDIN of the parent to be the read handle to the 
		//         pipe, so it is inherited by the child process. 
		//     4.  Create a noninheritable duplicate of the write handle, 
		//         and close the inheritable write handle. 

		// Save the handle to the current STDIN. 
		hSaveStdin = GetStdHandle( STD_INPUT_HANDLE );

		// Create a pipe for the child process's STDIN. 
		if ( !CreatePipe( &hChildStdinRd, &hChildStdinWr, &saAttr, 0 ) ) throw(1);

		// Set a read handle to the pipe to be STDIN. 
		if ( !SetStdHandle( STD_INPUT_HANDLE, hChildStdinRd ) ) throw(1);

		// Duplicate the write handle to the pipe so it is not inherited. 
		fSuccess = DuplicateHandle( GetCurrentProcess(),
									hChildStdinWr,
									GetCurrentProcess(),
									&hChildStdinWrDup,
									0,
									FALSE,
									DUPLICATE_SAME_ACCESS );
		if ( fSuccess == FALSE ) throw(1);

		CloseHandle( hChildStdinWr );
	}
	catch (...)
	{
		return FALSE;
	}

	// Now create the child process.
	SHELLEXECUTEINFO shinf = { sizeof(SHELLEXECUTEINFO) };
	shinf.lpFile = path.GetPath();
	shinf.lpParameters = path.GetFileParams();
	//shinf.lpDirectory = path.GetDirectory();
	shinf.fMask = SEE_MASK_FLAG_NO_UI |
				  SEE_MASK_NO_CONSOLE |
				  SEE_MASK_FLAG_DDEWAIT |
				  SEE_MASK_NOCLOSEPROCESS;
	shinf.nShow = SW_HIDE;
	BOOL bSuccess = ::ShellExecuteEx( &shinf );
	if ( bSuccess && shinf.hInstApp <= (HINSTANCE)32 ) bSuccess = FALSE;
	HANDLE hProcess = shinf.hProcess;

	try
	{
		if ( bSuccess == FALSE || hProcess == NULL ) throw(1);

		if ( hChildStdoutWr != NULL )
		{
			CloseHandle( hChildStdoutWr );
			hChildStdoutWr = NULL;
		}

		// After process creation, restore the saved STDIN and STDOUT.
		if ( hSaveStdin != NULL )
		{
			if ( !SetStdHandle( STD_INPUT_HANDLE, hSaveStdin ) ) throw(1);
			CloseHandle( hSaveStdin );
			hSaveStdin = NULL;
		}

		if ( hSaveStdout != NULL )
		{
			if ( !SetStdHandle( STD_OUTPUT_HANDLE, hSaveStdout ) ) throw(1);
			CloseHandle( hSaveStdout );
			hSaveStdout = NULL;
		}

		if ( hChildStdoutRdDup != NULL )
		{
			// Read output from the child process, and write to parent's STDOUT.
			const int BUFSIZE = 1024;
			DWORD dwRead;
			CMemBuffer< char, BUFSIZE > bufStr; // ��������� �����
			CMemBuffer< char, BUFSIZE > bufCmdLine; // ��������� �����
			for (;;)
			{
				if( ReadFile( hChildStdoutRdDup,
							  bufCmdLine.GetBuffer(),
							  BUFSIZE,
							  &dwRead,
							  NULL ) == FALSE ||
					dwRead == 0 )
				{
					DWORD exit_code = 0;
					if ( ::GetExitCodeProcess( hProcess, &exit_code ) == FALSE ||
						 exit_code != STILL_ACTIVE )
					{
						break;
					}
					else
					{
						continue;
					}
				}
				bufCmdLine[ dwRead ] = '\0';
				::OemToAnsi( bufCmdLine.GetBuffer(), bufStr.GetBuffer() );
				strOut->Append( bufStr.GetBuffer() );
			}
			CloseHandle( hChildStdoutRdDup );
			hChildStdoutRdDup = NULL;
		}
		FreeConsole();
	}
	catch (...)
	{
		if ( hChildStdoutWr != NULL ) CloseHandle( hChildStdoutWr );
		if ( hSaveStdin != NULL ) CloseHandle( hSaveStdin );
		if ( hSaveStdout != NULL ) CloseHandle( hSaveStdout );
		if ( hChildStdoutRdDup != NULL ) CloseHandle( hChildStdoutRdDup );
		if ( bSuccess == FALSE || hProcess == NULL ) return FALSE;
	}

	::GetExitCodeProcess( hProcess, out_exitcode );
	CloseHandle( hProcess );
	return TRUE;
}

static int exec( lua_State* L )
{
	// ��������� ����������� �������
	CPath file = luaL_checkstring( L, 1 );
	const char* verb = lua_tostring( L, 2 );
	int noshow = lua_toboolean( L, 3 );
	int dowait = lua_toboolean( L, 4 );

	BOOL useConsoleOut = dowait && noshow && ( verb == NULL );

	DWORD exit_code = (DWORD)-1;
	BOOL bSuccess = FALSE;
	CSimpleString strOut;

	if ( useConsoleOut != FALSE )
	{
		bSuccess = RunProcessHide( file, &exit_code, &strOut ) ||
				   ExecuteHide( file, &exit_code, &strOut );
	}
	else
	{
		HANDLE hProcess = NULL;
		// ��������� �������
		if ( verb != NULL && // ���� ���� ������� �������
			 strcmp( verb, "explore" ) == 0 && // ���� ������� ������� explore
			 CPath::IsFileExists( file.GetPath() ) ) // ��������� ���� �� ���
		{
			SHELLEXECUTEINFO shinf = { sizeof(SHELLEXECUTEINFO) };
			shinf.lpFile = "explorer.exe";
			CSimpleString sFileParams;
			sFileParams.Append( "/e, /select," );
			sFileParams.Append( file.GetPath() );
			shinf.lpParameters = sFileParams.GetString();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
						  SEE_MASK_NO_CONSOLE |
						  SEE_MASK_FLAG_DDEWAIT |
						  SEE_MASK_NOCLOSEPROCESS;
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx( &shinf );
			if ( bSuccess && shinf.hInstApp <= (HINSTANCE)32 ) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}
		else if ( verb != NULL && // ���� ���� ������� �������
				  strcmp( verb, "select" ) == 0 && // ���� ������� ������� select
				  CPath::IsPathExist( file.GetPath() ) ) // ��������� ���������� ����
		{
			SHELLEXECUTEINFO shinf = { sizeof(SHELLEXECUTEINFO) };
			shinf.lpFile = "explorer.exe";
			CSimpleString sFileParams;
			sFileParams.Append( "/select," );
			sFileParams.Append( file.GetPath() );
			shinf.lpParameters = sFileParams.GetString();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
						  SEE_MASK_NO_CONSOLE |
						  SEE_MASK_FLAG_DDEWAIT |
						  SEE_MASK_NOCLOSEPROCESS;
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx( &shinf );
			if ( bSuccess && shinf.hInstApp <= (HINSTANCE)32 ) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}
		else
		{
			SHELLEXECUTEINFO shinf = { sizeof(SHELLEXECUTEINFO) };
			shinf.lpFile = file.GetPath();
			shinf.lpParameters = file.GetFileParams();
			shinf.lpVerb = verb;
			//shinf.lpDirectory = file.GetDirectory();
			shinf.fMask = SEE_MASK_FLAG_NO_UI |
						  SEE_MASK_NO_CONSOLE |
						  SEE_MASK_FLAG_DDEWAIT;
			if ( verb == NULL )
			{
				shinf.fMask |= SEE_MASK_NOCLOSEPROCESS;
			}
			else
			{
				shinf.fMask |= SEE_MASK_INVOKEIDLIST;
			}
			shinf.nShow = noshow ? SW_HIDE : SW_SHOWNORMAL;
			bSuccess = ::ShellExecuteEx( &shinf );
			if ( bSuccess && shinf.hInstApp <= (HINSTANCE)32 ) bSuccess = FALSE;
			hProcess = shinf.hProcess;
		}

		if ( dowait != FALSE && hProcess != NULL )
		{
			// ���� ���� ������� �� ����������
			::WaitForSingleObject( hProcess, INFINITE );
		}

		if ( hProcess != NULL )
		{
			if ( dowait != FALSE ) ::GetExitCodeProcess( hProcess, &exit_code );
			CloseHandle( hProcess );
		}

		if ( bSuccess != FALSE )
		{
			::SetLastError( 0 );
			DWORD dw;
			int len;
			char* lpMsgBuf = GetLastErrorString( &dw, &len );
			strOut.Append( lpMsgBuf );
			::LocalFree( lpMsgBuf );
		}
	}

	if ( bSuccess == FALSE )
	{
		lua_pushboolean( L, FALSE );
		lua_pushlasterr( L, NULL );
	}
	else
	{
		exit_code != (DWORD)-1 ? lua_pushnumber( L, exit_code ) : lua_pushboolean( L, TRUE );
		lua_pushstring( L, strOut.GetString() );
	}

	return 2;
}

static int getclipboardtext( lua_State* L )
{
	CSimpleString clipText;
	if ( ::IsClipboardFormatAvailable( CF_TEXT ) )
	{
		if ( ::OpenClipboard( NULL ) )
		{
			HANDLE hData = ::GetClipboardData( CF_TEXT );
			if ( hData != NULL )
			{
				clipText.Append( (char*)::GlobalLock( hData ) );
				::GlobalUnlock( hData );
			}
			::CloseClipboard();
		}
	}
	lua_pushstring( L, clipText.GetString() );
	return 1;
}

static int findfiles( lua_State* L )
{
	const char* filename = luaL_checkstring( L, 1 );

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = ::FindFirstFileA( filename, &findFileData );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		// create table for result
		lua_createtable( L, 1, 0 );

		lua_Integer num = 1;
		BOOL isFound = TRUE;
		while ( isFound != FALSE )
		{
			// store file info
			lua_pushinteger( L, num );
			lua_createtable( L, 0, 4 );

			lua_pushstring( L, findFileData.cFileName );
			lua_setfield( L, -2, "name" );

			lua_pushboolean( L, findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY );
			lua_setfield( L, -2, "isdirectory" );

			lua_pushnumber( L, findFileData.dwFileAttributes );
			lua_setfield( L, -2, "attributes" );

			lua_pushnumber( L, findFileData.nFileSizeHigh * ((lua_Number)MAXDWORD + 1) +
							   findFileData.nFileSizeLow );
			lua_setfield( L, -2, "size" );

			lua_settable( L, -3 );
			num++;

			// next
			isFound = ::FindNextFileA( hFind, &findFileData );
		}

		::FindClose( hFind );

		return 1;
	}

	// files not found
	return 0;
}

extern int showinputbox(lua_State*);

#pragma warning(pop)

static const struct luaL_reg shell[] = 
{
	{ "exec", exec },
	{ "msgbox", msgbox },
	{ "getfileattr", getfileattr },
	{ "setfileattr", setfileattr },
	{ "fileexists", fileexists },
	{ "getclipboardtext", getclipboardtext },
	{ "findfiles", findfiles },
	{ "inputbox", showinputbox },
	{ NULL, NULL }
};

extern "C" __declspec(dllexport) int luaopen_shell( lua_State* L )
{
	luaL_register( L, "shell", shell );
	return 1;
}
