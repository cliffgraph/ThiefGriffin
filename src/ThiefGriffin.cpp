// ThiefGriffin.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//
#include "pch.h"
#include <iostream>
#include "CComPort.h"
#include <deque>
#include "tools.h"
#include "tgf.h"
#include "if_thiefcom.h"
#include "CTimeCount.h"
#ifdef _WIN32
#include "timeapi.h"		// for timeBeginPeriod
#endif

struct OPTS{
	bool	bHelp;
	bool	bListComPort;
	tstring ComPortName;
	tstring SaveFilePath;
	OPTS() : bHelp(false), bListComPort(false){}
};

static void printTitle()
{
	wprintf(_T("ThiefGriffin. version 1.2 by Harumakkin 2022\n"));
	return;
}

static void printHelp()
{
	wprintf(
		_T(" -h : help message\n")
		_T(" -l : Com port list\n")
		_T(" -c : Target COM port name. ex> -c COM1\n")
		_T(" -f : save file path\n")
	);
	std::vector<TGF_ATOM> v;
	wprintf(_T("std::vector max_size = %I64u\n"), static_cast<uint64_t>(v.max_size()));
	return;
}

static void printCommPortList()
{
	// COMポート一覧の表示
	std::unique_ptr<std::vector<COMM_NAMES>> pList(NNEW std::vector<COMM_NAMES>);
	std::unique_ptr<CComPort> pPort(NNEW CComPort());
	pPort->GetListupComs(pList.get());
	for( auto n : *pList ){
	    std::wcout << n.ShortName << _T(" : ") << n.Name << _T("\n");
	}
	std::wcout << _T("\n");
	return;
}

struct PRINTINFO
{
	int	counter;
	int	counter_old;
	int tc;
	int unknown_counter;
	int counter_per_tc;
	int max_counter_per_tc;
	PRINTINFO() : counter(0), counter_old(0), tc(0), unknown_counter(0), counter_per_tc(0), max_counter_per_tc(0) {}
};

static void printRecvThiefCom(const thiefcom::THIEFCOM &dt, PRINTINFO *pInfo)
{
	wprintf_s(_T("%d (%3d/%d), %d (%d): "),
		pInfo->counter, pInfo->counter_per_tc, pInfo->max_counter_per_tc, pInfo->tc, pInfo->unknown_counter);
	if ((dt.code & thiefcom::CODE_TC) != 0) {
		pInfo->tc = static_cast<int>(((dt.code & 0x7f) << 16) | (dt.data1 << 8) | dt.data2);
		wprintf_s(_T(" TC----%06X"), pInfo->tc);
		pInfo->counter_per_tc = pInfo->counter - pInfo->counter_old;
		if (pInfo->max_counter_per_tc < pInfo->counter_per_tc)
			pInfo->max_counter_per_tc = pInfo->counter_per_tc;
		pInfo->counter_old = pInfo->counter;
	}
	else switch(dt.code){
		case thiefcom::CODE_SYSINFO:
			wprintf_s(_T(" SYS---%02X %02X%02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_WAIT:
			wprintf_s(_T(" WAIt--%02X %04X"), dt.code, static_cast<int>((dt.data1 << 8) | dt.data2));
			break;
		case thiefcom::CODE_OPLL:
			wprintf_s(_T(" OPLL: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_PSG:
			wprintf_s(_T("  PSG: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_SCC_90:
			wprintf_s(_T("SCC90: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_SCC_98:
			wprintf_s(_T("SCC98: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_SCC_B0:
			wprintf_s(_T("SCCB0: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_SCC_B8:
			wprintf_s(_T("SCCB8: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		case thiefcom::CODE_SCC_BF:
			wprintf_s(_T("SCCBF: %02X %02X %02X"), dt.code, dt.data1, dt.data2);
			break;
		default:
			wprintf_s(_T(" ????: %02X-%02X-%02X"), dt.code, dt.data1, dt.data2);
			++pInfo->unknown_counter;
			break;
	}
	wprintf_s(_T("\r"));
	return;
}

static void pushToFileData(
	const thiefcom::THIEFCOM &dt, std::vector<TGF_ATOM> *pList, PRINTINFO *pInfo)
{
	TGF_ATOM atom;

	if( (dt.code & 0x80) != 0 ){
		atom.mark = TGF_M_TC;
		atom.data1 = static_cast<uint16_t>(dt.code&0x7f);
		atom.data2 = static_cast<uint16_t>((dt.data1 << 8) | dt.data2);
		pList->push_back(atom);
	}
	else switch(dt.code){
		case thiefcom::CODE_SYSINFO:
			atom.mark	= TGF_M_SYSINFO;
			atom.data1	= static_cast<uint16_t>((dt.data1<<8)|dt.data2);
			atom.data2	= 0x0000;
			pList->push_back(atom);
			break;
		case thiefcom::CODE_WAIT:
			atom.mark	= TGF_M_WAIT;
			atom.data1	= static_cast<uint16_t>((dt.data1<<8)|dt.data2);
			atom.data2	= 0x0000;
			pList->push_back(atom);
			break;
		//
		case thiefcom::CODE_OPLL:
			atom.mark	= TGF_M_OPLL;
			atom.data1	= dt.data1;
			atom.data2	= dt.data2;
			pList->push_back(atom);
			break;
		//
		case thiefcom::CODE_PSG:
			atom.mark	= TGF_M_PSG;
			atom.data1	= dt.data1;
			atom.data2	= dt.data2;
			pList->push_back(atom);
 			break;
		//
		case thiefcom::CODE_SCC_90:
			atom.mark	= TGF_M_SCC;
			atom.data1	= static_cast<uint16_t>(0x9000|dt.data1);
			atom.data2	= static_cast<uint16_t>(dt.data2);
			pList->push_back(atom);
 			break;
		case thiefcom::CODE_SCC_98:
			atom.mark	= TGF_M_SCC;
			atom.data1	= static_cast<uint16_t>(0x9800|dt.data1);
			atom.data2	= static_cast<uint16_t>(dt.data2);
			pList->push_back(atom);
 			break;
		case thiefcom::CODE_SCC_B0:
			atom.mark	= TGF_M_SCC;
			atom.data1	= static_cast<uint16_t>(0xB000|dt.data1);
			atom.data2	= static_cast<uint16_t>(dt.data2);
			pList->push_back(atom);
 			break;
		case thiefcom::CODE_SCC_B8:
			atom.mark	= TGF_M_SCC;
			atom.data1	= static_cast<uint16_t>(0xB800|dt.data1);
			atom.data2	= static_cast<uint16_t>(dt.data2);
			pList->push_back(atom);
 			break;
		case thiefcom::CODE_SCC_BF:
			atom.mark	= TGF_M_SCC;
			atom.data1	= static_cast<uint16_t>(0xBF00|dt.data1);
			atom.data2	= static_cast<uint16_t>(dt.data2);
			pList->push_back(atom);
 			break;
	}
	return;
}


static bool options(int argc, wchar_t *argv[], OPTS *pOpts)
{
	if( argc <= 1 )
		return false;
	for( int t = 0; t < argc; ++t){
		tstring s(argv[t]);
		if( s == _T("-l") ){
			pOpts->bListComPort = true;
		}
		else if( s == _T("-h") ){
			pOpts->bHelp = true;
		}
		else if( s == _T("-c") ){
			if( argc <= (t+1) )
				return false;
			pOpts->ComPortName = argv[t+1];
		}
		else if( s == _T("-f") ){
			if( argc <= (t+1) )
				return false;
			pOpts->SaveFilePath = argv[t+1];
		}
	}
	return true;
}

int wmain(int argc, wchar_t *argv[])
{
	OPTS opts;
	printTitle();
	if( !options(argc, argv, &opts) ){
		return 1;
	}
	if( opts.bHelp ){
		printHelp();
		return 0;
	}
	if( opts.bListComPort ){
		printCommPortList();
		return 0;
	}

	// COMポートを開く
	std::unique_ptr<CComPort> pPort(NNEW CComPort());
	HANDLE hWaitRecv = CreateEvent(nullptr, true, false, nullptr);		// 受信を通知するイベント
	if (hWaitRecv == 0)
		return 1;
	if( !pPort->OpenPort(hWaitRecv, opts.ComPortName) ){
	    std::wcout << " Can't open COM port:" << opts.ComPortName << "\n";
		return 1;
	}
	else {
		std::wcout << " Opend COM port:" << opts.ComPortName << "\n";
	}

	// 受信バッファ、データ構築用領域の各頬
	static const size_t RECVSIZE = 1024*4;
	std::unique_ptr<uint8_t[]> pRecvData(NNEW uint8_t[RECVSIZE]);
	std::unique_ptr<std::deque<thiefcom::THIEFCOM>> pFIFO(NNEW std::deque<thiefcom::THIEFCOM>());
	std::unique_ptr<std::vector<TGF_ATOM>> pTgfAtoms(NNEW std::vector<TGF_ATOM>());

#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	// 受信
	bool bFirst = true;
	PRINTINFO prtinfo;
	thiefcom::THIEFCOM temp;
	int index = 0;
	while(!_kbhit()) {
	 	size_t s = pPort->Receive(pRecvData.get(), RECVSIZE);
	 	if (s == 0)
	 		continue;
	 	for( size_t t = 0; t < s; ++t){
			temp.data[index++] = pRecvData[t];
			if( index == sizeof(temp.data) ){
				index = 0;
				pFIFO->push_back(temp);
			}
		} 
		while( !pFIFO->empty() ){
			++prtinfo.counter;
			auto &dt = pFIFO->front();
			printRecvThiefCom(dt, &prtinfo);
			pushToFileData(dt, pTgfAtoms.get(), &prtinfo);
			pFIFO->pop_front();
		}
	}
	pPort->ClosePort();
	CloseHandle(hWaitRecv);

#ifdef _WIN32
	timeEndPeriod(1);
#endif

	if( !opts.SaveFilePath.empty() ){
		_tremove(opts.SaveFilePath.c_str());
		std::wcout << _T("Writing to ") << opts.SaveFilePath << _T("\n");
		size_t num = sizeof(TGF_ATOM) * pTgfAtoms->size();
		t_WriteFileAdd(opts.SaveFilePath, reinterpret_cast<const uint8_t*>(pTgfAtoms->data()), num);
		std::wcout << _T("count = ") << (int)pTgfAtoms->size() << _T(".\n");
	}

    return 0;
}
