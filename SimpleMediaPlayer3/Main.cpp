#include <stdio.h>
#include <dshow.h>

// 파일명을 저장 할 변수
char g_fileName[256];

// 경로 및 파일명을 저장 할 변수
char g_pathFileName[512];

/**
*	미디어 파일을 파일 탐색기를 통해 선택(파일의 경로 및 파일명을 가져옴)하는 메소드
*
*	@return
*/
BOOL GetMediaFileName(void) {
	// 오픈 파일 네임 객체
	OPENFILENAME ofn;

	// 오픈 파일 네임 객체 설정
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = NULL;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = NULL;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = (char*)calloc(1, 512);
	ofn.nMaxFile = 512;
	ofn.lpstrFileTitle = (char*)calloc(1, 512);
	ofn.nMaxFileTitle = 255;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = "Select file to render...";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = NULL;

	// 파일을 열 수 없는 경우
	if (!GetOpenFileName(&ofn)) {
		// 문자열들 메모리 해제
		free(ofn.lpstrFile);
		free(ofn.lpstrFileTitle);

		return false;
	}

	// 파일을 가져 온 경우 
	else {
		// 오픈 파일 네임 객체에서 경로 및 파일명을 복사
		strcpy(g_pathFileName, ofn.lpstrFile);

		// 오픈 파일 네임 객체에서 파일명을 복사
		strcpy(g_fileName, ofn.lpstrFileTitle);

		// 문자열들 메모리 해제
		free(ofn.lpstrFile);
		free(ofn.lpstrFileTitle);
	}

	return true;
}

/**
*	필터 그래프 매니져를 이용하여 스트림을 파일로 저장하는 메소드
*
*	@param pGraph (필터 그래프 매니져 포인터 변수)
*	@param wszPath (저장 경로)
*	@return
*/
HRESULT SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath) {
	const WCHAR wszStreamName[] = L"ActiveMovieGraph";
	HRESULT hr;
	IStorage *pStorage = NULL;

	// 파일을 만듬
	hr = StgCreateDocfile(wszPath, STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStorage);

	// 파일을 만드는데 실패했을 경우
	if (FAILED(hr)) {
		return hr;
	}

	IStream *pStream;

	// 파일에 쓰기 위한 스트림을 만듬
	hr = pStorage->CreateStream(wszStreamName, STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStream);

	// 스트림을 만드는데 실패했을 경우
	if (FAILED(hr)) {
		// 스토리지 자원 반납
		pStorage->Release();

		return hr;
	}

	IPersistStream *pPersist = NULL;

	// 필터 매니져을 통해 펄시스트 스트림 인터페이스를 가져옴
	pGraph->QueryInterface(IID_IPersistStream, reinterpret_cast<void **>(&pPersist));

	// 펄시스트 스트림에 저장
	hr = pPersist->Save(pStream, TRUE);

	// 사용했던 자원들 반납
	pStream->Release();
	pPersist->Release();

	// 펄시스트 스트림에 저장을 완료했을 경우
	if (SUCCEEDED(hr)) {
		// 파일에 저장
		hr = pStorage->Commit(STGC_DEFAULT);
	}

	// 스토리지 자원 반납
	pStorage->Release();

	return hr;
}

/**
*	@Deprecated
*	필터로 부터 INPUT 또는 OUTPUT 핀을 가져오는 메소드
*	(스플릿 필터와 같이 아웃풋이 2개인 경우 사용하지 못함)
*
*	@param pFilter
*	@param PinDir
*	@return
*/
IPin *GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir) {
	BOOL bFound = FALSE;
	IEnumPins *pEnum;
	IPin *pPin;

	// 필터로 부터 핀 구조체를 가져옴
	HRESULT hr = pFilter->EnumPins(&pEnum);

	if (FAILED(hr)) {
		return NULL;
	}

	// 핀 구조체를 돌면서 핀을 가져옴
	while (pEnum->Next(1, &pPin, 0) == S_OK) {
		PIN_DIRECTION PinDirThis;
		pPin->QueryDirection(&PinDirThis);

		// 핀의 방향에 일치하는 핀이 검색되면 종료
		if (bFound = (PinDir == PinDirThis)) {
			break;
		}

		pPin->Release();
	}

	pEnum->Release();

	return (bFound ? pPin : NULL);
}

/**
*	필터로 부터 연결되어있지 않은 INPUT 또는 OUTPUT 핀을 가져오는 메소드
*
*	@param pFilter
*	@param PinDir
*	@param ppPin
*	@return
*/
HRESULT GetUnConnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin) {
	IEnumPins *pEnum = 0;
	IPin *pPin = 0;

	if (!ppPin) {
		return E_POINTER;
	}

	*ppPin = 0;

	// 필터로 부터 핀 구조체를 가져옴
	HRESULT hr = pFilter->EnumPins(&pEnum);

	if (FAILED(hr)) {
		return hr;
	}

	// 핀 구조체를 돌면서 연결되어있지 않은 핀을 찾음
	while (pEnum->Next(1, &pPin, NULL) == S_OK) {
		PIN_DIRECTION ThisPinDir;

		pPin->QueryDirection(&ThisPinDir);

		// 핀의 방향에 일치하는 핀이 있을 경우
		if (ThisPinDir == PinDir) {
			IPin *pTmp = 0;

			// 해당 핀의 연결 여부를 확인
			hr = pPin->ConnectedTo(&pTmp);

			if (SUCCEEDED(hr)) {
				pTmp->Release();
			}

			else {
				pEnum->Release();

				// 연결되어 있지 않은 경우 핀 저장
				*ppPin = pPin;

				return S_OK;
			}
		}

		pPin->Release();
	}

	pEnum->Release();

	return E_FAIL;
}

int main() {
	// 필터 그래프 매니져 포인터 변수
	IGraphBuilder *pGraph = NULL;

	// 미디어 컨트롤 포인터 변수
	IMediaControl *pControl = NULL;

	// 미디어 이벤트 포인터 변수
	IMediaEvent *pEvent = NULL;

	// 필터 변수
	IBaseFilter *InputFileFilter = NULL;
	IBaseFilter *StreamSplitterFilter = NULL;
	IBaseFilter *VideoDecoderFilter = NULL;
	IBaseFilter *AudioDecoderFilter = NULL;
	IBaseFilter *VideoRendererFilter = NULL;
	IBaseFilter *AudioRendererFilter = NULL;

	// 핀 변수
	IPin *FileOutputPin = NULL;
	IPin *StreamSplitterInputPin = NULL;
	IPin *StreamSplitterOutputPin = NULL;
	IPin *VideoDecoderInputPin = NULL;
	IPin *VideoDecoderOutputPin = NULL;
	IPin *AudioDecoderInputPin = NULL;
	IPin *AudioDecoderOutputPin = NULL;
	IPin *VideoRendererInputPin = NULL;
	IPin *AudioRendererInputPin = NULL;

	// COM(Component Object Model) 라이브러리를 생성 (DirectShow는 COM 라이브러리 기반)
	HRESULT hr = CoInitialize(NULL);

	// COM 라이브러리가 생성되지 못한 경우
	if (FAILED(hr)) {
		// 에러 메세지 출력 및 프로그램 종료 
		printf("ERROR - Could not initialize COM library.\n");
		return hr;
	}

	// 필터 그래프 매니져 인스턴스 생성
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGraph);

	// 필터 그래프 매니져가 생성되지 못한 경우
	if (FAILED(hr)) {
		// 에러 메세지 출력 및 프로그램 종료
		printf("ERROR - Could not create the Filter Graph Manager.\n");
		return hr;
	}

	// 필터 그래프 매니져를 통해 미디어 컨트롤 인스턴스를 가져옴
	// (미디어 컨트롤의 역할 : 미디어 스트림 제어 ex. 시작, 정지, 일시정지)
	hr = pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);

	// 미디어 컨트롤 객체 생성이 실패했을 경우
	if (FAILED(hr)) {
		// 에러 메세지 출력
		printf("ERROR - Could not create the Media Control object.\n");

		// 필터 그래프 매니져 자원 반납
		pGraph->Release();

		// COM 라이브러리를 닫음
		CoUninitialize();

		// 프로그램 종료
		return hr;
	}

	// 필터 그래프 매니져를 통해 미디어 이벤트 인스턴스를 가져옴
	// (미디어 이벤트의 역할 : 필터 그래프 매니져의 이벤트를 수신 ex. 재생 완료)
	hr = pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);

	// 미디어 이벤트 객체 생성이 실패했을 경우
	if (FAILED(hr)) {
		// 에러 메세지 출력, 사용했던 자원 반환 및 프로그램 종료
		printf("ERROR - Could not create the Media Event object.\n");

		// 사용했던 자원들을 반납
		pGraph->Release();
		pControl->Release();

		// COM 라이브러리를 닫음
		CoUninitialize();

		// 프로그램 종료
		return hr;
	}

	// 미디어 파일의 경로와 이름을 가져오지 못한 경우
	if (!GetMediaFileName()) {
		// 프로그램 종료
		return 0;
	}

	// 유니코드 일 경우
#ifndef UNICODE
	WCHAR wFileName[MAX_PATH];

	// 파일의 경로 및 파일명을 멀티바이트 캐릭터로 변경
	MultiByteToWideChar(CP_ACP, 0, g_pathFileName, -1, wFileName, MAX_PATH);

	// 필터 그래프 매니져를 통해 소스 필터 생성
	hr = pGraph->AddSourceFilter(wFileName, wFileName, &InputFileFilter);
	// 유니코드가 아닌 경우
#else
	// 필터 그래프 매니져를 통해 소스 필터 생성
	hr = pGraph->AddSourceFilter(wFileName, wFileName, &InputFileFilter);
#endif

	// 소스 필터를 생성했을 경우
	if (SUCCEEDED(hr)) {
		// 스트림 스플릿 필터 인스턴스를 가져옴
		hr = CoCreateInstance(CLSID_MPEG1Splitter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&StreamSplitterFilter);

		// 스트림 스플릿 필터 인스턴스를 가져온 경우
		if (SUCCEEDED(hr)) {
			// 필터 그래프 매니져를 통해 스트림 스플릿 필터를 추가
			hr = pGraph->AddFilter(StreamSplitterFilter, L"Stream Splitter");

			// 스트림 스플릿 필터가 추가된 경우
			if (SUCCEEDED(hr)) {
				// 소스 필터의 아웃풋 핀과 스트림 스플릿 필터의 인풋 핀을 가져왔을 경우
				if (SUCCEEDED(GetUnConnectedPin(InputFileFilter, PINDIR_OUTPUT, &FileOutputPin)) &&
					SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_INPUT, &StreamSplitterInputPin))) {
					// 두 핀을 연결
					hr = pGraph->Connect(FileOutputPin, StreamSplitterInputPin);
				}

				// 비디오 디코더 필터 인스턴스를 가져옴
				hr = CoCreateInstance(CLSID_CMpegVideoCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&VideoDecoderFilter);

				// 비디오 디코더 필터 인스턴스를 가져온 경우
				if (SUCCEEDED(hr)) {
					// 필터 그래프 매니져를 통해 비디오 디코더 필터를 추가
					hr = pGraph->AddFilter(VideoDecoderFilter, L"Video Decoder");

					// 비디오 디코더 필터가 추가된 경우
					if (SUCCEEDED(hr)) {
						// 스트림 스플릿 필터의 아웃풋 핀과 비디오 디코터의 인풋 핀을 가져왔을 경우
						if (SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_OUTPUT, &StreamSplitterOutputPin)) &&
							SUCCEEDED(GetUnConnectedPin(VideoDecoderFilter, PINDIR_INPUT, &VideoDecoderInputPin))) {
							// 두 핀을 연결
							hr = pGraph->Connect(StreamSplitterOutputPin, VideoDecoderInputPin);
						}

						// 비디오 랜더 필터 인스턴스를 가져옴
						hr = CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&VideoRendererFilter);

						// 비디오 랜더 필터 인스턴스를 가져온 경우
						if (SUCCEEDED(hr)) {
							// 필터 그래프 매니져를 통해 비디오 랜더 필터를 추가
							hr = pGraph->AddFilter(VideoRendererFilter, L"Video Renderer");

							// 비디오 랜더 필터가 추가된 경우
							if (SUCCEEDED(hr)) {
								// 비디오 디코더 필터의 아웃풋 핀과 비디오 랜더 필터의 인풋 핀을 가져왔을 경우
								if (SUCCEEDED(GetUnConnectedPin(VideoDecoderFilter, PINDIR_OUTPUT, &VideoDecoderOutputPin)) &&
									SUCCEEDED(GetUnConnectedPin(VideoRendererFilter, PINDIR_INPUT, &VideoRendererInputPin))) {
									// 두 핀을 연결
									hr = pGraph->Connect(VideoDecoderOutputPin, VideoRendererInputPin);
								}
							}
						}
					}
				}

				// 오디오 디코더 필터 인스턴스를 가져옴
				hr = CoCreateInstance(CLSID_CMpegAudioCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&AudioDecoderFilter);

				// 오디오 디코더 필터 인스턴스를 가져온 경우
				if (SUCCEEDED(hr)) {
					// 필터 그래프 매니져를 통해 오디오 디코더 필터를 추가
					hr = pGraph->AddFilter(AudioDecoderFilter, L"Audio Decoder");

					// 오디오 디코더 필터가 추가된 경우
					if (SUCCEEDED(hr)) {
						// 스트림 스플릿 필터의 아웃풋 핀과 오디오 디코터의 인풋 핀을 가져왔을 경우
						if (SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_OUTPUT, &StreamSplitterOutputPin)) &&
							SUCCEEDED(GetUnConnectedPin(AudioDecoderFilter, PINDIR_INPUT, &AudioDecoderInputPin))) {
							// 두 핀을 연결
							hr = pGraph->Connect(StreamSplitterOutputPin, AudioDecoderInputPin);
						}

						// 오디오 랜더 필터 인스턴스를 가져옴
						hr = CoCreateInstance(CLSID_AudioRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&AudioRendererFilter);

						// 오디오 랜더 필터 인스턴스를 가져온 경우
						if (SUCCEEDED(hr)) {
							// 필터 그래프 매니져를 통해 오디오 랜더 필터를 추가
							hr = pGraph->AddFilter(AudioRendererFilter, L"Video Renderer");

							// 오디오 랜더 필터가 추가된 경우
							if (SUCCEEDED(hr)) {
								// 오디오 디코더 필터의 아웃풋 핀과 오디오 랜더 필터의 인풋 핀을 가져왔을 경우
								if (SUCCEEDED(GetUnConnectedPin(AudioDecoderFilter, PINDIR_OUTPUT, &AudioDecoderOutputPin)) &&
									SUCCEEDED(GetUnConnectedPin(AudioRendererFilter, PINDIR_INPUT, &AudioRendererInputPin))) {
									// 두 핀을 연결
									hr = pGraph->Connect(AudioDecoderOutputPin, AudioRendererInputPin);
								}
							}
						}
					}
				}
			}
		}

		// 미디어 컨트롤을 통해 스트림 재생
		hr = pControl->Run();

		// 스트림 재생이 된 경우
		if (SUCCEEDED(hr)) {
			long evCode;

			// 미디어 이벤트를 통해 재생 완료까지 대기
			pEvent->WaitForCompletion(INFINITE, &evCode);
		}

		// 미디어 컨트롤을 통해 스트림 정지
		hr = pControl->Stop();

		// 필터 그래프를 그래프에딧에서 볼 수 있도록 파일로 저장
		SaveGraphFile(pGraph, L"C:\\Users\\devetude\\Desktop\\SimpleMediaPlayer3\\MyGraph.GRF");
	}

	// 미디어 파일의 필터 그래프를 생성하지 못한 경우
	else {
		// 에러 메세지 출력
		printf("ERROR - Could not find the media fila.\n");
	}

	// 생성되었던 필터 변수들의 자원을 반납
	if (FileOutputPin) {
		FileOutputPin->Release();
	}

	if (StreamSplitterFilter) {
		StreamSplitterFilter->Release();
	}

	if (VideoDecoderFilter) {
		VideoDecoderFilter->Release();
	}

	if (AudioDecoderFilter) {
		AudioDecoderFilter->Release();
	}

	if (VideoRendererFilter) {
		VideoRendererFilter->Release();
	}

	if (AudioRendererFilter) {
		AudioRendererFilter->Release();
	}

	// 생성되었던 핀 변수들의 자원을 반납
	if (FileOutputPin) {
		FileOutputPin->Release();
	}

	if (StreamSplitterInputPin) {
		StreamSplitterInputPin->Release();
	}

	if (StreamSplitterOutputPin) {
		StreamSplitterOutputPin->Release();
	}

	if (VideoDecoderInputPin) {
		VideoDecoderInputPin->Release();
	}

	if (VideoDecoderOutputPin) {
		VideoDecoderOutputPin->Release();
	}

	if (AudioDecoderInputPin) {
		AudioDecoderInputPin->Release();
	}

	if (AudioDecoderOutputPin) {
		AudioDecoderOutputPin->Release();
	}

	if (VideoRendererInputPin) {
		VideoRendererInputPin->Release();
	}

	if (AudioRendererInputPin) {
		AudioRendererInputPin->Release();
	}

	// 사용했던 자원들을 반납
	pControl->Release();
	pEvent->Release();
	pGraph->Release();

	// COM 라이브러리를 닫음
	CoUninitialize();
}