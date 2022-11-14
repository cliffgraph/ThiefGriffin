#include "pch.h"
#include "CComPort.h"
#include "setupapi.h"
#include <winioctl.h>
#include <process.h> 

#pragma comment(lib,"setupapi.lib")

void CComPort::GetListupComs(std::vector<COMM_NAMES> *pList)
{
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, 0, 0, DIGCF_PRESENT| DIGCF_DEVICEINTERFACE);
 
	for (int t = 0; true; ++t) {
	 	SP_DEVINFO_DATA DeviceInfoData;
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(hDevInfo, t, &DeviceInfoData) )
			break;

		HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV,KEY_QUERY_VALUE);
		if (hKey != INVALID_HANDLE_VALUE) {
			BYTE buf[256];
			DWORD type=0;
			DWORD len = sizeof(buf);
			// "COM#" 名称 -> sname
			RegQueryValueEx(hKey,_T("PortName"), nullptr, &type, (LPBYTE)buf, &len);
			const tstring sname(reinterpret_cast<TCHAR *>(buf));
			if (sname.find_first_of(_T("COM")) != tstring::npos) {
				// "説明(COM#)" -> fname
				if (SetupDiGetDeviceRegistryProperty(
						hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, &type, buf, sizeof(buf), nullptr)) {
					const tstring fname(reinterpret_cast<TCHAR *>(buf));
					COMM_NAMES ns = {fname, sname};
					pList->push_back(ns);
				}
			}
		}
	}
	return;
}

CComPort::CComPort()
{
	m_PortName.clear();
	m_hComm = INVALID_HANDLE_VALUE;
	m_hRecv = INVALID_HANDLE_VALUE;
	return;
}

CComPort::~CComPort()
{
	if (m_hComm != INVALID_HANDLE_VALUE)
		ClosePort();
	return;
}


void CComPort::launcherForRecvThread(void *pArgs)
{
	auto *pSts = reinterpret_cast<CComPort*>(pArgs);
	pSts->recvThread();
	return;
}

void CComPort::recvThread()
{
	m_bThreadAlive = true;

	while( m_bThreadAlive ) {
		DWORD waitResult = WaitForMultipleObjects(2, &m_hThreadEvents[0], false, INFINITE );
		if( waitResult == WAIT_OBJECT_0 )
			break; /* Thread finish */
		if (waitResult == WAIT_OBJECT_0 + 1)
		{
			DWORD size;
			if (GetOverlappedResult(m_hComm, &m_Ovr, &size, false) != 0) {
				if (size == 0)
					continue;
				if (m_hRecv != INVALID_HANDLE_VALUE)
					SetEvent(m_hRecv);
			}
			else {
				ResetEvent(m_Ovr.hEvent);
			}
		}
	}

	// 終了を親に通知
	SetEvent(m_hThreadEnd);

	// スレッドの終了
	_endthread();
	return;
}

HANDLE CComPort::GetHangle() const
{
	return m_hComm;
}


bool CComPort::OpenPort(HANDLE hRecv, const tstring &name)
{
	_tcscpy_s(m_DCB.devName, COMM_DCB::LEN_NAME, name.c_str());
	TCHAR comName[255 +1];
	_stprintf_s(comName, sizeof(comName) / sizeof(comName[0]), _T("\\\\.\\%s"), name.c_str());

	HANDLE hCom = CreateFile(
		comName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if (hCom == INVALID_HANDLE_VALUE) {
		// MessageBox(hWnd, comname, _TEXT("Open Error"), MB_OK);
		return false;
	}

	m_hRecv = hRecv;
	m_PortName = name;

	// 現状のエラーやバッファ破棄などを実行する
	DWORD DErr;
	ClearCommError(hCom, &DErr, nullptr);
	PurgeComm(hCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	// 通信パラメーターの設定
	auto &dcb = m_DCB.config.dcb;
	GetCommState(hCom, &dcb);
	dcb.DCBlength = sizeof(DCB);
	dcb.BaudRate = 1000000;			// 1Mbps   , ex)CBR_9600
	dcb.ByteSize = 8;
	dcb.fNull = false;
	dcb.fBinary = true;
	dcb.fParity = false;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fErrorChar = false;
	dcb.fAbortOnError = false;
	dcb.ErrorChar = 0x00;
	if (SetCommState(hCom, &dcb) == 0) {
#if !defined(NDEBUG)
		DebugBreak();
#endif
	}
	//DWORD fNull : 1;       /* Enable Null stripping           */
	//DWORD fRtsControl : 2;  /* Rts Flow control                */
	//DWORD fAbortOnError : 1; /* Abort all reads and writes on Error */
	//DWORD fDummy2 : 17;     /* Reserved                        */
	//WORD wReserved;       /* Not currently used              */
	//WORD XonLim;          /* Transmit X-ON threshold         */
	//WORD XoffLim;         /* Transmit X-OFF threshold        */
	//BYTE ByteSize;        /* Number of bits/byte, 4-8        */
	//BYTE Parity;          /* 0-4=None,Odd,Even,Mark,Space    */
	//BYTE StopBits;        /* 0,1,2 = 1, 1.5, 2               */
	//char XonChar;         /* Tx and Rx X-ON character        */
	//char XoffChar;        /* Tx and Rx X-OFF character       */
	//char ErrorChar;       /* Error replacement char          */
	//char EofChar;         /* End of Input character          */
	//char EvtChar;         /* Received Event character        */
	//WORD wReserved1;      /* Fill for now.                   */

	// タイムアウト時間の設定
	COMMTIMEOUTS timeout;
	memset(&timeout, 0, sizeof timeout);
	timeout.ReadIntervalTimeout = MAXDWORD;
	timeout.ReadTotalTimeoutConstant = 0;
	timeout.ReadTotalTimeoutMultiplier = 0;
	timeout.WriteTotalTimeoutConstant = 0;
	timeout.WriteTotalTimeoutMultiplier = 0;
	SetCommTimeouts(hCom, &timeout);

	// 
	m_hThreadEvents[0]	= CreateEvent(nullptr, true, false, nullptr);		// スレッド終了要求イベント
	m_hThreadEvents[1]	= CreateEvent(nullptr, true, false, nullptr);		// 受信イベント
	m_hThreadEnd		= CreateEvent(nullptr, true, false, nullptr);		// スレッド終了しました通知イベント

	memset(&m_Ovr, 0, sizeof m_Ovr);
	m_Ovr.hEvent = m_hThreadEvents[1];

	m_hComm = hCom;

	// 受信のみを監視する
	SetCommMask(hCom, EV_RXCHAR);

	_beginthread(CComPort::launcherForRecvThread, 0, this);
	return true;
}

void CComPort::ClosePort()
{
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		// スレッドに終了を通知して、終了を待つ
		m_bThreadAlive = false;
		SetCommMask(m_hComm, 0);
		SetEvent(m_hThreadEvents[0]);
		WaitForSingleObject(m_hThreadEnd, INFINITE);
		//
		PurgeComm(m_hComm, PURGE_RXCLEAR);
		CloseHandle(m_hComm);
		//
		CloseHandle(m_hThreadEvents[0]);
		CloseHandle(m_hThreadEvents[1]);
		CloseHandle(m_hThreadEnd);
		m_hComm = m_hThreadEnd = INVALID_HANDLE_VALUE;
		m_hThreadEvents[0] = m_hThreadEvents[1] = INVALID_HANDLE_VALUE;
		m_PortName.clear();
	}
	return;
}

size_t CComPort::Send(const uint8_t data)
{
	assert(m_hComm != 0);
	DWORD sendSize;
	BOOL ret = ::WriteFile(m_hComm, &data, 1, &sendSize, &m_Ovr);
	if (ret == FALSE)
		sendSize = 0;
	return sendSize;
}

size_t CComPort::Send(const uint8_t *pData, const size_t sz)
{
	assert(m_hComm != 0);
	DWORD sendSize;
	BOOL ret = ::WriteFile(m_hComm, pData, static_cast<DWORD>(sz), &sendSize, &m_Ovr);
	if (ret == FALSE)
		sendSize = 0;
	return sendSize;
}

size_t CComPort::Receive(uint8_t *pData, const size_t areaSize)
{
	assert(m_hComm != 0);
	DWORD readSize;
	BOOL ret = ::ReadFile(m_hComm, pData, static_cast<DWORD>(areaSize), &readSize, &m_Ovr);
	if (ret == FALSE)
		readSize = 0;
	return readSize;
}

bool CComPort::IsOpend() const
{
	return (m_hComm != INVALID_HANDLE_VALUE);
}

const tstring &CComPort::GetPortName() const
{
	return m_PortName;
}

