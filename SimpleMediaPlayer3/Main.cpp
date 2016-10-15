#include <stdio.h>
#include <dshow.h>

// ���ϸ��� ���� �� ����
char g_fileName[256];

// ��� �� ���ϸ��� ���� �� ����
char g_pathFileName[512];

/**
*	�̵�� ������ ���� Ž���⸦ ���� ����(������ ��� �� ���ϸ��� ������)�ϴ� �޼ҵ�
*
*	@return
*/
BOOL GetMediaFileName(void) {
	// ���� ���� ���� ��ü
	OPENFILENAME ofn;

	// ���� ���� ���� ��ü ����
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

	// ������ �� �� ���� ���
	if (!GetOpenFileName(&ofn)) {
		// ���ڿ��� �޸� ����
		free(ofn.lpstrFile);
		free(ofn.lpstrFileTitle);

		return false;
	}

	// ������ ���� �� ��� 
	else {
		// ���� ���� ���� ��ü���� ��� �� ���ϸ��� ����
		strcpy(g_pathFileName, ofn.lpstrFile);

		// ���� ���� ���� ��ü���� ���ϸ��� ����
		strcpy(g_fileName, ofn.lpstrFileTitle);

		// ���ڿ��� �޸� ����
		free(ofn.lpstrFile);
		free(ofn.lpstrFileTitle);
	}

	return true;
}

/**
*	���� �׷��� �Ŵ����� �̿��Ͽ� ��Ʈ���� ���Ϸ� �����ϴ� �޼ҵ�
*
*	@param pGraph (���� �׷��� �Ŵ��� ������ ����)
*	@param wszPath (���� ���)
*	@return
*/
HRESULT SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath) {
	const WCHAR wszStreamName[] = L"ActiveMovieGraph";
	HRESULT hr;
	IStorage *pStorage = NULL;

	// ������ ����
	hr = StgCreateDocfile(wszPath, STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &pStorage);

	// ������ ����µ� �������� ���
	if (FAILED(hr)) {
		return hr;
	}

	IStream *pStream;

	// ���Ͽ� ���� ���� ��Ʈ���� ����
	hr = pStorage->CreateStream(wszStreamName, STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStream);

	// ��Ʈ���� ����µ� �������� ���
	if (FAILED(hr)) {
		// ���丮�� �ڿ� �ݳ�
		pStorage->Release();

		return hr;
	}

	IPersistStream *pPersist = NULL;

	// ���� �Ŵ����� ���� �޽ý�Ʈ ��Ʈ�� �������̽��� ������
	pGraph->QueryInterface(IID_IPersistStream, reinterpret_cast<void **>(&pPersist));

	// �޽ý�Ʈ ��Ʈ���� ����
	hr = pPersist->Save(pStream, TRUE);

	// ����ߴ� �ڿ��� �ݳ�
	pStream->Release();
	pPersist->Release();

	// �޽ý�Ʈ ��Ʈ���� ������ �Ϸ����� ���
	if (SUCCEEDED(hr)) {
		// ���Ͽ� ����
		hr = pStorage->Commit(STGC_DEFAULT);
	}

	// ���丮�� �ڿ� �ݳ�
	pStorage->Release();

	return hr;
}

/**
*	@Deprecated
*	���ͷ� ���� INPUT �Ǵ� OUTPUT ���� �������� �޼ҵ�
*	(���ø� ���Ϳ� ���� �ƿ�ǲ�� 2���� ��� ������� ����)
*
*	@param pFilter
*	@param PinDir
*	@return
*/
IPin *GetPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir) {
	BOOL bFound = FALSE;
	IEnumPins *pEnum;
	IPin *pPin;

	// ���ͷ� ���� �� ����ü�� ������
	HRESULT hr = pFilter->EnumPins(&pEnum);

	if (FAILED(hr)) {
		return NULL;
	}

	// �� ����ü�� ���鼭 ���� ������
	while (pEnum->Next(1, &pPin, 0) == S_OK) {
		PIN_DIRECTION PinDirThis;
		pPin->QueryDirection(&PinDirThis);

		// ���� ���⿡ ��ġ�ϴ� ���� �˻��Ǹ� ����
		if (bFound = (PinDir == PinDirThis)) {
			break;
		}

		pPin->Release();
	}

	pEnum->Release();

	return (bFound ? pPin : NULL);
}

/**
*	���ͷ� ���� ����Ǿ����� ���� INPUT �Ǵ� OUTPUT ���� �������� �޼ҵ�
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

	// ���ͷ� ���� �� ����ü�� ������
	HRESULT hr = pFilter->EnumPins(&pEnum);

	if (FAILED(hr)) {
		return hr;
	}

	// �� ����ü�� ���鼭 ����Ǿ����� ���� ���� ã��
	while (pEnum->Next(1, &pPin, NULL) == S_OK) {
		PIN_DIRECTION ThisPinDir;

		pPin->QueryDirection(&ThisPinDir);

		// ���� ���⿡ ��ġ�ϴ� ���� ���� ���
		if (ThisPinDir == PinDir) {
			IPin *pTmp = 0;

			// �ش� ���� ���� ���θ� Ȯ��
			hr = pPin->ConnectedTo(&pTmp);

			if (SUCCEEDED(hr)) {
				pTmp->Release();
			}

			else {
				pEnum->Release();

				// ����Ǿ� ���� ���� ��� �� ����
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
	// ���� �׷��� �Ŵ��� ������ ����
	IGraphBuilder *pGraph = NULL;

	// �̵�� ��Ʈ�� ������ ����
	IMediaControl *pControl = NULL;

	// �̵�� �̺�Ʈ ������ ����
	IMediaEvent *pEvent = NULL;

	// ���� ����
	IBaseFilter *InputFileFilter = NULL;
	IBaseFilter *StreamSplitterFilter = NULL;
	IBaseFilter *VideoDecoderFilter = NULL;
	IBaseFilter *AudioDecoderFilter = NULL;
	IBaseFilter *VideoRendererFilter = NULL;
	IBaseFilter *AudioRendererFilter = NULL;

	// �� ����
	IPin *FileOutputPin = NULL;
	IPin *StreamSplitterInputPin = NULL;
	IPin *StreamSplitterOutputPin = NULL;
	IPin *VideoDecoderInputPin = NULL;
	IPin *VideoDecoderOutputPin = NULL;
	IPin *AudioDecoderInputPin = NULL;
	IPin *AudioDecoderOutputPin = NULL;
	IPin *VideoRendererInputPin = NULL;
	IPin *AudioRendererInputPin = NULL;

	// COM(Component Object Model) ���̺귯���� ���� (DirectShow�� COM ���̺귯�� ���)
	HRESULT hr = CoInitialize(NULL);

	// COM ���̺귯���� �������� ���� ���
	if (FAILED(hr)) {
		// ���� �޼��� ��� �� ���α׷� ���� 
		printf("ERROR - Could not initialize COM library.\n");
		return hr;
	}

	// ���� �׷��� �Ŵ��� �ν��Ͻ� ����
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGraph);

	// ���� �׷��� �Ŵ����� �������� ���� ���
	if (FAILED(hr)) {
		// ���� �޼��� ��� �� ���α׷� ����
		printf("ERROR - Could not create the Filter Graph Manager.\n");
		return hr;
	}

	// ���� �׷��� �Ŵ����� ���� �̵�� ��Ʈ�� �ν��Ͻ��� ������
	// (�̵�� ��Ʈ���� ���� : �̵�� ��Ʈ�� ���� ex. ����, ����, �Ͻ�����)
	hr = pGraph->QueryInterface(IID_IMediaControl, (void **)&pControl);

	// �̵�� ��Ʈ�� ��ü ������ �������� ���
	if (FAILED(hr)) {
		// ���� �޼��� ���
		printf("ERROR - Could not create the Media Control object.\n");

		// ���� �׷��� �Ŵ��� �ڿ� �ݳ�
		pGraph->Release();

		// COM ���̺귯���� ����
		CoUninitialize();

		// ���α׷� ����
		return hr;
	}

	// ���� �׷��� �Ŵ����� ���� �̵�� �̺�Ʈ �ν��Ͻ��� ������
	// (�̵�� �̺�Ʈ�� ���� : ���� �׷��� �Ŵ����� �̺�Ʈ�� ���� ex. ��� �Ϸ�)
	hr = pGraph->QueryInterface(IID_IMediaEvent, (void **)&pEvent);

	// �̵�� �̺�Ʈ ��ü ������ �������� ���
	if (FAILED(hr)) {
		// ���� �޼��� ���, ����ߴ� �ڿ� ��ȯ �� ���α׷� ����
		printf("ERROR - Could not create the Media Event object.\n");

		// ����ߴ� �ڿ����� �ݳ�
		pGraph->Release();
		pControl->Release();

		// COM ���̺귯���� ����
		CoUninitialize();

		// ���α׷� ����
		return hr;
	}

	// �̵�� ������ ��ο� �̸��� �������� ���� ���
	if (!GetMediaFileName()) {
		// ���α׷� ����
		return 0;
	}

	// �����ڵ� �� ���
#ifndef UNICODE
	WCHAR wFileName[MAX_PATH];

	// ������ ��� �� ���ϸ��� ��Ƽ����Ʈ ĳ���ͷ� ����
	MultiByteToWideChar(CP_ACP, 0, g_pathFileName, -1, wFileName, MAX_PATH);

	// ���� �׷��� �Ŵ����� ���� �ҽ� ���� ����
	hr = pGraph->AddSourceFilter(wFileName, wFileName, &InputFileFilter);
	// �����ڵ尡 �ƴ� ���
#else
	// ���� �׷��� �Ŵ����� ���� �ҽ� ���� ����
	hr = pGraph->AddSourceFilter(wFileName, wFileName, &InputFileFilter);
#endif

	// �ҽ� ���͸� �������� ���
	if (SUCCEEDED(hr)) {
		// ��Ʈ�� ���ø� ���� �ν��Ͻ��� ������
		hr = CoCreateInstance(CLSID_MPEG1Splitter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&StreamSplitterFilter);

		// ��Ʈ�� ���ø� ���� �ν��Ͻ��� ������ ���
		if (SUCCEEDED(hr)) {
			// ���� �׷��� �Ŵ����� ���� ��Ʈ�� ���ø� ���͸� �߰�
			hr = pGraph->AddFilter(StreamSplitterFilter, L"Stream Splitter");

			// ��Ʈ�� ���ø� ���Ͱ� �߰��� ���
			if (SUCCEEDED(hr)) {
				// �ҽ� ������ �ƿ�ǲ �ɰ� ��Ʈ�� ���ø� ������ ��ǲ ���� �������� ���
				if (SUCCEEDED(GetUnConnectedPin(InputFileFilter, PINDIR_OUTPUT, &FileOutputPin)) &&
					SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_INPUT, &StreamSplitterInputPin))) {
					// �� ���� ����
					hr = pGraph->Connect(FileOutputPin, StreamSplitterInputPin);
				}

				// ���� ���ڴ� ���� �ν��Ͻ��� ������
				hr = CoCreateInstance(CLSID_CMpegVideoCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&VideoDecoderFilter);

				// ���� ���ڴ� ���� �ν��Ͻ��� ������ ���
				if (SUCCEEDED(hr)) {
					// ���� �׷��� �Ŵ����� ���� ���� ���ڴ� ���͸� �߰�
					hr = pGraph->AddFilter(VideoDecoderFilter, L"Video Decoder");

					// ���� ���ڴ� ���Ͱ� �߰��� ���
					if (SUCCEEDED(hr)) {
						// ��Ʈ�� ���ø� ������ �ƿ�ǲ �ɰ� ���� �������� ��ǲ ���� �������� ���
						if (SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_OUTPUT, &StreamSplitterOutputPin)) &&
							SUCCEEDED(GetUnConnectedPin(VideoDecoderFilter, PINDIR_INPUT, &VideoDecoderInputPin))) {
							// �� ���� ����
							hr = pGraph->Connect(StreamSplitterOutputPin, VideoDecoderInputPin);
						}

						// ���� ���� ���� �ν��Ͻ��� ������
						hr = CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&VideoRendererFilter);

						// ���� ���� ���� �ν��Ͻ��� ������ ���
						if (SUCCEEDED(hr)) {
							// ���� �׷��� �Ŵ����� ���� ���� ���� ���͸� �߰�
							hr = pGraph->AddFilter(VideoRendererFilter, L"Video Renderer");

							// ���� ���� ���Ͱ� �߰��� ���
							if (SUCCEEDED(hr)) {
								// ���� ���ڴ� ������ �ƿ�ǲ �ɰ� ���� ���� ������ ��ǲ ���� �������� ���
								if (SUCCEEDED(GetUnConnectedPin(VideoDecoderFilter, PINDIR_OUTPUT, &VideoDecoderOutputPin)) &&
									SUCCEEDED(GetUnConnectedPin(VideoRendererFilter, PINDIR_INPUT, &VideoRendererInputPin))) {
									// �� ���� ����
									hr = pGraph->Connect(VideoDecoderOutputPin, VideoRendererInputPin);
								}
							}
						}
					}
				}

				// ����� ���ڴ� ���� �ν��Ͻ��� ������
				hr = CoCreateInstance(CLSID_CMpegAudioCodec, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&AudioDecoderFilter);

				// ����� ���ڴ� ���� �ν��Ͻ��� ������ ���
				if (SUCCEEDED(hr)) {
					// ���� �׷��� �Ŵ����� ���� ����� ���ڴ� ���͸� �߰�
					hr = pGraph->AddFilter(AudioDecoderFilter, L"Audio Decoder");

					// ����� ���ڴ� ���Ͱ� �߰��� ���
					if (SUCCEEDED(hr)) {
						// ��Ʈ�� ���ø� ������ �ƿ�ǲ �ɰ� ����� �������� ��ǲ ���� �������� ���
						if (SUCCEEDED(GetUnConnectedPin(StreamSplitterFilter, PINDIR_OUTPUT, &StreamSplitterOutputPin)) &&
							SUCCEEDED(GetUnConnectedPin(AudioDecoderFilter, PINDIR_INPUT, &AudioDecoderInputPin))) {
							// �� ���� ����
							hr = pGraph->Connect(StreamSplitterOutputPin, AudioDecoderInputPin);
						}

						// ����� ���� ���� �ν��Ͻ��� ������
						hr = CoCreateInstance(CLSID_AudioRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&AudioRendererFilter);

						// ����� ���� ���� �ν��Ͻ��� ������ ���
						if (SUCCEEDED(hr)) {
							// ���� �׷��� �Ŵ����� ���� ����� ���� ���͸� �߰�
							hr = pGraph->AddFilter(AudioRendererFilter, L"Video Renderer");

							// ����� ���� ���Ͱ� �߰��� ���
							if (SUCCEEDED(hr)) {
								// ����� ���ڴ� ������ �ƿ�ǲ �ɰ� ����� ���� ������ ��ǲ ���� �������� ���
								if (SUCCEEDED(GetUnConnectedPin(AudioDecoderFilter, PINDIR_OUTPUT, &AudioDecoderOutputPin)) &&
									SUCCEEDED(GetUnConnectedPin(AudioRendererFilter, PINDIR_INPUT, &AudioRendererInputPin))) {
									// �� ���� ����
									hr = pGraph->Connect(AudioDecoderOutputPin, AudioRendererInputPin);
								}
							}
						}
					}
				}
			}
		}

		// �̵�� ��Ʈ���� ���� ��Ʈ�� ���
		hr = pControl->Run();

		// ��Ʈ�� ����� �� ���
		if (SUCCEEDED(hr)) {
			long evCode;

			// �̵�� �̺�Ʈ�� ���� ��� �Ϸ���� ���
			pEvent->WaitForCompletion(INFINITE, &evCode);
		}

		// �̵�� ��Ʈ���� ���� ��Ʈ�� ����
		hr = pControl->Stop();

		// ���� �׷����� �׷����������� �� �� �ֵ��� ���Ϸ� ����
		SaveGraphFile(pGraph, L"C:\\Users\\devetude\\Desktop\\SimpleMediaPlayer3\\MyGraph.GRF");
	}

	// �̵�� ������ ���� �׷����� �������� ���� ���
	else {
		// ���� �޼��� ���
		printf("ERROR - Could not find the media fila.\n");
	}

	// �����Ǿ��� ���� �������� �ڿ��� �ݳ�
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

	// �����Ǿ��� �� �������� �ڿ��� �ݳ�
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

	// ����ߴ� �ڿ����� �ݳ�
	pControl->Release();
	pEvent->Release();
	pGraph->Release();

	// COM ���̺귯���� ����
	CoUninitialize();
}